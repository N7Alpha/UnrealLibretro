#ifndef ULNET_H
#define ULNET_H

#include "sam2.h"

typedef struct ulnet_nat_agent ulnet_nat_agent_t;

typedef enum ulnet_nat_state {
    ULNET_NAT_STATE_DISCONNECTED = 0,
    ULNET_NAT_STATE_CONNECTING,
    ULNET_NAT_STATE_READY,
    ULNET_NAT_STATE_FAILED
} ulnet_nat_state_t;

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef ULNET_MALLOC
#define ULNET_MALLOC(size) malloc(size)
#endif

#ifndef ULNET_FREE
#define ULNET_FREE(ptr) free(ptr)
#endif

#ifndef ULNET_MEMMOVE
#define ULNET_MEMMOVE(a, b, sz) memmove(a, b, sz)
#endif

#ifndef ULNET_LINKAGE
#ifdef __cplusplus
#define ULNET_LINKAGE extern "C"
#else
#define ULNET_LINKAGE extern
#endif
#endif

#ifndef ULNET_ZSTD_COMPRESS_BOUND
#define ULNET_ZSTD_COMPRESS_BOUND(src_size) UZSTD_COMPRESS_BOUND(src_size)
#endif

#ifndef ULNET_ZSTD_COMPRESS
#define ULNET_ZSTD_COMPRESS(dst, dst_capacity, src, src_size, quality) \
    uzstd_compress(dst, dst_capacity, src, src_size, quality)
#endif

#ifndef ULNET_ZSTD_DECOMPRESS
#define ULNET_ZSTD_DECOMPRESS(dst, dst_capacity, src, src_size) \
    uzstd_decompress(dst, dst_capacity, src, src_size)
#endif

// The payload here is regarding the max payload that we *can* use
// We don't want to exceed the MTU because that can result in guranteed lost packets under certain conditions
// Considering various things like UDP/IP headers, STUN/TURN headers, and additional junk
// load-balancers/routers might add I keep this conservative
#define ULNET_PACKET_SIZE_BYTES_MAX 1408

#define ULNET_CORE_OPTIONS_MAX 128
#define ULNET_STATE_PACKET_HISTORY_SIZE 64

#define ULNET_HEADER_SIZE                        1
#define ULNET_FLAGS_MASK                         0b00011111
#define ULNET_CHANNEL_MASK                       0b11100000

#define ULNET_CHANNEL_EXTRA                      0b00000000
#define ULNET_CHANNEL_ASCII                      0b01000000
#define ULNET_CHANNEL_SPECTATOR_INPUT            0b01100000
#define ULNET_CHANNEL_SAVESTATE_TRANSFER         0b10000000
#define ULNET_CHANNEL_RELIABLE                   0b10100000
#define ULNET_CHANNEL_INPUT_HI                   0b11000000
#define ULNET_CHANNEL_INPUT                      0b11100000

#define ULNET_RELIABLE_FLAG_ACK_ONLY             0b00010000

// Bytes ulnet__wrap_packet prepends when a packet is sent over the reliable channel:
// channel byte + 16-bit sequence + 16-bit ack. State packets are always wrapped to carry ACKs, so
// their payload must leave room for this or the wrap silently fails (ulnet__wrap_packet returns -1).
#define ULNET_RELIABLE_WRAPPER_BYTES (ULNET_HEADER_SIZE + 2 * (int)sizeof(uint16_t))

#define ULNET_PACKET_FLAG_TX                     0x1000
#define ULNET_PACKET_FLAG_TX_RELIABLE_RETRANSMIT 0x2000

#define ulnet_exit_header  "E" "X" "I" "T" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"
#define ULNET_EXIT_HEADER {'E','X','I','T',    '0' + SAM2_VERSION_MAJOR, '.',    '0' + SAM2_VERSION_MINOR, 'r'}

#define ULNET_WAITING_FOR_SAVE_STATE_SENTINEL    987654321012345678LL

#define ULNET_SESSION_FLAG_TICKED                0b00000001ULL
#define ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY    0b00000010ULL
#define ULNET_SESSION_FLAG_READY_TO_TICK_SET     0b00000100ULL
#define ULNET_SESSION_FLAG_DRAW_IMGUI            0b00001000ULL

static SAM2_FORCEINLINE bool ulnet__is_input_channel(uint8_t channel_and_flags) {
    uint8_t channel = channel_and_flags & ULNET_CHANNEL_MASK;
    return channel == ULNET_CHANNEL_INPUT || channel == ULNET_CHANNEL_INPUT_HI;
}


#define ULNET_MAX_SAMPLE_SIZE 128

// Reliable data is stop-and-wait on the wire; this bounds queued transmit payloads and RX debug history.
#define ULNET_RELIABLE_ACK_BUFFER_SIZE 128

#define ULNET_RS_GF_SIZE 255
#define ULNET_RS_GF_ORDER 256
#define ULNET_RS_DATA_BLOCKS_MAX 239
#define ULNET_RS_TOTAL_BLOCKS_MAX 255

// This constant defines the maximum number of frames that can be buffered before blocking.
// A value of 2 implies no delay can be accomidated.
//```
// Consider the following scenario:
// logical-time | peer a        | peer b
// --------------------------------------------
// 0            | send input 0a | send input 0b
// 1            | recv input 0b | recv input 0a
// 2            | ------------- | tick frame 0
// 3            | ------------- | send input 1b
// 4            | recv input 1b | -------------
// 5            | tick frame 0  | -------------
//```
// The issue occurs at logical-time 4 when peer a receives input 1 before ticking frame 0.
// If the input buffer only holds 1 frame, the input packet for frame 0 would be overwritten.
// To handle the case where a peer immediately ticks and sends an input after receiving,
// the input buffer needs to hold at least 2 frames.
//
// Setting ULNET_DELAY_BUFFER_SIZE to 2 allows for no frame delay while still handling this scenario.
// The following constant is set to 8 which yields 3 frames of delay this corresponds to a max RTT PING of 100 ms to not stutter
#define ULNET_DELAY_BUFFER_SIZE 8

#define ULNET_DELAY_FRAMES_MAX (ULNET_DELAY_BUFFER_SIZE/2-1)
#define ULNET_ROOM_CHANGE_LEAD_FRAMES (ULNET_DELAY_FRAMES_MAX + 1)
#if ULNET_ROOM_CHANGE_LEAD_FRAMES <= 0 || ULNET_ROOM_CHANGE_LEAD_FRAMES > ULNET_DELAY_BUFFER_SIZE
#error ULNET_ROOM_CHANGE_LEAD_FRAMES must fit in the input delay buffer
#endif

#define ULNET_PORT_COUNT 8
typedef int16_t ulnet_input_state_t[64]; // This must be a POD for putting into packets

typedef struct ulnet_packet_ref {
    uint8_t *data;
    uint16_t size;
    uint16_t flags;
} ulnet_packet_ref_t;
static const ulnet_packet_ref_t ulnet_packet_ref_null = { NULL, 0, 0 };

static void ulnet_packet_ref_clear(ulnet_packet_ref_t *ref);
static int ulnet_packet_ref_set(ulnet_packet_ref_t *ref, const void *data, size_t size, uint16_t flags);

typedef struct ulnet_core_option {
    char key[128];
    char value[128];
} ulnet_core_option_t;

// @todo This is really sparse so you should just add routines to read values from it in the serialized format
typedef struct {
    int64_t frame; // Frame for which currently buffered input, room snapshot, and core_option should be applied
    int64_t room_effective_frame; // Frame at which `room` becomes the simulation room for everyone (a boundary, advertised ahead)
    ulnet_input_state_t input_state[ULNET_DELAY_BUFFER_SIZE][ULNET_PORT_COUNT];
    sam2_room_t room; // Authoritative room snapshot; only meaningful from the authority. Applied at room_effective_frame
    ulnet_core_option_t core_option[ULNET_DELAY_BUFFER_SIZE]; // Max 1 option per frame provided by the authority

    int64_t save_state_frame; // This is the current frame the peer is on the essentially
    uint32_t save_state_hash[ULNET_DELAY_BUFFER_SIZE];
    uint32_t input_state_hash[ULNET_DELAY_BUFFER_SIZE];
    int64_t input_poll_unix_usec[ULNET_DELAY_BUFFER_SIZE];
} ulnet_state_t;
SAM2_STATIC_ASSERT(
    sizeof(ulnet_state_t) ==
    (sizeof(((ulnet_state_t *)0)->frame)
    + sizeof(((ulnet_state_t *)0)->room_effective_frame)
    + sizeof(((ulnet_state_t *)0)->input_state)
    + sizeof(((ulnet_state_t *)0)->room)
    + sizeof(((ulnet_state_t *)0)->core_option))
    + sizeof(((ulnet_state_t *)0)->save_state_frame)
    + sizeof(((ulnet_state_t *)0)->save_state_hash)
    + sizeof(((ulnet_state_t *)0)->input_state_hash)
    + sizeof(((ulnet_state_t *)0)->input_poll_unix_usec),
    "ulnet_state_t is not packed"
);

typedef struct {
    uint8_t channel_and_port;
    uint8_t ping_send_unix_usec_le[8]; // Local userspace TX time, T1/T3 in the RFC 5905 exchange
    uint8_t ping_echo_send_unix_usec_le[8]; // Peer's previous T1 echoed back to it
    uint8_t ping_echo_callsite_receive_unix_usec_le[8]; // Our userspace receive time for that previous peer packet, T2
    uint8_t ping_echo_kernel_receive_unix_usec_le[8]; // Same previous packet's kernel SO_TIMESTAMP T2, if available
    uint8_t coded_state[];
} ulnet_state_packet_t;

typedef struct {
    uint8_t channel_and_flags;
    uint8_t sequence_le[2];
    uint8_t ack_sequence_le[2];
    uint8_t payload[];
} ulnet_reliable_packet_t;

#define FEC_PACKET_GROUPS_MAX 16
#define FEC_REDUNDANT_BLOCKS 16 // ULNET is hardcoded based on this value so it can't really be changed without breaking the protocol

#define ULNET_SAVESTATE_TRANSFER_FLAG_K_IS_239         0b0001
#define ULNET_SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0 0b0010
#define ULNET_SAVESTATE_TRANSFER_FLAG_ACK              0b0100
#define ULNET_SAVESTATE_TRANSFER_ID_MASK               0b11000
#define ULNET_SAVESTATE_TRANSFER_ID_SHIFT              3
#define ULNET_SAVESTATE_TRANSFER_DEFAULT_BANDWIDTH_BITS_PER_SECOND (8LL * 1024LL * 1024LL)
#define ULNET_SAVESTATE_TRANSFER_MAX_RETRIES 2
#define ULNET_SAVESTATE_TRANSFER_ACK_TIMEOUT_MICROSECONDS 2000000LL

typedef struct {
    uint8_t channel_and_flags;
    union {
        uint8_t reed_solomon_k;
        uint8_t packet_groups;
        uint8_t sequence_hi;
    };

    uint8_t sequence_lo;

    //uint8_t payload[]; // Variable size; at most ULNET_PACKET_SIZE_BYTES_MAX-3
} ulnet_save_state_packet_header_t;

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
    int64_t total_size_bytes;
    int64_t frame_counter;
    sam2_room_t room;

    uint32_t checksum;
    int32_t compressed_options_size;
    int32_t compressed_savestate_size;
    int32_t decompressed_savestate_size;
#if 0
    uint8_t compressed_savestate_data[compressed_savestate_size];
    uint8_t compressed_options_data[compressed_options_size];
#else
    uint8_t compressed_data[];
#endif
} savestate_transfer_payload_t;

typedef struct ulnet_transport_inproc_buffer {
    uint8_t msg[256][ULNET_PACKET_SIZE_BYTES_MAX];
    uint16_t msg_size[256];
    int32_t count;  // Number of messages available
} ulnet_inproc_buf_t;

typedef struct ulnet_transport_inproc {
    ulnet_inproc_buf_t buf1; // Smaller peer_id -> larger peer_id
    ulnet_inproc_buf_t buf2; // Larger peer_id -> smaller peer_id
} ulnet_transport_inproc_t;

typedef struct ulnet_session {
    int64_t frame_counter;
    int64_t delay_frames;
    int64_t core_wants_tick_at_unix_usec;
    int64_t flags;
    uint16_t our_peer_id;

    sam2_room_t room_we_are_in; // The committed simulation room

    // Authority only: the desired room and the boundary frame at which it takes effect for everyone.
    // Spectator (topology-clear) edits are mirrored into room_we_are_in immediately (determinism-safe);
    // active-set (topology bit / player) edits land here and are advertised ahead in state packets.
    sam2_room_t next_room;
    int64_t next_room_effective_frame;
    int64_t authority_room_snapshot_last_sent_frame;

    ulnet_input_state_t next_input_state[SAM2_PORT_MAX]; // This is the next input state that will be buffered, it is not yet applied to the state buffer
    ulnet_core_option_t next_core_option;

    ulnet_core_option_t core_options[ULNET_CORE_OPTIONS_MAX]; // @todo I don't like this here

    ulnet_state_t state[SAM2_TOTAL_PEERS]; // Indexed uniformly by port; only p2p (topology-bit) ports contribute deterministic input

    // MARK: Peer fields
    uint64_t peer_needs_sync_bitfield;
    uint64_t peer_pending_disconnect_bitfield;
    int use_inproc_transport; // "Tag" for the following union
    union {
        ulnet_nat_agent_t *agent[SAM2_TOTAL_PEERS]; // Invariant: agent[p] (when non-NULL) is the connection to room_we_are_in.peer_ids[p]
        ulnet_transport_inproc_t *inproc[SAM2_TOTAL_PEERS];
    };

    int64_t peer_desynced_frame[SAM2_TOTAL_PEERS];
    ulnet_input_state_t spectator_suggested_input_state[SAM2_TOTAL_PEERS][ULNET_PORT_COUNT];
    ulnet_packet_ref_t state_packet_history[SAM2_TOTAL_PEERS][ULNET_STATE_PACKET_HISTORY_SIZE]; // Indexable by (frame / ULNET_DELAY_BUFFER_SIZE) % ULNET_STATE_PACKET_HISTORY_SIZE
    ulnet_packet_ref_t packet_history[SAM2_TOTAL_PEERS][256]; // All packets circular buffer in order they were sent/recv
    uint8_t packet_history_next[SAM2_TOTAL_PEERS];
    int64_t reliable_retransmit_delay_microseconds;
    int64_t reliable_last_transmit_time[SAM2_TOTAL_PEERS];
    ulnet_packet_ref_t reliable_tx_packet_history[SAM2_TOTAL_PEERS][ULNET_RELIABLE_ACK_BUFFER_SIZE]; // Queued reliable data, indexed by sequence % size; only tx_head is in flight
    ulnet_packet_ref_t reliable_rx_packet_history[SAM2_TOTAL_PEERS][ULNET_RELIABLE_ACK_BUFFER_SIZE]; // RX debug/duplicate history, indexed by sequence % size
    uint16_t reliable_tx_next_seq[SAM2_TOTAL_PEERS]; // Next sequence to assign
    uint16_t reliable_tx_head[SAM2_TOTAL_PEERS];     // Oldest unacked sequence / transmit queue head
    uint16_t reliable_rx_head[SAM2_TOTAL_PEERS];     // Next sequence we expect to receive
    int64_t peer_last_packet_send_unix_usec[SAM2_TOTAL_PEERS];
    int64_t peer_last_packet_callsite_receive_unix_usec[SAM2_TOTAL_PEERS];
    int64_t peer_last_packet_kernel_receive_unix_usec[SAM2_TOTAL_PEERS];
    int64_t peer_clock_offset_usec[SAM2_TOTAL_PEERS]; // RFC 5905 theta: remote clock ~= local clock + offset
    int64_t peer_kernel_clock_offset_usec[SAM2_TOTAL_PEERS];
    int64_t peer_packet_ping_samples[SAM2_TOTAL_PEERS];
    int64_t peer_packet_ping_usec[SAM2_TOTAL_PEERS]; // RFC 5905 delta from userspace receive/send timestamps
    int64_t peer_packet_kernel_ping_samples[SAM2_TOTAL_PEERS];
    int64_t peer_packet_kernel_ping_usec[SAM2_TOTAL_PEERS]; // RFC 5905 delta from kernel SO_TIMESTAMP receive timestamps
    int64_t peer_input_to_core_ping_usec[SAM2_TOTAL_PEERS];
    int64_t pending_packet_kernel_receive_unix_usec;

    // MARK: Save state transfer
    int compression_quality;
    uint8_t remote_packet_groups; // This is used to bookkeep how much data we actually need to receive to reform the complete savestate
    uint8_t remote_savestate_transfer_id;
    uint8_t remote_savestate_completed_transfer_id;
    ulnet_packet_ref_t packet_reference[FEC_PACKET_GROUPS_MAX][ULNET_RS_TOTAL_BLOCKS_MAX];
    int fec_index_counter[FEC_PACKET_GROUPS_MAX]; // Counts unique packets received in each packet group
    uint8_t savestate_transfer_id;
    uint8_t savestate_transfer_retry_count;
    uint64_t savestate_transfer_awaiting_bitfield;
    int64_t savestate_transfer_ack_deadline_unix_usec;

    void *user_ptr;
    int (*sam2_send_callback)(void *user_ptr, char *response);
    int (*populate_core_options_callback)(void *user_ptr, ulnet_core_option_t options[ULNET_CORE_OPTIONS_MAX]);

    void (*retro_run)(void *user_ptr);
    size_t (*retro_serialize_size)(void *user_ptr);
    bool (*retro_serialize)(void *user_ptr, void *, size_t);
    bool (*retro_unserialize)(void *user_ptr, const void *data, size_t size);

    float debug_udp_recv_drop_rate;
    float debug_udp_send_drop_rate;

    char nat_stun_host[64];
    uint16_t nat_stun_port;

    bool imgui_packet_table_show_most_recent_first;
    int input_packet_size[SAM2_TOTAL_PEERS][ULNET_MAX_SAMPLE_SIZE];
    int save_state_execution_time_cycles[ULNET_MAX_SAMPLE_SIZE];
} ulnet_session_t;

#if __cplusplus >= 201103L
#include <type_traits>
static_assert(std::is_trivially_default_constructible<ulnet_session_t>::value && std::is_standard_layout<ulnet_session_t>::value,
    "ulnet_session_t must be a POD type for safe memory operations");
#endif

ULNET_LINKAGE int ulnet_process_message(ulnet_session_t *session, const char *response);
ULNET_LINKAGE void ulnet_send_save_state(ulnet_session_t *session, int port, void *save_state, size_t save_state_size, int64_t save_state_frame);
ULNET_LINKAGE void ulnet_startup_nat_for_peer(ulnet_session_t *session, uint64_t peer_id, int p, const char *remote_signal);
ULNET_LINKAGE void ulnet_disconnect_peer(ulnet_session_t *session, int peer_port);
ULNET_LINKAGE void ulnet__reconcile_connections(ulnet_session_t *session, const sam2_room_t *new_room);
ULNET_LINKAGE void ulnet_session_init_defaulted(ulnet_session_t *session);
ULNET_LINKAGE void ulnet_receive_packet_callback(ulnet_nat_agent_t *agent, const char *packet, size_t size, void *user_ptr);
ULNET_LINKAGE ulnet_nat_state_t ulnet_nat_get_state(ulnet_nat_agent_t *agent);
ULNET_LINKAGE const char *ulnet_nat_state_to_string(ulnet_nat_state_t state);
ULNET_LINKAGE void ulnet_set_stun_server(ulnet_session_t *session, const char *host, uint16_t port);
ULNET_LINKAGE int ulnet_udp_send(ulnet_session_t *session, int port, const uint8_t *packet, size_t size);
ULNET_LINKAGE int ulnet_reliable_send_with_acks_only(ulnet_session_t *session, int port, const uint8_t *packet, int size);
ULNET_LINKAGE int ulnet_reliable_send(ulnet_session_t *session, int port, const uint8_t *packet, int size);
ULNET_LINKAGE int ulnet_message_send(ulnet_session_t *session, int port, const uint8_t *message);
ULNET_LINKAGE int ulnet_poll_session(ulnet_session_t *session, bool force_save_state_on_tick, uint8_t *save_state, size_t save_state_capacity,
    double frame_rate, double max_sleeping_allowed_when_polling_network_seconds);
ULNET_LINKAGE void ulnet_session_tear_down(ulnet_session_t *session);
ULNET_LINKAGE int64_t ulnet__get_unix_time_microseconds();
ULNET_LINKAGE uint32_t ulnet_xxh32(const void* data, size_t len, uint32_t seed);
ULNET_LINKAGE int64_t ulnet__room_advertise_frame_from_effective_frame(int64_t effective_frame);

ULNET_LINKAGE void ulnet_imgui_show_session(ulnet_session_t *session);
ULNET_LINKAGE void ulnet_imgui_show_recent_packets_table(ulnet_session_t *session, int p);
ULNET_LINKAGE void ulnet_imgui_plot_history(ulnet_session_t *session);
ULNET_LINKAGE int ulnet_test_ice(ulnet_session_t **session_1_out, ulnet_session_t **session_2_out);
ULNET_LINKAGE int ulnet_test_inproc(ulnet_session_t **session_1_out, ulnet_session_t **session_2_out);
ULNET_LINKAGE int ulnet_test_inproc_reliable_ack_unblocks_queue(void);

static bool ulnet_is_authority(ulnet_session_t *session) {
    return    session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]
           || session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] == 0; // @todo I don't think this extra check should be necessary
}

// A port hosts a p2p player when it is occupied and its topology bit is set.
// p2p players hold direct mesh links and contribute deterministic input. The authority at port 0 is
// only a p2p player when bit 0 is set; with bit 0 clear it is coordinator/trusted observer only.
static inline bool ulnet_port_is_p2p(const sam2_room_t *room, int port) {
    if (port < 0 || port >= SAM2_TOTAL_PEERS) return false;
    if (room->peer_ids[port] <= SAM2_PORT_SENTINELS_MAX) return false;
    return (room->peer_topology & (1ULL << port)) != 0;
}

// A port contributes deterministic input when it hosts a p2p player that is not flagged inactive.
// The per-port inactive flags only have bits for the low ports (the uint32 flags field can't hold 64);
// higher ports are always considered active.
static inline bool ulnet_port_is_active_player(const sam2_room_t *room, int port) {
    if (!ulnet_port_is_p2p(room, port)) return false;
    if (port < SAM2_PORT_MAX && (room->flags & (SAM2_FLAG_PORT0_PEER_IS_INACTIVE << port))) return false;
    return true;
}

static bool ulnet_is_spectator(ulnet_session_t *session, uint64_t peer_id) {
    int port = sam2_get_port_of_peer(&session->room_we_are_in, peer_id);

    return    session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED
           && port >= 0
           && port != SAM2_AUTHORITY_INDEX
           && !ulnet_port_is_p2p(&session->room_we_are_in, port);
}
#endif

#ifndef UZSTD_H
#define UZSTD_H

#ifndef UZSTD_LINKAGE
#ifdef __cplusplus
#define UZSTD_LINKAGE extern "C"
#else
#define UZSTD_LINKAGE extern
#endif
#endif

#define UZSTD_IS_ERROR(result) ((result) == (size_t)-1)
#define UZSTD_COMPRESS_BOUND(src_size) ((size_t)(src_size) + (((size_t)(src_size)) >> 17) * 3 + 32)

UZSTD_LINKAGE size_t uzstd_compress_bound(size_t src_size);
UZSTD_LINKAGE size_t uzstd_compress(void *dst, size_t dst_cap, const void *src, size_t src_size, int level);
UZSTD_LINKAGE size_t uzstd_decompress(void *dst, size_t dst_cap, const void *src, size_t src_size);
UZSTD_LINKAGE unsigned long long uzstd_frame_content_size(const void *src, size_t src_size);

#endif /* UZSTD_H */

#if defined(ULNET_IMPLEMENTATION)
#ifndef ULNET_NAT_C
#define ULNET_NAT_C
#if defined(ULNET_IMGUI)
#include "imgui.h"
#include "implot.h"

#define IMH(statement) if (session->flags & ULNET_SESSION_FLAG_DRAW_IMGUI) { statement }
#else
#define IMH(statement) do {} while (0);
#endif

#include <assert.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#define ULNET_SOCKET_T uintptr_t
#define ULNET_SOCKET_INVALID INVALID_SOCKET
#define ULNET_CLOSESOCKET closesocket
#define ULNET_SOCKERRNO ((int)WSAGetLastError())
#define ULNET_EWOULDBLOCK WSAEWOULDBLOCK
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#define ULNET_SOCKET_T int
#define ULNET_SOCKET_INVALID (-1)
#define ULNET_CLOSESOCKET close
#define ULNET_SOCKERRNO errno
#define ULNET_EWOULDBLOCK EWOULDBLOCK
#endif

#define ULNET_NAT_SIGNAL_PREFIX "ULN1"
#define ULNET_NAT_SIGNAL_CANDIDATE 'C'
#define ULNET_NAT_PROBE "ULN1P"
#define ULNET_NAT_PROBE_ACK "ULN1A"
#define ULNET_NAT_CANDIDATES_MAX 16
#define ULNET_NAT_POLL_PACKET_MAX 1600
#define ULNET_NAT_CHECK_PACING_USEC 50000
#define ULNET_NAT_CONNECT_TIMEOUT_USEC 5000000

typedef struct ulnet_nat_candidate {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    uint8_t transaction_id[12];
    int64_t next_check_time_usec;
} ulnet_nat_candidate_t;

struct ulnet_nat_agent {
    ULNET_SOCKET_T socket;
    ulnet_nat_state_t state;
    ulnet_session_t *session;
    int peer_port;
    int candidate_count;
    ulnet_nat_candidate_t candidate[ULNET_NAT_CANDIDATES_MAX];
    struct sockaddr_storage selected_addr;
    socklen_t selected_addr_len;
    struct sockaddr_storage stun_server_addr;
    socklen_t stun_server_addr_len;
    uint8_t stun_transaction_id[12];
    uint32_t rng_state;
    int stun_candidate_sent;
    int64_t connect_deadline_usec;
    int64_t last_stun_time_usec;
};

#define ULNET__STUN_BINDING_REQUEST  0x0001
#define ULNET__STUN_BINDING_RESPONSE 0x0101
#define ULNET__STUN_MAGIC_COOKIE     0x2112A442u
#define ULNET__STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020

static uint16_t ulnet__read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t ulnet__read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void ulnet__write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void ulnet__write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static int64_t ulnet__read_le64s(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) {
        v = (v << 8) | p[i];
    }
    return (int64_t)v;
}

static void ulnet__write_le64s(uint8_t *p, int64_t v) {
    uint64_t u = (uint64_t)v;
    for (int i = 0; i < 8; i++) {
        p[i] = (uint8_t)(u >> (8 * i));
    }
}

static int ulnet__socket_would_block(void) {
#ifdef _WIN32
    return ULNET_SOCKERRNO == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

static int ulnet__set_nonblocking(ULNET_SOCKET_T sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

static void ulnet__enable_kernel_rx_timestamps(ULNET_SOCKET_T sock) {
#ifndef _WIN32
#if defined(SO_TIMESTAMP)
    int enabled = 1;
    setsockopt(sock, SOL_SOCKET, SO_TIMESTAMP, &enabled, sizeof(enabled));
#else
    (void)sock;
#endif
#else
    (void)sock;
#endif
}

static int ulnet__recvfrom_with_timestamp(ULNET_SOCKET_T sock, char *packet, size_t packet_capacity,
    struct sockaddr_storage *from, socklen_t *from_len, int64_t *kernel_receive_time_usec) {
    *kernel_receive_time_usec = 0;

#ifdef _WIN32
    return (int)recvfrom(sock, packet, (int)packet_capacity, 0, (struct sockaddr *)from, from_len);
#else
#if defined(SO_TIMESTAMP) && defined(SCM_TIMESTAMP)
    struct iovec iov;
    struct msghdr msg;
    char control[CMSG_SPACE(sizeof(struct timeval))];

    memset(&iov, 0, sizeof(iov));
    iov.iov_base = packet;
    iov.iov_len = packet_capacity;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = from;
    msg.msg_namelen = *from_len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    int ret = (int)recvmsg(sock, &msg, 0);
    if (ret >= 0) {
        *from_len = msg.msg_namelen;
        for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (   cmsg->cmsg_level == SOL_SOCKET
                && cmsg->cmsg_type == SCM_TIMESTAMP
                && cmsg->cmsg_len >= CMSG_LEN(sizeof(struct timeval))) {
                struct timeval tv;
                memcpy(&tv, CMSG_DATA(cmsg), sizeof(tv));
                *kernel_receive_time_usec = (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
                break;
            }
        }
    }

    return ret;
#else
    return (int)recvfrom(sock, packet, packet_capacity, 0, (struct sockaddr *)from, from_len);
#endif
#endif
}

static void ulnet__addr_from_ipv4_mapped(struct sockaddr_storage *addr, uint32_t ipv4_network_order, uint16_t port) {
    struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
    memset(addr, 0, sizeof(*addr));
    a6->sin6_family = AF_INET6;
    a6->sin6_port = htons(port);
    a6->sin6_addr.s6_addr[10] = 0xff;
    a6->sin6_addr.s6_addr[11] = 0xff;
    memcpy(&a6->sin6_addr.s6_addr[12], &ipv4_network_order, 4);
}

static int ulnet__addr_equal(const struct sockaddr_storage *a, const struct sockaddr_storage *b) {
    if (a->ss_family != b->ss_family) return 0;
    if (a->ss_family == AF_INET) {
        const struct sockaddr_in *a4 = (const struct sockaddr_in *)a;
        const struct sockaddr_in *b4 = (const struct sockaddr_in *)b;
        return a4->sin_port == b4->sin_port && a4->sin_addr.s_addr == b4->sin_addr.s_addr;
    }
    if (a->ss_family == AF_INET6) {
        const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)a;
        const struct sockaddr_in6 *b6 = (const struct sockaddr_in6 *)b;
        return a6->sin6_port == b6->sin6_port
            && a6->sin6_scope_id == b6->sin6_scope_id
            && memcmp(&a6->sin6_addr, &b6->sin6_addr, sizeof(a6->sin6_addr)) == 0;
    }
    return 0;
}

static int ulnet__format_addr(const struct sockaddr_storage *addr, char *host, size_t host_size, uint16_t *port) {
    void *src = NULL;
    if (addr->ss_family == AF_INET) {
        const struct sockaddr_in *a4 = (const struct sockaddr_in *)addr;
        src = (void *)&a4->sin_addr;
        *port = ntohs(a4->sin_port);
    } else if (addr->ss_family == AF_INET6) {
        const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)addr;
        *port = ntohs(a6->sin6_port);
        if (IN6_IS_ADDR_V4MAPPED(&a6->sin6_addr)) {
            struct in_addr addr4;
            memcpy(&addr4.s_addr, &a6->sin6_addr.s6_addr[12], 4);
            return inet_ntop(AF_INET, &addr4, host, (socklen_t)host_size) ? 0 : -1;
        }
        src = (void *)&a6->sin6_addr;
    } else {
        return -1;
    }

    return inet_ntop(addr->ss_family, src, host, (socklen_t)host_size) ? 0 : -1;
}

#if defined(ULNET_NAT_DEBUG)
static void ulnet__nat_debug_addr(const char *prefix, const struct sockaddr_storage *addr) {
    char host[INET6_ADDRSTRLEN];
    uint16_t port = 0;
    if (ulnet__format_addr(addr, host, sizeof(host), &port) == 0) {
        SAM2_LOG_INFO("%s %s:%u", prefix, host, (unsigned)port);
    }
}
#else
#define ulnet__nat_debug_addr(prefix, addr) do { (void)(prefix); (void)(addr); } while (0)
#endif

static int ulnet__parse_addr(const char *host, uint16_t port, struct sockaddr_storage *addr, socklen_t *addr_len) {
    struct in_addr ipv4;
    if (inet_pton(AF_INET, host, &ipv4) == 1) {
        ulnet__addr_from_ipv4_mapped(addr, ipv4.s_addr, port);
        *addr_len = sizeof(struct sockaddr_in6);
        return 0;
    }

    struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
    memset(addr, 0, sizeof(*addr));
    a6->sin6_family = AF_INET6;
    a6->sin6_port = htons(port);
    if (inet_pton(AF_INET6, host, &a6->sin6_addr) == 1) {
        *addr_len = sizeof(*a6);
        return 0;
    }

    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char port_string[16];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    snprintf(port_string, sizeof(port_string), "%u", (unsigned)port);
    if (getaddrinfo(host, port_string, &hints, &res) != 0 || !res) {
        return -1;
    }

    if (res->ai_family == AF_INET) {
        struct sockaddr_in *res4 = (struct sockaddr_in *)res->ai_addr;
        ulnet__addr_from_ipv4_mapped(addr, res4->sin_addr.s_addr, port);
        *addr_len = sizeof(struct sockaddr_in6);
    } else {
        memcpy(addr, res->ai_addr, res->ai_addrlen);
        *addr_len = (socklen_t)res->ai_addrlen;
    }
    freeaddrinfo(res);
    return 0;
}

static void ulnet__nat_random_transaction_id(ulnet_nat_agent_t *agent, uint8_t transaction_id[12]) {
    for (int i = 0; i < 12; i++) {
        agent->rng_state = 1664525u * agent->rng_state + 1013904223u;
        transaction_id[i] = (uint8_t)(agent->rng_state >> 24);
    }
}

static void ulnet__nat_set_state(ulnet_nat_agent_t *agent, ulnet_nat_state_t state) {
    if (!agent || agent->state == state) return;
    if (state < agent->state) return;

    ulnet_nat_state_t old_state = agent->state;
    agent->state = state;

    if (state == ULNET_NAT_STATE_FAILED && agent->session) {
        agent->session->peer_pending_disconnect_bitfield |= (1ULL << agent->peer_port);
        return;
    }

    if (old_state < ULNET_NAT_STATE_READY && state >= ULNET_NAT_STATE_READY) {
        ulnet_session_t *session = agent->session;
        if (   session
            && agent->peer_port >= 0
            && agent->peer_port < SAM2_TOTAL_PEERS
            && session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
            SAM2_LOG_INFO("Peer %05" PRId16 " connected; scheduling savestate sync", session->room_we_are_in.peer_ids[agent->peer_port]);
            session->peer_needs_sync_bitfield |= (1ULL << agent->peer_port);
        }
    }
}

static ulnet_nat_candidate_t *ulnet__nat_find_candidate(ulnet_nat_agent_t *agent, const struct sockaddr_storage *addr) {
    for (int i = 0; i < agent->candidate_count; i++) {
        if (ulnet__addr_equal(&agent->candidate[i].addr, addr)) {
            return &agent->candidate[i];
        }
    }
    return NULL;
}

static void ulnet__nat_select_candidate(ulnet_nat_agent_t *agent, const struct sockaddr_storage *addr, socklen_t addr_len) {
    agent->selected_addr = *addr;
    agent->selected_addr_len = addr_len;
    ulnet__nat_set_state(agent, ULNET_NAT_STATE_READY);
}

static ulnet_nat_candidate_t *ulnet__nat_find_candidate_from_transaction_id(ulnet_nat_agent_t *agent, const uint8_t transaction_id[12]) {
    for (int i = 0; i < agent->candidate_count; i++) {
        if (memcmp(agent->candidate[i].transaction_id, transaction_id, 12) == 0) {
            return &agent->candidate[i];
        }
    }
    return NULL;
}

static int ulnet__nat_send_control(ulnet_nat_agent_t *agent, const char *control, size_t size,
    const struct sockaddr_storage *addr, socklen_t addr_len);

static int ulnet__nat_add_candidate(ulnet_nat_agent_t *agent, const struct sockaddr_storage *addr, socklen_t addr_len) {
    if (ulnet__nat_find_candidate(agent, addr)) {
        return 0;
    }

    if (agent->candidate_count >= ULNET_NAT_CANDIDATES_MAX) {
        return -1;
    }

    ulnet_nat_candidate_t *candidate = &agent->candidate[agent->candidate_count];
    memset(candidate, 0, sizeof(*candidate));
    candidate->addr = *addr;
    candidate->addr_len = addr_len;
    candidate->next_check_time_usec = 0;
    agent->candidate_count++;
    ulnet__nat_debug_addr("Added NAT candidate", addr);
    return 0;
}

static int ulnet__nat_send_signal(ulnet_nat_agent_t *agent, char kind, const char *host, uint16_t port) {
    sam2_signal_message_t response = { SAM2_SIGN_HEADER };
    response.peer_id = agent->session->room_we_are_in.peer_ids[agent->peer_port];
    if (kind == ULNET_NAT_SIGNAL_CANDIDATE) {
        snprintf(response.ice_sdp, sizeof(response.ice_sdp), "%s%c %s %u", ULNET_NAT_SIGNAL_PREFIX, kind, host, (unsigned)port);
    } else {
        snprintf(response.ice_sdp, sizeof(response.ice_sdp), "%s%c", ULNET_NAT_SIGNAL_PREFIX, kind);
    }
    return agent->session->sam2_send_callback(agent->session->user_ptr, (char *)&response);
}

static int ulnet__nat_ipv4_is_usable(const struct in_addr *addr) {
    uint32_t ip = ntohl(addr->s_addr);
    return ip != 0 /* IN4_IS_ADDR_UNSPECIFIED */
        && (ip >> 24) != 127 /* IN4_IS_ADDR_LOOPBACK */
        && (ip >> 16) != 0xa9fe /* IN4_IS_ADDR_LINKLOCAL */;
}

static int ulnet__nat_ipv6_is_usable(const struct in6_addr *addr) {
    return !IN6_IS_ADDR_UNSPECIFIED(addr)
        && !IN6_IS_ADDR_LOOPBACK(addr)
        && !IN6_IS_ADDR_LINKLOCAL(addr)
        && !IN6_IS_ADDR_SITELOCAL(addr);
}

typedef struct ulnet_nat_host_candidate_state {
    struct sockaddr_storage seen[ULNET_NAT_CANDIDATES_MAX];
    int seen_count;
    int sent;
} ulnet_nat_host_candidate_state_t;

static int ulnet__addr_normalize(const struct sockaddr *addr, uint16_t force_port, struct sockaddr_storage *out) {
    if (!addr) return -1;
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *a4 = (const struct sockaddr_in *)addr;
        if (!ulnet__nat_ipv4_is_usable(&a4->sin_addr)) return -1;
        ulnet__addr_from_ipv4_mapped(out, a4->sin_addr.s_addr, force_port);
        return 0;
    } else if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)addr;
        if (!ulnet__nat_ipv6_is_usable(&a6->sin6_addr)) return -1;
        memset(out, 0, sizeof(*out));
        struct sockaddr_in6 *out6 = (struct sockaddr_in6 *)out;
        out6->sin6_family = AF_INET6;
        out6->sin6_port = htons(force_port);
        out6->sin6_addr = a6->sin6_addr;
        out6->sin6_scope_id = a6->sin6_scope_id;
        return 0;
    }
    return -1;
}

static int ulnet__nat_send_host_candidate_addr(ulnet_nat_agent_t *agent, uint16_t port,
    const struct sockaddr *addr, ulnet_nat_host_candidate_state_t *state) {
    struct sockaddr_storage norm_addr;
    if (ulnet__addr_normalize(addr, port, &norm_addr) != 0) {
        return 0;
    }

    for (int i = 0; i < state->seen_count; i++) {
        if (ulnet__addr_equal(&state->seen[i], &norm_addr)) {
            return 0;
        }
    }

    if (state->seen_count < ULNET_NAT_CANDIDATES_MAX) {
        state->seen[state->seen_count++] = norm_addr;
    }

    char host[INET6_ADDRSTRLEN];
    uint16_t out_port = 0;
    if (ulnet__format_addr(&norm_addr, host, sizeof(host), &out_port) == 0) {
        if (ulnet__nat_send_signal(agent, ULNET_NAT_SIGNAL_CANDIDATE, host, out_port) == 0) {
            state->sent++;
        }
    }
    return 0;
}

static int ulnet__nat_send_host_candidates(ulnet_nat_agent_t *agent) {
    struct sockaddr_storage local_addr;
    socklen_t local_addr_len = sizeof(local_addr);
    char bound_host[INET6_ADDRSTRLEN];
    uint16_t port = 0;

    if (getsockname(agent->socket, (struct sockaddr *)&local_addr, &local_addr_len) != 0) {
        return -1;
    }

    if (ulnet__format_addr(&local_addr, bound_host, sizeof(bound_host), &port) != 0) {
        return -1;
    }

    if (strcmp(bound_host, "0.0.0.0") != 0 && strcmp(bound_host, "::") != 0) {
        return ulnet__nat_send_signal(agent, ULNET_NAT_SIGNAL_CANDIDATE, bound_host, port);
    }

    ulnet_nat_host_candidate_state_t state;
    memset(&state, 0, sizeof(state));

#ifdef _WIN32
    union {
        SOCKET_ADDRESS_LIST list;
        char bytes[16 * 1024];
    } buffer;
    DWORD bytes = 0;
    SOCKET_ADDRESS_LIST *addresses = &buffer.list;

    if (WSAIoctl(agent->socket, SIO_ADDRESS_LIST_QUERY, NULL, 0,
        buffer.bytes, sizeof(buffer.bytes), &bytes, NULL, NULL) != 0) {
        return -1;
    }

    for (int i = 0; i < addresses->iAddressCount; i++) {
        ulnet__nat_send_host_candidate_addr(agent, port, addresses->Address[i].lpSockaddr, &state);
    }
#else
    struct ifaddrs *ifas = NULL;
    if (getifaddrs(&ifas) != 0) {
        return -1;
    }

    for (struct ifaddrs *ifa = ifas; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (!(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK)) continue;
        ulnet__nat_send_host_candidate_addr(agent, port, ifa->ifa_addr, &state);
    }

    freeifaddrs(ifas);
#endif
    return state.sent;
}

static int ulnet__nat_send_stun_request(ulnet_nat_agent_t *agent) {
    if (agent->stun_server_addr_len == 0) {
        return -1;
    }

    uint8_t request[20];
    memset(request, 0, sizeof(request));
    ulnet__write_be16(request, ULNET__STUN_BINDING_REQUEST);
    ulnet__write_be32(request + 4, ULNET__STUN_MAGIC_COOKIE);
    ulnet__nat_random_transaction_id(agent, agent->stun_transaction_id);
    memcpy(request + 8, agent->stun_transaction_id, sizeof(agent->stun_transaction_id));

    int ret = (int)sendto(agent->socket, (const char *)request, sizeof(request), 0,
        (const struct sockaddr *)&agent->stun_server_addr, agent->stun_server_addr_len);
    if (ret == (int)sizeof(request)) {
        agent->last_stun_time_usec = ulnet__get_unix_time_microseconds();
        return 0;
    }

    return -1;
}

static int ulnet__nat_send_peer_stun_request(ulnet_nat_agent_t *agent, ulnet_nat_candidate_t *candidate) {
    uint8_t request[20];
    memset(request, 0, sizeof(request));
    ulnet__write_be16(request, ULNET__STUN_BINDING_REQUEST);
    ulnet__write_be32(request + 4, ULNET__STUN_MAGIC_COOKIE);
    memcpy(request + 8, candidate->transaction_id, sizeof(candidate->transaction_id));

    int ret = (int)sendto(agent->socket, (const char *)request, sizeof(request), 0,
        (const struct sockaddr *)&candidate->addr, candidate->addr_len);
    ulnet__nat_debug_addr(ret == (int)sizeof(request) ? "Sent peer STUN request to" : "Failed peer STUN request to", &candidate->addr);
    return ret == (int)sizeof(request) ? 0 : -1;
}

static int ulnet__nat_send_stun_response(ulnet_nat_agent_t *agent, const uint8_t *request, const struct sockaddr_storage *to, socklen_t to_len) {
    uint8_t response[44];
    memset(response, 0, sizeof(response));
    ulnet__write_be16(response + 0, ULNET__STUN_BINDING_RESPONSE);
    ulnet__write_be32(response + 4, ULNET__STUN_MAGIC_COOKIE);
    memcpy(response + 8, request + 8, 12); // Trans ID
    ulnet__write_be16(response + 20, ULNET__STUN_ATTR_XOR_MAPPED_ADDRESS);

    int response_size = 0;
    if (to->ss_family == AF_INET) {
        const struct sockaddr_in *to4 = (const struct sockaddr_in *)to;
        uint16_t xport = ntohs(to4->sin_port) ^ (uint16_t)(ULNET__STUN_MAGIC_COOKIE >> 16);
        uint32_t xaddr = ntohl(to4->sin_addr.s_addr) ^ ULNET__STUN_MAGIC_COOKIE;

        ulnet__write_be16(response + 2, 12);  // Msg length
        ulnet__write_be16(response + 22, 8);  // Attr length
        response[25] = 0x01;                  // Family ipv4
        ulnet__write_be16(response + 26, xport);
        ulnet__write_be32(response + 28, xaddr);
        response_size = 32;
    } else if (to->ss_family == AF_INET6) {
        const struct sockaddr_in6 *to6 = (const struct sockaddr_in6 *)to;
        uint16_t xport = ntohs(to6->sin6_port) ^ (uint16_t)(ULNET__STUN_MAGIC_COOKIE >> 16);

        if (IN6_IS_ADDR_V4MAPPED(&to6->sin6_addr)) {
            uint32_t raw_ip = ulnet__read_be32(&to6->sin6_addr.s6_addr[12]);
            uint32_t xaddr = raw_ip ^ ULNET__STUN_MAGIC_COOKIE;

            ulnet__write_be16(response + 2, 12);  // Msg length
            ulnet__write_be16(response + 22, 8);  // Attr length
            response[25] = 0x01;                  // Family ipv4
            ulnet__write_be16(response + 26, xport);
            ulnet__write_be32(response + 28, xaddr);
            response_size = 32;
        } else {
            ulnet__write_be16(response + 2, 24);  // Msg length
            ulnet__write_be16(response + 22, 20); // Attr length
            response[25] = 0x02;                  // Family ipv6
            ulnet__write_be16(response + 26, xport);
            for (int i = 0; i < 16; i++) {
                response[28 + i] = to6->sin6_addr.s6_addr[i] ^ request[4 + i];
            }
            response_size = 44;
        }
    } else {
        return -1;
    }

    int ret = (int)sendto(agent->socket, (const char *)response, response_size, 0, (const struct sockaddr *)to, to_len);
    ulnet__nat_debug_addr(ret == response_size ? "Sent peer STUN response to" : "Failed peer STUN response to", to);
    return ret == response_size ? 0 : -1;
}

static int ulnet__nat_process_stun_response(ulnet_nat_agent_t *agent, const uint8_t *packet, int size,
    const struct sockaddr_storage *from, socklen_t from_len) {
    if (size < 20
        || ulnet__read_be16(packet) != ULNET__STUN_BINDING_RESPONSE
        || ulnet__read_be32(packet + 4) != ULNET__STUN_MAGIC_COOKIE) {
        return 0;
    }

    int server_response = memcmp(packet + 8, agent->stun_transaction_id, sizeof(agent->stun_transaction_id)) == 0;
    ulnet_nat_candidate_t *peer_response_candidate = ulnet__nat_find_candidate_from_transaction_id(agent, packet + 8);
    if (!server_response && !peer_response_candidate) {
        return 1;
    }

    int message_len = ulnet__read_be16(packet + 2);
    int offset = 20;
    int end = SAM2_MIN(size, 20 + message_len);

    while (offset + 4 <= end) {
        uint16_t attr_type = ulnet__read_be16(packet + offset);
        uint16_t attr_len = ulnet__read_be16(packet + offset + 2);
        const uint8_t *attr = packet + offset + 4;
        int padded_len = (attr_len + 3) & ~3;

        if (offset + 4 + attr_len > end) {
            return 1;
        }

        if (attr_type == ULNET__STUN_ATTR_XOR_MAPPED_ADDRESS && attr_len >= 8) {
            char host[INET6_ADDRSTRLEN];
            uint16_t port = (uint16_t)(ulnet__read_be16(attr + 2) ^ (ULNET__STUN_MAGIC_COOKIE >> 16));
            const char *mapped = NULL;

            if (attr[1] == 0x01 && attr_len >= 8) {
                struct in_addr addr4;
                addr4.s_addr = htonl(ulnet__read_be32(attr + 4) ^ ULNET__STUN_MAGIC_COOKIE);
                mapped = inet_ntop(AF_INET, &addr4, host, sizeof(host));
            } else if (attr[1] == 0x02 && attr_len >= 20) {
                struct in6_addr addr6;
                for (int i = 0; i < 16; i++) {
                    addr6.s6_addr[i] = attr[4 + i] ^ packet[4 + i];
                }
                mapped = inet_ntop(AF_INET6, &addr6, host, sizeof(host));
            }

            if (mapped) {
                if (server_response) {
                    SAM2_LOG_INFO("STUN mapped UDP candidate %s:%u", host, (unsigned)port);
                    ulnet__nat_send_signal(agent, ULNET_NAT_SIGNAL_CANDIDATE, host, port);
                    agent->stun_candidate_sent = 1;
                } else {
                    ulnet__nat_debug_addr("Peer STUN response from", from);
                }
            }
            if (peer_response_candidate) {
                peer_response_candidate->next_check_time_usec = 0;
                ulnet__nat_add_candidate(agent, from, from_len);
                ulnet__nat_select_candidate(agent, from, from_len);
            }
            return 1;
        }

        offset += 4 + padded_len;
    }

    return 1;
}

static int ulnet__nat_process_stun_request(ulnet_nat_agent_t *agent, const uint8_t *packet, int size,
    const struct sockaddr_storage *from, socklen_t from_len) {
    if (size < 20
        || ulnet__read_be16(packet) != ULNET__STUN_BINDING_REQUEST
        || ulnet__read_be32(packet + 4) != ULNET__STUN_MAGIC_COOKIE) {
        return 0;
    }

    ulnet__nat_add_candidate(agent, from, from_len);
    ulnet__nat_send_stun_response(agent, packet, from, from_len);
    if (agent->state < ULNET_NAT_STATE_READY) {
        ulnet__nat_send_control(agent, ULNET_NAT_PROBE, sizeof(ULNET_NAT_PROBE) - 1, from, from_len);
    }
    return 1;
}

static void ulnet__nat_destroy(ulnet_nat_agent_t *agent) {
    if (agent) {
        if (agent->socket != ULNET_SOCKET_INVALID) {
            ULNET_CLOSESOCKET(agent->socket);
        }
#ifdef _WIN32
        WSACleanup();
#endif
        ULNET_FREE(agent);
    }
}

ULNET_LINKAGE ulnet_nat_agent_t *ulnet__nat_create(ulnet_session_t *session, int peer_port) {
    ulnet_nat_agent_t *agent = (ulnet_nat_agent_t *)ULNET_MALLOC(sizeof(*agent));
    if (!agent) return NULL;
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        ULNET_FREE(agent);
        return NULL;
    }
#endif

    memset(agent, 0, sizeof(*agent));
    agent->socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    agent->state = ULNET_NAT_STATE_CONNECTING;
    agent->session = session;
    agent->peer_port = peer_port;
    agent->rng_state = (uint32_t)ulnet__get_unix_time_microseconds() ^ (uint32_t)(uintptr_t)agent ^ ((uint32_t)peer_port << 16);
    agent->connect_deadline_usec = ulnet__get_unix_time_microseconds() + ULNET_NAT_CONNECT_TIMEOUT_USEC;
    int v6only = 0;
    struct sockaddr_in6 bind_addr;

    if (agent->socket == ULNET_SOCKET_INVALID) {
        goto err;
    }

    setsockopt(agent->socket, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&v6only, sizeof(v6only));
    ulnet__enable_kernel_rx_timestamps(agent->socket);

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin6_family = AF_INET6;
    bind_addr.sin6_addr = in6addr_any;
    bind_addr.sin6_port = htons(0);

    if (bind(agent->socket, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0 || ulnet__set_nonblocking(agent->socket) != 0) {
        goto err;
    }

    if (session->nat_stun_host[0] != '\0'
        && ulnet__parse_addr(session->nat_stun_host, session->nat_stun_port, &agent->stun_server_addr, &agent->stun_server_addr_len) == 0) {
        SAM2_LOG_INFO("Using STUN server %s:%u", session->nat_stun_host, (unsigned)session->nat_stun_port);
        ulnet__nat_send_stun_request(agent);
    }

    return agent;

err:ulnet__nat_destroy(agent);
    return NULL;
}

ULNET_LINKAGE ulnet_nat_state_t ulnet_nat_get_state(ulnet_nat_agent_t *agent) {
    return agent ? agent->state : ULNET_NAT_STATE_DISCONNECTED;
}

static bool ulnet__peer_link_ready(ulnet_session_t *session, int p) {
    if (!session->agent[p]) return false;
    if (session->use_inproc_transport) return true;
    return ulnet_nat_get_state(session->agent[p]) == ULNET_NAT_STATE_READY;
}

ULNET_LINKAGE const char *ulnet_nat_state_to_string(ulnet_nat_state_t state) {
    switch (state) {
    case ULNET_NAT_STATE_DISCONNECTED: return "DISCONNECTED";
    case ULNET_NAT_STATE_CONNECTING:   return "CONNECTING";
    case ULNET_NAT_STATE_READY:        return "READY";
    case ULNET_NAT_STATE_FAILED:       return "FAILED";
    default:                           return "UNKNOWN";
    }
}

ULNET_LINKAGE void ulnet_set_stun_server(ulnet_session_t *session, const char *host, uint16_t port) {
    if (!session) return;

    session->nat_stun_host[0] = '\0';
    if (host) {
        strncpy(session->nat_stun_host, host, sizeof(session->nat_stun_host) - 1);
        session->nat_stun_host[sizeof(session->nat_stun_host) - 1] = '\0';
    }
    session->nat_stun_port = port;
}

static int ulnet__nat_process_signal(ulnet_nat_agent_t *agent, const char *signal) {
    if (!signal || strncmp(signal, ULNET_NAT_SIGNAL_PREFIX, strlen(ULNET_NAT_SIGNAL_PREFIX)) != 0) {
        return -1;
    }

    char kind = signal[4];
    if (kind == 'D') {
        return 0;
    }

    if (kind == ULNET_NAT_SIGNAL_CANDIDATE) {
        char host[INET6_ADDRSTRLEN];
        unsigned port = 0;
        struct sockaddr_storage addr;
        socklen_t addr_len = 0;
        if (sscanf(signal + 5, " %45s %u", host, &port) != 2 || port > 65535) {
            return -1;
        }
        if (ulnet__parse_addr(host, (uint16_t)port, &addr, &addr_len) != 0) {
            return -1;
        }
        return ulnet__nat_add_candidate(agent, &addr, addr_len) == 0
            ? 0
            : -1;
    }

    return -1;
}

static int ulnet__nat_send_control(ulnet_nat_agent_t *agent, const char *control, size_t size, const struct sockaddr_storage *addr, socklen_t addr_len) {
    int ret = (int)sendto(agent->socket, control, (int)size, 0, (const struct sockaddr *)addr, addr_len);
    ulnet__nat_debug_addr(ret == (int)size ? "Sent NAT control to" : "Failed NAT control send to", addr);
    return ret == (int)size ? 0 : -1;
}

static int ulnet__nat_send(ulnet_nat_agent_t *agent, const uint8_t *packet, size_t size) {
    if (!agent || agent->selected_addr_len == 0) {
        return -1;
    }

    int ret = (int)sendto(agent->socket, (const char *)packet, (int)size, 0,
        (const struct sockaddr *)&agent->selected_addr, agent->selected_addr_len);
    return ret == (int)size ? 0 : -1;
}

static void ulnet__nat_poll_agent(ulnet_nat_agent_t *agent) {
    if (!agent) return;

    int64_t now = ulnet__get_unix_time_microseconds();
    if (   agent->stun_server_addr_len != 0
        && !agent->stun_candidate_sent
        && agent->state < ULNET_NAT_STATE_READY
        && now - agent->last_stun_time_usec > 500000) {
        ulnet__nat_send_stun_request(agent);
    }

    int checks_timed_out = agent->state < ULNET_NAT_STATE_READY
        && agent->connect_deadline_usec != 0
        && now > agent->connect_deadline_usec;
    if (checks_timed_out) {
        SAM2_LOG_ERROR("NAT traversal timed out for peer %05" PRId16 "; TODO rollback peer-to-peer room transition",
            agent->session ? agent->session->room_we_are_in.peer_ids[agent->peer_port] : 0);
        ulnet__nat_set_state(agent, ULNET_NAT_STATE_FAILED);
    } else if (agent->state < ULNET_NAT_STATE_READY) {
        for (int i = 0; i < agent->candidate_count; i++) {
            ulnet_nat_candidate_t *candidate = &agent->candidate[i];
            if (candidate->next_check_time_usec > now) {
                continue;
            }

            ulnet__nat_random_transaction_id(agent, candidate->transaction_id);
            ulnet__nat_send_peer_stun_request(agent, candidate);
            ulnet__nat_send_control(agent, ULNET_NAT_PROBE, sizeof(ULNET_NAT_PROBE) - 1, &candidate->addr, candidate->addr_len);
            candidate->next_check_time_usec = now + ULNET_NAT_CHECK_PACING_USEC;
        }
    }

    for (;;) {
        char packet[ULNET_NAT_POLL_PACKET_MAX];
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);
        int64_t kernel_receive_time_usec = 0;
        int ret = ulnet__recvfrom_with_timestamp(agent->socket, packet, sizeof(packet), &from, &from_len, &kernel_receive_time_usec);
        if (ret < 0) {
            int err = ULNET_SOCKERRNO;
            if (ulnet__socket_would_block()) {
                break;
            }
#ifdef _WIN32
            if (err == WSAECONNRESET || err == WSAENETRESET || err == WSAECONNREFUSED) {
                break;
            }
#else
            if (err == EINTR) {
                continue;
            }
            if (err == ECONNREFUSED || err == ECONNRESET) {
                break;
            }
#endif
            ulnet__nat_set_state(agent, ULNET_NAT_STATE_FAILED);
            break;
        }
        if (ret == 0) {
            break;
        }

        if (ulnet__nat_process_stun_request(agent, (const uint8_t *)packet, ret, &from, from_len)) {
            continue;
        }

        if (ulnet__nat_process_stun_response(agent, (const uint8_t *)packet, ret, &from, from_len)) {
            continue;
        }

        ulnet__nat_add_candidate(agent, &from, from_len);
        ulnet__nat_debug_addr("Received NAT packet from", &from);

        if (ret == (int)(sizeof(ULNET_NAT_PROBE) - 1) && memcmp(packet, ULNET_NAT_PROBE, sizeof(ULNET_NAT_PROBE) - 1) == 0) {
            ulnet__nat_send_control(agent, ULNET_NAT_PROBE_ACK, sizeof(ULNET_NAT_PROBE_ACK) - 1, &from, from_len);
            if (agent->state < ULNET_NAT_STATE_READY) {
                ulnet__nat_send_control(agent, ULNET_NAT_PROBE, sizeof(ULNET_NAT_PROBE) - 1, &from, from_len);
            }
            continue;
        }
        if (ret == (int)(sizeof(ULNET_NAT_PROBE_ACK) - 1) && memcmp(packet, ULNET_NAT_PROBE_ACK, sizeof(ULNET_NAT_PROBE_ACK) - 1) == 0) {
            ulnet__nat_select_candidate(agent, &from, from_len);
            continue;
        }

        if (agent->state < ULNET_NAT_STATE_READY) {
            ulnet__nat_select_candidate(agent, &from, from_len);
        }
        agent->session->pending_packet_kernel_receive_unix_usec = kernel_receive_time_usec;
        ulnet_receive_packet_callback(agent, packet, (size_t)ret, agent->session);
    }
}
#endif

#define UZSTD_IMPLEMENTATION
#if defined(UZSTD_IMPLEMENTATION) && (!defined(UZSTD_NO_COMPRESSOR) || !defined(UZSTD_NO_DECOMPRESSOR))

#ifndef UZSTD_MALLOC
#include <stdlib.h>
#define UZSTD_MALLOC(sz) malloc(sz)
#endif
#ifndef UZSTD_FREE
#include <stdlib.h>
#define UZSTD_FREE(p) free(p)
#endif
#ifndef UZSTD_MEMCPY
#include <string.h>
#define UZSTD_MEMCPY(dst, src, n) memcpy((dst), (src), (n))
#endif
#ifndef UZSTD_MEMSET
#include <string.h>
#define UZSTD_MEMSET(dst, c, n) memset((dst), (c), (n))
#endif
#ifndef UZSTD_MEMCMP
#include <string.h>
#define UZSTD_MEMCMP(a, b, n) memcmp((a), (b), (n))
#endif

#ifndef UZSTD_COMMON_C
#define UZSTD_COMMON_C

typedef unsigned char      uzstd__u8;
typedef unsigned short     uzstd__u16;
typedef unsigned int       uzstd__u32;
typedef unsigned long long uzstd__u64;

static int uzstd__highbit(uzstd__u32 v) { int n=0; while (v>1) { v>>=1; n++; } return n; } /* v>0 */


/* ---------------- sequence codes (RFC 8878 tables) ---------------- */
static const uzstd__u32 uzstd__llbase[36] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,18,20,22,24,28,
    32,40,48,64,128,256,512,1024,2048,4096,8192,16384,32768,65536};
static const uzstd__u8 uzstd__llbits[36] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,4,6,7,8,9,10,11,12,13,14,15,16};
/* match length tables in terms of mlv = match_length - 3 */
static const uzstd__u32 uzstd__mlbase[53] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
    26,27,28,29,30,31,32,34,36,38,40,44,48,56,64,80,96,128,256,512,1024,2048,4096,8192,16384,32768,65536};
static const uzstd__u8 uzstd__mlbits[53] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,1,1,1,1,2,2,3,3,4,4,5,7,8,9,10,11,12,13,14,15,16};


#endif /* UZSTD_COMMON_C */

#if !defined(UZSTD_NO_COMPRESSOR)
#ifndef UZSTD_COMPRESSOR_C
#define UZSTD_COMPRESSOR_C

static uzstd__u32 uzstd__read32(const void *p) { uzstd__u32 v; UZSTD_MEMCPY(&v, p, 4); return v; }
static void uzstd__wle16(uzstd__u8 *p, uzstd__u32 v) { p[0]=(uzstd__u8)v; p[1]=(uzstd__u8)(v>>8); }
static void uzstd__wle32(uzstd__u8 *p, uzstd__u32 v) { uzstd__wle16(p,v); uzstd__wle16(p+2,v>>16); }

/* ---------------- backward bitstream writer (FSE/Huffman streams) ---------------- */
typedef struct { uzstd__u64 acc; int cnt; uzstd__u8 *start, *p, *end; } uzstd__bw;

static void uzstd__bw_init(uzstd__bw *b, uzstd__u8 *p, uzstd__u8 *end) {
    b->acc = 0; b->cnt = 0; b->start = b->p = p; b->end = end;
}
static void uzstd__bw_add(uzstd__bw *b, uzstd__u32 v, int nb) { /* nb <= 31 */
    b->acc |= (uzstd__u64)(v & ((1u<<nb)-1)) << b->cnt; b->cnt += nb;
    while (b->cnt >= 8) { if (b->p < b->end) *b->p = (uzstd__u8)b->acc; b->p++; b->acc >>= 8; b->cnt -= 8; }
}
/* terminate with marker bit; returns stream size, 0 on overflow */
static size_t uzstd__bw_close(uzstd__bw *b) {
    uzstd__bw_add(b, 1, 1);
    if (b->cnt) { if (b->p < b->end) *b->p = (uzstd__u8)b->acc; b->p++; }
    return b->p > b->end ? 0 : (size_t)(b->p - b->start);
}

/* ---------------- FSE ---------------- */
typedef struct { uzstd__u16 next[1<<9]; uzstd__u32 dnb[64]; int dfs[64]; int log; } uzstd__fse_ct;
typedef struct { uzstd__u32 v; } uzstd__fse_cs;

/* normalized counts (sum = 1<<log, all >= 1 where cnt > 0); requires (1<<log) >= distinct */
static void uzstd__fse_norm(const uzstd__u32 *cnt, int max_sym, uzstd__u32 total, int log, short *norm) {
    int s, larg = 0, diff; uzstd__u32 sum = 0;
    for (s = 0; s <= max_sym; s++) {
        uzstd__u32 nv = cnt[s] ? (uzstd__u32)(((uzstd__u64)cnt[s] << log) / total) : 0;
        if (cnt[s] && !nv) nv = 1;
        norm[s] = (short)nv; sum += nv;
        if (cnt[s] > cnt[larg]) larg = s;
    }
    diff = (1<<log) - (int)sum;
    if (diff > 0) norm[larg] = (short)(norm[larg] + diff);
    while (diff < 0) { /* shave the biggest */
        int big = larg;
        for (s = 0; s <= max_sym; s++) if (norm[s] > norm[big]) big = s;
        norm[big]--; diff++;
    }
}

/* serialize normalized counts; returns bytes written, 0 on overflow */
static size_t uzstd__fse_write_ncount(uzstd__u8 *out, size_t cap, const short *norm, int log) {
    uzstd__u8 *d = out, *dend = out + cap;
    int tsize = 1<<log, remaining = tsize+1, threshold = tsize, nb_bits = log+1;
    uzstd__u32 bits = (uzstd__u32)(log - 5); int bcnt = 4, sym = 0, prev0 = 0;
    while (remaining > 1) {
        if (prev0) {
            int start = sym;
            while (!norm[sym]) sym++;
            while (sym >= start+24) {
                start += 24; bits += 0xFFFFu << bcnt;
                if (d > dend-2) return 0;
                d[0]=(uzstd__u8)bits; d[1]=(uzstd__u8)(bits>>8); d += 2; bits >>= 16;
            }
            while (sym >= start+3) { start += 3; bits += 3u << bcnt; bcnt += 2; }
            bits += (uzstd__u32)(sym - start) << bcnt; bcnt += 2;
            if (bcnt > 16) {
                if (d > dend-2) return 0;
                d[0]=(uzstd__u8)bits; d[1]=(uzstd__u8)(bits>>8); d += 2; bits >>= 16; bcnt -= 16;
            }
        }
        {   int count = norm[sym++], max = (2*threshold-1) - remaining;
            remaining -= count < 0 ? -count : count;
            count++;
            if (count >= threshold) count += max;
            bits += (uzstd__u32)count << bcnt;
            bcnt += nb_bits;
            bcnt -= (count < max);
            prev0 = (count == 1);
            while (remaining < threshold) { nb_bits--; threshold >>= 1; }
        }
        if (bcnt > 16) {
            if (d > dend-2) return 0;
            d[0]=(uzstd__u8)bits; d[1]=(uzstd__u8)(bits>>8); d += 2; bits >>= 16; bcnt -= 16;
        }
    }
    if (d > dend-2) return 0;
    d[0]=(uzstd__u8)bits; d[1]=(uzstd__u8)(bits>>8);
    return (size_t)(d - out) + (size_t)((bcnt+7)/8);
}

static void uzstd__fse_build_ct(uzstd__fse_ct *ct, const short *norm, int max_sym, int log) {
    int tsize = 1<<log, step = (tsize>>1)+(tsize>>3)+3, mask = tsize-1;
    uzstd__u8 tsym[1<<9]; uzstd__u32 cumul[64];
    int s, total = 0; uzstd__u32 pos = 0, u;
    ct->log = log;
    cumul[0] = 0;
    for (s = 0; s <= max_sym; s++) {
        int k; cumul[s+1] = cumul[s] + (uzstd__u32)norm[s];
        for (k = 0; k < norm[s]; k++) { tsym[pos] = (uzstd__u8)s; pos = (pos + (uzstd__u32)step) & (uzstd__u32)mask; }
    }
    for (u = 0; u < (uzstd__u32)tsize; u++) { s = tsym[u]; ct->next[cumul[s]++] = (uzstd__u16)(tsize + u); }
    for (s = 0; s <= max_sym; s++) {
        if (norm[s] == 0)      { ct->dnb[s] = ((uzstd__u32)(log+1)<<16) - (1u<<log); ct->dfs[s] = 0; }
        else if (norm[s] == 1) { ct->dnb[s] = ((uzstd__u32)log<<16) - (1u<<log); ct->dfs[s] = total - 1; total += 1; }
        else {
            int mb = log - uzstd__highbit((uzstd__u32)norm[s]-1);
            ct->dnb[s] = ((uzstd__u32)mb<<16) - ((uzstd__u32)norm[s]<<mb);
            ct->dfs[s] = total - norm[s]; total += norm[s];
        }
    }
}
static void uzstd__fse_rle_ct(uzstd__fse_ct *ct) { /* degenerate 0-bit table */
    UZSTD_MEMSET(ct, 0, sizeof *ct);
}
static void uzstd__fse_init_state(uzstd__fse_cs *st, const uzstd__fse_ct *ct, int s) {
    uzstd__u32 nb = (ct->dnb[s] + (1u<<15)) >> 16;
    st->v = ct->next[(((nb<<16) - ct->dnb[s]) >> nb) + (uzstd__u32)ct->dfs[s]];
}
static void uzstd__fse_encode(uzstd__bw *b, uzstd__fse_cs *st, const uzstd__fse_ct *ct, int s) {
    uzstd__u32 nb = (st->v + ct->dnb[s]) >> 16;
    uzstd__bw_add(b, st->v, (int)nb);
    st->v = ct->next[(st->v >> nb) + (uzstd__u32)ct->dfs[s]];
}
static void uzstd__fse_flush_state(uzstd__bw *b, const uzstd__fse_cs *st, const uzstd__fse_ct *ct) {
    uzstd__bw_add(b, st->v, ct->log);
}

static int uzstd__llcode(uzstd__u32 v) {
    return v<16 ? (int)v : v<24 ? 16+(int)((v-16)>>1) : v<32 ? 20+(int)((v-24)>>2)
         : v<48 ? 22+(int)((v-32)>>3) : v<64 ? 24 : 19+uzstd__highbit(v);
}
static int uzstd__mlcode(uzstd__u32 m) { /* m = match_length - 3 */
    return m<32 ? (int)m : m<40 ? 32+(int)((m-32)>>1) : m<48 ? 36+(int)((m-40)>>2)
         : m<64 ? 38+(int)((m-48)>>3) : m<96 ? 40+(int)((m-64)>>4) : m<128 ? 42 : 36+uzstd__highbit(m);
}


/* ---------------- compression context ---------------- */
#define UZSTD__BLOCK_MAX  (128*1024)
#define UZSTD__MAX_SEQ    (UZSTD__BLOCK_MAX/3 + 8)
#define UZSTD__HASH_LOG   17
#define UZSTD__TMP_CAP    (UZSTD__BLOCK_MAX + 8192)

typedef struct { uzstd__u32 ll, ml, ov; } uzstd__seq; /* lit len, match len, offset value */
typedef struct {
    uzstd__u32 *htab, *chain;
    uzstd__seq *seqs;
    uzstd__u8  *lit, *tmp;
    uzstd__u32 rep[3];
    int nseq; uzstd__u32 nlit;
    int depth, lazy;
} uzstd__ctx;

static uzstd__u32 uzstd__hash(uzstd__u32 v) { return (v * 2654435761u) >> (32 - UZSTD__HASH_LOG); }

static uzstd__u32 uzstd__mlen(const uzstd__u8 *a, const uzstd__u8 *b, const uzstd__u8 *end) {
    const uzstd__u8 *bs = b; /* count match length of b vs a, b limited by end */
    while (b+4 <= end && uzstd__read32(a) == uzstd__read32(b)) { a += 4; b += 4; }
    while (b < end && *a == *b) { a++; b++; }
    return (uzstd__u32)(b - bs);
}

/* search best match at i (must already be inserted in chain); returns length (0 = none) */
static uzstd__u32 uzstd__find(uzstd__ctx *cx, const uzstd__u8 *base, size_t i, size_t end, uzstd__u32 *odist) {
    uzstd__u32 v = uzstd__read32(base+i);
    uzstd__u32 rep_len = 0, rep_dist = 0, cb = 3, cb_dist = 0, m;
    int r, att = cx->depth;
    for (r = 0; r < 3; r++) {
        uzstd__u32 d = cx->rep[r], x;
        if (d > i) continue;
        x = uzstd__read32(base+i-d) ^ v;
        if (!x) {
            uzstd__u32 L = 4 + uzstd__mlen(base+i-d+4, base+i+4, base+end);
            if (L > rep_len) { rep_len = L; rep_dist = d; }
        } else if (rep_len < 3 && !UZSTD_MEMCMP(base+i-d, base+i, 3)) { rep_len = 3; rep_dist = d; }
    }
    m = cx->chain[i];
    while (m && att-- > 0 && i + cb < end) {
        size_t cand = m - 1;
        if (base[cand+cb] == base[i+cb] && uzstd__read32(base+cand) == v) {
            uzstd__u32 L = 4 + uzstd__mlen(base+cand+4, base+i+4, base+end);
            if (L > cb) { cb = L; cb_dist = (uzstd__u32)(i - cand); if (L >= 128) break; }
        }
        m = cx->chain[cand];
    }
    if (rep_len >= 3 && rep_len + 3 > cb) { *odist = rep_dist; return rep_len; }
    if (cb >= 4 && cb_dist) { *odist = cb_dist; return cb; }
    return 0;
}

/* map distance -> offset value + update repcodes */
static uzstd__u32 uzstd__offval(uzstd__ctx *cx, uzstd__u32 d, uzstd__u32 ll) {
    uzstd__u32 *R = cx->rep, ov;
    if (ll) {
        if (d == R[0]) return 1;
        else if (d == R[1]) { ov = 2; R[1]=R[0]; R[0]=d; }
        else if (d == R[2]) { ov = 3; R[2]=R[1]; R[1]=R[0]; R[0]=d; }
        else { ov = d + 3;   R[2]=R[1]; R[1]=R[0]; R[0]=d; }
    } else {
        if (d == R[1])      { ov = 1; R[1]=R[0]; R[0]=d; }
        else if (d == R[2]) { ov = 2; R[2]=R[1]; R[1]=R[0]; R[0]=d; }
        else if (d == R[0]-1) { ov = 3; R[2]=R[1]; R[1]=R[0]; R[0]=d; }
        else { ov = d + 3;   R[2]=R[1]; R[1]=R[0]; R[0]=d; }
    }
    return ov;
}

#define UZSTD__INS(p) do { uzstd__u32 h_ = uzstd__hash(uzstd__read32(base+(p))); \
    cx->chain[p] = cx->htab[h_]; cx->htab[h_] = (uzstd__u32)(p)+1; } while (0)

/* parse block [bstart, bstart+bsize) into cx->seqs / cx->lit */
static void uzstd__parse(uzstd__ctx *cx, const uzstd__u8 *base, size_t bstart, size_t bsize) {
    size_t i = bstart, ins = bstart, anchor = bstart, end = bstart + bsize;
    cx->nseq = 0; cx->nlit = 0;
    while (i + 4 <= end) {
        uzstd__u32 len, dist;
        while (ins <= i) { UZSTD__INS(ins); ins++; }
        len = uzstd__find(cx, base, i, end, &dist);
        if (!len) { i++; continue; }
        while (cx->lazy && len < 64 && i + 5 <= end) { /* lazy: try i+1 */
            uzstd__u32 len2, dist2;
            if (ins <= i+1) { UZSTD__INS(ins); ins++; }
            len2 = uzstd__find(cx, base, i+1, end, &dist2);
            if (len2 > len) { i++; len = len2; dist = dist2; } else break;
        }
        while (i > anchor && i > dist && base[i-1] == base[i-dist-1]) { i--; len++; } /* extend back */
        {   uzstd__u32 ll = (uzstd__u32)(i - anchor);
            uzstd__seq *q = &cx->seqs[cx->nseq++];
            UZSTD_MEMCPY(cx->lit + cx->nlit, base + anchor, ll); cx->nlit += ll;
            q->ll = ll; q->ml = len; q->ov = uzstd__offval(cx, dist, ll);
        }
        i += len; anchor = i;
    }
    UZSTD_MEMCPY(cx->lit + cx->nlit, base + anchor, end - anchor); cx->nlit += (uzstd__u32)(end - anchor);
}

/* ---------------- sequences section ---------------- */
/* write table description for one stream; sets *mode, builds ct; 0 = fail */
static size_t uzstd__seq_table(uzstd__u8 *d, size_t cap, const uzstd__u32 *cnt, int max_sym,
                               uzstd__u32 total, int max_log, int *mode, uzstd__fse_ct *ct) {
    short norm[64];
    int s, distinct = 0, last = 0, log;
    size_t sz;
    for (s = 0; s <= max_sym; s++) if (cnt[s]) { distinct++; last = s; }
    if (cap < 8) return 0;
    if (distinct == 1) { *mode = 1; d[0] = (uzstd__u8)last; uzstd__fse_rle_ct(ct); return 1; }
    log = total > 1 ? uzstd__highbit(total-1) - 2 : 5;
    s = uzstd__highbit((uzstd__u32)max_sym) + 2;
    if (log < s) log = s;
    if (log < 5) log = 5;
    if (log > max_log) log = max_log;
    uzstd__fse_norm(cnt, max_sym, total, log, norm);
    sz = uzstd__fse_write_ncount(d, cap, norm, log);
    if (!sz) return 0;
    uzstd__fse_build_ct(ct, norm, max_sym, log);
    *mode = 2;
    return sz;
}

static size_t uzstd__encode_sequences(uzstd__ctx *cx, uzstd__u8 *dst, uzstd__u8 *dend) {
    uzstd__u8 *d = dst, *modep;
    int ns = cx->nseq, n, mll = 0, mof = 0, mml = 0, modes[3];
    uzstd__u32 cll[36], cof[32], cml[53];
    uzstd__fse_ct ctll, ctof, ctml;
    uzstd__bw bw;
    size_t sz;
    if (dend - d < 4) return 0;
    if (ns < 128) *d++ = (uzstd__u8)ns;
    else if (ns < 0x7F00) { d[0] = (uzstd__u8)((ns>>8)+0x80); d[1] = (uzstd__u8)ns; d += 2; }
    else { d[0] = 0xFF; uzstd__wle16(d+1, (uzstd__u32)(ns - 0x7F00)); d += 3; }
    if (!ns) return (size_t)(d - dst);
    UZSTD_MEMSET(cll, 0, sizeof cll); UZSTD_MEMSET(cof, 0, sizeof cof); UZSTD_MEMSET(cml, 0, sizeof cml);
    for (n = 0; n < ns; n++) {
        uzstd__seq q = cx->seqs[n];
        int lc = uzstd__llcode(q.ll), oc = uzstd__highbit(q.ov), mc = uzstd__mlcode(q.ml - 3);
        cll[lc]++; cof[oc]++; cml[mc]++;
        if (lc > mll) mll = lc; if (oc > mof) mof = oc; if (mc > mml) mml = mc;
    }
    modep = d++;
    if (!(sz = uzstd__seq_table(d, (size_t)(dend-d), cll, mll, (uzstd__u32)ns, 9, &modes[0], &ctll))) return 0;
    d += sz;
    if (!(sz = uzstd__seq_table(d, (size_t)(dend-d), cof, mof, (uzstd__u32)ns, 8, &modes[1], &ctof))) return 0;
    d += sz;
    if (!(sz = uzstd__seq_table(d, (size_t)(dend-d), cml, mml, (uzstd__u32)ns, 9, &modes[2], &ctml))) return 0;
    d += sz;
    *modep = (uzstd__u8)((modes[0]<<6) | (modes[1]<<4) | (modes[2]<<2));
    {   uzstd__fse_cs sll, sof, sml;
        uzstd__seq q = cx->seqs[ns-1];
        int lc = uzstd__llcode(q.ll), oc = uzstd__highbit(q.ov), mc = uzstd__mlcode(q.ml - 3);
        uzstd__bw_init(&bw, d, dend);
        uzstd__fse_init_state(&sml, &ctml, mc);
        uzstd__fse_init_state(&sof, &ctof, oc);
        uzstd__fse_init_state(&sll, &ctll, lc);
        uzstd__bw_add(&bw, q.ll - uzstd__llbase[lc], uzstd__llbits[lc]);
        uzstd__bw_add(&bw, q.ml - 3 - uzstd__mlbase[mc], uzstd__mlbits[mc]);
        uzstd__bw_add(&bw, q.ov - (1u<<oc), oc);
        for (n = ns-2; n >= 0; n--) {
            q = cx->seqs[n];
            lc = uzstd__llcode(q.ll); oc = uzstd__highbit(q.ov); mc = uzstd__mlcode(q.ml - 3);
            uzstd__fse_encode(&bw, &sof, &ctof, oc);
            uzstd__fse_encode(&bw, &sml, &ctml, mc);
            uzstd__fse_encode(&bw, &sll, &ctll, lc);
            uzstd__bw_add(&bw, q.ll - uzstd__llbase[lc], uzstd__llbits[lc]);
            uzstd__bw_add(&bw, q.ml - 3 - uzstd__mlbase[mc], uzstd__mlbits[mc]);
            uzstd__bw_add(&bw, q.ov - (1u<<oc), oc);
        }
        uzstd__fse_flush_state(&bw, &sml, &ctml);
        uzstd__fse_flush_state(&bw, &sof, &ctof);
        uzstd__fse_flush_state(&bw, &sll, &ctll);
        if (!(sz = uzstd__bw_close(&bw))) return 0;
        d += sz;
    }
    return (size_t)(d - dst);
}

/* ---------------- Huffman ---------------- */
/* code lengths (<= 11, kraft-complete) via sorted two-queue; returns maxbits, 0 if < 2 symbols */
static int uzstd__huf_lengths(const uzstd__u32 *cnt, int *last_sym, uzstd__u8 *len) {
    uzstd__u64 a[256];
    uzstd__u32 ncnt[512];
    short parent[512], depth[512];
    int m = 0, s, i, k, li, ni, nc, maxbits = 0;
    UZSTD_MEMSET(len, 0, 256);
    for (s = 0; s < 256; s++) if (cnt[s]) { a[m++] = ((uzstd__u64)cnt[s]<<9) | (uzstd__u32)s; *last_sym = s; }
    if (m < 2) return 0;
    for (i = 1; i < m; i++) { /* insertion sort ascending */
        uzstd__u64 v = a[i]; int j = i;
        while (j > 0 && a[j-1] > v) { a[j] = a[j-1]; j--; }
        a[j] = v;
    }
    for (i = 0; i < m; i++) ncnt[i] = (uzstd__u32)(a[i]>>9);
    li = 0; ni = nc = m;
    for (k = 0; k < m-1; k++) {
        int x, y;
        x = (li < m && (ni == nc || ncnt[li] <= ncnt[ni])) ? li++ : ni++;
        y = (li < m && (ni == nc || ncnt[li] <= ncnt[ni])) ? li++ : ni++;
        ncnt[nc] = ncnt[x] + ncnt[y]; parent[x] = (short)nc; parent[y] = (short)nc; nc++;
    }
    depth[nc-1] = 0;
    for (i = nc-2; i >= 0; i--) depth[i] = (short)(depth[parent[i]] + 1);
    for (i = 0; i < m; i++) {
        int L = depth[i] > 11 ? 11 : depth[i];
        len[a[i] & 511] = (uzstd__u8)L;
        if (L > maxbits) maxbits = L;
    }
    if (maxbits == 11) { /* repair kraft sum after capping */
        uzstd__u32 K = 0, C = 1u<<11;
        for (i = 0; i < m; i++) K += 1u << (11 - len[a[i] & 511]);
        while (K > C) /* lengthen rarest */
            for (i = 0; i < m && K > C; i++) {
                uzstd__u8 *L = &len[a[i] & 511];
                if (*L < 11) { K -= 1u << (10 - *L); (*L)++; }
            }
        while (K < C) /* shorten most frequent */
            for (i = m-1; i >= 0 && K < C; i--) {
                uzstd__u8 *L = &len[a[i] & 511];
                if (*L > 1 && K + (1u << (11 - *L)) <= C) { K += 1u << (11 - *L); (*L)--; }
            }
        maxbits = 0;
        for (i = 0; i < m; i++) if (len[a[i] & 511] > maxbits) maxbits = len[a[i] & 511];
    }
    return maxbits;
}

/* canonical codes matching zstd order: weight ascending, symbol ascending */
static int uzstd__huf_canonical(const uzstd__u8 *len, uzstd__u16 *code, int last_sym, int maxbits) {
    uzstd__u32 p = 0; int wv, s;
    for (wv = 1; wv <= maxbits; wv++)
        for (s = 0; s <= last_sym; s++) if (len[s] && maxbits+1-len[s] == wv) {
            code[s] = (uzstd__u16)(p >> (wv-1)); p += 1u << (wv-1);
        }
    return p == (1u<<maxbits);
}

/* tree description: FSE-compressed weights if smaller, else direct nibbles; 0 = fail */
static size_t uzstd__huf_tree(uzstd__u8 *d, size_t cap, const uzstd__u8 *len, int last_sym, int maxbits) {
    uzstd__u8 w[256];
    int s, nw = last_sym; /* explicit weights: symbols 0..last_sym-1, last is implied */
    size_t direct = 1 + (size_t)((nw+1)/2);
    if (cap < direct + 8 || cap < 136) return 0;
    for (s = 0; s < nw; s++) w[s] = len[s] ? (uzstd__u8)(maxbits+1-len[s]) : 0;
    if (nw >= 2) { /* FSE attempt (interleaved 2-state stream, accuracy <= 6) */
        uzstd__u32 cnt[16];
        short norm[16];
        int mw = 0, distinct = 0, log;
        UZSTD_MEMSET(cnt, 0, sizeof cnt);
        for (s = 0; s < nw; s++) cnt[w[s]]++;
        for (s = 0; s < 16; s++) if (cnt[s]) { distinct++; mw = s; }
        if (distinct >= 2) {
            uzstd__fse_ct ct;
            uzstd__bw bw;
            size_t ncsz, bssz;
            log = uzstd__highbit((uzstd__u32)(nw-1)) - 2;
            s = uzstd__highbit((uzstd__u32)mw) + 2;
            if (log < s) log = s;
            if (log < 5) log = 5;
            if (log > 6) log = 6;
            uzstd__fse_norm(cnt, mw, (uzstd__u32)nw, log, norm);
            ncsz = uzstd__fse_write_ncount(d+1, 127, norm, log);
            if (ncsz) {
                uzstd__fse_cs c1, c2;
                int ip = nw;
                uzstd__fse_build_ct(&ct, norm, mw, log);
                uzstd__bw_init(&bw, d+1+ncsz, d+1+127);
                if (nw & 1) {
                    uzstd__fse_init_state(&c1, &ct, w[--ip]);
                    uzstd__fse_init_state(&c2, &ct, w[--ip]);
                    uzstd__fse_encode(&bw, &c1, &ct, w[--ip]);
                } else {
                    uzstd__fse_init_state(&c2, &ct, w[--ip]);
                    uzstd__fse_init_state(&c1, &ct, w[--ip]);
                }
                while (ip > 0) {
                    uzstd__fse_encode(&bw, &c2, &ct, w[--ip]);
                    uzstd__fse_encode(&bw, &c1, &ct, w[--ip]);
                }
                uzstd__fse_flush_state(&bw, &c2, &ct);
                uzstd__fse_flush_state(&bw, &c1, &ct);
                bssz = uzstd__bw_close(&bw);
                if (bssz && ncsz + bssz <= 127 && (nw > 128 || 1 + ncsz + bssz < direct)) {
                    d[0] = (uzstd__u8)(ncsz + bssz);
                    return 1 + ncsz + bssz;
                }
            }
        }
    }
    if (nw > 128) return 0;
    d[0] = (uzstd__u8)(127 + nw);
    UZSTD_MEMSET(d+1, 0, (size_t)((nw+1)/2));
    for (s = 0; s < nw; s++) d[1 + (s>>1)] |= (uzstd__u8)(w[s] << ((s&1) ? 0 : 4));
    return direct;
}

/* one huffman stream (symbols encoded last-to-first); 0 = overflow */
static size_t uzstd__huf_stream(uzstd__u8 *d, uzstd__u8 *dend, const uzstd__u8 *src, size_t n,
                                const uzstd__u16 *code, const uzstd__u8 *len) {
    uzstd__bw bw;
    size_t i = n;
    uzstd__bw_init(&bw, d, dend);
    while (i > 0) { i--; uzstd__bw_add(&bw, code[src[i]], len[src[i]]); }
    return uzstd__bw_close(&bw);
}

/* ---------------- literals section ---------------- */
static size_t uzstd__lit_section(uzstd__ctx *cx, uzstd__u8 *d, size_t cap) {
    uzstd__u32 n = cx->nlit;
    const uzstd__u8 *L = cx->lit;
    size_t rawh = n < 32 ? 1 : n < 4096 ? 2 : 3;
    if (cap < rawh + n + 8) return 0;
    if (n >= 2) {
        uzstd__u32 cnt[256], i;
        uzstd__u16 code[256];
        uzstd__u8 len[256];
        int last_sym = 0, maxbits, distinct = 0;
        UZSTD_MEMSET(cnt, 0, sizeof cnt);
        for (i = 0; i < n; i++) cnt[L[i]]++;
        for (i = 0; i < 256; i++) if (cnt[i]) distinct++;
        if (distinct == 1) { /* RLE literals */
            if (rawh == 1) d[0] = (uzstd__u8)(1u | (n<<3));
            else if (rawh == 2) uzstd__wle16(d, 1u | (1u<<2) | (n<<4));
            else { d[0] = (uzstd__u8)(1u | (3u<<2) | (n<<4)); uzstd__wle16(d+1, n>>4); }
            d[rawh] = L[0];
            return rawh + 1;
        }
        if (n >= 64 && (maxbits = uzstd__huf_lengths(cnt, &last_sym, len)) != 0
            && uzstd__huf_canonical(len, code, last_sym, maxbits)) {
            uzstd__u64 bits = 0;
            for (i = 0; i <= (uzstd__u32)last_sym; i++) bits += (uzstd__u64)cnt[i]*len[i];
            if ((bits>>3) + 32 < n) {
                size_t hsize = n <= 1023 ? 3 : n <= 16383 ? 4 : 5, tsz, csize = 0;
                int four = n > 1023;
                uzstd__u8 *body = d + hsize, *bend = d + cap;
                tsz = uzstd__huf_tree(body, (size_t)(bend - body), len, last_sym, maxbits);
                if (tsz && !four) {
                    size_t ssz = uzstd__huf_stream(body+tsz, bend, L, n, code, len);
                    if (ssz) csize = tsz + ssz;
                } else if (tsz) {
                    uzstd__u32 q = (n+3)/4;
                    uzstd__u8 *p = body + tsz + 6;
                    size_t s1, s2, s3, s4;
                    s1 = uzstd__huf_stream(p, bend, L, q, code, len); p += s1;
                    s2 = s1 ? uzstd__huf_stream(p, bend, L+q, q, code, len) : 0; p += s2;
                    s3 = s2 ? uzstd__huf_stream(p, bend, L+2*q, q, code, len) : 0; p += s3;
                    s4 = s3 ? uzstd__huf_stream(p, bend, L+3*q, n-3*q, code, len) : 0;
                    if (s4 && s1 <= 0xFFFF && s2 <= 0xFFFF && s3 <= 0xFFFF) {
                        uzstd__wle16(body+tsz, (uzstd__u32)s1);
                        uzstd__wle16(body+tsz+2, (uzstd__u32)s2);
                        uzstd__wle16(body+tsz+4, (uzstd__u32)s3);
                        csize = tsz + 6 + s1 + s2 + s3 + s4;
                    }
                }
                if (csize && hsize + csize < rawh + n) {
                    if (hsize == 3) { uzstd__u32 v = 2u | (n<<4) | ((uzstd__u32)csize<<14);
                        d[0]=(uzstd__u8)v; uzstd__wle16(d+1, v>>8); }
                    else if (hsize == 4) uzstd__wle32(d, 2u | (2u<<2) | (n<<4) | ((uzstd__u32)csize<<18));
                    else { uzstd__u64 v = 2u | (3u<<2) | ((uzstd__u64)n<<4) | ((uzstd__u64)csize<<22);
                        uzstd__wle32(d, (uzstd__u32)v); d[4] = (uzstd__u8)(v>>32); }
                    return hsize + csize;
                }
            }
        }
    }
    if (rawh == 1) d[0] = (uzstd__u8)(n<<3);                  /* type=0 raw, 5-bit size */
    else if (rawh == 2) uzstd__wle16(d, (1u<<2) | (n<<4));    /* 12-bit size */
    else { d[0] = (uzstd__u8)((3u<<2) | (n<<4)); uzstd__wle16(d+1, n>>4); } /* 20-bit */
    UZSTD_MEMCPY(d + rawh, cx->lit, n);
    return rawh + n;
}

/* ---------------- frame / block driver ---------------- */
UZSTD_LINKAGE size_t uzstd_compress_bound(size_t n) {
    return UZSTD_COMPRESS_BOUND(n);
}

static size_t uzstd__frame_header(uzstd__u8 *dst, uzstd__u64 csize) {
    uzstd__u8 *d = dst;
    int fcs = csize >= 0xFFFFFFFFull ? 3 : csize > 0xFFFF ? 2 : csize > 0xFF ? 1 : 0;
    uzstd__wle32(d, 0xFD2FB528u); d += 4;
    *d++ = (uzstd__u8)((fcs<<6) | 0x20); /* single_segment=1, no checksum, no dict */
    switch (fcs) {
    case 0: *d++ = (uzstd__u8)csize; break;
    case 1: uzstd__wle16(d, (uzstd__u32)(csize-256)); d += 2; break;
    case 2: uzstd__wle32(d, (uzstd__u32)csize); d += 4; break;
    default: uzstd__wle32(d, (uzstd__u32)csize); uzstd__wle32(d+4, (uzstd__u32)(csize>>32)); d += 8;
    }
    return (size_t)(d - dst);
}

static void uzstd__block_header(uzstd__u8 *dst, int last, int type, uzstd__u32 size) {
    uzstd__u32 v = (size<<3) | ((uzstd__u32)type<<1) | (uzstd__u32)last;
    dst[0]=(uzstd__u8)v; dst[1]=(uzstd__u8)(v>>8); dst[2]=(uzstd__u8)(v>>16);
}

/* returns block content size (header at dst, content at dst+3) */
static size_t uzstd__compress_block(uzstd__ctx *cx, uzstd__u8 *dst, const uzstd__u8 *base,
                                    size_t bstart, size_t n, int last) {
    const uzstd__u8 *src = base + bstart;
    {   size_t i; /* RLE block */
        for (i = 1; i < n; i++) if (src[i] != src[0]) break;
        if (i == n && n > 1) { uzstd__block_header(dst, last, 1, (uzstd__u32)n); dst[3] = src[0]; return 1; }
    }
    if (n >= 16) {
        uzstd__parse(cx, base, bstart, n);
        {   uzstd__u8 *t = cx->tmp, *tend = cx->tmp + UZSTD__TMP_CAP;
            size_t sz = uzstd__lit_section(cx, t, (size_t)(tend - t));
            if (sz) {
                t += sz;
                sz = uzstd__encode_sequences(cx, t, tend);
                if (sz) {
                    size_t csz = (size_t)(t + sz - cx->tmp);
                    if (csz < n) {
                        uzstd__block_header(dst, last, 2, (uzstd__u32)csz);
                        UZSTD_MEMCPY(dst+3, cx->tmp, csz);
                        return csz;
                    }
                }
            }
        }
    }
    uzstd__block_header(dst, last, 0, (uzstd__u32)n);
    UZSTD_MEMCPY(dst+3, src, n);
    return n;
}

UZSTD_LINKAGE size_t uzstd_compress(void *dst_v, size_t dst_cap, const void *src_v, size_t src_size, int level) {
    uzstd__u8 *dst = (uzstd__u8*)dst_v, *d = dst;
    const uzstd__u8 *src = (const uzstd__u8*)src_v;
    size_t pos = 0;
    uzstd__ctx cx;
    uzstd__u8 *mem;
    if (dst_cap < UZSTD_COMPRESS_BOUND(src_size) || src_size >= 0x7FFFFFF0u) return (size_t)-1;
    d += uzstd__frame_header(d, src_size);
    if (src_size == 0) { uzstd__block_header(d, 1, 0, 0); return (size_t)(d + 3 - dst); }
    if (level < 1) level = 1;
    if (level > 9) level = 9;
    mem = (uzstd__u8*)UZSTD_MALLOC(((1u<<UZSTD__HASH_LOG) + src_size)*4
                             + UZSTD__MAX_SEQ*sizeof(uzstd__seq) + UZSTD__BLOCK_MAX + UZSTD__TMP_CAP);
    if (!mem) return (size_t)-1;
    cx.htab  = (uzstd__u32*)mem;
    cx.chain = cx.htab + (1u<<UZSTD__HASH_LOG);
    cx.seqs  = (uzstd__seq*)(cx.chain + src_size);
    cx.lit   = (uzstd__u8*)(cx.seqs + UZSTD__MAX_SEQ);
    cx.tmp   = cx.lit + UZSTD__BLOCK_MAX;
    UZSTD_MEMSET(cx.htab, 0, (1u<<UZSTD__HASH_LOG)*4);
    cx.rep[0] = 1; cx.rep[1] = 4; cx.rep[2] = 8;
    {   static const int depths[9] = {1,2,4,8,16,32,48,64,96};
        cx.depth = depths[level-1];
        cx.lazy = level >= 3;
    }
    while (pos < src_size) {
        size_t bn = src_size - pos > UZSTD__BLOCK_MAX ? UZSTD__BLOCK_MAX : src_size - pos;
        int last = pos + bn == src_size;
        d += 3 + uzstd__compress_block(&cx, d, src, pos, bn, last);
        pos += bn;
    }
    UZSTD_FREE(mem);
    return (size_t)(d - dst);
}


#endif /* UZSTD_COMPRESSOR_C */
#endif /* !UZSTD_NO_COMPRESSOR */

#if !defined(UZSTD_NO_DECOMPRESSOR)
#ifndef UZSTD_DECOMPRESSOR_C
#define UZSTD_DECOMPRESSOR_C

/* ================================================================== */
/* ============================ decompressor ============================= */
/* ================================================================== */

static uzstd__u32 uzstd__le32(const uzstd__u8 *p) {
    return (uzstd__u32)p[0] | ((uzstd__u32)p[1]<<8) | ((uzstd__u32)p[2]<<16) | ((uzstd__u32)p[3]<<24);
}
static uzstd__u64 uzstd__le64(const uzstd__u8 *p) {
    return (uzstd__u64)uzstd__le32(p) | ((uzstd__u64)uzstd__le32(p+4) << 32);
}

/* backward bitstream reader. p must have 8 readable bytes before it (block
** scratch front pad / preceding headers); reads use a +64-bit bias so byte
** indexing stays non-negative for look-ahead slightly past the stream start. */
typedef struct { const uzstd__u8 *p; long long pos; int err; } uzstd__br;

static int uzstd__br_init(uzstd__br *b, const uzstd__u8 *p, size_t n) {
    b->p = p; b->err = 0; b->pos = 0;
    if (!n || !p[n-1]) { b->err = 1; return 0; }
    b->pos = (long long)(n-1)*8 + uzstd__highbit(p[n-1]); /* strip end marker bit */
    return 1;
}
static uzstd__u32 uzstd__br_look(const uzstd__br *b, int nb) { /* nb <= 31 */
    uzstd__u64 x = (uzstd__u64)(b->pos - nb + 64);
    return (uzstd__u32)((uzstd__le64(b->p - 8 + (x>>3)) >> (x & 7)) & ((1u<<nb)-1));
}
static uzstd__u32 uzstd__br_read(uzstd__br *b, int nb) {
    uzstd__u32 v;
    if (nb > b->pos) { b->err = 1; b->pos = 0; return 0; }
    v = uzstd__br_look(b, nb);
    b->pos -= nb;
    return v;
}
static uzstd__u32 uzstd__fwd_bits(const uzstd__u8 *p, size_t bitpos, int nb) { /* forward LE reader */
    return (uzstd__u32)((uzstd__le64(p + (bitpos>>3)) >> (bitpos & 7)) & ((1u<<nb)-1));
}

/* FSE decode table entry / build (RFC 4.1.1) */
typedef struct { uzstd__u16 base; uzstd__u8 sym, nb; } uzstd__dte;

static int uzstd__fse_dtable(uzstd__dte *dt, const short *norm, int max_sym, int log) {
    int tsize = 1<<log, high = tsize-1, mask = tsize-1, step = (tsize>>1)+(tsize>>3)+3;
    uzstd__u16 next[64];
    int s, u, pos = 0;
    for (s = 0; s <= max_sym; s++) {
        if (norm[s] == -1) { dt[high--].sym = (uzstd__u8)s; next[s] = 1; }
        else next[s] = (uzstd__u16)norm[s];
    }
    for (s = 0; s <= max_sym; s++) {
        int i;
        for (i = 0; i < norm[s]; i++) {
            dt[pos].sym = (uzstd__u8)s;
            do { pos = (pos + step) & mask; } while (pos > high);
        }
    }
    if (pos != 0) return 0;
    for (u = 0; u < tsize; u++) {
        int ns = next[dt[u].sym]++;
        dt[u].nb = (uzstd__u8)(log - uzstd__highbit((uzstd__u32)ns));
        dt[u].base = (uzstd__u16)((ns << dt[u].nb) - tsize);
    }
    return 1;
}

/* read FSE table description; norm[0..max_sym] must be pre-zeroed.
** returns bytes consumed, 0 on corruption. */
static size_t uzstd__read_ncount(short *norm, int max_sym, int max_log, int *log_out,
                                 const uzstd__u8 *ip, size_t avail) {
    size_t bp = 4;
    int log, remaining, threshold, nbits, sym = 0, prev0 = 0;
    if (!avail) return 0;
    log = (ip[0] & 15) + 5;
    if (log > max_log) return 0;
    *log_out = log;
    remaining = (1<<log) + 1; threshold = 1<<log; nbits = log + 1;
    while (remaining > 1) {
        if (prev0) {
            while (uzstd__fwd_bits(ip, bp, 16) == 0xFFFF) {
                sym += 24; bp += 16;
                if (sym > max_sym || (bp>>3) > avail) return 0;
            }
            while (uzstd__fwd_bits(ip, bp, 2) == 3) { sym += 3; bp += 2; if (sym > max_sym) return 0; }
            sym += (int)uzstd__fwd_bits(ip, bp, 2); bp += 2;
            if (sym > max_sym) return 0;
        }
        {   int max = (2*threshold-1) - remaining, count;
            if ((int)uzstd__fwd_bits(ip, bp, nbits-1) < max) {
                count = (int)uzstd__fwd_bits(ip, bp, nbits-1); bp += (size_t)(nbits-1);
            } else {
                count = (int)uzstd__fwd_bits(ip, bp, nbits); bp += (size_t)nbits;
                if (count >= threshold) count -= max;
            }
            count--; /* 0 => -1 (low prob), 1 => 0, ... */
            remaining -= count < 0 ? -count : count;
            if (remaining < 1 || sym > max_sym) return 0;
            norm[sym++] = (short)count;
            prev0 = (count == 0);
            while (remaining < threshold) { nbits--; threshold >>= 1; }
        }
        if ((bp>>3) > avail) return 0;
    }
    bp = (bp + 7) >> 3;
    return bp <= avail ? bp : 0;
}

/* decoder context: tables persist across blocks within a frame */
typedef struct {
    uzstd__dte dll[1<<9], dml[1<<9], dof[1<<8];
    int tlog[3], tok[3];                /* ll/of/ml table logs + repeat-valid flags */
    uzstd__u16 huf[1<<11];
    int huf_log, huf_ok;
    uzstd__u32 rep[3];
    uzstd__u8 lit[131072];              /* decoded literals */
    uzstd__u8 blk[8 + 131072 + 8];      /* padded copy of block content */
} uzstd__dctx;

/* Huffman tree description -> cx->huf decode table; returns bytes consumed, 0 = fail */
static size_t uzstd__huf_tree_d(uzstd__dctx *cx, const uzstd__u8 *ip, size_t avail) {
    uzstd__u8 w[256];
    uzstd__u32 sum = 0, rest;
    int nw, i, tlog;
    size_t used;
    if (!avail) return 0;
    if (ip[0] >= 128) { /* direct: 4-bit weights */
        nw = ip[0] - 127;
        used = 1 + (size_t)((nw+1)/2);
        if (used > avail) return 0;
        for (i = 0; i < nw; i++) w[i] = (i&1) ? (ip[1+(i>>1)] & 15) : (ip[1+(i>>1)] >> 4);
    } else { /* FSE-compressed weights, two interleaved states */
        short norm[16];
        uzstd__dte dt[64];
        uzstd__br b;
        int log, s1, s2, n = 0;
        size_t csz = ip[0], nc;
        if (1 + csz > avail) return 0;
        UZSTD_MEMSET(norm, 0, sizeof norm);
        nc = uzstd__read_ncount(norm, 15, 6, &log, ip+1, csz);
        if (!nc || !uzstd__fse_dtable(dt, norm, 15, log)) return 0;
        if (!uzstd__br_init(&b, ip+1+nc, csz-nc)) return 0;
        s1 = (int)uzstd__br_read(&b, log);
        s2 = (int)uzstd__br_read(&b, log);
        if (b.err) return 0;
        while (1) { /* decode until bitstream exhausted */
            int t;
            if (n > 254) return 0;
            w[n++] = dt[s1].sym;
            if (dt[s1].nb > b.pos) { if (n > 254) return 0; w[n++] = dt[s2].sym; break; }
            s1 = dt[s1].base + (int)uzstd__br_read(&b, dt[s1].nb);
            t = s1; s1 = s2; s2 = t;
        }
        nw = n;
        used = 1 + csz;
    }
    for (i = 0; i < nw; i++) { if (w[i] > 11) return 0; if (w[i]) sum += 1u << (w[i]-1); }
    if (!sum) return 0;
    tlog = uzstd__highbit(sum) + 1;
    rest = (1u<<tlog) - sum;
    if (tlog > 11 || (rest & (rest-1))) return 0; /* must complete to a power of 2 */
    w[nw++] = (uzstd__u8)(uzstd__highbit(rest) + 1); /* implied last weight */
    {   uzstd__u32 p = 0; int wv, s; /* fill order: weight asc, symbol asc */
        for (wv = 1; wv <= tlog; wv++)
            for (s = 0; s < nw; s++) if (w[s] == wv) {
                uzstd__u32 k, cnt = 1u << (wv-1), e = (uzstd__u32)s | ((uzstd__u32)(tlog+1-wv) << 8);
                for (k = 0; k < cnt; k++) cx->huf[p++] = (uzstd__u16)e;
            }
    }
    cx->huf_log = tlog; cx->huf_ok = 1;
    return used;
}

static int uzstd__huf_stream_d(uzstd__dctx *cx, uzstd__u8 *out, size_t n, const uzstd__u8 *p, size_t sz) {
    uzstd__br b;
    int log = cx->huf_log;
    size_t i;
    if (!uzstd__br_init(&b, p, sz)) return 0;
    for (i = 0; i < n; i++) {
        uzstd__u32 e = cx->huf[uzstd__br_look(&b, log)];
        b.pos -= (int)(e >> 8);
        out[i] = (uzstd__u8)e;
        if (b.pos < 0) return 0;
    }
    return b.pos == 0; /* stream must be fully consumed */
}

/* literals section; sets lit ptr + size, returns bytes consumed, 0 = fail */
static size_t uzstd__lits_d(uzstd__dctx *cx, const uzstd__u8 *ip, size_t avail,
                            const uzstd__u8 **litp, size_t *litsz) {
    int type, sf;
    size_t regen, h;
    if (!avail) return 0;
    type = ip[0] & 3; sf = (ip[0] >> 2) & 3;
    if (type <= 1) { /* raw / RLE */
        if (sf == 1)      { if (avail < 2) return 0; h = 2; regen = (((size_t)ip[0] | ((size_t)ip[1]<<8)) >> 4); }
        else if (sf == 3) { if (avail < 3) return 0; h = 3; regen = (((size_t)ip[0] | ((size_t)ip[1]<<8) | ((size_t)ip[2]<<16)) >> 4); }
        else              { h = 1; regen = (size_t)(ip[0] >> 3); }
        if (regen > 131072) return 0;
        *litsz = regen;
        if (type == 0) {
            if (h + regen > avail) return 0;
            *litp = ip + h; /* point into block scratch, no copy */
            return h + regen;
        }
        if (h + 1 > avail) return 0;
        UZSTD_MEMSET(cx->lit, ip[h], regen);
        *litp = cx->lit;
        return h + 1;
    }
    {   /* compressed (2) / treeless (3) */
        const uzstd__u8 *p;
        size_t csz, rem, used;
        int four = (sf != 0);
        if (sf <= 1) { if (avail < 3) return 0; h = 3;
            { uzstd__u32 v = (uzstd__u32)ip[0] | ((uzstd__u32)ip[1]<<8) | ((uzstd__u32)ip[2]<<16);
              regen = (v>>4) & 0x3FF; csz = v >> 14; } }
        else if (sf == 2) { if (avail < 4) return 0; h = 4;
            { uzstd__u32 v = uzstd__le32(ip); regen = (v>>4) & 0x3FFF; csz = v >> 18; } }
        else { if (avail < 5) return 0; h = 5;
            { uzstd__u64 v = (uzstd__u64)uzstd__le32(ip) | ((uzstd__u64)ip[4]<<32);
              regen = (size_t)((v>>4) & 0x3FFFF); csz = (size_t)(v >> 22); } }
        if (!regen || regen > 131072 || !csz || h + csz > avail) return 0;
        p = ip + h; rem = csz;
        if (type == 2) {
            used = uzstd__huf_tree_d(cx, p, rem);
            if (!used) return 0;
            p += used; rem -= used;
        } else if (!cx->huf_ok) return 0; /* treeless without previous tree */
        if (!four) {
            if (!uzstd__huf_stream_d(cx, cx->lit, regen, p, rem)) return 0;
        } else {
            size_t q = (regen + 3) >> 2, sz1, sz2, sz3, sz4;
            if (3*q >= regen || rem < 6) return 0; /* 4 streams need >= 4 literals */
            sz1 = (size_t)p[0] | ((size_t)p[1]<<8);
            sz2 = (size_t)p[2] | ((size_t)p[3]<<8);
            sz3 = (size_t)p[4] | ((size_t)p[5]<<8);
            p += 6; rem -= 6;
            if (sz1 + sz2 + sz3 + 1 > rem) return 0;
            sz4 = rem - sz1 - sz2 - sz3;
            if (!uzstd__huf_stream_d(cx, cx->lit,       q,           p,             sz1)) return 0;
            if (!uzstd__huf_stream_d(cx, cx->lit + q,   q,           p+sz1,         sz2)) return 0;
            if (!uzstd__huf_stream_d(cx, cx->lit + 2*q, q,           p+sz1+sz2,     sz3)) return 0;
            if (!uzstd__huf_stream_d(cx, cx->lit + 3*q, regen - 3*q, p+sz1+sz2+sz3, sz4)) return 0;
        }
        *litp = cx->lit; *litsz = regen;
        return h + csz;
    }
}

/* RFC 3.1.1.3.2.2: predefined distributions */
static const short uzstd__llnorm[36] = {4,3,2,2,2,2,2,2,2,2,2,2,2,1,1,1,
    2,2,2,2,2,2,2,2,2,3,2,1,1,1,1,1,-1,-1,-1,-1};
static const short uzstd__ofnorm[29] = {1,1,1,1,1,1,2,2,2,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,-1,-1,-1,-1,-1};
static const short uzstd__mlnorm[53] = {1,4,3,2,2,2,2,2,2,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,-1,-1,-1,-1,-1,-1,-1};

/* prepare one sequence decode table per its mode; returns bytes consumed or -1 */
static size_t uzstd__seq_prep(uzstd__dctx *cx, int k /*0=ll 1=of 2=ml*/, int mode,
                              const uzstd__u8 *ip, size_t avail) {
    static const short * const dnorm[3] = { uzstd__llnorm, uzstd__ofnorm, uzstd__mlnorm };
    static const uzstd__u8 dlog[3] = {6,5,6}, dmxs[3] = {35,28,52}, mxs[3] = {35,31,52}, mxl[3] = {9,8,9};
    uzstd__dte *dt = k == 0 ? cx->dll : k == 1 ? cx->dof : cx->dml;
    switch (mode) {
    case 0: /* predefined */
        if (!uzstd__fse_dtable(dt, dnorm[k], dmxs[k], dlog[k])) return (size_t)-1;
        cx->tlog[k] = dlog[k]; cx->tok[k] = 1;
        return 0;
    case 1: /* RLE: one byte = the code */
        if (!avail || ip[0] > mxs[k]) return (size_t)-1;
        dt[0].sym = ip[0]; dt[0].nb = 0; dt[0].base = 0;
        cx->tlog[k] = 0; cx->tok[k] = 1;
        return 1;
    case 2: { /* FSE table description */
        short norm[64];
        int log;
        size_t nc;
        UZSTD_MEMSET(norm, 0, sizeof norm);
        nc = uzstd__read_ncount(norm, mxs[k], mxl[k], &log, ip, avail);
        if (!nc || !uzstd__fse_dtable(dt, norm, mxs[k], log)) return (size_t)-1;
        cx->tlog[k] = log; cx->tok[k] = 1;
        return nc; }
    default: /* repeat previous */
        return cx->tok[k] ? 0 : (size_t)-1;
    }
}

/* decode one compressed block (content already copied into padded scratch) */
static int uzstd__block_d(uzstd__dctx *cx, uzstd__u8 **op_io, uzstd__u8 *oend,
                          const uzstd__u8 *frame_base, const uzstd__u8 *ip, size_t n) {
    const uzstd__u8 *iend = ip + n, *lp, *lend;
    uzstd__u8 *op = *op_io, *ostart = op;
    size_t litsz, used, ns, i;
    used = uzstd__lits_d(cx, ip, n, &lp, &litsz);
    if (!used) return 0;
    ip += used; lend = lp + litsz;
    if (ip >= iend) return 0;
    if (ip[0] < 128)      { ns = ip[0]; ip += 1; }
    else if (ip[0] < 255) { if (iend - ip < 2) return 0; ns = ((size_t)(ip[0]-128)<<8) + ip[1]; ip += 2; }
    else                  { if (iend - ip < 3) return 0; ns = ((size_t)ip[1] | ((size_t)ip[2]<<8)) + 0x7F00; ip += 3; }
    if (ns == 0) {
        if (ip != iend) return 0;
    } else {
        uzstd__br b;
        uzstd__u32 sll, sof, sml;
        int modes;
        modes = *ip++;
        if (modes & 3) return 0; /* reserved bits */
        for (i = 0; i < 3; i++) {
            used = uzstd__seq_prep(cx, (int)i, (modes >> (6 - 2*(int)i)) & 3, ip, (size_t)(iend - ip));
            if (UZSTD_IS_ERROR(used) || used > (size_t)(iend - ip)) return 0;
            ip += used;
        }
        if (!uzstd__br_init(&b, ip, (size_t)(iend - ip))) return 0;
        sll = uzstd__br_read(&b, cx->tlog[0]);
        sof = uzstd__br_read(&b, cx->tlog[1]);
        sml = uzstd__br_read(&b, cx->tlog[2]);
        for (i = 0; i < ns; i++) {
            uzstd__dte ell = cx->dll[sll], eof = cx->dof[sof], eml = cx->dml[sml];
            uzstd__u32 ov, ll, ml, off;
            ov = (1u << eof.sym) + uzstd__br_read(&b, eof.sym);
            ml = 3 + uzstd__mlbase[eml.sym] + uzstd__br_read(&b, uzstd__mlbits[eml.sym]);
            ll = uzstd__llbase[ell.sym] + uzstd__br_read(&b, uzstd__llbits[ell.sym]);
            if (ov > 3) {
                off = ov - 3;
                cx->rep[2] = cx->rep[1]; cx->rep[1] = cx->rep[0]; cx->rep[0] = off;
            } else {
                uzstd__u32 idx = ov - 1 + (ll == 0);
                if (idx == 3) { off = cx->rep[0] - 1; if (!off) return 0; }
                else off = cx->rep[idx];
                if (idx) {
                    if (idx > 1) cx->rep[2] = cx->rep[1];
                    cx->rep[1] = cx->rep[0]; cx->rep[0] = off;
                }
            }
            if (i + 1 < ns) { /* no state update after last sequence */
                sll = ell.base + uzstd__br_read(&b, ell.nb);
                sml = eml.base + uzstd__br_read(&b, eml.nb);
                sof = eof.base + uzstd__br_read(&b, eof.nb);
            }
            if (b.err) return 0;
            if (ll > (size_t)(lend - lp) || (size_t)ll + ml > (size_t)(oend - op)) return 0;
            UZSTD_MEMCPY(op, lp, ll); op += ll; lp += ll;
            if (off > (size_t)(op - frame_base)) return 0;
            {   const uzstd__u8 *mp = op - off;
                uzstd__u8 *e = op + ml;
                if (off >= 8 && (size_t)(oend - op) >= (size_t)ml + 8) {
                    do { UZSTD_MEMCPY(op, mp, 8); op += 8; mp += 8; } while (op < e);
                    op = e;
                } else while (op < e) *op++ = *mp++;
            }
        }
        if (b.err || b.pos != 0) return 0; /* bitstream must be fully consumed */
    }
    used = (size_t)(lend - lp); /* trailing literals */
    if (used > (size_t)(oend - op)) return 0;
    UZSTD_MEMCPY(op, lp, used); op += used;
    if ((size_t)(op - ostart) > 131072) return 0; /* Block_Maximum_Size */
    *op_io = op;
    return 1;
}

UZSTD_LINKAGE unsigned long long uzstd_frame_content_size(const void *src_v, size_t n) {
    const uzstd__u8 *ip = (const uzstd__u8*)src_v, *iend = ip + n;
    while (iend - ip >= 8 && (uzstd__le32(ip) & 0xFFFFFFF0u) == 0x184D2A50u) {
        size_t sk = uzstd__le32(ip+4);
        if ((size_t)(iend - ip) - 8 < sk) return (unsigned long long)-1;
        ip += 8 + sk;
    }
    if (iend - ip < 6 || uzstd__le32(ip) != 0xFD2FB528u) return (unsigned long long)-1;
    {   int fhd = ip[4], fcsf = fhd >> 6, ss = (fhd >> 5) & 1;
        static const int dsz[4] = {0,1,2,4};
        ip += 5 + (ss ? 0 : 1) + dsz[fhd & 3];
        if (fcsf == 0 && !ss) return (unsigned long long)-1; /* unknown */
        switch (fcsf) {
        case 0:  return iend - ip < 1 ? (unsigned long long)-1 : ip[0];
        case 1:  return iend - ip < 2 ? (unsigned long long)-1 : 256ull + ((uzstd__u32)ip[0] | ((uzstd__u32)ip[1]<<8));
        case 2:  return iend - ip < 4 ? (unsigned long long)-1 : uzstd__le32(ip);
        default: return iend - ip < 8 ? (unsigned long long)-1 : uzstd__le64(ip);
        }
    }
}

UZSTD_LINKAGE size_t uzstd_decompress(void *dst_v, size_t dst_cap, const void *src_v, size_t src_size) {
    const uzstd__u8 *ip = (const uzstd__u8*)src_v, *iend = ip + src_size;
    uzstd__u8 *op = (uzstd__u8*)dst_v, *oend = op + dst_cap;
    uzstd__dctx *cx = 0;
    while (iend - ip >= 4) {
        if ((uzstd__le32(ip) & 0xFFFFFFF0u) == 0x184D2A50u) { /* skippable frame */
            size_t sk;
            if (iend - ip < 8) goto fail;
            sk = uzstd__le32(ip+4);
            if ((size_t)(iend - ip) - 8 < sk) goto fail;
            ip += 8 + sk;
            continue;
        }
        if (uzstd__le32(ip) != 0xFD2FB528u) goto fail;
        ip += 4;
        {   uzstd__u64 fcs = (uzstd__u64)-1;
            uzstd__u8 *fstart = op;
            int fhd, fcsf, ss, ck;
            if (ip >= iend) goto fail;
            fhd = *ip++;
            if (fhd & 8) goto fail; /* reserved bit */
            fcsf = fhd >> 6; ss = (fhd >> 5) & 1; ck = (fhd >> 2) & 1;
            if (!ss) { if (ip >= iend) goto fail; ip++; } /* window descriptor (unused: single-shot) */
            {   static const int dsz[4] = {0,1,2,4}; /* dictionary id: must be absent or zero */
                int k, nb = dsz[fhd & 3];
                uzstd__u32 did = 0;
                if (iend - ip < nb) goto fail;
                for (k = 0; k < nb; k++) did |= (uzstd__u32)ip[k] << (8*k);
                if (did) goto fail; /* dictionaries unsupported */
                ip += nb;
            }
            {   int nb = fcsf == 0 ? ss : fcsf == 1 ? 2 : fcsf == 2 ? 4 : 8, k;
                if (iend - ip < nb) goto fail;
                if (nb) {
                    fcs = 0;
                    for (k = 0; k < nb; k++) fcs |= (uzstd__u64)ip[k] << (8*k);
                    if (fcsf == 1) fcs += 256;
                    ip += nb;
                }
            }
            if (!cx) { cx = (uzstd__dctx*)UZSTD_MALLOC(sizeof *cx); if (!cx) goto fail; }
            cx->huf_ok = 0; cx->tok[0] = cx->tok[1] = cx->tok[2] = 0;
            cx->rep[0] = 1; cx->rep[1] = 4; cx->rep[2] = 8;
            for (;;) {
                uzstd__u32 bh;
                int last, type;
                size_t bsz;
                if (iend - ip < 3) goto fail;
                bh = (uzstd__u32)ip[0] | ((uzstd__u32)ip[1]<<8) | ((uzstd__u32)ip[2]<<16);
                ip += 3;
                last = bh & 1; type = (bh >> 1) & 3; bsz = bh >> 3;
                if (type == 3 || bsz > 131072) goto fail;
                if (type == 0) {
                    if ((size_t)(iend - ip) < bsz || (size_t)(oend - op) < bsz) goto fail;
                    UZSTD_MEMCPY(op, ip, bsz); op += bsz; ip += bsz;
                } else if (type == 1) {
                    if (iend - ip < 1 || (size_t)(oend - op) < bsz) goto fail;
                    UZSTD_MEMSET(op, ip[0], bsz); op += bsz; ip += 1;
                } else {
                    if (!bsz || (size_t)(iend - ip) < bsz) goto fail;
                    UZSTD_MEMSET(cx->blk, 0, 8);
                    UZSTD_MEMCPY(cx->blk + 8, ip, bsz);
                    UZSTD_MEMSET(cx->blk + 8 + bsz, 0, 8);
                    if (!uzstd__block_d(cx, &op, oend, fstart, cx->blk + 8, bsz)) goto fail;
                    ip += bsz;
                }
                if (last) break;
            }
            if (fcs != (uzstd__u64)-1 && (uzstd__u64)(op - fstart) != fcs) goto fail;
            if (ck) { if (iend - ip < 4) goto fail; ip += 4; } /* checksum skipped, not verified */
        }
    }
    if (ip != iend) goto fail;
    UZSTD_FREE(cx);
    return (size_t)(op - (uzstd__u8*)dst_v);
fail:
    UZSTD_FREE(cx);
    return (size_t)-1;
}


#endif /* UZSTD_DECOMPRESSOR_C */
#endif /* !UZSTD_NO_DECOMPRESSOR */

#endif /* UZSTD_IMPLEMENTATION */

#ifndef ULNET_C
#define ULNET_C
#define ULNET__XXH32_PRIME1 2654435761u
#define ULNET__XXH32_PRIME2 2246822519u
#define ULNET__XXH32_PRIME3 3266489917u
#define ULNET__XXH32_PRIME4  668265263u
#define ULNET__XXH32_PRIME5  374761393u

static uint32_t ulnet__read32le(const void *p) {
    const uint8_t *b = (const uint8_t *)p;
    return ((uint32_t)b[0])
         | ((uint32_t)b[1] << 8)
         | ((uint32_t)b[2] << 16)
         | ((uint32_t)b[3] << 24);
}

static uint32_t ulnet__rotl32(uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}

ULNET_LINKAGE uint32_t ulnet_xxh32(const void* data, size_t len, uint32_t seed) {
    const uint8_t *p = data ? (const uint8_t *)data : (const uint8_t *)"";
    const uint8_t *end = p + len;
    uint32_t h32;

    if (len >= 16) {
        const uint8_t *limit = end - 16;
        uint32_t v1 = seed + ULNET__XXH32_PRIME1 + ULNET__XXH32_PRIME2;
        uint32_t v2 = seed + ULNET__XXH32_PRIME2;
        uint32_t v3 = seed;
        uint32_t v4 = seed - ULNET__XXH32_PRIME1;

        do {
            v1 = ulnet__rotl32(v1 + ulnet__read32le(p) * ULNET__XXH32_PRIME2, 13) * ULNET__XXH32_PRIME1;
            p += 4;
            v2 = ulnet__rotl32(v2 + ulnet__read32le(p) * ULNET__XXH32_PRIME2, 13) * ULNET__XXH32_PRIME1;
            p += 4;
            v3 = ulnet__rotl32(v3 + ulnet__read32le(p) * ULNET__XXH32_PRIME2, 13) * ULNET__XXH32_PRIME1;
            p += 4;
            v4 = ulnet__rotl32(v4 + ulnet__read32le(p) * ULNET__XXH32_PRIME2, 13) * ULNET__XXH32_PRIME1;
            p += 4;
        } while (p <= limit);

        h32 = ulnet__rotl32(v1, 1) + ulnet__rotl32(v2, 7) + ulnet__rotl32(v3, 12) + ulnet__rotl32(v4, 18);
    } else {
        h32 = seed + ULNET__XXH32_PRIME5;
    }

    h32 += (uint32_t)len;

    while (p + 4 <= end) {
        h32 += ulnet__read32le(p) * ULNET__XXH32_PRIME3;
        h32 = ulnet__rotl32(h32, 17) * ULNET__XXH32_PRIME4;
        p += 4;
    }

    while (p < end) {
        h32 += (uint32_t)(*p) * ULNET__XXH32_PRIME5;
        h32 = ulnet__rotl32(h32, 11) * ULNET__XXH32_PRIME1;
        p++;
    }

    h32 ^= h32 >> 15;
    h32 *= ULNET__XXH32_PRIME2;
    h32 ^= h32 >> 13;
    h32 *= ULNET__XXH32_PRIME3;
    h32 ^= h32 >> 16;

    return h32;
}

static void ulnet_packet_ref_clear(ulnet_packet_ref_t *ref) {
    if (ref->data) {
        ULNET_FREE(ref->data);
    }
    *ref = ulnet_packet_ref_null;
}

static int ulnet_packet_ref_set(ulnet_packet_ref_t *ref, const void *data, size_t size, uint16_t flags) {
    if (size == 0 || size > UINT16_MAX) {
        ulnet_packet_ref_clear(ref);
        return -1;
    }

    uint8_t *copy = (uint8_t *) ULNET_MALLOC(size);
    if (!copy) {
        return -1;
    }

    memcpy(copy, data, size);
    ulnet_packet_ref_clear(ref);
    ref->data = copy;
    ref->size = (uint16_t) size;
    ref->flags = flags;
    return 0;
}

static void ulnet_packet_ref_clear_many(ulnet_packet_ref_t *refs, size_t count) {
    for (size_t i = 0; i < count; i++) {
        ulnet_packet_ref_clear(&refs[i]);
    }
}

static void ulnet__sleep(unsigned int msec) {
#if defined(_WIN32)
    // Windows implementation
    Sleep((DWORD)(msec));
#else
    struct timespec timeout;
    int rc;

    timeout.tv_sec = msec / 1000;
    timeout.tv_nsec = (msec % 1000) * 1000 * 1000;

    do
        rc = nanosleep(&timeout, &timeout);
    while (rc == -1 && errno == EINTR);

    assert(rc == 0);
#endif
}

#ifdef _WIN32
ULNET_LINKAGE int64_t ulnet__get_unix_time_microseconds() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    ULARGE_INTEGER ul;
    ul.LowPart = ft.dwLowDateTime;
    ul.HighPart = ft.dwHighDateTime;

    int64_t unix_time = (int64_t)(ul.QuadPart - 116444736000000000LL) / 10;

    return unix_time;
}
#else
ULNET_LINKAGE int64_t ulnet__get_unix_time_microseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif


#ifdef _WIN32
#include <intrin.h> // For __rdtsc, __cpuid
#endif
uint64_t ulnet__rdtsc() {
// Disabled because ARM64 platforms don't usually let you read the cycle counter
#if 0 && defined(__aarch64__)                  /* === ARM64 (Broken) == */
    unsigned long long v;

    // An ISB guarantees all previous instructions retire before we sample
    __asm__ __volatile__ (
        "isb\n\t"                 // Finish everything already issued
        "mrs %0, pmccntr_el0\n\t" // Read the 64-bit counter
        : "=r" (v)                // Write to v
        :                         // No inputs
        : "memory"                // Prohibit compiler from hoisting loads/stores
    );

    return v;
                                               /* === x86/x86_64 ====== */
#elif defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#if defined(_MSC_VER)                          /* --- MSVC compiler --- */
    int cpuInfo[4];
    __cpuid(cpuInfo, 0);          // Retire previous instructions
    return __rdtsc();
#elif defined(__GNUC__)                        /* clang/GCC compiler -- */
    unsigned int lo, hi;
    __asm__ __volatile__ (
        "cpuid\n\t"               // Serialize execution
        "rdtsc\n\t"               // Read timestamp counter
        : "=a" (lo), "=d" (hi)    // write to lo and hi
        : "a" (0)                 // Input for cpuid (eax=0)
        : "ebx", "ecx"            // cpuid clobbers these
    );
    return ((uint64_t)hi << 32) | lo;
#endif
#endif                                         /* === Unsupported ===== */
    return 1000 * ulnet__get_unix_time_microseconds();
}

static inline int ulnet__sequence_cmp(uint16_t s1, uint16_t s2) {
    if (s1 == s2) {
        return 0;
    } else {
        return (uint16_t)(s1 - s2) < 32768 ? 1 : -1; // Defined overflow, the cast is necessary because of implicit type conversion
    }
}
static inline int ulnet__sequence_greater_than(uint16_t s1, uint16_t s2) { return ulnet__sequence_cmp(s1, s2) > 0; }
static inline int ulnet__sequence_less_than(uint16_t s1, uint16_t s2)    { return ulnet__sequence_cmp(s1, s2) < 0; }
static inline int ulnet__sequence_in_range_inclusive(uint16_t sequence, uint16_t low, uint16_t high) { return !ulnet__sequence_less_than(sequence, low) && !ulnet__sequence_greater_than(sequence, high); }

static void ulnet__logical_partition(int sz, int redundant, int *n, int *out_k, int *packet_size, int *packet_groups) {
    int k_max = ULNET_RS_GF_SIZE - redundant;
    *packet_groups = 1;
    int k = (sz - 1) / (*packet_groups * *packet_size) + 1;

    if (k > k_max) {
        *packet_groups = (k - 1) / k_max + 1;
        *packet_size = (sz - 1) / (k_max * *packet_groups) + 1;
        k = (sz - 1) / (*packet_groups * *packet_size) + 1;
    }

    int repair = (k * redundant + k_max - 1) / k_max;
    if (repair < 1) {
        repair = 1;
    }
    *n = k + repair;
    *out_k = k;
}

// This is a little confusing since the lower byte of sequence corresponds to the largest stride
static int64_t ulnet__logical_partition_offset_bytes(uint8_t sequence_hi, uint8_t sequence_lo, int block_size_bytes, int block_stride) {
    return (int64_t) sequence_hi * block_size_bytes + sequence_lo * block_size_bytes * block_stride;
}

static int ulnet__rs_repair_block_count(int k) {
    if (k <= 0) {
        return 0;
    }

    int repair = (k * FEC_REDUNDANT_BLOCKS + ULNET_RS_DATA_BLOCKS_MAX - 1) / ULNET_RS_DATA_BLOCKS_MAX;
    return repair > 0 ? repair : 1;
}

static int ulnet__rs_total_block_count(int k) {
    return k + ulnet__rs_repair_block_count(k);
}

static uint8_t ulnet__rs_exp[ULNET_RS_GF_SIZE * 2];
static uint8_t ulnet__rs_log[ULNET_RS_GF_ORDER];
static uint8_t ulnet__rs_initialized;

static void ulnet__rs_init(void) {
    if (ulnet__rs_initialized) {
        return;
    }

    uint16_t x = 1;
    for (int i = 0; i < ULNET_RS_GF_SIZE; i++) {
        ulnet__rs_exp[i] = (uint8_t)x;
        ulnet__rs_log[x] = (uint8_t)i;
        x <<= 1;
        if (x & 0x100) {
            x ^= 0x11d;
        }
    }

    for (int i = ULNET_RS_GF_SIZE; i < ULNET_RS_GF_SIZE * 2; i++) {
        ulnet__rs_exp[i] = ulnet__rs_exp[i - ULNET_RS_GF_SIZE];
    }

    ulnet__rs_initialized = 1;
}

static inline uint8_t ulnet__rs_mul(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) {
        return 0;
    }

    return ulnet__rs_exp[ulnet__rs_log[a] + ulnet__rs_log[b]];
}

static inline uint8_t ulnet__rs_inv(uint8_t a) {
    return ulnet__rs_exp[ULNET_RS_GF_SIZE - ulnet__rs_log[a]];
}

static void ulnet__rs_addmul(uint8_t *dst, const uint8_t *src, uint8_t c, int size) {
    if (c == 0) {
        return;
    }

    for (int i = 0; i < size; i++) {
        dst[i] ^= ulnet__rs_mul(src[i], c);
    }
}

static int ulnet__rs_invert_matrix(uint8_t *m, int k) {
    uint8_t inv[ULNET_RS_DATA_BLOCKS_MAX * ULNET_RS_DATA_BLOCKS_MAX];

    if (k <= 0 || k > ULNET_RS_DATA_BLOCKS_MAX) {
        return -1;
    }

    memset(inv, 0, (size_t)k * (size_t)k);
    for (int i = 0; i < k; i++) {
        inv[i * k + i] = 1;
    }

    for (int col = 0; col < k; col++) {
        int pivot = col;
        while (pivot < k && m[pivot * k + col] == 0) {
            pivot++;
        }

        if (pivot == k) {
            return -1;
        }

        if (pivot != col) {
            for (int i = 0; i < k; i++) {
                uint8_t t = m[col * k + i];
                m[col * k + i] = m[pivot * k + i];
                m[pivot * k + i] = t;

                t = inv[col * k + i];
                inv[col * k + i] = inv[pivot * k + i];
                inv[pivot * k + i] = t;
            }
        }

        uint8_t scale = ulnet__rs_inv(m[col * k + col]);
        for (int i = 0; i < k; i++) {
            m[col * k + i] = ulnet__rs_mul(m[col * k + i], scale);
            inv[col * k + i] = ulnet__rs_mul(inv[col * k + i], scale);
        }

        for (int row = 0; row < k; row++) {
            if (row == col) {
                continue;
            }

            uint8_t c = m[row * k + col];
            if (c == 0) {
                continue;
            }

            for (int i = 0; i < k; i++) {
                m[row * k + i] ^= ulnet__rs_mul(m[col * k + i], c);
                inv[row * k + i] ^= ulnet__rs_mul(inv[col * k + i], c);
            }
        }
    }

    memcpy(m, inv, (size_t)k * (size_t)k);
    return 0;
}

static int ulnet__rs_build_matrix(int k, int n, uint8_t *matrix) {
    uint8_t vandermonde[ULNET_RS_TOTAL_BLOCKS_MAX * ULNET_RS_DATA_BLOCKS_MAX];
    uint8_t top_inverse[ULNET_RS_DATA_BLOCKS_MAX * ULNET_RS_DATA_BLOCKS_MAX];

    if (k <= 0 || k > ULNET_RS_DATA_BLOCKS_MAX || n < k || n > ULNET_RS_TOTAL_BLOCKS_MAX) {
        return -1;
    }

    ulnet__rs_init();
    memset(vandermonde, 0, (size_t)n * (size_t)k);
    vandermonde[0] = 1;
    for (int row = 1; row < n; row++) {
        uint8_t x = ulnet__rs_exp[row - 1];
        uint8_t v = 1;
        for (int col = 0; col < k; col++) {
            vandermonde[row * k + col] = v;
            v = ulnet__rs_mul(v, x);
        }
    }

    memcpy(top_inverse, vandermonde, (size_t)k * (size_t)k);
    if (ulnet__rs_invert_matrix(top_inverse, k) != 0) {
        return -1;
    }

    memset(matrix, 0, (size_t)n * (size_t)k);
    for (int i = 0; i < k; i++) {
        matrix[i * k + i] = 1;
    }

    for (int row = k; row < n; row++) {
        for (int col = 0; col < k; col++) {
            uint8_t acc = 0;
            for (int i = 0; i < k; i++) {
                acc ^= ulnet__rs_mul(vandermonde[row * k + i], top_inverse[i * k + col]);
            }
            matrix[row * k + col] = acc;
        }
    }

    return 0;
}

static void ulnet__rs_encode(void **blocks, int k, int n, int block_size) {
    uint8_t matrix[ULNET_RS_TOTAL_BLOCKS_MAX * ULNET_RS_DATA_BLOCKS_MAX];

    if (ulnet__rs_build_matrix(k, n, matrix) != 0) {
        SAM2_LOG_ERROR("Failed to build Reed-Solomon encoding matrix");
        assert(0);
        return;
    }

    for (int row = k; row < n; row++) {
        uint8_t *dst = (uint8_t *)blocks[row];
        memset(dst, 0, (size_t)block_size);
        for (int col = 0; col < k; col++) {
            ulnet__rs_addmul(dst, (const uint8_t *)blocks[col], matrix[row * k + col], block_size);
        }
    }
}

static int ulnet__rs_decode(ulnet_packet_ref_t blocks[ULNET_RS_TOTAL_BLOCKS_MAX], int k, int n, int block_size) {
    uint8_t matrix[ULNET_RS_TOTAL_BLOCKS_MAX * ULNET_RS_DATA_BLOCKS_MAX];
    uint8_t decode_matrix[ULNET_RS_DATA_BLOCKS_MAX * ULNET_RS_DATA_BLOCKS_MAX];
    uint8_t missing[ULNET_RS_DATA_BLOCKS_MAX];
    uint8_t *decoded[ULNET_RS_DATA_BLOCKS_MAX];
    int index[ULNET_RS_DATA_BLOCKS_MAX];
    int received = 0;
    int missing_count = 0;

    if (ulnet__rs_build_matrix(k, n, matrix) != 0) {
        return -1;
    }

    for (int i = 0; i < n && received < k; i++) {
        if (blocks[i].data != NULL) {
            index[received++] = i;
        }
    }

    if (received < k) {
        return -1;
    }

    for (int row = 0; row < k; row++) {
        memcpy(&decode_matrix[row * k], &matrix[index[row] * k], (size_t)k);
    }

    if (ulnet__rs_invert_matrix(decode_matrix, k) != 0) {
        return -1;
    }

    for (int row = 0; row < k; row++) {
        if (blocks[row].data == NULL) {
            missing[missing_count] = (uint8_t)row;
            decoded[missing_count] = (uint8_t *)ULNET_MALLOC((size_t)block_size);
            if (decoded[missing_count] == NULL) {
                for (int i = 0; i < missing_count; i++) {
                    ULNET_FREE(decoded[i]);
                }
                return -1;
            }
            memset(decoded[missing_count], 0, (size_t)block_size);
            missing_count++;
        }
    }

    for (int m = 0; m < missing_count; m++) {
        int row = missing[m];
        for (int col = 0; col < k; col++) {
            ulnet__rs_addmul(decoded[m], blocks[index[col]].data, decode_matrix[row * k + col], block_size);
        }
    }

    for (int m = 0; m < missing_count; m++) {
        int row = missing[m];
        if (ulnet_packet_ref_set(&blocks[row], decoded[m], (size_t)block_size, 0) != 0) {
            for (int i = m; i < missing_count; i++) {
                ULNET_FREE(decoded[i]);
            }
            return -1;
        }
        ULNET_FREE(decoded[m]);
    }

    return 0;
}

ULNET_LINKAGE void ulnet_input_poll(ulnet_session_t *session, ulnet_input_state_t (*input_state)[ULNET_PORT_COUNT]) {
    int64_t input_digest_time_usec = ulnet__get_unix_time_microseconds();

    for (int peer_idx = 0; peer_idx < SAM2_TOTAL_PEERS; peer_idx++) {
        if (ulnet_port_is_active_player(&session->room_we_are_in, peer_idx)) {

            if (!(session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)) {
                assert(peer_idx == SAM2_AUTHORITY_INDEX);
            }

            assert(session->state[peer_idx].frame <= session->frame_counter + (ULNET_DELAY_BUFFER_SIZE-1));
            assert(session->state[peer_idx].frame >= session->frame_counter);

            // Any player may drive any controller; everyone's input is OR-merged ("anyone controls any
            // port"). The peer slot indexes state[] (64-wide), the controller is a separate axis, so a
            // p2p player on any slot contributes correctly without indexing controllers out of bounds.
            for (int port = 0; port < ULNET_PORT_COUNT; port++) {
                for (int i = 0; i < SAM2_ARRAY_LENGTH((*input_state)[port]); i++) {
                    (*input_state)[port][i] |= session->state[peer_idx].input_state[session->frame_counter % ULNET_DELAY_BUFFER_SIZE][port][i];
                }
            }

            int64_t poll_time_usec = session->state[peer_idx].input_poll_unix_usec[session->frame_counter % ULNET_DELAY_BUFFER_SIZE];
            int our_port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);
            if (   poll_time_usec > 0
                && (peer_idx == our_port || session->peer_packet_ping_samples[peer_idx] > 0)) {
                int64_t clock_offset_usec = session->peer_packet_kernel_ping_samples[peer_idx] > 0
                    ? session->peer_kernel_clock_offset_usec[peer_idx]
                    : session->peer_clock_offset_usec[peer_idx];
                int64_t poll_time_local_usec = poll_time_usec - clock_offset_usec;
                session->peer_input_to_core_ping_usec[peer_idx] = input_digest_time_usec - poll_time_local_usec;
            }
        }
    }
}

double core_wants_tick_in_seconds(int64_t core_wants_tick_at_unix_usec) {
    double seconds = (core_wants_tick_at_unix_usec - ulnet__get_unix_time_microseconds()) / 1000000.0;
    return seconds;
}

static void ulnet_update_state_history(ulnet_session_t *session, const uint8_t *packet, size_t packet_size) {
    // Only store every 8th packet... frame 7, 15, 23, etc.
    if (!ulnet__is_input_channel(packet[0])) {
        SAM2_LOG_ERROR("Attempt to store non-input packet in state history");
        return;
    }

    int port = packet[0] & 0b00111111;
    int64_t frame;
    rle8_decode(&packet[sizeof(ulnet_state_packet_t)], packet_size - sizeof(ulnet_state_packet_t), (uint8_t *) &frame, sizeof(frame));
    if ((frame + 1) % ULNET_DELAY_BUFFER_SIZE == 0) {
        int history_idx = (frame / ULNET_DELAY_BUFFER_SIZE) % ULNET_STATE_PACKET_HISTORY_SIZE;

        SAM2_LOG_DEBUG("Storing state packet for port %d at history index %d for frame %lld", port, history_idx, (long long)frame);
        ulnet_packet_ref_set(&session->state_packet_history[port][history_idx], packet, packet_size, 0);
    }
}

// Returns a negative number on error
ULNET_LINKAGE int ulnet_udp_send(ulnet_session_t *session, int port, const uint8_t *packet, size_t size) {
    // Basic packet validation
    if (size - 1 >= ULNET_PACKET_SIZE_BYTES_MAX) {
        SAM2_LOG_ERROR("Attempt to send packet with invalid size: %zu bytes, max allowed: %d", size, ULNET_PACKET_SIZE_BYTES_MAX);
        return -1;
    }

    // Perform channel-specific packet validation
    switch (packet[0] & ULNET_CHANNEL_MASK) {
    case ULNET_CHANNEL_EXTRA: {
        SAM2_LOG_ERROR("Attempt to send packet on reserved/invalid channel (header byte 0x%02" PRIx8 ")", packet[0]);
        return -1;
    }
    case ULNET_CHANNEL_INPUT_HI:
    case ULNET_CHANNEL_INPUT: {
        if (size < sizeof(ulnet_state_packet_t)) {
            SAM2_LOG_ERROR("Input packet too small: %zu bytes", size);
            return -1;
        }

        ulnet_state_packet_t *state_packet = (ulnet_state_packet_t *)packet;
        int64_t encoded_state_size = size - sizeof(ulnet_state_packet_t);
        int64_t decoded_size = rle8_decode_size(state_packet->coded_state, encoded_state_size);

        if (decoded_size < 0) {
            SAM2_LOG_ERROR("Input packet has invalid RLE encoding");
            return -1;
        }

        if (decoded_size > sizeof(ulnet_state_t)) {
            SAM2_LOG_ERROR("Input packet would decode to %" PRId64 " bytes, exceeding destination buffer size %zu",
                            decoded_size, sizeof(ulnet_state_t));
            return -1;
        }

        if (decoded_size < sizeof(ulnet_state_t)) {
            SAM2_LOG_WARN("Input packet would decode to only %" PRId64 " bytes, expected %zu",
                        decoded_size, sizeof(ulnet_state_t));
            // Allow undersized packets as they might be partial updates
        }
        break;
    }

    case ULNET_CHANNEL_SPECTATOR_INPUT: {
        if (size < sizeof(ulnet_state_packet_t)) {
            SAM2_LOG_ERROR("Spectator input packet too small: %zu bytes", size);
            return -1;
        }

        // Skip the header (1 byte) to get to the encoded data
        int64_t encoded_size = size - 1;
        int64_t decoded_size = rle8_decode_size((const uint8_t*)packet + 1, encoded_size);

        if (decoded_size < 0) {
            SAM2_LOG_ERROR("Spectator input packet has invalid RLE encoding");
            return -1;
        }

        if (decoded_size > sizeof(session->spectator_suggested_input_state[0])) {
            SAM2_LOG_ERROR("Spectator input would decode to %" PRId64 " bytes, exceeding buffer size %zu",
                            decoded_size, sizeof(session->spectator_suggested_input_state[0]));
            return -1;
        }
        break;
    }

    case ULNET_CHANNEL_RELIABLE:
        if (size <= 1) {
            SAM2_LOG_ERROR("Reliable packet contains only header");
            return -1;
        }
        break;
    }

    // packet_history is read only by the imgui debug table; skip the per-packet malloc/copy otherwise
    if (session->flags & ULNET_SESSION_FLAG_DRAW_IMGUI) {
        ulnet_packet_ref_set(&session->packet_history[port][session->packet_history_next[port]++], packet, size, ULNET_PACKET_FLAG_TX);
    }

    if (rand() / ((float) RAND_MAX) < session->debug_udp_send_drop_rate) {
        SAM2_LOG_ERROR("Intentionally dropped a sent UDP packet");
        return 0;
    }

    if (session->use_inproc_transport) {
        ulnet_inproc_buf_t *buf;
        if (session->our_peer_id < session->room_we_are_in.peer_ids[port]) {
            buf = &session->inproc[port]->buf1;
        } else {
            buf = &session->inproc[port]->buf2;
        }

        if (buf->count >= sizeof(buf->msg) / sizeof(buf->msg[0])) {
            SAM2_LOG_FATAL("Inproc transport buffer is full, cannot send packet");
            return -1;
        }

        buf->msg_size[buf->count] = size;
        memcpy(buf->msg[buf->count], packet, size);
        buf->count++;
        return 0;
    } else {
        return ulnet__nat_send(session->agent[port], packet, size);
    }
}

// Simplified wrap packet function
static int ulnet__wrap_packet(const uint8_t packet[/* size */], int size, uint16_t sequence,
    uint16_t ack_sequence, uint8_t wrapped_packet[/* ULNET_PACKET_SIZE_BYTES_MAX */]) {
    if ((wrapped_packet[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_RELIABLE) {
        SAM2_LOG_ERROR("Expected filled out first header byte");
        return -1;
    }

    int offset = ULNET_HEADER_SIZE;

    // Always include sequence
    memcpy(&wrapped_packet[offset], &sequence, sizeof(sequence));
    offset += sizeof(sequence);

    // Always include ack
    memcpy(&wrapped_packet[offset], &ack_sequence, sizeof(ack_sequence));
    offset += sizeof(ack_sequence);

    if (ULNET_PACKET_SIZE_BYTES_MAX < offset + size) {
        SAM2_LOG_ERROR("Reliable packet too large: %d bytes", size);
        return -1;
    }

    memcpy(&wrapped_packet[offset], packet, size);

    return offset + size;
}

static int ulnet__reliable_send_head(ulnet_session_t *session, int port, bool retransmit) {
    uint16_t head_sequence = session->reliable_tx_head[port];
    uint16_t next_sequence = session->reliable_tx_next_seq[port];

    if (!ulnet__sequence_less_than(head_sequence, next_sequence)) {
        return 0;
    }

    ulnet_packet_ref_t packet_ref = session->reliable_tx_packet_history[port][head_sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE];
    ulnet_reliable_packet_t *packet = (ulnet_reliable_packet_t *) packet_ref.data;
    int packet_size = packet_ref.size;

    if (!packet || memcmp(&packet->sequence_le, &head_sequence, sizeof(head_sequence)) != 0) {
        SAM2_LOG_FATAL("Head of queue packet overwritten");
        return -1;
    }

    uint8_t packet_to_send[ULNET_PACKET_SIZE_BYTES_MAX];
    memcpy(packet_to_send, packet, packet_size);
    packet = (ulnet_reliable_packet_t *) packet_to_send;

    memcpy(&packet->ack_sequence_le, &session->reliable_rx_head[port], sizeof(packet->ack_sequence_le));

    SAM2_LOG_INFO("%s reliable packet with sequence %d",
        retransmit ? "Retransmitting" : "Sending queued", head_sequence);

    session->reliable_last_transmit_time[port] = ulnet__get_unix_time_microseconds();
    int status = ulnet_udp_send(session, port, packet_to_send, packet_size);
    if (status != 0) {
        SAM2_LOG_ERROR("Failed to send reliable packet with sequence %d", head_sequence);
    } else if (retransmit && (session->flags & ULNET_SESSION_FLAG_DRAW_IMGUI)) {
        session->packet_history[port][(uint8_t)(session->packet_history_next[port] - 1)].flags |= ULNET_PACKET_FLAG_TX_RELIABLE_RETRANSMIT;
    }

    return status;
}

// Simplified reliable send
ULNET_LINKAGE int ulnet_reliable_send(ulnet_session_t *session, int port, const uint8_t *packet, int size) {
    uint8_t tmp[ULNET_PACKET_SIZE_BYTES_MAX];

    tmp[0] = ULNET_CHANNEL_RELIABLE;
    uint16_t queued_packets = (uint16_t)(session->reliable_tx_next_seq[port] - session->reliable_tx_head[port]);
    if (queued_packets >= ULNET_RELIABLE_ACK_BUFFER_SIZE) {
        SAM2_LOG_ERROR("Reliable send queue is full for port %d", port);
        return -1;
    }

    uint16_t sequence = session->reliable_tx_next_seq[port];
    uint16_t ack_sequence = session->reliable_rx_head[port];

    int maybe_wrapped_size = ulnet__wrap_packet(packet, size, sequence, ack_sequence, tmp);
    if (maybe_wrapped_size < 0) {
        return maybe_wrapped_size;
    }

    ulnet_packet_ref_set(
        &session->reliable_tx_packet_history[port][sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE],
        tmp,
        maybe_wrapped_size,
        ULNET_PACKET_FLAG_TX
    );
    session->reliable_tx_next_seq[port]++;

    if (queued_packets == 0) {
        return ulnet__reliable_send_head(session, port, false);
    }

    return 0;
}

ULNET_LINKAGE int ulnet_reliable_send_with_acks_only(ulnet_session_t *session, int port, const uint8_t *packet, int size) {
    uint8_t tmp[ULNET_PACKET_SIZE_BYTES_MAX];

    tmp[0] = ULNET_CHANNEL_RELIABLE | ULNET_RELIABLE_FLAG_ACK_ONLY;
    uint16_t sequence = 0; // Ignored
    uint16_t ack_sequence = session->reliable_rx_head[port];

    int maybe_wrapped_size = ulnet__wrap_packet(packet, size, sequence, ack_sequence, tmp);
    if (maybe_wrapped_size < 0) {
        return maybe_wrapped_size;
    } else {
        return ulnet_udp_send(session, port, tmp, maybe_wrapped_size);
    }
}

static sam2_message_metadata_t ulnet__message_metadata[] = {
    {ulnet_exit_header, SAM2_HEADER_SIZE},
};

ULNET_LINKAGE int ulnet_message_send(ulnet_session_t *session, int port, const uint8_t *message) {
    if ((message[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_ASCII) {
        SAM2_LOG_FATAL("Attempt to send non-ASCII message with ulnet_message_send");
        return -1;
    }

    sam2_message_metadata_t *metadata = sam2_get_metadata((const char *) message);

    for (int i = 0; i < SAM2_ARRAY_LENGTH(ulnet__message_metadata); i++) {
        if (sam2_header_matches((char *)message, ulnet__message_metadata[i].header)) {
            metadata = &ulnet__message_metadata[i];
        }
    }

    return ulnet_reliable_send(session, port, message, metadata->message_size);
}

static SAM2_FORCEINLINE int64_t ulnet__get_frame_from_packet(const uint8_t *packet) {
    int64_t frame;
    rle8_decode(&packet[sizeof(ulnet_state_packet_t)], sizeof(frame), (uint8_t *) &frame, sizeof(frame));
    return frame;
}

static void ulnet__stamp_state_packet_ping(ulnet_session_t *session, int peer_port, uint8_t *packet, int64_t send_time_usec) {
    if (!ulnet__is_input_channel(packet[0])) return;

    ulnet_state_packet_t *state_packet = (ulnet_state_packet_t *) packet;
    ulnet__write_le64s(state_packet->ping_send_unix_usec_le, send_time_usec);
    ulnet__write_le64s(state_packet->ping_echo_send_unix_usec_le, session->peer_last_packet_send_unix_usec[peer_port]);
    ulnet__write_le64s(state_packet->ping_echo_callsite_receive_unix_usec_le, session->peer_last_packet_callsite_receive_unix_usec[peer_port]);
    ulnet__write_le64s(state_packet->ping_echo_kernel_receive_unix_usec_le, session->peer_last_packet_kernel_receive_unix_usec[peer_port]);
}


static void ulnet__memor(void *dst, const void *src, size_t n) {
    int16_t *d = (int16_t *)dst;
    const int16_t *s = (const int16_t *)src;
    size_t words = n / sizeof(int16_t);

    for (size_t i = 0; i < words; i++) {
        d[i] |= s[i];
    }
}

ULNET_LINKAGE int64_t ulnet__room_advertise_frame_from_effective_frame(int64_t effective_frame) {
    return effective_frame - ULNET_ROOM_CHANGE_LEAD_FRAMES;
}

static void ulnet__publish_authority_room_snapshot(ulnet_session_t *session, int state_port) {
    assert(ulnet_is_authority(session));

    if (   session->next_room_effective_frame > 0
        && session->state[state_port].frame < ulnet__room_advertise_frame_from_effective_frame(session->next_room_effective_frame)
        && memcmp(&session->next_room, &session->room_we_are_in, sizeof(sam2_room_t)) != 0) {
        session->state[state_port].room = session->room_we_are_in;
        session->state[state_port].room_effective_frame = 0;
    } else {
        session->state[state_port].room = session->next_room;
        session->state[state_port].room_effective_frame = session->next_room_effective_frame;
    }
}

static bool ulnet__authority_has_pending_room_change(ulnet_session_t *session) {
    return    session->next_room_effective_frame > 0
           && memcmp(&session->next_room, &session->room_we_are_in, sizeof(sam2_room_t)) != 0;
}

static int64_t ulnet__encode_state_packet(ulnet_session_t *session, int state_port, uint8_t *packet, size_t packet_capacity) {
    packet[0] = ULNET_CHANNEL_INPUT_HI | state_port;
    ulnet_state_packet_t *state_packet = (ulnet_state_packet_t *) packet;
    memset(state_packet->ping_send_unix_usec_le, 0, sizeof(state_packet->ping_send_unix_usec_le));
    memset(state_packet->ping_echo_send_unix_usec_le, 0, sizeof(state_packet->ping_echo_send_unix_usec_le));
    memset(state_packet->ping_echo_callsite_receive_unix_usec_le, 0, sizeof(state_packet->ping_echo_callsite_receive_unix_usec_le));
    memset(state_packet->ping_echo_kernel_receive_unix_usec_le, 0, sizeof(state_packet->ping_echo_kernel_receive_unix_usec_le));

    // Reserve room for the reliable wrapper: these state packets are always sent through the reliable
    // channel (ulnet_reliable_send / ..._with_acks_only), and a payload that fits unwrapped but not
    // wrapped would be silently dropped by ulnet__wrap_packet.
    int64_t encoded_size = rle8_encode_capped(
        (uint8_t *) &session->state[state_port],
        sizeof(session->state[0]),
        &packet[sizeof(ulnet_state_packet_t)],
        packet_capacity - sizeof(ulnet_state_packet_t) - ULNET_RELIABLE_WRAPPER_BYTES
    );

    if (encoded_size < 0) {
        return encoded_size;
    }

    return sizeof(ulnet_state_packet_t) + encoded_size;
}

static void ulnet__send_state_packet_to_ready_peers(ulnet_session_t *session, int state_port) {
    uint8_t packet[ULNET_PACKET_SIZE_BYTES_MAX];
    int64_t packet_size = ulnet__encode_state_packet(session, state_port, packet, sizeof(packet));
    if (packet_size < 0 || packet_size > ULNET_PACKET_SIZE_BYTES_MAX) {
        // A state that won't compress into one packet can't be sent this frame; skip rather than crash.
        // It is recoverable -- the next frame re-encodes fresh state.
        SAM2_LOG_WARN("State packet for port %d too large to send this frame; skipping", state_port);
        return;
    }

    ulnet_update_state_history(session, packet, packet_size);

    bool sent = false;
    for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
        if (!ulnet__peer_link_ready(session, p)) continue;
        ulnet__stamp_state_packet_ping(session, p, packet, ulnet__get_unix_time_microseconds());
        ulnet_reliable_send_with_acks_only(session, p, packet, packet_size);
        sent = true;
    }

    if (sent && ulnet_is_authority(session) && state_port == SAM2_AUTHORITY_INDEX) {
        session->authority_room_snapshot_last_sent_frame = SAM2_MAX(
            session->authority_room_snapshot_last_sent_frame,
            session->state[state_port].frame
        );
    }
}

static int64_t ulnet__required_authority_snapshot_frame(int64_t frame_counter) {
    int64_t required = frame_counter + 1 - ULNET_ROOM_CHANGE_LEAD_FRAMES;
    return required > 0 ? required : -1;
}

static void ulnet__savestate_tx_poll(ulnet_session_t *session);
static int ulnet__send_save_state_to_peers(ulnet_session_t *session, uint64_t peer_bitfield, void *save_state,
    size_t save_state_size, int64_t save_state_frame);

#define ULNET_POLL_SESSION_SAVED_STATE    0b00000001
#define ULNET_POLL_SESSION_TICKED         0b00000010
#define ULNET_POLL_SESSION_BUFFERED_INPUT 0b00000100
ULNET_LINKAGE int ulnet_poll_session(ulnet_session_t *session, bool force_save_state_on_tick, uint8_t *save_state, size_t save_state_capacity,
    double frame_rate, double max_sleeping_allowed_when_polling_network_seconds) {

    int our_port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);

    IMH(ImGui::Begin("P2P UDP Netplay", NULL, ImGuiWindowFlags_AlwaysAutoResize);)
    int status = 0;

    bool we_are_authority = ulnet_is_authority(session);
    bool we_are_player = our_port != -1 && ulnet_port_is_p2p(&session->room_we_are_in, our_port);
    bool we_are_coordinator_only_authority = we_are_authority && our_port == SAM2_AUTHORITY_INDEX && !we_are_player;

    // Poll input with buffering for netplay
    if (our_port == -1) {
        SAM2_LOG_WARN("No port associated for our peer_id=%d, skipping input polling", session->our_peer_id);
    } else if (we_are_player) {
      if (session->state[our_port].frame < session->frame_counter + session->delay_frames) {
        status |= ULNET_POLL_SESSION_BUFFERED_INPUT;
        // @todo The preincrement does not make sense to me here, but things have been working
        int64_t next_buffer_index = ++session->state[our_port].frame % ULNET_DELAY_BUFFER_SIZE;

        session->state[our_port].core_option[next_buffer_index] = session->next_core_option;
        session->state[our_port].input_poll_unix_usec[next_buffer_index] = ulnet__get_unix_time_microseconds();

        // Advertise the authoritative room snapshot and the frame it takes effect.
        // Only the authority's snapshot is consulted.
        if (we_are_authority) {
            ulnet__publish_authority_room_snapshot(session, our_port);
        } else {
            session->state[our_port].room = session->room_we_are_in;
            session->state[our_port].room_effective_frame = 0;
        }

        // Incoporate input from spectators into our input. This has the drawback of round trip latency but requires a single connection to the server
        memset(session->state[our_port].input_state[next_buffer_index], 0, sizeof(session->state[our_port].input_state[next_buffer_index]));
        for (int i = 0; i < SAM2_ARRAY_LENGTH(session->agent); i++) {
            if (session->agent[i]) {
                for (int p = 0; p < SAM2_PORT_MAX; p++) {
                    for (int j = 0; j < SAM2_ARRAY_LENGTH(session->state[our_port].input_state[next_buffer_index][p]); j++) {
                        session->state[our_port].input_state[next_buffer_index][p][j] |= session->spectator_suggested_input_state[i][p][j];
                    }
                }
            }
        }

        ulnet__memor(session->state[our_port].input_state[next_buffer_index], session->next_input_state, sizeof(ulnet_input_state_t[SAM2_PORT_MAX]));
      }
    } else if (we_are_coordinator_only_authority) {
        if (session->state[SAM2_AUTHORITY_INDEX].frame < session->frame_counter) {
            status |= ULNET_POLL_SESSION_BUFFERED_INPUT;
            int64_t next_buffer_index = ++session->state[SAM2_AUTHORITY_INDEX].frame % ULNET_DELAY_BUFFER_SIZE;
            session->state[SAM2_AUTHORITY_INDEX].core_option[next_buffer_index] = session->next_core_option;
            session->state[SAM2_AUTHORITY_INDEX].input_poll_unix_usec[next_buffer_index] = ulnet__get_unix_time_microseconds();
            memset(session->state[SAM2_AUTHORITY_INDEX].input_state[next_buffer_index], 0,
                sizeof(session->state[SAM2_AUTHORITY_INDEX].input_state[next_buffer_index]));
            ulnet__publish_authority_room_snapshot(session, SAM2_AUTHORITY_INDEX);
        } else {
            ulnet__publish_authority_room_snapshot(session, SAM2_AUTHORITY_INDEX);
        }
    } else {
        // We are a client-server spectator: suggest input to the authority who folds it into its own
        memcpy(session->spectator_suggested_input_state[63], session->next_input_state, sizeof(session->spectator_suggested_input_state[63]));
    }

    if (our_port != -1) {
        uint8_t packet[ULNET_PACKET_SIZE_BYTES_MAX];
        int64_t packet_size;

        // A coordinator-only authority sits on SAM2_AUTHORITY_INDEX, so its state port is just our_port.
        bool we_send_authoritative_state = we_are_player || we_are_coordinator_only_authority;
        if (we_send_authoritative_state) {
            if (we_are_authority) {
                ulnet__publish_authority_room_snapshot(session, our_port);
            }
            // Store every 8th complete state snapshot for spectator reconstruction and diagnostics.
            // The state stream itself is idempotent latest-state traffic; correctness comes from the
            // frame gates below, while the wrapper carries ACKs for reliable control messages.
            ulnet__send_state_packet_to_ready_peers(session, our_port);
        } else {
            packet[0] = ULNET_CHANNEL_SPECTATOR_INPUT;
            memset(packet + 1, 0, sizeof(ulnet_state_packet_t) - 1);
            packet_size = sizeof(ulnet_state_packet_t) + rle8_encode_capped(
                (uint8_t *) &session->spectator_suggested_input_state[63],
                sizeof(session->spectator_suggested_input_state[63]),
                &packet[sizeof(ulnet_state_packet_t)],
                sizeof(packet) - sizeof(ulnet_state_packet_t)
            );

            if (packet_size < 0 || packet_size > ULNET_PACKET_SIZE_BYTES_MAX) {
                // Recoverable: skip this frame's send rather than crashing on a state that won't fit
                SAM2_LOG_WARN("Outgoing packet too large to send this frame; skipping");
                packet_size = -1;
            }

            for (int p = 0; packet_size >= 0 && p < SAM2_ARRAY_LENGTH(session->agent); p++) {
                // Wait until we can send netplay messages to everyone without fail
                if (ulnet__peer_link_ready(session, p)) {
                    ulnet_reliable_send_with_acks_only(session, p, packet, packet_size);
                    SAM2_LOG_DEBUG("Sent spectator input packet dest peer_ids[%d]=%05" PRId16, p, session->room_we_are_in.peer_ids[p]);
                }
            }
        }
    }

    // @todo This timing code is messy I should formally model the problem and then create a solution based on that
    bool ignore_frame_pacing_so_we_can_catch_up = false;
    int64_t poll_entry_time_usec = ulnet__get_unix_time_microseconds();

    if (session->use_inproc_transport) {
        for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
            if (!session->inproc[p]) continue;

            ulnet_inproc_buf_t *buf;
            if (session->our_peer_id < session->room_we_are_in.peer_ids[p]) {
                buf = &session->inproc[p]->buf2;
            } else {
                buf = &session->inproc[p]->buf1;
            }

            for (int i = 0; i < buf->count; i++) {
                ulnet_receive_packet_callback((ulnet_nat_agent_t *)session->inproc[p], (char*)buf->msg[i], buf->msg_size[i], session);
            }
            buf->count = 0;  // Mark all messages as delivered
        }
    } else {
        // Get rid of dead agents first
        ulnet_nat_agent_t *agent[SAM2_ARRAY_LENGTH(session->agent)] = {0};
        int agent_count = 0;
        for (int p = 0; p < SAM2_ARRAY_LENGTH(session->agent); p++) {
            if (session->agent[p]) {
                if (   ulnet_nat_get_state(session->agent[p]) == ULNET_NAT_STATE_FAILED
                    || session->peer_pending_disconnect_bitfield & (1ULL << p)) {
                    if (!ulnet_port_is_p2p(&session->room_we_are_in, p)) {
                        SAM2_LOG_INFO("%s %05" PRId16 " left",
                            p == SAM2_AUTHORITY_INDEX ? "Coordinator" : "Spectator",
                            session->room_we_are_in.peer_ids[p]);
                    } else {
                        SAM2_LOG_ERROR("Peer %05" PRId16 " disconnected before leaving the room this should force a resync which I don't do right now @todo" , session->room_we_are_in.peer_ids[p]);
                    }

                    ulnet_disconnect_peer(session, p);
                } else {
                    agent[agent_count++] = session->agent[p];
                }
            }
        }

        int debug_loop_count = 0;
        do {
            if (ulnet_is_spectator(session, session->our_peer_id)) {
                int64_t authority_frame = -1;

                // The number of packets we check here is reasonable, since if we miss ULNET_DELAY_BUFFER_SIZE consecutive packets our connection is irrecoverable anyway
                for (int i = 0; i < ULNET_DELAY_BUFFER_SIZE; i++) {
                    int64_t frame = -1;
                    ulnet_packet_ref_t state_packet_ref = session->state_packet_history[SAM2_AUTHORITY_INDEX][(session->frame_counter + i) % ULNET_STATE_PACKET_HISTORY_SIZE];
                    uint8_t *state_packet = state_packet_ref.data;

                    if (state_packet) {
                        rle8_decode(&state_packet[sizeof(ulnet_state_packet_t)], state_packet_ref.size - sizeof(ulnet_state_packet_t), (uint8_t *) &frame, sizeof(frame));
                        authority_frame = SAM2_MAX(authority_frame, frame);
                    }
                }

                ignore_frame_pacing_so_we_can_catch_up = false; // authority_frame - session->frame_counter > 1;
            }

            double timeout_milliseconds = 1e3 * core_wants_tick_in_seconds(session->core_wants_tick_at_unix_usec);

            if (timeout_milliseconds < 0.0 || ignore_frame_pacing_so_we_can_catch_up) {
                timeout_milliseconds = 0.0; // No blocking
            } else if (timeout_milliseconds < 1.0) {
                timeout_milliseconds = 1.0; // Preempt ourselves otherwise we'll be busy waiting when 0 < timeout < 1 due to truncation
            }

            timeout_milliseconds = SAM2_MIN(timeout_milliseconds, 1000.0 * max_sleeping_allowed_when_polling_network_seconds);

            if (agent_count > 0) {
                for (int ai = 0; ai < agent_count; ai++) {
                    ulnet__nat_poll_agent(agent[ai]);
                }
                if (timeout_milliseconds > 0.0) {
                    ulnet__sleep((unsigned int) timeout_milliseconds);
                }
            } else {
                if (timeout_milliseconds > 0.0) {
                    ulnet__sleep((unsigned int) timeout_milliseconds);
                }
            }

            debug_loop_count++;
        } while (   core_wants_tick_in_seconds(session->core_wants_tick_at_unix_usec) > 0.0
                 && ulnet__get_unix_time_microseconds() - poll_entry_time_usec < 1e6 * max_sleeping_allowed_when_polling_network_seconds
                 && !ignore_frame_pacing_so_we_can_catch_up);

        if (debug_loop_count > 20) {
            SAM2_LOG_WARN("ulnet NAT poll loop ran %d times. This is inefficent", debug_loop_count);
        }
    }

    for (int port = 0; port < SAM2_TOTAL_PEERS; port++) {
        if (!session->agent[port]) continue;
        if (ulnet__get_unix_time_microseconds() < session->reliable_last_transmit_time[port] + session->reliable_retransmit_delay_microseconds) continue;

        ulnet__reliable_send_head(session, port, true);
    }
    ulnet__savestate_tx_poll(session);

    // Reconstruct input required for next tick if we're spectating
    if (ulnet_is_spectator(session, session->our_peer_id)) {
        for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
            if (ulnet_port_is_active_player(&session->room_we_are_in, p)) {
                int history_index_for_frame = (session->frame_counter / ULNET_DELAY_BUFFER_SIZE) % ULNET_STATE_PACKET_HISTORY_SIZE;
                ulnet_packet_ref_t ref = session->state_packet_history[p][history_index_for_frame];
                uint8_t *packet_data = ref.data;

                if (packet_data == NULL) {
                    continue; // We don't have this packet
                }

                ulnet_state_packet_t *maybe_state_packet_for_frame = (ulnet_state_packet_t *) packet_data;

                int64_t frame = -1;
                rle8_decode(maybe_state_packet_for_frame->coded_state, ref.size - sizeof(ulnet_state_packet_t),
                           (uint8_t *) &frame, sizeof(frame));

                if (SAM2_ABS(frame - session->frame_counter) < ULNET_DELAY_BUFFER_SIZE) {
                    rle8_decode(
                        maybe_state_packet_for_frame->coded_state, ref.size - sizeof(ulnet_state_packet_t),
                        (uint8_t *) &session->state[p], sizeof(session->state[p])
                    );
                }

                if (session->state[p].frame - session->frame_counter > ULNET_STATE_PACKET_HISTORY_SIZE * ULNET_DELAY_BUFFER_SIZE) {
                    SAM2_LOG_ERROR("We are too far behind to catch up we should resync");
                }
            }
        }
    }

IMH(ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);)

IMH(ulnet_imgui_show_session(session);)
IMH(ulnet_imgui_plot_history(session);)

IMH(ImGui::SeparatorText("Things We are Waiting on Before we can Tick");)
IMH(if                            (session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) { ImGui::Text("Waiting for savestate"); })
    bool netplay_ready_to_tick = !(session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL);
    for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
        if (!ulnet_port_is_active_player(&session->room_we_are_in, p)) continue;
    IMH(if                      (session->state[p].frame <  session->frame_counter) { ImGui::Text("Input state on port %d is too old", p); })
        netplay_ready_to_tick &= session->state[p].frame >= session->frame_counter;
    IMH(if                      (session->state[p].frame >= session->frame_counter + ULNET_DELAY_BUFFER_SIZE) { ImGui::Text("Input state on port %d is too new (ahead by %" PRId64 " frames)", p, session->state[p].frame - (session->frame_counter + ULNET_DELAY_BUFFER_SIZE)); })
        netplay_ready_to_tick &= session->state[p].frame <  session->frame_counter + ULNET_DELAY_BUFFER_SIZE; // This is needed for spectators only. By protocol it should always true for non-spectators unless we have a bug or someone is misbehaving
    }

    if (!(session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) && our_port != -1 && ulnet_port_is_p2p(&session->room_we_are_in, our_port)) {
        int64_t frames_buffered = session->state[our_port].frame - session->frame_counter + 1;
        assert(frames_buffered <= ULNET_DELAY_BUFFER_SIZE);
        assert(frames_buffered >= 0);
    IMH(if                      (frames_buffered <  session->delay_frames) { ImGui::Text("We have not buffered enough frames still need %" PRId64, session->delay_frames - frames_buffered); })
        netplay_ready_to_tick &= frames_buffered >= session->delay_frames;
    }

    if (   !(session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL)
        && (session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)
        && !we_are_authority) {
        int64_t required_authority_frame = ulnet__required_authority_snapshot_frame(session->frame_counter);
    IMH(if                      (required_authority_frame >= 0 && session->state[SAM2_AUTHORITY_INDEX].frame < required_authority_frame) { ImGui::Text("Waiting for authority room snapshot frame %" PRId64, required_authority_frame); })
        if (required_authority_frame >= 0) {
            netplay_ready_to_tick &= session->state[SAM2_AUTHORITY_INDEX].frame >= required_authority_frame;
        }
    }

    IMH(ImGui::End();)
    if (!netplay_ready_to_tick) {
        // @todo You should pick a time here that is a reasonable guess about when we'll receive the next packet instead of this
        //       My initial thought was doing a cfar, but making this equal to median jitter is probably good enough
        // This avoids busy waiting
        int sleep_milliseconds_upper_bound = (ulnet__get_unix_time_microseconds() - poll_entry_time_usec) / 1000;
        int sleep_milliseconds = SAM2_MIN(3, sleep_milliseconds_upper_bound);
        if (sleep_milliseconds > 0) {
            ulnet__sleep(sleep_milliseconds);
        }
    }

    if (   netplay_ready_to_tick
        && (core_wants_tick_in_seconds(session->core_wants_tick_at_unix_usec) <= 0.0
        || ignore_frame_pacing_so_we_can_catch_up)) {
        status |= ULNET_POLL_SESSION_TICKED;

        int64_t target_frame_time_usec = 1000000 / frame_rate - 1000; // @todo There is a leftover millisecond bias here for some reason
        int64_t current_time_unix_usec = ulnet__get_unix_time_microseconds();
        session->core_wants_tick_at_unix_usec = SAM2_MAX(session->core_wants_tick_at_unix_usec, current_time_unix_usec - target_frame_time_usec);
        session->core_wants_tick_at_unix_usec = SAM2_MIN(session->core_wants_tick_at_unix_usec, current_time_unix_usec + target_frame_time_usec);

        ulnet_core_option_t maybe_core_option_for_this_frame = session->state[SAM2_AUTHORITY_INDEX].core_option[session->frame_counter % ULNET_DELAY_BUFFER_SIZE];
        if (maybe_core_option_for_this_frame.key[0] != '\0') {
            if (strcmp(maybe_core_option_for_this_frame.key, "netplay_delay_frames") == 0) {
                session->delay_frames = atoi(maybe_core_option_for_this_frame.value);
            }

            for (int i = 0; i < SAM2_ARRAY_LENGTH(session->core_options); i++) {
                if (strcmp(session->core_options[i].key, maybe_core_option_for_this_frame.key) == 0) {
                    session->core_options[i] = maybe_core_option_for_this_frame;
                    session->flags |= ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY;
                    break;
                }
            }
        }

        session->flags &= ~ULNET_SESSION_FLAG_TICKED;
        bool save_state_allocated = false;
        size_t  save_state_size;
        int64_t save_state_frame = session->frame_counter;
        bool should_start_savestate_transfer = session->peer_needs_sync_bitfield && !session->savestate_transfer_awaiting_bitfield;
        if (force_save_state_on_tick || should_start_savestate_transfer) {
            uint64_t start = ulnet__rdtsc();
            save_state_size = session->retro_serialize_size(session->user_ptr);
            if (save_state_size > save_state_capacity) {
                SAM2_LOG_WARN("Save state size %zu is larger than buffer size %zu", save_state_size, save_state_capacity);
                save_state = (uint8_t *) ULNET_MALLOC(save_state_size);
                save_state_allocated = true;
            }
            session->retro_serialize(session->user_ptr, save_state, save_state_size);
            session->save_state_execution_time_cycles[session->frame_counter % ULNET_MAX_SAMPLE_SIZE] = ulnet__rdtsc() - start;
            status |= ULNET_POLL_SESSION_SAVED_STATE;

            if (session->flags & ULNET_SESSION_FLAG_TICKED) {
                SAM2_LOG_DEBUG("We ticked while saving state on frame %" PRId64, session->frame_counter);
                save_state_frame++; // @todo I think this is right I really need to write some kind of test though
            }
        }

        if (should_start_savestate_transfer) {
            uint64_t target_bitfield = session->peer_needs_sync_bitfield;
            if (ulnet__send_save_state_to_peers(session, target_bitfield, save_state, save_state_size, save_state_frame) == 0) {
                session->peer_needs_sync_bitfield &= ~target_bitfield;
            }
        }

        if (!(session->flags & ULNET_SESSION_FLAG_TICKED)) {
            session->retro_run(session->user_ptr);
        }

        session->core_wants_tick_at_unix_usec += 1000000 / frame_rate;

        // Membership consensus: adopt the authority's room snapshot at the frame it is scheduled for.
        // The authority drives this off its own desired room (next_room); everyone else reads the
        // snapshot the authority advertised in its state packets.
        sam2_room_t incoming_room;
        int64_t incoming_effective_frame;
        if (ulnet_is_authority(session)) {
            incoming_room = session->next_room;
            incoming_effective_frame = session->next_room_effective_frame;
        } else {
            incoming_room = session->state[SAM2_AUTHORITY_INDEX].room;
            incoming_effective_frame = session->state[SAM2_AUTHORITY_INDEX].room_effective_frame;
        }

        bool authority_matches = incoming_room.peer_ids[SAM2_AUTHORITY_INDEX] == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX];

        if (   authority_matches
            && session->frame_counter + 1 >= incoming_effective_frame
            && memcmp(&incoming_room, &session->room_we_are_in, sizeof(sam2_room_t)) != 0) {
            SAM2_LOG_INFO("Adopting authoritative room snapshot at frame %" PRId64, session->frame_counter + 1);

            ulnet__reconcile_connections(session, &incoming_room); // commits room_we_are_in, rebuilds changed agents (never swaps)
            if (ulnet_is_authority(session)) {
                session->next_room_effective_frame = 0;
            }

            if (!(session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)) {
                SAM2_LOG_INFO("Room '%s' was abandoned", session->room_we_are_in.name);
                ulnet_session_tear_down(session);
                ulnet_session_init_defaulted(session);
            }
        }

        // Room could have changed at this point so recompute our_port
        our_port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);

        if (   session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED
            && status & ULNET_POLL_SESSION_SAVED_STATE
            && our_port != -1
            && ulnet_port_is_p2p(&session->room_we_are_in, our_port)) {
            session->state[our_port].save_state_frame = save_state_frame;
            session->state[our_port].save_state_hash[save_state_frame % ULNET_DELAY_BUFFER_SIZE] = ulnet_xxh32(save_state, save_state_size, 0);
            //session->state[our_port].input_state_hash[save_state_frame % ULNET_DELAY_BUFFER_SIZE] = ulnet_xxh32(session->state[our_port].input_state, sizeof(session->state[our_port].input_state), 0);
        }

        if (save_state_allocated) {
            ULNET_FREE(save_state);
            save_state = NULL;
        }

        // Ideally I'd place this right after ticking the core, but we need to update the room state first
        session->frame_counter++;
    }

    return status;
}

// Connection policy: which ports we maintain a transport to.
// - The authority keeps a link to every occupied port (it relays to spectators and players alike).
// - Everyone keeps a link to the authority (port 0).
// - Two p2p players hold a direct mesh link to each other.
static bool ulnet__should_connect_to_port(ulnet_session_t *session, const sam2_room_t *room, int p) {
    uint16_t pid = room->peer_ids[p];
    if (pid <= SAM2_PORT_SENTINELS_MAX) return false;
    if (pid == session->our_peer_id)    return false;
    if (ulnet_is_authority(session))    return true;
    if (p == SAM2_AUTHORITY_INDEX)      return true;

    int our_port = sam2_get_port_of_peer((sam2_room_t *) room, session->our_peer_id);
    return ulnet_port_is_p2p(room, p) && ulnet_port_is_p2p(room, our_port);
}

// Adopt `new_room` as the committed room. Ports whose occupant changed are fully reconstructed
// (the old agent is torn down and a fresh one is built) -- peers never swap slots and connection
// state is never reused across a different peer.
ULNET_LINKAGE void ulnet__reconcile_connections(ulnet_session_t *session, const sam2_room_t *new_room) {
    sam2_room_t old_room = session->room_we_are_in;

    // Tear down agents whose port occupant changed (invariant: agent[p] is the link to old peer_ids[p])
    for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
        if (session->agent[p] && old_room.peer_ids[p] != new_room->peer_ids[p]) {
            ulnet_disconnect_peer(session, p);
        }
    }

    session->room_we_are_in = *new_room;

    for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
        bool want = ulnet__should_connect_to_port(session, &session->room_we_are_in, p);

        if (want && session->agent[p] == NULL && !session->use_inproc_transport) {
            // Convention: the lesser peer id initiates ICE; the greater id builds its agent when the signal arrives
            if (session->our_peer_id < session->room_we_are_in.peer_ids[p]) {
                ulnet_startup_nat_for_peer(session, session->room_we_are_in.peer_ids[p], p, NULL);
            }
        } else if (!want && session->agent[p]) {
            ulnet_disconnect_peer(session, p);
        }

        // A newly present p2p player must not look "behind" relative to where we already are.
        // This includes a spectator promoted in place: the occupant is the same, but the port just
        // joined the deterministic input set.
        bool was_p2p = ulnet_port_is_p2p(&old_room, p);
        bool now_p2p = ulnet_port_is_p2p(&session->room_we_are_in, p);
        if (now_p2p && (!was_p2p || new_room->peer_ids[p] != old_room.peer_ids[p])) {
            session->state[p].frame = SAM2_MAX(session->state[p].frame, session->frame_counter);
        } else if (was_p2p && !now_p2p) {
            memset(session->state[p].input_state, 0, sizeof(session->state[p].input_state));
        }
    }
}

static void ulnet_peer_init_defaulted(ulnet_session_t *session, int peer_port) {
    session->reliable_tx_next_seq [peer_port] = 0;
    session->reliable_tx_head[peer_port] = 0;
    session->reliable_rx_head[peer_port] = 0;
    session->peer_desynced_frame[peer_port] = 0;
    session->packet_history_next[peer_port] = 0;
    session->reliable_last_transmit_time[peer_port] = 0;
    session->peer_last_packet_send_unix_usec[peer_port] = 0;
    session->peer_last_packet_callsite_receive_unix_usec[peer_port] = 0;
    session->peer_last_packet_kernel_receive_unix_usec[peer_port] = 0;
    session->peer_clock_offset_usec[peer_port] = 0;
    session->peer_kernel_clock_offset_usec[peer_port] = 0;
    session->peer_packet_ping_samples[peer_port] = 0;
    session->peer_packet_ping_usec[peer_port] = 0;
    session->peer_packet_kernel_ping_samples[peer_port] = 0;
    session->peer_packet_kernel_ping_usec[peer_port] = 0;
    session->peer_input_to_core_ping_usec[peer_port] = 0;
    session->peer_needs_sync_bitfield        &= ~(1ULL << peer_port);
    session->peer_pending_disconnect_bitfield &= ~(1ULL << peer_port);
    session->savestate_transfer_awaiting_bitfield &= ~(1ULL << peer_port);
    if (!session->savestate_transfer_awaiting_bitfield) {
        session->savestate_transfer_retry_count = 0;
        session->savestate_transfer_ack_deadline_unix_usec = 0;
    }
}

static void ulnet_clear_peer_packet_history(ulnet_session_t *session, int peer_port) {
    ulnet_packet_ref_clear_many(session->state_packet_history[peer_port], ULNET_STATE_PACKET_HISTORY_SIZE);
    ulnet_packet_ref_clear_many(session->packet_history[peer_port], 256);
    ulnet_packet_ref_clear_many(session->reliable_tx_packet_history[peer_port], ULNET_RELIABLE_ACK_BUFFER_SIZE);
    ulnet_packet_ref_clear_many(session->reliable_rx_packet_history[peer_port], ULNET_RELIABLE_ACK_BUFFER_SIZE);
}

ULNET_LINKAGE void ulnet_disconnect_peer(ulnet_session_t *session, int peer_port) {
    session->peer_pending_disconnect_bitfield &= ~(1ULL << peer_port);

    SAM2_LOG_INFO("Disconnecting %s %05" PRId16 " at port %d",
        ulnet_port_is_p2p(&session->room_we_are_in, peer_port) ? "p2p player" : "spectator",
        session->room_we_are_in.peer_ids[peer_port], peer_port);

    assert(session->agent[peer_port] != NULL);
    if (!session->use_inproc_transport) {
        ulnet__nat_destroy(session->agent[peer_port]);
    }
    session->agent[peer_port] = NULL; // Required for the agent[p] <-> peer_ids[p] invariant (also fixes a dangling pointer)

    ulnet_clear_peer_packet_history(session, peer_port);
    ulnet_peer_init_defaulted(session, peer_port);
}

// The authority's desired room (next_room) is what new join/leave/topology decisions edit.
// For a non-authority peer it always mirrors the committed room.
static sam2_room_t *ulnet__authority_desired_room(ulnet_session_t *session) {
    if (ulnet_is_authority(session)) {
        return &session->next_room;
    }
    session->next_room = session->room_we_are_in;
    return &session->next_room;
}

static void ulnet__schedule_active_set_change(ulnet_session_t *session) {
    assert(ulnet_is_authority(session));

    int64_t advertise_frame = SAM2_MAX(
        session->state[SAM2_AUTHORITY_INDEX].frame,
        session->authority_room_snapshot_last_sent_frame + 1
    );
    advertise_frame = SAM2_MAX(advertise_frame, 1);

    session->next_room_effective_frame = advertise_frame + ULNET_ROOM_CHANGE_LEAD_FRAMES;
}

static void ulnet__authority_remove_peer(ulnet_session_t *session, sam2_room_t *desired, int port, bool was_player) {
    if (was_player && ulnet__authority_has_pending_room_change(session)) {
        SAM2_LOG_WARN("Ignoring player leave for port %d while another room change is pending", port);
        return;
    }

    desired->peer_ids[port] = SAM2_PORT_AVAILABLE;
    desired->peer_topology &= ~(1ULL << port);

    if (was_player) {
        ulnet__schedule_active_set_change(session);
    } else {
        session->room_we_are_in.peer_ids[port] = SAM2_PORT_AVAILABLE;
        session->room_we_are_in.peer_topology &= ~(1ULL << port);
        session->peer_pending_disconnect_bitfield |= (1ULL << port);
    }
}

static inline void ulnet__reset_save_state_bookkeeping(ulnet_session_t *session) {
    ulnet_packet_ref_clear_many(&session->packet_reference[0][0], FEC_PACKET_GROUPS_MAX * ULNET_RS_TOTAL_BLOCKS_MAX);
    session->remote_packet_groups = FEC_PACKET_GROUPS_MAX;
    session->remote_savestate_transfer_id = 0xff;
    memset(session->fec_index_counter, 0, sizeof(session->fec_index_counter));
}

// Every stored shard in a packet group must share the systematic block size; a mismatch means the
// sender changed parameters mid-transfer (or a stale shard lingers) and the group can't be decoded
static bool ulnet__savestate_blocks_size_consistent(ulnet_session_t *session, int sequence_hi, int n, size_t expected_size) {
    for (int i = 0; i < n; i++) {
        ulnet_packet_ref_t *ref = &session->packet_reference[sequence_hi][i];
        if (ref->data != NULL && ref->size != expected_size) {
            return false;
        }
    }
    return true;
}

static void ulnet__reset_to_local_solo(ulnet_session_t *session);

static uint8_t ulnet__savestate_transfer_id(uint8_t channel_and_flags) {
    return (uint8_t)((channel_and_flags & ULNET_SAVESTATE_TRANSFER_ID_MASK) >> ULNET_SAVESTATE_TRANSFER_ID_SHIFT);
}

static uint8_t ulnet__savestate_transfer_id_flags(uint8_t transfer_id) {
    return (uint8_t)((transfer_id << ULNET_SAVESTATE_TRANSFER_ID_SHIFT) & ULNET_SAVESTATE_TRANSFER_ID_MASK);
}

static void ulnet__savestate_tx_clear(ulnet_session_t *session) {
    session->savestate_transfer_awaiting_bitfield = 0;
    session->savestate_transfer_ack_deadline_unix_usec = 0;
}

static void ulnet__savestate_tx_poll(ulnet_session_t *session) {
    if (!session->savestate_transfer_awaiting_bitfield
        || ulnet__get_unix_time_microseconds() < session->savestate_transfer_ack_deadline_unix_usec) {
        return;
    }

    uint64_t retry_bitfield = session->savestate_transfer_awaiting_bitfield | session->peer_needs_sync_bitfield;
    if (session->savestate_transfer_retry_count >= ULNET_SAVESTATE_TRANSFER_MAX_RETRIES) {
        SAM2_LOG_ERROR("Savestate transfer failed after %d retries for peers 0x%016" PRIx64 "; returning to solo session",
            (int)session->savestate_transfer_retry_count, retry_bitfield);
        ulnet__reset_to_local_solo(session);
        return;
    }

    session->savestate_transfer_retry_count++;
    SAM2_LOG_WARN("Savestate transfer id=%u failed for peers 0x%016" PRIx64 "; retrying at %" PRId64 " bits/s",
        (unsigned)session->savestate_transfer_id, retry_bitfield,
        ULNET_SAVESTATE_TRANSFER_DEFAULT_BANDWIDTH_BITS_PER_SECOND / (1LL << session->savestate_transfer_retry_count));
    ulnet__savestate_tx_clear(session);
    session->peer_needs_sync_bitfield = retry_bitfield;
}

static void ulnet__reset_to_local_solo(ulnet_session_t *session) {
    ulnet__savestate_tx_clear(session);

    for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
        if (session->agent[i]) {
            ulnet_disconnect_peer(session, i); // also clears history + peer-defaults the slot
        } else {
            ulnet_clear_peer_packet_history(session, i);
            ulnet_peer_init_defaulted(session, i);
        }
    }
    ulnet__reset_save_state_bookkeeping(session);

    memset(&session->room_we_are_in, 0, sizeof(session->room_we_are_in));
    session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session->our_peer_id;
    session->room_we_are_in.peer_topology = (1ULL << SAM2_AUTHORITY_INDEX);
    session->next_room = session->room_we_are_in;
    session->next_room_effective_frame = 0;
    session->authority_room_snapshot_last_sent_frame = -1;
    session->frame_counter = 0;
    session->peer_needs_sync_bitfield = 0;
    session->savestate_transfer_retry_count = 0;
    session->remote_savestate_completed_transfer_id = 0xff;
}

ULNET_LINKAGE void ulnet_session_tear_down(ulnet_session_t *session) {
    if (session->agent[SAM2_AUTHORITY_INDEX]) {
        ulnet_message_send(session, SAM2_AUTHORITY_INDEX, (const uint8_t *) ulnet_exit_header);
    }

    ulnet__reset_to_local_solo(session);
    session->state[SAM2_AUTHORITY_INDEX].frame = 0;
}

ULNET_LINKAGE void ulnet_session_init_defaulted(ulnet_session_t *session) {
    for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
        assert(session->agent[i] == NULL);
    }

    memset(&session->state, 0, sizeof(session->state));
    memset(&session->room_we_are_in, 0, sizeof(session->room_we_are_in));
    session->reliable_retransmit_delay_microseconds = 50000; // 50 milliseconds
    session->compression_quality = 8;

    ulnet__reset_to_local_solo(session);
}

static void ulnet__check_for_desync(ulnet_state_t *our_state, ulnet_state_t *their_state, int64_t *our_desync_frame) {
    int64_t desync_frame = 0;
    int64_t latest_common_frame = SAM2_MIN(our_state->save_state_frame, their_state->save_state_frame);
    int64_t frame_difference = SAM2_ABS(our_state->save_state_frame - their_state->save_state_frame);
    int64_t total_frames_to_compare = ULNET_DELAY_BUFFER_SIZE - frame_difference;

    for (int f = total_frames_to_compare-1; f >= 0 ; f--) { // Start from the oldest frame
        int64_t frame = latest_common_frame - f;
        int64_t frame_index = frame % ULNET_DELAY_BUFFER_SIZE;

        if (our_state->input_state_hash[frame_index] != their_state->input_state_hash[frame_index]) {
            SAM2_LOG_ERROR("Input state hash mismatch for frame %" PRId64 " Our hash: %" PRIx32 " Their hash: %" PRIx32 "",
                frame, our_state->input_state_hash[frame_index], their_state->input_state_hash[frame_index]);
        } else if (   our_state->save_state_hash[frame_index] != 0
                   && their_state->save_state_hash[frame_index] != 0
                   && our_state->save_state_hash[frame_index] != their_state->save_state_hash[frame_index]) {
            SAM2_LOG_ERROR("Save state hash mismatch for frame %" PRId64 " Our hash: %08" PRIx32 " Their hash: %08" PRIx32,
                frame, our_state->save_state_hash[frame_index], their_state->save_state_hash[frame_index]);
            desync_frame = frame;
            break;
        }
    }

    if (desync_frame == 0 && *our_desync_frame != 0) {
        SAM2_LOG_INFO("Peer resynced on frame %" PRId64, total_frames_to_compare-1);
    }

    *our_desync_frame = desync_frame;
}

ULNET_LINKAGE void ulnet__process_udp_packet(ulnet_session_t *session, int p, const uint8_t *data, size_t size);

static bool ulnet__estimate_ping_usec(int64_t local_send_usec, int64_t remote_receive_usec, int64_t remote_send_usec,
    int64_t local_receive_usec, int64_t *delay_usec, int64_t *offset_usec) {
    if (   local_send_usec <= 0
        || remote_receive_usec <= 0
        || remote_send_usec < remote_receive_usec
        || local_receive_usec < local_send_usec) {
        return false;
    }

    // RFC 5905 theta/delta with local T1/T4 and remote T2/T3.
    *delay_usec = (local_receive_usec - local_send_usec) - (remote_send_usec - remote_receive_usec);
    *offset_usec = ((remote_receive_usec - local_send_usec) + (remote_send_usec - local_receive_usec)) / 2;
    return *delay_usec >= 0;
}

static void ulnet__process_state_packet_ping(ulnet_session_t *session, int p, const uint8_t *data, size_t size,
    int64_t callsite_receive_time_usec, int64_t kernel_receive_time_usec) {
    if (size < sizeof(ulnet_state_packet_t) || !ulnet__is_input_channel(data[0])) return;

    const ulnet_state_packet_t *state_packet = (const ulnet_state_packet_t *) data;
    int64_t remote_send_usec = ulnet__read_le64s(state_packet->ping_send_unix_usec_le);
    int64_t echoed_local_send_usec = ulnet__read_le64s(state_packet->ping_echo_send_unix_usec_le);
    int64_t remote_callsite_receive_usec = ulnet__read_le64s(state_packet->ping_echo_callsite_receive_unix_usec_le);
    int64_t remote_kernel_receive_usec = ulnet__read_le64s(state_packet->ping_echo_kernel_receive_unix_usec_le);

    if (remote_send_usec > 0) {
        session->peer_last_packet_send_unix_usec[p] = remote_send_usec;
        session->peer_last_packet_callsite_receive_unix_usec[p] = callsite_receive_time_usec;
        session->peer_last_packet_kernel_receive_unix_usec[p] = kernel_receive_time_usec;
    }

    int64_t delay_usec = 0;
    int64_t offset_usec = 0;
    if (ulnet__estimate_ping_usec(echoed_local_send_usec, remote_callsite_receive_usec,
        remote_send_usec, callsite_receive_time_usec, &delay_usec, &offset_usec)) {
        session->peer_packet_ping_usec[p] = delay_usec;
        session->peer_clock_offset_usec[p] = offset_usec;
        session->peer_packet_ping_samples[p]++;
    }

    if (ulnet__estimate_ping_usec(echoed_local_send_usec, remote_kernel_receive_usec,
        remote_send_usec, kernel_receive_time_usec, &delay_usec, &offset_usec)) {
        session->peer_packet_kernel_ping_usec[p] = delay_usec;
        session->peer_kernel_clock_offset_usec[p] = offset_usec;
        session->peer_packet_kernel_ping_samples[p]++;
    }
}

// MARK: UDP Packet Processing
ULNET_LINKAGE void ulnet_receive_packet_callback(ulnet_nat_agent_t *agent, const char *packet, size_t size, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p;
    SAM2_LOCATE(session->agent, agent, p);
    if (p == -1) {
        SAM2_LOG_ERROR("No agent associated for packet on channel 0x%" PRIx8 "", packet[0] & ULNET_CHANNEL_MASK);
        return;
    }

    if (rand() / ((float) RAND_MAX) < session->debug_udp_recv_drop_rate) {
        SAM2_LOG_DEBUG("Intentionally dropped a received UDP packet");
        return;
    }

    if (session->flags & ULNET_SESSION_FLAG_DRAW_IMGUI) { // imgui-debug-only history; skip otherwise
        ulnet_packet_ref_set(&session->packet_history[p][session->packet_history_next[p]++], packet, size, 0);
    }

    if (   (packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_RELIABLE
        && !(packet[0] & ULNET_RELIABLE_FLAG_ACK_ONLY)
        && size >= sizeof(ulnet_reliable_packet_t)) {
        uint16_t sequence = ((uint16_t)packet[2] << 8) | packet[1];
        ulnet_packet_ref_set(&session->reliable_rx_packet_history[p][sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE], packet, size, 0);
    }

    if (session->flags & ULNET_SESSION_FLAG_READY_TO_TICK_SET) {
        SAM2_LOG_ERROR("Received a UDP packet while we were ready to tick. Set a breakpoint here to investigate");
    }

    ulnet__process_udp_packet(session, p, (const uint8_t *) packet, size); // Fallthrough to the next function
    session->pending_packet_kernel_receive_unix_usec = 0;
}

ULNET_LINKAGE void ulnet__process_udp_packet(ulnet_session_t *session, int p, const uint8_t *data, size_t size) {
    if (size == 0) {
        SAM2_LOG_WARN("Received a UDP packet with no payload");
        return;
    }

    uint8_t channel_and_flags = data[0];

    switch (channel_and_flags & ULNET_CHANNEL_MASK) {
    case ULNET_CHANNEL_EXTRA: {
        SAM2_LOG_WARN("Received packet on reserved/invalid channel (header byte 0x%02" PRIx8 ") -- likely a zeroed/corrupt header", channel_and_flags);
        break;
    }
    case ULNET_CHANNEL_ASCII: {
        SAM2_LOG_INFO("Received message with header '%.*s' from peer %05" PRIu16 " on channel 0x%" PRIx8 " with %zu bytes",
            (int)SAM2_MIN(size, SAM2_HEADER_SIZE), data, session->room_we_are_in.peer_ids[p], channel_and_flags & ULNET_CHANNEL_MASK, size);

        if (sam2_header_matches((const char *) data, ulnet_exit_header)) {
            bool leaver_is_player = ulnet_port_is_p2p(&session->room_we_are_in, p);
            session->peer_pending_disconnect_bitfield |= (1ULL << p);

            if (ulnet_is_authority(session)) {
                ulnet__authority_remove_peer(session, ulnet__authority_desired_room(session), p, leaver_is_player);
            }
        } else if (sam2_header_matches((const char *) data, sam2_join_header)) {
            // @todo This can be much simpler
            sam2_room_join_message_t join_message;
            memcpy(&join_message, data, sizeof(join_message));
            join_message.peer_id = session->room_we_are_in.peer_ids[p];
            if (ulnet_process_message(session, (const char *) &join_message) != 0) {
                SAM2_LOG_ERROR("Failed to process join message");
            }
        }

        break;
    }
    case ULNET_CHANNEL_RELIABLE: {
        if (size < sizeof(ulnet_reliable_packet_t)) {
            SAM2_LOG_WARN("Reliable packet missing header");
            break;
        }

        ulnet_reliable_packet_t *reliable_packet = (ulnet_reliable_packet_t *) data;

        uint16_t old_tx_head = session->reliable_tx_head[p];
        uint16_t ack_sequence = (reliable_packet->ack_sequence_le[1] << 8) | reliable_packet->ack_sequence_le[0];
        if (!ulnet__sequence_in_range_inclusive(ack_sequence, old_tx_head, session->reliable_tx_next_seq[p])) {
            SAM2_LOG_WARN("Ignoring invalid reliable ACK seq=%u from peer %05" PRIu16 " outside tx queue [%u, %u]",
                ack_sequence, session->room_we_are_in.peer_ids[p], old_tx_head, session->reliable_tx_next_seq[p]);
            ack_sequence = old_tx_head;
        } else if (ulnet__sequence_greater_than(ack_sequence, old_tx_head)) {
            session->reliable_tx_head[p] = ack_sequence;
        }

        bool process_payload = true;
        uint16_t rx_sequence;
        if (!(channel_and_flags & ULNET_RELIABLE_FLAG_ACK_ONLY)) {
            memcpy(&rx_sequence, &reliable_packet->sequence_le, sizeof(rx_sequence));

            if (rx_sequence == session->reliable_rx_head[p]) {
                session->reliable_rx_head[p]++;
            } else if (ulnet__sequence_greater_than(rx_sequence, session->reliable_rx_head[p])) {
                SAM2_LOG_ERROR("Received out-of-order reliable packet seq=%d, expecting %d",
                    rx_sequence, session->reliable_rx_head[p]); // @todo This is a protocol violation, we should disconnect the peer
                process_payload = false;
            } else {
                SAM2_LOG_DEBUG("Received old reliable packet seq=%d, already processed", rx_sequence);
                process_payload = false;
            }
        }

        if (ulnet__sequence_greater_than(ack_sequence, old_tx_head)) {
            ulnet__reliable_send_head(session, p, false);
        }
        if (process_payload) {
            ulnet__process_udp_packet(session, p, data + sizeof(ulnet_reliable_packet_t), size - sizeof(ulnet_reliable_packet_t));
        }
        break;
    }
    case ULNET_CHANNEL_INPUT_HI:
    case ULNET_CHANNEL_INPUT: {
        assert(size <= ULNET_PACKET_SIZE_BYTES_MAX);

        int64_t receive_time_usec = ulnet__get_unix_time_microseconds();

        ulnet_state_packet_t *input_packet = (ulnet_state_packet_t *) data; // @todo Violates strict aliasing rule
        int original_sender_port = data[0] & 0b00111111;

        if (   p != original_sender_port
            && p != SAM2_AUTHORITY_INDEX) {
            SAM2_LOG_WARN("Non-authority gave us someones input eventually this should be verified with a signature");
            return;
        }

        bool sender_is_authority = p == SAM2_AUTHORITY_INDEX;
        bool sender_is_mesh_player = ulnet_port_is_p2p(&session->room_we_are_in, p);
        bool original_is_authority = original_sender_port == SAM2_AUTHORITY_INDEX;
        bool original_is_mesh_player = ulnet_port_is_p2p(&session->room_we_are_in, original_sender_port);

        if (!sender_is_authority && !sender_is_mesh_player) {
            SAM2_LOG_WARN("A spectator sent us a UDP packet for unsupported channel ULNET_CHANNEL_INPUT");
            return;
        }

        if (!original_is_authority && !original_is_mesh_player) {
            SAM2_LOG_WARN("Received input packet for non-player port %d", original_sender_port);
            return;
        }

        if (p == original_sender_port) {
            ulnet__process_state_packet_ping(session, p, data, size, receive_time_usec,
                session->pending_packet_kernel_receive_unix_usec);
        }

        int64_t coded_state_size = size - sizeof(ulnet_state_packet_t);

        int64_t frame;
        rle8_decode(input_packet->coded_state, coded_state_size, (uint8_t *) &frame, sizeof(frame));

        SAM2_LOG_DEBUG("Recv input packet for frame %" PRId64 " from peer_ids[%d]=%05" PRId16 "",
            frame, original_sender_port, session->room_we_are_in.peer_ids[original_sender_port]);

        if (frame < session->state[original_sender_port].frame) {
            // UDP packets can arrive out of order this is normal
            SAM2_LOG_DEBUG("Received outdated input packet for frame %" PRId64 ". We are already on frame %" PRId64 ". Dropping it",
                frame, session->state[original_sender_port].frame);
        } else {
            int64_t input_consumed = 0;
            int64_t output_produced = rle8_decode_extra(
                input_packet->coded_state, coded_state_size,
                &input_consumed,
                (uint8_t *) &session->state[original_sender_port],
                sizeof(ulnet_state_t)
            );

            if (input_consumed != coded_state_size) {
                SAM2_LOG_WARN("Received input packet with oversize payload %" PRId64 " bytes left to decode", input_consumed - coded_state_size);
            } else if (output_produced != sizeof(ulnet_state_t)) {
                SAM2_LOG_WARN("Received input packet with insuffcient size %" PRId64 " bytes produced", output_produced);
            }

            ulnet_update_state_history(session, data, size);

            // Relay this player's input to every other peer (the always-on client-server path).
            // Spectators depend on this; p2p players may also receive it directly -- duplicate
            // frame-stamped input is harmless (older frames are dropped above).
            if (ulnet_is_authority(session)) {
                for (int s = 0; s < SAM2_TOTAL_PEERS; s++) {
                    if (s == p) continue; // Don't echo back to the sender
                    if (!session->agent[s]) continue;
                    uint8_t relayed_packet[ULNET_PACKET_SIZE_BYTES_MAX];
                    memcpy(relayed_packet, data, size);
                    ulnet__stamp_state_packet_ping(session, s, relayed_packet, ulnet__get_unix_time_microseconds());
                    ulnet_reliable_send_with_acks_only(session, s, relayed_packet, size);
                }
            }
        }

        // Check for desync
        int our_port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);
        if (   our_port != -1
            && ulnet_port_is_p2p(&session->room_we_are_in, our_port)
            && original_is_mesh_player) {
            ulnet__check_for_desync(
                &session->state[our_port],
                &session->state[original_sender_port],
                &session->peer_desynced_frame[our_port]
            );
        }

        break;
    }
    case ULNET_CHANNEL_SPECTATOR_INPUT: {
        rle8_decode(
            (const uint8_t *) &data[sizeof(ulnet_state_packet_t)], size - sizeof(ulnet_state_packet_t),
            (uint8_t *) &session->spectator_suggested_input_state[p], sizeof(session->spectator_suggested_input_state[p])
        );

        break;
    }
    case ULNET_CHANNEL_SAVESTATE_TRANSFER: {
        if (session->remote_packet_groups == 0) {
            // This is kind of a hack. Since every field in ulnet_session can just be zero-inited
            // besides this one. I just use this check here to set it to it's correct initial value
            session->remote_packet_groups = FEC_PACKET_GROUPS_MAX;
        }

        if (size < sizeof(ulnet_save_state_packet_header_t)) {
            SAM2_LOG_WARN("Recv savestate transfer packet with size smaller than header");
            break;
        }

        if (size > ULNET_PACKET_SIZE_BYTES_MAX) {
            SAM2_LOG_WARN("Recv savestate transfer packet potentially larger than MTU");
        }

        if (channel_and_flags & ULNET_SAVESTATE_TRANSFER_FLAG_ACK) {
            uint8_t transfer_id = ulnet__savestate_transfer_id(channel_and_flags);
            if (   ulnet_is_authority(session)
                && transfer_id == session->savestate_transfer_id
                && (session->savestate_transfer_awaiting_bitfield & (1ULL << p))) {
                session->savestate_transfer_awaiting_bitfield &= ~(1ULL << p);
                if (session->savestate_transfer_awaiting_bitfield == 0) {
                    SAM2_LOG_INFO("Savestate transfer id=%u completed", (unsigned)session->savestate_transfer_id);
                    ulnet__savestate_tx_clear(session);
                    session->savestate_transfer_retry_count = 0;
                }
            }
            break;
        }

        if (p != SAM2_AUTHORITY_INDEX) {
            printf("Received savestate transfer packet from non-authority agent\n");
            break;
        }

        ulnet_save_state_packet_header_t savestate_transfer_header;
        memcpy(&savestate_transfer_header, data, sizeof(ulnet_save_state_packet_header_t)); // Strict-aliasing

        uint8_t transfer_id = ulnet__savestate_transfer_id(channel_and_flags);
        if (session->remote_savestate_completed_transfer_id == transfer_id) {
            break;
        }
        if (session->remote_savestate_transfer_id != transfer_id) {
            ulnet__reset_save_state_bookkeeping(session);
            session->remote_savestate_transfer_id = transfer_id;
            session->remote_savestate_completed_transfer_id = 0xff;
        }

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
        if (k == 0 || k > ULNET_RS_DATA_BLOCKS_MAX) {
            SAM2_LOG_WARN("Received savestate transfer packet with invalid k");
            break;
        }
        if (session->remote_packet_groups == 0 || session->remote_packet_groups > FEC_PACKET_GROUPS_MAX) {
            SAM2_LOG_WARN("Received savestate transfer packet with invalid packet group count");
            break;
        }
        int n = ulnet__rs_total_block_count(k);

        if (sequence_hi >= FEC_PACKET_GROUPS_MAX) {
            SAM2_LOG_WARN("Received savestate transfer packet with sequence_hi >= FEC_PACKET_GROUPS_MAX");
            break;
        }

        if (session->fec_index_counter[sequence_hi] == k) {
            // We already have received enough Reed-Solomon blocks to decode the payload; we can ignore this packet
            break;
        }

        uint8_t sequence_lo = savestate_transfer_header.sequence_lo;
        if (sequence_lo >= n) {
            SAM2_LOG_WARN("Received savestate transfer packet with sequence_lo >= n");
            break;
        }

        // Store by shard number so decode sees the same systematic layout the sender encoded.
        if (session->packet_reference[sequence_hi][sequence_lo].data != NULL) {
            break;
        }

        SAM2_LOG_DEBUG("Received savestate packet sequence_hi: %hhu sequence_lo: %hhu", sequence_hi, sequence_lo);

        size_t payload_size = size - sizeof(ulnet_save_state_packet_header_t);

        if (!ulnet__savestate_blocks_size_consistent(session, sequence_hi, n, payload_size)) {
            SAM2_LOG_WARN("Received savestate transfer packet with mismatched block size");
            ulnet__reset_save_state_bookkeeping(session);
            break;
        }

        if (ulnet_packet_ref_set(
            &session->packet_reference[sequence_hi][sequence_lo],
            data + sizeof(ulnet_save_state_packet_header_t),
            payload_size,
            0
        ) != 0) {
            SAM2_LOG_ERROR("Failed to store savestate transfer packet");
            break;
        }
        session->fec_index_counter[sequence_hi]++;

        if (session->fec_index_counter[sequence_hi] == k) {
            int rs_block_size;
            int64_t compressed_data_size;
            int64_t compressed_savestate_size;
            int64_t compressed_options_size;
            int32_t remote_payload_size;
            bool all_data_decoded = true;
            uint32_t their_savestate_transfer_payload_checksum;
            uint32_t our_savestate_transfer_payload_checksum;
            unsigned char *save_state_data = NULL;
            savestate_transfer_payload_t *savestate_transfer_payload = NULL;

            SAM2_LOG_DEBUG("Received all the savestate data for packet group: %hhu", sequence_hi);
            rs_block_size = (int) (size - sizeof(ulnet_save_state_packet_header_t));
            if (ulnet__rs_decode(session->packet_reference[sequence_hi], k, n, rs_block_size) != 0) {
                SAM2_LOG_ERROR("Failed to decode savestate transfer packet group");
                goto cleanup;
            }

            for (int i = 0; i < session->remote_packet_groups; i++) {
                all_data_decoded &= session->fec_index_counter[i] >= k;
            }

            if (!all_data_decoded) {
                break;
            }

            savestate_transfer_payload = (savestate_transfer_payload_t *) ULNET_MALLOC(sizeof(savestate_transfer_payload_t) /* Fixed size header */ + k * session->remote_packet_groups * rs_block_size);
            if (savestate_transfer_payload == NULL) {
                SAM2_LOG_ERROR("Failed to allocate savestate transfer payload");
                ulnet__reset_save_state_bookkeeping(session);
                break;
            }

            remote_payload_size = 0;
            for (int i = 0; i < k; i++) {
                for (int j = 0; j < session->remote_packet_groups; j++) {
                    void *decoded_packet = session->packet_reference[j][i].data;
                    memcpy(((uint8_t *) savestate_transfer_payload) + remote_payload_size, decoded_packet, rs_block_size);
                    remote_payload_size += rs_block_size;
                }
            }

            SAM2_LOG_INFO("Received savestate transfer payload for frame %" PRId64 "", savestate_transfer_payload->frame_counter);

            if (   savestate_transfer_payload->total_size_bytes > k * (int) rs_block_size * session->remote_packet_groups
                || savestate_transfer_payload->total_size_bytes < (int64_t)sizeof(savestate_transfer_payload_t)) {
                SAM2_LOG_ERROR("Savestate transfer payload total size would out-of-bounds when computing hash: %" PRId64 "", savestate_transfer_payload->total_size_bytes);
                goto cleanup;
            }

            compressed_data_size = savestate_transfer_payload->total_size_bytes - (int64_t)sizeof(savestate_transfer_payload_t);
            compressed_savestate_size = savestate_transfer_payload->compressed_savestate_size;
            compressed_options_size = savestate_transfer_payload->compressed_options_size;
            if (   compressed_savestate_size < 0
                || compressed_options_size < 0
                || compressed_savestate_size + compressed_options_size > compressed_data_size
                || savestate_transfer_payload->decompressed_savestate_size <= 0) {
                SAM2_LOG_ERROR("Savestate transfer payload has invalid compressed size fields");
                goto cleanup;
            }

            their_savestate_transfer_payload_checksum = savestate_transfer_payload->checksum;
            savestate_transfer_payload->checksum = 0; // Needed to recompute the hash correctly
            our_savestate_transfer_payload_checksum = ulnet_xxh32(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes, 0);

            if (their_savestate_transfer_payload_checksum != our_savestate_transfer_payload_checksum) {
                SAM2_LOG_ERROR("Savestate transfer payload hash mismatch: %" PRIx32 " != %" PRIx32 "", savestate_transfer_payload->checksum, our_savestate_transfer_payload_checksum);
                goto cleanup;
            }

            if (ULNET_ZSTD_DECOMPRESS(
                session->core_options, sizeof(session->core_options),
                savestate_transfer_payload->compressed_data + savestate_transfer_payload->compressed_savestate_size,
                savestate_transfer_payload->compressed_options_size
            ) < 0) {
                SAM2_LOG_ERROR("ZSTD decompression failed for core options");
                goto cleanup;
            }

            session->flags |= ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY;
            //session.retro_run(); // Apply options before loading savestate; Lets hope this isn't necessary

            save_state_data = (unsigned char *) ULNET_MALLOC(savestate_transfer_payload->decompressed_savestate_size);
            if (save_state_data == NULL) {
                SAM2_LOG_ERROR("Failed to allocate decompressed savestate buffer");
                goto cleanup;
            }

            int64_t save_state_size = ULNET_ZSTD_DECOMPRESS(
                save_state_data,
                savestate_transfer_payload->decompressed_savestate_size,
                savestate_transfer_payload->compressed_data,
                savestate_transfer_payload->compressed_savestate_size
            );
            if (save_state_size < 0) {
                SAM2_LOG_ERROR("ZSTD decompression failed for savestate");
                goto cleanup;
            }
            if (!session->retro_unserialize(session->user_ptr, save_state_data, save_state_size)) {
                SAM2_LOG_ERROR("Failed to load savestate");
                goto cleanup;
            }
            SAM2_LOG_DEBUG("Save state loaded");
            session->frame_counter = savestate_transfer_payload->frame_counter;
            session->room_we_are_in = savestate_transfer_payload->room;
            session->remote_savestate_completed_transfer_id = transfer_id;
            ulnet_save_state_packet_header_t ack = {0};
            ack.channel_and_flags = ULNET_CHANNEL_SAVESTATE_TRANSFER
                | ULNET_SAVESTATE_TRANSFER_FLAG_ACK
                | ulnet__savestate_transfer_id_flags(transfer_id);
            ulnet_udp_send(session, p, (const uint8_t *)&ack, sizeof(ack));
cleanup:
            ULNET_FREE(save_state_data);
            ULNET_FREE(savestate_transfer_payload);
            ulnet__reset_save_state_bookkeeping(session);
        }
        break;
    }
    default:
        SAM2_LOG_WARN("Unknown channel: 0x%" PRIx8 " with flags: 0x%" PRIx8 , (data[0] & ULNET_CHANNEL_MASK) >> 4, data[0] & ULNET_FLAGS_MASK);
    }
}


ULNET_LINKAGE void ulnet_startup_nat_for_peer(ulnet_session_t *session, uint64_t peer_id, int p, const char *remote_signal) {
    if (p < 0 || p >= SAM2_TOTAL_PEERS) {
        SAM2_LOG_FATAL("Invalid peer port %d", p);
    }

    if (peer_id <= SAM2_PORT_SENTINELS_MAX) {
        SAM2_LOG_FATAL("Peer ID cannot be zero");
    }

    SAM2_LOG_INFO("Starting ulnet NAT traversal for peer %05" PRId64, peer_id);

    session->room_we_are_in.peer_ids[p] = peer_id;

    assert(session->agent[p] == NULL);
    session->agent[p] = ulnet__nat_create(session, p);
    if (!session->agent[p]) {
        SAM2_LOG_ERROR("Failed to create ulnet NAT agent");
        return;
    }

    if (remote_signal) {
        ulnet__nat_process_signal(session->agent[p], remote_signal);
    }

    ulnet__nat_send_host_candidates(session->agent[p]);
}

int ulnet_process_message(ulnet_session_t *session, const char *response) {

    if (sam2_get_metadata((char *) response) == NULL) {
        return -1;
    }

    if (sam2_header_matches(response, sam2_conn_header)) {
        sam2_connect_message_t *connect_message = (sam2_connect_message_t *) response;
        SAM2_LOG_INFO("We were assigned the peer id %05" PRIu16, (uint16_t) connect_message->peer_id);

        session->our_peer_id = connect_message->peer_id;
        session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session->our_peer_id;
    } else if (sam2_header_matches(response, sam2_make_header)) {
        sam2_room_make_message_t *room_make = (sam2_room_make_message_t *) response;

        if (!(session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)) {
            session->room_we_are_in = room_make->room;
            session->next_room = session->room_we_are_in;
            session->next_room_effective_frame = 0;
        }
    } else if (sam2_header_matches(response, sam2_join_header)) {
        // A peer (or the local GUI) requests a room change: leave, or toggle player<->spectator at its
        // own port. Peers never move ports -- a slot is a fixed identity. The authority edits its desired
        // room (next_room); active-set changes are scheduled ahead, spectator edits are immediate.
        sam2_room_join_message_t *room_join = (sam2_room_join_message_t *) response;
        if (!ulnet_is_authority(session)) {
            SAM2_LOG_WARN("Only the authority handles join requests");
            return -1;
        }
        if (room_join->room.peer_ids[SAM2_AUTHORITY_INDEX] != session->our_peer_id) {
            SAM2_LOG_WARN("Join request targets a different room");
            return -1;
        }

        sam2_room_t *desired = ulnet__authority_desired_room(session); // &next_room
        uint16_t peer_id = (uint16_t) room_join->peer_id;
        int current_port   = sam2_get_port_of_peer(desired, peer_id);
        int requested_port = sam2_get_port_of_peer(&room_join->room, peer_id);

        SAM2_LOG_INFO("Peer %05" PRId16 " requested a room change (current_port=%d requested_port=%d)", peer_id, current_port, requested_port);

        // The authority may toggle room-level flags on itself (e.g. abandon the room)
        if (peer_id == session->our_peer_id) {
            desired->flags = room_join->room.flags;
            session->room_we_are_in.flags = room_join->room.flags;
        }

        if (current_port == -1) {
            SAM2_LOG_WARN("Peer %05" PRId16 " is not in the room; connect (signal) first to spectate", peer_id);
        } else if (requested_port == -1) {
            // Leave. A departing player is dropped on a scheduled frame so everyone stops awaiting
            // its input together; the reconcile tears its agent down after its final input arrives.
            ulnet__authority_remove_peer(session, desired, current_port, ulnet_port_is_p2p(desired, current_port));
        } else if (requested_port != current_port) {
            SAM2_LOG_WARN("Peer %05" PRId16 " requested a port move which is unsupported (slots are fixed)", peer_id);
        } else {
            // Same port: maybe a player<->spectator topology change
            bool wants_player = (room_join->room.peer_topology >> current_port) & 1ULL;
            bool is_player    = ulnet_port_is_p2p(desired, current_port);
            if (wants_player != is_player) {
                if (ulnet__authority_has_pending_room_change(session)) {
                    SAM2_LOG_WARN("Ignoring active-set change from peer %05" PRId16 " while another room change is pending", peer_id);
                } else {
                    if (wants_player) desired->peer_topology |=  (1ULL << current_port);
                    else              desired->peer_topology &= ~(1ULL << current_port);
                    ulnet__schedule_active_set_change(session);
                }
            } else {
                SAM2_LOG_WARN("Join request from peer %05" PRId16 " changed nothing", peer_id);
            }
        }
    }  else if (sam2_header_matches(response, sam2_sign_header)) {
        sam2_signal_message_t *room_signal = (sam2_signal_message_t *) response;
        SAM2_LOG_INFO("Received signal from peer %05" PRId16 "", room_signal->peer_id);

        int p = sam2_get_port_of_peer(&session->room_we_are_in, room_signal->peer_id);

        if (p == -1 && ulnet_is_authority(session)) {
            for (p = SAM2_AUTHORITY_INDEX + 1; p < SAM2_TOTAL_PEERS; p++) if (session->room_we_are_in.peer_ids[p] == SAM2_PORT_AVAILABLE) break;

            if (p == SAM2_TOTAL_PEERS) {
                SAM2_LOG_WARN("We can't let them in as a spectator there are too many spectators");
                static sam2_error_message_t error = {
                    SAM2_FAIL_HEADER, 0,
                    "Authority has reached the maximum number of spectators",
                    SAM2_RESPONSE_ROOM_FULL
                };
                error.peer_id = room_signal->peer_id;
                session->sam2_send_callback(session->user_ptr, (char *) &error);
                p = -1;
            } else {
                SAM2_LOG_INFO("Admitting peer %05" PRId16 " as a spectator at slot %d", room_signal->peer_id, p);
            }
        } else if (p == -1) {
            SAM2_LOG_WARN("Received unknown signal when we weren't the authority");
            static sam2_error_message_t error = {
                SAM2_FAIL_HEADER, 0,
                "Received unknown signal when we weren't the authority",
                SAM2_RESPONSE_PEER_ERROR
            };
            error.peer_id = room_signal->peer_id;
            session->sam2_send_callback(session->user_ptr, (char *) &error);
        }

        if (p != -1 && session->agent[p] == NULL) {
            SAM2_LOG_INFO("Creating NAT agent for peer %05" PRId16 " at slot %d", room_signal->peer_id, p);
            ulnet_startup_nat_for_peer(session, room_signal->peer_id, p, /* remote_signal = */ room_signal->ice_sdp); // sets peer_ids[p]

            // Advertise the new spectator so other peers learn of it in authority state packets.
            if (ulnet_is_authority(session)) {
                session->next_room.peer_ids[p] = room_signal->peer_id; // topology bit stays clear (spectator)
            }
        } else if (p != -1 && session->agent[p]) {
            if (ulnet__nat_process_signal(session->agent[p], room_signal->ice_sdp)) {
                SAM2_LOG_ERROR("Unable to add NAT candidate '%s'", room_signal->ice_sdp);
            }
        }
    }

    return 0;
}

static int ulnet__send_save_state_to_peers(ulnet_session_t *session, uint64_t peer_bitfield, void *save_state,
    size_t save_state_size, int64_t save_state_frame) {
    assert(save_state);
    peer_bitfield &= ~(1ULL << SAM2_AUTHORITY_INDEX);
    if (session->savestate_transfer_awaiting_bitfield) {
        session->peer_needs_sync_bitfield |= peer_bitfield;
        return -1;
    }
    int peer_count = 0;
    for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
        if ((peer_bitfield & (1ULL << p)) && !ulnet__peer_link_ready(session, p)) {
            peer_bitfield &= ~(1ULL << p);
            session->peer_needs_sync_bitfield |= (1ULL << p);
        }
        if (peer_bitfield & (1ULL << p)) peer_count++;
    }
    if (peer_bitfield == 0) return 0;

    int packet_payload_size_bytes = ULNET_PACKET_SIZE_BYTES_MAX - sizeof(ulnet_save_state_packet_header_t);
    int n, k, packet_groups;

    int64_t save_state_transfer_payload_compressed_bound_size_bytes = (int64_t)ULNET_ZSTD_COMPRESS_BOUND(save_state_size)
        + (int64_t)ULNET_ZSTD_COMPRESS_BOUND(sizeof(session->core_options));
    ulnet__logical_partition(sizeof(savestate_transfer_payload_t) /* Header */ + save_state_transfer_payload_compressed_bound_size_bytes,
                      FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size_bytes, &packet_groups);

    size_t savestate_transfer_payload_plus_parity_bound_bytes = packet_groups * n * packet_payload_size_bytes;

    // This points to the savestate transfer payload, but also the remaining bytes at the end hold our parity blocks
    // Having this data in a single contiguous buffer makes indexing easier
    savestate_transfer_payload_t *savestate_transfer_payload = (savestate_transfer_payload_t *) ULNET_MALLOC(savestate_transfer_payload_plus_parity_bound_bytes);
    if (!savestate_transfer_payload) {
        SAM2_LOG_ERROR("Failed to allocate savestate transfer payload");
        return -1;
    }

    savestate_transfer_payload->decompressed_savestate_size = save_state_size;
    int64_t compressed_savestate_size = ULNET_ZSTD_COMPRESS(
        savestate_transfer_payload->compressed_data,
        save_state_transfer_payload_compressed_bound_size_bytes,
        save_state, save_state_size, session->compression_quality
    );

    if (compressed_savestate_size < 0 || compressed_savestate_size > INT32_MAX) {
        SAM2_LOG_ERROR("ZSTD compression failed for savestate");
        ULNET_FREE(savestate_transfer_payload);
        return -1;
    }
    savestate_transfer_payload->compressed_savestate_size = (int32_t)compressed_savestate_size;

    int64_t compressed_options_size = ULNET_ZSTD_COMPRESS(
        savestate_transfer_payload->compressed_data + savestate_transfer_payload->compressed_savestate_size,
        save_state_transfer_payload_compressed_bound_size_bytes - savestate_transfer_payload->compressed_savestate_size,
        session->core_options, sizeof(session->core_options), session->compression_quality
    );

    if (compressed_options_size < 0 || compressed_options_size > INT32_MAX) {
        SAM2_LOG_ERROR("ZSTD compression failed for core options");
        ULNET_FREE(savestate_transfer_payload);
        return -1;
    }
    savestate_transfer_payload->compressed_options_size = (int32_t)compressed_options_size;

    ulnet__logical_partition(
        sizeof(savestate_transfer_payload_t) /* Header */ + savestate_transfer_payload->compressed_savestate_size + savestate_transfer_payload->compressed_options_size,
        FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size_bytes, &packet_groups
    );
    assert(savestate_transfer_payload_plus_parity_bound_bytes >= packet_groups * n * packet_payload_size_bytes); // If this fails my logic calculating the bounds was just wrong

    savestate_transfer_payload->frame_counter = save_state_frame;
    savestate_transfer_payload->room = session->room_we_are_in;
    savestate_transfer_payload->total_size_bytes = sizeof(savestate_transfer_payload_t) + savestate_transfer_payload->compressed_savestate_size + savestate_transfer_payload->compressed_options_size;

    savestate_transfer_payload->checksum = 0;
    savestate_transfer_payload->checksum = ulnet_xxh32(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes, 0);
    // Create parity blocks for Reed-Solomon. n - k in total for each packet group
    // We have "packet grouping" because pretty much every implementation of Reed-Solomon doesn't support more than 255 blocks
    // and unfragmented UDP packets over ethernet are limited to ULNET_PACKET_SIZE_BYTES_MAX
    // This makes the code more complicated and the error correcting properties slightly worse but it's a practical tradeoff
    for (int j = 0; j < packet_groups; j++) {
        void *data[255];

        for (int i = 0; i < n; i++) {
            data[i] = (unsigned char *) savestate_transfer_payload + ulnet__logical_partition_offset_bytes(j, i, packet_payload_size_bytes, packet_groups);
        }

        ulnet__rs_encode(data, k, n, packet_payload_size_bytes);
    }

    session->savestate_transfer_id = (uint8_t)((session->savestate_transfer_id + 1) & 0x3);
    session->savestate_transfer_awaiting_bitfield = peer_bitfield;
    int64_t transfer_bandwidth_bits_per_second =
        ULNET_SAVESTATE_TRANSFER_DEFAULT_BANDWIDTH_BITS_PER_SECOND / (1LL << session->savestate_transfer_retry_count);

    SAM2_LOG_INFO("Starting savestate transfer id=%u to peers 0x%016" PRIx64 " at %" PRId64 " bits/s retry=%u",
        (unsigned)session->savestate_transfer_id, peer_bitfield,
        transfer_bandwidth_bits_per_second,
        (unsigned)session->savestate_transfer_retry_count);

    // Send original data blocks and parity blocks
    // @todo I wrote this in such a way that you can do a zero-copy when creating the packets to send
    for (int j = 0; j < packet_groups; j++) {
        for (int i = 0; i < n; i++) {
            ulnet_save_state_packet_fragment2_t packet;
            packet.channel_and_flags = ULNET_CHANNEL_SAVESTATE_TRANSFER | ulnet__savestate_transfer_id_flags(session->savestate_transfer_id);
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

            memcpy(packet.payload, (unsigned char *) savestate_transfer_payload + ulnet__logical_partition_offset_bytes(j, i, packet_payload_size_bytes, packet_groups), packet_payload_size_bytes);

            for (uint64_t p = 0; p < SAM2_TOTAL_PEERS; p++) {
                if (!(peer_bitfield & (1ULL << p))) continue;
                ulnet_udp_send(session, (int)p, (const uint8_t *) &packet, sizeof(ulnet_save_state_packet_header_t) + packet_payload_size_bytes);
            }
        }

        if (j + 1 < packet_groups) {
            int64_t packet_size_bits = (int64_t)(sizeof(ulnet_save_state_packet_header_t) + packet_payload_size_bytes) * 8LL;
            int64_t group_bits = packet_size_bits * n * peer_count;
            int64_t interval_usec = (group_bits * 1000000LL + transfer_bandwidth_bits_per_second - 1)
                / transfer_bandwidth_bits_per_second;
            if (interval_usec > 0) {
                ulnet__sleep((unsigned int)SAM2_MAX(1, interval_usec / 1000));
            }
        }
    }

    session->savestate_transfer_ack_deadline_unix_usec =
        ulnet__get_unix_time_microseconds() + ULNET_SAVESTATE_TRANSFER_ACK_TIMEOUT_MICROSECONDS;
    ULNET_FREE(savestate_transfer_payload);
    return 0;
}

// Pass in save state since often retro_serialize can tick the core
ULNET_LINKAGE void ulnet_send_save_state(ulnet_session_t *session, int port, void *save_state, size_t save_state_size, int64_t save_state_frame) {
    if (port < 0 || port >= SAM2_TOTAL_PEERS) {
        SAM2_LOG_ERROR("Invalid savestate transfer port %d", port);
        return;
    }

    ulnet__send_save_state_to_peers(session, 1ULL << port, save_state, save_state_size, save_state_frame);
}

#if defined(ULNET_IMGUI)
void ulnet_imgui_show_room(const sam2_room_t& room, int our_peer_id = -1) {
    const ImVec4 WHITE(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 GOLD(1.0f, 0.843f, 0.0f, 1.0f);
    ImGui::Text("Room: %s", room.name);
    ImGui::Text("Flags: %08" PRIx32, room.flags);
    ImGui::Text("Core: %s", room.core_and_version);
    ImGui::Text("ROM Hash: %08" PRIx32, room.rom_hash);

    for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
        if (p != SAM2_AUTHORITY_INDEX && room.peer_ids[p] == SAM2_PORT_AVAILABLE) continue;

        const char *role =
            p == SAM2_AUTHORITY_INDEX
            ? (ulnet_port_is_p2p(&room, p) ? "Authority + Player" : "Coordinator")
            : (ulnet_port_is_p2p(&room, p) ? "Player" : "Spectator");
        ImGui::Text("%s %d Peer ID: ", role, p);

        ImGui::SameLine();
        if (room.peer_ids[p] == SAM2_PORT_AVAILABLE) {
            ImGui::Text("Available");
        } else if (room.peer_ids[p] == SAM2_PORT_UNAVAILABLE) {
            ImGui::Text("Unavailable");
        } else {
            if (our_peer_id == room.peer_ids[p]) {
                ImGui::TextColored(GOLD, "%05" PRId16, room.peer_ids[p]);
            } else {
                ImGui::TextColored(WHITE, "%05" PRId16, room.peer_ids[p]);
            }
        }
    }
}

ULNET_LINKAGE void ulnet_imgui_plot_history(ulnet_session_t *session) {
    int our_port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);
    int column_count = 1;
    for (int port = 0; port < SAM2_TOTAL_PEERS; port++) {
        if (session->room_we_are_in.peer_ids[port] > SAM2_PORT_SENTINELS_MAX) {
            column_count++;
        }
    }

    if (ImGui::BeginTable("PacketHistory", column_count,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
                          ImVec2(0.0f, 300.0f))) {

        // Header row
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        for (int port = 0; port < SAM2_TOTAL_PEERS; port++) {
            if (session->room_we_are_in.peer_ids[port] <= SAM2_PORT_SENTINELS_MAX) continue;

            char label[64];
            uint16_t peer_id = session->room_we_are_in.peer_ids[port];
            snprintf(label, sizeof(label), "Port %d\nPeer %05" PRIu16, port, peer_id);
            ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_WidthFixed, 120.0f);
        }

        ImGui::TableHeadersRow();
        for (int i = 0; i < ULNET_STATE_PACKET_HISTORY_SIZE; i++) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", i);

            for (int port = 0; port < SAM2_TOTAL_PEERS; port++) {
                if (session->room_we_are_in.peer_ids[port] <= SAM2_PORT_SENTINELS_MAX) continue;

                ImGui::TableNextColumn();

                ulnet_packet_ref_t packet_ref = session->state_packet_history[port][i];
                ulnet_state_packet_t *state_packet = (ulnet_state_packet_t *) packet_ref.data;

                if (state_packet == NULL) {
                    ImGui::TextDisabled("---");
                } else {
                    int64_t frame;
                    int decode_result = rle8_decode(
                        state_packet->coded_state,
                        packet_ref.size - sizeof(ulnet_state_packet_t),
                        (uint8_t *)&frame,
                        sizeof(frame)
                    );

                    if (decode_result < 0) {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ERROR");
                    } else {
                        int64_t frame_diff = frame - session->frame_counter;

                        // Color code based on freshness
                        if (frame_diff > 0) {
                            // Future frame (shouldn't happen usually)
                            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                                             "%" PRId64 " (+%" PRId64 ")", frame, frame_diff);
                        } else if (frame_diff >= -10) {
                            // Very recent (green)
                            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                                             "%" PRId64 " (%" PRId64 ")", frame, frame_diff);
                        } else if (frame_diff >= -60) {
                            // Recent (yellow)
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                                             "%" PRId64 " (%" PRId64 ")", frame, frame_diff);
                        } else {
                            // Old (gray)
                            ImGui::TextDisabled("%" PRId64 " (%" PRId64 ")", frame, frame_diff);
                        }
                    }
                }
            }
        }

        ImGui::EndTable();
    }
}

ULNET_LINKAGE void ulnet_imgui_show_session(ulnet_session_t *session) {
    ImGui::Text("Frame Counter: %" PRIi64, session->frame_counter);
    if (ImGui::CollapsingHeader("Room we are in")) {
        ulnet_imgui_show_room(session->room_we_are_in, session->our_peer_id);
    }
    // Plot Input Packet Size vs. Frame
    // @todo The gaps in the graph can be explained by out-of-order arrival of packets I think I don't even record those to history but I should
    //       There is some other weird behavior that might be related to not checking the frame field in the packet if its too old it shouldn't be in the plot obviously
    if (session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED && ImGui::CollapsingHeader("Network Activity")) {
        ImPlot::SetNextAxisLimits(ImAxis_X1, session->frame_counter - ULNET_MAX_SAMPLE_SIZE, session->frame_counter, ImGuiCond_Always);
        ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, 512, ImGuiCond_Always);
        if (ImPlot::BeginPlot("State-Packet Size vs. Frame")) {
            ImPlot::SetupAxis(ImAxis_X1, "ulnet_state_t::frame");
            ImPlot::SetupAxis(ImAxis_Y1, "Size Bytes");

            for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
                if (session->room_we_are_in.peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;

                ulnet_packet_ref_t ref = session->state_packet_history[p][session->frame_counter % ULNET_STATE_PACKET_HISTORY_SIZE];
                uint8_t *peer_packet = ref.data;
                session->input_packet_size[p][session->frame_counter % ULNET_MAX_SAMPLE_SIZE] = peer_packet ? ref.size : 0;

                char label[32];
                snprintf(label, sizeof(label),
                    p == SAM2_AUTHORITY_INDEX ? "Authority" : ulnet_port_is_p2p(&session->room_we_are_in, p) ? "Player %d" : "Spectator %d", p);

                int xs[ULNET_MAX_SAMPLE_SIZE], ys[ULNET_MAX_SAMPLE_SIZE];
                for (int j = 0, frame = SAM2_MAX(0, session->frame_counter - ULNET_MAX_SAMPLE_SIZE + 1); j < ULNET_MAX_SAMPLE_SIZE; j++, frame++) {
                    xs[j] = frame;
                    ys[j] = session->input_packet_size[p][frame % ULNET_MAX_SAMPLE_SIZE];
                }
                ImPlot::PlotLine(label, xs, ys, ULNET_MAX_SAMPLE_SIZE);
            }
            ImPlot::EndPlot();
        }
    }

    ImGui::SliderFloat("UDP Induced Receive Drop Rate", &session->debug_udp_recv_drop_rate, 0.0f, 1.0f);
    ImGui::SliderFloat("UDP Induced Transmit Drop Rate", &session->debug_udp_send_drop_rate, 0.0f, 1.0f);

    int active_connections = 0;
    for (int p = 0; p < SAM2_TOTAL_PEERS; p++)
        if (session->agent[p]) active_connections++;
    ImGui::Text("Active connections: %d", active_connections);

    if (ImGui::BeginTabBar("PeerTabs")) {
        for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
            if (!session->agent[p]) continue;

            char tabName[32];
            snprintf(tabName, sizeof(tabName),
                     p == SAM2_AUTHORITY_INDEX ? "Authority %d (%05" PRId16 ")"
                     : ulnet_port_is_p2p(&session->room_we_are_in, p) ? "Player %d (%05" PRId16 ")"
                     : "Spectator %d (%05" PRId16 ")",
                     p, session->room_we_are_in.peer_ids[p]);

            if (ImGui::BeginTabItem(tabName)) {
                if (ImGui::CollapsingHeader("Reliable Protocol State", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Transmit: Next Seq=%u, Head=%u", session->reliable_tx_next_seq[p], session->reliable_tx_head[p]);
                    ImGui::Text("Receive: Next Expected=%u", session->reliable_rx_head[p]);
                }

                if (ImGui::CollapsingHeader("Ping Estimates", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (session->peer_packet_ping_samples[p] > 0) {
                        ImGui::Text("Packet ping (callsite): %.3f ms", session->peer_packet_ping_usec[p] / 1000.0);
                        ImGui::Text("Clock offset (callsite): %.3f ms", session->peer_clock_offset_usec[p] / 1000.0);
                    } else {
                        ImGui::TextDisabled("Packet ping (callsite): waiting for echoed timestamp");
                    }

                    if (session->peer_packet_kernel_ping_samples[p] > 0) {
                        ImGui::Text("Packet ping (SO_TIMESTAMP): %.3f ms", session->peer_packet_kernel_ping_usec[p] / 1000.0);
                        ImGui::Text("Clock offset (SO_TIMESTAMP): %.3f ms", session->peer_kernel_clock_offset_usec[p] / 1000.0);
                    } else {
                        ImGui::TextDisabled("Packet ping (SO_TIMESTAMP): unavailable");
                    }

                    if (session->peer_input_to_core_ping_usec[p] > 0) {
                        ImGui::Text("Input poll to core: %.3f ms", session->peer_input_to_core_ping_usec[p] / 1000.0);
                    } else {
                        ImGui::TextDisabled("Input poll to core: waiting for consumed input");
                    }
                }

                if (ImGui::CollapsingHeader("Recent Packets", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ulnet_imgui_show_recent_packets_table(session, p);
                }

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void ulnet_imgui_show_recent_packets_table(ulnet_session_t *session, int p) {
    const char *headers[] = {"Dir", "Type", "Reliable", "Size", "Details"};
    const float widths[] = {40.0f, 80.0f, 70.0f, 60.0f, 0.0f};
    int columns_count = sizeof(headers) / sizeof(headers[0]);
    int packets_display_count = sizeof(session->packet_history[p]) / sizeof(session->packet_history[p][0]);

    ImGui::Checkbox("Show Recent", &session->imgui_packet_table_show_most_recent_first);

    // Early return if we're not visible
    if (!ImGui::BeginTable("PacketHistory", columns_count, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
        ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 10))) {
        return;
    }

    for (int i = 0; i < columns_count; i++) {
        ImGui::TableSetupColumn(headers[i], i < 4 ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthStretch, widths[i]);
    }
    ImGui::TableHeadersRow();

    for (int i = 0; i < packets_display_count; i++) {
        uint8_t idx;
        if (session->imgui_packet_table_show_most_recent_first) {
            idx = (session->packet_history_next[p] + i) & 0xFF; // Show most recent first
        } else {
            idx = i;
        }

        ulnet_packet_ref_t ref = session->packet_history[p][idx];
        uint8_t *packet_data = ref.data;
        int packet_size = ref.size;
        if (!packet_data || packet_size == 0) {
            continue;
        }

        ImGui::TableNextRow();

        // Direction
        ImGui::TableNextColumn();
        const char *dir = (ref.flags & ULNET_PACKET_FLAG_TX_RELIABLE_RETRANSMIT) ? "re-TX" :
                        (ref.flags & ULNET_PACKET_FLAG_TX) ? "TX" : "RX";
        ImVec4 dirColor = (ref.flags & ULNET_PACKET_FLAG_TX) ?
                        ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImGui::TextColored(dirColor, "%s", dir);

        // Packet type
        ImGui::TableNextColumn();
        bool is_reliable = (packet_data[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_RELIABLE;
        bool is_reliable_ack = is_reliable && (packet_data[0] & ULNET_RELIABLE_FLAG_ACK_ONLY);

        uint8_t *payload_start = packet_data;
        size_t payload_size = packet_size;
        if (is_reliable && packet_size >= (int)sizeof(ulnet_reliable_packet_t)) {
            payload_start = ((ulnet_reliable_packet_t *)packet_data)->payload;
            payload_size = packet_size - sizeof(ulnet_reliable_packet_t);
        }
        uint8_t channel = payload_start[0] & ULNET_CHANNEL_MASK;

        const struct { uint8_t ch; const char *name; ImVec4 color; } channels[] = {
            {ULNET_CHANNEL_SPECTATOR_INPUT, "Spectator", {1.0f, 1.0f, 1.0f, 1.0f}},
            {ULNET_CHANNEL_SAVESTATE_TRANSFER, "Savestate", {1.0f, 0.6f, 0.0f, 1.0f}},
            {ULNET_CHANNEL_ASCII, "ASCII", {1.0f, 1.0f, 0.5f, 1.0f}},
            {ULNET_CHANNEL_RELIABLE, "Error", {1.0f, 1.0f, 1.0f, 1.0f}}
        };

        const char *channelName = "Unknown";
        ImVec4 channelColor = {1.0f, 1.0f, 1.0f, 1.0f};
        if (ulnet__is_input_channel(payload_start[0])) {
            channelName = "Input";
            channelColor = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
        } else {
            for (int j = 0; j < sizeof(channels)/sizeof(channels[0]); j++) {
                if (channel == channels[j].ch) {
                    channelName = channels[j].name;
                    channelColor = channels[j].color;
                    break;
                }
            }
        }
        ImGui::TextColored(channelColor, "%s", channelName);

        // Reliable status
        ImGui::TableNextColumn();
        if (is_reliable && packet_size >= 3) {
            uint16_t seq;
            memcpy(&seq, &packet_data[1], sizeof(seq));
            uint16_t diff = (session->reliable_tx_head[p] - seq) & 0xFFFF;

            const char *status = ulnet__sequence_greater_than(seq, session->reliable_tx_head[p]) ? "Unacked" : "Acked";
            ImVec4 statusColor = strcmp(status, "Acked") == 0 ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) :
                                ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(statusColor, "%s", status);
        } else if (is_reliable_ack) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Meta");
        } else {
            ImGui::TextDisabled("N/A");
        }

        // Size
        ImGui::TableNextColumn();
        ImGui::Text("%d B", packet_size);

        // Details
        ImGui::TableNextColumn();
        char details[256] = {0};
        int pos = 0;

        // Reliable details
        if (is_reliable || is_reliable_ack) {
            if (packet_size >= 3 && !is_reliable_ack) {
                uint16_t seq;
                memcpy(&seq, &packet_data[1], sizeof(seq));
                pos += snprintf(details + pos, sizeof(details) - pos, "Seq=%u", seq);
            }
        }

        // Channel-specific details
        if (pos > 0) pos += snprintf(details + pos, sizeof(details) - pos, " | ");

        if (ulnet__is_input_channel(payload_start[0]) && payload_size > sizeof(ulnet_state_packet_t)) {
            int64_t frame = 0;
            rle8_decode(&payload_start[sizeof(ulnet_state_packet_t)],
                        payload_size - sizeof(ulnet_state_packet_t), (uint8_t *)&frame, sizeof(frame));
            pos += snprintf(details + pos, sizeof(details) - pos, "Port %d Frame %" PRId64,
                payload_start[0] & 0b00111111, frame);
        } else if (channel == ULNET_CHANNEL_SAVESTATE_TRANSFER && payload_size >= sizeof(ulnet_save_state_packet_header_t)) {
            ulnet_save_state_packet_header_t header;
            memcpy(&header, payload_start, sizeof(header));
            if (header.channel_and_flags & ULNET_SAVESTATE_TRANSFER_FLAG_K_IS_239) {
                pos += snprintf(details + pos, sizeof(details) - pos,
                                (header.channel_and_flags & ULNET_SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0) ?
                                "Init: %d groups" : "Group %d, Block %d",
                                (header.channel_and_flags & ULNET_SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0) ?
                                header.packet_groups : header.sequence_hi, header.sequence_lo);
            } else {
                pos += snprintf(details + pos, sizeof(details) - pos, "K=%d, Block %d",
                                header.reed_solomon_k, header.sequence_lo);
            }
        } else if (channel == ULNET_CHANNEL_ASCII) {
            pos += snprintf(details + pos, sizeof(details) - pos, "\"%.*s\"%s",
                            (int)SAM2_MIN(payload_size, SAM2_HEADER_SIZE), (char *)payload_start,
                            payload_size > 8 ? "..." : "");
        }

        ImGui::Text("%s", details);
    }
    ImGui::EndTable();
}

#endif
#endif
#endif
