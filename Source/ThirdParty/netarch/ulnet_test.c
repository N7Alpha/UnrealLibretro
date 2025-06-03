#define SAM2_ENABLE_LOGGING
#include "ulnet.h"
#include "sam2.h"
#include "juice/juice.h"

#define ULNET__TEST_SAM2_PORT (SAM2_SERVER_DEFAULT_PORT + 1)

int ulnet__test_forward_messages(sam2_server_t *server, ulnet_session_t *session, sam2_socket_t socket) {
    int status;
    sam2_message_u message;

    sam2_server_poll(server);
    for (;;) {
        status = sam2_client_poll(socket, &message);
        if (status < 0) {
            SAM2_LOG_ERROR("Error polling sam2 server: %d", status);
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

static const char g_serialize_test_data[] = "Test data";
size_t ulnet__test_retro_serialize_size(void *user_ptr) {
    return sizeof(g_serialize_test_data);
}

bool ulnet__test_retro_serialize(void *user_ptr, void *data, size_t size) {
    if (size < sizeof(g_serialize_test_data) - 1) {
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
    void *msg1;
    void *msg2;
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
        sessions[i]->sam2_send_callback = ulnet__test_sam2_send_callback;
        sessions[i]->user_ptr = &sockets[i];
        sessions[i]->retro_run = ulnet__test_retro_run;
        sessions[i]->retro_serialize_size = ulnet__test_retro_serialize_size;
        sessions[i]->retro_serialize = ulnet__test_retro_serialize;
        sessions[i]->retro_unserialize = ulnet__test_retro_unserialize;

        status = sam2_client_connect(&sockets[i], "localhost", ULNET__TEST_SAM2_PORT);
        if (status) {
            SAM2_LOG_ERROR("Error while starting connection to sam2 server");
            goto _10;
        }

        int connection_established = 0;
        for (int attempt = 0; attempt < 10; attempt++) {
            connection_established = sam2_client_poll_connection(sockets[i], 0);
            status = sam2_server_poll(server);
            if (status < 0) {
                SAM2_LOG_ERROR("Error running uv loop: %d", status);
                goto _10;
            }
        }

        if (!connection_established) {
            SAM2_LOG_ERROR("Failed to connect to sam2 server");
            status = 1;
            goto _10;
        }

        status = ulnet__test_forward_messages(server, sessions[i], sockets[i]);
        if (status < 0) {
            SAM2_LOG_ERROR("Error forwarding messages: %d", status);
            goto _10;
        }
    }

    sessions[1]->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = sessions[0]->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX];
    sessions[1]->frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    ulnet_startup_ice_for_peer(sessions[1], sessions[0]->our_peer_id, SAM2_AUTHORITY_INDEX, NULL);

    for (int attempt = 0; attempt < 10000; attempt++) {
        for (int i = 0; i < sizeof(sessions)/sizeof(sessions[0]); i++) {
            status = ulnet_poll_session(sessions[i], 0, 0, 0, 60.0, 16e-3);
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
    }

    if (!(   sessions[0]
          && sessions[0]->agent[SAM2_SPECTATOR_START]
          && juice_get_state(sessions[0]->agent[SAM2_SPECTATOR_START]) == JUICE_STATE_COMPLETED)) {
        SAM2_LOG_ERROR("Failed to establish connection");
        status = 1;
        goto _10;
    }

    sessions[0]->debug_udp_recv_drop_rate = 1.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "HELLO", sizeof("HELLO") - 1);
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);

    sessions[0]->debug_udp_recv_drop_rate = 0.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "WORLD", sizeof("WORLD") - 1);
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);

    ulnet_reliable_send_with_acks_only(sessions[0], SAM2_SPECTATOR_START, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1);
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3);
    sessions[1]->reliable_next_retransmit_time = 0;
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3);
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);

    msg1 = arena_deref(&sessions[1]->arena, sessions[1]->reliable_tx_packet_history[SAM2_SPECTATOR_START][0]);
    msg2 = arena_deref(&sessions[1]->arena, sessions[1]->reliable_tx_packet_history[SAM2_SPECTATOR_START][1]);
    if (!(   msg1 && memcmp(msg1, "HELLO", sizeof("HELLO") - 1) == 0
          && msg2 && memcmp(msg2, "WORLD", sizeof("WORLD") - 1) == 0)) {
        SAM2_LOG_ERROR("Failed to send reliable messages");
        status = 1;
        goto _10;
    }

_10:sam2_server_destroy(server);
    free(server);

    if (session_1_out) {
        *session_1_out = sessions[0];
    } else {
        free(sessions[0]);
    }
    if (session_2_out) {
        *session_2_out = sessions[1];
    } else {
        free(sessions[1]);
    }

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
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "HELLO", sizeof("HELLO") - 1);
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);

    sessions[0]->debug_udp_recv_drop_rate = 0.0f;
    ulnet_reliable_send(sessions[1], SAM2_AUTHORITY_INDEX, (const uint8_t*) "WORLD", sizeof("WORLD") - 1);
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);

    ulnet_reliable_send_with_acks_only(sessions[0], SAM2_SPECTATOR_START, (const uint8_t*) "ACK CARRIER", sizeof("ACK CARRIER") - 1);
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3);
    sessions[1]->reliable_next_retransmit_time = 0;
    ulnet_poll_session(sessions[1], 0, 0, 0, 60.0, 16e-3);
    ulnet_poll_session(sessions[0], 0, 0, 0, 60.0, 16e-3);

    // Verify messages were received
    void *msg1 = arena_deref(&sessions[0]->arena,
        sessions[0]->reliable_rx_packet_history[SAM2_SPECTATOR_START][1 % ULNET_RELIABLE_ACK_BUFFER_SIZE]);
    void *msg2 = arena_deref(&sessions[0]->arena,
        sessions[0]->reliable_rx_packet_history[SAM2_SPECTATOR_START][2 % ULNET_RELIABLE_ACK_BUFFER_SIZE]);

    if (!msg1 || !msg2) {
        SAM2_LOG_ERROR("Failed to receive reliable messages");
        status = 1;
        goto cleanup;
    }

    // Check wrapped packet contents
    int offset1 = ulnet_wrapped_header_size((uint8_t*)msg1, ULNET_PACKET_SIZE_BYTES_MAX);
    int offset2 = ulnet_wrapped_header_size((uint8_t*)msg2, ULNET_PACKET_SIZE_BYTES_MAX);

    if (memcmp((uint8_t*)msg1 + offset1, "HELLO", 5) != 0 ||
        memcmp((uint8_t*)msg2 + offset2, "WORLD", 5) != 0) {
        SAM2_LOG_ERROR("Message content mismatch");
        status = 1;
        goto cleanup;
    }

cleanup:
    if (!session_1_out) free(sessions[0]);
    else *session_1_out = sessions[0];

    if (!session_2_out) free(sessions[1]);
    else *session_2_out = sessions[1];

    return status;
}
