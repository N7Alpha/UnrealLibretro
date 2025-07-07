#define SAM2_ENABLE_LOGGING
#include "ulnet.h"
#include "sam2.h"
#include "juice/juice.h"

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

        if (   sessions[0]
            && sessions[0]->agent[SAM2_SPECTATOR_START]
            && juice_get_state(sessions[0]->agent[SAM2_SPECTATOR_START]) == JUICE_STATE_COMPLETED) {
            connection_established = 1;
            break;
        }
    }

    if (!connection_established) {
        SAM2_LOG_ERROR("Failed to establish connection within 2 seconds");
        status = 1;
        goto _10;
    }

    sessions[0]->debug_udp_recv_drop_rate = 1.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "HELLO", sizeof("HELLO") - 1); // DROP
    sessions[0]->debug_udp_recv_drop_rate = 0.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "WORLD", sizeof("WORLD") - 1); // (NOT SENT) ADDED TO OUTGOING BUFFER
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3); // RETRANSMIT "HELLO"
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3); // RECEIVE "HELLO"
    ulnet_reliable_send_with_acks_only(sessions[0], SAM2_SPECTATOR_START, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1);
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3); // RETRANSMIT "WORLD"
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3); // RECEIVE "WORLD"

#if 0
    ulnet_reliable_send_with_acks_only(sessions[0], SAM2_SPECTATOR_START, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1); // ACK "WORLD"
    for (int i = 0; i < 5; i++) {
        ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3);
        ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);
    }
#endif
    ulnet_reliable_packet_t *msg1 = (ulnet_reliable_packet_t *) arena_deref(&sessions[0]->arena, sessions[0]->reliable_rx_packet_history[SAM2_SPECTATOR_START][0]);
    ulnet_reliable_packet_t *msg2 = (ulnet_reliable_packet_t *) arena_deref(&sessions[0]->arena, sessions[0]->reliable_rx_packet_history[SAM2_SPECTATOR_START][1]);

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

    // Create two sessions
    for (int i = 0; i < 2; i++) {
        sessions[i] = (ulnet_session_t *)calloc(1, sizeof(ulnet_session_t));
        ulnet_session_init_defaulted(sessions[i]);
        sessions[i]->reliable_retransmit_delay_microseconds = 0;
        sessions[i]->use_inproc_transport = true;
        sessions[i]->retro_run = ulnet__test_retro_run;
        sessions[i]->retro_serialize_size = ulnet__test_retro_serialize_size;
        sessions[i]->retro_serialize = ulnet__test_retro_serialize;
        sessions[i]->retro_unserialize = ulnet__test_retro_unserialize;
    }

    // Set up peer IDs and room state
    sessions[0]->our_peer_id = 10001;  // Authority
    sessions[1]->our_peer_id = 30002;  // Spectator

    // Configure room state for both sessions
    sam2_room_t room = {0};
    room.flags = SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    room.peer_ids[SAM2_AUTHORITY_INDEX] = 10001;
    room.peer_ids[SAM2_SPECTATOR_START] = 30002;

    sessions[0]->room_we_are_in = room;
    sessions[1]->room_we_are_in = room;

    // Connect the transport
    sessions[0]->inproc[SAM2_SPECTATOR_START] = &transport;
    sessions[1]->inproc[SAM2_AUTHORITY_INDEX] = &transport;
    sessions[0]->agent_peer_ids[SAM2_SPECTATOR_START] = room.peer_ids[SAM2_SPECTATOR_START];
    sessions[1]->agent_peer_ids[SAM2_AUTHORITY_INDEX] = room.peer_ids[SAM2_AUTHORITY_INDEX];

    // Prime savestate transfer
    sessions[1]->frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    sessions[0]->peer_needs_sync_bitfield |= (1ULL << SAM2_SPECTATOR_START);

    sessions[0]->debug_udp_recv_drop_rate = 1.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "HELLO", sizeof("HELLO") - 1); // DROP
    sessions[0]->debug_udp_recv_drop_rate = 0.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "WORLD", sizeof("WORLD") - 1); // (NOT SENT) added to reliable_tx_packet_history
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3); // RETRANSMIT "HELLO"
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3); // RECEIVE "HELLO"
    ulnet_reliable_send_with_acks_only(sessions[0], SAM2_SPECTATOR_START, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1); // ACK "HELLO"
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3); // RETRANSMIT "WORLD"
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3); // RECEIVE "WORLD"

    ulnet_reliable_packet_t *msg1 = (ulnet_reliable_packet_t *) arena_deref(&sessions[0]->arena, sessions[0]->reliable_rx_packet_history[SAM2_SPECTATOR_START][0]);
    ulnet_reliable_packet_t *msg2 = (ulnet_reliable_packet_t *) arena_deref(&sessions[0]->arena, sessions[0]->reliable_rx_packet_history[SAM2_SPECTATOR_START][1]);

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


#include <zstd.h>

#if defined(ULNET_TEST_MAIN)
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

int main () {
    ulnet_session_t *session_1 = NULL;
    ulnet_session_t *session_2 = NULL;

    juice_set_log_level(JUICE_LOG_LEVEL_DEBUG);

    int status = ulnet_test_inproc(NULL, NULL);
    if (status != 0) {
        printf("Inproc test failed with status: %d\n", status);
        return status;
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

