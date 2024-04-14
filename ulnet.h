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
#define ULNET_PACKET_SIZE_BYTES_MAX 1408

#define ULNET_SPECTATOR_MAX 64
#define ULNET_CORE_OPTIONS_MAX 128
#define ULNET_STATE_PACKET_HISTORY_SIZE 256

#define ULNET_FLAGS_MASK                      0x0F
#define ULNET_CHANNEL_MASK                    0xF0

#define ULNET_CHANNEL_EXTRA                   0x00
#define ULNET_CHANNEL_INPUT                   0x10
#define ULNET_CHANNEL_INPUT_AUDIT_CONSISTENCY 0x20
#define ULNET_CHANNEL_SAVESTATE_TRANSFER      0x30
#define ULNET_CHANNEL_DESYNC_DEBUG            0xF0


#define ULNET_SESSION_FLAG_WAITING_FOR_SAVE_STATE 0x1
#define ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY     0x2

// This constant defines the maximum number of frames that can be buffered before blocking.
// A value of 2 implies no delay can be accomidated.
//```
// Consider the following scenario:
// logical-time | peer a       | peer b
// ------------------------------------------
// 0            | send input 0 | send input 0
// 1            | recv input 0 | recv input 0
// 2            | ------------ | tick frame 0
// 3            | ------------ | send input 1
// 4            | recv input 1 | ------------
// 5            | tick frame 0 | ------------
//```
// The issue occurs at logical-time 4 when peer a receives input 1 before ticking frame 0.
// If the input buffer only holds 1 frame, the input packet for frame 0 would be overwritten.
// To handle the case where a peer immediately ticks and sends an input after receiving,
// the input buffer needs to hold at least 2 frames.
//
// Setting ULNET_DELAY_BUFFER_SIZE to 2 allows for no frame delay while still handling this scenario.
// However, the constant is set to 8 to provide additional buffering capacity if needed.
#define ULNET_DELAY_BUFFER_SIZE 8

#define ULNET_DELAY_FRAMES_MAX (ULNET_DELAY_BUFFER_SIZE/2-1)

const int PortCount = 4;
typedef int16_t FLibretroInputState[64]; // This must be a POD for putting into packets

struct ulnet_core_option_t {
    char key[128];
    char value[128];
};

// @todo This is really sparse so you should just add routines to read values from it in the serialized format
typedef struct {
    int64_t frame;
    FLibretroInputState input_state[ULNET_DELAY_BUFFER_SIZE][PortCount];
    sam2_room_t room_state[ULNET_DELAY_BUFFER_SIZE];
    ulnet_core_option_t core_option[ULNET_DELAY_BUFFER_SIZE]; // Max 1 option per frame provided by the authority
} ulnet_state_t;
SAM2_STATIC_ASSERT(
      sizeof(ulnet_state_t) ==
     (sizeof(ulnet_state_t::frame)
    + sizeof(ulnet_state_t::room_state)
    + sizeof(ulnet_state_t::input_state)
    + sizeof(ulnet_state_t::core_option)),
    "ulnet_state_t is not packed"
);

typedef struct {
    uint8_t channel_and_port;
    uint8_t coded_state[];
} ulnet_state_packet_t;

// @todo Just roll this all into ulnet_state_t
typedef struct {
    uint8_t channel_and_flags;
    uint8_t spacing[7];

    int64_t frame;
    int64_t save_state_hash[ULNET_DELAY_BUFFER_SIZE];
    int64_t input_state_hash[ULNET_DELAY_BUFFER_SIZE];
    //int64_t options_state_hash[ULNET_DELAY_BUFFER_SIZE]; // @todo
} desync_debug_packet_t;

#define FEC_PACKET_GROUPS_MAX 16
#define FEC_REDUNDANT_BLOCKS 16 // ULNET is hardcoded based on this value so it can't really be changed

#define ULNET_SAVESTATE_TRANSFER_FLAG_K_IS_239         0b0001
#define ULNET_SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0 0b0010

// @todo Just get rid of these
#define COMPRESSED_SAVE_STATE_BOUND_BYTES ZSTD_COMPRESSBOUND(20 * 1024 * 1024) // @todo Magic number
#define COMPRESSED_CORE_OPTIONS_BOUND_BYTES ZSTD_COMPRESSBOUND(sizeof(ulnet_core_option_t[ULNET_CORE_OPTIONS_MAX])) // @todo Probably make the type in here a typedef
#define COMPRESSED_DATA_WITH_REDUNDANCY_BOUND_BYTES (255 * (COMPRESSED_SAVE_STATE_BOUND_BYTES + COMPRESSED_CORE_OPTIONS_BOUND_BYTES) / (255 - FEC_REDUNDANT_BLOCKS))

typedef struct {
    uint8_t channel_and_flags;
    union {
        uint8_t reed_solomon_k;
        uint8_t packet_groups;
        uint8_t sequence_hi;
    };

    uint8_t sequence_lo;

    uint8_t payload[]; // Variable size; at most ULNET_PACKET_SIZE_BYTES_MAX-3
} ulnet_save_state_packet_fragment_t;

typedef struct {
    uint8_t channel_and_flags;
    union {
        uint8_t reed_solomon_k;
        uint8_t packet_groups;
        uint8_t sequence_hi;
    };

    uint8_t sequence_lo;

    uint8_t payload[ULNET_PACKET_SIZE_BYTES_MAX-3]; // Variable size; at most ULNET_PACKET_SIZE_BYTES_MAX-3
} ulnet_save_state_packet_fragment2_t;
SAM2_STATIC_ASSERT(sizeof(ulnet_save_state_packet_fragment2_t) == ULNET_PACKET_SIZE_BYTES_MAX, "Savestate transfer is the wrong size");

typedef struct {
    int64_t total_size_bytes; // @todo This isn't necessary
    int64_t frame_counter;
    sam2_room_t room;
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
    sam2_room_t room_we_are_in_future;

    ulnet_core_option_t core_options[ULNET_CORE_OPTIONS_MAX]; // @todo I don't like this here

    // @todo Change these so they're all peer_*
    juice_agent_t *agent               [SAM2_PORT_MAX + 1 /* Plus Authority */ + ULNET_SPECTATOR_MAX];
    uint64_t       agent_peer_id       [SAM2_PORT_MAX + 1 /* Plus Authority */ + ULNET_SPECTATOR_MAX];
    int64_t        peer_desynced_frame [SAM2_PORT_MAX + 1 /* Plus Authority */ + ULNET_SPECTATOR_MAX];
    ulnet_state_t  state               [SAM2_PORT_MAX + 1 /* Plus Authority */];
    unsigned char  state_packet_history[SAM2_PORT_MAX + 1 /* Plus Authority */][ULNET_STATE_PACKET_HISTORY_SIZE][ULNET_PACKET_SIZE_BYTES_MAX];

    int64_t spectator_count;

    desync_debug_packet_t desync_debug_packet;

    unsigned char remote_savestate_transfer_packets[COMPRESSED_DATA_WITH_REDUNDANCY_BOUND_BYTES + FEC_PACKET_GROUPS_MAX * (GF_SIZE - FEC_REDUNDANT_BLOCKS) * sizeof(ulnet_save_state_packet_fragment_t)];
    int64_t remote_savestate_transfer_offset;
    uint8_t remote_packet_groups; // This is used to bookkeep how much data we actually need to receive to reform the complete savestate
    void *fec_packet[FEC_PACKET_GROUPS_MAX][GF_SIZE - FEC_REDUNDANT_BLOCKS];
    int fec_index[FEC_PACKET_GROUPS_MAX][GF_SIZE - FEC_REDUNDANT_BLOCKS];
    int fec_index_counter[FEC_PACKET_GROUPS_MAX]; // Counts packets received in each "packet group"

    void *user_ptr;
    int (*sam2_send_callback)(void *user_ptr, char *response);
    int (*populate_core_options_callback)(void *user_ptr, ulnet_core_option_t options[ULNET_CORE_OPTIONS_MAX]);

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

void MovePeer(ulnet_session_t *session, int peer_existing_port, int peer_new_port) {
    assert(peer_existing_port != peer_new_port);
    assert(session->agent[peer_new_port] == NULL);
    assert(session->agent_peer_id[peer_new_port] <= SAM2_PORT_SENTINELS_MAX);
    assert(session->agent[peer_existing_port] != NULL);

    if (peer_existing_port > SAM2_AUTHORITY_INDEX) {
        session->agent_peer_id[peer_existing_port] = session->agent_peer_id[--session->spectator_count];
    }

    session->agent[peer_new_port] = session->agent[peer_existing_port];
    session->agent_peer_id[peer_new_port] = session->agent_peer_id[peer_existing_port];
    session->agent_peer_id[peer_existing_port] = 0;
    session->agent[peer_existing_port] = NULL;
}

bool ulnet_is_authority(ulnet_session_t *session) {
    return    session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]
           || session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] == 0;
}

bool ulnet_is_spectator(ulnet_session_t *session, uint64_t peer_id) {
    return    session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_INITIALIZED
           && sam2_get_port_of_peer(&session->room_we_are_in, peer_id) == -1;
}

void ulnet_send_save_state(ulnet_session_t *session, juice_agent_t *agent);

// Interactive-Connectivity-Establishment callbacks that libjuice calls
static void on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr);
static void on_candidate(juice_agent_t *agent, const char *sdp, void *user_ptr);
static void on_gathering_done(juice_agent_t *agent, void *user_ptr);
static void ulnet_receive_packet_callback(juice_agent_t *agent, const char *data, size_t size, void *user_ptr); // We get all our packets in this one

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
    config.cb_recv = ulnet_receive_packet_callback;

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
        assert(port != -1);

        return port;
    } else {
        return SAM2_AUTHORITY_INDEX;
    }
}

// On state changed
static void on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p;
    SAM2_LOCATE(session->agent, agent, p);

    if (   state == JUICE_STATE_CONNECTED
        && session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
        SAM2_LOG_INFO("Sending savestate to peer %016" PRIx64, session->our_peer_id);
        // @todo Probably this should be changed to a needs save state flag since this function is called from a callback
        //       but it might not matter
        ulnet_send_save_state(session, agent);
    } else if (state == JUICE_STATE_FAILED) {
        if (p >= SAM2_PORT_MAX+1) {
            SAM2_LOG_INFO("Spectator %016" PRIx64 " left" , session->room_we_are_in.peer_ids[p]);
            session->agent_peer_id[p] = session->our_peer_id;
        } else {

        }

    }
}

// On local candidate gathered
static void on_candidate(juice_agent_t *agent, const char *sdp, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p;
    SAM2_LOCATE(session->agent, agent, p);
    if (p == -1) {
        SAM2_LOG_ERROR("No agent found");
        return;
    }

    sam2_signal_message_t response = { SAM2_SIGN_HEADER };

    response.peer_id = session->agent_peer_id[p];
    if (strlen(sdp) < sizeof(response.ice_sdp)) {
        strcpy(response.ice_sdp, sdp);
        session->sam2_send_callback(session->user_ptr, (char *) &response);
    } else {
        SAM2_LOG_ERROR("Candidate too large");
        return;
    }
}

// On local candidates gathering done
static void on_gathering_done(juice_agent_t *agent, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p;
    SAM2_LOCATE(session->agent, agent, p);
    if (p == -1) {
        SAM2_LOG_ERROR("No agent found");
        return;
    }

    sam2_signal_message_t response = { SAM2_SIGN_HEADER };

    response.peer_id = session->agent_peer_id[p];
    session->sam2_send_callback(session->user_ptr, (char *) &response);
}

static inline void ulnet_disconnect_peer(ulnet_session_t *session, int peer_port) {
    if (session->agent[peer_port]) {
        juice_destroy(session->agent[peer_port]);
        session->agent[peer_port] = NULL;
    }

    session->agent_peer_id[peer_port] = 0;
    session->peer_desynced_frame[peer_port] = 0;
}

static inline void ulnet_reset_save_state_bookkeeping(ulnet_session_t *session) {
    session->remote_packet_groups = FEC_PACKET_GROUPS_MAX;
    session->remote_savestate_transfer_offset = 0;
    memset(session->fec_index_counter, 0, sizeof(session->fec_index_counter));
    session->flags &= ~ULNET_SESSION_FLAG_WAITING_FOR_SAVE_STATE;
}

static inline void ulnet_session_init_defaulted(ulnet_session_t *session) {
    memset(&session->state, 0, sizeof(session->state));
    memset(&session->state_packet_history, 0, sizeof(session->state_packet_history));

    ulnet_reset_save_state_bookkeeping(session);
}

int ulnet_poll_session(ulnet_session_t *session, sam2_message_u *response) {

    if (sam2_get_metadata((char *) response) == NULL) {
        return -1;
    }

    if (memcmp(response, sam2_make_header, SAM2_HEADER_TAG_SIZE) == 0) {
        sam2_room_make_message_t *room_make = (sam2_room_make_message_t *) response;
        assert(session->our_peer_id == room_make->room.peer_ids[SAM2_AUTHORITY_INDEX]);
        session->room_we_are_in = session->room_we_are_in_future = room_make->room;
    } else if (memcmp(response, sam2_conn_header, SAM2_HEADER_TAG_SIZE) == 0) {
        sam2_connect_message_t *connect_message = (sam2_connect_message_t *) response;
        SAM2_LOG_INFO("We were assigned the peer id %" PRIx64, connect_message->peer_id);

        session->our_peer_id = connect_message->peer_id;
        session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session->our_peer_id;
    } else if (memcmp(response, sam2_join_header, SAM2_HEADER_TAG_SIZE) == 0) {
        if (!ulnet_is_authority(session)) {
            SAM2_LOG_FATAL("We shouldn't get here anymore"); // @todo Make error instead
        }
        sam2_room_join_message_t *room_join = (sam2_room_join_message_t *) response;

        SAM2_LOG_INFO("Peer %" PRIx64 " has asked to change something about the room in some way e.g. leaving, joining, etc.", room_join->peer_id);
        assert(sam2_same_room(&session->room_we_are_in_future, &room_join->room));

        int current_port = sam2_get_port_of_peer(&session->room_we_are_in_future, room_join->peer_id);
        int desired_port = sam2_get_port_of_peer(&room_join->room, room_join->peer_id);

        if (desired_port == -1) {
            if (current_port != -1) {
                SAM2_LOG_INFO("Peer %" PRIx64 " left", room_join->peer_id);
                ulnet_disconnect_peer(session, current_port);

                session->room_we_are_in_future.peer_ids[current_port] = SAM2_PORT_AVAILABLE;

                sam2_room_join_message_t join_message = {
                    SAM2_JOIN_HEADER,
                    session->room_we_are_in_future
                };

                session->sam2_send_callback(session->user_ptr, (char *) &join_message);
            } else {
                SAM2_LOG_WARN("Peer %" PRIx64 " did something that doesn't look like joining or leaving", room_join->peer_id);

                sam2_error_message_t error = {
                    SAM2_FAIL_HEADER,
                    SAM2_RESPONSE_AUTHORITY_ERROR,
                    "Client made unsupported join request",
                    room_join->peer_id
                };

                session->sam2_send_callback(session->user_ptr, (char *) &error);
            }
        } else {
            if (current_port != desired_port) {
                if (session->room_we_are_in_future.peer_ids[desired_port] != SAM2_PORT_AVAILABLE) {
                    SAM2_LOG_INFO("Peer %" PRIx64 " tried to join on unavailable port", room_join->room.peer_ids[current_port]);
                    sam2_error_message_t error = {
                        SAM2_FAIL_HEADER,
                        SAM2_RESPONSE_AUTHORITY_ERROR,
                        "Peer tried to join on unavailable port",
                        room_join->peer_id
                    };

                    session->sam2_send_callback(session->user_ptr, (char *) &error);
                } else {
                    session->room_we_are_in_future.peer_ids[desired_port] = room_join->peer_id;

                    if (current_port != -1) {
                        session->room_we_are_in_future.peer_ids[current_port] = SAM2_PORT_AVAILABLE;
                    }

                    sam2_room_join_message_t join_message = {
                        SAM2_JOIN_HEADER,
                        session->room_we_are_in_future
                    };

                    session->sam2_send_callback(session->user_ptr, (char *) &join_message);
                }
            }
        }
    } else if (memcmp(response, sam2_sign_header, SAM2_HEADER_TAG_SIZE) == 0) {
        sam2_signal_message_t *room_signal = (sam2_signal_message_t *) response;
        SAM2_LOG_INFO("Received signal from peer %" PRIx64 "", room_signal->peer_id);

        int p;
        SAM2_LOCATE(session->agent_peer_id, room_signal->peer_id, p);

        if (p == -1) {
            SAM2_LOG_INFO("Received signal from unknown peer");

            if (session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
                if (session->spectator_count == ULNET_SPECTATOR_MAX) {
                    SAM2_LOG_WARN("We can't let them in as a spectator there are too many spectators");

                    static sam2_error_message_t error = { 
                        SAM2_FAIL_HEADER,
                        SAM2_RESPONSE_AUTHORITY_ERROR,
                        "Authority has reached the maximum number of spectators"
                    };

                    session->sam2_send_callback(session->user_ptr, (char *) &error);
                } else {
                    SAM2_LOG_INFO("We are letting them in as a spectator");

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
                SAM2_LOG_WARN("Received unknown signal when we weren't the authority");

                static sam2_error_message_t error = { 
                    SAM2_FAIL_HEADER,
                    SAM2_RESPONSE_AUTHORITY_ERROR,
                    "Received unknown signal when we weren't the authority",
                    room_signal->peer_id
                };

                session->sam2_send_callback(session->user_ptr, (char *) &error);
            }
        }

        if (p != -1) {
            if (strlen(room_signal->ice_sdp) == 0) {
                SAM2_LOG_INFO("Received remote gathering done from peer %" PRIx64 "", room_signal->peer_id);
                juice_set_remote_gathering_done(session->agent[p]);
            } else if (strncmp(room_signal->ice_sdp, "a=ice", strlen("a=ice")) == 0) {
                juice_set_remote_description(session->agent[p], room_signal->ice_sdp);
            } else if (strncmp(room_signal->ice_sdp, "a=candidate", strlen("a=candidate")) == 0) {
                juice_add_remote_candidate(session->agent[p], room_signal->ice_sdp);
            } else {
                SAM2_LOG_ERROR("Unable to parse signal message '%s'", room_signal->ice_sdp);
            }
        }
    }

    return 0;
}

static void ulnet_receive_packet_callback(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p;
    SAM2_LOCATE(session->agent, agent, p);

    if (p == -1) {
        SAM2_LOG_ERROR("No agent associated for packet on channel 0x%" PRIx8 "", data[0] & ULNET_CHANNEL_MASK);
        return;
    }

    if (size == 0) {
        SAM2_LOG_WARN("Received a UDP packet with no payload");
        return;
    }

    if (   p >= SAM2_PORT_MAX+1
        && ULNET_CHANNEL_INPUT != (data[0] & ULNET_CHANNEL_MASK)) {
        SAM2_LOG_WARN("A spectator sent us a UDP packet for unsupported channel %" PRIx8 " for some reason", data[0] & ULNET_CHANNEL_MASK);
        return;
    }

    uint8_t channel_and_flags = data[0];
    switch (channel_and_flags & ULNET_CHANNEL_MASK) {
    case ULNET_CHANNEL_EXTRA: {
        assert(!"This is an error currently\n");
    }
    case ULNET_CHANNEL_INPUT: {
        assert(size <= ULNET_PACKET_SIZE_BYTES_MAX);

        ulnet_state_packet_t *input_packet = (ulnet_state_packet_t *) data;
        int8_t original_sender_port = data[0] & ULNET_FLAGS_MASK;

        if (   p != original_sender_port
            && p != SAM2_AUTHORITY_INDEX) {
            SAM2_LOG_WARN("Non-authority gave us someones input eventually this should be verified with a signature");
        }

        if (original_sender_port >= SAM2_PORT_MAX+1) {
            SAM2_LOG_WARN("Received input packet for port %d which is out of range", original_sender_port);
            break;
        }

        if (rle8_decode_size(input_packet->coded_state, size - 1) != sizeof(ulnet_state_t)) {
            SAM2_LOG_WARN("Received input packet with an invalid decode size");
            break;
        }

        int64_t frame;
        rle8_decode(input_packet->coded_state, size - 1, (uint8_t *) &frame, sizeof(frame));

        SAM2_LOG_DEBUG("Recv input packet for frame %" PRId64 " from peer_ids[%d]=%" PRIx64 "",
            frame, original_sender_port, session->room_we_are_in.peer_ids[original_sender_port]);

        if (frame < session->state[original_sender_port].frame) {
            // UDP packets can arrive out of order this is normal
            SAM2_LOG_DEBUG("Received outdated input packet for frame %" PRId64 ". We are already on frame %" PRId64 ". Dropping it",
                frame, session->state[original_sender_port].frame);
        } else {
            rle8_decode(
                input_packet->coded_state, size - 1,
                (uint8_t *) &session->state[original_sender_port], sizeof(ulnet_state_t)
            );

            // Store the input packet in the history buffer. Arbitrary zero runs decode to no bytes conveniently so we don't need to store the packet size
            int i = 0;
            for (; i < size; i++) {
                session->state_packet_history[original_sender_port][frame % ULNET_STATE_PACKET_HISTORY_SIZE][i] = data[i];
            }

            for (; i < SAM2_ARRAY_LENGTH(session->state_packet_history[0][0]); i++) {
                session->state_packet_history[original_sender_port][frame % ULNET_STATE_PACKET_HISTORY_SIZE][i] = 0;
            }

            // Broadcast the input packet to spectators
            if (ulnet_is_authority(session)) {
                for (int i = 0; i < ULNET_SPECTATOR_MAX; i++) {
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
    case ULNET_CHANNEL_DESYNC_DEBUG: {
        // @todo This channel doesn't receive messages reliably, but I think it should be changed to in the same manner as the input channel
        assert(size == sizeof(desync_debug_packet_t));

        desync_debug_packet_t their_desync_debug_packet;
        memcpy(&their_desync_debug_packet, data, sizeof(desync_debug_packet_t)); // Strict-aliasing

        desync_debug_packet_t &our_desync_debug_packet = session->desync_debug_packet;

        int64_t latest_common_frame = SAM2_MIN(our_desync_debug_packet.frame, their_desync_debug_packet.frame);
        int64_t frame_difference = SAM2_ABS(our_desync_debug_packet.frame - their_desync_debug_packet.frame);
        int64_t total_frames_to_compare = ULNET_DELAY_BUFFER_SIZE - frame_difference;
        for (int f = total_frames_to_compare-1; f >= 0 ; f--) {
            int64_t frame_to_compare = latest_common_frame - f;
            int64_t frame_index = frame_to_compare % ULNET_DELAY_BUFFER_SIZE;

            if (our_desync_debug_packet.input_state_hash[frame_index] != their_desync_debug_packet.input_state_hash[frame_index]) {
                SAM2_LOG_ERROR("Input state hash mismatch for frame %" PRId64 " Our hash: %" PRIx64 " Their hash: %" PRIx64 "", 
                    frame_to_compare, our_desync_debug_packet.input_state_hash[frame_index], their_desync_debug_packet.input_state_hash[frame_index]);
            } else if (   our_desync_debug_packet.save_state_hash[frame_index]
                       && their_desync_debug_packet.save_state_hash[frame_index]) {

                if (our_desync_debug_packet.save_state_hash[frame_index] != their_desync_debug_packet.save_state_hash[frame_index]) {
                    if (!session->peer_desynced_frame[p]) {
                        session->peer_desynced_frame[p] = frame_to_compare;
                    }

                    SAM2_LOG_ERROR("Save state hash mismatch for frame %" PRId64 " Our hash: %" PRIx64 " Their hash: %" PRIx64 "",
                        frame_to_compare, our_desync_debug_packet.save_state_hash[frame_index], their_desync_debug_packet.save_state_hash[frame_index]);
                } else if (session->peer_desynced_frame[p]) {
                    session->peer_desynced_frame[p] = 0;
                    SAM2_LOG_INFO("Peer resynced frame on frame %" PRId64 "", frame_to_compare);
                }
            }
        }

        break;
    }
    case ULNET_CHANNEL_SAVESTATE_TRANSFER: {
        if (session->remote_packet_groups == 0) {
            // This is kind of a hack. Since every field in ulnet_session can just be zero-inited
            // besides this one. I just use this check here to set it to it's correct initial value
            session->remote_packet_groups = FEC_PACKET_GROUPS_MAX;
        }

        if (session->agent[SAM2_AUTHORITY_INDEX] != agent) {
            printf("Received savestate transfer packet from non-authority agent\n");
            break;
        }

        if (size < sizeof(ulnet_save_state_packet_fragment_t)) {
            SAM2_LOG_WARN("Recv savestate transfer packet with size smaller than header");
            break;
        }

        if (size > ULNET_PACKET_SIZE_BYTES_MAX) {
            SAM2_LOG_WARN("Recv savestate transfer packet potentially larger than MTU");
        }

        ulnet_save_state_packet_fragment_t savestate_transfer_header;
        memcpy(&savestate_transfer_header, data, sizeof(ulnet_save_state_packet_fragment_t)); // Strict-aliasing

        uint8_t sequence_hi = 0;
        int k = 239;
        if (channel_and_flags & ULNET_SAVESTATE_TRANSFER_FLAG_K_IS_239) {
            if (channel_and_flags & ULNET_SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0) {
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

        SAM2_LOG_DEBUG("Received savestate packet sequence_hi: %hhu sequence_lo: %hhu", sequence_hi, sequence_lo);

        uint8_t *copied_packet_ptr = (uint8_t *) memcpy(session->remote_savestate_transfer_packets + session->remote_savestate_transfer_offset, data, size);
        session->fec_packet[sequence_hi][sequence_lo] = copied_packet_ptr + sizeof(ulnet_save_state_packet_fragment_t);
        session->remote_savestate_transfer_offset += size;

        session->fec_index[sequence_hi][session->fec_index_counter[sequence_hi]++] = sequence_lo;

        if (session->fec_index_counter[sequence_hi] == k) {
            SAM2_LOG_DEBUG("Received all the savestate data for packet group: %hhu", sequence_hi);

            int redudant_blocks_sent = k * FEC_REDUNDANT_BLOCKS / (GF_SIZE - FEC_REDUNDANT_BLOCKS);
            void *rs_code = fec_new(k, k + redudant_blocks_sent);
            int rs_block_size = (int) (size - sizeof(ulnet_save_state_packet_fragment_t));
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

                SAM2_LOG_INFO("Received savestate transfer payload for frame %" PRId64 "", savestate_transfer_payload->frame_counter);

                size_t ret = ZSTD_decompress(
                    session->core_options, sizeof(session->core_options),
                    savestate_transfer_payload->compressed_data + savestate_transfer_payload->compressed_savestate_size,
                    savestate_transfer_payload->compressed_options_size
                );

                unsigned char *save_state_data = NULL;
                if (ZSTD_isError(ret)) {
                    SAM2_LOG_ERROR("Error decompressing core options: %s", ZSTD_getErrorName(ret));
                } else {
                    session->flags |= ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY;
                    //session.retro_run(); // Apply options before loading savestate; Lets hope this isn't necessary

                    int64_t remote_savestate_hash = fnv1a_hash(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes);
                    SAM2_LOG_INFO("Received savestate payload with hash: %llx size: %llu bytes",
                        remote_savestate_hash, savestate_transfer_payload->total_size_bytes);

                    save_state_data = (unsigned char *) malloc(session->retro_serialize_size());

                    int64_t save_state_size = ZSTD_decompress(
                        save_state_data,
                        session->retro_serialize_size(),
                        savestate_transfer_payload->compressed_data,
                        savestate_transfer_payload->compressed_savestate_size
                    );

                    if (ZSTD_isError(save_state_size)) {
                        SAM2_LOG_ERROR("Error decompressing savestate: %s", ZSTD_getErrorName(save_state_size));
                    } else {
                        if (!session->retro_unserialize(save_state_data, save_state_size)) {
                            SAM2_LOG_ERROR("Failed to load savestate");
                        } else {
                            SAM2_LOG_DEBUG("Save state loaded");
                            session->frame_counter = savestate_transfer_payload->frame_counter;
                            session->room_we_are_in = savestate_transfer_payload->room;
                        }
                    }
                }

                if (save_state_data != NULL) {
                    free(save_state_data);
                }

                free(savestate_transfer_payload);

                ulnet_reset_save_state_bookkeeping(session);
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

    int packet_payload_size_bytes = ULNET_PACKET_SIZE_BYTES_MAX - sizeof(ulnet_save_state_packet_fragment_t);
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
        SAM2_LOG_ERROR("ZSTD_compress failed: %s", ZSTD_getErrorName(savestate_transfer_payload->compressed_savestate_size));
        assert(0);
    }

    savestate_transfer_payload->compressed_options_size = ZSTD_compress(
        savestate_transfer_payload->compressed_data + savestate_transfer_payload->compressed_savestate_size,
        save_state_transfer_payload_compressed_bound_size_bytes - savestate_transfer_payload->compressed_savestate_size,
        session->core_options, sizeof(session->core_options), session->zstd_compress_level
    );

    if (ZSTD_isError(savestate_transfer_payload->compressed_options_size)) {
        SAM2_LOG_ERROR("ZSTD_compress failed: %s", ZSTD_getErrorName(savestate_transfer_payload->compressed_options_size));
        assert(0);
    }

    logical_partition(sizeof(savestate_transfer_payload_t) /* Header */ + savestate_transfer_payload->compressed_savestate_size + savestate_transfer_payload->compressed_options_size,
                      FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size_bytes, &packet_groups);
    assert(savestate_transfer_payload_plus_parity_bound_bytes >= packet_groups * n * packet_payload_size_bytes); // If this fails my logic calculating the bounds was just wrong

    savestate_transfer_payload->frame_counter = session->frame_counter;
    savestate_transfer_payload->room = session->room_we_are_in;
    savestate_transfer_payload->compressed_savestate_size = savestate_transfer_payload->compressed_savestate_size;
    savestate_transfer_payload->total_size_bytes = sizeof(savestate_transfer_payload_t) + savestate_transfer_payload->compressed_savestate_size;

    uint64_t hash = fnv1a_hash(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes);
    printf("Sending savestate payload with hash: %" PRIx64 " size: %" PRId64 " bytes\n", hash, savestate_transfer_payload->total_size_bytes);

    // Create parity blocks for Reed-Solomon. n - k in total for each packet group
    // We have "packet grouping" because pretty much every implementation of Reed-Solomon doesn't support more than 255 blocks
    // and unfragmented UDP packets over ethernet are limited to ULNET_PACKET_SIZE_BYTES_MAX
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
            ulnet_save_state_packet_fragment2_t packet;
            packet.channel_and_flags = ULNET_CHANNEL_SAVESTATE_TRANSFER;
            if (k == 239) {
                packet.channel_and_flags |= ULNET_SAVESTATE_TRANSFER_FLAG_K_IS_239;
                if (j == 0) {
                    packet.channel_and_flags |= ULNET_SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0;
                    packet.packet_groups = packet_groups;
                } else {
                    packet.sequence_hi = j;
                }
            } else {
                packet.reed_solomon_k = k;
            }

            packet.sequence_lo = i;

            memcpy(packet.payload, (unsigned char *) savestate_transfer_payload + logical_partition_offset_bytes(j, i, packet_payload_size_bytes, packet_groups), packet_payload_size_bytes);

            int status = juice_send(agent, (char *) &packet, sizeof(ulnet_save_state_packet_fragment_t) + packet_payload_size_bytes);
            assert(status == 0);
        }
    }

    free(savebuffer);
    free(savestate_transfer_payload);
}
