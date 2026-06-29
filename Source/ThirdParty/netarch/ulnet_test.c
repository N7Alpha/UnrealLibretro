#define SAM2_ENABLE_LOGGING
#define ULNET_TRANSPORT_CUSTOM // The tests dispatch the transport interface on a global bool (inproc vs ICE-lite)
#include "ulnet.h"
#include "sam2.h"

#ifndef ULNET__TEST_SAM2_PORT
#define ULNET__TEST_SAM2_PORT (SAM2_SERVER_DEFAULT_PORT + 1)
#endif

// An arbitrary occupied slot for the single spectator used by the inproc reliable-channel tests.
// (Role is per-port topology now, so any free non-authority slot works; this just needs to be stable.)
#define ULNET__TEST_SPECTATOR_PORT 9
#define ULNET__TEST_PLAYER1_PORT 1
#define ULNET__TEST_PLAYER2_PORT 33
#define ULNET__TEST_RELAY_SPECTATOR_PORT 2

extern ulnet_nat_agent_t *ulnet__nat_create(ulnet_session_t *session, int peer_port);
extern void ulnet__process_udp_packet(ulnet_session_t *session, int p, const uint8_t *data, size_t size);

static const char *g_test_name = "";
static bool g_ulnet_transport_use_inproc = false;

typedef struct ulnet_transport_inproc_buffer {
    uint8_t msg[256][ULNET_PACKET_SIZE_BYTES_MAX];
    uint16_t msg_size[256];
    int32_t count;
} ulnet_inproc_buf_t;

typedef struct ulnet_transport_inproc {
    ulnet_inproc_buf_t buf1; // Smaller peer_id -> larger peer_id
    ulnet_inproc_buf_t buf2; // Larger peer_id -> smaller peer_id
} ulnet_transport_inproc_t;

typedef struct ulnet_inproc_conn {
    ulnet_transport_inproc_t *shared; // Owned by the test, not by the connection
    ulnet_session_t *session;
    int port;
} ulnet_inproc_conn_t;

// Wire one side of an inproc link: give session->peer[port] a connection over the shared buffer pair.
void ulnet__test_inproc_wire(ulnet_session_t *session, int port, ulnet_transport_inproc_t *shared);

static ulnet_inproc_buf_t *ulnet__inproc_tx(ulnet_inproc_conn_t *c) {
    return c->session->our_peer_id < c->session->room_we_are_in.peer_ids[c->port] ? &c->shared->buf1 : &c->shared->buf2;
}
static ulnet_inproc_buf_t *ulnet__inproc_rx(ulnet_inproc_conn_t *c) {
    return c->session->our_peer_id < c->session->room_we_are_in.peer_ids[c->port] ? &c->shared->buf2 : &c->shared->buf1;
}

static ulnet_transport_conn_t *ulnet_transport_inproc_open(ulnet_session_t *session, int port, const char *remote_signal) {
    (void)session; (void)port; (void)remote_signal;
    return NULL; // inproc links both sides out of band via ulnet__test_inproc_wire()
}
static void ulnet_transport_inproc_close(ulnet_transport_conn_t *conn) { free(conn); }
static int ulnet_transport_inproc_send(ulnet_transport_conn_t *conn, const uint8_t *packet, size_t size) {
    ulnet_inproc_buf_t *buf = ulnet__inproc_tx((ulnet_inproc_conn_t *)conn);
    if (buf->count >= (int)(sizeof(buf->msg) / sizeof(buf->msg[0]))) {
        SAM2_LOG_FATAL("Inproc transport buffer is full, cannot send packet");
        return -1;
    }
    buf->msg_size[buf->count] = (uint16_t)size;
    memcpy(buf->msg[buf->count], packet, size);
    buf->count++;
    return 0;
}
static ulnet_transport_state_t ulnet_transport_inproc_state(const ulnet_transport_conn_t *conn) { (void)conn; return ULNET_TRANSPORT_READY; }
static int ulnet_transport_inproc_signal(ulnet_transport_conn_t *conn, const char *signal) { (void)conn; (void)signal; return 0; }
static int ulnet_transport_inproc_service(ulnet_session_t *session, int timeout_milliseconds) {
    int processed_packets = 0;
    for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
        if (!session->peer[p] || !session->peer[p]->transport) continue;
        ulnet_inproc_buf_t *buf = ulnet__inproc_rx((ulnet_inproc_conn_t *)session->peer[p]->transport);
        for (int i = 0; i < buf->count; i++) {
            ulnet_receive_packet(session, p, buf->msg[i], buf->msg_size[i]);
            processed_packets++;
        }
        buf->count = 0;
    }
    if (timeout_milliseconds > 0 && processed_packets == 0) ulnet__sleep((unsigned int)timeout_milliseconds);
    return processed_packets;
}

void ulnet__test_inproc_wire(ulnet_session_t *session, int port, ulnet_transport_inproc_t *shared) {
    if (!session->peer[port]) ulnet__peer_alloc(session, port);
    ulnet_peer_t *peer = session->peer[port];
    ulnet_inproc_conn_t *conn = (ulnet_inproc_conn_t *)calloc(1, sizeof(*conn));
    conn->shared = shared;
    conn->session = session;
    conn->port = port;
    peer->transport = (ulnet_transport_conn_t *)conn;
}

// The dispatcher: the protocol core calls these; route to inproc or the built-in ICE-lite backend.
ulnet_transport_conn_t *ulnet_transport_open(ulnet_session_t *session, int port, const char *remote_signal) {
    return g_ulnet_transport_use_inproc ? ulnet_transport_inproc_open(session, port, remote_signal)
                                        : ulnet_icelite_open(session, port, remote_signal);
}
void ulnet_transport_close(ulnet_transport_conn_t *conn) {
    if (g_ulnet_transport_use_inproc) ulnet_transport_inproc_close(conn); else ulnet_icelite_close(conn);
}
int ulnet_transport_send(ulnet_transport_conn_t *conn, const uint8_t *packet, size_t size) {
    return g_ulnet_transport_use_inproc ? ulnet_transport_inproc_send(conn, packet, size)
                                        : ulnet_icelite_send(conn, packet, size);
}
ulnet_transport_state_t ulnet_transport_state(const ulnet_transport_conn_t *conn) {
    return g_ulnet_transport_use_inproc ? ulnet_transport_inproc_state(conn) : ulnet_icelite_state(conn);
}
int ulnet_transport_signal(ulnet_transport_conn_t *conn, const char *signal) {
    return g_ulnet_transport_use_inproc ? ulnet_transport_inproc_signal(conn, signal)
                                        : ulnet_icelite_signal(conn, signal);
}
int ulnet_transport_service(ulnet_session_t *session, int timeout_milliseconds) {
    return g_ulnet_transport_use_inproc ? ulnet_transport_inproc_service(session, timeout_milliseconds)
                                        : ulnet_icelite_service(session, timeout_milliseconds);
}

int ulnet__test_forward_messages(sam2_server_t *server, ulnet_session_t *session, sam2_socket_t socket) {
    g_test_name = __func__;
    int status;
    sam2_message_u message;

    for (;;) {
        status = sam2_server_poll(server);
        if (status < 0) {
            SAM2_LOG_ERROR("Error polling sam2 server: %d", status);
            return status;
        }

        status = sam2_client_poll(socket, &message);
        if (status < 0) {
            SAM2_LOG_ERROR("Error polling sam2 client: %d", status);
            return status;
        } else if (status > 0) {
            status = ulnet_process_message(session, (const char *)&message);
            if (status < 0) {
                SAM2_LOG_ERROR("Error processing message: %d", status);
                return status;
            }
        } else {
            break;
        }
    }

    return 0;
}

int ulnet__test_sam2_send_callback(void *socket, char *message) {
    sam2_socket_t sam2_socket = *((sam2_socket_t *) socket);
    return sam2_client_send(sam2_socket, message);
}

void ulnet__test_retro_run(void *user_ptr) {

}

static const char g_serialize_test_data[] = {'T', 'E', 'S', 'T'};
size_t ulnet__test_retro_serialize_size(void *user_ptr) {
    return sizeof(g_serialize_test_data);
}

bool ulnet__test_retro_serialize(void *user_ptr, void *data, size_t size) {
    if (size < sizeof(g_serialize_test_data)) {
        SAM2_LOG_ERROR("Buffer too small for serialization");
        return false;
    }
    memcpy(data, g_serialize_test_data, sizeof(g_serialize_test_data));
    return true;
}

bool ulnet__test_retro_unserialize(void *user_ptr, const void *data, size_t size) {
    return memcmp(data, g_serialize_test_data, size) == 0;
}

static const uint8_t *g_large_savestate;
static size_t g_large_savestate_size;
static bool g_large_savestate_received;

static bool ulnet__test_large_retro_unserialize(void *user_ptr, const void *data, size_t size) {
    (void)user_ptr;
    g_large_savestate_received =
        size == g_large_savestate_size && memcmp(data, g_large_savestate, size) == 0;
    return g_large_savestate_received;
}

int ulnet__test_discard_send_callback(void *user_ptr, char *message) {
    (void)user_ptr;
    (void)message;
    return 0;
}

// A port is a fixed peer identity: promoting the same peer in place keeps its agent and per-port
// state, while a change of occupant fully reconstructs the agent (it is never swapped or reused).
static int ulnet_test_no_slot_swap(void) {
    g_test_name = __func__;
    ulnet_session_t session;
    memset(&session, 0, sizeof(session));
    ulnet_session_init_defaulted(&session);
    g_ulnet_transport_use_inproc = false;
    session.our_peer_id = 10001;
    session.sam2_send_callback = ulnet__test_discard_send_callback;
    session.room_we_are_in.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session.our_peer_id;

    const int slot = 3; // A low port so it is eligible to become a p2p player
    session.room_we_are_in.peer_ids[slot] = 30002; // present as a spectator (topology bit clear)
    ulnet__peer_alloc(&session, slot);
    session.peer[slot]->transport = ulnet_transport_open(&session, slot, NULL);
    if (!session.peer[slot]->transport) {
        SAM2_LOG_ERROR("Unable to create test transport connection");
        return 1;
    }
    session.peer[slot]->reliable_rx_head = 0x1234;
    ulnet_transport_conn_t *agent_before = session.peer[slot]->transport;

    int failed = 0;

    // Promotion in place: same peer gains the topology bit -> agent and per-port state are preserved
    sam2_room_t promoted = session.room_we_are_in;
    promoted.peer_topology |= (1ULL << slot);
    ulnet__reconcile_connections(&session, &promoted);
    failed |= session.peer[slot]->transport != agent_before;
    failed |= !(session.room_we_are_in.peer_topology & (1ULL << slot));
    failed |= session.peer[slot]->reliable_rx_head != 0x1234;
    if (failed) {
        SAM2_LOG_ERROR("Promotion in place must not reconstruct the agent or reset per-port state");
        if (session.peer[slot]->transport) ulnet_disconnect_peer(&session, slot);
        return 1;
    }

    // Occupant change: a different peer takes the slot -> the old agent is fully torn down and rebuilt
    sam2_room_t replaced = session.room_we_are_in;
    replaced.peer_ids[slot] = 30003;
    ulnet__reconcile_connections(&session, &replaced);
    failed |= session.room_we_are_in.peer_ids[slot] != 30003;
    failed |= session.peer[slot]->reliable_rx_head != 0; // proves disconnect_peer ran (full reconstruct)

    if (session.peer[slot]->transport) ulnet_disconnect_peer(&session, slot);
    if (failed) {
        SAM2_LOG_ERROR("Changing a port's occupant must fully reconstruct, not reuse, the agent");
        return 1;
    }

    return 0;
}

// Regression: a freshly connected solo session must be an active p2p player so the core gets real
// input. The connect handler seats us at the authority port; if it forgets the topology bit we fall
// into the coordinator-only-authority path and feed the core zeroed input ("input doesn't work until
// you make a room"). See the solo-room fix in ulnet_process_message's sam2_conn_header branch.
static int ulnet_test_solo_connect_is_active_player(void) {
    g_test_name = __func__;
    ulnet_session_t session;
    memset(&session, 0, sizeof(session));
    ulnet_session_init_defaulted(&session);
    session.sam2_send_callback = ulnet__test_discard_send_callback;

    const uint16_t peer_id = 10001;
    sam2_connect_message_t connect = { SAM2_CONN_HEADER, peer_id, {0} };
    if (ulnet_process_message(&session, (const char *) &connect) != 0) {
        SAM2_LOG_ERROR("Failed to process connect message");
        return 1;
    }

    int failed = 0;
    failed |= session.our_peer_id != peer_id;
    failed |= sam2_get_port_of_peer(&session.room_we_are_in, peer_id) != SAM2_AUTHORITY_INDEX;
    // The crux: the solo authority must be a player, not a coordinator-only authority.
    failed |= !ulnet_port_is_p2p(&session.room_we_are_in, SAM2_AUTHORITY_INDEX);
    failed |= !ulnet_port_is_active_player(&session.room_we_are_in, SAM2_AUTHORITY_INDEX);
    if (failed) {
        SAM2_LOG_ERROR("Solo connect must seat us as an active p2p player at the authority port");
        return 1;
    }

    return 0;
}

static int ulnet_test_room_change_scheduling_guards(void) {
    g_test_name = __func__;
    ulnet_session_t session;
    memset(&session, 0, sizeof(session));
    ulnet_session_init_defaulted(&session);
    session.our_peer_id = 10001;
    session.sam2_send_callback = ulnet__test_discard_send_callback;
    session.room_we_are_in.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session.our_peer_id;
    session.room_we_are_in.peer_ids[ULNET__TEST_SPECTATOR_PORT] = 30002;
    session.room_we_are_in.peer_topology = (1ULL << SAM2_AUTHORITY_INDEX);
    session.next_room = session.room_we_are_in;

    sam2_room_join_message_t promote = { SAM2_JOIN_HEADER };
    promote.room = session.room_we_are_in;
    promote.room.peer_topology |= (1ULL << ULNET__TEST_SPECTATOR_PORT);
    promote.peer_id = session.room_we_are_in.peer_ids[ULNET__TEST_SPECTATOR_PORT];

    if (ulnet_process_message(&session, (const char *) &promote) != 0) {
        SAM2_LOG_ERROR("Failed to schedule spectator promotion");
        return 1;
    }

    int64_t advertise_frame = ulnet__room_advertise_frame_from_effective_frame(session.next_room_effective_frame);
    if (advertise_frame != 1) {
        SAM2_LOG_ERROR("Room change advertised on frame %" PRId64 " instead of frame 1", advertise_frame);
        return 1;
    }
    if (session.next_room_effective_frame != 1 + ULNET_ROOM_CHANGE_LEAD_FRAMES) {
        SAM2_LOG_ERROR("Room change effective frame was %" PRId64 " instead of %" PRId64,
            session.next_room_effective_frame, (int64_t)(1 + ULNET_ROOM_CHANGE_LEAD_FRAMES));
        return 1;
    }

    sam2_room_t first_pending = session.next_room;
    int64_t first_effective_frame = session.next_room_effective_frame;

    sam2_room_join_message_t demote_authority = { SAM2_JOIN_HEADER };
    demote_authority.room = session.room_we_are_in;
    demote_authority.room.peer_topology &= ~(1ULL << SAM2_AUTHORITY_INDEX);
    demote_authority.peer_id = session.our_peer_id;

    if (ulnet_process_message(&session, (const char *) &demote_authority) != 0) {
        SAM2_LOG_ERROR("Failed to process overlapping active-set change");
        return 1;
    }
    if (session.next_room_effective_frame != first_effective_frame ||
        memcmp(&session.next_room, &first_pending, sizeof(session.next_room)) != 0) {
        SAM2_LOG_ERROR("Overlapping active-set change replaced an already pending room change");
        return 1;
    }

    return 0;
}

static void ulnet__test_inproc_pair_setup(ulnet_session_t *sessions[2],
    ulnet_transport_inproc_t *transport, int64_t retransmit_delay_microseconds) {
    g_ulnet_transport_use_inproc = true;
    for (int i = 0; i < 2; i++) {
        sessions[i] = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(sessions[i]);
        sessions[i]->reliable_retransmit_delay_microseconds = retransmit_delay_microseconds;
        sessions[i]->retro_run = ulnet__test_retro_run;
        sessions[i]->retro_serialize_size = ulnet__test_retro_serialize_size;
        sessions[i]->retro_serialize = ulnet__test_retro_serialize;
        sessions[i]->retro_unserialize = ulnet__test_retro_unserialize;
    }

    sessions[0]->our_peer_id = 10001;
    sessions[1]->our_peer_id = 30002;

    sam2_room_t room = {0};
    room.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    room.peer_ids[SAM2_AUTHORITY_INDEX] = sessions[0]->our_peer_id;
    room.peer_ids[ULNET__TEST_SPECTATOR_PORT] = sessions[1]->our_peer_id;
    room.peer_topology = (1ULL << SAM2_AUTHORITY_INDEX); // Authority is a p2p player; session[1] is a spectator

    sessions[0]->room_we_are_in = room;
    sessions[1]->room_we_are_in = room;
    sessions[0]->next_room = room;
    sessions[1]->next_room = room;

    // agent[p] is the link to room.peer_ids[p] (no separate agent_peer_ids table any more)
    ulnet__test_inproc_wire(sessions[0], ULNET__TEST_SPECTATOR_PORT, transport);
    ulnet__test_inproc_wire(sessions[1], SAM2_AUTHORITY_INDEX, transport);

    sessions[1]->frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    sessions[0]->peer_needs_sync_bitfield |= (1ULL << ULNET__TEST_SPECTATOR_PORT);
}

static void ulnet__test_poll_inproc_sessions(ulnet_session_t **sessions, int count, uint8_t *save_state, size_t save_state_size) {
    for (int i = 0; i < count; i++) {
        sessions[i]->core_wants_tick_at_unix_usec = 0;
        ulnet_poll_session(sessions[i], 0, save_state, save_state_size, 60.0, 0.0);
    }
}

static int ulnet__test_sync_inproc_pair(ulnet_session_t *sessions[2], uint8_t *save_state, size_t save_state_size) {
    for (int i = 0; i < 600; i++) {
        ulnet__test_poll_inproc_sessions(sessions, 2, save_state, save_state_size);
        if (sessions[1]->frame_counter != ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) {
            return 0;
        }
    }

    SAM2_LOG_ERROR("inproc pair did not sync");
    return 1;
}

static int ulnet__test_request_local_role_toggle(ulnet_session_t *authority, int port) {
    sam2_room_join_message_t req = { SAM2_JOIN_HEADER };
    req.room = authority->room_we_are_in;
    req.room.peer_topology ^= (1ULL << port);
    req.peer_id = authority->our_peer_id;
    return ulnet_process_message(authority, (const char *) &req);
}

static int ulnet__test_request_remote_role_toggle(ulnet_session_t *peer, int port) {
    sam2_room_join_message_t req = { SAM2_JOIN_HEADER };
    req.room = peer->room_we_are_in;
    req.room.peer_topology ^= (1ULL << port);
    req.peer_id = peer->our_peer_id;
    return ulnet_message_send(peer, SAM2_AUTHORITY_INDEX, (const uint8_t *) &req);
}

static int ulnet__test_expect_room_change_lead(ulnet_session_t *authority, const char *label) {
    int64_t advertise_frame = ulnet__room_advertise_frame_from_effective_frame(authority->next_room_effective_frame);
    if (advertise_frame <= authority->authority_room_snapshot_last_sent_frame) {
        SAM2_LOG_ERROR("%s reused an authority state frame that may already have been sent", label);
        return 1;
    }
    int64_t expected_advertise_frame = SAM2_MAX(
        authority->peer[SAM2_AUTHORITY_INDEX]->state.frame,
        authority->authority_room_snapshot_last_sent_frame + 1
    );
    expected_advertise_frame = SAM2_MAX(expected_advertise_frame, 1);
    int64_t expected_effective_frame = expected_advertise_frame + ULNET_ROOM_CHANGE_LEAD_FRAMES;
    if (advertise_frame != expected_advertise_frame ||
        authority->next_room_effective_frame != expected_effective_frame) {
        SAM2_LOG_ERROR("%s scheduled advertise/effective frames %" PRId64 "/%" PRId64
            " instead of %" PRId64 "/%" PRId64, label,
            advertise_frame, authority->next_room_effective_frame,
            expected_advertise_frame, expected_effective_frame);
        return 1;
    }

    return 0;
}

int ulnet_test_ice(ulnet_session_t **session_1_out, ulnet_session_t **session_2_out) {
    g_test_name = __func__;
    g_ulnet_transport_use_inproc = false;
    sam2_server_t *server = 0;
    ulnet_session_t *sessions[2] = {0};
    sam2_socket_t sockets[2] = {0};
    int test_passed = 1;

    server = (sam2_server_t *) malloc(sizeof(sam2_server_t));
    int status = sam2_server_init(server, ULNET__TEST_SAM2_PORT);
    if (status) {
        SAM2_LOG_ERROR("Error while initializing sam2 server");
        goto _10;
    }

    for (int i = 0; i < sizeof(sessions)/sizeof(sessions[0]); i++) {
        sessions[i] = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(sessions[i]);
        ulnet_set_stun_server(sessions[i], "127.0.0.1", ULNET__TEST_SAM2_PORT);
        sessions[i]->reliable_retransmit_delay_microseconds = 0;
        sessions[i]->sam2_send_callback = ulnet__test_sam2_send_callback;
        sessions[i]->user_ptr = &sockets[i];
        sessions[i]->retro_run = ulnet__test_retro_run;
        sessions[i]->retro_serialize_size = ulnet__test_retro_serialize_size;
        sessions[i]->retro_serialize = ulnet__test_retro_serialize;
        sessions[i]->retro_unserialize = ulnet__test_retro_unserialize;

        status = sam2_client_connect(&sockets[i], "127.0.0.1", ULNET__TEST_SAM2_PORT);
        if (status) {
            SAM2_LOG_ERROR("Error while starting connection to sam2 server");
            goto _10;
        }

        int connection_established = 0;
        for (int64_t start_time = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - start_time < 2000000;) {
            connection_established = sam2_client_poll_connection(sockets[i], 0);
            status = ulnet__test_forward_messages(server, sessions[i], sockets[i]);
            if (status < 0) {
                SAM2_LOG_ERROR("Error forwarding messages: %d", status);
                goto _10;
            }

            if (sessions[i]->our_peer_id) {
                connection_established = 1;
                break;
            }
        }

        if (!connection_established) {
            SAM2_LOG_ERROR("Failed to connect to sam2 server");
            status = 1;
            goto _10;
        }
    }

    sam2_room_t room = {0};
    room.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    room.peer_ids[SAM2_AUTHORITY_INDEX] = 10001;
    room.peer_ids[ULNET__TEST_SPECTATOR_PORT] = 30002;
    room.peer_topology = (1ULL << SAM2_AUTHORITY_INDEX);

    // @todo The behavior right now sucks if you don't first make the room before having the person try to join it. It should just reply with a reasonable error
    // Have session 0 make the room
    sam2_room_make_message_t request = { SAM2_MAKE_HEADER };
    request.room = room;
    request.room.flags |= SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    request.room.peer_topology |= (1ULL << SAM2_AUTHORITY_INDEX);
    sam2_client_send(sockets[0], (char *)&request);

    // Have session 1 join the room
    sessions[1]->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = sessions[0]->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX];
    sessions[1]->frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    ulnet_startup_nat_for_peer(sessions[1], sessions[0]->our_peer_id, SAM2_AUTHORITY_INDEX, NULL);

    // Give at least 2 seconds for ICE connection establishment
    int connection_established = 0;

    for (int64_t start_time = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - start_time < 2000000;) {
        for (int i = 0; i < sizeof(sessions)/sizeof(sessions[0]); i++) {
            status = ulnet_poll_session(sessions[i], 0, 0, 0, 60.0, 50e-3);
            if (status < 0) {
                SAM2_LOG_ERROR("Error polling ulnet session: %d", status);
                goto _10;
            }

            status = ulnet__test_forward_messages(server, sessions[i], sockets[i]);
            if (status < 0) {
                SAM2_LOG_ERROR("Error forwarding messages: %d", status);
                goto _10;
            }
        }

        int spectator_port = -1;
        if (sessions[0]) {
            spectator_port = sam2_get_port_of_peer(&sessions[0]->room_we_are_in, sessions[1]->our_peer_id);
        }
        if (   spectator_port != -1
            && sessions[0]->peer[spectator_port]->transport
            && ulnet_transport_state(sessions[0]->peer[spectator_port]->transport) == ULNET_TRANSPORT_READY) {
            connection_established = 1;
            break;
        }
    }

    if (!connection_established) {
        SAM2_LOG_ERROR("Failed to establish connection within 2 seconds");
        status = 1;
        goto _10;
    }

    int spectator_port = -1;
    spectator_port = sam2_get_port_of_peer(&sessions[0]->room_we_are_in, sessions[1]->our_peer_id);
    if (spectator_port == -1) {
        SAM2_LOG_ERROR("Failed to locate spectator port for peer %05" PRIu16, sessions[1]->our_peer_id);
        status = 1;
        goto _10;
    }

    int spectator_synced = sessions[1]->frame_counter != ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    for (int64_t start_time = ulnet__get_unix_time_microseconds();
         !spectator_synced && ulnet__get_unix_time_microseconds() - start_time < 2000000;) {
        for (int i = 0; i < sizeof(sessions)/sizeof(sessions[0]); i++) {
            status = ulnet_poll_session(sessions[i], 0, 0, 0, 60.0, 50e-3);
            if (status < 0) {
                SAM2_LOG_ERROR("Error polling ulnet session while waiting for sync: %d", status);
                goto _10;
            }

            status = ulnet__test_forward_messages(server, sessions[i], sockets[i]);
            if (status < 0) {
                SAM2_LOG_ERROR("Error forwarding messages while waiting for sync: %d", status);
                goto _10;
            }
        }

        spectator_synced = sessions[1]->frame_counter != ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    }

    if (!spectator_synced) {
        SAM2_LOG_ERROR("Spectator connection established but savestate sync never started");
        status = 1;
        goto _10;
    }

    sessions[0]->debug_udp_recv_drop_rate = 1.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "HELLO", sizeof("HELLO") - 1); // DROP
    sessions[0]->debug_udp_recv_drop_rate = 0.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "WORLD", sizeof("WORLD") - 1); // (NOT SENT) ADDED TO OUTGOING BUFFER

    ulnet_reliable_packet_t *msg1 = NULL;
    ulnet_reliable_packet_t *msg2 = NULL;
    int ack_carrier_sent = 0;
    for (int64_t start_time = ulnet__get_unix_time_microseconds();
         ulnet__get_unix_time_microseconds() - start_time < 2000000;) {
        for (int i = 0; i < sizeof(sessions)/sizeof(sessions[0]); i++) {
            status = ulnet_poll_session(sessions[i], 0, 0, 0, 60.0, 16e-3);
            if (status < 0) {
                SAM2_LOG_ERROR("Error polling reliable delivery test: %d", status);
                goto _10;
            }

            status = ulnet__test_forward_messages(server, sessions[i], sockets[i]);
            if (status < 0) {
                SAM2_LOG_ERROR("Error forwarding reliable delivery test messages: %d", status);
                goto _10;
            }
        }

        ulnet_peer_diagnostics_t *diagnostics =
            sessions[0]->peer[spectator_port] ? sessions[0]->peer[spectator_port]->diagnostics : NULL;
        msg1 = diagnostics ? (ulnet_reliable_packet_t *)diagnostics->reliable_rx_packet_history[0].data : NULL;
        msg2 = diagnostics ? (ulnet_reliable_packet_t *)diagnostics->reliable_rx_packet_history[1].data : NULL;
        if (msg1 && !ack_carrier_sent) {
            status = ulnet_reliable_send_with_acks_only(sessions[0], spectator_port, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1);
            if (status < 0) {
                SAM2_LOG_ERROR("Failed to send ACK carrier");
                goto _10;
            }
            ack_carrier_sent = 1;
        }
        if (msg1 && msg2) break;
    }

    if (!(   msg1 && memcmp(msg1->payload, "HELLO", sizeof("HELLO") - 1) == 0
          && msg2 && memcmp(msg2->payload, "WORLD", sizeof("WORLD") - 1) == 0)) {
        SAM2_LOG_ERROR("Failed to send reliable messages");
        SAM2_LOG_ERROR("reliable diag: ack_sent=%d tx_head=%u tx_next=%u rx_head=%u msg1=%p msg2=%p",
            ack_carrier_sent,
            sessions[1]->peer[SAM2_AUTHORITY_INDEX]->reliable_tx_head,
            sessions[1]->peer[SAM2_AUTHORITY_INDEX]->reliable_tx_next_seq,
            sessions[0]->peer[spectator_port]->reliable_rx_head,
            (void *)msg1,
            (void *)msg2);
        status = 1;
    }

_10:sam2_server_destroy(server);
    free(server);

    if (sessions[0]) {
        ulnet_session_tear_down(sessions[0]);
    }
    if (sessions[1]) {
        ulnet_session_tear_down(sessions[1]);
    }
    if (!session_1_out) free(sessions[0]);
    else *session_1_out = sessions[0];

    if (!session_2_out) free(sessions[1]);
    else *session_2_out = sessions[1];

    return status;
}

// Returns nonzero (and logs the offending field) if any per-port connection state for `slot` is not
// fully zeroed -- i.e. the slot is not in the clean state a fresh peer should occupy. This is the
// class of bug ("forgot to zero something out") that the manual GUI join/leave churn used to surface.
static int ulnet__test_slot_not_clear(ulnet_session_t *s, int slot) {
    ulnet_peer_t *peer = s->peer[slot];
    struct { const char *name; long long value; } fields[] = {
        { "room peer_id",        s->room_we_are_in.peer_ids[slot] != SAM2_PORT_AVAILABLE },
        { "next_room peer_id",   s->next_room.peer_ids[slot]       != SAM2_PORT_AVAILABLE },
        { "topology bit",        (s->room_we_are_in.peer_topology >> slot) & 1ULL },
        { "peer record",         peer != NULL },
        { "needs_sync bit",      (s->peer_needs_sync_bitfield >> slot) & 1ULL },
        { "pending_disc bit",    (s->peer_pending_disconnect_bitfield >> slot) & 1ULL },
    };

    int dirty = 0;
    for (int i = 0; i < (int)(sizeof(fields)/sizeof(fields[0])); i++) {
        if (fields[i].value) {
            SAM2_LOG_ERROR("Slot %d not cleared: %s left as %lld", slot, fields[i].name, fields[i].value);
            dirty = 1;
        }
    }
    return dirty;
}

static int ulnet__test_ice_pump(sam2_server_t *server, ulnet_session_t **sessions, sam2_socket_t *sockets, int count) {
    g_test_name = __func__;
    for (int i = 0; i < count; i++) {
        if (!sessions[i]) continue;
        int status = ulnet_poll_session(sessions[i], 0, 0, 0, 60.0, 5e-3);
        if (status < 0) return status;
        status = ulnet__test_forward_messages(server, sessions[i], sockets[i]);
        if (status < 0) return status;
    }
    return 0;
}

// Have `joiner` signal `authority` and wait until the authority has it on a ready port and the joiner
// has finished its savestate sync. Returns the slot the authority placed it on via *out_slot.
static int ulnet__test_ice_join(sam2_server_t *server, ulnet_session_t **sessions, sam2_socket_t *sockets, int count,
    int authority_idx, int joiner_idx, int *out_slot) {
    g_test_name = __func__;
    ulnet_session_t *authority = sessions[authority_idx];
    ulnet_session_t *joiner = sessions[joiner_idx];

    ulnet_session_tear_down(joiner);
    ulnet_session_init_defaulted(joiner);
    joiner->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = authority->our_peer_id;
    joiner->next_room = joiner->room_we_are_in;
    joiner->frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    ulnet_startup_nat_for_peer(joiner, authority->our_peer_id, SAM2_AUTHORITY_INDEX, NULL);

    for (int64_t t0 = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - t0 < 5000000;) {
        int status = ulnet__test_ice_pump(server, sessions, sockets, count);
        if (status < 0) return status;

        int slot = sam2_get_port_of_peer(&authority->room_we_are_in, joiner->our_peer_id);
        if (   slot != -1
            && authority->peer[slot]->transport
            && ulnet_transport_state(authority->peer[slot]->transport) == ULNET_TRANSPORT_READY
            && joiner->frame_counter != ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) {
            if (out_slot) *out_slot = slot;
            return 0;
        }
    }

    SAM2_LOG_ERROR("Peer %05" PRIu16 " failed to join within timeout", joiner->our_peer_id);
    return 1;
}

// Real-ICE version of the GUI "Become Player" flow. This specifically verifies the authority's
// desired room (`next_room`) was initialized from the MAKE response before it handles the JOIN toggle.
int ulnet_test_ice_promote_spectator(void) {
    g_test_name = __func__;
    g_ulnet_transport_use_inproc = false;
    enum { A = 0, B = 1, SESSION_COUNT = 2 };
    sam2_server_t *server = (sam2_server_t *) malloc(sizeof(sam2_server_t));
    ulnet_session_t *sessions[SESSION_COUNT] = {0};
    sam2_socket_t sockets[SESSION_COUNT] = {0};
    int status = sam2_server_init(server, ULNET__TEST_SAM2_PORT);
    if (status) {
        SAM2_LOG_ERROR("Error while initializing SAM2 server");
        goto done;
    }

    for (int i = 0; i < SESSION_COUNT; i++) {
        sessions[i] = (ulnet_session_t *) calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(sessions[i]);
        ulnet_set_stun_server(sessions[i], "127.0.0.1", ULNET__TEST_SAM2_PORT);
        sessions[i]->reliable_retransmit_delay_microseconds = 0;
        sessions[i]->sam2_send_callback = ulnet__test_sam2_send_callback;
        sessions[i]->user_ptr = &sockets[i];
        sessions[i]->retro_run = ulnet__test_retro_run;
        sessions[i]->retro_serialize_size = ulnet__test_retro_serialize_size;
        sessions[i]->retro_serialize = ulnet__test_retro_serialize;
        sessions[i]->retro_unserialize = ulnet__test_retro_unserialize;

        status = sam2_client_connect(&sockets[i], "127.0.0.1", ULNET__TEST_SAM2_PORT);
        if (status) goto done;

        int connected = 0;
        for (int64_t t0 = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - t0 < 2000000;) {
            sam2_client_poll_connection(sockets[i], 0);
            status = ulnet__test_forward_messages(server, sessions[i], sockets[i]);
            if (status < 0) goto done;
            if (sessions[i]->our_peer_id) { connected = 1; break; }
        }
        if (!connected) {
            SAM2_LOG_ERROR("Session %d failed to connect to sam2 server", i);
            status = 1;
            goto done;
        }
    }

    {
        sam2_room_make_message_t make = { SAM2_MAKE_HEADER };
        make.room.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
        make.room.peer_ids[SAM2_AUTHORITY_INDEX] = sessions[A]->our_peer_id;
        make.room.peer_topology = (1ULL << SAM2_AUTHORITY_INDEX);
        sam2_client_send(sockets[A], (char *) &make);
    }

    int made = 0;
    for (int64_t t0 = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - t0 < 2000000;) {
        status = ulnet__test_ice_pump(server, sessions, sockets, SESSION_COUNT);
        if (status < 0) goto done;
        made =    (sessions[A]->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)
               && (sessions[A]->next_room.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED);
        if (made) break;
    }
    if (!made) {
        SAM2_LOG_ERROR("Authority did not initialize hosted room and desired room from MAKE");
        status = 1;
        goto done;
    }

    int b_slot = -1;
    status = ulnet__test_ice_join(server, sessions, sockets, SESSION_COUNT, A, B, &b_slot);
    if (status) goto done;

    int b_our_port = sam2_get_port_of_peer(&sessions[B]->room_we_are_in, sessions[B]->our_peer_id);
    if (b_our_port != b_slot) {
        SAM2_LOG_ERROR("Joiner sees itself at port %d but authority assigned port %d", b_our_port, b_slot);
        status = 1;
        goto done;
    }

    {
        sam2_room_join_message_t req = { SAM2_JOIN_HEADER };
        req.room = sessions[B]->room_we_are_in;
        req.room.peer_topology ^= (1ULL << b_our_port);
        req.peer_id = sessions[B]->our_peer_id;
        status = ulnet_message_send(sessions[B], SAM2_AUTHORITY_INDEX, (const uint8_t *) &req);
        if (status) goto done;
    }

    int promoted = 0;
    for (int64_t t0 = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - t0 < 5000000;) {
        status = ulnet__test_ice_pump(server, sessions, sockets, SESSION_COUNT);
        if (status < 0) goto done;
        promoted =    ulnet_port_is_p2p(&sessions[A]->room_we_are_in, b_slot)
                   && ulnet_port_is_p2p(&sessions[B]->room_we_are_in, b_slot);
        if (promoted) break;
    }

    if (!promoted) {
        SAM2_LOG_ERROR("Real-ICE spectator promotion did not propagate");
        status = 1;
        goto done;
    }
    if (!(sessions[A]->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)) {
        SAM2_LOG_ERROR("Promotion adopted a stale desired room and abandoned the hosted room");
        status = 1;
        goto done;
    }

    status = ulnet__test_request_local_role_toggle(sessions[A], SAM2_AUTHORITY_INDEX);
    if (status) goto done;
    status = ulnet__test_expect_room_change_lead(sessions[A], "Real-ICE coordinator toggle");
    if (status) goto done;

    int coordinator_mode = 0;
    for (int64_t t0 = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - t0 < 5000000;) {
        status = ulnet__test_ice_pump(server, sessions, sockets, SESSION_COUNT);
        if (status < 0) goto done;
        coordinator_mode =    !ulnet_port_is_p2p(&sessions[A]->room_we_are_in, SAM2_AUTHORITY_INDEX)
                           && !ulnet_port_is_p2p(&sessions[B]->room_we_are_in, SAM2_AUTHORITY_INDEX)
                           &&  ulnet_port_is_p2p(&sessions[A]->room_we_are_in, b_slot)
                           &&  ulnet_port_is_p2p(&sessions[B]->room_we_are_in, b_slot);
        if (coordinator_mode) break;
    }

    if (!coordinator_mode) {
        SAM2_LOG_ERROR("Real-ICE coordinator-only authority mode did not propagate");
        status = 1;
        goto done;
    }
    if (ulnet_port_is_active_player(&sessions[A]->room_we_are_in, SAM2_AUTHORITY_INDEX)) {
        SAM2_LOG_ERROR("Real-ICE coordinator authority is still in active input set");
        status = 1;
        goto done;
    }

    sessions[A]->delay_frames = 0;
    sessions[B]->delay_frames = 0;
    int64_t b_start_frame = sessions[B]->frame_counter;
    int coordinator_ticks = 0;
    for (int64_t t0 = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - t0 < 5000000;) {
        status = ulnet__test_ice_pump(server, sessions, sockets, SESSION_COUNT);
        if (status < 0) goto done;
        coordinator_ticks =    sessions[B]->frame_counter > b_start_frame + ULNET_DELAY_FRAMES_MAX + 1
                            && sessions[A]->peer[b_slot]->state.frame + 1 >= sessions[A]->frame_counter
                            && sessions[B]->peer[SAM2_AUTHORITY_INDEX]->state.frame >= 7;
        if (coordinator_ticks) break;
    }
    if (!coordinator_ticks) {
        SAM2_LOG_ERROR("Real-ICE coordinator-only authority did not heartbeat a zero-delay player");
        status = 1;
        goto done;
    }

    status = 0;

done:
    if (server) {
        sam2_server_destroy(server);
        free(server);
    }
    for (int i = 0; i < SESSION_COUNT; i++) {
        if (sessions[i]) {
            ulnet_session_tear_down(sessions[i]);
            free(sessions[i]);
        }
    }
    return status;
}

// Tear the leaver down (which signals an exit to the authority) and wait until the authority has both
// removed it from the room and torn its agent down.
static int ulnet__test_ice_leave(sam2_server_t *server, ulnet_session_t **sessions, sam2_socket_t *sockets, int count,
    int authority_idx, int leaver_idx) {
    ulnet_session_t *authority = sessions[authority_idx];
    uint16_t leaver_id = sessions[leaver_idx]->our_peer_id;
    int slot = sam2_get_port_of_peer(&authority->room_we_are_in, leaver_id);

    ulnet_session_tear_down(sessions[leaver_idx]);
    ulnet_session_init_defaulted(sessions[leaver_idx]);

    for (int64_t t0 = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - t0 < 5000000;) {
        int status = ulnet__test_ice_pump(server, sessions, sockets, count);
        if (status < 0) return status;

        if (   sam2_get_port_of_peer(&authority->room_we_are_in, leaver_id) == -1
            && (slot == -1 || authority->peer[slot] == NULL || authority->peer[slot]->transport == NULL)) {
            return 0;
        }
    }

    SAM2_LOG_ERROR("Authority failed to release peer %05" PRIu16 " within timeout", leaver_id);
    return 1;
}

// Real-signaling-server churn test: two peers are joined, then a third repeatedly joins and leaves --
// landing on a different port the second time (a fourth peer takes the freed slot in between). Verifies
// the authority fully zeroes a departed peer's per-port state, never swaps slots, and never disturbs
// the peers that stay. This used to be done by hand in the GUI to flush out connection-management bugs.
int ulnet_test_ice_churn(void) {
    g_test_name = __func__;
    g_ulnet_transport_use_inproc = false;
    enum { A = 0, B = 1, C = 2, D = 3, SESSION_COUNT = 4 };
    sam2_server_t *server = (sam2_server_t *) malloc(sizeof(sam2_server_t));
    ulnet_session_t *sessions[SESSION_COUNT] = {0};
    sam2_socket_t sockets[SESSION_COUNT] = {0};

    int status = sam2_server_init(server, ULNET__TEST_SAM2_PORT);
    if (status) {
        SAM2_LOG_ERROR("Error while initializing sam2 server");
        goto done;
    }

    for (int i = 0; i < SESSION_COUNT; i++) {
        sessions[i] = (ulnet_session_t *) calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(sessions[i]);
        ulnet_set_stun_server(sessions[i], "127.0.0.1", ULNET__TEST_SAM2_PORT);
        sessions[i]->reliable_retransmit_delay_microseconds = 0;
        sessions[i]->sam2_send_callback = ulnet__test_sam2_send_callback;
        sessions[i]->user_ptr = &sockets[i];
        sessions[i]->retro_run = ulnet__test_retro_run;
        sessions[i]->retro_serialize_size = ulnet__test_retro_serialize_size;
        sessions[i]->retro_serialize = ulnet__test_retro_serialize;
        sessions[i]->retro_unserialize = ulnet__test_retro_unserialize;

        status = sam2_client_connect(&sockets[i], "127.0.0.1", ULNET__TEST_SAM2_PORT);
        if (status) {
            SAM2_LOG_ERROR("Error while starting connection to sam2 server");
            goto done;
        }

        int connected = 0;
        for (int64_t t0 = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - t0 < 2000000;) {
            sam2_client_poll_connection(sockets[i], 0);
            status = ulnet__test_forward_messages(server, sessions[i], sockets[i]);
            if (status < 0) goto done;
            if (sessions[i]->our_peer_id) { connected = 1; break; }
        }
        if (!connected) {
            SAM2_LOG_ERROR("Session %d failed to connect to sam2 server", i);
            status = 1;
            goto done;
        }
    }

    // The authority hosts the room (it is the p2p player at port 0)
    {
        sam2_room_make_message_t make = { SAM2_MAKE_HEADER };
        make.room.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
        make.room.peer_ids[SAM2_AUTHORITY_INDEX] = sessions[A]->our_peer_id;
        make.room.peer_topology = (1ULL << SAM2_AUTHORITY_INDEX);
        sam2_client_send(sockets[A], (char *) &make);

        sessions[A]->room_we_are_in.flags |= SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
        sessions[A]->next_room = sessions[A]->room_we_are_in;
    }

    int b_slot = -1;
    status = ulnet__test_ice_join(server, sessions, sockets, SESSION_COUNT, A, B, &b_slot);
    if (status) goto done;
    void *b_agent = (void *) sessions[A]->peer[b_slot]->transport;

    int c_slot_first = -1;
    status = ulnet__test_ice_join(server, sessions, sockets, SESSION_COUNT, A, C, &c_slot_first);
    if (status) goto done;
    if (c_slot_first == b_slot) {
        SAM2_LOG_ERROR("Two peers ended up on the same slot %d", c_slot_first);
        status = 1;
        goto done;
    }
    if (sam2_get_port_of_peer(&sessions[A]->room_we_are_in, sessions[B]->our_peer_id) != b_slot
        || (void *) sessions[A]->peer[b_slot]->transport != b_agent) {
        SAM2_LOG_ERROR("Persistent peer B was disturbed when C joined (slot swap?)");
        status = 1;
        goto done;
    }

    // C leaves -- its slot must be fully reclaimed and zeroed
    status = ulnet__test_ice_leave(server, sessions, sockets, SESSION_COUNT, A, C);
    if (status) goto done;
    if (ulnet__test_slot_not_clear(sessions[A], c_slot_first)) {
        SAM2_LOG_ERROR("Authority did not fully clear slot %d after C left", c_slot_first);
        status = 1;
        goto done;
    }
    if (sam2_get_port_of_peer(&sessions[A]->room_we_are_in, sessions[B]->our_peer_id) != b_slot
        || (void *) sessions[A]->peer[b_slot]->transport != b_agent) {
        SAM2_LOG_ERROR("Persistent peer B was disturbed when C left");
        status = 1;
        goto done;
    }

    // D takes the freed slot so that C's rejoin is forced onto a different port
    int d_slot = -1;
    status = ulnet__test_ice_join(server, sessions, sockets, SESSION_COUNT, A, D, &d_slot);
    if (status) goto done;

    int c_slot_second = -1;
    status = ulnet__test_ice_join(server, sessions, sockets, SESSION_COUNT, A, C, &c_slot_second);
    if (status) goto done;
    if (c_slot_second == c_slot_first) {
        SAM2_LOG_ERROR("Expected C to rejoin on a different port than %d", c_slot_first);
        status = 1;
        goto done;
    }
    if (sam2_get_port_of_peer(&sessions[A]->room_we_are_in, sessions[B]->our_peer_id) != b_slot
        || (void *) sessions[A]->peer[b_slot]->transport != b_agent) {
        SAM2_LOG_ERROR("Persistent peer B was disturbed across C's churn");
        status = 1;
        goto done;
    }

    // C leaves once more from its new port -- that slot must also come back fully clean
    status = ulnet__test_ice_leave(server, sessions, sockets, SESSION_COUNT, A, C);
    if (status) goto done;
    if (ulnet__test_slot_not_clear(sessions[A], c_slot_second)) {
        SAM2_LOG_ERROR("Authority did not fully clear slot %d after C left again", c_slot_second);
        status = 1;
        goto done;
    }

    status = 0;

done:
    if (server) {
        sam2_server_destroy(server);
        free(server);
    }
    for (int i = 0; i < SESSION_COUNT; i++) {
        if (sessions[i]) {
            ulnet_session_tear_down(sessions[i]);
            free(sessions[i]);
        }
    }
    return status;
}

// Reproduces the GUI "Become Player" flow over inproc: a connected spectator promotes itself by
// toggling its own topology bit; the change must take effect for both the authority and the spectator.
int ulnet_test_inproc_promote_spectator(void) {
    g_test_name = __func__;
    ulnet_session_t *sessions[2] = {0};
    ulnet_transport_inproc_t transport = {0};
    uint8_t save_state[256];
    int status = 0;
    int spec_slot = ULNET__TEST_SPECTATOR_PORT;

    ulnet__test_inproc_pair_setup(sessions, &transport, 0);
    sessions[0]->delay_frames = 1;
    sessions[1]->delay_frames = 1;

    // Drive frames until the spectator finishes its savestate sync and both are ticking
    int synced = 0;
    for (int i = 0; i < 600 && !synced; i++) {
        for (int j = 0; j < 2; j++) {
            sessions[j]->core_wants_tick_at_unix_usec = 0;
            ulnet_poll_session(sessions[j], 0, save_state, sizeof(save_state), 60.0, 0.0);
        }
        synced = sessions[1]->frame_counter != ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    }
    if (!synced) {
        SAM2_LOG_ERROR("promote test: spectator never synced");
        status = 1;
        goto done;
    }
    SAM2_LOG_INFO("promote test: spectator synced at frame %" PRId64 " (slot %d)", sessions[1]->frame_counter, spec_slot);

    // While still spectating, drive port 0 and verify the authority folds the suggestion into its
    // deterministic player state.
    sessions[1]->next_input_state[0][0] = 1;
    sessions[1]->next_input_state[0][ULNET_INPUT_ANALOG_FIRST_INDEX] = 4321;
    sessions[1]->next_input_state[3][3] = 1;
    int spectator_input_forwarded = 0;
    for (int i = 0; i < 120 && !spectator_input_forwarded; i++) {
        sessions[1]->next_input_state[0][0] = 1;
        sessions[1]->next_input_state[0][ULNET_INPUT_ANALOG_FIRST_INDEX] = 4321;
        sessions[1]->next_input_state[3][3] = 1;
        ulnet__test_poll_inproc_sessions(sessions, 2, save_state, sizeof(save_state));
        ulnet_input_state_t received;
        int64_t authority_state_frame =
            sessions[0]->peer[SAM2_AUTHORITY_INDEX]->state.frame;
        ulnet__state_history_unpack_input(sessions[0], SAM2_AUTHORITY_INDEX,
            authority_state_frame, 0, received);
        spectator_input_forwarded =
               received[0] == 1
            && received[ULNET_INPUT_ANALOG_FIRST_INDEX] == 4321;
        ulnet__state_history_unpack_input(sessions[0], SAM2_AUTHORITY_INDEX,
            authority_state_frame, 3, received);
        spectator_input_forwarded &= received[3] == 1;
    }
    if (!spectator_input_forwarded) {
        SAM2_LOG_ERROR("promote test: spectator input was not folded into authority state");
        status = 1;
        goto done;
    }

    // The spectator promotes itself -- exactly what the GUI "Become Player" button does
    {
        sam2_room_join_message_t req = { SAM2_JOIN_HEADER };
        req.room = sessions[1]->room_we_are_in;
        req.room.peer_topology ^= (1ULL << spec_slot);
        req.peer_id = sessions[1]->our_peer_id;
        ulnet_message_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t *) &req);
    }

    int promoted_auth = 0, promoted_spec = 0;
    for (int i = 0; i < 600; i++) {
        for (int j = 0; j < 2; j++) {
            sessions[j]->core_wants_tick_at_unix_usec = 0;
            ulnet_poll_session(sessions[j], 0, save_state, sizeof(save_state), 60.0, 0.0);
        }
        promoted_auth = ulnet_port_is_p2p(&sessions[0]->room_we_are_in, spec_slot);
        promoted_spec = ulnet_port_is_p2p(&sessions[1]->room_we_are_in, spec_slot);
        if (promoted_auth && promoted_spec) break;
    }

    SAM2_LOG_INFO("promote diag: authority frame=%" PRId64 " spectator frame=%" PRId64,
        sessions[0]->frame_counter, sessions[1]->frame_counter);
    SAM2_LOG_INFO("promote diag: spectator state[auth].room bit=%d eff_frame=%" PRId64 " | spectator room bit=%d | authority room bit=%d",
        (int)((sessions[1]->peer[SAM2_AUTHORITY_INDEX]->state.room.peer_topology >> spec_slot) & 1),
        sessions[1]->peer[SAM2_AUTHORITY_INDEX]->state.room_effective_frame,
        (int)((sessions[1]->room_we_are_in.peer_topology >> spec_slot) & 1),
        (int)((sessions[0]->room_we_are_in.peer_topology >> spec_slot) & 1));

    if (!promoted_auth) { SAM2_LOG_ERROR("promote test: authority never applied the promotion"); status = 1; }
    if (!promoted_spec) { SAM2_LOG_ERROR("promote test: spectator never observed its own promotion"); status = 1; }
    if (status == 0) {
        int64_t authority_frame = sessions[0]->frame_counter;
        int64_t player_frame = sessions[1]->frame_counter;
        int input_port = 0; // netarch/debug default: every peer modifies emulated port 0
        sessions[1]->next_input_state[0][0] = 1;
        sessions[1]->next_input_state[0][ULNET_INPUT_ANALOG_FIRST_INDEX] = 1234;
        for (int i = 0; i < 120; i++) {
            sessions[1]->next_input_state[0][0] = 1;
            sessions[1]->next_input_state[0][ULNET_INPUT_ANALOG_FIRST_INDEX] = 1234;
            ulnet__test_poll_inproc_sessions(sessions, 2, save_state, sizeof(save_state));
        }
        if (   sessions[0]->frame_counter <= authority_frame + ULNET_DELAY_FRAMES_MAX
            || sessions[1]->frame_counter <= player_frame + ULNET_DELAY_FRAMES_MAX) {
            SAM2_LOG_ERROR("promote test: sessions stalled after promotion "
                "(authority=%" PRId64 "->%" PRId64 ", player=%" PRId64 "->%" PRId64 ")",
                authority_frame, sessions[0]->frame_counter,
                player_frame, sessions[1]->frame_counter);
            status = 1;
        } else {
            // The loop polls authority then player, so deliver the player's final packet before
            // inspecting the exact frame the authority is waiting to run.
            ulnet_service_network(sessions[0], 0);
            ulnet_input_state_t received;
            ulnet__state_history_unpack_input(sessions[0], spec_slot,
                sessions[0]->frame_counter, input_port, received);
            if (received[0] != 1
                || received[ULNET_INPUT_ANALOG_FIRST_INDEX] != 1234) {
                SAM2_LOG_ERROR("promote test: player input was not forwarded "
                    "(controller=%d button=%d analog=%d frame=%" PRId64 ")",
                    input_port, received[0],
                    received[ULNET_INPUT_ANALOG_FIRST_INDEX],
                    sessions[0]->frame_counter);
                status = 1;
            }
        }
    }
    if (status == 0) SAM2_LOG_INFO("promote test: spectator successfully became a ticking player");

done:
    ulnet_session_tear_down(sessions[0]);
    ulnet_session_tear_down(sessions[1]);
    free(sessions[0]);
    free(sessions[1]);
    return status;
}

int ulnet_test_inproc_coordinator_only_authority(void) {
    g_test_name = __func__;
    ulnet_session_t *sessions[2] = {0};
    ulnet_transport_inproc_t transport = {0};
    uint8_t save_state[256];
    int status = 0;
    int player_slot = ULNET__TEST_SPECTATOR_PORT;

    ulnet__test_inproc_pair_setup(sessions, &transport, 0);
    sessions[0]->delay_frames = 1;
    sessions[1]->delay_frames = 1;

    if (ulnet__test_sync_inproc_pair(sessions, save_state, sizeof(save_state)) != 0) {
        status = 1;
        goto done;
    }

    if (ulnet__test_request_remote_role_toggle(sessions[1], player_slot) != 0) {
        status = 1;
        goto done;
    }

    int promoted = 0;
    for (int i = 0; i < 600 && !promoted; i++) {
        ulnet__test_poll_inproc_sessions(sessions, 2, save_state, sizeof(save_state));
        promoted =    ulnet_port_is_p2p(&sessions[0]->room_we_are_in, player_slot)
                   && ulnet_port_is_p2p(&sessions[1]->room_we_are_in, player_slot);
    }
    if (!promoted) {
        SAM2_LOG_ERROR("coordinator test: remote player promotion did not apply");
        status = 1;
        goto done;
    }

    if (ulnet__test_request_local_role_toggle(sessions[0], SAM2_AUTHORITY_INDEX) != 0) {
        status = 1;
        goto done;
    }
    int64_t scheduled_out_frame = sessions[0]->next_room_effective_frame;
    if (ulnet__test_expect_room_change_lead(sessions[0], "coordinator leave-mesh")) {
        status = 1;
        goto done;
    }

    int coordinator_mode = 0;
    for (int i = 0; i < 600 && !coordinator_mode; i++) {
        ulnet__test_poll_inproc_sessions(sessions, 2, save_state, sizeof(save_state));
        coordinator_mode =    !ulnet_port_is_p2p(&sessions[0]->room_we_are_in, SAM2_AUTHORITY_INDEX)
                           && !ulnet_port_is_p2p(&sessions[1]->room_we_are_in, SAM2_AUTHORITY_INDEX);
    }
    if (!coordinator_mode) {
        SAM2_LOG_ERROR("coordinator test: authority topology bit did not clear");
        status = 1;
        goto done;
    }
    if (ulnet_port_is_active_player(&sessions[0]->room_we_are_in, SAM2_AUTHORITY_INDEX)) {
        SAM2_LOG_ERROR("coordinator test: authority is still in the active input set");
        status = 1;
        goto done;
    }
    if (sessions[0]->frame_counter < scheduled_out_frame || sessions[1]->frame_counter < scheduled_out_frame) {
        SAM2_LOG_ERROR("coordinator test: authority topology changed before scheduled frame");
        status = 1;
        goto done;
    }

    sessions[0]->delay_frames = 0;
    sessions[1]->delay_frames = 0;

    int64_t player_start_frame = sessions[1]->frame_counter;
    for (int i = 0; i < 80; i++) {
        ulnet__test_poll_inproc_sessions(sessions, 2, save_state, sizeof(save_state));
    }
    if (sessions[1]->frame_counter <= player_start_frame + ULNET_DELAY_FRAMES_MAX + 1) {
        SAM2_LOG_ERROR("coordinator test: zero-delay player did not advance with coordinator heartbeat");
        status = 1;
        goto done;
    }
    if (sessions[0]->peer[player_slot]->state.frame + 1 < sessions[0]->frame_counter) {
        SAM2_LOG_ERROR("coordinator test: authority did not receive remote player input");
        status = 1;
        goto done;
    }

    int64_t authority_snapshot_frame = sessions[1]->peer[SAM2_AUTHORITY_INDEX]->state.frame;
    int64_t blocked_boundary = authority_snapshot_frame + ULNET_ROOM_CHANGE_LEAD_FRAMES;
    for (int i = 0; i < 80 && sessions[1]->frame_counter < blocked_boundary; i++) {
        sessions[0]->core_wants_tick_at_unix_usec = 0;
        ulnet_poll_session(sessions[0], 0, save_state, sizeof(save_state), 60.0, 0.0);
        transport.buf1.count = 0; // Drop authority -> player heartbeat, but keep player -> authority input flowing
        sessions[1]->core_wants_tick_at_unix_usec = 0;
        ulnet_poll_session(sessions[1], 0, save_state, sizeof(save_state), 60.0, 0.0);
    }
    int64_t blocked_frame = sessions[1]->frame_counter;
    for (int i = 0; i < 12; i++) {
        sessions[0]->core_wants_tick_at_unix_usec = 0;
        ulnet_poll_session(sessions[0], 0, save_state, sizeof(save_state), 60.0, 0.0);
        transport.buf1.count = 0;
        sessions[1]->core_wants_tick_at_unix_usec = 0;
        ulnet_poll_session(sessions[1], 0, save_state, sizeof(save_state), 60.0, 0.0);
    }
    if (blocked_frame != blocked_boundary || sessions[1]->frame_counter != blocked_boundary) {
        SAM2_LOG_ERROR("coordinator test: player stalled at frame %" PRId64 " instead of room-change boundary %" PRId64,
            blocked_frame, blocked_boundary);
        status = 1;
        goto done;
    }

    int unblocked = 0;
    for (int i = 0; i < 160 && !unblocked; i++) {
        ulnet__test_poll_inproc_sessions(sessions, 2, save_state, sizeof(save_state));
        unblocked = sessions[1]->frame_counter > blocked_boundary;
    }
    if (!unblocked) {
        SAM2_LOG_ERROR("coordinator test: player did not resume after authority snapshot arrived");
        status = 1;
        goto done;
    }

    int64_t authority_state_before_join = sessions[0]->peer[SAM2_AUTHORITY_INDEX]->state.frame;
    if (ulnet__test_request_local_role_toggle(sessions[0], SAM2_AUTHORITY_INDEX) != 0) {
        status = 1;
        goto done;
    }
    if (ulnet__test_expect_room_change_lead(sessions[0], "coordinator join-mesh")) {
        status = 1;
        goto done;
    }

    int authority_player_mode = 0;
    for (int i = 0; i < 600 && !authority_player_mode; i++) {
        ulnet__test_poll_inproc_sessions(sessions, 2, save_state, sizeof(save_state));
        authority_player_mode =    ulnet_port_is_p2p(&sessions[0]->room_we_are_in, SAM2_AUTHORITY_INDEX)
                                && ulnet_port_is_p2p(&sessions[1]->room_we_are_in, SAM2_AUTHORITY_INDEX);
    }
    if (!authority_player_mode) {
        SAM2_LOG_ERROR("coordinator test: authority topology bit did not set");
        status = 1;
        goto done;
    }
    if (sessions[0]->peer[SAM2_AUTHORITY_INDEX]->state.frame < authority_state_before_join) {
        SAM2_LOG_ERROR("coordinator test: authority frame regressed while joining mesh");
        status = 1;
        goto done;
    }

done:
    if (sessions[0]) {
        ulnet_session_tear_down(sessions[0]);
        free(sessions[0]);
    }
    if (sessions[1]) {
        ulnet_session_tear_down(sessions[1]);
        free(sessions[1]);
    }
    return status;
}

int ulnet_test_inproc_coordinator_two_player_mesh(void) {
    g_test_name = __func__;
    enum { A = 0, B = 1, C = 2, SESSION_COUNT = 3 };
    ulnet_session_t *sessions[SESSION_COUNT] = {0};
    ulnet_transport_inproc_t transport_ab = {0};
    ulnet_transport_inproc_t transport_ac = {0};
    ulnet_transport_inproc_t transport_bc = {0};
    uint8_t save_state[256];
    int status = 0;

    g_ulnet_transport_use_inproc = true;
    for (int i = 0; i < SESSION_COUNT; i++) {
        sessions[i] = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(sessions[i]);
        sessions[i]->retro_run = ulnet__test_retro_run;
        sessions[i]->retro_serialize_size = ulnet__test_retro_serialize_size;
        sessions[i]->retro_serialize = ulnet__test_retro_serialize;
        sessions[i]->retro_unserialize = ulnet__test_retro_unserialize;
        sessions[i]->delay_frames = 0;
    }

    sessions[A]->our_peer_id = 10001;
    sessions[B]->our_peer_id = 30002;
    sessions[C]->our_peer_id = 30003;

    sam2_room_t room = {0};
    room.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    room.peer_ids[SAM2_AUTHORITY_INDEX] = sessions[A]->our_peer_id;
    room.peer_ids[ULNET__TEST_PLAYER1_PORT] = sessions[B]->our_peer_id;
    room.peer_ids[ULNET__TEST_PLAYER2_PORT] = sessions[C]->our_peer_id;
    room.peer_topology = (1ULL << ULNET__TEST_PLAYER1_PORT) | (1ULL << ULNET__TEST_PLAYER2_PORT);

    for (int i = 0; i < SESSION_COUNT; i++) {
        sessions[i]->room_we_are_in = room;
        sessions[i]->next_room = room;
    }

    ulnet__peer_alloc(sessions[A], ULNET__TEST_PLAYER1_PORT);
    ulnet__peer_alloc(sessions[A], ULNET__TEST_PLAYER2_PORT);
    ulnet__peer_alloc(sessions[B], ULNET__TEST_PLAYER2_PORT);
    ulnet__peer_alloc(sessions[C], ULNET__TEST_PLAYER1_PORT);
    ulnet__test_inproc_wire(sessions[A], ULNET__TEST_PLAYER1_PORT, &transport_ab);
    ulnet__test_inproc_wire(sessions[B], SAM2_AUTHORITY_INDEX, &transport_ab);
    ulnet__test_inproc_wire(sessions[A], ULNET__TEST_PLAYER2_PORT, &transport_ac);
    ulnet__test_inproc_wire(sessions[C], SAM2_AUTHORITY_INDEX, &transport_ac);
    ulnet__test_inproc_wire(sessions[B], ULNET__TEST_PLAYER2_PORT, &transport_bc);
    ulnet__test_inproc_wire(sessions[C], ULNET__TEST_PLAYER1_PORT, &transport_bc);

    for (int i = 0; i < 180; i++) {
        ulnet__test_poll_inproc_sessions(sessions, SESSION_COUNT, save_state, sizeof(save_state));
    }

    if (ulnet_port_is_active_player(&sessions[A]->room_we_are_in, SAM2_AUTHORITY_INDEX)) {
        SAM2_LOG_ERROR("two-player coordinator test: authority is active input");
        status = 1;
        goto done;
    }
    if (   !ulnet_port_is_p2p(&sessions[B]->room_we_are_in, ULNET__TEST_PLAYER1_PORT)
        || !ulnet_port_is_p2p(&sessions[B]->room_we_are_in, ULNET__TEST_PLAYER2_PORT)
        || !ulnet_port_is_p2p(&sessions[C]->room_we_are_in, ULNET__TEST_PLAYER1_PORT)
        || !ulnet_port_is_p2p(&sessions[C]->room_we_are_in, ULNET__TEST_PLAYER2_PORT)) {
        SAM2_LOG_ERROR("two-player coordinator test: remote mesh bits are not set");
        status = 1;
        goto done;
    }
    if (sessions[B]->frame_counter < 24 || sessions[C]->frame_counter < 24) {
        SAM2_LOG_ERROR("two-player coordinator test: mesh players did not advance");
        status = 1;
        goto done;
    }
    if (   sessions[A]->peer[ULNET__TEST_PLAYER1_PORT]->state.frame + 1 < sessions[A]->frame_counter
        || sessions[A]->peer[ULNET__TEST_PLAYER2_PORT]->state.frame + 1 < sessions[A]->frame_counter) {
        SAM2_LOG_ERROR("two-player coordinator test: authority did not receive both players");
        status = 1;
        goto done;
    }
    if (   sessions[B]->peer[ULNET__TEST_PLAYER2_PORT]->state.frame + 1 < sessions[B]->frame_counter
        || sessions[C]->peer[ULNET__TEST_PLAYER1_PORT]->state.frame + 1 < sessions[C]->frame_counter) {
        SAM2_LOG_ERROR("two-player coordinator test: remote players did not exchange direct input");
        status = 1;
        goto done;
    }
    if (   sessions[B]->peer[SAM2_AUTHORITY_INDEX]->state.frame < 15
        || sessions[C]->peer[SAM2_AUTHORITY_INDEX]->state.frame < 15) {
        SAM2_LOG_ERROR("two-player coordinator test: authority heartbeat did not reach both players");
        status = 1;
        goto done;
    }

done:
    for (int i = 0; i < SESSION_COUNT; i++) {
        if (sessions[i]) {
            ulnet_session_tear_down(sessions[i]);
            free(sessions[i]);
        }
    }
    return status;
}

int ulnet_test_inproc_high_port_authority_relay(void) {
    g_test_name = __func__;
    enum { A = 0, B = 1, C = 2, SESSION_COUNT = 3 };
    ulnet_session_t *sessions[SESSION_COUNT] = {0};
    ulnet_transport_inproc_t transport_ab = {0};
    ulnet_transport_inproc_t transport_ac = {0};
    uint8_t save_state[256];
    int status = 0;

    g_ulnet_transport_use_inproc = true;
    for (int i = 0; i < SESSION_COUNT; i++) {
        sessions[i] = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(sessions[i]);
        sessions[i]->retro_run = ulnet__test_retro_run;
        sessions[i]->retro_serialize_size = ulnet__test_retro_serialize_size;
        sessions[i]->retro_serialize = ulnet__test_retro_serialize;
        sessions[i]->retro_unserialize = ulnet__test_retro_unserialize;
        sessions[i]->delay_frames = 0;
    }

    sessions[A]->our_peer_id = 10001;
    sessions[B]->our_peer_id = 30002;
    sessions[C]->our_peer_id = 30003;

    sam2_room_t room = {0};
    room.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    room.peer_ids[SAM2_AUTHORITY_INDEX] = sessions[A]->our_peer_id;
    room.peer_ids[ULNET__TEST_PLAYER2_PORT] = sessions[B]->our_peer_id;
    room.peer_ids[ULNET__TEST_RELAY_SPECTATOR_PORT] = sessions[C]->our_peer_id;
    room.peer_topology = (1ULL << ULNET__TEST_PLAYER2_PORT);

    for (int i = 0; i < SESSION_COUNT; i++) {
        sessions[i]->room_we_are_in = room;
        sessions[i]->next_room = room;
    }

    ulnet__peer_alloc(sessions[A], ULNET__TEST_PLAYER2_PORT);
    ulnet__peer_alloc(sessions[A], ULNET__TEST_RELAY_SPECTATOR_PORT);
    ulnet__test_inproc_wire(sessions[A], ULNET__TEST_PLAYER2_PORT, &transport_ab);
    ulnet__test_inproc_wire(sessions[B], SAM2_AUTHORITY_INDEX, &transport_ab);
    ulnet__test_inproc_wire(sessions[A], ULNET__TEST_RELAY_SPECTATOR_PORT, &transport_ac);
    ulnet__test_inproc_wire(sessions[C], SAM2_AUTHORITY_INDEX, &transport_ac);

    for (int i = 0; i < 120; i++) {
        ulnet__test_poll_inproc_sessions(sessions, SESSION_COUNT, save_state, sizeof(save_state));
    }

    if (!ulnet_port_is_p2p(&sessions[A]->room_we_are_in, ULNET__TEST_PLAYER2_PORT)) {
        SAM2_LOG_ERROR("high-port relay test: player high slot is not p2p");
        status = 1;
        goto done;
    }
    if (sessions[A]->peer[ULNET__TEST_PLAYER2_PORT]->state.frame < 16) {
        SAM2_LOG_ERROR("high-port relay test: authority did not decode high-port player input");
        status = 1;
        goto done;
    }
    if (sessions[C]->peer[ULNET__TEST_PLAYER2_PORT]->state.frame < 16) {
        SAM2_LOG_ERROR("high-port relay test: spectator did not receive relayed high-port player input");
        status = 1;
        goto done;
    }
    if (sessions[C]->peer[ULNET__TEST_PLAYER2_PORT]->transport) {
        SAM2_LOG_ERROR("high-port relay test: spectator unexpectedly has a direct high-port link");
        status = 1;
        goto done;
    }

done:
    for (int i = 0; i < SESSION_COUNT; i++) {
        if (sessions[i]) {
            ulnet_session_tear_down(sessions[i]);
            free(sessions[i]);
        }
    }
    return status;
}

int ulnet_test_inproc_spectator_recovers_after_state_burst_loss(void) {
    g_test_name = __func__;
    ulnet_session_t *sessions[2] = {0};
    ulnet_transport_inproc_t transport = {0};
    uint8_t save_state[256];
    int status = 0;

    ulnet__test_inproc_pair_setup(sessions, &transport, 0);
    sessions[0]->delay_frames = 0;
    sessions[1]->delay_frames = 0;

    if (ulnet__test_sync_inproc_pair(sessions, save_state, sizeof(save_state)) != 0) {
        status = 1;
        goto done;
    }

    for (int i = 0; i < 16; i++) {
        ulnet__test_poll_inproc_sessions(sessions, 2, save_state, sizeof(save_state));
    }

    sessions[1]->debug_udp_recv_drop_rate = 1.0f;
    for (int i = 0; i < 40; i++) {
        ulnet__test_poll_inproc_sessions(sessions, 2, save_state, sizeof(save_state));
    }
    sessions[1]->debug_udp_recv_drop_rate = 0.0f;

    int64_t outage_gap = sessions[0]->frame_counter - sessions[1]->frame_counter;
    if (outage_gap < 20) {
        SAM2_LOG_ERROR("spectator burst-loss recovery test: spectator only fell %" PRId64 " frames behind", outage_gap);
        status = 1;
        goto done;
    }

    int recovered = 0;
    for (int i = 0; i < 300; i++) {
        sessions[0]->core_wants_tick_at_unix_usec = 0;
        ulnet_poll_session(sessions[0], 0, save_state, sizeof(save_state), 60.0, 0.0);

        for (int j = 0; j < 4; j++) {
            sessions[1]->core_wants_tick_at_unix_usec = 0;
            ulnet_poll_session(sessions[1], 0, save_state, sizeof(save_state), 60.0, 0.0);
        }

        if (sessions[0]->frame_counter - sessions[1]->frame_counter <= ULNET_DELAY_FRAMES_MAX + 1) {
            recovered = 1;
            break;
        }
    }

    if (!recovered) {
        SAM2_LOG_ERROR("spectator burst-loss recovery test: spectator did not recover from %" PRId64
            "-frame gap (authority=%" PRId64 " spectator=%" PRId64 " auth_state=%" PRId64 ")",
            outage_gap, sessions[0]->frame_counter, sessions[1]->frame_counter,
            sessions[1]->peer[SAM2_AUTHORITY_INDEX]->state.frame);
        status = 1;
        goto done;
    }

done:
    if (sessions[0]) {
        ulnet_session_tear_down(sessions[0]);
        free(sessions[0]);
    }
    if (sessions[1]) {
        ulnet_session_tear_down(sessions[1]);
        free(sessions[1]);
    }
    return status;
}

// Returns the payload of the reliable packet the authority recorded for `sequence`, or NULL.
static const uint8_t *ulnet__test_rx_payload(ulnet_session_t *authority, uint16_t sequence) {
    ulnet_peer_diagnostics_t *diagnostics =
        authority->peer[ULNET__TEST_SPECTATOR_PORT]->diagnostics;
    ulnet_reliable_packet_t *rx = (ulnet_reliable_packet_t *)
        diagnostics->reliable_rx_packet_history[sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE].data;
    return rx ? rx->payload : NULL;
}

// Exercise the reliable channel directly (no ticking / savestate): the sliding window must keep multiple
// packets in flight and the receiver must deliver them in order.
int ulnet_test_inproc(ulnet_session_t **session_1_out, ulnet_session_t **session_2_out) {
    g_test_name = __func__;
    ulnet_session_t *sessions[2] = {0};
    ulnet_transport_inproc_t transport = {0};
    int status = 0;
    const uint8_t *p0, *p1, *p2;

    ulnet__test_inproc_pair_setup(sessions, &transport, 0);
    sessions[0]->peer_needs_sync_bitfield = 0; // Reliable-channel only: no savestate sync / ticking
    sessions[1]->frame_counter = 0;

    // Three reliable messages with no ACK in between -- a stop-and-wait sender could only put one in
    // flight; the sliding window puts all three out at once.
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "AAA", 3);
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "BBB", 3);
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "CCC", 3);

    if ((uint16_t)(sessions[1]->peer[SAM2_AUTHORITY_INDEX]->reliable_tx_next_seq
                 - sessions[1]->peer[SAM2_AUTHORITY_INDEX]->reliable_tx_head) != 3) {
        SAM2_LOG_ERROR("Expected three unacked reliable packets in flight");
        status = 1;
        goto done;
    }

    ulnet_service_network(sessions[0], 0); // Authority drains all three

    if (sessions[0]->peer[ULNET__TEST_SPECTATOR_PORT]->reliable_rx_head != 3) {
        SAM2_LOG_ERROR("Authority did not deliver all three reliable packets (rx_head=%u)",
            sessions[0]->peer[ULNET__TEST_SPECTATOR_PORT]->reliable_rx_head);
        status = 1;
        goto done;
    }

    p0 = ulnet__test_rx_payload(sessions[0], 0);
    p1 = ulnet__test_rx_payload(sessions[0], 1);
    p2 = ulnet__test_rx_payload(sessions[0], 2);
    if (!(   p0 && memcmp(p0, "AAA", 3) == 0
          && p1 && memcmp(p1, "BBB", 3) == 0
          && p2 && memcmp(p2, "CCC", 3) == 0)) {
        SAM2_LOG_ERROR("Reliable packets did not arrive in order");
        status = 1;
        goto done;
    }

    // Delivery must generate a cumulative ACK without waiting for unrelated application traffic.
    ulnet_service_network(sessions[1], 0);

    if (sessions[1]->peer[SAM2_AUTHORITY_INDEX]->reliable_tx_head != 3) {
        SAM2_LOG_ERROR("Immediate cumulative ACK did not free the transmit window (tx_head=%u)",
            sessions[1]->peer[SAM2_AUTHORITY_INDEX]->reliable_tx_head);
        status = 1;
    }

done:
    ulnet_session_tear_down(sessions[0]);
    ulnet_session_tear_down(sessions[1]);
    if (!session_1_out) free(sessions[0]);
    else *session_1_out = sessions[0];

    if (!session_2_out) free(sessions[1]);
    else *session_2_out = sessions[1];

    return status;
}

// Drop a middle sequence, deliver later sequences out of order, then retransmit the missing one and
// confirm the buffered packets drain in order.
int ulnet_test_inproc_reliable_ack_unblocks_queue(void) {
    g_test_name = __func__;
    ulnet_session_t *sessions[2] = {0};
    ulnet_transport_inproc_t transport = {0};
    int status = 0;
    const uint8_t *p0, *p1, *p2;

    ulnet__test_inproc_pair_setup(sessions, &transport, 0);
    sessions[0]->peer_needs_sync_bitfield = 0;
    sessions[1]->frame_counter = 0;

    // s1 has the larger peer_id, so its packets toward the authority queue in buf2.
    ulnet_inproc_buf_t *to_authority = &transport.buf2;

    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "AAA", 3); // seq 0
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "BBB", 3); // seq 1 (dropped in transit)
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "CCC", 3); // seq 2

    if (to_authority->count != 3) {
        SAM2_LOG_ERROR("Expected three queued reliable packets in transport (got %d)", to_authority->count);
        status = 1;
        goto done;
    }

    // Drop the middle packet (seq 1) by removing it from the transport queue.
    memcpy(to_authority->msg[1], to_authority->msg[2], to_authority->msg_size[2]);
    to_authority->msg_size[1] = to_authority->msg_size[2];
    to_authority->count = 2;

    ulnet_service_network(sessions[0], 0); // Receives seq 0 (delivered) and seq 2 (buffered out of order)

    if (sessions[0]->peer[ULNET__TEST_SPECTATOR_PORT]->reliable_rx_head != 1) {
        SAM2_LOG_ERROR("Out-of-order packet was delivered early (rx_head=%u)",
            sessions[0]->peer[ULNET__TEST_SPECTATOR_PORT]->reliable_rx_head);
        status = 1;
        goto done;
    }
    if (sessions[0]->peer[ULNET__TEST_SPECTATOR_PORT]->reliable_rx_pending[2 % ULNET_RELIABLE_ACK_BUFFER_SIZE].data == NULL) {
        SAM2_LOG_ERROR("Out-of-order packet was not buffered in the reorder buffer");
        status = 1;
        goto done;
    }

    // Retransmit (delay is 0) redelivers seq 1; seq 2 then drains behind it.
    ulnet_service_network(sessions[1], 0);
    ulnet_service_network(sessions[0], 0);

    if (sessions[0]->peer[ULNET__TEST_SPECTATOR_PORT]->reliable_rx_head != 3) {
        SAM2_LOG_ERROR("Reorder buffer did not drain after retransmit (rx_head=%u)",
            sessions[0]->peer[ULNET__TEST_SPECTATOR_PORT]->reliable_rx_head);
        status = 1;
        goto done;
    }

    p0 = ulnet__test_rx_payload(sessions[0], 0);
    p1 = ulnet__test_rx_payload(sessions[0], 1);
    p2 = ulnet__test_rx_payload(sessions[0], 2);
    if (!(   p0 && memcmp(p0, "AAA", 3) == 0
          && p1 && memcmp(p1, "BBB", 3) == 0
          && p2 && memcmp(p2, "CCC", 3) == 0)) {
        SAM2_LOG_ERROR("Reliable packets did not drain in order after retransmit");
        status = 1;
    }

done:
    ulnet_session_tear_down(sessions[0]);
    ulnet_session_tear_down(sessions[1]);
    free(sessions[0]);
    free(sessions[1]);

    return status;
}

int ulnet_test_inproc_savestate_retry_failure_goes_solo(void) {
    g_test_name = __func__;
    ulnet_session_t *sessions[2] = {0};
    ulnet_transport_inproc_t transport = {0};
    uint8_t save_state[256];
    uint64_t spectator_bit = 1ULL << ULNET__TEST_SPECTATOR_PORT;
    int status = 0;

    ulnet__test_inproc_pair_setup(sessions, &transport, 0);
    sessions[0]->delay_frames = 0;

    sessions[0]->core_wants_tick_at_unix_usec = 0;
    ulnet_poll_session(sessions[0], 0, save_state, sizeof(save_state), 60.0, 0.0);
    if (sessions[0]->savestate_transfer_awaiting_bitfield != spectator_bit ||
        sessions[0]->savestate_transfer_retry_count != 0 ||
        sessions[0]->peer_needs_sync_bitfield != 0) {
        SAM2_LOG_ERROR("savestate retry test: initial transfer was not active as expected");
        status = 1;
        goto done;
    }
    uint8_t first_transfer_id = sessions[0]->savestate_transfer_id;

    sessions[0]->savestate_transfer_ack_deadline_unix_usec = ulnet__get_unix_time_microseconds() - 1;
    sessions[0]->core_wants_tick_at_unix_usec = 0;
    ulnet_poll_session(sessions[0], 0, save_state, sizeof(save_state), 60.0, 0.0);
    if (sessions[0]->savestate_transfer_awaiting_bitfield != spectator_bit ||
        sessions[0]->savestate_transfer_retry_count != 1 ||
        sessions[0]->peer_needs_sync_bitfield != 0 ||
        sessions[0]->savestate_transfer_id == first_transfer_id) {
        SAM2_LOG_ERROR("savestate retry test: first timeout did not start retry transfer");
        status = 1;
        goto done;
    }
    uint8_t second_transfer_id = sessions[0]->savestate_transfer_id;

    sessions[0]->savestate_transfer_ack_deadline_unix_usec = ulnet__get_unix_time_microseconds() - 1;
    sessions[0]->core_wants_tick_at_unix_usec = 0;
    ulnet_poll_session(sessions[0], 0, save_state, sizeof(save_state), 60.0, 0.0);
    if (sessions[0]->savestate_transfer_awaiting_bitfield != spectator_bit ||
        sessions[0]->savestate_transfer_retry_count != 2 ||
        sessions[0]->peer_needs_sync_bitfield != 0 ||
        sessions[0]->savestate_transfer_id == second_transfer_id) {
        SAM2_LOG_ERROR("savestate retry test: second timeout did not start retry transfer");
        status = 1;
        goto done;
    }

    sessions[0]->savestate_transfer_ack_deadline_unix_usec = ulnet__get_unix_time_microseconds() - 1;
    sessions[0]->core_wants_tick_at_unix_usec = 0;
    ulnet_poll_session(sessions[0], 0, save_state, sizeof(save_state), 60.0, 0.0);
    if (   sessions[0]->savestate_transfer_awaiting_bitfield != 0
        || sessions[0]->savestate_transfer_retry_count != 0
        || sessions[0]->peer_needs_sync_bitfield != 0
        || (sessions[0]->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)
        || sessions[0]->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] != sessions[0]->our_peer_id
        || sessions[0]->room_we_are_in.peer_ids[ULNET__TEST_SPECTATOR_PORT] != SAM2_PORT_AVAILABLE
        || sessions[0]->room_we_are_in.peer_topology != (1ULL << SAM2_AUTHORITY_INDEX)) {
        SAM2_LOG_ERROR("savestate retry test: terminal failure did not reset to solo room");
        status = 1;
        goto done;
    }

done:
    ulnet_session_tear_down(sessions[0]);
    ulnet_session_tear_down(sessions[1]);
    free(sessions[0]);
    free(sessions[1]);
    return status;
}

int ulnet_test_inproc_multigroup_savestate(void) {
    enum { SAVESTATE_SIZE = 700000 };
    ulnet_session_t *sessions[2] = {0};
    ulnet_transport_inproc_t transport = {0};
    uint8_t *save_state = (uint8_t *)malloc(SAVESTATE_SIZE);
    int status = 1;
    if (!save_state) return 1;

    uint32_t random = 1;
    for (int i = 0; i < SAVESTATE_SIZE; i++) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        save_state[i] = (uint8_t)random;
    }

    ulnet__test_inproc_pair_setup(sessions, &transport, 0);
    g_large_savestate = save_state;
    g_large_savestate_size = SAVESTATE_SIZE;
    g_large_savestate_received = false;
    sessions[1]->retro_unserialize = ulnet__test_large_retro_unserialize;

    uint64_t spectator_bit = 1ULL << ULNET__TEST_SPECTATOR_PORT;
    if (ulnet__send_save_state_to_peers(sessions[0], spectator_bit,
            save_state, SAVESTATE_SIZE, 0) != 0
        || !sessions[0]->savestate_tx_payload) {
        SAM2_LOG_ERROR("multigroup savestate test: transfer did not remain pending after its first group");
        goto done_multigroup;
    }

    for (int i = 0; i < FEC_PACKET_GROUPS_MAX + 1 && !g_large_savestate_received; i++) {
        ulnet_service_network(sessions[1], 0);
        if (sessions[0]->savestate_tx_payload) {
            sessions[0]->savestate_tx_next_group_unix_usec = 0;
            ulnet_service_network(sessions[0], 0);
        }
    }
    ulnet_service_network(sessions[1], 0);
    ulnet_service_network(sessions[0], 0);

    if (!g_large_savestate_received
        || sessions[0]->savestate_tx_payload
        || sessions[0]->savestate_transfer_awaiting_bitfield) {
        SAM2_LOG_ERROR("multigroup savestate test: transfer did not complete");
        goto done_multigroup;
    }
    status = 0;

done_multigroup:
    ulnet_session_tear_down(sessions[0]);
    ulnet_session_tear_down(sessions[1]);
    free(sessions[0]);
    free(sessions[1]);
    free(save_state);
    g_large_savestate = NULL;
    g_large_savestate_size = 0;
    return status;
}

static uint32_t ulnet__test_fuzz_next(uint32_t *rng) {
    *rng = *rng * 1664525u + 1013904223u;
    return *rng;
}


// Churn the room (peers joining, leaving, and toggling player<->spectator in place) and assert the
// fixed-slot invariant after every step: a peer never moves slots, an unchanged occupant keeps its
// exact agent and per-port state, and changing a slot's occupant fully reconstructs its agent.
int ulnet_test_inproc_room_switch_fuzz(void) {
    g_test_name = __func__;
    enum { ITERATIONS = 400 };
    ulnet_session_t session;
    // The per-slot transports are ~0.5MB each; keep the 64-wide array off the stack
    ulnet_transport_inproc_t *transports = (ulnet_transport_inproc_t *) calloc(SAM2_TOTAL_PEERS, sizeof(ulnet_transport_inproc_t));
    uint16_t expected_peer[SAM2_TOTAL_PEERS];
    void    *expected_agent[SAM2_TOTAL_PEERS];
    uint16_t expected_rx[SAM2_TOTAL_PEERS];
    uint32_t rng = 0xC0FFEEu;
    uint16_t next_peer_id = 20000;

    memset(&session, 0, sizeof(session));
    memset(expected_peer, 0, sizeof(expected_peer));
    memset(expected_agent, 0, sizeof(expected_agent));
    memset(expected_rx, 0, sizeof(expected_rx));

    ulnet_session_init_defaulted(&session);
    g_ulnet_transport_use_inproc = true;
    session.our_peer_id = 10001;
    session.sam2_send_callback = ulnet__test_discard_send_callback;
    session.room_we_are_in.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session.our_peer_id;

    for (int iter = 0; iter < ITERATIONS; iter++) {
        sam2_room_t next = session.room_we_are_in;
        int op = (int)(ulnet__test_fuzz_next(&rng) % 3);
        int slot = 1 + (int)(ulnet__test_fuzz_next(&rng) % (SAM2_TOTAL_PEERS - 1)); // never disturb the authority at port 0
        bool joined = false;

        if (op == 0 && next.peer_ids[slot] == SAM2_PORT_AVAILABLE) {
            next.peer_ids[slot] = next_peer_id++; // A new peer joins this slot as a spectator
            joined = true;
        } else if (op == 1 && next.peer_ids[slot] > SAM2_PORT_SENTINELS_MAX) {
            next.peer_ids[slot] = SAM2_PORT_AVAILABLE; // Peer leaves
            next.peer_topology &= ~(1ULL << slot);
        } else if (op == 2 && next.peer_ids[slot] > SAM2_PORT_SENTINELS_MAX) {
            next.peer_topology ^= (1ULL << slot); // Toggle player<->spectator in place (same peer)
        } else {
            continue;
        }

        ulnet__reconcile_connections(&session, &next);

        if (joined) {
            // The transport layer wires a fresh agent once the connection is established; reconcile
            // does not create inproc agents. Mimic that here for the newly occupied slot.
            ulnet__test_inproc_wire(&session, slot, &transports[slot]);
            session.peer[slot]->reliable_rx_head = (uint16_t)(0x1000 + slot);
            expected_peer[slot] = session.room_we_are_in.peer_ids[slot];
            expected_agent[slot] = &transports[slot];
            expected_rx[slot] = (uint16_t)(0x1000 + slot);
        } else if (session.room_we_are_in.peer_ids[slot] == SAM2_PORT_AVAILABLE) {
            expected_peer[slot] = 0;
            expected_agent[slot] = NULL;
            expected_rx[slot] = 0;
        }

        // Validate the whole room: nothing swapped, nothing reused across a different peer
        for (int p = 1; p < SAM2_TOTAL_PEERS; p++) {
            if (expected_peer[p] != 0) {
                if (   session.room_we_are_in.peer_ids[p] != expected_peer[p]
                    || (void *)((ulnet_inproc_conn_t *)session.peer[p]->transport)->shared != expected_agent[p]
                    || session.peer[p]->reliable_rx_head != expected_rx[p]) {
                    SAM2_LOG_ERROR("Slot %d state moved/changed unexpectedly at iteration %d (op %d)", p, iter, op);
                    return 1;
                }
            } else {
                if (session.room_we_are_in.peer_ids[p] != SAM2_PORT_AVAILABLE || session.peer[p] != NULL) {
                    SAM2_LOG_ERROR("Freed slot %d still occupied at iteration %d (op %d)", p, iter, op);
                    return 1;
                }
            }
        }
    }

    ulnet_session_tear_down(&session);
    free(transports);
    return 0;
}

int ulnet_test_reliable_rejects_bad_sequence_state(void) {
    g_test_name = __func__;
    ulnet_session_t session = {0};
    uint8_t packet[ULNET_PACKET_SIZE_BYTES_MAX] = {0};
    uint8_t ack_packet[sizeof(ulnet_reliable_packet_t)] = {0};
    int status = 0;

    ulnet_session_init_defaulted(&session);

    if (ulnet_reliable_send(&session, SAM2_AUTHORITY_INDEX, packet, sizeof(packet)) >= 0) {
        SAM2_LOG_ERROR("Oversized reliable packet unexpectedly sent");
        status = 1;
    }

    if (session.peer[SAM2_AUTHORITY_INDEX]->reliable_tx_next_seq != 0) {
        SAM2_LOG_ERROR("Oversized reliable packet consumed a sequence number");
        status = 1;
    }

    session.peer[SAM2_AUTHORITY_INDEX]->reliable_tx_next_seq = 2;
    session.peer[SAM2_AUTHORITY_INDEX]->reliable_tx_head = 0;
    ack_packet[0] = ULNET_CHANNEL_RELIABLE | ULNET_RELIABLE_FLAG_ACK_ONLY;
    ack_packet[3] = 3;

    ulnet__process_udp_packet(&session, SAM2_AUTHORITY_INDEX, ack_packet, sizeof(ack_packet));
    if (session.peer[SAM2_AUTHORITY_INDEX]->reliable_tx_head != 0) {
        SAM2_LOG_ERROR("Invalid reliable ACK advanced tx head");
        status = 1;
    }

    ulnet_session_tear_down(&session);
    return status;
}

static void ulnet__test_fill_zstd_buffer(uint8_t *data, size_t size, int pattern) {
    for (size_t i = 0; i < size; i++) {
        switch (pattern) {
        case 0:
            data[i] = 0;
            break;
        case 1:
            data[i] = (uint8_t)(i & 0xff);
            break;
        case 2:
            data[i] = (uint8_t)((i * 37 + i / 3 + 11) & 0xff);
            break;
        default:
            data[i] = (uint8_t)((i % 251) == 0 ? i : 0x5a);
            break;
        }
    }
}

int ulnet_test_zstd_codec(void) {
    g_test_name = __func__;
    const size_t test_sizes[] = {0, 1, 2, 3, 31, 4096, 65536};
    uint8_t empty_sentinel = 0;

    for (int pattern = 0; pattern < 4; pattern++) {
        for (size_t i = 0; i < sizeof(test_sizes) / sizeof(test_sizes[0]); i++) {
            size_t size = test_sizes[i];
            uint8_t *original = size ? (uint8_t *)malloc(size) : &empty_sentinel;
            uint8_t *decoded = size ? (uint8_t *)malloc(size) : &empty_sentinel;
            size_t compressed_capacity = (size_t)ULNET_ZSTD_COMPRESS_BOUND(size);
            uint8_t *compressed = (uint8_t *)malloc(compressed_capacity ? compressed_capacity : 1);

            if (!original || !decoded || !compressed) {
                SAM2_LOG_ERROR("Failed to allocate zstd test buffers");
                if (size && original) free(original);
                if (size && decoded) free(decoded);
                if (compressed) free(compressed);
                return 1;
            }

            ulnet__test_fill_zstd_buffer(original, size, pattern);

            int64_t compressed_size = ULNET_ZSTD_COMPRESS(compressed, compressed_capacity, original, size, 8);
            if (compressed_size < 0) {
                SAM2_LOG_ERROR("ULNET_ZSTD_COMPRESS failed for size %zu pattern %d", size, pattern);
                if (size) free(original);
                if (size) free(decoded);
                free(compressed);
                return 1;
            }

            int64_t decoded_size = ULNET_ZSTD_DECOMPRESS(decoded, size, compressed, compressed_size);
            if (decoded_size != (int64_t)size || memcmp(original, decoded, size) != 0) {
                SAM2_LOG_ERROR("ULNET_ZSTD_DECOMPRESS round trip failed for size %zu pattern %d", size, pattern);
                if (size) free(original);
                if (size) free(decoded);
                free(compressed);
                return 1;
            }

            if (size) free(original);
            if (size) free(decoded);
            free(compressed);
        }
    }

    return 0;
}

// These exercise ulnet.h's internal (static) packed encode/decode/validate helpers, so they only build
// when the implementation is in this same translation unit (the preferred tcc/ULNET_TEST_MAIN command).
// The cmake netarch build compiles ulnet_test.c separately from the implementation, so it skips them.
#if defined(ULNET_IMPLEMENTATION)

// Encode one frame of state for a player at SAM2_AUTHORITY_INDEX, then decode it into a fresh session and
// confirm every field (input, hashes, ping header, optional room, optional core option) round trips.
int ulnet_test_packed_state_round_trip(void) {
    g_test_name = __func__;
    ulnet_session_t *enc = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
    ulnet_session_t *dec = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
    int status = 0;

    ulnet_session_init_defaulted(enc);
    ulnet_session_init_defaulted(dec);

    sam2_room_t room = {0};
    room.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    room.peer_ids[SAM2_AUTHORITY_INDEX] = 12345;
    room.peer_topology = (1ULL << SAM2_AUTHORITY_INDEX);
    enc->room_we_are_in = room;
    dec->room_we_are_in = room;

    int port = SAM2_AUTHORITY_INDEX;

    int64_t frame = 1000;
    ulnet_peer_state_t *st = &enc->peer[port]->state;
    st->frame = frame;
    st->room_effective_frame = 1234;
    st->save_state_frame = 999;
    st->room = room;
    st->save_state_hash = 0xDEADBEEFu;
    st->input_state_hash = 0xCAFEBABEu;
    ulnet_input_state_t input[ULNET_PORT_COUNT] = {{0}};
    input[0][0] = 1; // joypad bit 0
    input[0][5] = 1; // joypad bit 5
    input[0][ULNET_INPUT_ANALOG_FIRST_INDEX] = -1234;
    input[7][7] = 1;
    input[7][ULNET_INPUT_ANALOG_FIRST_INDEX] = 2345;
    strcpy(enc->next_core_option.key, "netplay_delay_frames");
    strcpy(enc->next_core_option.value, "3");

    uint8_t packet[ULNET_PACKET_SIZE_BYTES_MAX];
    int64_t size = ulnet__encode_state_packet(enc, port, input, 0xff, 0x1122334455667788LL,
        &enc->next_core_option, packet, sizeof(packet));
    if (size < 0) {
        SAM2_LOG_ERROR("packed state round trip: encode failed");
        status = 1;
        goto done;
    }

    // Ping header fields ride alongside the payload.
    enc->peer[port]->last_packet_send_unix_usec = 0x4242;
    ulnet__stamp_state_packet_ping(enc, port, packet, 0x9999);
    ulnet_state_packet_t *hdr = (ulnet_state_packet_t *) packet;
    if (ulnet__read_le64s(hdr->ping_send_unix_usec_le) != 0x9999
        || ulnet__read_le64s(hdr->ping_echo_send_unix_usec_le) != 0x4242) {
        SAM2_LOG_ERROR("packed state round trip: ping fields not preserved");
        status = 1;
        goto done;
    }

    if (ulnet__validate_packed_state_packet(packet, size) != 0) {
        SAM2_LOG_ERROR("packed state round trip: validation rejected a valid packet");
        status = 1;
        goto done;
    }

    if (ulnet__decode_packed_state_packet(dec, port, packet, size) != 0) {
        SAM2_LOG_ERROR("packed state round trip: decode failed");
        status = 1;
        goto done;
    }

    ulnet_update_state_history(dec, packet, size);

    ulnet_peer_state_t *got = &dec->peer[port]->state;
    if (   got->frame != frame
        || got->room_effective_frame != 1234
        || got->save_state_frame != 999
        || ulnet__state_history_input_poll_time(dec, port, frame) != 0x1122334455667788LL
        || got->save_state_hash != 0xDEADBEEFu
        || got->input_state_hash != 0xCAFEBABEu) {
        SAM2_LOG_ERROR("packed state round trip: scalar field mismatch");
        status = 1;
        goto done;
    }
    ulnet_input_state_t roundtrip_input;
    ulnet__state_history_unpack_input(dec, port, frame, 0, roundtrip_input);
    if (   roundtrip_input[0] != 1
        || roundtrip_input[5] != 1
        || roundtrip_input[1] != 0
        || roundtrip_input[ULNET_INPUT_ANALOG_FIRST_INDEX] != -1234) {
        SAM2_LOG_ERROR("packed state round trip: input did not round trip");
        status = 1;
        goto done;
    }
    ulnet__state_history_unpack_input(dec, port, frame, 7, roundtrip_input);
    if (roundtrip_input[7] != 1
        || roundtrip_input[ULNET_INPUT_ANALOG_FIRST_INDEX] != 2345) {
        SAM2_LOG_ERROR("packed state round trip: all-port input did not round trip");
        status = 1;
        goto done;
    }
    ulnet_core_option_t roundtrip_option;
    ulnet__state_history_core_option(dec, port, frame, &roundtrip_option);
    if (   strcmp(roundtrip_option.key, "netplay_delay_frames") != 0
        || strcmp(roundtrip_option.value, "3") != 0) {
        SAM2_LOG_ERROR("packed state round trip: core option did not round trip");
        status = 1;
        goto done;
    }
    if (memcmp(&got->room, &room, sizeof(room)) != 0) {
        SAM2_LOG_ERROR("packed state round trip: room snapshot did not round trip");
        status = 1;
        goto done;
    }

done:
    ulnet_session_tear_down(enc);
    ulnet_session_tear_down(dec);
    free(enc);
    free(dec);
    return status;
}

// Pack suggested input for every controller port and confirm it round trips, and that decode clears
// stale state first.
int ulnet_test_packed_spectator_round_trip(void) {
    g_test_name = __func__;
    ulnet_input_state_t in[ULNET_PORT_COUNT];
    ulnet_input_state_t out[ULNET_PORT_COUNT];
    uint8_t packet[ULNET_PACKED_SPECTATOR_INPUT_SIZE_MAX];
    int status = 0;

    memset(in, 0, sizeof(in));
    for (int port = 0; port < ULNET_PORT_COUNT; port++) {
        in[port][0] = 1;                                          // joypad bit 0 on every port
        in[port][port % ULNET_INPUT_JOYPAD_WORDS] = 1;            // a port-specific joypad bit
        in[port][ULNET_INPUT_ANALOG_FIRST_INDEX] = (int16_t)(100 + port);
    }

    int packet_size = ulnet__encode_spectator_input(in, packet, sizeof(packet));

    if (packet_size != ULNET_PACKED_SPECTATOR_INPUT_SIZE_MAX
        || packet[0] != ULNET_CHANNEL_SPECTATOR_INPUT) {
        SAM2_LOG_ERROR("spectator round trip: wrong channel byte");
        status = 1;
        goto done;
    }

    // Pre-fill the destination with garbage to prove decode clears it first.
    for (int port = 0; port < ULNET_PORT_COUNT; port++) {
        for (int i = 0; i < (int)(sizeof(ulnet_input_state_t) / sizeof(int16_t)); i++) {
            out[port][i] = 0x5a5a;
        }
    }

    if (ulnet__decode_spectator_input(packet, packet_size, out) < 0) {
        SAM2_LOG_ERROR("spectator round trip: decode failed");
        status = 1;
        goto done;
    }

    for (int port = 0; port < ULNET_PORT_COUNT; port++) {
        if (   out[port][0] != 1
            || out[port][port % ULNET_INPUT_JOYPAD_WORDS] != 1
            || out[port][ULNET_INPUT_ANALOG_FIRST_INDEX] != (int16_t)(100 + port)) {
            SAM2_LOG_ERROR("spectator round trip: port %d input did not round trip", port);
            status = 1;
            goto done;
        }
        // A joypad bit we never set must have been cleared (proves decode zeroes stale state).
        if (out[port][7] != 0 && (port % ULNET_INPUT_JOYPAD_WORDS) != 7 && port != 7) {
            SAM2_LOG_ERROR("spectator round trip: port %d stale state not cleared", port);
            status = 1;
            goto done;
        }
    }

done:
    return status;
}

// Spectator input normally rides inside an ACK-only reliable wrapper. This catches regressions where
// ACK-only packets are treated as empty control packets and their latest-only payload is dropped.
int ulnet_test_packed_spectator_ack_only_delivery(void) {
    g_test_name = __func__;
    ulnet_session_t *sessions[2] = {0};
    ulnet_transport_inproc_t transport = {0};
    uint8_t packet[ULNET_PACKED_SPECTATOR_INPUT_SIZE_MAX];
    int status = 0;

    ulnet__test_inproc_pair_setup(sessions, &transport, 0);
    sessions[0]->peer_needs_sync_bitfield = 0;
    sessions[1]->frame_counter = 0;

    ulnet__peer_alloc(sessions[1], 63); // zeroes the whole peer, including spectator_suggested_input_state
    sessions[1]->peer[63]->spectator_suggested_input_state[0][0] = 1;
    sessions[1]->peer[63]->spectator_suggested_input_state[3][5] = 1;
    sessions[1]->peer[63]->spectator_suggested_input_state[3][ULNET_INPUT_ANALOG_FIRST_INDEX] = -321;
    int packet_size = ulnet__encode_spectator_input(
        sessions[1]->peer[63]->spectator_suggested_input_state,
        packet, sizeof(packet));

    if (ulnet_reliable_send_with_acks_only(sessions[1], SAM2_AUTHORITY_INDEX,
            packet, packet_size) != 0) {
        SAM2_LOG_ERROR("spectator ack-only delivery: send failed");
        status = 1;
        goto done;
    }

    ulnet_service_network(sessions[0], 0);

    if (   sessions[0]->peer[ULNET__TEST_SPECTATOR_PORT]->spectator_suggested_input_state[0][0] != 1
        || sessions[0]->peer[ULNET__TEST_SPECTATOR_PORT]->spectator_suggested_input_state[3][5] != 1
        || sessions[0]->peer[ULNET__TEST_SPECTATOR_PORT]->spectator_suggested_input_state[3][ULNET_INPUT_ANALOG_FIRST_INDEX] != -321) {
        SAM2_LOG_ERROR("spectator ack-only delivery: payload was not delivered through wrapper");
        status = 1;
    }

done:
    if (sessions[0]) {
        ulnet_session_tear_down(sessions[0]);
        free(sessions[0]);
    }
    if (sessions[1]) {
        ulnet_session_tear_down(sessions[1]);
        free(sessions[1]);
    }
    return status;
}

// Validation must reject truncated packets, bad controller ports, malformed optional data, and trailing bytes.
int ulnet_test_packed_state_validation(void) {
    g_test_name = __func__;
    ulnet_session_t *enc = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
    int status = 0;

    ulnet_session_init_defaulted(enc);

    sam2_room_t room = {0};
    room.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    room.peer_ids[SAM2_AUTHORITY_INDEX] = 777;
    room.peer_topology = (1ULL << SAM2_AUTHORITY_INDEX);
    enc->room_we_are_in = room;

    int port = SAM2_AUTHORITY_INDEX;
    enc->peer[port]->state.frame = 5;
    enc->peer[port]->state.room = room;
    strcpy(enc->next_core_option.key, "k");
    strcpy(enc->next_core_option.value, "v");

    uint8_t packet[ULNET_PACKET_SIZE_BYTES_MAX];
    ulnet_input_state_t input[ULNET_PORT_COUNT] = {{0}};
    int64_t size = ulnet__encode_state_packet(enc, port, input, 0x01, 0,
        &enc->next_core_option, packet, sizeof(packet));
    if (size < 0 || ulnet__validate_packed_state_packet(packet, size) != 0) {
        SAM2_LOG_ERROR("validation test: baseline packet was not valid");
        status = 1;
        goto done;
    }

    // Truncated below the fixed header/payload.
    if (ulnet__validate_packed_state_packet(packet, sizeof(ulnet_state_packet_t)) == 0) {
        SAM2_LOG_ERROR("validation test: accepted a truncated packet");
        status = 1;
        goto done;
    }

    // Trailing byte beyond the encoded optional data.
    if (ulnet__validate_packed_state_packet(packet, size + 1) == 0) {
        SAM2_LOG_ERROR("validation test: accepted trailing bytes");
        status = 1;
        goto done;
    }

    {
        // Claim all eight input ports without appending the additional packed frames.
        uint8_t bad[ULNET_PACKET_SIZE_BYTES_MAX];
        memcpy(bad, packet, size);
        ulnet_packed_state_payload_t *payload = (ulnet_packed_state_payload_t *)&bad[sizeof(ulnet_state_packet_t)];
        payload->variable[0] = 0xff;
        if (ulnet__validate_packed_state_packet(bad, size) == 0) {
            SAM2_LOG_ERROR("validation test: accepted truncated packed input fields");
            status = 1;
            goto done;
        }
    }

    {
        // Malformed optional: claim a core option is present but provide no key/value bytes.
        uint8_t bad[ULNET_PACKET_SIZE_BYTES_MAX];
        ulnet_session_t *plain = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(plain);
        // No room/core option -> a minimal packet; flip on the core-option flag without appending data.
        ulnet_input_state_t input[ULNET_PORT_COUNT] = {{0}};
        int64_t plain_size = ulnet__encode_state_packet(plain, ULNET__TEST_PLAYER1_PORT,
            input, 0, 0, NULL, bad, sizeof(bad));
        ulnet_packed_state_payload_t *payload = (ulnet_packed_state_payload_t *)&bad[sizeof(ulnet_state_packet_t)];
        payload->flags |= ULNET_PACKED_STATE_FLAG_CORE_OPTION_PRESENT;
        int reject = ulnet__validate_packed_state_packet(bad, plain_size) != 0;
        ulnet_session_tear_down(plain);
        free(plain);
        if (plain_size < 0 || !reject) {
            SAM2_LOG_ERROR("validation test: accepted malformed optional data");
            status = 1;
            goto done;
        }
    }

done:
    ulnet_session_tear_down(enc);
    free(enc);
    return status;
}

int ulnet_test_sparse_peer_storage(void) {
    g_test_name = __func__;
    ulnet_session_t session = {0};
    int status = 0;

    ulnet_session_init_defaulted(&session);
    if (!session.peer[SAM2_AUTHORITY_INDEX]) {
        SAM2_LOG_ERROR("sparse peer test: local peer was not allocated");
        return 1;
    }
    for (int p = 1; p < SAM2_TOTAL_PEERS; p++) {
        if (session.peer[p]) {
            SAM2_LOG_ERROR("sparse peer test: unused port %d was allocated", p);
            status = 1;
            goto done;
        }
    }

    ulnet__peer_alloc(&session, SAM2_TOTAL_PEERS - 1);
    if (!session.peer[SAM2_TOTAL_PEERS - 1] || session.peer[1]) {
        SAM2_LOG_ERROR("sparse peer test: high sparse port allocation failed");
        status = 1;
        goto done;
    }
    ulnet_peer_t *peer = session.peer[SAM2_TOTAL_PEERS - 1];
    peer->diagnostics = (ulnet_peer_diagnostics_t *)calloc(1, sizeof(*peer->diagnostics));
    if (!peer->diagnostics) {
        SAM2_LOG_ERROR("sparse peer test: diagnostics allocation failed");
        status = 1;
        goto done;
    }
    ulnet__peer_release(&session, SAM2_TOTAL_PEERS - 1);
    if (session.peer[SAM2_TOTAL_PEERS - 1]) {
        SAM2_LOG_ERROR("sparse peer test: released port remained allocated");
        status = 1;
        goto done;
    }

done:
    ulnet_session_tear_down(&session);
    return status;
}

#endif // ULNET_IMPLEMENTATION

#if defined(ULNET_TEST_MAIN)
#include <stdarg.h>
#include <sys/stat.h>

typedef struct ulnet__test_file_signal_context {
    const char *dir;
    const char *name;
} ulnet__test_file_signal_context_t;

static int64_t ulnet__test_now_usec(void) {
    return ulnet__get_unix_time_microseconds();
}

static void ulnet__test_signal_path(char *path, size_t size, const char *dir, const char *name) {
    snprintf(path, size, "%s/%s.signals", dir, name);
}

static int ulnet__test_append_text_file(const char *path, const char *text) {
    FILE *file = fopen(path, "ab");
    if (!file) {
        return -1;
    }
    fputs(text, file);
    fputc('\n', file);
    fclose(file);
    return 0;
}

static int ulnet__test_file_signal_send_callback(void *user_ptr, char *message) {
    g_test_name = __func__;
    ulnet__test_file_signal_context_t *ctx = (ulnet__test_file_signal_context_t *)user_ptr;
    sam2_signal_message_t *signal = (sam2_signal_message_t *)message;
    char path[1024];
    ulnet__test_signal_path(path, sizeof(path), ctx->dir, ctx->name);
    return ulnet__test_append_text_file(path, signal->ice_sdp);
}

static int ulnet__test_apply_remote_file_signals(ulnet_nat_agent_t *agent, const char *dir, const char *remote_name, long *offset) {
    g_test_name = __func__;
    char path[1024];
    char line[256];
    ulnet__test_signal_path(path, sizeof(path), dir, remote_name);

    FILE *file = fopen(path, "rb");
    if (!file) {
        return 0;
    }

    if (fseek(file, *offset, SEEK_SET) != 0) {
        fclose(file);
        *offset = 0;
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }
        if (line[0]) {
            ulnet__nat_process_signal(agent, line);
        }
    }

    *offset = ftell(file);
    fclose(file);
    return 0;
}

static int ulnet__test_nat_matrix_file_peer(const char *stun_host, int stun_port, const char *dir,
    const char *name, const char *remote_name, int timeout_seconds) {
    g_test_name = __func__;
    mkdir(dir, 0777);

    ulnet__test_file_signal_context_t signal_ctx;
    signal_ctx.dir = dir;
    signal_ctx.name = name;

    ulnet_session_t session;
    memset(&session, 0, sizeof(session));
    ulnet_session_init_defaulted(&session);
    g_ulnet_transport_use_inproc = false; // This subcommand drives the real ICE-lite agent directly
    ulnet_set_stun_server(&session, stun_host, (uint16_t)stun_port);
    session.sam2_send_callback = ulnet__test_file_signal_send_callback;
    session.user_ptr = &signal_ctx;

    uint64_t remote_peer_id = strcmp(remote_name, "authority") == 0 ? 10002 : 10003;
    ulnet_startup_nat_for_peer(&session, remote_peer_id, 0, NULL);
    ulnet_nat_agent_t *agent = (ulnet_nat_agent_t *) session.peer[0]->transport;
    if (!agent) {
        return 1;
    }

    long remote_offset = 0;
    int64_t deadline = ulnet__test_now_usec() + (int64_t)timeout_seconds * 1000000;
    while (ulnet__test_now_usec() < deadline) {
        ulnet__test_apply_remote_file_signals(agent, dir, remote_name, &remote_offset);
        ulnet__nat_poll_agent(agent);

        ulnet_transport_state_t state = ulnet_nat_get_state(agent);
        if (state == ULNET_TRANSPORT_READY) {
            ulnet_disconnect_peer(&session, 0);
            return 0;
        }

        ulnet__sleep(1);
    }

    SAM2_LOG_ERROR("%s timed out at NAT state %s", name, ulnet_transport_state_to_string(ulnet_nat_get_state(agent)));
    ulnet_disconnect_peer(&session, 0);
    return 1;
}

static int ulnet__test_poll_client_messages(ulnet_session_t *session, sam2_socket_t socket) {
    g_test_name = __func__;
    for (;;) {
        sam2_message_u message;
        int status = sam2_client_poll(socket, &message);
        if (status < 0) {
            SAM2_LOG_ERROR("Error polling SAM2 client: %d", status);
            return status;
        }
        if (status == 0) {
            return 0;
        }

        status = ulnet_process_message(session, (const char *)&message);
        if (status < 0) {
            SAM2_LOG_ERROR("Error processing SAM2 message: %d", status);
            return status;
        }
    }
}

static int ulnet__test_connect_client(ulnet_session_t *session, sam2_socket_t *socket, const char *host, int port, int timeout_seconds) {
    g_test_name = __func__;
    int status = sam2_client_connect(socket, host, port);
    if (status) {
        SAM2_LOG_ERROR("Error while connecting to SAM2 server %s:%d", host, port);
        return status;
    }

    int64_t deadline = ulnet__get_unix_time_microseconds() + (int64_t)timeout_seconds * 1000000;
    while (ulnet__get_unix_time_microseconds() < deadline) {
        sam2_client_poll_connection(*socket, 10);
        status = ulnet__test_poll_client_messages(session, *socket);
        if (status < 0) {
            return status;
        }
        if (session->our_peer_id) {
            return 0;
        }
        ulnet__sleep(1);
    }

    SAM2_LOG_ERROR("Timed out waiting for SAM2 peer id");
    return 1;
}

static int ulnet__test_nat_matrix_server(int port, int timeout_seconds) {
    g_test_name = __func__;
    sam2_server_t *server = (sam2_server_t *)calloc(1, sizeof(*server));
    if (!server) {
        return 1;
    }

    int status = sam2_server_init(server, port);
    if (status) {
        SAM2_LOG_ERROR("Error while initializing SAM2 server");
        free(server);
        return status;
    }

    int64_t deadline = ulnet__get_unix_time_microseconds() + (int64_t)timeout_seconds * 1000000;
    while (ulnet__get_unix_time_microseconds() < deadline) {
        status = sam2_server_poll(server);
        if (status < 0) {
            SAM2_LOG_ERROR("Error polling SAM2 server: %d", status);
            break;
        }
        ulnet__sleep(1);
    }

    sam2_server_destroy(server);
    free(server);
    return status < 0 ? status : 0;
}

static int ulnet__test_nat_matrix_authority(const char *host, int port, const char *ready_path, int timeout_seconds) {
    g_test_name = __func__;
    ulnet_session_t session;
    sam2_socket_t socket = SAM2_SOCKET_INVALID;
    memset(&session, 0, sizeof(session));
    ulnet_session_init_defaulted(&session);
    ulnet_set_stun_server(&session, host, (uint16_t)port);
    session.reliable_retransmit_delay_microseconds = 0;
    session.sam2_send_callback = ulnet__test_sam2_send_callback;
    session.user_ptr = &socket;
    session.retro_run = ulnet__test_retro_run;
    session.retro_serialize_size = ulnet__test_retro_serialize_size;
    session.retro_serialize = ulnet__test_retro_serialize;
    session.retro_unserialize = ulnet__test_retro_unserialize;

    int status = ulnet__test_connect_client(&session, &socket, host, port, timeout_seconds);
    if (status) {
        return status;
    }

    sam2_room_make_message_t request = { SAM2_MAKE_HEADER };
    request.room.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    request.room.peer_ids[SAM2_AUTHORITY_INDEX] = session.our_peer_id;
    request.room.peer_topology = (1ULL << SAM2_AUTHORITY_INDEX);
    status = sam2_client_send(socket, (char *)&request);
    if (status) {
        SAM2_LOG_ERROR("Failed to send MAKE message");
        return status;
    }

    if (ready_path && ready_path[0]) {
        FILE *ready_file = fopen(ready_path, "w");
        if (ready_file) {
            fprintf(ready_file, "%u\n", (unsigned)session.our_peer_id);
            fclose(ready_file);
        }
    }

    int saw_connected_peer = 0;
    int64_t deadline = ulnet__get_unix_time_microseconds() + (int64_t)timeout_seconds * 1000000;
    while (ulnet__get_unix_time_microseconds() < deadline) {
        status = ulnet_poll_session(&session, 0, 0, 0, 60.0, 5e-3);
        if (status < 0) {
            break;
        }

        status = ulnet__test_poll_client_messages(&session, socket);
        if (status < 0) {
            break;
        }

        for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
            if (ulnet_transport_state(session.peer[p]->transport) == ULNET_TRANSPORT_READY) {
                saw_connected_peer = 1;
            }
        }
        ulnet__sleep(1);
    }

    ulnet_session_tear_down(&session);
    return status < 0 ? status : (saw_connected_peer ? 0 : 1);
}

static int ulnet__test_nat_matrix_spectator(const char *host, int port, uint16_t authority_peer_id, int timeout_seconds) {
    g_test_name = __func__;
    ulnet_session_t session;
    sam2_socket_t socket = SAM2_SOCKET_INVALID;
    memset(&session, 0, sizeof(session));
    ulnet_session_init_defaulted(&session);
    ulnet_set_stun_server(&session, host, (uint16_t)port);
    session.reliable_retransmit_delay_microseconds = 0;
    session.sam2_send_callback = ulnet__test_sam2_send_callback;
    session.user_ptr = &socket;
    session.retro_run = ulnet__test_retro_run;
    session.retro_serialize_size = ulnet__test_retro_serialize_size;
    session.retro_serialize = ulnet__test_retro_serialize;
    session.retro_unserialize = ulnet__test_retro_unserialize;

    int status = ulnet__test_connect_client(&session, &socket, host, port, timeout_seconds);
    if (status) {
        return status;
    }

    session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = authority_peer_id;
    session.frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    ulnet_startup_nat_for_peer(&session, authority_peer_id, SAM2_AUTHORITY_INDEX, NULL);

    int64_t deadline = ulnet__get_unix_time_microseconds() + (int64_t)timeout_seconds * 1000000;
    while (ulnet__get_unix_time_microseconds() < deadline) {
        status = ulnet_poll_session(&session, 0, 0, 0, 60.0, 5e-3);
        if (status < 0) {
            break;
        }

        status = ulnet__test_poll_client_messages(&session, socket);
        if (status < 0) {
            break;
        }

        if (session.frame_counter != ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) {
            ulnet_session_tear_down(&session);
            return 0;
        }
        ulnet__sleep(1);
    }

    ulnet_session_tear_down(&session);
    SAM2_LOG_ERROR("Timed out waiting for spectator savestate sync");
    return 1;
}

void sam2_log_write(int level, const char *file, int line, const char *format, ...) {
    if (level == 2) {
        printf("WARN %s:%d | %s | ", file, line, g_test_name);
    } else if (level > 2) {
        printf("ERROR %s:%d | %s | ", file, line, g_test_name);
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    if (level == 4) {
        printf("Fatal error in %s:%d\n", file, line);
        abort();
    }
}

int main (int argc, char **argv) {
    ulnet_session_t *session_1 = NULL;
    ulnet_session_t *session_2 = NULL;
    bool inproc_only = argc > 1 && strcmp(argv[1], "--inproc-only") == 0;

    if (argc > 1 && strcmp(argv[1], "--nat-matrix-server") == 0) {
        int port = argc > 2 ? atoi(argv[2]) : ULNET__TEST_SAM2_PORT;
        int timeout_seconds = argc > 3 ? atoi(argv[3]) : 30;
        return ulnet__test_nat_matrix_server(port, timeout_seconds);
    }

    if (argc > 1 && strcmp(argv[1], "--nat-matrix-authority") == 0) {
        if (argc < 6) {
            fprintf(stderr, "usage: %s --nat-matrix-authority <host> <port> <ready-file> <timeout-seconds>\n", argv[0]);
            return 2;
        }
        return ulnet__test_nat_matrix_authority(argv[2], atoi(argv[3]), argv[4], atoi(argv[5]));
    }

    if (argc > 1 && strcmp(argv[1], "--nat-matrix-spectator") == 0) {
        if (argc < 6) {
            fprintf(stderr, "usage: %s --nat-matrix-spectator <host> <port> <authority-peer-id> <timeout-seconds>\n", argv[0]);
            return 2;
        }
        return ulnet__test_nat_matrix_spectator(argv[2], atoi(argv[3]), (uint16_t)atoi(argv[4]), atoi(argv[5]));
    }

    if (argc > 1 && strcmp(argv[1], "--nat-matrix-peer-file") == 0) {
        if (argc < 8) {
            fprintf(stderr, "usage: %s --nat-matrix-peer-file <stun-host> <stun-port> <signal-dir> <name> <remote-name> <timeout-seconds>\n", argv[0]);
            return 2;
        }
        return ulnet__test_nat_matrix_file_peer(argv[2], atoi(argv[3]), argv[4], argv[5], argv[6], atoi(argv[7]));
    }

    int status = ulnet_test_inproc(NULL, NULL);
    if (status != 0) {
        printf("Inproc test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_zstd_codec();
    if (status != 0) {
        printf("Zstd codec test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_inproc_reliable_ack_unblocks_queue();
    if (status != 0) {
        printf("Inproc reliable reorder/retransmit test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_packed_state_round_trip();
    if (status != 0) {
        printf("Packed state round trip test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_packed_spectator_round_trip();
    if (status != 0) {
        printf("Packed spectator round trip test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_packed_spectator_ack_only_delivery();
    if (status != 0) {
        printf("Packed spectator ACK-only delivery test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_packed_state_validation();
    if (status != 0) {
        printf("Packed state validation test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_sparse_peer_storage();
    if (status != 0) {
        printf("Sparse peer storage test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_inproc_savestate_retry_failure_goes_solo();
    if (status != 0) {
        printf("Inproc savestate retry/failure test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_inproc_multigroup_savestate();
    if (status != 0) {
        printf("Inproc multigroup savestate test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_inproc_room_switch_fuzz();
    if (status != 0) {
        printf("Inproc room switch fuzz test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_reliable_rejects_bad_sequence_state();
    if (status != 0) {
        printf("Reliable bad sequence state test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_no_slot_swap();
    if (status != 0) {
        printf("No-slot-swap reconstruct test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_solo_connect_is_active_player();
    if (status != 0) {
        printf("Solo connect active-player test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_room_change_scheduling_guards();
    if (status != 0) {
        printf("Room change scheduling guard test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_inproc_promote_spectator();
    if (status != 0) {
        printf("Promote spectator test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_inproc_coordinator_only_authority();
    if (status != 0) {
        printf("Coordinator-only authority test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_inproc_coordinator_two_player_mesh();
    if (status != 0) {
        printf("Coordinator-only two-player mesh test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_inproc_high_port_authority_relay();
    if (status != 0) {
        printf("High-port authority relay test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_inproc_spectator_recovers_after_state_burst_loss();
    if (status != 0) {
        printf("Spectator burst-loss recovery test failed with status: %d\n", status);
        return status;
    }

    if (inproc_only) {
        printf("Inproc tests passed successfully!\n");
        return 0;
    }

    status = ulnet_test_ice(&session_1, &session_2);
    //ulnet_session_tear_down(session_1);
    //ulnet_session_tear_down(session_2);
    free(session_1);
    free(session_2);
    session_1 = NULL;
    session_2 = NULL;
    if (status != 0) {
        printf("ICE test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_ice_promote_spectator();
    if (status != 0) {
        printf("ICE promote spectator test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_ice_churn();
    if (status != 0) {
        printf("ICE churn (join/leave/rejoin) test failed with status: %d\n", status);
        return status;
    }

    uint8_t numbers_one_to_thirty[30];
    for (int i = 0; i < 30; i++) {
        numbers_one_to_thirty[i] = i + 1;
    }

    if (ulnet_xxh32(numbers_one_to_thirty, sizeof(numbers_one_to_thirty), 0) != 0xa4b09c4b) {
        printf("XXH32 hash test failed\n");
        return 1;
    }

    printf("All tests passed successfully!\n");
    return 0;
}
#endif
