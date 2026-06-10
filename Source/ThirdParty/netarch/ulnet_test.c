#define SAM2_ENABLE_LOGGING
#include "ulnet.h"
#include "sam2.h"
#include "miniz.h"

#define ULNET__TEST_SAM2_PORT (SAM2_SERVER_DEFAULT_PORT + 1)

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

#if defined(ULNET_IMPLEMENTATION)
static int ulnet_test_swap_agent_moves_peer_state(void) {
    ulnet_session_t session;
    memset(&session, 0, sizeof(session));
    ulnet_session_init_defaulted(&session);
    session.our_peer_id = 10001;
    session.room_we_are_in.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session.our_peer_id;
    session.room_we_are_in.peer_ids[8] = 30002;
    session.room_we_are_in.peer_ids[9] = 30003;
    session.sam2_send_callback = ulnet__test_discard_send_callback;

    session.agent[9] = ulnet__nat_create(&session, 9);
    if (!session.agent[9]) {
        SAM2_LOG_ERROR("Unable to create test NAT agent");
        return 1;
    }

    session.agent_peer_ids[8] = 30002;
    session.agent_peer_ids[9] = 30003;
    session.peer_needs_sync_bitfield = 1ULL << 9;
    session.peer_pending_disconnect_bitfield = 1ULL << 8;
    session.peer_desynced_frame[9] = 1234;
    session.packet_history_next[9] = 77;
    session.reliable_last_transmit_time[9] = 5678;
    session.reliable_tx_next_seq[9] = 9;
    session.reliable_tx_head[9] = 8;
    session.reliable_rx_head[9] = 7;

    ulnet_swap_agent(&session, 8, 9);

    int failed = 0;
    failed |= session.agent[8] == NULL;
    failed |= session.agent[8] && session.agent[8]->peer_port != 8;
    failed |= session.agent[9] != NULL;
    failed |= (session.peer_needs_sync_bitfield & (1ULL << 8)) == 0;
    failed |= (session.peer_needs_sync_bitfield & (1ULL << 9)) != 0;
    failed |= (session.peer_pending_disconnect_bitfield & (1ULL << 9)) == 0;
    failed |= session.agent_peer_ids[8] != 30003;
    failed |= session.agent_peer_ids[9] != 30002;
    failed |= session.peer_desynced_frame[8] != 1234;
    failed |= session.packet_history_next[8] != 77;
    failed |= session.reliable_last_transmit_time[8] != 5678;
    failed |= session.reliable_tx_next_seq[8] != 9;
    failed |= session.reliable_tx_head[8] != 8;
    failed |= session.reliable_rx_head[8] != 7;

    ulnet_disconnect_peer(&session, 8);
    if (failed) {
        SAM2_LOG_ERROR("ulnet_swap_agent did not move all peer state");
        return 1;
    }

    return 0;
}
#endif

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
    room.peer_ids[SAM2_SPECTATOR_START] = sessions[1]->our_peer_id;

    sessions[0]->room_we_are_in = room;
    sessions[1]->room_we_are_in = room;

    sessions[0]->inproc[SAM2_SPECTATOR_START] = transport;
    sessions[1]->inproc[SAM2_AUTHORITY_INDEX] = transport;
    sessions[0]->agent_peer_ids[SAM2_SPECTATOR_START] = room.peer_ids[SAM2_SPECTATOR_START];
    sessions[1]->agent_peer_ids[SAM2_AUTHORITY_INDEX] = room.peer_ids[SAM2_AUTHORITY_INDEX];

    sessions[1]->frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    sessions[0]->peer_needs_sync_bitfield |= (1ULL << SAM2_SPECTATOR_START);
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
    room.peer_ids[SAM2_SPECTATOR_START] = 30002;

    // @todo The behavior right now sucks if you don't first make the room before having the person try to join it. It should just reply with a reasonable error
    // Have session 0 make the room
    sam2_room_make_message_t request = { SAM2_MAKE_HEADER };
    request.room = room;
    request.room.flags |= SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    sam2_client_send(sockets[0], (char *)&request);

    // Have session 1 join the room
    sessions[1]->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = sessions[0]->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX];
    sessions[1]->frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    ulnet_startup_ice_for_peer(sessions[1], sessions[0]->our_peer_id, SAM2_AUTHORITY_INDEX, NULL);

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
            SAM2_LOCATE(sessions[0]->agent_peer_ids, sessions[1]->our_peer_id, spectator_port);
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
    SAM2_LOCATE(sessions[0]->agent_peer_ids, sessions[1]->our_peer_id, spectator_port);
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
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3); // RETRANSMIT "HELLO"
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3); // RECEIVE "HELLO"
    ulnet_reliable_send_with_acks_only(sessions[0], spectator_port, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1);
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3); // RETRANSMIT "WORLD"
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3); // RECEIVE "WORLD"

#if 0
    ulnet_reliable_send_with_acks_only(sessions[0], SAM2_SPECTATOR_START, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1); // ACK "WORLD"
    for (int i = 0; i < 5; i++) {
        ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3);
        ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);
    }
#endif
    ulnet_reliable_packet_t *msg1 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[spectator_port][0].data;
    ulnet_reliable_packet_t *msg2 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[spectator_port][1].data;

    if (!(   msg1 && memcmp(msg1->payload, "HELLO", sizeof("HELLO") - 1) == 0
          && msg2 && memcmp(msg2->payload, "WORLD", sizeof("WORLD") - 1) == 0)) {
        SAM2_LOG_ERROR("Failed to send reliable messages");
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
    ulnet_reliable_send_with_acks_only(sessions[0], SAM2_SPECTATOR_START, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1); // ACK "HELLO"
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3); // RETRANSMIT "WORLD"
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3); // RECEIVE "WORLD"

    ulnet_reliable_packet_t *msg1 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[SAM2_SPECTATOR_START][0].data;
    ulnet_reliable_packet_t *msg2 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[SAM2_SPECTATOR_START][1].data;

    if (!(   msg1 && memcmp(msg1->payload, "HELLO", sizeof("HELLO") - 1) == 0
          && msg2 && memcmp(msg2->payload, "WORLD", sizeof("WORLD") - 1) == 0)) {
        SAM2_LOG_ERROR("Failed to send reliable messages");
        status = 1;
    }

    sessions[0]->inproc[SAM2_SPECTATOR_START] = NULL;
    sessions[1]->inproc[SAM2_AUTHORITY_INDEX] = NULL;
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
    ulnet_reliable_send_with_acks_only(sessions[0], SAM2_SPECTATOR_START, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1);
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3);
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);

    ulnet_reliable_packet_t *msg1 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[SAM2_SPECTATOR_START][0].data;
    ulnet_reliable_packet_t *msg2 = (ulnet_reliable_packet_t *) sessions[0]->reliable_rx_packet_history[SAM2_SPECTATOR_START][1].data;

    if (!(   msg1 && memcmp(msg1->payload, "HELLO", sizeof("HELLO") - 1) == 0
          && msg2 && memcmp(msg2->payload, "WORLD", sizeof("WORLD") - 1) == 0)) {
        SAM2_LOG_ERROR("Reliable ACK did not immediately unblock the queued packet");
        status = 1;
    }

    sessions[0]->inproc[SAM2_SPECTATOR_START] = NULL;
    sessions[1]->inproc[SAM2_AUTHORITY_INDEX] = NULL;
    ulnet_session_tear_down(sessions[0]);
    ulnet_session_tear_down(sessions[1]);
    free(sessions[0]);
    free(sessions[1]);

    return status;
}

#if defined(ULNET_IMPLEMENTATION)
static void ulnet__test_fill_deflate_buffer(uint8_t *data, size_t size, int pattern) {
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

int ulnet_test_deflate_codec(void) {
    const size_t test_sizes[] = {0, 1, 2, 3, 31, 4096, 65536};
    uint8_t empty_sentinel = 0;

    for (int pattern = 0; pattern < 4; pattern++) {
        for (size_t i = 0; i < sizeof(test_sizes) / sizeof(test_sizes[0]); i++) {
            size_t size = test_sizes[i];
            uint8_t *original = size ? (uint8_t *)malloc(size) : &empty_sentinel;
            uint8_t *decoded = size ? (uint8_t *)malloc(size) : &empty_sentinel;
            size_t compressed_capacity = ULNET_DEFLATE_COMPRESS_BOUND(size);
            uint8_t *compressed = (uint8_t *)malloc(compressed_capacity ? compressed_capacity : 1);

            if (!original || !decoded || !compressed) {
                SAM2_LOG_ERROR("Failed to allocate deflate test buffers");
                if (size && original) free(original);
                if (size && decoded) free(decoded);
                if (compressed) free(compressed);
                return 1;
            }

            ulnet__test_fill_deflate_buffer(original, size, pattern);

            int64_t compressed_size = ULNET_DEFLATE_COMPRESS(compressed, compressed_capacity, original, size, 8);
            if (compressed_size < 0) {
                SAM2_LOG_ERROR("ULNET_DEFLATE_COMPRESS failed for size %zu pattern %d", size, pattern);
                if (size) free(original);
                if (size) free(decoded);
                free(compressed);
                return 1;
            }

            int64_t decoded_size = ULNET_DEFLATE_DECOMPRESS(decoded, size, compressed, compressed_size);
            if (decoded_size != (int64_t)size || memcmp(original, decoded, size) != 0) {
                SAM2_LOG_ERROR("ULNET_DEFLATE_DECOMPRESS round trip failed for size %zu pattern %d", size, pattern);
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

int ulnet_test_deflate_decodes_miniz(void) {
    const int miniz_levels[] = {MZ_BEST_SPEED, MZ_DEFAULT_LEVEL, MZ_BEST_COMPRESSION};
    const size_t test_sizes[] = {0, 1, 257, 32768, 98304};
    uint8_t empty_sentinel = 0;

    for (size_t level_idx = 0; level_idx < sizeof(miniz_levels) / sizeof(miniz_levels[0]); level_idx++) {
        for (size_t size_idx = 0; size_idx < sizeof(test_sizes) / sizeof(test_sizes[0]); size_idx++) {
            size_t size = test_sizes[size_idx];
            uint8_t *original = size ? (uint8_t *)malloc(size) : &empty_sentinel;
            uint8_t *decoded = size ? (uint8_t *)malloc(size) : &empty_sentinel;
            mz_ulong compressed_capacity = mz_compressBound((mz_ulong)size);
            uint8_t *compressed = (uint8_t *)malloc(compressed_capacity ? compressed_capacity : 1);

            if (!original || !decoded || !compressed) {
                SAM2_LOG_ERROR("Failed to allocate miniz compatibility test buffers");
                if (size && original) free(original);
                if (size && decoded) free(decoded);
                if (compressed) free(compressed);
                return 1;
            }

            ulnet__test_fill_deflate_buffer(original, size, (int)(level_idx + size_idx) % 4);

            mz_ulong compressed_size = compressed_capacity;
            int miniz_status = mz_compress2(compressed, &compressed_size, original, (mz_ulong)size, miniz_levels[level_idx]);
            if (miniz_status != MZ_OK) {
                SAM2_LOG_ERROR("mz_compress2 failed with status %d", miniz_status);
                if (size) free(original);
                if (size) free(decoded);
                free(compressed);
                return 1;
            }

            int64_t decoded_size = ULNET_DEFLATE_DECOMPRESS(decoded, size, compressed, compressed_size);
            if (decoded_size != (int64_t)size || memcmp(original, decoded, size) != 0) {
                SAM2_LOG_ERROR("ULNET_DEFLATE_DECOMPRESS failed to decode miniz data for size %zu level %d",
                    size, miniz_levels[level_idx]);
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

#endif

void ulnet__bench_xxh32() {
    const size_t test_size = 64 * 1024 * 1024;
    const int iterations = 30;

    uint8_t* test_data = malloc(test_size);
    if (!test_data) {
        printf("Failed to allocate test buffer\n");
        return;
    }

    for (size_t i = 0; i < test_size; i++) {
        test_data[i] = (uint8_t)(i * 0x9E3779B1);
    }

    // Warm up
    volatile uint32_t dummy = 0;
    for (int i = 0; i < 5; i++) {
        dummy ^= ulnet_xxh32(test_data, test_size, 0);
    }

    uint64_t start_unix_us = ulnet__get_unix_time_microseconds();
    uint32_t result = 0;

    for (int i = 0; i < iterations; i++) {
        test_data[0] = (uint8_t)i; // Prevent compiler optimization
        result ^= ulnet_xxh32(test_data, test_size, 0);
    }

    double total_bytes = (double)test_size * iterations;
    uint64_t elapsed_us = ulnet__get_unix_time_microseconds() - start_unix_us;

    double elapsed_seconds = elapsed_us / 1e6;

    double bytes_per_second = total_bytes / elapsed_seconds;
    double gigabytes_per_second = bytes_per_second / (1024.0 * 1024.0 * 1024.0);

    printf("xxh32 Throughput: %.3f GB/s\n", gigabytes_per_second);

    free(test_data);
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
    ulnet_startup_ice_for_peer(&session, authority_peer_id, SAM2_AUTHORITY_INDEX, NULL);

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

#if defined(ULNET_IMPLEMENTATION)
    status = ulnet_test_deflate_codec();
    if (status != 0) {
        printf("Deflate codec test failed with status: %d\n", status);
        return status;
    }

    status = ulnet_test_deflate_decodes_miniz();
    if (status != 0) {
        printf("Deflate miniz compatibility test failed with status: %d\n", status);
        return status;
    }
#endif

    status = ulnet_test_inproc_reliable_ack_unblocks_queue();
    if (status != 0) {
        printf("Inproc reliable ACK unblock test failed with status: %d\n", status);
        return status;
    }

#if defined(ULNET_IMPLEMENTATION)
    status = ulnet_test_swap_agent_moves_peer_state();
    if (status != 0) {
        printf("Agent swap peer-state test failed with status: %d\n", status);
        return status;
    }
#endif

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

    uint8_t numbers_one_to_thirty[30];
    for (int i = 0; i < 30; i++) {
        numbers_one_to_thirty[i] = i + 1;
    }

    if (ulnet_xxh32(numbers_one_to_thirty, sizeof(numbers_one_to_thirty), 0) != 0xa4b09c4b) {
        printf("XXH32 hash test failed\n");
        return 1;
    }

    ulnet__bench_xxh32();

    printf("All tests passed successfully!\n");
    return 0;
}
#endif
