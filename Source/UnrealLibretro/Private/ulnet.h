#ifndef ULNET_H
#define ULNET_H

#include "sam2.h"

typedef struct juice_agent juice_agent_t;
#include "zstd.h"
#include "common/xxhash.h"
#include "fec.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef ULNET_LINKAGE
#ifdef __cplusplus
#define ULNET_LINKAGE extern "C"
#else
#define ULNET_LINKAGE extern
#endif
#endif

// The payload here is regarding the max payload that we *can* use
// We don't want to exceed the MTU because that can result in guranteed lost packets under certain conditions
// Considering various things like UDP/IP headers, STUN/TURN headers, and additional junk
// load-balancers/routers might add I keep this conservative
#define ULNET_PACKET_SIZE_BYTES_MAX 1408

#define ULNET_SPECTATOR_MAX (SAM2_TOTAL_PEERS - SAM2_PORT_MAX - 1)
#define ULNET_CORE_OPTIONS_MAX 128
#define ULNET_STATE_PACKET_HISTORY_SIZE 64

#define ULNET_HEADER_SIZE                        1
#define ULNET_FLAGS_MASK                         0b00011111
#define ULNET_CHANNEL_MASK                       0b11100000

#define ULNET_CHANNEL_EXTRA                      0b00000000
#define ULNET_CHANNEL_INPUT                      0b00100000
#define ULNET_CHANNEL_ASCII                      0b01000000
#define ULNET_CHANNEL_SPECTATOR_INPUT            0b01100000
#define ULNET_CHANNEL_SAVESTATE_TRANSFER         0b10000000
#define ULNET_CHANNEL_RELIABLE                   0b10100000

#define ULNET_RELIABLE_FLAG_ACK_BITS_ONLY        0b00010000
#define ULNET_RELIABLE_MASK_ACK_BYTES            0b00001111

#define ULNET_PACKET_FLAG_TX                     0x1000
#define ULNET_PACKET_FLAG_TX_RELIABLE_RETRANSMIT 0x2000
#define ULNET_PACKET_FLAG_RX_RELIABLE_OBSERVED   0x2000

#define ulnet_exit_header  "E" "X" "I" "T" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"
#define ULNET_EXIT_HEADER {'E','X','I','T',    '0' + SAM2_VERSION_MAJOR, '.',    '0' + SAM2_VERSION_MINOR, 'r'}

#define ULNET_WAITING_FOR_SAVE_STATE_SENTINEL    INT64_MAX

#define ULNET_SESSION_FLAG_TICKED                0b00000001ULL
#define ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY    0b00000010ULL
#define ULNET_SESSION_FLAG_READY_TO_TICK_SET     0b00000100ULL
#define ULNET_SESSION_FLAG_DRAW_IMGUI            0b00001000ULL

// @todo Remove this define once it becomes possible through normal featureset
#define ULNET__DEBUG_EVERYONE_ON_PORT_0

#define ULNET_MAX_SAMPLE_SIZE 128

#define ULNET_RELIABLE_ACK_BUFFER_SIZE 128
#define ULNET_RELIABLE_RETRANSMIT_INTERVAL_MS 150

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

#define ULNET_PORT_COUNT 8
typedef int16_t ulnet_input_state_t[64]; // This must be a POD for putting into packets

typedef struct arena_ref {
    uint16_t flags_and_generation; // Upper 4 bits available for user flags
    uint16_t size;
    uint32_t offset;
} arena_ref_t;
static const arena_ref_t arena_null = { 0x0, 0x0, 0x0 };

typedef struct arena {
    uint16_t generation; // Wraps around on overflow, only lower 12 bits used
    uint32_t head; // Wraps around when exceeding arena size
    uint8_t arena[2 * 1024 * 1024];
} arena_t;

ULNET_LINKAGE arena_ref_t arena_alloc(arena_t *arena, uint16_t size);
ULNET_LINKAGE void *arena_deref(arena_t *arena, arena_ref_t reference);
ULNET_LINKAGE arena_ref_t arena_reref(arena_ref_t ref, int offset);

typedef struct ulnet_core_option {
    char key[128];
    char value[128];
} ulnet_core_option_t;

// @todo This is really sparse so you should just add routines to read values from it in the serialized format
typedef struct {
    int64_t frame; // Frame for which currently buffered input, room_xor_delta, and core_option should be applied
    ulnet_input_state_t input_state[ULNET_DELAY_BUFFER_SIZE][ULNET_PORT_COUNT];
    sam2_room_t room_xor_delta[ULNET_DELAY_BUFFER_SIZE];
    ulnet_core_option_t core_option[ULNET_DELAY_BUFFER_SIZE]; // Max 1 option per frame provided by the authority

    int64_t save_state_frame; // This is the current frame the peer is on the essentially
    uint32_t save_state_hash[ULNET_DELAY_BUFFER_SIZE];
    uint32_t input_state_hash[ULNET_DELAY_BUFFER_SIZE];
} ulnet_state_t;
SAM2_STATIC_ASSERT(
    sizeof(ulnet_state_t) ==
    (sizeof(((ulnet_state_t *)0)->frame)
    + sizeof(((ulnet_state_t *)0)->input_state)
    + sizeof(((ulnet_state_t *)0)->room_xor_delta)
    + sizeof(((ulnet_state_t *)0)->core_option))
    + sizeof(((ulnet_state_t *)0)->save_state_frame)
    + sizeof(((ulnet_state_t *)0)->save_state_hash)
    + sizeof(((ulnet_state_t *)0)->input_state_hash),
    "ulnet_state_t is not packed"
);

typedef struct {
    uint8_t channel_and_port;
    uint8_t coded_state[];
} ulnet_state_packet_t;

typedef struct {
    uint8_t channel_and_flags;
    //if (channel_and_flags & ULNET_RELIABLE_FLAG_ACK_BITS_ONLY) {
    uint16_t tx_sequence;
    //}

    union {
        // if ((channel_and_flags & ULNET_CHANNEL_MASK) > 0) {
        struct {
            uint16_t rx_sequence;
            //uint8_t ack_bits[/* (channel_and_flags & ULNET_RELIABLE_MASK_ACK_BYTES) */];
            //void *payload[];
        };
        //} else {
        //uint8_t payload[];
        //}
    };
} ulnet_reliable_packet_t;

#define FEC_PACKET_GROUPS_MAX 16
#define FEC_REDUNDANT_BLOCKS 16 // ULNET is hardcoded based on this value so it can't really be changed without breaking the protocol

#define ULNET_SAVESTATE_TRANSFER_FLAG_K_IS_239         0b0001
#define ULNET_SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0 0b0010

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

    uint32_t xxhash;
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

    sam2_room_t room_we_are_in;
    sam2_room_t next_room_xor_delta;

    ulnet_core_option_t core_options[ULNET_CORE_OPTIONS_MAX]; // @todo I don't like this here
    arena_t arena;
    double reliable_next_retransmit_time;

    ulnet_state_t state[SAM2_PORT_MAX+1];

    // MARK: Peer fields
    uint64_t peer_needs_sync_bitfield;
    uint64_t peer_pending_disconnect_bitfield;
    int use_inproc_transport; // "Tag" for the following union
    union {
        juice_agent_t *agent[SAM2_TOTAL_PEERS];
        ulnet_transport_inproc_t *inproc[SAM2_TOTAL_PEERS];
    };
    uint16_t       agent_peer_ids[SAM2_TOTAL_PEERS];
    int64_t peer_desynced_frame[SAM2_TOTAL_PEERS];
    ulnet_input_state_t spectator_suggested_input_state[SAM2_TOTAL_PEERS][ULNET_PORT_COUNT];
    arena_ref_t state_packet_history[SAM2_TOTAL_PEERS][ULNET_STATE_PACKET_HISTORY_SIZE]; // Indexable by (frame / ULNET_DELAY_BUFFER_SIZE) % ULNET_STATE_PACKET_HISTORY_SIZE
    arena_ref_t packet_history[SAM2_TOTAL_PEERS][256]; // All packets circular buffer in order they were sent/recv
    uint8_t packet_history_next[SAM2_TOTAL_PEERS];
    arena_ref_t reliable_tx_packet_history[SAM2_TOTAL_PEERS][ULNET_RELIABLE_ACK_BUFFER_SIZE]; // Indexable by sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE
    arena_ref_t reliable_rx_packet_history[SAM2_TOTAL_PEERS][ULNET_RELIABLE_ACK_BUFFER_SIZE]; // Indexable by sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE
    uint16_t reliable_tx_next_seq[SAM2_TOTAL_PEERS]; // Greatest sequence we have sent
    uint16_t reliable_tx_greatest[SAM2_TOTAL_PEERS]; // Greatest sequence a peer has seen from us
    uint16_t reliable_rx_greatest[SAM2_TOTAL_PEERS]; // Greatest sequence we have seen from a peer
    uint64_t reliable_tx_missing [SAM2_TOTAL_PEERS]; // Bitfield with 1's representing *known* missing packets, offset from `reliable_tx_greatest`, the 0th bit is always 0 as that is the greatest they have seen
    uint64_t reliable_rx_missing [SAM2_TOTAL_PEERS]; // Bitfield with 1's representing *known* missing packets, offset from `reliable_rx_greatest`, the 0th bit is always 0 as that is the greatest we have seen

    // MARK: Save state transfer
    int zstd_compress_level;
    int64_t remote_savestate_transfer_offset;
    uint8_t remote_packet_groups; // This is used to bookkeep how much data we actually need to receive to reform the complete savestate
    arena_ref_t packet_reference[FEC_PACKET_GROUPS_MAX][GF_SIZE - FEC_REDUNDANT_BLOCKS];
    int fec_index[FEC_PACKET_GROUPS_MAX][GF_SIZE - FEC_REDUNDANT_BLOCKS];
    int fec_index_counter[FEC_PACKET_GROUPS_MAX]; // Counts packets received in each "packet group"

    void *user_ptr;
    int (*sam2_send_callback)(void *user_ptr, char *response);
    int (*populate_core_options_callback)(void *user_ptr, ulnet_core_option_t options[ULNET_CORE_OPTIONS_MAX]);

    void (*retro_run)(void *user_ptr);
    size_t (*retro_serialize_size)(void *user_ptr);
    bool (*retro_serialize)(void *user_ptr, void *, size_t);
    bool (*retro_unserialize)(void *user_ptr, const void *data, size_t size);

    float debug_udp_recv_drop_rate;
    float debug_udp_send_drop_rate;

    bool imgui_packet_table_show_most_recent_first;
    int input_packet_size[SAM2_PORT_MAX + 1][ULNET_MAX_SAMPLE_SIZE];
    int save_state_execution_time_cycles[ULNET_MAX_SAMPLE_SIZE];
} ulnet_session_t;

#if __cplusplus >= 201103L
#include <type_traits>
static_assert(std::is_trivially_default_constructible<ulnet_session_t>::value && std::is_standard_layout<ulnet_session_t>::value,
    "ulnet_session_t must be a POD type for safe memory operations");
#endif

ULNET_LINKAGE int ulnet_process_message(ulnet_session_t *session, const char *response);
ULNET_LINKAGE void ulnet_send_save_state(ulnet_session_t *session, int port, void *save_state, size_t save_state_size, int64_t save_state_frame);
ULNET_LINKAGE void ulnet_startup_ice_for_peer(ulnet_session_t *session, uint64_t peer_id, int p, const char *remote_description);
ULNET_LINKAGE void ulnet_disconnect_peer(ulnet_session_t *session, int peer_port);
ULNET_LINKAGE void ulnet_swap_agent(ulnet_session_t *session, int peer_existing_port, int peer_new_port);
ULNET_LINKAGE void ulnet_session_init_defaulted(ulnet_session_t *session);
ULNET_LINKAGE void ulnet_receive_packet_callback(juice_agent_t *agent, const char *packet, size_t size, void *user_ptr);
ULNET_LINKAGE int ulnet_udp_send(ulnet_session_t *session, int port, const uint8_t *packet, size_t size);
ULNET_LINKAGE int ulnet_reliable_send_with_acks_only(ulnet_session_t *session, int port, const uint8_t *packet, int size);
ULNET_LINKAGE int ulnet_reliable_send(ulnet_session_t *session, int port, const uint8_t *packet, int size);
ULNET_LINKAGE int ulnet_poll_session(ulnet_session_t *session, bool force_save_state_on_tick, uint8_t *save_state, size_t save_state_capacity,
    double frame_rate, double max_sleeping_allowed_when_polling_network_seconds);
ULNET_LINKAGE int ulnet_wrapped_header_size(uint8_t *packet, int size);
ULNET_LINKAGE void ulnet_session_tear_down(ulnet_session_t *session);
ULNET_LINKAGE int64_t ulnet__get_unix_time_microseconds();
ULNET_LINKAGE uint32_t ulnet_xxh32(const void* data, size_t len, uint32_t seed);

ULNET_LINKAGE void ulnet_imgui_show_session(ulnet_session_t *session);
ULNET_LINKAGE void ulnet_imgui_show_recent_packets_table(ulnet_session_t *session, int p);
ULNET_LINKAGE int ulnet_test_ice(ulnet_session_t **session_1_out, ulnet_session_t **session_2_out);
ULNET_LINKAGE int ulnet_test_inproc(ulnet_session_t **session_1_out, ulnet_session_t **session_2_out);

static bool ulnet_is_authority(ulnet_session_t *session) {
    return    session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]
           || session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] == 0; // @todo I don't think this extra check should be necessary
}

static bool ulnet_is_spectator(ulnet_session_t *session, uint64_t peer_id) {
    int port = sam2_get_port_of_peer(&session->room_we_are_in, peer_id);

    return    session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED
           && (port >= SAM2_SPECTATOR_START || port == -1);
}

static inline void ulnet__xor_delta(void *dest, void *src, int size) {
    for (int i = 0; i < size; i++) {
        ((uint8_t *) dest)[i] ^= ((uint8_t *) src)[i];
    }
}
#endif

#if defined(ULNET_IMPLEMENTATION)
#ifndef ULNET_C
#define ULNET_C
#if defined(ULNET_IMGUI)
#include "imgui.h"
#include "implot.h"

#define _IMH_CAT(a,b) a##b
#define IMH(statement) if (session->flags & ULNET_SESSION_FLAG_DRAW_IMGUI) { statement }
#else
#define IMH(statement) do {} while (0);
#endif

#include "juice/juice.h"
#include <assert.h>
#include <time.h>

#if defined(__GNUC__) || defined(__clang__)
#define ULNET__CTZ(x) ((x) ? __builtin_ctzll(x) : 64)
#elif defined(_MSC_VER)
static inline int ULNET__CTZ(uint64_t x) {
    unsigned long index;
    if (_BitScanForward64(&index, x))
        return (int)index;
    return 64;
}
#else
static inline int ULNET__CTZ(uint64_t x) {
    if (x == 0) return 64;
    int n = 0;
    if ((x & 0x00000000FFFFFFFF) == 0) { n += 32; x >>= 32; }
    if ((x & 0x000000000000FFFF) == 0) { n += 16; x >>= 16; }
    if ((x & 0x00000000000000FF) == 0) { n += 8; x >>= 8; }
    if ((x & 0x000000000000000F) == 0) { n += 4; x >>= 4; }
    if ((x & 0x0000000000000003) == 0) { n += 2; x >>= 2; }
    if ((x & 0x0000000000000001) == 0) { n += 1; }
    return n;
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ULNET__CLZ(x) ((x) ? __builtin_clzll(x) : 64)
#elif defined(_MSC_VER)
static inline int ULNET__CLZ(uint64_t x) {
    unsigned long index;
    if (_BitScanReverse64(&index, x))
        return 63 - (int)index;
    return 64;
}
#else
static inline int ULNET__CLZ(uint64_t x) {
    if (x == 0) return 64;
    int n = 0;
    if ((x & 0xFFFFFFFF00000000) == 0) { n += 32; x <<= 32; }
    if ((x & 0xFFFF000000000000) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF00000000000000) == 0) { n += 8; x <<= 8; }
    if ((x & 0xF000000000000000) == 0) { n += 4; x <<= 4; }
    if ((x & 0xC000000000000000) == 0) { n += 2; x <<= 2; }
    if ((x & 0x8000000000000000) == 0) { n += 1; }
    return n;
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ULNET__POPCNT(x) __builtin_popcountll(x)
#elif defined(_MSC_VER) && defined(_M_X64)
static inline int ULNET__POPCNT(uint64_t x) {
    return (int)__popcnt64(x);
}
#else
static inline int ULNET__POPCNT(uint64_t x) {
    // Hamming weight algorithm (bit count)
    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (int)((x * 0x0101010101010101ULL) >> 56);
}
#endif

#define XXH_PRIME32_1 2654435761u
#define XXH_PRIME32_2 2246822519u
#define XXH_PRIME32_3 3266489917u
#define XXH_PRIME32_4  668265263u
#define XXH_PRIME32_5  374761393u

static inline uint32_t read_unaligned_u32(const void* p) {
    uint32_t val;
    memcpy(&val, p, sizeof(val));
    return val;
}

static inline uint32_t xxh32_rotl(uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}

ULNET_LINKAGE uint32_t ulnet_xxh32(const void* data, size_t len, uint32_t seed) {
    const uint8_t* p = (const uint8_t*)data;
    const uint8_t* end = p + len;
    uint32_t h32;

    if (len >= 16) {
        const uint8_t* limit = end - 16;
        uint32_t v1 = seed + XXH_PRIME32_1 + XXH_PRIME32_2;
        uint32_t v2 = seed + XXH_PRIME32_2;
        uint32_t v3 = seed + 0;
        uint32_t v4 = seed - XXH_PRIME32_1;

        do {
            v1 = xxh32_rotl(v1 + read_unaligned_u32(p)      * XXH_PRIME32_2, 13) * XXH_PRIME32_1;
            v2 = xxh32_rotl(v2 + read_unaligned_u32(p + 4)  * XXH_PRIME32_2, 13) * XXH_PRIME32_1;
            v3 = xxh32_rotl(v3 + read_unaligned_u32(p + 8)  * XXH_PRIME32_2, 13) * XXH_PRIME32_1;
            v4 = xxh32_rotl(v4 + read_unaligned_u32(p + 12) * XXH_PRIME32_2, 13) * XXH_PRIME32_1;
            p += 16;
        } while (p <= limit);

        h32 = xxh32_rotl(v1, 1) + xxh32_rotl(v2, 7) + xxh32_rotl(v3, 12) + xxh32_rotl(v4, 18);
    } else {
        h32 = seed + XXH_PRIME32_5;
    }

    h32 += (uint32_t)len;

    while (p + 4 <= end) {
        h32 += read_unaligned_u32(p) * XXH_PRIME32_3;
        h32 = xxh32_rotl(h32, 17) * XXH_PRIME32_4;
        p += 4;
    }

    while (p < end) {
        h32 += (*p) * XXH_PRIME32_5;
        h32 = xxh32_rotl(h32, 11) * XXH_PRIME32_1;
        p++;
    }

    h32 ^= h32 >> 15;
    h32 *= XXH_PRIME32_2;
    h32 ^= h32 >> 13;
    h32 *= XXH_PRIME32_3;
    h32 ^= h32 >> 16;

    return h32;
}

ULNET_LINKAGE arena_ref_t arena_alloc(arena_t *arena, uint16_t size) {
    // Reject zero-sized and too-large allocations
    if (size - 1  >= sizeof(arena->arena)) {
        return arena_null;
    }

    // Check if allocation would exceed arena remaining space
    if (arena->head + size > sizeof(arena->arena)) {
        // Wrap around to beginning if we don't have space
        arena->head = 0;
        // Increment generation on wrap, preserving only lower 12 bits
        arena->generation = (arena->generation + 1) & 0x0FFF;
    }

    arena_ref_t ref = {
        arena->generation,
        size,
        arena->head
    };

    arena->head += size;

    return ref;
}

ULNET_LINKAGE void *arena_deref(arena_t *arena, arena_ref_t reference) {
    if (memcmp(&reference, &arena_null, sizeof(reference)) == 0) {
        return NULL;
    }

    // Validate bounds
    if (   reference.offset >= sizeof(arena->arena)
        || reference.offset + reference.size > sizeof(arena->arena)) {
        return NULL;
    }

    void *ptr = &arena->arena[reference.offset];

    // Extract just the generation part, ignoring user flags
    uint16_t ref_gen = reference.flags_and_generation & 0x0FFF;

    // Case 1: Same generation - definitely valid
    if (arena->generation == ref_gen) {
        return ptr;
    }

    // Case 2: Arena has wrapped around once (generation + 1)
    // Handle generation wraparound properly using modular arithmetic
    if (   ((arena->generation - ref_gen) & 0x0FFF) == 1
        && reference.offset >= arena->head) {
        // Return the original pointer - the data is still valid
        // This condition checks if the reference is in memory that hasn't been
        // overwritten yet after a wraparound
        return ptr;
    }

    // In all other cases, the memory has been overwritten
    return NULL;
}

arena_ref_t arena_reref(arena_ref_t ref, int offset) {
    if (offset > ref.size) {
        return arena_null;
    } else {
        arena_ref_t new_ref = ref;
        new_ref.offset += offset;
        new_ref.size -= offset;
        return new_ref;
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
#elif defined(__x86_64__) || defined(__i386__) /* === x86/x86_64 ====== */
#if defined(_MSC_VER)                          /* --- MSVC compiler --- */
    int cpuInfo[4];
    __cpuid(cpuInfo, 0);          // Retire previous instructions
    return __rdtsc();
#elif defined(__GNUC__)                        /* --- GCC compiler ---- */
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
        return (uint16_t)(s1 - s2) <= 32768 ? 1 : -1; // Defined overflow, the cast is necessary because of implicit type conversion
    }
}
static inline int ulnet__sequence_greater_than(uint16_t s1, uint16_t s2) { return ulnet__sequence_cmp(s1, s2) > 0; }
static inline int ulnet__sequence_less_than(uint16_t s1, uint16_t s2)    { return ulnet__sequence_cmp(s1, s2) < 0; }

static void ulnet__logical_partition(int sz, int redundant, int *n, int *out_k, int *packet_size, int *packet_groups) {
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
static int64_t ulnet__logical_partition_offset_bytes(uint8_t sequence_hi, uint8_t sequence_lo, int block_size_bytes, int block_stride) {
    return (int64_t) sequence_hi * block_size_bytes + sequence_lo * block_size_bytes * block_stride;
}

ULNET_LINKAGE void ulnet_input_poll(ulnet_session_t *session, ulnet_input_state_t (*input_state)[ULNET_PORT_COUNT]) {
    for (int peer_idx = 0; peer_idx < SAM2_PORT_MAX+1; peer_idx++) {
        if (   session->room_we_are_in.peer_ids[peer_idx] > SAM2_PORT_SENTINELS_MAX
            && !(session->room_we_are_in.flags & (SAM2_FLAG_PORT0_PEER_IS_INACTIVE << peer_idx))) {

            if (!(session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)) {
                assert(peer_idx == SAM2_AUTHORITY_INDEX);
            }

            assert(session->state[peer_idx].frame <= session->frame_counter + (ULNET_DELAY_BUFFER_SIZE-1));
            assert(session->state[peer_idx].frame >= session->frame_counter);
            for (int i = 0; i < SAM2_ARRAY_LENGTH((*input_state)[0]); i++) {
                #if defined(ULNET__DEBUG_EVERYONE_ON_PORT_0)
                int port = 0;
                #else
                int port = peer_idx;
                #endif
                (*input_state)[port][i] |= session->state[peer_idx].input_state[session->frame_counter % ULNET_DELAY_BUFFER_SIZE][port][i];
            }
        }
    }
}

// @todo Weird interface
ULNET_LINKAGE ulnet_input_state_t (*ulnet_query_generate_next_input(ulnet_session_t *session, ulnet_core_option_t *next_frame_option))[ULNET_PORT_COUNT] {
    // Poll input with buffering for netplay
    int our_port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);
    if (our_port == -1) {
        SAM2_LOG_WARN("No port associated for our peer_id=%d, skipping input polling", session->our_peer_id);
        return NULL;
    } else if (our_port < SAM2_SPECTATOR_START) {
        if (session->state[our_port].frame >= session->frame_counter + session->delay_frames) {
            return NULL; // We already buffered enough input
        }

        // @todo The preincrement does not make sense to me here, but things have been working
        int64_t next_buffer_index = ++session->state[our_port].frame % ULNET_DELAY_BUFFER_SIZE;

        session->state[our_port].core_option[next_buffer_index] = *next_frame_option;
        memset(next_frame_option, 0, sizeof(*next_frame_option));

        //if (ulnet_is_authority(session)) {
            session->state[our_port].room_xor_delta[next_buffer_index] = session->next_room_xor_delta;
            memset(&session->next_room_xor_delta, 0, sizeof(session->next_room_xor_delta));
        //}

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

        return &session->state[our_port].input_state[next_buffer_index];
    } else {
        memset(session->spectator_suggested_input_state[63], 0, sizeof(session->spectator_suggested_input_state[63]));
        return &session->spectator_suggested_input_state[63];
    }
}

double core_wants_tick_in_seconds(int64_t core_wants_tick_at_unix_usec) {
    double seconds = (core_wants_tick_at_unix_usec - ulnet__get_unix_time_microseconds()) / 1000000.0;
    return seconds;
}

static void ulnet_update_state_history(ulnet_session_t *session, arena_ref_t packet_ref) {
    // Only store every 8th packet... frame 7, 15, 23, etc.
    uint8_t *packet = (uint8_t *)arena_deref(&session->arena, packet_ref);
    if ((packet[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_INPUT) {
        SAM2_LOG_ERROR("Attempt to store non-input packet in state history");
    }

    int port = packet[0] & ULNET_FLAGS_MASK;
    int64_t frame;
    rle8_decode(&packet[sizeof(ulnet_state_packet_t)], packet_ref.size - sizeof(ulnet_state_packet_t), (uint8_t *) &frame, sizeof(frame));
    if ((frame + 1) % ULNET_DELAY_BUFFER_SIZE == 0) {
        int history_idx = (frame / ULNET_DELAY_BUFFER_SIZE) % ULNET_STATE_PACKET_HISTORY_SIZE;

        session->state_packet_history[port][history_idx] = packet_ref;
    }
}

ULNET_LINKAGE int ulnet_wrapped_header_size(uint8_t *packet, int size) {
    if (size > 0 && (packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_RELIABLE) {
        int reliable_header_size = ULNET_HEADER_SIZE;
        if (!(packet[0] & ULNET_RELIABLE_FLAG_ACK_BITS_ONLY)) {
            reliable_header_size += sizeof(((ulnet_reliable_packet_t*)0x0)->tx_sequence);
        }

        uint8_t nbytes = packet[0] & ULNET_RELIABLE_MASK_ACK_BYTES;
        if (nbytes > 0) {
            reliable_header_size += sizeof(((ulnet_reliable_packet_t*)0x0)->rx_sequence) + nbytes;
        }

        return reliable_header_size;
    } else {
        return 0;
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
        SAM2_LOG_ERROR("Attempt to send packet with invalid channel: %d", packet[0] & ULNET_CHANNEL_MASK);
        return -1;
    }
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

    arena_ref_t packet_ref = arena_alloc(&session->arena, (uint16_t)size);
    packet_ref.flags_and_generation |= ULNET_PACKET_FLAG_TX;
    memcpy(arena_deref(&session->arena, packet_ref), packet, size);
    session->packet_history[port][session->packet_history_next[port]++] = packet_ref;

    if ((packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_RELIABLE) {
        // Add to reliable packet history
        uint16_t sequence = ((uint16_t)packet[2] << 8) | packet[1];
        session->reliable_tx_packet_history[port][sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE] = packet_ref;
    }

    int offset_to_payload = ulnet_wrapped_header_size((uint8_t *)packet, size);
    if ((packet[offset_to_payload] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_INPUT) {
        ulnet_update_state_history(session, arena_reref(packet_ref, offset_to_payload));
    }

    if (rand() / ((float) RAND_MAX) < session->debug_udp_send_drop_rate) {
        SAM2_LOG_ERROR("Intentionally dropped a sent UDP packet");
        return 0;
    }

    if (session->use_inproc_transport) {
        ulnet_inproc_buf_t *buf;
        if (session->our_peer_id < session->agent_peer_ids[port]) {
            buf = &session->inproc[port]->buf1;
        } else {
            buf = &session->inproc[port]->buf2;
        }

        if (buf->count >= sizeof(buf->msg) / sizeof(buf->msg[0])) {
            SAM2_LOG_FATAL("Inproc transport buffer is full, cannot send packet");
        }

        buf->msg_size[buf->count] = size;
        memcpy(buf->msg[buf->count], packet, size);
        buf->count++;
        return 0;
    } else {
        return juice_send(session->agent[port], (const char *)packet, size);
    }
}

static int ulnet__wrap_packet(const uint8_t packet[/* size */], int size, uint16_t sequence,
    uint64_t rx_missing, uint16_t rx_greatest, uint8_t wrapped_packet[/* ULNET_PACKET_SIZE_BYTES_MAX */]) {
    if ((wrapped_packet[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_RELIABLE) {
        SAM2_LOG_ERROR("Expected filled out first header byte");
        return -1;
    }

    int offset = ULNET_HEADER_SIZE;

    if (!(wrapped_packet[0] & ULNET_RELIABLE_FLAG_ACK_BITS_ONLY)) {
        memcpy(&wrapped_packet[offset], &sequence, sizeof(sequence));
        offset += sizeof(sequence);
    } else {
        wrapped_packet[0] |= ULNET_RELIABLE_FLAG_ACK_BITS_ONLY;
    }

    uint8_t nbytes = 0;
    // Count how many ack bytes we need to send if any
    for (uint64_t t = rx_missing; t && nbytes < 8;) { ++nbytes; t >>= 8; }
    wrapped_packet[0] |= nbytes;

    if (nbytes > 0) {
        memcpy(&wrapped_packet[offset], &rx_greatest, sizeof(rx_greatest));
        offset += sizeof(rx_greatest);
        memcpy(&wrapped_packet[offset], &rx_missing, nbytes);
        offset += nbytes;
    }

    if (ULNET_PACKET_SIZE_BYTES_MAX < offset + size) {
        SAM2_LOG_ERROR("Reliable packet too large: %d bytes", size);
        return -1;
    }

    memcpy(&wrapped_packet[offset], packet, size);

    return offset + size;
}

ULNET_LINKAGE int ulnet_reliable_send_with_acks_only(ulnet_session_t *session, int port, const uint8_t *packet, int size) {
    uint8_t tmp[ULNET_PACKET_SIZE_BYTES_MAX] = { ULNET_CHANNEL_RELIABLE | ULNET_RELIABLE_FLAG_ACK_BITS_ONLY };

    int maybe_wrapped_size = ulnet__wrap_packet(packet, size, 0 /* ignored */, session->reliable_rx_missing[port], session->reliable_rx_greatest[port], tmp);
    if (maybe_wrapped_size < 0) {
        return -1;
    } else {
        return ulnet_udp_send(session, port, tmp, maybe_wrapped_size);
    }
}

// Sends message reliably by wrapping the packet in a reliable header then sending it
ULNET_LINKAGE int ulnet_reliable_send(ulnet_session_t *session, int port, const uint8_t *packet, int size) {
    uint8_t tmp[ULNET_PACKET_SIZE_BYTES_MAX] = { ULNET_CHANNEL_RELIABLE };
    int offset = ULNET_HEADER_SIZE;

    uint16_t sequence = ++session->reliable_tx_next_seq[port];

    int maybe_wrapped_size = ulnet__wrap_packet(packet, size, sequence, session->reliable_rx_missing[port], session->reliable_rx_greatest[port], tmp);
    if (maybe_wrapped_size < 0) {
        return -1;
    } else {
        return ulnet_udp_send(session, port, tmp, maybe_wrapped_size);
    }
}

static sam2_message_metadata_t ulnet__message_metadata[] = {
    {ulnet_exit_header, SAM2_HEADER_SIZE},
};

void ulnet_message_send(ulnet_session_t *session, int port, const uint8_t *message) {
    if ((message[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_ASCII) {
        SAM2_LOG_FATAL("Attempt to send non-ASCII message with ulnet_message_send");
    }

    sam2_message_metadata_t *metadata = sam2_get_metadata((const char *) message);

    for (int i = 0; i < SAM2_ARRAY_LENGTH(ulnet__message_metadata); i++) {
        if (sam2_header_matches((char *)message, ulnet__message_metadata[i].header)) {
            metadata = &ulnet__message_metadata[i];
        }
    }

    ulnet_reliable_send(session, port, message, metadata->message_size);
}

static SAM2_FORCEINLINE int64_t ulnet__get_frame_from_packet(const uint8_t *packet) {
    int64_t frame;
    rle8_decode(&packet[sizeof(ulnet_state_packet_t)], sizeof(frame), (uint8_t *) &frame, sizeof(frame));
    return frame;
}

int ulnet_send(ulnet_session_t *session, int port, const uint8_t *packet, size_t size) {
    if ((packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_INPUT) {
        int packet_port = packet[0] & ULNET_FLAGS_MASK;
        int64_t packet_frame = ulnet__get_frame_from_packet(packet);
        // Only send every 8th packet reliably... frame 7, 15, 23, etc.
        if ((packet_frame + 1) % ULNET_DELAY_BUFFER_SIZE == 0) {
            void *sent_packet = arena_deref(&session->arena, session->state_packet_history[packet_port][(packet_frame / ULNET_DELAY_BUFFER_SIZE) % ULNET_STATE_PACKET_HISTORY_SIZE]);

            if (sent_packet == NULL || packet_frame != ulnet__get_frame_from_packet((uint8_t *) sent_packet)) {
                return ulnet_reliable_send(session, port, packet, size);
            }
        } else {
            return ulnet_udp_send(session, port, packet, size);
        }
    } else if ((packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_SPECTATOR_INPUT) {
        return ulnet_reliable_send_with_acks_only(session, port, packet, size);
    } else {
        return ulnet_udp_send(session, port, packet, size);
    }

    return 0;
}

static void ulnet__check_retransmissions(ulnet_session_t *session, double current_time_seconds) {
    // Skip if not time to retransmit yet
    if (current_time_seconds < session->reliable_next_retransmit_time) {
        return;
    } else {
        // Update next retransmit time
        session->reliable_next_retransmit_time = current_time_seconds + ULNET_RELIABLE_RETRANSMIT_INTERVAL_MS/1000.0;
    }

    for (int port = 0; port < SAM2_TOTAL_PEERS; port++) {
        uint64_t reliable_unacked_bitfield = session->reliable_tx_missing[port];
        while (reliable_unacked_bitfield) {
            // Find the position of the next set bit
            int bit_pos = ULNET__CTZ(reliable_unacked_bitfield);

            // Clear this bit so we don't process it again in this loop
            uint64_t bit_mask = (1ULL << bit_pos);
            reliable_unacked_bitfield &= ~bit_mask;

            // Calculate the sequence number for this bit position
            // The bit at position 0 represents sequence (greatest_sequence)
            // The bit at position 1 represents sequence (greatest_sequence - 1), etc.
            uint16_t sequence = (session->reliable_tx_greatest[port] - bit_pos) & 0xFFFF;

            arena_ref_t packet_ref = session->reliable_tx_packet_history[port][sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE];
            uint8_t *packet = (uint8_t *) arena_deref(&session->arena, packet_ref);
            int packet_size = packet_ref.size;

            if (packet) {
                SAM2_LOG_INFO("Retransmitting packet with sequence %d", sequence);

                // Recreate the packet with updated ack bits
                uint8_t wrapped_packet[ULNET_PACKET_SIZE_BYTES_MAX] = { ULNET_CHANNEL_RELIABLE };
                uint16_t sequence = ((uint16_t)packet[2] << 8) | packet[1];
                int old_header_size = ulnet_wrapped_header_size(packet, packet_size);
                int wrapped_size = ulnet__wrap_packet(&packet[old_header_size], packet_size - old_header_size, sequence,
                    session->reliable_rx_missing[port], session->reliable_rx_greatest[port], wrapped_packet);

                if (ulnet_udp_send(session, port, wrapped_packet, wrapped_size)) {
                    SAM2_LOG_ERROR("Failed to retransmit packet with sequence %d", sequence);
                } else {
                    session->packet_history[port][session->packet_history_next[port] - 1].flags_and_generation |= ULNET_PACKET_FLAG_TX_RELIABLE_RETRANSMIT;
                }
            } else {
                SAM2_LOG_WARN("Unable to resend packet arena reference overwritten");
            }
        }
    }
}

#define ULNET_POLL_SESSION_SAVED_STATE 0b00000001
#define ULNET_POLL_SESSION_TICKED      0b00000010
// This procedure always sends an input packet if the core is ready to tick. This subsumes retransmission logic and generally makes protocol logic less strict
ULNET_LINKAGE int ulnet_poll_session(ulnet_session_t *session, bool force_save_state_on_tick, uint8_t *save_state, size_t save_state_capacity,
    double frame_rate, double max_sleeping_allowed_when_polling_network_seconds) {

    int our_port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);

    IMH(ImGui::Begin("P2P UDP Netplay", NULL, ImGuiWindowFlags_AlwaysAutoResize);)
    int status = 0;

    if (session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED
        && our_port != -1) {
        uint8_t packet[ULNET_PACKET_SIZE_BYTES_MAX];
        int64_t packet_size;

        if (our_port >= SAM2_SPECTATOR_START) {
            packet[0] = ULNET_CHANNEL_SPECTATOR_INPUT;
            packet_size = sizeof(ulnet_state_packet_t) + rle8_encode_capped(
                (uint8_t *) &session->spectator_suggested_input_state[63],
                sizeof(session->spectator_suggested_input_state[63]),
                &packet[sizeof(ulnet_state_packet_t)],
                sizeof(packet) - sizeof(ulnet_state_packet_t)
            );
        } else {
            packet[0] = ULNET_CHANNEL_INPUT | our_port;
            packet_size = sizeof(ulnet_state_packet_t) + rle8_encode_capped(
                (uint8_t *) &session->state[our_port],
                sizeof(session->state[0]),
                &packet[sizeof(ulnet_state_packet_t)],
                sizeof(packet) - sizeof(ulnet_state_packet_t)
            );
        }

        if (packet_size > ULNET_PACKET_SIZE_BYTES_MAX) {
            SAM2_LOG_FATAL("Input packet too large to send");
        }

        for (int p = 0; p < SAM2_ARRAY_LENGTH(session->agent); p++) {
            if (!session->agent[p]) continue;
            juice_state_t state = juice_get_state(session->agent[p]);

            // Wait until we can send netplay messages to everyone without fail
            if (state == JUICE_STATE_CONNECTED || state == JUICE_STATE_COMPLETED) {
                ulnet_send(session, p, packet, packet_size);

                if (our_port < SAM2_SPECTATOR_START) {
                    SAM2_LOG_DEBUG("Sent input packet for frame %" PRId64 " dest peer_ids[%d]=%05" PRId16,
                        session->state[our_port].frame, p, session->room_we_are_in.peer_ids[p]);
                } else {
                    SAM2_LOG_DEBUG("Sent spectator input packet dest peer_ids[%d]=%05" PRId16, p, session->room_we_are_in.peer_ids[p]);
                }
            }
        }
    }

    // Get rid of dead agents
    juice_agent_t *agent[SAM2_ARRAY_LENGTH(session->agent)] = {0};
    int agent_count = 0;
    for (int p = 0; p < SAM2_ARRAY_LENGTH(session->agent); p++) {
        if (session->agent[p]) {
            if (   juice_get_state(session->agent[p]) == JUICE_STATE_FAILED
                || session->peer_pending_disconnect_bitfield & (1ULL << p)) {
                if (p >= SAM2_PORT_MAX+1) {
                    SAM2_LOG_INFO("Spectator %05" PRId16 " left" , session->room_we_are_in.peer_ids[p]);
                } else {
                    SAM2_LOG_ERROR("Peer %05" PRId16 " disconnected before leaving the room this should force a resync which I don't do right now @todo" , session->room_we_are_in.peer_ids[p]);
                }

                ulnet_disconnect_peer(session, p);
            } else {
                agent[agent_count++] = session->agent[p];
            }
        }
    }

    // Update reliable endpoints
    double current_time_seconds = ulnet__get_unix_time_microseconds() / 1e6;

    ulnet__check_retransmissions(session, current_time_seconds);

    // @todo This timing code is messy I should formally model the problem and then create a solution based on that
    bool ignore_frame_pacing_so_we_can_catch_up = false;
    int64_t poll_entry_time_usec = ulnet__get_unix_time_microseconds();

    if (session->use_inproc_transport) {
        for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
            if (!session->inproc[p]) continue;

            ulnet_inproc_buf_t *buf;
            if (session->our_peer_id < session->agent_peer_ids[p]) {
                buf = &session->inproc[p]->buf2;
            } else {
                buf = &session->inproc[p]->buf1;
            }

            for (int i = 0; i < buf->count; i++) {
                ulnet_receive_packet_callback((juice_agent_t *)session->inproc[p], (char*)buf->msg[i], buf->msg_size[i], session);
            }
            buf->count = 0;  // Mark all messages as delivered
        }
    } else {
        int debug_loop_count = 0;
        do {
            if (ulnet_is_spectator(session, session->our_peer_id)) {
                int64_t authority_frame = -1;

                // The number of packets we check here is reasonable, since if we miss ULNET_DELAY_BUFFER_SIZE consecutive packets our connection is irrecoverable anyway
                for (int i = 0; i < ULNET_DELAY_BUFFER_SIZE; i++) {
                    int64_t frame = -1;
                    arena_ref_t state_packet_ref = session->state_packet_history[SAM2_AUTHORITY_INDEX][(session->frame_counter + i) % ULNET_STATE_PACKET_HISTORY_SIZE];
                    uint8_t *state_packet = (uint8_t *) arena_deref(&session->arena, state_packet_ref);

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
                int ret = juice_user_poll(agent, agent_count, (int) timeout_milliseconds);
                // This will call ulnet_receive_packet_callback in a loop
                if (ret < 0) {
                    SAM2_LOG_FATAL("Error polling agent (%d)", ret);
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
            SAM2_LOG_WARN("juice_user_poll was called %d times. This is inefficent", debug_loop_count);
        }
    }

    // Reconstruct input required for next tick if we're spectating
    if (ulnet_is_spectator(session, session->our_peer_id)) {
        for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
            if (session->room_we_are_in.peer_ids[p] > SAM2_PORT_SENTINELS_MAX) {
                int history_index_for_frame = (session->frame_counter / ULNET_DELAY_BUFFER_SIZE) % ULNET_STATE_PACKET_HISTORY_SIZE;
                arena_ref_t ref = session->state_packet_history[p][history_index_for_frame];
                uint8_t *packet_data = (uint8_t *) arena_deref(&session->arena, ref);

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

IMH(ImGui::SeparatorText("Things We are Waiting on Before we can Tick");)
IMH(if                            (session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) { ImGui::Text("Waiting for savestate"); })
    bool netplay_ready_to_tick = !(session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL);
    for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
        if (session->room_we_are_in.peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;
    IMH(if                      (session->state[p].frame <  session->frame_counter) { ImGui::Text("Input state on port %d is too old", p); })
        netplay_ready_to_tick &= session->state[p].frame >= session->frame_counter;
    IMH(if                      (session->state[p].frame >= session->frame_counter + ULNET_DELAY_BUFFER_SIZE) { ImGui::Text("Input state on port %d is too new (ahead by %" PRId64 " frames)", p, session->state[p].frame - (session->frame_counter + ULNET_DELAY_BUFFER_SIZE)); })
        netplay_ready_to_tick &= session->state[p].frame <  session->frame_counter + ULNET_DELAY_BUFFER_SIZE; // This is needed for spectators only. By protocol it should always true for non-spectators unless we have a bug or someone is misbehaving
    }

    if (!(session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) && our_port != -1 && our_port < SAM2_SPECTATOR_START) {
        int64_t frames_buffered = session->state[our_port].frame - session->frame_counter + 1;
        assert(frames_buffered <= ULNET_DELAY_BUFFER_SIZE);
        assert(frames_buffered >= 0);
    IMH(if                      (frames_buffered <  session->delay_frames) { ImGui::Text("We have not buffered enough frames still need %" PRId64, session->delay_frames - frames_buffered); })
        netplay_ready_to_tick &= frames_buffered >= session->delay_frames;
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
        if (force_save_state_on_tick || session->peer_needs_sync_bitfield) {
            uint64_t start = ulnet__rdtsc();
            save_state_size = session->retro_serialize_size(session->user_ptr);
            if (save_state_size > save_state_capacity) {
                SAM2_LOG_WARN("Save state size %zu is larger than buffer size %zu", save_state_size, save_state_capacity);
                save_state = (uint8_t *) malloc(save_state_size);
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

        if (session->peer_needs_sync_bitfield) {
            for (uint64_t p = 0; p < SAM2_ARRAY_LENGTH(session->agent); p++) {
                if (session->peer_needs_sync_bitfield & (1ULL << p)) {
                    ulnet_send_save_state(session, p, save_state, save_state_size, save_state_frame);
                    session->peer_needs_sync_bitfield &= ~(1ULL << p);
                }
            }
        }

        if (!(session->flags & ULNET_SESSION_FLAG_TICKED)) {
            session->retro_run(session->user_ptr);
        }

        session->core_wants_tick_at_unix_usec += 1000000 / frame_rate;

        sam2_room_t new_room_state = session->room_we_are_in;
        ulnet__xor_delta(&new_room_state, &session->state[SAM2_AUTHORITY_INDEX].room_xor_delta[session->frame_counter % ULNET_DELAY_BUFFER_SIZE], sizeof(sam2_room_t));

        if (memcmp(&new_room_state, &session->room_we_are_in, sizeof(sam2_room_t)) != 0) {
            SAM2_LOG_INFO("Something about the room we're in was changed by the authority");

            // When the room changes reuse existing peer connections if possible
            for (int j = 0; j < SAM2_TOTAL_PEERS; j++) {
                for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
                    if (new_room_state.peer_ids[j] == session->agent_peer_ids[i]) {
                        if (new_room_state.peer_ids[j] <= SAM2_PORT_SENTINELS_MAX) continue;
                        ulnet_swap_agent(session, j, i); // Note: This mutates session->agent_peer_ids
                        break;
                    }
                }
            }

            // Create new connections for new peers and dispose of unneeded ones
            int our_new_port = sam2_get_port_of_peer(&new_room_state, session->our_peer_id);
            if (our_new_port != -1 && our_new_port < SAM2_SPECTATOR_START) {
                for (int p = 0; p < SAM2_PORT_MAX; p++) {
                    if (   new_room_state.peer_ids[p] > SAM2_PORT_SENTINELS_MAX
                        && new_room_state.peer_ids[p] != session->our_peer_id
                        && new_room_state.peer_ids[p] != session->agent_peer_ids[p]) {
                        if (session->agent[p]) {
                            ulnet_disconnect_peer(session, p);
                        }

                        // Convention: The peer with the lesser ID initiates ICE
                        if (session->our_peer_id < new_room_state.peer_ids[p]) {
                            ulnet_startup_ice_for_peer(session, new_room_state.peer_ids[p], p, NULL);
                        }
                    }
                }
            }

            for (int p = 0; p < SAM2_SPECTATOR_START; p++) {
                if (new_room_state.peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;

                if (new_room_state.peer_ids[p] != session->room_we_are_in.peer_ids[p]) {
                    session->state[p].frame = SAM2_MAX(session->state[p].frame, session->frame_counter);
                }
            }

            session->room_we_are_in = new_room_state;
            if (!(session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)) {
                SAM2_LOG_INFO("Client %05" PRId16 " abandoned the room '%s'", session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX], session->room_we_are_in.name);
                for (int peer_port = 0; peer_port < SAM2_ARRAY_LENGTH(session->agent); peer_port++) {
                    if (session->agent[peer_port]) {
                        ulnet_disconnect_peer(session, peer_port);
                    }
                    session->room_we_are_in.peer_ids[peer_port] = SAM2_PORT_AVAILABLE;
                }
                ulnet_session_init_defaulted(session);
            }
        }

        // Room could have changed at this point so recompute our_port
        our_port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);

        if (   session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED
            && status & ULNET_POLL_SESSION_SAVED_STATE
            && our_port != -1
            && our_port < SAM2_SPECTATOR_START) {
            session->state[our_port].save_state_frame = save_state_frame;
            session->state[our_port].save_state_hash[save_state_frame % ULNET_DELAY_BUFFER_SIZE] = ulnet_xxh32(save_state, save_state_size, 0);
            session->state[our_port].input_state_hash[save_state_frame % ULNET_DELAY_BUFFER_SIZE] = ulnet_xxh32(session->state[our_port].input_state, sizeof(session->state[our_port].input_state), 0);
        }

        if (save_state_allocated) {
            free(save_state);
            save_state = NULL;
        }

        // Ideally I'd place this right after ticking the core, but we need to update the room state first
        session->frame_counter++;
    }

    return status;
}

ULNET_LINKAGE void ulnet_swap_agent(ulnet_session_t *session, int peer_existing_port, int peer_new_port) {
    if (peer_existing_port == peer_new_port) return;

    #define ULNET__SWAP(x, y, T) do { T temp = (x); (x) = (y); (y) = temp; } while(0)
    ULNET__SWAP(session->agent[peer_existing_port], session->agent[peer_new_port], juice_agent_t *);
    ULNET__SWAP(session->reliable_tx_next_seq[peer_existing_port], session->reliable_tx_next_seq[peer_new_port], uint16_t);
    ULNET__SWAP(session->reliable_tx_missing [peer_existing_port], session->reliable_tx_missing [peer_new_port], uint64_t);
    ULNET__SWAP(session->reliable_rx_greatest[peer_existing_port], session->reliable_rx_greatest[peer_new_port], uint16_t);
    ULNET__SWAP(session->reliable_rx_missing [peer_existing_port], session->reliable_rx_missing [peer_new_port], uint64_t);
    ULNET__SWAP(session->agent_peer_ids[peer_existing_port], session->agent_peer_ids[peer_new_port], int64_t);
}

static void ulnet_peer_init_defaulted(ulnet_session_t *session, int peer_port) {
    session->agent_peer_ids       [peer_port] = 0;
    session->reliable_rx_greatest [peer_port] = 0;
    session->reliable_tx_greatest [peer_port] = 0;
    session->reliable_tx_next_seq [peer_port] = 0;
    session->reliable_tx_missing  [peer_port] = 0;
    session->reliable_rx_missing  [peer_port] = 0;
}

ULNET_LINKAGE void ulnet_disconnect_peer(ulnet_session_t *session, int peer_port) {
    session->peer_pending_disconnect_bitfield &= ~(1ULL << peer_port);

    if (peer_port > SAM2_AUTHORITY_INDEX) {
        SAM2_LOG_INFO("Disconnecting spectator %05" PRId16, session->room_we_are_in.peer_ids[peer_port]);
    } else {
        SAM2_LOG_INFO("Disconnecting Peer %05" PRId16, session->room_we_are_in.peer_ids[peer_port]);
    }

    assert(session->agent[peer_port] != NULL);
    juice_destroy(session->agent[peer_port]);
    session->agent[peer_port] = NULL;

    ulnet_peer_init_defaulted(session, peer_port);
}

static sam2_room_t ulnet__infer_future_room_we_are_in(ulnet_session_t *session) {
    // This looks weird but really we're just figuring out what the current state of the room
    // looks like so we can generate deltas against it
    sam2_room_t future_room_we_are_in = session->room_we_are_in;
    if (session->frame_counter != ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) {
        for (int64_t frame = session->frame_counter+1LL; frame < session->state[SAM2_AUTHORITY_INDEX].frame; frame++) {
            ulnet__xor_delta(
                &future_room_we_are_in,
                &session->state[SAM2_AUTHORITY_INDEX].room_xor_delta[frame % ULNET_DELAY_BUFFER_SIZE],
                sizeof(session->room_we_are_in)
            );
        }
    }

    return future_room_we_are_in;
}

static inline void ulnet__reset_save_state_bookkeeping(ulnet_session_t *session) {
    session->remote_packet_groups = FEC_PACKET_GROUPS_MAX;
    session->remote_savestate_transfer_offset = 0;
    memset(session->fec_index_counter, 0, sizeof(session->fec_index_counter));
}

ULNET_LINKAGE void ulnet_session_tear_down(ulnet_session_t *session) {
    if (session->agent[SAM2_AUTHORITY_INDEX]) {
        ulnet_message_send(session, SAM2_AUTHORITY_INDEX, (const uint8_t *) ulnet_exit_header);
    }

    for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
        if (session->agent[i]) {
            ulnet_disconnect_peer(session, i);
        }
    }

    session->room_we_are_in.flags &= ~SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session->our_peer_id;
    session->frame_counter = 0;
    session->state[SAM2_AUTHORITY_INDEX].frame = 0;
}

ULNET_LINKAGE void ulnet_session_init_defaulted(ulnet_session_t *session) {
    for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
        assert(session->agent[i] == NULL);

        ulnet_peer_init_defaulted(session, i);
    }

    memset(&session->state, 0, sizeof(session->state));

    memset(session->state_packet_history, 0, sizeof(session->state_packet_history));

    session->reliable_next_retransmit_time = 0;

    session->frame_counter = 0;
    session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session->our_peer_id;

    ulnet__reset_save_state_bookkeeping(session);
}

// MARK: libjuice callbacks
static void ulnet__on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p;
    SAM2_LOCATE(session->agent, agent, p);
    if (p == -1) {
        SAM2_LOG_ERROR("Couldn't find agent on port=%d", p);
        return;
    }

    if (   state == JUICE_STATE_CONNECTED
        && session->our_peer_id == session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
        SAM2_LOG_INFO("Setting peer needs sync bit for peer %05" PRId16, session->our_peer_id);
        session->peer_needs_sync_bitfield |= (1ULL << p);
    } else if (state == JUICE_STATE_FAILED) {
        //ulnet_disconnect_peer(session, p); // This is called from within juice_user_poll()... So freeing the agent here isn't safe
        session->peer_pending_disconnect_bitfield |= (1ULL << p);
    }
}

static void ulnet__on_candidate(juice_agent_t *agent, const char *sdp, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p;
    SAM2_LOCATE(session->agent, agent, p);
    if (p == -1) {
        SAM2_LOG_ERROR("No agent found");
        return;
    }

    sam2_signal_message_t response = { SAM2_SIGN_HEADER };

    response.peer_id = session->agent_peer_ids[p];
    if (strlen(sdp) < sizeof(response.ice_sdp)) {
        strcpy(response.ice_sdp, sdp);
        session->sam2_send_callback(session->user_ptr, (char *) &response);
    } else {
        SAM2_LOG_ERROR("Candidate too large");
        return;
    }
}

static void ulnet__on_gathering_done(juice_agent_t *agent, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    int p;
    SAM2_LOCATE(session->agent, agent, p);
    if (p == -1) {
        SAM2_LOG_ERROR("No agent found");
        return;
    }

    sam2_signal_message_t response = { SAM2_SIGN_HEADER };

    response.peer_id = session->agent_peer_ids[p];
    session->sam2_send_callback(session->user_ptr, (char *) &response);
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
            SAM2_LOG_ERROR("Save state hash mismatch for frame %" PRId64 " Our hash: %016" PRIx32 " Their hash: %016" PRIx32,
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

static void ulnet__process_udp_packet(ulnet_session_t *session, int p, arena_ref_t packet_ref);
// MARK: UDP Packet Processing
ULNET_LINKAGE void ulnet_receive_packet_callback(juice_agent_t *agent, const char *packet, size_t size, void *user_ptr) {
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

    arena_ref_t packet_ref = arena_alloc(&session->arena, size);
    memcpy(arena_deref(&session->arena, packet_ref), packet, size);
    session->packet_history[p][session->packet_history_next[p]++] = packet_ref;

    if ((packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_RELIABLE) {
        uint16_t sequence = ((uint16_t)packet[2] << 8) | packet[1];
        session->reliable_rx_packet_history[p][sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE] = packet_ref;
    }

    if (session->flags & ULNET_SESSION_FLAG_READY_TO_TICK_SET) {
        SAM2_LOG_ERROR("Received a UDP packet while we were ready to tick. Set a breakpoint here to investigate");
    }

    ulnet__process_udp_packet(session, p, packet_ref); // Fallthrough to the next function
}

static void ulnet__process_udp_packet(ulnet_session_t *session, int p, arena_ref_t packet_ref) {
    const char *data = (const char *) arena_deref(&session->arena, packet_ref);
    size_t size = packet_ref.size;

    if (size == 0) {
        SAM2_LOG_WARN("Received a UDP packet with no payload");
        return;
    }

    uint8_t channel_and_flags = data[0];

    switch (channel_and_flags & ULNET_CHANNEL_MASK) {
    case ULNET_CHANNEL_EXTRA: {
        assert(!"This is an error currently\n");
        break;
    }
    case ULNET_CHANNEL_ASCII: {
        SAM2_LOG_INFO("Received message with header '%.*s' from peer %05" PRIu16 " on channel 0x%" PRIx8 " with %zu bytes",
            (int)SAM2_MIN(size, SAM2_HEADER_SIZE), data, session->agent_peer_ids[p], channel_and_flags & ULNET_CHANNEL_MASK, size);

        if (sam2_header_matches(data, ulnet_exit_header)) {
            if (p >= SAM2_SPECTATOR_START) {
                session->peer_pending_disconnect_bitfield |= (1ULL << p);

                if (ulnet_is_authority(session)) {
                    sam2_room_t future_room_we_are_in = ulnet__infer_future_room_we_are_in(session);
                    session->next_room_xor_delta.peer_ids[p] = future_room_we_are_in.peer_ids[p] ^ SAM2_PORT_AVAILABLE;
                }
            } else {
                SAM2_LOG_WARN("Protocol violation: room.peer_ids[%d]=%05" PRId16 " signaled disconnect before exiting room", p, session->room_we_are_in.peer_ids[p]);
                sam2_error_message_t error = {
                    SAM2_FAIL_HEADER,
                    session->room_we_are_in.peer_ids[p],
                    "Protocol violation: Signaled disconnect before detatching port",
                    SAM2_RESPONSE_AUTHORITY_ERROR
                };

                session->sam2_send_callback(session->user_ptr, (char *) &error);
                // @todo Resync broadcast
            }
        } else if (sam2_header_matches(data, sam2_join_header)) {
            // @todo This can be much simpler
            sam2_room_join_message_t join_message;
            memcpy(&join_message, data, sizeof(join_message));
            join_message.peer_id = session->agent_peer_ids[p];
            if (ulnet_process_message(session, (const char *) &join_message) != 0) {
                SAM2_LOG_ERROR("Failed to process join message");
            }
        }

        break;
    }
    case ULNET_CHANNEL_RELIABLE: {
        int nbytes = data[0] & ULNET_RELIABLE_MASK_ACK_BYTES;
        int offset = ULNET_HEADER_SIZE;
        if (nbytes > 8) {
            SAM2_LOG_WARN("Received a packet with too many ACK bytes (%d)", nbytes);
            break;
        }

        uint16_t *greatest = &session->reliable_rx_greatest[p];
        uint64_t *missing  = &session->reliable_rx_missing [p];
        uint16_t sequence_start = (*greatest + ULNET__CLZ(*missing) - 63) & 0xFFFF;
        if (!(data[0] & ULNET_RELIABLE_FLAG_ACK_BITS_ONLY)) {
            // Read sequence of the transmitted packet
            uint16_t rx_sequence;
            if (offset + sizeof(rx_sequence) > size) {
                SAM2_LOG_WARN("Reliable packet too small to contain sequence number");
                break;
            }
            memcpy(&rx_sequence, &data[offset], sizeof(rx_sequence));
            offset += sizeof(rx_sequence);

            // Update receive-side sliding-window
            if (ulnet__sequence_greater_than(rx_sequence, *greatest)) {
                uint16_t diff = (rx_sequence - *greatest) & 0xFFFF;
                if (diff >= 64) { *missing = UINT64_MAX; }
                else            { *missing = ((*missing<<diff)|((1ULL<<(diff))-2)); }
                *greatest = rx_sequence;
            } else {
                uint16_t diff = (*greatest - rx_sequence) & 0xFFFF;
                if (diff < 64) *missing &= ~(1ULL<<(diff));
            }
        }

        // Process packets as permitted by sliding window logic
        uint16_t sequence_end = (*greatest + ULNET__CLZ(*missing) - 63) & 0xFFFF;
        for (uint16_t seq = sequence_start; ulnet__sequence_less_than(seq, sequence_end); seq++) {
            arena_ref_t rx_packet_ref = session->reliable_rx_packet_history[p][seq % ULNET_RELIABLE_ACK_BUFFER_SIZE];
            uint8_t *rx_packet = (uint8_t *)arena_deref(&session->arena, rx_packet_ref);
            if (   rx_packet == NULL
                || ((uint16_t)rx_packet[2] << 8 | rx_packet[1]) != seq) {
                SAM2_LOG_ERROR("We have fallen behind packet seq=%d overwritten", seq);
                continue;
            }

            int offset_to_payload = ulnet_wrapped_header_size(rx_packet, rx_packet_ref.size);
            uint8_t *rx_packet_payload = &rx_packet[offset_to_payload];

            if ((rx_packet[offset_to_payload] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_RELIABLE) {
                SAM2_LOG_WARN("Received a double-wrapped reliable packet");
            } else {
                ulnet__process_udp_packet(session, p, arena_reref(rx_packet_ref, offset_to_payload));
            }
        }

        // Look at the ack bits if they're there
        if (nbytes > 0) {
            uint16_t tx_greatest;
            if (offset + sizeof(tx_greatest) + nbytes > size) {
                SAM2_LOG_WARN("Reliable packet too small for ACK bitfield (%zu bytes needed, %zu available)",
                    offset + sizeof(uint16_t) + nbytes, size);
                break;
            }

            uint64_t tx_missing = 0;
            memcpy(&tx_greatest, &data[offset], sizeof(tx_greatest));
            offset += sizeof(tx_greatest);
            memcpy(&tx_missing, &data[offset], nbytes);
            offset += nbytes;

            if (   ulnet__sequence_greater_than(tx_greatest, session->reliable_tx_greatest[p])
                // This next check ensures we store the most recent missing bitfield when the greatest transmit sequence is the same
                || (tx_greatest == session->reliable_tx_greatest[p] && ULNET__POPCNT(tx_missing) < ULNET__POPCNT(session->reliable_tx_missing[p]))) {
                session->reliable_tx_greatest[p] = tx_greatest;
                session->reliable_tx_missing[p] = tx_missing;
            }
        }

        if (data[0] & ULNET_RELIABLE_FLAG_ACK_BITS_ONLY) {
            ulnet__process_udp_packet(session, p, arena_reref(packet_ref, offset));
        }

        break;
    }
    case ULNET_CHANNEL_INPUT: {
        assert(size <= ULNET_PACKET_SIZE_BYTES_MAX);

        ulnet_state_packet_t *input_packet = (ulnet_state_packet_t *) data; // @todo Violates strict aliasing rule
        int8_t original_sender_port = data[0] & ULNET_FLAGS_MASK;

        if (   p != original_sender_port
            && p != SAM2_AUTHORITY_INDEX) {
            SAM2_LOG_WARN("Non-authority gave us someones input eventually this should be verified with a signature");
        }

        if (p >= SAM2_SPECTATOR_START) {
            SAM2_LOG_WARN("A spectator sent us a UDP packet for unsupported channel ULNET_CHANNEL_INPUT");
            return;
        }

        if (original_sender_port >= SAM2_PORT_MAX+1) {
            SAM2_LOG_WARN("Received input packet for port %d which is out of range", original_sender_port);
            break;
        }

        int64_t coded_state_size = size - ULNET_HEADER_SIZE;

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

            ulnet_update_state_history(session, packet_ref);

            // Broadcast the input packet to spectators
            if (ulnet_is_authority(session)) {
                for (int s = SAM2_SPECTATOR_START; s < SAM2_TOTAL_PEERS; s++) {
                    ulnet_send(session, s, (const uint8_t *)data, size);
                }
            }
        }

        // Check for desync
        int our_port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);
        if (our_port != -1 && our_port < SAM2_SPECTATOR_START) {
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

        if (p != SAM2_AUTHORITY_INDEX) {
            printf("Received savestate transfer packet from non-authority agent\n");
            break;
        }

        if (size < sizeof(ulnet_save_state_packet_header_t)) {
            SAM2_LOG_WARN("Recv savestate transfer packet with size smaller than header");
            break;
        }

        if (size > ULNET_PACKET_SIZE_BYTES_MAX) {
            SAM2_LOG_WARN("Recv savestate transfer packet potentially larger than MTU");
        }

        ulnet_save_state_packet_header_t savestate_transfer_header;
        memcpy(&savestate_transfer_header, data, sizeof(ulnet_save_state_packet_header_t)); // Strict-aliasing

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

        if (sequence_hi >= FEC_PACKET_GROUPS_MAX) {
            SAM2_LOG_WARN("Received savestate transfer packet with sequence_hi >= FEC_PACKET_GROUPS_MAX");
            break;
        }

        if (session->fec_index_counter[sequence_hi] == k) {
            // We already have received enough Reed-Solomon blocks to decode the payload; we can ignore this packet
            break;
        }

        uint8_t sequence_lo = savestate_transfer_header.sequence_lo;

        SAM2_LOG_DEBUG("Received savestate packet sequence_hi: %hhu sequence_lo: %hhu", sequence_hi, sequence_lo);

        size_t payload_size = size - sizeof(ulnet_save_state_packet_header_t);
        arena_ref_t ref = arena_alloc(&session->arena, payload_size);
        memcpy(arena_deref(&session->arena, ref), data + sizeof(ulnet_save_state_packet_header_t), payload_size);

        session->remote_savestate_transfer_offset += size;

        session->packet_reference[sequence_hi][session->fec_index_counter[sequence_hi]] = ref;
        session->fec_index [sequence_hi][session->fec_index_counter[sequence_hi]++] = sequence_lo;

        if (session->fec_index_counter[sequence_hi] == k) {
            SAM2_LOG_DEBUG("Received all the savestate data for packet group: %hhu", sequence_hi);
            void *fec_packet[GF_SIZE - FEC_REDUNDANT_BLOCKS];
            for (int i = 0; i < k; i++) {
                fec_packet[i] = arena_deref(&session->arena, session->packet_reference[sequence_hi][k]);
            }

            int redudant_blocks_sent = k * FEC_REDUNDANT_BLOCKS / (GF_SIZE - FEC_REDUNDANT_BLOCKS);
            void *rs_code = fec_new(k, k + redudant_blocks_sent);
            int rs_block_size = (int) (size - sizeof(ulnet_save_state_packet_header_t));
            int status = fec_decode(rs_code, fec_packet, session->fec_index[sequence_hi], rs_block_size);
            assert(status == 0);
            fec_free(rs_code);

            bool all_data_decoded = true;
            for (int i = 0; i < session->remote_packet_groups; i++) {
                all_data_decoded &= session->fec_index_counter[i] >= k;
            }

            if (all_data_decoded) {
                size_t ret = 0;
                uint32_t their_savestate_transfer_payload_xxhash = 0;
                uint32_t   our_savestate_transfer_payload_xxhash = 0;
                unsigned char *save_state_data = NULL;
                savestate_transfer_payload_t *savestate_transfer_payload = (savestate_transfer_payload_t *) malloc(sizeof(savestate_transfer_payload_t) /* Fixed size header */ + k * session->remote_packet_groups * rs_block_size);

                int32_t remote_payload_size = 0;
                for (int i = 0; i < k; i++) {
                    for (int j = 0; j < session->remote_packet_groups; j++) {
                        void *decoded_packet = arena_deref(&session->arena, session->packet_reference[j][i]);
                        if (decoded_packet == NULL) {
                            SAM2_LOG_ERROR("Savestate transfer packet already overwritten");
                            goto cleanup;
                        }
                        memcpy(((uint8_t *) savestate_transfer_payload) + remote_payload_size, decoded_packet, rs_block_size);
                        remote_payload_size += rs_block_size;
                    }
                }

                SAM2_LOG_INFO("Received savestate transfer payload for frame %" PRId64 "", savestate_transfer_payload->frame_counter);

                if (   savestate_transfer_payload->total_size_bytes > k * (int) rs_block_size * session->remote_packet_groups
                    || savestate_transfer_payload->total_size_bytes < 0) {
                    SAM2_LOG_ERROR("Savestate transfer payload total size would out-of-bounds when computing hash: %" PRId64 "", savestate_transfer_payload->total_size_bytes);
                    goto cleanup;
                }

                their_savestate_transfer_payload_xxhash = savestate_transfer_payload->xxhash;
                savestate_transfer_payload->xxhash = 0; // Needed to recompute the hash correctly
                our_savestate_transfer_payload_xxhash = ulnet_xxh32(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes, 0);

                if (their_savestate_transfer_payload_xxhash != our_savestate_transfer_payload_xxhash) {
                    SAM2_LOG_ERROR("Savestate transfer payload hash mismatch: %" PRIx32 " != %" PRIx32 "", savestate_transfer_payload->xxhash, our_savestate_transfer_payload_xxhash);
                    goto cleanup;
                }

                ret = ZSTD_decompress(
                    session->core_options, sizeof(session->core_options),
                    savestate_transfer_payload->compressed_data + savestate_transfer_payload->compressed_savestate_size,
                    savestate_transfer_payload->compressed_options_size
                );

                if (ZSTD_isError(ret)) {
                    SAM2_LOG_ERROR("Error decompressing core options: %s", ZSTD_getErrorName(ret));
                } else {
                    session->flags |= ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY;
                    //session.retro_run(); // Apply options before loading savestate; Lets hope this isn't necessary

                    save_state_data = (unsigned char *) malloc(savestate_transfer_payload->decompressed_savestate_size);

                    int64_t save_state_size = ZSTD_decompress(
                        save_state_data,
                        savestate_transfer_payload->decompressed_savestate_size,
                        savestate_transfer_payload->compressed_data,
                        savestate_transfer_payload->compressed_savestate_size
                    );

                    if (ZSTD_isError(save_state_size)) {
                        SAM2_LOG_ERROR("Error decompressing savestate: %s", ZSTD_getErrorName(save_state_size));
                    } else {
                        if (!session->retro_unserialize(session->user_ptr, save_state_data, save_state_size)) {
                            SAM2_LOG_ERROR("Failed to load savestate");
                        } else {
                            SAM2_LOG_DEBUG("Save state loaded");
                            session->frame_counter = savestate_transfer_payload->frame_counter;
                            session->room_we_are_in = savestate_transfer_payload->room;
                        }
                    }
                }

cleanup:
                if (save_state_data != NULL) {
                    free(save_state_data);
                }

                free(savestate_transfer_payload);

                ulnet__reset_save_state_bookkeeping(session);
            }
        }
        break;
    }
    default:
        SAM2_LOG_WARN("Unknown channel: 0x%" PRIx8 " with flags: 0x%" PRIx8 , (data[0] & ULNET_CHANNEL_MASK) >> 4, data[0] & ULNET_FLAGS_MASK);
    }
}


ULNET_LINKAGE void ulnet_startup_ice_for_peer(ulnet_session_t *session, uint64_t peer_id, int p, const char *remote_description) {
    if (p < 0 || p >= SAM2_TOTAL_PEERS) {
        SAM2_LOG_FATAL("Invalid peer port %d", p);
    }

    if (peer_id <= SAM2_PORT_SENTINELS_MAX) {
        SAM2_LOG_FATAL("Peer ID cannot be zero");
    }

    SAM2_LOG_INFO("Starting Interactive-Connectivity-Establishment for peer %05" PRId64, peer_id);

    juice_config_t config;
    memset(&config, 0, sizeof(config));

    // STUN server example*
    config.concurrency_mode = JUICE_CONCURRENCY_MODE_USER;
    config.stun_server_host = "stun2.l.google.com"; // @todo Put a bad url here to test how to handle that
    config.stun_server_port = 19302;
    //config.bind_address = "127.0.0.1";

    config.cb_state_changed = ulnet__on_state_changed;
    config.cb_candidate = ulnet__on_candidate;
    config.cb_gathering_done = ulnet__on_gathering_done;
    config.cb_recv = ulnet_receive_packet_callback;

    config.user_ptr = (void *) session;

    session->agent_peer_ids[p] = peer_id;

    assert(session->agent[p] == NULL);
    session->agent[p] = juice_create(&config);

    if (remote_description) {
        // Right now I think there could be some kind of bug or race condition in my code or libjuice when there
        // is an ICE role conflict. A role conflict is benign, but when a spectator connects the authority will never fully
        // establish the connection even though the spectator manages to. If I avoid the role conflict by setting
        // the remote description here then my connection establishes fine, but I should look into this eventually @todo
        juice_set_remote_description(session->agent[p], remote_description);
    }

    sam2_signal_message_t signal_message = { SAM2_SIGN_HEADER };
    signal_message.peer_id = peer_id;
    juice_get_local_description(session->agent[p], signal_message.ice_sdp, sizeof(signal_message.ice_sdp));
    session->sam2_send_callback(session->user_ptr, (char *) &signal_message);

    // This call starts an asynchronous task that requires periodic polling via juice_user_poll to complete
    // it will call the ulnet__on_gathering_done callback once it's finished
    juice_gather_candidates(session->agent[p]);

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
        }
    } else if (sam2_header_matches(response, sam2_join_header)) {
        sam2_room_join_message_t *room_join = (sam2_room_join_message_t *) response;
        if (ulnet_is_authority(session)) {
            if (room_join->room.peer_ids[SAM2_AUTHORITY_INDEX] != session->our_peer_id) {
                SAM2_LOG_WARN("Authority can't join their own room");
                return -1;
            }
        } else {
            SAM2_LOG_FATAL("We shouldn't get here anymore"); // @todo Make error instead
        }

        sam2_room_t future_room_we_are_in = ulnet__infer_future_room_we_are_in(session);
        sam2_room_t futureer_room_we_are_in = future_room_we_are_in;
        ulnet__xor_delta(&futureer_room_we_are_in, &session->next_room_xor_delta, sizeof(session->room_we_are_in));

        SAM2_LOG_INFO("Peer %05" PRId16 " has asked to change something about the room in some way e.g. leaving, joining, etc.", (uint16_t) room_join->peer_id);
        assert(sam2_same_room(&future_room_we_are_in, &room_join->room));

        int current_port = sam2_get_port_of_peer(&future_room_we_are_in, room_join->peer_id);
        int desired_port = sam2_get_port_of_peer(&room_join->room, room_join->peer_id);

        if (desired_port == -1) {
            if (current_port != -1) {
                SAM2_LOG_INFO("Peer %05" PRId16 " left", (uint16_t) room_join->peer_id);

                int avail_port = SAM2_SPECTATOR_START;
                for (; avail_port < SAM2_TOTAL_PEERS; avail_port++) if (futureer_room_we_are_in.peer_ids[avail_port] == SAM2_PORT_AVAILABLE) break;

                if (avail_port == SAM2_TOTAL_PEERS) {
                    SAM2_LOG_WARN("No available spectator port found for peer %05" PRId16 ". Request rejected", (uint16_t) room_join->peer_id);
                } else {
                    futureer_room_we_are_in.peer_ids[current_port] = SAM2_PORT_AVAILABLE;
                    futureer_room_we_are_in.peer_ids[avail_port] = room_join->peer_id;
                }
            } else {
                SAM2_LOG_WARN("Peer %05" PRId16 " did something that doesn't look like joining or leaving", (uint16_t) room_join->peer_id);

                sam2_error_message_t error = {
                    SAM2_FAIL_HEADER,
                    (uint16_t) room_join->peer_id,
                    "Client made unsupported join request",
                    SAM2_RESPONSE_AUTHORITY_ERROR
                };

                session->sam2_send_callback(session->user_ptr, (char *) &error);
            }
        } else {
            if (current_port != desired_port) {
                if (future_room_we_are_in.peer_ids[desired_port] != SAM2_PORT_AVAILABLE) {
                    SAM2_LOG_INFO("Peer %05" PRId16 " tried to join on unavailable port", room_join->room.peer_ids[current_port]);
                    sam2_error_message_t error = {
                        SAM2_FAIL_HEADER,
                        (uint16_t) room_join->peer_id,
                        "Peer tried to join on unavailable port",
                        SAM2_RESPONSE_AUTHORITY_ERROR
                    };

                    session->sam2_send_callback(session->user_ptr, (char *) &error);
                } else {
                    futureer_room_we_are_in.peer_ids[desired_port] = room_join->peer_id;

                    if (current_port != -1) {
                        futureer_room_we_are_in.peer_ids[current_port] = SAM2_PORT_AVAILABLE;
                    }
                }
            }
        }

        // @todo We should mask for values peers are allowed to change
        if (room_join->peer_id == session->our_peer_id) {
            futureer_room_we_are_in.flags = room_join->room.flags;
        }

        session->next_room_xor_delta = futureer_room_we_are_in;
        ulnet__xor_delta(&session->next_room_xor_delta, &future_room_we_are_in, sizeof(session->room_we_are_in));

        sam2_room_t no_xor_delta = {0};
        if (memcmp(&session->next_room_xor_delta, &no_xor_delta, sizeof(sam2_room_t)) == 0) {
            SAM2_LOG_WARN("Peer %05" PRId16 " didn't change anything after making join request", (uint16_t) room_join->peer_id);
        } else {
            sam2_room_make_message_t make_message = {
                SAM2_MAKE_HEADER,
                futureer_room_we_are_in
            };

            session->sam2_send_callback(session->user_ptr, (char *) &make_message);
        }
    }  else if (sam2_header_matches(response, sam2_sign_header)) {
        sam2_signal_message_t *room_signal = (sam2_signal_message_t *) response;
        SAM2_LOG_INFO("Received signal from peer %05" PRId16 "", room_signal->peer_id);

//        if (!(session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)) {
//            SAM2_LOG_WARN("Ignoring signal from %05" PRId16 ". We aren't in a netplay session presently", room_signal->peer_id);
//            return 0;
//        }

        int p = 0;
        if (ulnet_is_authority(session)) {
            SAM2_LOCATE(session->agent_peer_ids, room_signal->peer_id, p);

            if (p == -1) {
                SAM2_LOG_INFO("Received signal from unknown peer");
                sam2_room_t future_room_we_are_in = ulnet__infer_future_room_we_are_in(session);
                for (p = SAM2_SPECTATOR_START; p < SAM2_TOTAL_PEERS; p++) if (future_room_we_are_in.peer_ids[p] == SAM2_PORT_AVAILABLE) break;

                if (p == SAM2_TOTAL_PEERS) {
                    SAM2_LOG_WARN("We can't let them in as a spectator there are too many spectators");

                    static sam2_error_message_t error = {
                        SAM2_FAIL_HEADER, 0,
                        "Authority has reached the maximum number of spectators",
                        SAM2_RESPONSE_ROOM_FULL
                    };

                    session->sam2_send_callback(session->user_ptr, (char *) &error);
                } else {
                    SAM2_LOG_INFO("We are letting them in as a spectator");
                    session->next_room_xor_delta.peer_ids[p] = future_room_we_are_in.peer_ids[p] ^ room_signal->peer_id;
                }
            }
        } else {
            SAM2_LOCATE(session->room_we_are_in.peer_ids, room_signal->peer_id, p);

            if (p == -1) {
                SAM2_LOG_WARN("Received unknown signal when we weren't the authority");

                static sam2_error_message_t error = {
                    SAM2_FAIL_HEADER, 0,
                    "Received unknown signal when we weren't the authority",
                    SAM2_RESPONSE_PEER_ERROR
                };

                error.peer_id = room_signal->peer_id;

                session->sam2_send_callback(session->user_ptr, (char *) &error);
            }
        }

        if (p != -1 && session->agent[p] == NULL) {
            ulnet_startup_ice_for_peer(session, room_signal->peer_id, p, /* remote_desciption = */ room_signal->ice_sdp);
        }

        if (p != -1) { // Can fail if we run out of spots for spectators
            if (room_signal->ice_sdp[0] == '\0') {
                SAM2_LOG_INFO("Received remote gathering done from peer %05" PRId16 "", room_signal->peer_id);
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

// Pass in save state since often retro_serialize can tick the core
ULNET_LINKAGE void ulnet_send_save_state(ulnet_session_t *session, int port, void *save_state, size_t save_state_size, int64_t save_state_frame) {
    assert(save_state);

    int packet_payload_size_bytes = ULNET_PACKET_SIZE_BYTES_MAX - sizeof(ulnet_save_state_packet_header_t);
    int n, k, packet_groups;

    int64_t save_state_transfer_payload_compressed_bound_size_bytes = ZSTD_COMPRESSBOUND(save_state_size) + ZSTD_COMPRESSBOUND(sizeof(session->core_options));
    ulnet__logical_partition(sizeof(savestate_transfer_payload_t) /* Header */ + save_state_transfer_payload_compressed_bound_size_bytes,
                      FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size_bytes, &packet_groups);

    size_t savestate_transfer_payload_plus_parity_bound_bytes = packet_groups * n * packet_payload_size_bytes;

    // This points to the savestate transfer payload, but also the remaining bytes at the end hold our parity blocks
    // Having this data in a single contiguous buffer makes indexing easier
    savestate_transfer_payload_t *savestate_transfer_payload = (savestate_transfer_payload_t *) malloc(savestate_transfer_payload_plus_parity_bound_bytes);

    savestate_transfer_payload->decompressed_savestate_size = save_state_size;
    savestate_transfer_payload->compressed_savestate_size = ZSTD_compress(
        savestate_transfer_payload->compressed_data,
        save_state_transfer_payload_compressed_bound_size_bytes,
        save_state, save_state_size, session->zstd_compress_level
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

    ulnet__logical_partition(
        sizeof(savestate_transfer_payload_t) /* Header */ + savestate_transfer_payload->compressed_savestate_size + savestate_transfer_payload->compressed_options_size,
        FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size_bytes, &packet_groups
    );
    assert(savestate_transfer_payload_plus_parity_bound_bytes >= packet_groups * n * packet_payload_size_bytes); // If this fails my logic calculating the bounds was just wrong

    savestate_transfer_payload->frame_counter = save_state_frame;
    savestate_transfer_payload->room = session->room_we_are_in;
    savestate_transfer_payload->total_size_bytes = sizeof(savestate_transfer_payload_t) + savestate_transfer_payload->compressed_savestate_size + savestate_transfer_payload->compressed_options_size;

    savestate_transfer_payload->xxhash = 0;
    savestate_transfer_payload->xxhash = ulnet_xxh32(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes, 0);
    // Create parity blocks for Reed-Solomon. n - k in total for each packet group
    // We have "packet grouping" because pretty much every implementation of Reed-Solomon doesn't support more than 255 blocks
    // and unfragmented UDP packets over ethernet are limited to ULNET_PACKET_SIZE_BYTES_MAX
    // This makes the code more complicated and the error correcting properties slightly worse but it's a practical tradeoff
    void *rs_code = fec_new(k, n);
    for (int j = 0; j < packet_groups; j++) {
        void *data[255];

        for (int i = 0; i < n; i++) {
            data[i] = (unsigned char *) savestate_transfer_payload + ulnet__logical_partition_offset_bytes(j, i, packet_payload_size_bytes, packet_groups);
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

            memcpy(packet.payload, (unsigned char *) savestate_transfer_payload + ulnet__logical_partition_offset_bytes(j, i, packet_payload_size_bytes, packet_groups), packet_payload_size_bytes);

            int status = ulnet_send(session, port, (const uint8_t *) &packet, sizeof(ulnet_save_state_packet_header_t) + packet_payload_size_bytes);
            assert(status == 0);
        }
    }

    free(savestate_transfer_payload);
}

#if defined(ULNET_IMGUI)
ULNET_LINKAGE void ulnet_imgui_show_session(ulnet_session_t *session) {
    // Plot Input Packet Size vs. Frame
    // @todo The gaps in the graph can be explained by out-of-order arrival of packets I think I don't even record those to history but I should
    //       There is some other weird behavior that might be related to not checking the frame field in the packet if its too old it shouldn't be in the plot obviously
    if (session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED && ImGui::CollapsingHeader("Network Activity")) {
        ImPlot::SetNextAxisLimits(ImAxis_X1, session->frame_counter - ULNET_MAX_SAMPLE_SIZE, session->frame_counter, ImGuiCond_Always);
        ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, 512, ImGuiCond_Always);
        if (ImPlot::BeginPlot("State-Packet Size vs. Frame")) {
            ImPlot::SetupAxis(ImAxis_X1, "ulnet_state_t::frame");
            ImPlot::SetupAxis(ImAxis_Y1, "Size Bytes");

            for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
                if (session->room_we_are_in.peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;

                arena_ref_t ref = session->state_packet_history[p][session->frame_counter % ULNET_STATE_PACKET_HISTORY_SIZE];
                uint8_t *peer_packet = (uint8_t *)arena_deref(&session->arena, ref);
                session->input_packet_size[p][session->frame_counter % ULNET_MAX_SAMPLE_SIZE] = peer_packet ? ref.size : 0;

                char label[32];
                snprintf(label, sizeof(label), p == SAM2_AUTHORITY_INDEX ? "Authority" : "Port %d", p);

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
            snprintf(tabName, sizeof(tabName), p < SAM2_SPECTATOR_START ? "Peer %d (%05" PRId16 ")" : "Spectator (%05" PRId16 ")",
                     p, session->room_we_are_in.peer_ids[p]);

            if (ImGui::BeginTabItem(tabName)) {
                if (ImGui::CollapsingHeader("Reliable Protocol State", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Transmit: Next Seq=%u, Greatest Acked=%u", session->reliable_tx_next_seq[p], session->reliable_tx_greatest[p]);
                    ImGui::Text("Receive: Greatest Seq=%u", session->reliable_rx_greatest[p]);

                    double timeToRetransmit = session->reliable_next_retransmit_time - (ulnet__get_unix_time_microseconds() / 1e6);
                    ImGui::Text(timeToRetransmit > 0 ? "Next retransmit in: %.1f ms" : "Retransmission imminent", timeToRetransmit * 1000.0);
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

        arena_ref_t ref = session->packet_history[p][idx];
        uint8_t *packet_data = (uint8_t *)arena_deref(&session->arena, ref);
        int packet_size = ref.size;
        if (!packet_data || packet_size == 0) {
            continue;
        }

        ImGui::TableNextRow();

        // Direction
        ImGui::TableNextColumn();
        const char *dir = (ref.flags_and_generation & ULNET_PACKET_FLAG_TX_RELIABLE_RETRANSMIT) ? "re-TX" :
                        (ref.flags_and_generation & ULNET_PACKET_FLAG_TX) ? "TX" : "RX";
        ImVec4 dirColor = (ref.flags_and_generation & ULNET_PACKET_FLAG_TX) ?
                        ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImGui::TextColored(dirColor, "%s", dir);

        // Packet type
        ImGui::TableNextColumn();
        bool is_reliable = (packet_data[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_RELIABLE;
        bool is_reliable_ack = is_reliable && (packet_data[0] & ULNET_RELIABLE_FLAG_ACK_BITS_ONLY);

        int reliable_header_size = ulnet_wrapped_header_size(packet_data, packet_size);

        uint8_t *payload_start = &packet_data[reliable_header_size];
        uint8_t channel = payload_start[0] & ULNET_CHANNEL_MASK;

        const struct { uint8_t ch; const char *name; ImVec4 color; } channels[] = {
            {ULNET_CHANNEL_EXTRA, "Extra", {1.0f, 1.0f, 1.0f, 1.0f}},
            {ULNET_CHANNEL_INPUT, "Input", {0.5f, 1.0f, 0.5f, 1.0f}},
            {ULNET_CHANNEL_SPECTATOR_INPUT, "Spectator", {1.0f, 1.0f, 1.0f, 1.0f}},
            {ULNET_CHANNEL_SAVESTATE_TRANSFER, "Savestate", {1.0f, 0.6f, 0.0f, 1.0f}},
            {ULNET_CHANNEL_ASCII, "ASCII", {1.0f, 1.0f, 0.5f, 1.0f}},
            {ULNET_CHANNEL_RELIABLE, "Error", {1.0f, 1.0f, 1.0f, 1.0f}}
        };

        const char *channelName = "Unknown";
        ImVec4 channelColor = {1.0f, 1.0f, 1.0f, 1.0f};
        for (int j = 0; j < sizeof(channels)/sizeof(channels[0]); j++) {
            if (channel == channels[j].ch) {
                channelName = channels[j].name;
                channelColor = channels[j].color;
                break;
            }
        }
        ImGui::TextColored(channelColor, "%s", channelName);

        // Reliable status
        ImGui::TableNextColumn();
        if (is_reliable && packet_size >= 3) {
            uint16_t seq;
            memcpy(&seq, &packet_data[1], sizeof(seq));
            uint16_t diff = (session->reliable_tx_greatest[p] - seq) & 0xFFFF;

            const char *status = ulnet__sequence_greater_than(seq, session->reliable_tx_greatest[p]) ? "Indeterminate" :
                                (diff < 64 && (session->reliable_tx_missing[p] & (1ULL << diff))) ? "Unacked" : "Acked";
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

            int ack_bytes = packet_data[0] & ULNET_RELIABLE_MASK_ACK_BYTES;
            if (ack_bytes > 0) {
                int ack_offset = is_reliable_ack ? 1 : 3;
                if (packet_size >= ack_offset + 2 + ack_bytes) {
                    uint16_t rx_greatest;
                    uint64_t rx_missing = 0;
                    memcpy(&rx_greatest, &packet_data[ack_offset], sizeof(rx_greatest));
                    memcpy(&rx_missing, &packet_data[ack_offset + 2], ack_bytes);

                    if (pos > 0) pos += snprintf(details + pos, sizeof(details) - pos, ", ");
                    pos += snprintf(details + pos, sizeof(details) - pos, "ACKs=%u", rx_greatest);

                    int missing = ULNET__POPCNT(rx_missing);
                    if (missing > 0) pos += snprintf(details + pos, sizeof(details) - pos, ", Missing=%d", missing);
                }
            }
        }

        // Channel-specific details
        size_t payload_size = packet_size - reliable_header_size;
        if (pos > 0) pos += snprintf(details + pos, sizeof(details) - pos, " | ");

        if (channel == ULNET_CHANNEL_INPUT && payload_size > sizeof(ulnet_state_packet_t)) {
            int64_t frame = 0;
            rle8_decode(&payload_start[sizeof(ulnet_state_packet_t)],
                        payload_size - sizeof(ulnet_state_packet_t), (uint8_t *)&frame, sizeof(frame));
            pos += snprintf(details + pos, sizeof(details) - pos, "Frame %" PRId64, frame);
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
