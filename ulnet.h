#include "sam2.c"

#include "juice/juice.h"
#include "zstd.h"
#include "zstd/lib/common/xxhash.h"
#include "fec.h"

#include <stdint.h>
#include <assert.h>

#define JUICE_CONCURRENCY_MODE JUICE_CONCURRENCY_MODE_USER

// The payload here is regarding the max payload that we *can* use
// We don't want to exceed the MTU because that can result in guranteed lost packets under certain conditions
// Considering various things like UDP/IP headers, STUN/TURN headers, and additional junk 
// load-balancers/routers might add I keep this conservative
#define PACKET_MTU_PAYLOAD_SIZE_BYTES 1408

#define ULNET_MAX_ROOMS 1024
#define SPECTATOR_MAX 64
#define CORE_OPTIONS_MAX 128

#define FLAGS_MASK                      0x0F
#define CHANNEL_MASK                    0xF0

#define CHANNEL_EXTRA                   0x00
#define CHANNEL_INPUT                   0x10
#define CHANNEL_INPUT_AUDIT_CONSISTENCY 0x20
#define CHANNEL_SAVESTATE_TRANSFER      0x30
#define CHANNEL_DESYNC_DEBUG            0xF0

#define NETPLAY_INPUT_HISTORY_SIZE 256

#define ULNET_SESSION_FLAG_WAITING_FOR_SAVE_STATE 0x1
#define ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY     0x2

// This constant defines the maximum number of frames that can be buffered before blocking.
// A value of 2 implies no delay can be accomidated.
//
// Consider the following scenario:
// logical-time | peer a       | peer b
// ------------------------------------------
// 0            | send input 0 | send input 0
// 1            | recv input 0 | recv input 0
// 2            | ------------ | tick frame 0
// 3            | ------------ | send input 1
// 4            | recv input 1 | ------------
// 5            | tick frame 0 | ------------
//
// The issue occurs at logical-time 4 when peer a receives input 1 before ticking frame 0.
// If the input buffer only holds 1 frame, the input packet for frame 0 would be overwritten.
// To handle the case where a peer immediately ticks and sends an input after receiving,
// the input buffer needs to hold at least 2 frames.
//
// Setting INPUT_DELAY_FRAMES_MAX to 2 allows for no frame delay while still handling this scenario.
// However, the constant is set to 8 to provide additional buffering capacity if needed.
#define INPUT_DELAY_FRAMES_MAX 8

#define ULNET_DELAY_FRAMES_MAX (INPUT_DELAY_FRAMES_MAX/2-1)

const int PortCount = 4;
typedef int16_t FLibretroInputState[64]; // This must be a POD for putting into packets

struct core_option_t {
    char key[128];
    char value[128];
};

// @todo This is really sparse so you should just add routines to read values from it in the serialized format
typedef struct {
    int64_t frame;
    FLibretroInputState input_state[INPUT_DELAY_FRAMES_MAX][PortCount];
    core_option_t core_option[INPUT_DELAY_FRAMES_MAX]; // Max 1 option per frame provided by the authority
} netplay_input_state_t;
static_assert(sizeof(netplay_input_state_t) == (sizeof(netplay_input_state_t::frame) + sizeof(netplay_input_state_t::input_state) + sizeof(netplay_input_state_t::core_option)), "netplay_input_state_t is not packed");

typedef struct {
    uint8_t channel_and_port;
    uint8_t coded_netplay_input_state[];
} input_packet_t;

typedef struct {
    uint8_t channel_and_flags;
    uint8_t spacing[7];

    int64_t frame;
    int64_t save_state_hash[INPUT_DELAY_FRAMES_MAX];
    int64_t input_state_hash[INPUT_DELAY_FRAMES_MAX];
    //int64_t options_state_hash[INPUT_DELAY_FRAMES_MAX]; // @todo
} desync_debug_packet_t;

#define FEC_PACKET_GROUPS_MAX 16
#define FEC_REDUNDANT_BLOCKS 16 // ULNET is hardcoded based on this value so it can't really be changed

#define SAVESTATE_TRANSFER_FLAG_K_IS_239         0b0001
#define SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0 0b0010

#define COMPRESSED_SAVE_STATE_BOUND_BYTES ZSTD_COMPRESSBOUND(20 * 1024 * 1024) // @todo Magic number
#define COMPRESSED_CORE_OPTIONS_BOUND_BYTES ZSTD_COMPRESSBOUND(sizeof(core_option_t[CORE_OPTIONS_MAX])) // @todo Probably make the type in here a typedef
#define COMPRESSED_DATA_WITH_REDUNDANCY_BOUND_BYTES (255 * (COMPRESSED_SAVE_STATE_BOUND_BYTES + COMPRESSED_CORE_OPTIONS_BOUND_BYTES) / (255 - FEC_REDUNDANT_BLOCKS))

typedef struct {
    uint8_t channel_and_flags;
    union {
        uint8_t reed_solomon_k;
        uint8_t packet_groups;
        uint8_t sequence_hi;
    };

    uint8_t sequence_lo;

    uint8_t payload[]; // Variable size; at most PACKET_MTU_PAYLOAD_SIZE_BYTES-3
} savestate_transfer_packet_t;

typedef struct {
    uint8_t channel_and_flags;
    union {
        uint8_t reed_solomon_k;
        uint8_t packet_groups;
        uint8_t sequence_hi;
    };

    uint8_t sequence_lo;

    uint8_t payload[PACKET_MTU_PAYLOAD_SIZE_BYTES-3]; // Variable size; at most PACKET_MTU_PAYLOAD_SIZE_BYTES-3
} savestate_transfer_packet2_t;
static_assert(sizeof(savestate_transfer_packet2_t) == PACKET_MTU_PAYLOAD_SIZE_BYTES, "Savestate transfer is the wrong size");

typedef struct {
    int64_t total_size_bytes; // @todo This isn't necessary
    int64_t frame_counter;
    uint64_t encoding_chain; // @todo probably won't use this
    uint64_t xxhash;

    int64_t compressed_options_size;
    int64_t compressed_savestate_size;
#if 0
    uint8_t compressed_savestate_data[compressed_savestate_size];
    uint8_t compressed_options_data[compressed_options_size];
#else
    uint8_t compressed_data[]; 
#endif
} savestate_transfer_payload_t;

typedef struct ulnet_session {
    int zstd_compress_level;

    int64_t frame_counter;
    int64_t flags;
    uint64_t our_peer_id;

    sam2_room_t room_we_are_in;

    core_option_t core_options[CORE_OPTIONS_MAX]; // @todo I don't like this here

    // @todo Change these so they're all peer_*
    juice_agent_t *agent                     [SAM2_PORT_MAX + 1 /* Plus Authority */ + SPECTATOR_MAX];
    uint64_t       agent_peer_id             [SAM2_PORT_MAX + 1 /* Plus Authority */ + SPECTATOR_MAX];
    int64_t        peer_desynced_frame       [SAM2_PORT_MAX + 1 /* Plus Authority */ + SPECTATOR_MAX];
    int64_t        peer_joining_on_frame     [SAM2_PORT_MAX + 1 /* Plus Authority */];
    netplay_input_state_t netplay_input_state[SAM2_PORT_MAX + 1 /* Plus Authority */];
    unsigned char netplay_input_packet_history[SAM2_PORT_MAX+1][NETPLAY_INPUT_HISTORY_SIZE][PACKET_MTU_PAYLOAD_SIZE_BYTES];
    
    int64_t spectator_count;

    desync_debug_packet_t desync_debug_packet;
    uint64_t peer_ready_to_join_bitfield; // Used by the authority for tracking join acknowledgements

    unsigned char remote_savestate_transfer_packets[COMPRESSED_DATA_WITH_REDUNDANCY_BOUND_BYTES + FEC_PACKET_GROUPS_MAX * (GF_SIZE - FEC_REDUNDANT_BLOCKS) * sizeof(savestate_transfer_packet_t)];
    int64_t remote_savestate_transfer_offset;
    uint8_t remote_packet_groups; // This is used to bookkeep how much data we actually need to receive to reform the complete savestate
    void *fec_packet[FEC_PACKET_GROUPS_MAX][GF_SIZE - FEC_REDUNDANT_BLOCKS];
    int fec_index[FEC_PACKET_GROUPS_MAX][GF_SIZE - FEC_REDUNDANT_BLOCKS];
    int fec_index_counter[FEC_PACKET_GROUPS_MAX]; // Counts packets received in each "packet group"

    void *user_ptr;
    int (*sam2_send_callback)(void *user_ptr, char *response);
    int (*populate_core_options_callback)(void *user_ptr, core_option_t options[CORE_OPTIONS_MAX]);

    size_t (*retro_serialize_size)(void);
    bool (*retro_serialize)(void *data, size_t size);
    bool (*retro_unserialize)(const void *data, size_t size);
} ulnet_session_t;

static void logical_partition(int sz, int redundant, int *n, int *out_k, int *packet_size, int *packet_groups) {
    int k_max = GF_SIZE - redundant;
    *packet_groups = 1;
    int k = (sz - 1) / (*packet_groups * *packet_size) + 1;

    if (k > k_max) {
        *packet_groups = (k - 1) / k_max + 1;
        *packet_size = (sz - 1) / (k_max * *packet_groups) + 1;
        k = (sz - 1) / (*packet_groups * *packet_size) + 1;
    }

    *n = k + k * redundant / k_max;
    *out_k = k;
}

// This is a little confusing since the lower byte of sequence corresponds to the largest stride
static int64_t logical_partition_offset_bytes(uint8_t sequence_hi, uint8_t sequence_lo, int block_size_bytes, int block_stride) {
    return (int64_t) sequence_hi * block_size_bytes + sequence_lo * block_size_bytes * block_stride;
}

bool FindPeer(ulnet_session_t *session, juice_agent_t** peer_agent, int* peer_existing_port, uint64_t peer_id) {
    for (int p = 0; p < SAM2_ARRAY_LENGTH(session->agent); p++) {
        if (session->agent[p] && session->room_we_are_in.peer_ids[p] == peer_id) {
            *peer_agent = session->agent[p];
            *peer_existing_port = p;
            return true;
        }
    }
    return false;
}

void MovePeer(ulnet_session_t *session, int peer_existing_port, int peer_new_port) {
    assert(peer_existing_port != peer_new_port);
    assert(session->agent[peer_new_port] == NULL);
    assert(session->agent_peer_id[peer_new_port] == 0);
    assert(session->agent[peer_existing_port] != NULL);
    assert(session->agent_peer_id[peer_new_port] != 0);

    session->agent[peer_new_port] = session->agent[peer_existing_port];
    session->agent_peer_id[peer_new_port] = session->agent_peer_id[peer_existing_port];
    session->agent_peer_id[peer_existing_port] = 0;
    session->agent[peer_existing_port] = NULL;
}

bool ulnet_is_authority(ulnet_session_t *session) {
    return session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX];
}

bool ulnet_is_spectator(ulnet_session_t *session, uint64_t peer_id) {
    return    session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_INITIALIZED
           && sam2_get_port_of_peer(&session->room_we_are_in, peer_id) == -1;
}

bool ulnet_all_peers_ready_for_peer_to_join(ulnet_session_t *session, uint64_t joiner_peer_id) {
    assert(ulnet_is_authority(session));
    int joiner_port;
    SAM2_LOCATE(session->room_we_are_in.peer_ids, joiner_peer_id, joiner_port);

    if (joiner_port == -1) {
        assert(   session->our_peer_id == joiner_peer_id 
               || session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]);
        SAM2_LOG_DEBUG("Assuming %016" PRIx64 " is a spectator\n", joiner_peer_id);
        return true;
    }

    if (joiner_port == SAM2_AUTHORITY_INDEX) {
        return true;
    }

    return 0xFFULL == (0xFFULL & (session->peer_ready_to_join_bitfield >> (8 * joiner_port)));
}

void ulnet_send_save_state(ulnet_session_t *session, juice_agent_t *agent);

// Interactive-Connectivity-Establishment callbacks that libjuice calls
static void on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr);
static void on_candidate(juice_agent_t *agent, const char *sdp, void *user_ptr);
static void on_gathering_done(juice_agent_t *agent, void *user_ptr);
static void on_recv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr); // We get all our packets in this one

void startup_ice_for_peer(ulnet_session_t *session, juice_agent_t **agent, uint64_t *agent_peer_id, uint64_t peer_id, const char *remote_description = NULL) {
    juice_config_t config;
    memset(&config, 0, sizeof(config));

    // STUN server example*
    config.concurrency_mode = JUICE_CONCURRENCY_MODE;
    config.stun_server_host = "stun2.l.google.com"; // @todo Put a bad url here to test how to handle that
    config.stun_server_port = 19302;
    //config.bind_address = "127.0.0.1";

    config.cb_state_changed = on_state_changed;
    config.cb_candidate = on_candidate;
    config.cb_gathering_done = on_gathering_done;
    config.cb_recv = on_recv;

    config.user_ptr = (void *) session;

    *agent = juice_create(&config);

    *agent_peer_id = peer_id;

    if (remote_description) {
        // Right now I think there could be some kind of bug or race condition in my code or libjuice when there
        // is an ICE role conflict. A role conflict is benign, but when a spectator connects the authority will never fully
        // establish the connection even though the spectator manages to. If I avoid the role conflict by setting
        // the remote description here then my connection establishes fine, but I should look into this eventually @todo
        juice_set_remote_description(*agent, remote_description);
    }

    sam2_signal_message_t signal_message = { SAM2_SIGN_HEADER };
    signal_message.peer_id = peer_id;
    juice_get_local_description(*agent, signal_message.ice_sdp, sizeof(signal_message.ice_sdp));
    session->sam2_send_callback(session->user_ptr, (char *) &signal_message);

    // This call starts an asynchronous task that requires periodic polling via juice_user_poll to complete
    // it will call the on_gathering_done callback once it's finished
    juice_gather_candidates(*agent);
}

int ulnet_our_port(ulnet_session_t *session) {
    // @todo There is a bug here where we are sending out packets as the authority when we are not the authority
    if (session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_INITIALIZED) {
        int port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);

        if (port == -1) {
            return 0; // @todo This should be handled differently I just don't want to out-of-bounds right now
        } else {
            return port;
        }
    } else {
        return SAM2_AUTHORITY_INDEX;
    }
}

// On state changed
static void on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    if (   state == JUICE_STATE_CONNECTED
        && session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
        printf("Sending savestate to peer\n");
        // @todo Probably this should be changed to a needs save state flag since this function is called from a callback
        //       but it might not matter
        ulnet_send_save_state(session, agent);
    }
}

// On local candidate gathered
static void on_candidate(juice_agent_t *agent, const char *sdp, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p;
    SAM2_LOCATE(session->agent, agent, p);
    if (p == -1) {
        SAM2_LOG_ERROR("No agent found\n");
        return;
    }

    sam2_signal_message_t response = { SAM2_SIGN_HEADER };

    response.peer_id = session->agent_peer_id[p];
    if (strlen(sdp) < sizeof(response.ice_sdp)) {
        strcpy(response.ice_sdp, sdp);
        session->sam2_send_callback(session->user_ptr, (char *) &response);
    } else {
        SAM2_LOG_ERROR("Candidate too large\n");
        return;
    }
}

// On local candidates gathering done
static void on_gathering_done(juice_agent_t *agent, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p;
    SAM2_LOCATE(session->agent, agent, p);
    if (p == -1) {
        SAM2_LOG_ERROR("No agent found\n");
        return;
    }

    sam2_signal_message_t response = { SAM2_SIGN_HEADER };

    response.peer_id = session->agent_peer_id[p];
    session->sam2_send_callback(session->user_ptr, (char *) &response);
}

int ulnet_poll_session(ulnet_session_t *session, sam2_response_u *_response) {

    sam2_response_u &response = *_response;

    sam2_message_e response_tag = sam2_get_tag((const char *) _response);
    if (response_tag == SAM2_EMESSAGE_INVALID) {
        return -1;
    }

    switch (response_tag) {
    case SAM2_EMESSAGE_ERROR: {
        break;
    }
    case SAM2_EMESSAGE_LIST: {
        break;
    }
    case SAM2_EMESSAGE_MAKE: {
        sam2_room_make_message_t *room_make = &response.room_make_response;
        assert(session->our_peer_id == room_make->room.peer_ids[SAM2_AUTHORITY_INDEX]);
        session->room_we_are_in = room_make->room;
        break;
    }
    case SAM2_EMESSAGE_CONN: {
        sam2_connect_message_t *connect_message = &response.connect_message;
        SAM2_LOG_INFO("We were assigned the peer id %" PRIx64 "\n", connect_message->peer_id);

        session->our_peer_id = connect_message->peer_id;
        session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session->our_peer_id;

        break;
    }
    case SAM2_EMESSAGE_JOIN: {
        sam2_room_join_message_t *room_join = &response.room_join_response;

        if (   sam2_same_room(&session->room_we_are_in, &room_join->room)
            && session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_INITIALIZED) {
            SAM2_LOG_INFO("We switched rooms or joined a room for the first time %s\n", room_join->room.name);

            // @todo This should be actually handled, but it conflicts with the if statement below
        }

        if (!sam2_same_room(&session->room_we_are_in, &room_join->room)) {
            SAM2_LOG_INFO("We were let into the server by the authority\n");

            session->flags |= ULNET_SESSION_FLAG_WAITING_FOR_SAVE_STATE;
            session->frame_counter = 123456789000; // frame_counter is invalid before we get a savestate this should make logic issues more obvious
            session->room_we_are_in = room_join->room;
            session->peer_joining_on_frame[ulnet_our_port(session)] = INT64_MAX; // Upper bound
            memset(session->netplay_input_state, 0, sizeof(session->netplay_input_state));
            memset(session->netplay_input_packet_history, 0, sizeof(session->netplay_input_packet_history));

            for (int p = 0; p < SAM2_ARRAY_LENGTH(room_join->room.peer_ids); p++) {
                if (room_join->room.peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;
                if (room_join->room.peer_ids[p] == session->our_peer_id) continue;
                if (session->agent[p]) {
                    // We can get here if we're spectating since we already have an agent for the authority
                    assert(session->agent_peer_id[p] == room_join->room.peer_ids[p]);
                    continue;
                }

                session->peer_ready_to_join_bitfield |= (0xFFULL << SAM2_PORT_MAX * p); // @todo This should be based on a bitflag in the room

                startup_ice_for_peer(session, &session->agent[p], &session->agent_peer_id[p], room_join->room.peer_ids[p]);
            }
        } else {
            if (session->our_peer_id == room_join->room.peer_ids[SAM2_AUTHORITY_INDEX]) {
                SAM2_LOG_INFO("Someone has asked us to change the state of the server in some way e.g. leaving, joining, etc.\n");

                int sender_port = sam2_get_port_of_peer(&room_join->room, room_join->peer_id);

                if (sender_port == -1) {
                    SAM2_LOG_WARN("They didn't specify which port they're joining on\n");

                    sam2_error_response_t error = {
                        SAM2_FAIL_HEADER,
                        SAM2_RESPONSE_AUTHORITY_ERROR,
                        "Client didn't try to join on any ports",
                        room_join->peer_id
                    };

                    session->sam2_send_callback(session->user_ptr, (char *) &error);
                    break;
                } else {
                    //session->room_we_are_in.flags |= SAM2_FLAG_PORT0_PEER_IS_INACTIVE << p;

                    juice_agent_t *peer_agent;
                    int peer_existing_port;
                    if (FindPeer(session, &peer_agent, &peer_existing_port, room_join->room.peer_ids[sender_port])) {
                        if (peer_existing_port != sender_port) {
                            MovePeer(session, peer_existing_port, sender_port); // This only moves spectators to real ports right now
                        } else {
                            SAM2_LOG_INFO("Peer %" PRIx64 " has asked to change something about the room\n", room_join->room.peer_ids[sender_port]);

                        }
                    } else {
                        if (session->room_we_are_in.peer_ids[sender_port] != SAM2_PORT_AVAILABLE) {
                            sam2_error_response_t error = {
                                SAM2_FAIL_HEADER,
                                SAM2_RESPONSE_AUTHORITY_ERROR,
                                "Peer tried to join on unavailable port",
                                room_join->peer_id
                            };

                            session->sam2_send_callback(session->user_ptr, (char *) &error);
                            break;
                        } else {
                            SAM2_LOG_INFO("Peer %" PRIx64 " was let in by us the authority\n", room_join->room.peer_ids[sender_port]);

                            session->peer_joining_on_frame[sender_port] = session->frame_counter;
                            session->peer_ready_to_join_bitfield &= ~(0xFFULL << (8 * sender_port));

                            for (int peer_port = 0; peer_port < SAM2_PORT_MAX; peer_port++) {
                                if (session->room_we_are_in.peer_ids[peer_port] <= SAM2_PORT_SENTINELS_MAX) {
                                    session->peer_ready_to_join_bitfield |= 1ULL << (SAM2_PORT_MAX * sender_port + peer_port);
                                }
                            }

                            session->room_we_are_in.peer_ids[sender_port] = room_join->room.peer_ids[sender_port];
                            sam2_room_join_message_t response = { SAM2_JOIN_HEADER };
                            response.room = session->room_we_are_in;
                            session->sam2_send_callback(session->user_ptr, (char *) &response); // This must come before the next call

                            startup_ice_for_peer(
                                session,
                                &session->agent[sender_port],
                                &session->agent_peer_id[sender_port],
                                room_join->room.peer_ids[sender_port]
                            );

                            // @todo This check is basically duplicated code with the code in the ACKJ handler
                            if (ulnet_all_peers_ready_for_peer_to_join(session, room_join->room.peer_ids[sender_port])) {
                                // IS THIS NECESSARY? It doesn't seem like it so far with one connection

                                sam2_room_acknowledge_join_message_t response = { SAM2_ACKJ_HEADER };
                                response.room = session->room_we_are_in;
                                response.joiner_peer_id = room_join->room.peer_ids[sender_port];
                                response.frame_counter = session->peer_joining_on_frame[sender_port];

                                session->sam2_send_callback(session->user_ptr, (char *) &response);
                            }

                            break; // @todo I don't like this break
                        }
                    }

                    session->sam2_send_callback(session->user_ptr, (char *) &response);
                }
            } else {
                SAM2_LOG_INFO("Something about the room we're in was changed by the authority\n");

                assert(sam2_same_room(&session->room_we_are_in, &room_join->room));

                for (int p = 0; p < SAM2_ARRAY_LENGTH(room_join->room.peer_ids); p++) {
                    // @todo Check something other than just joins and leaves
                    if (room_join->room.peer_ids[p] != session->room_we_are_in.peer_ids[p]) {
                        if (room_join->room.peer_ids[p] == SAM2_PORT_AVAILABLE) {
                            SAM2_LOG_INFO("Peer %" PRIx64 " has left the room\n", session->room_we_are_in.peer_ids[p]);

                            if (session->agent[p]) {
                                juice_destroy(session->agent[p]);
                                session->agent[p] = NULL;
                            }

                            if (session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
                                //session->room_we_are_in.flags |= SAM2_FLAG_PORT0_PEER_IS_INACTIVE << p;
                            }

                            session->room_we_are_in.peer_ids[p] = SAM2_PORT_AVAILABLE;
                        } else {
                            SAM2_LOG_INFO("Peer %" PRIx64 " has joined the room\n", room_join->room.peer_ids[p]);

                            session->room_we_are_in.peer_ids[p] = room_join->room.peer_ids[p]; // This must come before the next call as the next call can generate an ICE candidate before returning
                            startup_ice_for_peer(session, &session->agent[p], &session->agent_peer_id[p], room_join->room.peer_ids[p]);

                            sam2_room_acknowledge_join_message_t response = { SAM2_ACKJ_HEADER };
                            response.room = session->room_we_are_in;
                            response.joiner_peer_id = session->room_we_are_in.peer_ids[p] = room_join->room.peer_ids[p];
                            response.frame_counter = session->peer_joining_on_frame[p] = session->frame_counter; // Lower bound

                            session->sam2_send_callback(session->user_ptr, (char *) &response);
                        }
                    }
                }
            }
        }
        break;
    }
    case SAM2_EMESSAGE_ACKJ: {
        sam2_room_acknowledge_join_message_t *acknowledge_room_join_message = &response.room_acknowledge_join_message;

        int joiner_port = sam2_get_port_of_peer(&session->room_we_are_in, acknowledge_room_join_message->joiner_peer_id);
        int sender_port = sam2_get_port_of_peer(&session->room_we_are_in, acknowledge_room_join_message->sender_peer_id);
        assert(joiner_port != -1);
        assert(sender_port != -1);
        assert(sender_port != joiner_port);

        if (session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
            session->peer_ready_to_join_bitfield |= ((1ULL << sender_port) << (8 * joiner_port));

            session->peer_joining_on_frame[joiner_port] = SAM2_MAX(
                session->peer_joining_on_frame[joiner_port],
                acknowledge_room_join_message->frame_counter
            );

            if (ulnet_all_peers_ready_for_peer_to_join(session, acknowledge_room_join_message->joiner_peer_id)) {
                sam2_room_acknowledge_join_message_t response = { SAM2_ACKJ_HEADER };
                response.room = session->room_we_are_in;
                response.joiner_peer_id = acknowledge_room_join_message->joiner_peer_id;
                response.frame_counter = session->peer_joining_on_frame[joiner_port];

                session->sam2_send_callback(session->user_ptr, (char *) &response);
            } else {
                SAM2_LOG_INFO("Peer %" PRIx64 " has been acknowledged by %" PRIx64 " but not all peers\n", 
                    acknowledge_room_join_message->joiner_peer_id, acknowledge_room_join_message->sender_peer_id);
            }
        } else {
            assert(acknowledge_room_join_message->sender_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]);
            assert(acknowledge_room_join_message->frame_counter >= session->peer_joining_on_frame[joiner_port] || session->our_peer_id == acknowledge_room_join_message->joiner_peer_id);
            SAM2_LOG_INFO("Authority told us peer %" PRIx64 " has been acknowledged by all peers and is joining on frame %" PRId64 " (our current frame %" PRId64 ")\n", 
                acknowledge_room_join_message->joiner_peer_id, acknowledge_room_join_message->frame_counter, session->frame_counter);

            session->peer_ready_to_join_bitfield |= 0xFFULL << (8 * joiner_port);
            session->peer_joining_on_frame[joiner_port] = acknowledge_room_join_message->frame_counter;

            if (acknowledge_room_join_message->joiner_peer_id == session->our_peer_id) {
                // @todo I feel like this shouldn't really have to be done, but I need it currently
                // since I use it to bookkeep the number of buffered frames of input
                session->netplay_input_state[ulnet_our_port(session)].frame = session->peer_joining_on_frame[joiner_port] - 1;
            }
        }
        break;
    }
    case SAM2_EMESSAGE_SIGNAL: {
        sam2_signal_message_t *room_signal = (sam2_signal_message_t *) &response;
        SAM2_LOG_INFO("Received signal from peer %" PRIx64 "\n", room_signal->peer_id);

        int p = -1;
        for (int i = 0; i < SAM2_ARRAY_LENGTH(session->agent_peer_id); i++) {
            if (session->agent_peer_id[i] == room_signal->peer_id) {
                p = i;
                break;
            }
        }

        if (p == -1) {
            SAM2_LOG_INFO("Received signal from unknown peer\n");

            if (session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
                if (session->spectator_count == SPECTATOR_MAX) {
                    SAM2_LOG_WARN("We can't let them in as a spectator there are too many spectators\n");

                    static sam2_error_response_t error = { 
                        SAM2_FAIL_HEADER,
                        SAM2_RESPONSE_AUTHORITY_ERROR,
                        "Authority has reached the maximum number of spectators"
                    };

                    session->sam2_send_callback(session->user_ptr, (char *) &error);
                } else {
                    SAM2_LOG_INFO("We are letting them in as a spectator\n");

                    startup_ice_for_peer(
                        session,
                        &session->agent[SAM2_PORT_MAX+1 + session->spectator_count],
                        &session->agent_peer_id[SAM2_PORT_MAX+1 + session->spectator_count],
                        room_signal->peer_id,
                        /* remote_desciption = */ room_signal->ice_sdp
                    );

                    p = session->spectator_count++;
                }
            } else {
                SAM2_LOG_WARN("Received unknown signal when we weren't the authority\n");

                static sam2_error_response_t error = { 
                    SAM2_FAIL_HEADER,
                    SAM2_RESPONSE_AUTHORITY_ERROR,
                    "Received unknown signal when we weren't the authority"
                };

                session->sam2_send_callback(session->user_ptr, (char *) &error);
            }
        }

        if (p != -1) {
            if (strlen(room_signal->ice_sdp) == 0) {
                SAM2_LOG_INFO("Received remote gathering done from peer %" PRIx64 "\n", room_signal->peer_id);
                juice_set_remote_gathering_done(session->agent[p]);
            } else if (strncmp(room_signal->ice_sdp, "a=ice", strlen("a=ice")) == 0) {
                juice_set_remote_description(session->agent[p], room_signal->ice_sdp);
            } else if (strncmp(room_signal->ice_sdp, "a=candidate", strlen("a=candidate")) == 0) {
                juice_add_remote_candidate(session->agent[p], room_signal->ice_sdp);
            } else {
                SAM2_LOG_ERROR("Unable to parse signal message '%s'\n", room_signal->ice_sdp);
            }
        }

        break;
    }
    default:
        SAM2_LOG_ERROR("Received unknown message (%d) from SAM2\n", response_tag);
        break;
    }

    return 0;
}

static void on_recv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p = -1;
    for (int i = 0; i < SAM2_ARRAY_LENGTH(session->agent); i++) {
        if (session->agent[i] == agent) {
            p = i;
            break;
        }
    }
    if (p == -1) {
        SAM2_LOG_ERROR("No agent associated for packet on channel 0x%" PRIx8 "\n", data[0] & CHANNEL_MASK);
        return;
    }

    if (size == 0) {
        SAM2_LOG_WARN("Received a UDP packet with no payload\n");
        return;
    }

    if (p >= SAM2_PORT_MAX+1
        && CHANNEL_INPUT != (data[0] & CHANNEL_MASK)) {
        SAM2_LOG_WARN("A spectator sent us a UDP packet for unsupported channel %" PRIx8 " for some reason\n", data[0] & CHANNEL_MASK);
        return;
    }

    uint8_t channel_and_flags = data[0];
    switch (channel_and_flags & CHANNEL_MASK) {
    case CHANNEL_EXTRA: {
        assert(!"This is an error currently\n");
    }
    case CHANNEL_INPUT: {
        assert(size <= PACKET_MTU_PAYLOAD_SIZE_BYTES);

        input_packet_t *input_packet = (input_packet_t *) data;
        int8_t original_sender_port = input_packet->channel_and_port & FLAGS_MASK;

        if (   p != original_sender_port
            && p != SAM2_AUTHORITY_INDEX) {
            SAM2_LOG_WARN("Non-authority gave us someones input eventually this should be verified with a signature\n");
        }

        if (original_sender_port >= SAM2_PORT_MAX+1) {
            SAM2_LOG_WARN("Received input packet for port %d which is out of range\n", original_sender_port);
            break;
        }

        if (rle8_decode_size(input_packet->coded_netplay_input_state, size - 1) != sizeof(netplay_input_state_t)) {
            SAM2_LOG_WARN("Received input packet with an invalid decode size\n");
            break;
        }

        int64_t frame;
        rle8_decode(input_packet->coded_netplay_input_state, size - 1, (uint8_t *) &frame, sizeof(frame));

        SAM2_LOG_DEBUG("Recv input packet for frame %" PRId64 " from peer_ids[%d]=%" PRIx64 "\n",
            frame, p, session->room_we_are_in.peer_ids[p]);

        if (   ulnet_is_authority(session)
            && frame < session->peer_joining_on_frame[p]) {
            SAM2_LOG_WARN("Received input packet for frame %" PRId64 " but we agreed the client would start sending input on frame %" PRId64 "\n",
                frame, session->peer_joining_on_frame[p]);
        } else if (frame < session->netplay_input_state[p].frame) {
            // UDP packets can arrive out of order this is normal
            SAM2_LOG_DEBUG("Received outdated input packet for frame %" PRId64 ". We are already on frame %" PRId64 ". Dropping it\n",
                frame, session->frame_counter);
        } else {
            rle8_decode(
                input_packet->coded_netplay_input_state, size - 1,
                (uint8_t *) &session->netplay_input_state[p], sizeof(netplay_input_state_t)
            );

            // Store the input packet in the history buffer. Zero runs decode to no bytes convieniently so we don't need to store the packet size
            int i = 0;
            for (; i < size; i++) {
                session->netplay_input_packet_history[original_sender_port][frame % NETPLAY_INPUT_HISTORY_SIZE][i] = data[i];
            }

            for (; i < PACKET_MTU_PAYLOAD_SIZE_BYTES; i++) {
                session->netplay_input_packet_history[original_sender_port][frame % NETPLAY_INPUT_HISTORY_SIZE][i] = 0;
            }

            // Broadcast the input packet to spectators
            if (ulnet_is_authority(session)) {
                for (int i = 0; i < SPECTATOR_MAX; i++) {
                    juice_agent_t *spectator_agent = session->agent[SAM2_PORT_MAX+1 + i];
                    if (spectator_agent) {
                        if (   juice_get_state(spectator_agent) == JUICE_STATE_CONNECTED
                            || juice_get_state(spectator_agent) == JUICE_STATE_COMPLETED) {
                            int status = juice_send(spectator_agent, data, size);
                            assert(status == 0);
                        }
                    }
                }
            }
        }

        break;
    }
    case CHANNEL_DESYNC_DEBUG: {
        // @todo This channel doesn't receive messages reliably, but I think it should be changed to in the same manner as the input channel
        assert(size == sizeof(desync_debug_packet_t));

        desync_debug_packet_t their_desync_debug_packet;
        memcpy(&their_desync_debug_packet, data, sizeof(desync_debug_packet_t)); // Strict-aliasing

        desync_debug_packet_t &our_desync_debug_packet = session->desync_debug_packet;

        int64_t latest_common_frame = SAM2_MIN(our_desync_debug_packet.frame, their_desync_debug_packet.frame);
        int64_t frame_difference = SAM2_ABS(our_desync_debug_packet.frame - their_desync_debug_packet.frame);
        int64_t total_frames_to_compare = INPUT_DELAY_FRAMES_MAX - frame_difference;
        for (int f = total_frames_to_compare-1; f >= 0 ; f--) {
            int64_t frame_to_compare = latest_common_frame - f;
            if (frame_to_compare < session->peer_joining_on_frame[p]) continue; // Gets rid of false postives when another peer is joining
            if (frame_to_compare < session->peer_joining_on_frame[ulnet_our_port(session)]) continue; // Gets rid of false postives when we are joining
            int64_t frame_index = frame_to_compare % INPUT_DELAY_FRAMES_MAX;

            if (our_desync_debug_packet.input_state_hash[frame_index] != their_desync_debug_packet.input_state_hash[frame_index]) {
                SAM2_LOG_ERROR("Input state hash mismatch for frame %" PRId64 " Our hash: %" PRIx64 " Their hash: %" PRIx64 "\n", 
                    frame_to_compare, our_desync_debug_packet.input_state_hash[frame_index], their_desync_debug_packet.input_state_hash[frame_index]);
            } else if (   our_desync_debug_packet.save_state_hash[frame_index]
                       && their_desync_debug_packet.save_state_hash[frame_index]) {

                if (our_desync_debug_packet.save_state_hash[frame_index] != their_desync_debug_packet.save_state_hash[frame_index]) {
                    if (!session->peer_desynced_frame[p]) {
                        session->peer_desynced_frame[p] = frame_to_compare;
                    }

                    SAM2_LOG_ERROR("Save state hash mismatch for frame %" PRId64 " Our hash: %" PRIx64 " Their hash: %" PRIx64 "\n",
                        frame_to_compare, our_desync_debug_packet.save_state_hash[frame_index], their_desync_debug_packet.save_state_hash[frame_index]);
                } else if (session->peer_desynced_frame[p]) {
                    session->peer_desynced_frame[p] = 0;
                    SAM2_LOG_INFO("Peer resynced frame on frame %" PRId64 "\n", frame_to_compare);
                }
            }
        }

        break;
    }
    case CHANNEL_SAVESTATE_TRANSFER: {
        if (session->remote_packet_groups == 0) {
            // This is kind of a hack. Since every field in ulnet_session can just be zero-inited
            // besides this one. I just use this check here to set it to it's correct initial value
            session->remote_packet_groups = FEC_PACKET_GROUPS_MAX;
        }

        if (session->agent[SAM2_AUTHORITY_INDEX] != agent) {
            printf("Received savestate transfer packet from non-authority agent\n");
            break;
        }

        if (size < sizeof(savestate_transfer_packet_t)) {
            SAM2_LOG_WARN("Recv savestate transfer packet with size smaller than header\n");
            break;
        }

        if (size > PACKET_MTU_PAYLOAD_SIZE_BYTES) {
            SAM2_LOG_WARN("Recv savestate transfer packet potentially larger than MTU\n");
        }

        savestate_transfer_packet_t savestate_transfer_header;
        memcpy(&savestate_transfer_header, data, sizeof(savestate_transfer_packet_t)); // Strict-aliasing

        uint8_t sequence_hi = 0;
        int k = 239;
        if (channel_and_flags & SAVESTATE_TRANSFER_FLAG_K_IS_239) {
            if (channel_and_flags & SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0) {
                session->remote_packet_groups = savestate_transfer_header.packet_groups;
            } else {
                sequence_hi = savestate_transfer_header.sequence_hi;
            }
        } else {
            k = savestate_transfer_header.reed_solomon_k;
            session->remote_packet_groups = 1; // k != 239 => 1 packet group
        }

        if (session->fec_index_counter[sequence_hi] == k) {
            // We already have received enough Reed-Solomon blocks to decode the payload; we can ignore this packet
            break;
        }

        uint8_t sequence_lo = savestate_transfer_header.sequence_lo;

        SAM2_LOG_DEBUG("Received savestate packet sequence_hi: %hhu sequence_lo: %hhu\n", sequence_hi, sequence_lo);

        uint8_t *copied_packet_ptr = (uint8_t *) memcpy(session->remote_savestate_transfer_packets + session->remote_savestate_transfer_offset, data, size);
        session->fec_packet[sequence_hi][sequence_lo] = copied_packet_ptr + sizeof(savestate_transfer_packet_t);
        session->remote_savestate_transfer_offset += size;

        session->fec_index[sequence_hi][session->fec_index_counter[sequence_hi]++] = sequence_lo;

        if (session->fec_index_counter[sequence_hi] == k) {
            SAM2_LOG_DEBUG("Received all the savestate data for packet group: %hhu\n", sequence_hi);

            int redudant_blocks_sent = k * FEC_REDUNDANT_BLOCKS / (GF_SIZE - FEC_REDUNDANT_BLOCKS);
            void *rs_code = fec_new(k, k + redudant_blocks_sent);
            int rs_block_size = (int) (size - sizeof(savestate_transfer_packet_t));
            int status = fec_decode(rs_code, session->fec_packet[sequence_hi], session->fec_index[sequence_hi], rs_block_size);
            assert(status == 0);
            fec_free(rs_code);

            bool all_data_decoded = true;
            for (int i = 0; i < session->remote_packet_groups; i++) {
                all_data_decoded &= session->fec_index_counter[i] >= k;
            }

            if (all_data_decoded) { 
                savestate_transfer_payload_t *savestate_transfer_payload = (savestate_transfer_payload_t *) malloc(sizeof(savestate_transfer_payload_t) /* Fixed size header */ + COMPRESSED_DATA_WITH_REDUNDANCY_BOUND_BYTES);

                int64_t remote_payload_size = 0;
                // @todo The last packet contains some number of garbage bytes probably add the size thing back?
                for (int i = 0; i < k; i++) {
                    for (int j = 0; j < session->remote_packet_groups; j++) {
                        memcpy(((uint8_t *) savestate_transfer_payload) + remote_payload_size, session->fec_packet[j][i], rs_block_size);
                        remote_payload_size += rs_block_size;
                    }
                }

                SAM2_LOG_INFO("Received savestate transfer payload for frame %" PRId64 "\n", savestate_transfer_payload->frame_counter);

                size_t ret = ZSTD_decompress(
                    session->core_options, sizeof(session->core_options),
                    savestate_transfer_payload->compressed_data + savestate_transfer_payload->compressed_savestate_size,
                    savestate_transfer_payload->compressed_options_size
                );

                unsigned char *save_state_data = NULL;
                if (ZSTD_isError(ret)) {
                    SAM2_LOG_ERROR("Error decompressing core options: %s\n", ZSTD_getErrorName(ret));
                } else {
                    session->flags |= ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY;
                    //session.retro_run(); // Apply options before loading savestate; Lets hope this isn't necessary

                    int64_t remote_savestate_hash = fnv1a_hash(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes);
                    SAM2_LOG_INFO("Received savestate payload with hash: %llx size: %llu bytes\n",
                        remote_savestate_hash, savestate_transfer_payload->total_size_bytes);

                    save_state_data = (unsigned char *) malloc(session->retro_serialize_size());

                    int64_t save_state_size = ZSTD_decompress(
                        save_state_data,
                        session->retro_serialize_size(),
                        savestate_transfer_payload->compressed_data, 
                        savestate_transfer_payload->compressed_savestate_size
                    );

                    if (ZSTD_isError(save_state_size)) {
                        SAM2_LOG_ERROR("Error decompressing savestate: %s\n", ZSTD_getErrorName(save_state_size));
                    } else {
                        if (!session->retro_unserialize(save_state_data, save_state_size)) {
                            SAM2_LOG_ERROR("Failed to load savestate\n");
                        } else {
                            SAM2_LOG_DEBUG("Save state loaded\n");
                            session->frame_counter = savestate_transfer_payload->frame_counter;
                            session->netplay_input_state[ulnet_our_port(session)].frame = savestate_transfer_payload->frame_counter; // @todo This shouldn't be necessary
                            memset(&session->netplay_input_state[ulnet_our_port(session)].input_state, 0, // @todo This should probably happen somewhere else
                             sizeof(session->netplay_input_state[ulnet_our_port(session)].input_state));
                        }
                    }
                }

                if (save_state_data != NULL) {
                    free(save_state_data);
                }

                free(savestate_transfer_payload);

                // Reset the savestate transfer state
                session->remote_packet_groups = FEC_PACKET_GROUPS_MAX;
                session->remote_savestate_transfer_offset = 0;
                memset(session->fec_index_counter, 0, sizeof(session->fec_index_counter));
                session->flags &= ~ULNET_SESSION_FLAG_WAITING_FOR_SAVE_STATE;
            }
        }
        break;
    }
    default:
        fprintf(stderr, "Unknown channel: %d\n", channel_and_flags);
        break;
    }
}

void ulnet_send_save_state(ulnet_session_t *session, juice_agent_t *agent) {
    size_t serialize_size = session->retro_serialize_size();
    void *savebuffer = malloc(serialize_size);

    if (!session->retro_serialize(savebuffer, serialize_size)) {
        assert(!"Failed to serialize");
    }

    int packet_payload_size_bytes = PACKET_MTU_PAYLOAD_SIZE_BYTES - sizeof(savestate_transfer_packet_t);
    int n, k, packet_groups;

    int64_t save_state_transfer_payload_compressed_bound_size_bytes = ZSTD_COMPRESSBOUND(serialize_size) + ZSTD_COMPRESSBOUND(sizeof(session->core_options));
    logical_partition(sizeof(savestate_transfer_payload_t) /* Header */ + save_state_transfer_payload_compressed_bound_size_bytes,
                      FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size_bytes, &packet_groups);

    size_t savestate_transfer_payload_plus_parity_bound_bytes = packet_groups * n * packet_payload_size_bytes;

    // This points to the savestate transfer payload, but also the remaining bytes at the end hold our parity blocks
    // Having this data in a single contiguous buffer makes indexing easier
    savestate_transfer_payload_t *savestate_transfer_payload = (savestate_transfer_payload_t *) malloc(savestate_transfer_payload_plus_parity_bound_bytes);

    savestate_transfer_payload->compressed_savestate_size = ZSTD_compress(
        savestate_transfer_payload->compressed_data,
        save_state_transfer_payload_compressed_bound_size_bytes,
        savebuffer, serialize_size, session->zstd_compress_level
    );

    if (ZSTD_isError(savestate_transfer_payload->compressed_savestate_size)) {
        SAM2_LOG_ERROR("ZSTD_compress failed: %s\n", ZSTD_getErrorName(savestate_transfer_payload->compressed_savestate_size));
        assert(0);
    }

    savestate_transfer_payload->compressed_options_size = ZSTD_compress(
        savestate_transfer_payload->compressed_data + savestate_transfer_payload->compressed_savestate_size,
        save_state_transfer_payload_compressed_bound_size_bytes - savestate_transfer_payload->compressed_savestate_size,
        session->core_options, sizeof(session->core_options), session->zstd_compress_level
    );

    if (ZSTD_isError(savestate_transfer_payload->compressed_options_size)) {
        SAM2_LOG_ERROR("ZSTD_compress failed: %s\n", ZSTD_getErrorName(savestate_transfer_payload->compressed_options_size));
        assert(0);
    }

    logical_partition(sizeof(savestate_transfer_payload_t) /* Header */ + savestate_transfer_payload->compressed_savestate_size + savestate_transfer_payload->compressed_options_size,
                      FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size_bytes, &packet_groups);
    assert(savestate_transfer_payload_plus_parity_bound_bytes >= packet_groups * n * packet_payload_size_bytes); // If this fails my logic calculating the bounds was just wrong

    savestate_transfer_payload->frame_counter = session->frame_counter;
    savestate_transfer_payload->compressed_savestate_size = savestate_transfer_payload->compressed_savestate_size;
    savestate_transfer_payload->total_size_bytes = sizeof(savestate_transfer_payload_t) + savestate_transfer_payload->compressed_savestate_size;

    uint64_t hash = fnv1a_hash(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes);
    printf("Sending savestate payload with hash: %" PRIx64 " size: %" PRId64 " bytes\n", hash, savestate_transfer_payload->total_size_bytes);

    // Create parity blocks for Reed-Solomon. n - k in total for each packet group
    // We have "packet grouping" because pretty much every implementation of Reed-Solomon doesn't support more than 255 blocks
    // and unfragmented UDP packets over ethernet are limited to PACKET_MTU_PAYLOAD_SIZE_BYTES
    // This makes the code more complicated and the error correcting properties slightly worse but it's a practical tradeoff
    void *rs_code = fec_new(k, n);
    for (int j = 0; j < packet_groups; j++) {
        void *data[255];
        
        for (int i = 0; i < n; i++) {
            data[i] = (unsigned char *) savestate_transfer_payload + logical_partition_offset_bytes(j, i, packet_payload_size_bytes, packet_groups);
        }

        for (int i = k; i < n; i++) {
            fec_encode(rs_code, (void **)data, data[i], i, packet_payload_size_bytes);
        }
    }
    fec_free(rs_code);

    // Send original data blocks and parity blocks
    // @todo I wrote this in such a way that you can do a zero-copy when creating the packets to send
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < packet_groups; j++) {
            savestate_transfer_packet2_t packet;
            packet.channel_and_flags = CHANNEL_SAVESTATE_TRANSFER;
            if (k == 239) {
                packet.channel_and_flags |= SAVESTATE_TRANSFER_FLAG_K_IS_239;
                if (j == 0) {
                    packet.channel_and_flags |= SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0;
                    packet.packet_groups = packet_groups;
                } else {
                    packet.sequence_hi = j;
                }
            } else {
                packet.reed_solomon_k = k;
            }

            packet.sequence_lo = i;

            memcpy(packet.payload, (unsigned char *) savestate_transfer_payload + logical_partition_offset_bytes(j, i, packet_payload_size_bytes, packet_groups), packet_payload_size_bytes);

            int status = juice_send(agent, (char *) &packet, sizeof(savestate_transfer_packet_t) + packet_payload_size_bytes);
            assert(status == 0);
        }
    }

    free(savebuffer);
    free(savestate_transfer_payload);
}
