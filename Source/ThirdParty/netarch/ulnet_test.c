#define SAM2_ENABLE_LOGGING
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

int ulnet__test_forward_messages(sam2_server_t *server, ulnet_session_t *session, sam2_socket_t socket) {
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

int ulnet__test_discard_send_callback(void *user_ptr, char *message) {
    (void)user_ptr;
    (void)message;
    return 0;
}

// A port is a fixed peer identity: promoting the same peer in place keeps its agent and per-port
// state, while a change of occupant fully reconstructs the agent (it is never swapped or reused).
static int ulnet_test_no_slot_swap(void) {
    ulnet_session_t session;
    memset(&session, 0, sizeof(session));
    ulnet_session_init_defaulted(&session);
    session.our_peer_id = 10001;
    session.sam2_send_callback = ulnet__test_discard_send_callback;
    session.room_we_are_in.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session.our_peer_id;

    const int slot = 3; // A low port so it is eligible to become a p2p player
    session.room_we_are_in.peer_ids[slot] = 30002; // present as a spectator (topology bit clear)
    session.agent[slot] = ulnet__nat_create(&session, slot);
    if (!session.agent[slot]) {
        SAM2_LOG_ERROR("Unable to create test NAT agent");
        return 1;
    }
    session.reliable_rx_head[slot] = 0x1234;
    ulnet_nat_agent_t *agent_before = session.agent[slot];

    int failed = 0;

    // Promotion in place: same peer gains the topology bit -> agent and per-port state are preserved
    sam2_room_t promoted = session.room_we_are_in;
    promoted.peer_topology |= (1ULL << slot);
    ulnet__reconcile_connections(&session, &promoted);
    failed |= session.agent[slot] != agent_before;
    failed |= !(session.room_we_are_in.peer_topology & (1ULL << slot));
    failed |= session.reliable_rx_head[slot] != 0x1234;
    if (failed) {
        SAM2_LOG_ERROR("Promotion in place must not reconstruct the agent or reset per-port state");
        if (session.agent[slot]) ulnet_disconnect_peer(&session, slot);
        return 1;
    }

    // Occupant change: a different peer takes the slot -> the old agent is fully torn down and rebuilt
    sam2_room_t replaced = session.room_we_are_in;
    replaced.peer_ids[slot] = 30003;
    ulnet__reconcile_connections(&session, &replaced);
    failed |= session.room_we_are_in.peer_ids[slot] != 30003;
    failed |= session.reliable_rx_head[slot] != 0; // proves disconnect_peer ran (full reconstruct)

    if (session.agent[slot]) ulnet_disconnect_peer(&session, slot);
    if (failed) {
        SAM2_LOG_ERROR("Changing a port's occupant must fully reconstruct, not reuse, the agent");
        return 1;
    }

    return 0;
}

static void ulnet__test_inproc_pair_setup(ulnet_session_t *sessions[2],
    ulnet_transport_inproc_t *transport, int64_t retransmit_delay_microseconds) {
    for (int i = 0; i < 2; i++) {
        sessions[i] = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(sessions[i]);
        sessions[i]->reliable_retransmit_delay_microseconds = retransmit_delay_microseconds;
        sessions[i]->use_inproc_transport = true;
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
    sessions[0]->inproc[ULNET__TEST_SPECTATOR_PORT] = transport;
    sessions[1]->inproc[SAM2_AUTHORITY_INDEX] = transport;

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

int ulnet_test_ice(ulnet_session_t **session_1_out, ulnet_session_t **session_2_out) {
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
            && sessions[0]->agent[spectator_port]
            && ulnet_nat_get_state(sessions[0]->agent[spectator_port]) == ULNET_NAT_STATE_READY) {
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

        msg1 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[spectator_port][0].data;
        msg2 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[spectator_port][1].data;
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
            sessions[1]->reliable_tx_head[SAM2_AUTHORITY_INDEX],
            sessions[1]->reliable_tx_next_seq[SAM2_AUTHORITY_INDEX],
            sessions[0]->reliable_rx_head[spectator_port],
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
    struct { const char *name; long long value; } fields[] = {
        { "room peer_id",        s->room_we_are_in.peer_ids[slot] != SAM2_PORT_AVAILABLE },
        { "next_room peer_id",   s->next_room.peer_ids[slot]       != SAM2_PORT_AVAILABLE },
        { "topology bit",        (s->room_we_are_in.peer_topology >> slot) & 1ULL },
        { "agent",               s->agent[slot] != NULL },
        { "reliable_rx_head",    s->reliable_rx_head[slot] },
        { "reliable_tx_head",    s->reliable_tx_head[slot] },
        { "reliable_tx_next_seq",s->reliable_tx_next_seq[slot] },
        { "packet_history_next", s->packet_history_next[slot] },
        { "reliable_last_tx",    s->reliable_last_transmit_time[slot] },
        { "peer_desynced_frame", s->peer_desynced_frame[slot] },
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
    ulnet_session_t *authority = sessions[authority_idx];
    ulnet_session_t *joiner = sessions[joiner_idx];

    joiner->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = authority->our_peer_id;
    joiner->frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    ulnet_startup_nat_for_peer(joiner, authority->our_peer_id, SAM2_AUTHORITY_INDEX, NULL);

    for (int64_t t0 = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - t0 < 5000000;) {
        int status = ulnet__test_ice_pump(server, sessions, sockets, count);
        if (status < 0) return status;

        int slot = sam2_get_port_of_peer(&authority->room_we_are_in, joiner->our_peer_id);
        if (   slot != -1
            && authority->agent[slot]
            && ulnet_nat_get_state(authority->agent[slot]) == ULNET_NAT_STATE_READY
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
    if (sessions[A]->next_room_effective_frame % ULNET_DELAY_BUFFER_SIZE != 0) {
        SAM2_LOG_ERROR("Real-ICE coordinator toggle was not scheduled on a block boundary");
        status = 1;
        goto done;
    }

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
        coordinator_ticks =    sessions[B]->frame_counter > b_start_frame + ULNET_DELAY_BUFFER_SIZE
                            && sessions[A]->state[b_slot].frame + 1 >= sessions[A]->frame_counter
                            && sessions[B]->state[SAM2_AUTHORITY_INDEX].frame >= 7;
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

    for (int64_t t0 = ulnet__get_unix_time_microseconds(); ulnet__get_unix_time_microseconds() - t0 < 5000000;) {
        int status = ulnet__test_ice_pump(server, sessions, sockets, count);
        if (status < 0) return status;

        if (   sam2_get_port_of_peer(&authority->room_we_are_in, leaver_id) == -1
            && (slot == -1 || authority->agent[slot] == NULL)) {
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
    void *b_agent = (void *) sessions[A]->agent[b_slot];

    int c_slot_first = -1;
    status = ulnet__test_ice_join(server, sessions, sockets, SESSION_COUNT, A, C, &c_slot_first);
    if (status) goto done;
    if (c_slot_first == b_slot) {
        SAM2_LOG_ERROR("Two peers ended up on the same slot %d", c_slot_first);
        status = 1;
        goto done;
    }
    if (sam2_get_port_of_peer(&sessions[A]->room_we_are_in, sessions[B]->our_peer_id) != b_slot
        || (void *) sessions[A]->agent[b_slot] != b_agent) {
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
        || (void *) sessions[A]->agent[b_slot] != b_agent) {
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
        || (void *) sessions[A]->agent[b_slot] != b_agent) {
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
        (int)((sessions[1]->state[SAM2_AUTHORITY_INDEX].room.peer_topology >> spec_slot) & 1),
        sessions[1]->state[SAM2_AUTHORITY_INDEX].room_effective_frame,
        (int)((sessions[1]->room_we_are_in.peer_topology >> spec_slot) & 1),
        (int)((sessions[0]->room_we_are_in.peer_topology >> spec_slot) & 1));

    if (!promoted_auth) { SAM2_LOG_ERROR("promote test: authority never applied the promotion"); status = 1; }
    if (!promoted_spec) { SAM2_LOG_ERROR("promote test: spectator never observed its own promotion"); status = 1; }
    if (status == 0) SAM2_LOG_INFO("promote test: spectator successfully became a player");

done:
    ulnet_session_tear_down(sessions[0]);
    ulnet_session_tear_down(sessions[1]);
    free(sessions[0]);
    free(sessions[1]);
    return status;
}

int ulnet_test_inproc_coordinator_only_authority(void) {
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
    if (scheduled_out_frame % ULNET_DELAY_BUFFER_SIZE != 0) {
        SAM2_LOG_ERROR("coordinator test: authority leave-mesh frame is not block aligned");
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
    if (sessions[1]->frame_counter <= player_start_frame + ULNET_DELAY_BUFFER_SIZE) {
        SAM2_LOG_ERROR("coordinator test: zero-delay player did not advance with coordinator heartbeat");
        status = 1;
        goto done;
    }
    if (sessions[0]->state[player_slot].frame + 1 < sessions[0]->frame_counter) {
        SAM2_LOG_ERROR("coordinator test: authority did not receive remote player input");
        status = 1;
        goto done;
    }

    int64_t authority_snapshot_frame = sessions[1]->state[SAM2_AUTHORITY_INDEX].frame;
    int64_t satisfied_block_start = ((authority_snapshot_frame + 1) / ULNET_DELAY_BUFFER_SIZE) * ULNET_DELAY_BUFFER_SIZE;
    int64_t blocked_boundary = satisfied_block_start + ULNET_DELAY_BUFFER_SIZE;
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
    if (blocked_frame != sessions[1]->frame_counter || blocked_frame > blocked_boundary) {
        SAM2_LOG_ERROR("coordinator test: player advanced into a block without authority snapshot");
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

    int64_t authority_state_before_join = sessions[0]->state[SAM2_AUTHORITY_INDEX].frame;
    if (ulnet__test_request_local_role_toggle(sessions[0], SAM2_AUTHORITY_INDEX) != 0) {
        status = 1;
        goto done;
    }
    int64_t scheduled_in_frame = sessions[0]->next_room_effective_frame;
    if (scheduled_in_frame % ULNET_DELAY_BUFFER_SIZE != 0) {
        SAM2_LOG_ERROR("coordinator test: authority join-mesh frame is not block aligned");
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
    if (sessions[0]->state[SAM2_AUTHORITY_INDEX].frame < authority_state_before_join) {
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
    enum { A = 0, B = 1, C = 2, SESSION_COUNT = 3 };
    ulnet_session_t *sessions[SESSION_COUNT] = {0};
    ulnet_transport_inproc_t transport_ab = {0};
    ulnet_transport_inproc_t transport_ac = {0};
    ulnet_transport_inproc_t transport_bc = {0};
    uint8_t save_state[256];
    int status = 0;

    for (int i = 0; i < SESSION_COUNT; i++) {
        sessions[i] = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(sessions[i]);
        sessions[i]->use_inproc_transport = true;
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

    sessions[A]->inproc[ULNET__TEST_PLAYER1_PORT] = &transport_ab;
    sessions[B]->inproc[SAM2_AUTHORITY_INDEX] = &transport_ab;
    sessions[A]->inproc[ULNET__TEST_PLAYER2_PORT] = &transport_ac;
    sessions[C]->inproc[SAM2_AUTHORITY_INDEX] = &transport_ac;
    sessions[B]->inproc[ULNET__TEST_PLAYER2_PORT] = &transport_bc;
    sessions[C]->inproc[ULNET__TEST_PLAYER1_PORT] = &transport_bc;

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
    if (   sessions[A]->state[ULNET__TEST_PLAYER1_PORT].frame + 1 < sessions[A]->frame_counter
        || sessions[A]->state[ULNET__TEST_PLAYER2_PORT].frame + 1 < sessions[A]->frame_counter) {
        SAM2_LOG_ERROR("two-player coordinator test: authority did not receive both players");
        status = 1;
        goto done;
    }
    if (   sessions[B]->state[ULNET__TEST_PLAYER2_PORT].frame + 1 < sessions[B]->frame_counter
        || sessions[C]->state[ULNET__TEST_PLAYER1_PORT].frame + 1 < sessions[C]->frame_counter) {
        SAM2_LOG_ERROR("two-player coordinator test: remote players did not exchange direct input");
        status = 1;
        goto done;
    }
    if (   sessions[B]->state[SAM2_AUTHORITY_INDEX].frame < 15
        || sessions[C]->state[SAM2_AUTHORITY_INDEX].frame < 15) {
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
    enum { A = 0, B = 1, C = 2, SESSION_COUNT = 3 };
    ulnet_session_t *sessions[SESSION_COUNT] = {0};
    ulnet_transport_inproc_t transport_ab = {0};
    ulnet_transport_inproc_t transport_ac = {0};
    uint8_t save_state[256];
    int status = 0;

    for (int i = 0; i < SESSION_COUNT; i++) {
        sessions[i] = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(sessions[i]);
        sessions[i]->use_inproc_transport = true;
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

    sessions[A]->inproc[ULNET__TEST_PLAYER2_PORT] = &transport_ab;
    sessions[B]->inproc[SAM2_AUTHORITY_INDEX] = &transport_ab;
    sessions[A]->inproc[ULNET__TEST_RELAY_SPECTATOR_PORT] = &transport_ac;
    sessions[C]->inproc[SAM2_AUTHORITY_INDEX] = &transport_ac;

    for (int i = 0; i < 120; i++) {
        ulnet__test_poll_inproc_sessions(sessions, SESSION_COUNT, save_state, sizeof(save_state));
    }

    if (!ulnet_port_is_p2p(&sessions[A]->room_we_are_in, ULNET__TEST_PLAYER2_PORT)) {
        SAM2_LOG_ERROR("high-port relay test: player high slot is not p2p");
        status = 1;
        goto done;
    }
    if (sessions[A]->state[ULNET__TEST_PLAYER2_PORT].frame < 16) {
        SAM2_LOG_ERROR("high-port relay test: authority did not decode high-port player input");
        status = 1;
        goto done;
    }
    if (sessions[C]->state[ULNET__TEST_PLAYER2_PORT].frame < 16) {
        SAM2_LOG_ERROR("high-port relay test: spectator did not receive relayed high-port player input");
        status = 1;
        goto done;
    }
    if (sessions[C]->agent[ULNET__TEST_PLAYER2_PORT]) {
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

int ulnet_test_inproc(ulnet_session_t **session_1_out, ulnet_session_t **session_2_out) {
    ulnet_session_t *sessions[2] = {0};
    ulnet_transport_inproc_t transport = {0};
    int status = 0;

    ulnet__test_inproc_pair_setup(sessions, &transport, 0);

    sessions[0]->debug_udp_recv_drop_rate = 1.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "HELLO", sizeof("HELLO") - 1); // DROP
    sessions[0]->debug_udp_recv_drop_rate = 0.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "WORLD", sizeof("WORLD") - 1); // (NOT SENT) added to reliable_tx_packet_history
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3); // RETRANSMIT "HELLO"
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3); // RECEIVE "HELLO"
    ulnet_reliable_send_with_acks_only(sessions[0], ULNET__TEST_SPECTATOR_PORT, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1); // ACK "HELLO"
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3); // RETRANSMIT "WORLD"
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3); // RECEIVE "WORLD"

    ulnet_reliable_packet_t *msg1 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[ULNET__TEST_SPECTATOR_PORT][0].data;
    ulnet_reliable_packet_t *msg2 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[ULNET__TEST_SPECTATOR_PORT][1].data;

    if (!(   msg1 && memcmp(msg1->payload, "HELLO", sizeof("HELLO") - 1) == 0
          && msg2 && memcmp(msg2->payload, "WORLD", sizeof("WORLD") - 1) == 0)) {
        SAM2_LOG_ERROR("Failed to send reliable messages");
        status = 1;
    }

    ulnet_session_tear_down(sessions[0]);
    ulnet_session_tear_down(sessions[1]);
    if (!session_1_out) free(sessions[0]);
    else *session_1_out = sessions[0];

    if (!session_2_out) free(sessions[1]);
    else *session_2_out = sessions[1];

    return status;
}

int ulnet_test_inproc_reliable_ack_unblocks_queue(void) {
    ulnet_session_t *sessions[2] = {0};
    ulnet_transport_inproc_t transport = {0};
    int status = 0;

    ulnet__test_inproc_pair_setup(sessions, &transport, 10 * 1000 * 1000);

    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "HELLO", sizeof("HELLO") - 1);
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "WORLD", sizeof("WORLD") - 1);

    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);
    ulnet_reliable_send_with_acks_only(sessions[0], ULNET__TEST_SPECTATOR_PORT, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1);
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3);
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);

    ulnet_reliable_packet_t *msg1 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[ULNET__TEST_SPECTATOR_PORT][0].data;
    ulnet_reliable_packet_t *msg2 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[ULNET__TEST_SPECTATOR_PORT][1].data;

    if (!(   msg1 && memcmp(msg1->payload, "HELLO", sizeof("HELLO") - 1) == 0
          && msg2 && memcmp(msg2->payload, "WORLD", sizeof("WORLD") - 1) == 0)) {
        SAM2_LOG_ERROR("Reliable ACK did not immediately unblock the queued packet");
        status = 1;
    }

    ulnet_session_tear_down(sessions[0]);
    ulnet_session_tear_down(sessions[1]);
    free(sessions[0]);
    free(sessions[1]);

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
    session.use_inproc_transport = true;
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
            session.inproc[slot] = &transports[slot];
            session.reliable_rx_head[slot] = (uint16_t)(0x1000 + slot);
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
                    || (void *)session.inproc[p] != expected_agent[p]
                    || session.reliable_rx_head[p] != expected_rx[p]) {
                    SAM2_LOG_ERROR("Slot %d state moved/changed unexpectedly at iteration %d (op %d)", p, iter, op);
                    return 1;
                }
            } else {
                if (session.room_we_are_in.peer_ids[p] != SAM2_PORT_AVAILABLE || session.agent[p] != NULL) {
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
    ulnet_session_t session = {0};
    uint8_t packet[ULNET_PACKET_SIZE_BYTES_MAX] = {0};
    uint8_t ack_packet[sizeof(ulnet_reliable_packet_t)] = {0};
    int status = 0;

    ulnet_session_init_defaulted(&session);

    if (ulnet_reliable_send(&session, SAM2_AUTHORITY_INDEX, packet, sizeof(packet)) >= 0) {
        SAM2_LOG_ERROR("Oversized reliable packet unexpectedly sent");
        status = 1;
    }

    if (session.reliable_tx_next_seq[SAM2_AUTHORITY_INDEX] != 0) {
        SAM2_LOG_ERROR("Oversized reliable packet consumed a sequence number");
        status = 1;
    }

    session.reliable_tx_next_seq[SAM2_AUTHORITY_INDEX] = 2;
    session.reliable_tx_head[SAM2_AUTHORITY_INDEX] = 0;
    ack_packet[0] = ULNET_CHANNEL_RELIABLE | ULNET_RELIABLE_FLAG_ACK_ONLY;
    ack_packet[3] = 3;

    ulnet__process_udp_packet(&session, SAM2_AUTHORITY_INDEX, ack_packet, sizeof(ack_packet));
    if (session.reliable_tx_head[SAM2_AUTHORITY_INDEX] != 0) {
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
    ulnet__test_file_signal_context_t *ctx = (ulnet__test_file_signal_context_t *)user_ptr;
    sam2_signal_message_t *signal = (sam2_signal_message_t *)message;
    char path[1024];
    ulnet__test_signal_path(path, sizeof(path), ctx->dir, ctx->name);
    return ulnet__test_append_text_file(path, signal->ice_sdp);
}

static int ulnet__test_apply_remote_file_signals(ulnet_nat_agent_t *agent, const char *dir, const char *remote_name, long *offset) {
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
    mkdir(dir, 0777);

    ulnet__test_file_signal_context_t signal_ctx;
    signal_ctx.dir = dir;
    signal_ctx.name = name;

    ulnet_session_t session;
    memset(&session, 0, sizeof(session));
    ulnet_session_init_defaulted(&session);
    ulnet_set_stun_server(&session, stun_host, (uint16_t)stun_port);
    session.sam2_send_callback = ulnet__test_file_signal_send_callback;
    session.user_ptr = &signal_ctx;

    uint64_t remote_peer_id = strcmp(remote_name, "authority") == 0 ? 10002 : 10003;
    ulnet_startup_nat_for_peer(&session, remote_peer_id, 0, NULL);
    if (!session.agent[0]) {
        return 1;
    }

    long remote_offset = 0;
    int64_t deadline = ulnet__test_now_usec() + (int64_t)timeout_seconds * 1000000;
    while (ulnet__test_now_usec() < deadline) {
        ulnet__test_apply_remote_file_signals(session.agent[0], dir, remote_name, &remote_offset);
        ulnet__nat_poll_agent(session.agent[0]);

        ulnet_nat_state_t state = ulnet_nat_get_state(session.agent[0]);
        if (state == ULNET_NAT_STATE_READY) {
            ulnet_disconnect_peer(&session, 0);
            return 0;
        }

        ulnet__sleep(1);
    }

    SAM2_LOG_ERROR("%s timed out at NAT state %s", name, ulnet_nat_state_to_string(ulnet_nat_get_state(session.agent[0])));
    ulnet_disconnect_peer(&session, 0);
    return 1;
}

static int ulnet__test_poll_client_messages(ulnet_session_t *session, sam2_socket_t socket) {
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
            if (ulnet_nat_get_state(session.agent[p]) == ULNET_NAT_STATE_READY) {
                saw_connected_peer = 1;
            }
        }
        ulnet__sleep(1);
    }

    ulnet_session_tear_down(&session);
    return status < 0 ? status : (saw_connected_peer ? 0 : 1);
}

static int ulnet__test_nat_matrix_spectator(const char *host, int port, uint16_t authority_peer_id, int timeout_seconds) {
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
        printf("WARN %s:%d | ", file, line);
    } else if (level > 2) {
        printf("ERROR %s:%d | ", file, line);
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
        printf("Inproc reliable ACK unblock test failed with status: %d\n", status);
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
