#ifndef ULNET_H
#define ULNET_H

#include "sam2.h"

typedef struct juice_agent juice_agent_t;
#include "zstd.h"
#include "common/xxhash.h"
#include "fec.h"

#include <stdint.h>
#include <stdbool.h>

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

#define ULNET_HEADER_SIZE                     1
#define ULNET_FLAGS_MASK                      0x0F
#define ULNET_CHANNEL_MASK                    0xF0

#define ULNET_CHANNEL_EXTRA                   0x00
#define ULNET_CHANNEL_INPUT                   0x10
#define ULNET_CHANNEL_SPECTATOR_INPUT         0x20
#define ULNET_CHANNEL_SAVESTATE_TRANSFER      0x30
#define ULNET_CHANNEL_ASCII_1                 0x40
#define ULNET_CHANNEL_ASCII_2                 0x50
#define ULNET_CHANNEL_RELIABLE                0x60

#define ulnet_exit_header  "E" "X" "I" "T" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"

#define ULNET_WAITING_FOR_SAVE_STATE_SENTINEL INT64_MAX

#define ULNET_SESSION_FLAG_TICKED                 0b00000001ULL
#define ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY     0b00000010ULL
#define ULNET_SESSION_FLAG_READY_TO_TICK_SET      0b00000100ULL

// @todo Remove this define once it becomes possible through normal featureset
#define ULNET__DEBUG_EVERYONE_ON_PORT_0

#ifdef ULNET_IMGUI
#define ULNET_MAX_SAMPLE_SIZE 128
#endif

#define ULNET_RELIABLE_ACK_BUFFER_SIZE 64
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

typedef struct arena_reference {
    uint16_t size;
    uint16_t generation;
    uint32_t offset;
} arena_reference_t;
static const arena_reference_t arena_reference_null = { 0x0, 0x0, 0x0 }; // Null reference is characterized by 0's in all fields

typedef struct arena {
    uint16_t generation; // Wraps around on overflow
    uint32_t head; // Wraps around when exceeding arena size
    uint8_t arena[2 * 1024 * 1024];
} arena_t;

arena_reference_t arena_allocate(arena_t *arena, uint16_t size) {
    // Reject zero-sized and too-large allocations
    if (size == 0 || size > sizeof(arena->arena)) {
        return arena_reference_null;
    }

    // Check if allocation would exceed arena remaining space
    if (arena->head + size > sizeof(arena->arena)) {
        // Wrap around to beginning if we don't have space
        arena->head = 0;
        // Increment generation on wrap
        arena->generation++;
    }

    arena_reference_t ref = {
        size,
        arena->generation,
        arena->head
    };

    // Advance head pointer
    arena->head += size;

    return ref;
}

void *arena_dereference(arena_t *arena, arena_reference_t reference) {
    // Check for null reference
    if (reference.size == 0 && reference.generation == 0 && reference.offset == 0) {
        return NULL;
    }

    // Validate bounds
    if (reference.offset >= sizeof(arena->arena) ||
        reference.offset + reference.size > sizeof(arena->arena)) {
        return NULL;
    }

    void *ptr = &arena->arena[reference.offset];

    // Case 1: Same generation - definitely valid
    if (arena->generation == reference.generation) {
        return ptr;
    }

    // Case 2: Arena has wrapped around once (generation + 1)
    // Handle generation wraparound properly using modular arithmetic
    if (   ((arena->generation - reference.generation) & 0xFFFF) == 1
        && reference.offset >= arena->head) {
        // Return the original pointer - the data is still valid
        // This condition checks if the reference is in memory that hasn't been
        // overwritten yet after a wraparound
        return ptr;
    }

    // In all other cases, the memory has been overwritten
    return NULL;
}


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
    int64_t save_state_hash[ULNET_DELAY_BUFFER_SIZE];
    int64_t input_state_hash[ULNET_DELAY_BUFFER_SIZE];
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
    uint64_t encoding_chain; // @todo probably won't use this
    uint64_t xxhash;

    int64_t compressed_options_size;
    int64_t compressed_savestate_size;
    int64_t decompressed_savestate_size;
#if 0
    uint8_t compressed_savestate_data[compressed_savestate_size];
    uint8_t compressed_options_data[compressed_options_size];
#else
    uint8_t compressed_data[];
#endif
} savestate_transfer_payload_t;

typedef struct ulnet_session {
    int64_t frame_counter;
    int64_t delay_frames;
    int64_t core_wants_tick_at_unix_usec;
    int64_t flags;
    uint16_t our_peer_id;

    sam2_room_t room_we_are_in;
    sam2_room_t next_room_xor_delta;

    ulnet_core_option_t core_options[ULNET_CORE_OPTIONS_MAX]; // @todo I don't like this here

    juice_agent_t *agent               [SAM2_TOTAL_PEERS];
    uint64_t       agent_peer_ids      [SAM2_TOTAL_PEERS];
    int64_t        peer_desynced_frame [SAM2_TOTAL_PEERS];
    ulnet_state_t  state               [SAM2_PORT_MAX+1];
    ulnet_input_state_t spectator_suggested_input_state[SAM2_TOTAL_PEERS][ULNET_PORT_COUNT];
    unsigned char  state_packet_history[SAM2_TOTAL_PEERS][ULNET_STATE_PACKET_HISTORY_SIZE][ULNET_PACKET_SIZE_BYTES_MAX];
    uint64_t       peer_needs_sync_bitfield;
    uint64_t       peer_pending_disconnect_bitfield;

    // MARK: Reliable tx/rx
    double reliable_next_retransmit_time;
    struct reliable_endpoint_t *reliable_endpoint[SAM2_TOTAL_PEERS];

    uint8_t reliable_pending_header[SAM2_TOTAL_PEERS][ULNET_RELIABLE_ACK_BUFFER_SIZE]; // Acts as tags for the next union
    union {
        struct {
            int64_t frame;
        } channel_state;

        struct {
            arena_reference_t reference;
        } channel_ascii;
    } reliable_pending_metadata[SAM2_TOTAL_PEERS][ULNET_RELIABLE_ACK_BUFFER_SIZE];
    arena_t arena;
    uint16_t reliable_greatest_sequence[SAM2_TOTAL_PEERS]; // Greatest sequence number sent, wraps around

    // MARK: Save state transfer
    int zstd_compress_level;
    int64_t remote_savestate_transfer_offset;
    uint8_t remote_packet_groups; // This is used to bookkeep how much data we actually need to receive to reform the complete savestate
    arena_reference_t packet_reference[FEC_PACKET_GROUPS_MAX][GF_SIZE - FEC_REDUNDANT_BLOCKS];
    int fec_index[FEC_PACKET_GROUPS_MAX][GF_SIZE - FEC_REDUNDANT_BLOCKS];
    int fec_index_counter[FEC_PACKET_GROUPS_MAX]; // Counts packets received in each "packet group"

    void *user_ptr;
    int (*sam2_send_callback)(void *user_ptr, char *response);
    int (*populate_core_options_callback)(void *user_ptr, ulnet_core_option_t options[ULNET_CORE_OPTIONS_MAX]);

    bool (*retro_unserialize)(const void *data, size_t size);

    float debug_udp_recv_drop_rate;
    float debug_udp_send_drop_rate;

#ifdef ULNET_IMGUI
    int sample_size; // @todo Remove. This shouldn't really be needed and its awkward to initialize
    int input_packet_size[SAM2_PORT_MAX + 1][ULNET_MAX_SAMPLE_SIZE];
    int save_state_execution_time_cycles[ULNET_MAX_SAMPLE_SIZE];
#endif
} ulnet_session_t;

ULNET_LINKAGE int ulnet_process_message(ulnet_session_t *session, const void *response);
ULNET_LINKAGE void ulnet_send_save_state(ulnet_session_t *session, int port, void *save_state, size_t save_state_size, int64_t save_state_frame);
ULNET_LINKAGE void ulnet_startup_ice_for_peer(ulnet_session_t *session, uint64_t peer_id, int p, const char *remote_description);
ULNET_LINKAGE void ulnet_disconnect_peer(ulnet_session_t *session, int peer_port);
ULNET_LINKAGE void ulnet__swap_agent(ulnet_session_t *session, int peer_existing_port, int peer_new_port);
ULNET_LINKAGE sam2_room_t ulnet__infer_future_room_we_are_in(ulnet_session_t *session);
ULNET_LINKAGE void ulnet_session_init_defaulted(ulnet_session_t *session);

static inline int ulnet_our_port(ulnet_session_t *session) {
    // @todo There is a bug here where we are sending out packets as the authority when we are not the authority
    if (session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
        int port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);
        if (port == -1) {
            SAM2_LOG_FATAL("Our peer ID %05" PRId16 " not found in room", session->our_peer_id);
        }

        return port;
    } else {
        return SAM2_AUTHORITY_INDEX;
    }
}

static inline int ulnet_locate_spectator(sam2_room_t *room, uint64_t peer_id) {
    for (int i = SAM2_SPECTATOR_START; i < SAM2_TOTAL_PEERS; i++) {
        if (room->peer_ids[i] == peer_id) {
            return i;
        }
    }

    return -1;
}

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
#define IMH(statement) statement
#else
#define IMH(statement)
#endif

#include "reliable.h"
#ifdef __cplusplus
extern "C"
#endif
int reliable_sequence_greater_than( uint16_t s1, uint16_t s2 );
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
    if (!ulnet_is_spectator(session, session->our_peer_id) && session->state[ulnet_our_port(session)].frame < session->frame_counter + session->delay_frames) {
        // @todo The preincrement does not make sense to me here, but things have been working
        int64_t next_buffer_index = ++session->state[ulnet_our_port(session)].frame % ULNET_DELAY_BUFFER_SIZE;

        session->state[ulnet_our_port(session)].core_option[next_buffer_index] = *next_frame_option;
        memset(next_frame_option, 0, sizeof(*next_frame_option));

        //if (ulnet_is_authority(session)) {
            session->state[ulnet_our_port(session)].room_xor_delta[next_buffer_index] = session->next_room_xor_delta;
            memset(&session->next_room_xor_delta, 0, sizeof(session->next_room_xor_delta));
        //}

        // Incoporate input from spectators into our input. This has the drawback of round trip latency but requires a single connection to the server
        memset(session->state[ulnet_our_port(session)].input_state[next_buffer_index], 0, sizeof(session->state[ulnet_our_port(session)].input_state[next_buffer_index]));
        for (int i = 0; i < SAM2_ARRAY_LENGTH(session->agent); i++) {
            if (session->agent[i]) {
                for (int p = 0; p < SAM2_PORT_MAX; p++) {
                    for (int j = 0; j < SAM2_ARRAY_LENGTH(session->state[ulnet_our_port(session)].input_state[next_buffer_index][p]); j++) {
                        session->state[ulnet_our_port(session)].input_state[next_buffer_index][p][j] |= session->spectator_suggested_input_state[i][p][j];
                    }
                }
            }
        }

        return &session->state[ulnet_our_port(session)].input_state[next_buffer_index];
    } else if (ulnet_is_spectator(session, session->our_peer_id)) {
        memset(session->spectator_suggested_input_state[63], 0, sizeof(session->spectator_suggested_input_state[63]));
        return &session->spectator_suggested_input_state[63];
    }

    return NULL;
}

#ifdef _WIN32
int64_t get_unix_time_microseconds() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    ULARGE_INTEGER ul;
    ul.LowPart = ft.dwLowDateTime;
    ul.HighPart = ft.dwHighDateTime;

    int64_t unix_time = (int64_t)(ul.QuadPart - 116444736000000000LL) / 10;

    return unix_time;
}
#else
int64_t get_unix_time_microseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

uint64_t ulnet__rdtsc() {
#if defined(__aarch64__) || defined(__arm__)
    return 1000 * get_unix_time_microseconds();
#elif defined(_MSC_VER)   /* MSVC compiler */
    return __rdtsc();
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))   /* GCC compiler on x86 platforms */
    unsigned int lo, hi;
    __asm__ __volatile__ (
      "rdtsc" : "=a" (lo), "=d" (hi)
    );
    return ((uint64_t)hi << 32) | lo;
#else
    /* Fallback for other platforms/compilers */
    return 1000 * get_unix_time_microseconds();
#endif
}

double core_wants_tick_in_seconds(int64_t core_wants_tick_at_unix_usec) {
    double seconds = (core_wants_tick_at_unix_usec - get_unix_time_microseconds()) / 1000000.0;
    return seconds;
}

// Returns a negative number on error
static int ulnet__udp_send(ulnet_session_t *session, int port, const char *packet, size_t size) {
    // Basic packet validation
    if (size == 0) {
        SAM2_LOG_ERROR("Attempt to send empty packet");
        return -1;
    }

    if (size > ULNET_PACKET_SIZE_BYTES_MAX) {
        SAM2_LOG_ERROR("Packet size exceeds maximum: %zu bytes", size);
        return -1;
    }

    // Validate channel
    uint8_t channel_and_flags = (uint8_t)packet[0];
    uint8_t channel = channel_and_flags & ULNET_CHANNEL_MASK;

    // Perform channel-specific validation
    switch (channel) {
        case ULNET_CHANNEL_EXTRA: {
            SAM2_LOG_FATAL("Attempt to send packet with invalid channel: %d", channel);
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

    if (rand() / ((float) RAND_MAX) < session->debug_udp_send_drop_rate) {
        SAM2_LOG_DEBUG("Intentionally dropped a sent UDP packet");
        return 0;
    } else {
        return juice_send(session->agent[port], packet, size);
    }
}

static int ulnet__reliable_send(ulnet_session_t *session, int port, unsigned char *packet, size_t size) {
    struct reliable_endpoint_t *endpoint = session->reliable_endpoint[port];
    if (!endpoint) {
        SAM2_LOG_ERROR("Attempt to send reliable packet on uninitialized endpoint");
        return 1;
    }

    // Get next sequence number before sending
    uint16_t sequence = reliable_endpoint_next_packet_sequence(endpoint);

    // Send the packet
    reliable_endpoint_send_packet(endpoint, packet, size);

    int slot_index = sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE;

    session->reliable_pending_header[port][slot_index] = packet[0];

    // Store packet-specific metadata
    if ((packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_INPUT) {
        int64_t frame;
        int header_size = sizeof(ulnet_state_packet_t);
        rle8_decode(&packet[header_size], ULNET_PACKET_SIZE_BYTES_MAX - header_size, (uint8_t *) &frame, sizeof(frame));
        session->reliable_pending_metadata[port][slot_index].channel_state.frame = frame;
    } else if (   (packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_ASCII_1
               || (packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_ASCII_2) {

        // For ASCII messages, allocate in the arena
        arena_reference_t ref = arena_allocate(&session->arena, (uint16_t)size);
        void *ptr = arena_dereference(&session->arena, ref);
        memcpy(ptr, packet, size);
        session->reliable_pending_metadata[port][slot_index].channel_ascii.reference = ref;
    } else if ((packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_SPECTATOR_INPUT) {
        session->reliable_pending_metadata[port][slot_index].channel_state.frame = session->frame_counter;
    } else {
        SAM2_LOG_ERROR("Tried to send a reliable message with invalid channel: %d", (packet[0] & ULNET_CHANNEL_MASK));
        return 1;
    }

    return 0;
}

void ulnet_message_send(ulnet_session_t *session, int port, unsigned char *packet) {
    if (   (packet[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_ASCII_1
        && (packet[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_ASCII_2) {
        SAM2_LOG_FATAL("Attempt to send non-ASCII message with ulnet_message_send");
    }

    ulnet__reliable_send(session, port, packet, sam2_get_metadata((const char *) packet)->message_size);
}

static uint64_t ulnet__process_reliable_acks(uint16_t acks[/* num_acks */], int num_acks, uint16_t *greatest_sequence) {
    uint64_t reliable_unacked_bitfield = 0;

    for (int i = 0; i < num_acks; i++) {
        SAM2_LOG_INFO("Received reliable ack: %d", acks[i]);
        uint16_t ack_sequence = acks[i];

        uint16_t sequence_difference = (*greatest_sequence - ack_sequence) % 65536; // This handles wrap-around correctly

        // If this is a new highest sequence
        if (reliable_sequence_greater_than(ack_sequence, *greatest_sequence)) {
            // Calculate how many new sequence numbers this represents
            uint16_t history_to_shift = (ack_sequence - *greatest_sequence) % 65536;

            uint64_t overwritten_unacked_bits;
            if (history_to_shift >= 64) {
                // If the shift is larger than our bitfield
                overwritten_unacked_bits = reliable_unacked_bitfield;
                reliable_unacked_bitfield = 0;
            } else {
                // If the shift is within our bitfield
                overwritten_unacked_bits = reliable_unacked_bitfield & ~(UINT64_MAX >> history_to_shift);

                // Shift the bitfield left to make room for new sequences
                reliable_unacked_bitfield <<= history_to_shift;

                // Mark all new sequence numbers as unacked, except the one we just acked
                // For example, if history_to_shift is 5, and we just acked sequence 10,
                // we want to mark sequences 6, 7, 8, and 9 as unacked (bits 0-3)
                if (history_to_shift > 1) {
                    int num_bits_to_set = (history_to_shift-1);
                    reliable_unacked_bitfield |= ((1ULL << num_bits_to_set) - 1);
                }
            }

            int lost_count = ULNET__POPCNT(overwritten_unacked_bits);
            if (lost_count > 0) {
                SAM2_LOG_WARN("Lost ability to retransmit %d packets", lost_count);
            }

            *greatest_sequence = ack_sequence;
        } else if (sequence_difference < 64) {
            // Clear the bit for this sequence (mark as acked)
            reliable_unacked_bitfield &= ~(1ULL << sequence_difference);
        } else {
            SAM2_LOG_INFO("Received ack for sequence %d that is too old", ack_sequence);
        }
    }

    return reliable_unacked_bitfield;
}

static void ulnet__check_retransmissions(ulnet_session_t *session, double current_time_seconds) {
    // Skip if not time to retransmit yet
    if (current_time_seconds < session->reliable_next_retransmit_time) {
        return;
    } else {
        // Update next retransmit time
        session->reliable_next_retransmit_time = current_time_seconds + (ULNET_RELIABLE_RETRANSMIT_INTERVAL_MS / 1000.0);
    }

    for (int port = 0; port < SAM2_TOTAL_PEERS; port++) {
        struct reliable_endpoint_t *endpoint = session->reliable_endpoint[port];
        if (!endpoint) {
            continue;
        }

        // First process any acks
        int num_acks;
        uint16_t *acks = reliable_endpoint_get_acks(endpoint, &num_acks);
        uint64_t reliable_unacked_bitfield = ulnet__process_reliable_acks(acks, num_acks, &session->reliable_greatest_sequence[port]);
        reliable_endpoint_clear_acks(endpoint);

        // Process each unacked packet (each 1 bit in the bitfield)
        //uint64_t bitfield = 0;
        while (reliable_unacked_bitfield) {
            // Find the position of the next set bit
            int bit_pos = ULNET__CTZ(reliable_unacked_bitfield);

            // Clear this bit so we don't process it again in this loop
            uint64_t bit_mask = (1ULL << bit_pos);
            reliable_unacked_bitfield &= ~bit_mask;

            // Calculate the sequence number for this bit position
            // The bit at position 0 represents sequence (greatest_sequence - 1)
            // The bit at position 1 represents sequence (greatest_sequence - 2), etc.
            uint16_t sequence = (session->reliable_greatest_sequence[port] - 1 - bit_pos) % 65536;

            int slot_index = sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE;

            uint8_t channel_and_flags = session->reliable_pending_header[port][slot_index];

            if ((channel_and_flags & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_INPUT) {
                // For input packets, look up the original packet from history
                int64_t frame = session->reliable_pending_metadata[port][slot_index].channel_state.frame;
                int frame_history_idx = (frame / ULNET_DELAY_BUFFER_SIZE) % ULNET_STATE_PACKET_HISTORY_SIZE;
                int sender_port = channel_and_flags & ULNET_FLAGS_MASK;

                if (sender_port < SAM2_ARRAY_LENGTH(session->state_packet_history)) {
                    unsigned char *packet = session->state_packet_history[sender_port][frame_history_idx];
                    if (packet[0] != 0) {  // Make sure it's a valid packet
                        int packet_size = 0;
                        // Find packet size (first zero byte pair)
                        for (packet_size = 0; packet_size < ULNET_PACKET_SIZE_BYTES_MAX - 1; packet_size++) {
                            if (packet[packet_size] == 0 && packet[packet_size+1] == 0) {
                                break;
                            }
                        }

                        if (packet_size > 0) {
                            SAM2_LOG_INFO("Retransmitting input packet for frame %" PRId64 ", sequence %d", frame, sequence);

                            // When we retransmit, we don't use the next sequence number - we use the original
                            // sequence number so it will be properly acked
                            ulnet__reliable_send(session, port, packet, packet_size);
                        } else {
                            SAM2_LOG_WARN("Invalid packet size for input packet frame %" PRId64, frame);
                        }
                    } else {
                        SAM2_LOG_WARN("Missing history for input packet frame %" PRId64, frame);
                    }
                } else {
                    SAM2_LOG_WARN("Invalid sender port %d for input packet", sender_port);
                }
            } else if ((channel_and_flags & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_SPECTATOR_INPUT) {
                uint8_t packet[ULNET_PACKET_SIZE_BYTES_MAX] = { ULNET_CHANNEL_SPECTATOR_INPUT };
                size_t packet_size = sizeof(ulnet_state_packet_t) + rle8_encode_capped(
                    (uint8_t *) &session->spectator_suggested_input_state[63],
                    sizeof(session->spectator_suggested_input_state[63]),
                    &packet[sizeof(ulnet_state_packet_t)],
                    sizeof(packet) - sizeof(ulnet_state_packet_t)
                );

                ulnet__reliable_send(session, port, packet, packet_size);
            } else if (   (channel_and_flags & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_ASCII_1
                       || (channel_and_flags & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_ASCII_2) {

                arena_reference_t ref = session->reliable_pending_metadata[port][slot_index].channel_ascii.reference;
                void *ptr = arena_dereference(&session->arena, ref);

                if (ptr) {
                    SAM2_LOG_INFO("Retransmitting ASCII message, sequence %d", sequence);
                    ulnet__reliable_send(session, port, (unsigned char*)ptr, ref.size);
                } else {
                    SAM2_LOG_WARN("Unable to resend packet arena reference overwritten: size=%d, generation=%d, offset=%d",
                                 ref.size, ref.generation, ref.offset);
                }
            } else {
                SAM2_LOG_WARN("Unknown channel for retransmission: %d", (channel_and_flags & ULNET_CHANNEL_MASK));
            }
        }
    }
}

static void ulnet_update_state_history(ulnet_session_t *session, uint8_t *packet, size_t size) {
    // Only store every 8th packet... frame 7, 15, 23, etc.
    int port = packet[0] & ULNET_FLAGS_MASK;
    int64_t frame;
    rle8_decode(&packet[1], ULNET_PACKET_SIZE_BYTES_MAX - 1, (uint8_t *) &frame, sizeof(frame));
    if ((frame + 1) % ULNET_DELAY_BUFFER_SIZE == 0) {
        int history_idx = (frame / ULNET_DELAY_BUFFER_SIZE) % ULNET_STATE_PACKET_HISTORY_SIZE;

        int i = 0;
        for (; i < size; i++) {
            session->state_packet_history[port][history_idx][i] = ((unsigned char *)packet)[i];
        }
        for (; i < SAM2_ARRAY_LENGTH(session->state_packet_history[0][0]); i++) {
            session->state_packet_history[port][history_idx][i] = 0;
        }
    }
}

static void ulnet__send_state_packet(ulnet_session_t *session, int port, uint8_t *packet, size_t size) {
    int64_t frame;

    if ((packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_INPUT) {
        rle8_decode(&packet[1], ULNET_PACKET_SIZE_BYTES_MAX, (uint8_t *) &frame, sizeof(frame));
    } else {
        // Setting this is a bit hacky and meaningless the end result is we send spectator input reliable packets
        // at around the same cadence as normal ones which we need to do that to send ack bits even though
        // spectator input doesn't need to be sent reliably
        frame = session->frame_counter;
    }

    uint16_t next_packet_sequence = reliable_endpoint_next_packet_sequence(session->reliable_endpoint[port]);
    uint16_t last_packet_sequence = (next_packet_sequence - 1) % ULNET_RELIABLE_ACK_BUFFER_SIZE;
    // Only send every 8th packet reliably... frame 7, 15, 23, etc.
    if (   (frame + 1) % ULNET_DELAY_BUFFER_SIZE == 0
        && session->reliable_pending_metadata[port][last_packet_sequence].channel_state.frame != frame) {
            ulnet__reliable_send(session, port, (unsigned char *) packet, size);
    } else {
        int result = ulnet__udp_send(session, port, (char *) packet, size);
        if (result < 0) {
            SAM2_LOG_WARN("Failed to send packet: %d", result);
        }
    }
}

#define ULNET_POLL_SESSION_SAVED_STATE 0b00000001
#define ULNET_POLL_SESSION_TICKED      0b00000010
// This procedure always sends an input packet if the core is ready to tick. This subsumes retransmission logic and generally makes protocol logic less strict
ULNET_LINKAGE int ulnet_poll_session(ulnet_session_t *session, bool force_save_state_on_tick, uint8_t *save_state, size_t save_state_size,
    double frame_rate, double max_sleeping_allowed_when_polling_network_seconds,
    void (*retro_run)(void), bool (*retro_serialize)(void *, size_t), bool (*retro_unserialize)(const void *, size_t)) {
    IMH(ImGui::Begin("P2P UDP Netplay", NULL, ImGuiWindowFlags_AlwaysAutoResize);)
    int status = 0;

    session->retro_unserialize = retro_unserialize; // If used this is invoked through a callback within this function call
    if (session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
        uint8_t packet[ULNET_PACKET_SIZE_BYTES_MAX];
        int64_t packet_size;

        if (ulnet_is_spectator(session, session->our_peer_id)) {
            packet[0] = ULNET_CHANNEL_SPECTATOR_INPUT;
            packet_size = sizeof(ulnet_state_packet_t) + rle8_encode_capped(
                (uint8_t *) &session->spectator_suggested_input_state[63],
                sizeof(session->spectator_suggested_input_state[63]),
                &packet[1],
                sizeof(packet) - 1
            );
        } else {
            packet[0] = ULNET_CHANNEL_INPUT | ulnet_our_port(session);
            packet_size = sizeof(ulnet_state_packet_t) + rle8_encode_capped(
                (uint8_t *) &session->state[ulnet_our_port(session)],
                sizeof(session->state[0]),
                &packet[1],
                sizeof(packet) - 1
            );

            ulnet_update_state_history(session, packet, packet_size);
        }

        if (packet_size > ULNET_PACKET_SIZE_BYTES_MAX) {
            SAM2_LOG_FATAL("Input packet too large to send");
        }

        for (int p = 0; p < SAM2_ARRAY_LENGTH(session->agent); p++) {
            if (!session->agent[p]) continue;
            juice_state_t state = juice_get_state(session->agent[p]);

            // Wait until we can send netplay messages to everyone without fail
            if (state == JUICE_STATE_CONNECTED || state == JUICE_STATE_COMPLETED) {
                ulnet__send_state_packet(session, p, packet, packet_size);

                if ((packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_INPUT) {
                    SAM2_LOG_DEBUG("Sent input packet for frame %" PRId64 " dest peer_ids[%d]=%05" PRId16,
                        session->state[ulnet_our_port(session)].frame, p, session->room_we_are_in.peer_ids[p]);
                } else {
                    SAM2_LOG_DEBUG("Sent spectator input packet dest peer_ids[%d]=%05" PRId16, p, session->room_we_are_in.peer_ids[p]);
                }
            }
        }
    }

#if defined(ULNET_IMGUI)
    { // Plot Input Packet Size vs. Frame
        // @todo The gaps in the graph can be explained by out-of-order arrival of packets I think I don't even record those to history but I should
        //       There is some other weird behavior that might be related to not checking the frame field in the packet if its too old it shouldn't be in the plot obviously
        ImPlot::SetNextAxisLimits(ImAxis_X1, session->frame_counter - session->sample_size, session->frame_counter, ImGuiCond_Always);
        ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, 512, ImGuiCond_Always);
        if (   session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED
            && ImPlot::BeginPlot("State-Packet Size vs. Frame")) {
            ImPlot::SetupAxis(ImAxis_X1, "ulnet_state_t::frame");
            ImPlot::SetupAxis(ImAxis_Y1, "Size Bytes");
            for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
                if (session->room_we_are_in.peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;

                uint8_t *peer_packet = session->state_packet_history[p][session->frame_counter % ULNET_STATE_PACKET_HISTORY_SIZE];
                int packet_size_bytes = 0;
                uint16_t u16_0 = 0;
                for (; packet_size_bytes < ULNET_PACKET_SIZE_BYTES_MAX; packet_size_bytes++) {
                    if (memcmp(peer_packet + packet_size_bytes, &u16_0, sizeof(u16_0)) == 0) break;
                }
                session->input_packet_size[p][session->frame_counter % session->sample_size] = packet_size_bytes;

                char label[32];
                if (p == SAM2_AUTHORITY_INDEX) {
                    strcpy(label, "Authority");
                } else {
                    sprintf(label, "Port %d", p);
                }

                int xs[ULNET_MAX_SAMPLE_SIZE];
                int ys[ULNET_MAX_SAMPLE_SIZE];
                for (int frame = SAM2_MAX(0, session->frame_counter - session->sample_size + 1), j = 0; j < session->sample_size; frame++, j++) {
                    xs[j] = frame;
                    ys[j] = session->input_packet_size[p][frame % session->sample_size];
                }

                ImPlot::PlotLine(label, xs, ys, session->sample_size);
            }

            ImPlot::EndPlot();
        }
    }
#endif

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
    double current_time_seconds = get_unix_time_microseconds() / 1e6;
    for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
        if (session->reliable_endpoint[i]) {
            reliable_endpoint_update(session->reliable_endpoint[i], current_time_seconds);
        }
    }

    ulnet__check_retransmissions(session, current_time_seconds);

#if defined(ULNET_IMGUI)
    ImGui::SliderFloat("UDP Induced Receive Drop Rate", &session->debug_udp_recv_drop_rate, 0.0f, 1.0f);
    ImGui::SliderFloat("UDP Induced Transmit Drop Rate", &session->debug_udp_send_drop_rate, 0.0f, 1.0f);
#endif

    // @todo This timing code is messy I should formally model the problem and then create a solution based on that
    bool ignore_frame_pacing_so_we_can_catch_up = false;
    int64_t poll_entry_time_usec = get_unix_time_microseconds();
    {
        int debug_loop_count = 0;
        do {
            if (ulnet_is_spectator(session, session->our_peer_id)) {
                int64_t authority_frame = -1;

                // The number of packets we check here is reasonable, since if we miss ULNET_DELAY_BUFFER_SIZE consecutive packets our connection is irrecoverable anyway
                for (int i = 0; i < ULNET_DELAY_BUFFER_SIZE; i++) {
                    int64_t frame = -1;
                    ulnet_state_packet_t *input_packet = (ulnet_state_packet_t *) session->state_packet_history[SAM2_AUTHORITY_INDEX][(session->frame_counter + i) % ULNET_STATE_PACKET_HISTORY_SIZE];
                    rle8_decode(input_packet->coded_state, ULNET_PACKET_SIZE_BYTES_MAX, (uint8_t *) &frame, sizeof(frame));
                    authority_frame = SAM2_MAX(authority_frame, frame);
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
                 && get_unix_time_microseconds() - poll_entry_time_usec < 1e6 * max_sleeping_allowed_when_polling_network_seconds
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
                ulnet_state_packet_t *maybe_state_packet_for_frame = (ulnet_state_packet_t *) session->state_packet_history[p][history_index_for_frame];

                int64_t frame = -1;
                rle8_decode(maybe_state_packet_for_frame->coded_state, ULNET_PACKET_SIZE_BYTES_MAX, (uint8_t *) &frame, sizeof(frame));

                if (SAM2_ABS(frame - session->frame_counter) < ULNET_DELAY_BUFFER_SIZE) {
                    rle8_decode(
                        maybe_state_packet_for_frame->coded_state, ULNET_PACKET_SIZE_BYTES_MAX - sizeof(maybe_state_packet_for_frame[0]),
                        (uint8_t *) &session->state[p], sizeof(session->state[p])
                    );
                }

                if (session->state[p].frame - session->frame_counter > ULNET_STATE_PACKET_HISTORY_SIZE * ULNET_DELAY_BUFFER_SIZE) {
                    SAM2_LOG_ERROR("We are too far behind to catch up we should resync");
                }
            }
        }
    }

#ifdef ULNET_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    ImGui::SeparatorText("Peer Latency");
    for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
        if (session->agent[i] && session->reliable_endpoint[i]) {
            float rtt = reliable_endpoint_rtt(session->reliable_endpoint[i]);
            ImGui::Text("Peer %05" PRId16 ": %.1f ms", session->room_we_are_in.peer_ids[i], rtt);

            ImGui::SameLine();
            float packet_loss = reliable_endpoint_packet_loss(session->reliable_endpoint[i]);
            ImGui::Text("Loss: %.1f%%", packet_loss);
        }
    }
#endif

IMH(ImGui::SeparatorText("Things We are Waiting on Before we can Tick");)
IMH(if                            (session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) { ImGui::Text("Waiting for savestate"); })
    bool netplay_ready_to_tick = !(session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL);
    if (session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
        for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
            if (session->room_we_are_in.peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;
        IMH(if                      (session->state[p].frame <  session->frame_counter) { ImGui::Text("Input state on port %d is too old", p); })
            netplay_ready_to_tick &= session->state[p].frame >= session->frame_counter;
        IMH(if                      (session->state[p].frame >= session->frame_counter + ULNET_DELAY_BUFFER_SIZE) { ImGui::Text("Input state on port %d is too new (ahead by %" PRId64 " frames)", p, session->state[p].frame - (session->frame_counter + ULNET_DELAY_BUFFER_SIZE)); })
            netplay_ready_to_tick &= session->state[p].frame <  session->frame_counter + ULNET_DELAY_BUFFER_SIZE; // This is needed for spectators only. By protocol it should always true for non-spectators unless we have a bug or someone is misbehaving
        }
    }

    if (!(session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) && !ulnet_is_spectator(session, session->our_peer_id)) {
        int64_t frames_buffered = session->state[ulnet_our_port(session)].frame - session->frame_counter + 1;
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
        int sleep_milliseconds_upper_bound = (get_unix_time_microseconds() - poll_entry_time_usec) / 1000;
        int sleep_milliseconds = SAM2_MIN(3, sleep_milliseconds_upper_bound);
        if (sleep_milliseconds > 0) {
            ulnet__sleep(sleep_milliseconds);
        }
    }

    if (   netplay_ready_to_tick
        && (core_wants_tick_in_seconds(session->core_wants_tick_at_unix_usec) <= 0.0
        || ignore_frame_pacing_so_we_can_catch_up)) {
        status |= ULNET_POLL_SESSION_TICKED;
        // @todo I don't think this makes sense you should keep reasonable timing yourself if you can't the authority should just kick you
        //int64_t authority_is_on_frame = session->state[SAM2_AUTHORITY_INDEX].frame;

        int64_t target_frame_time_usec = 1000000 / frame_rate - 1000; // @todo There is a leftover millisecond bias here for some reason
        int64_t current_time_unix_usec = get_unix_time_microseconds();
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
        int64_t save_state_frame = session->frame_counter;
        if (force_save_state_on_tick || session->peer_needs_sync_bitfield) {
            IMH(uint64_t start = ulnet__rdtsc();)
            retro_serialize(save_state, save_state_size);
            IMH(session->save_state_execution_time_cycles[session->frame_counter % session->sample_size] = ulnet__rdtsc() - start;)
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
            retro_run();
        }

        session->core_wants_tick_at_unix_usec += 1000000 / frame_rate;

        sam2_room_t new_room_state = session->room_we_are_in;
        ulnet__xor_delta(&new_room_state, &session->state[SAM2_AUTHORITY_INDEX].room_xor_delta[session->frame_counter % ULNET_DELAY_BUFFER_SIZE], sizeof(sam2_room_t));

        if (memcmp(&new_room_state, &session->room_we_are_in, sizeof(sam2_room_t)) != 0) {
            SAM2_LOG_INFO("Something about the room we're in was changed by the authority");

            for (int j = 0; j < SAM2_TOTAL_PEERS; j++) {
                for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
                    if (new_room_state.peer_ids[j] == session->agent_peer_ids[i]) {
                        if (new_room_state.peer_ids[j] <= SAM2_PORT_SENTINELS_MAX) continue;
                        ulnet__swap_agent(session, j, i);
                        break;
                    }
                }
            }

            if (sam2_get_port_of_peer(&new_room_state, session->our_peer_id) < SAM2_SPECTATOR_START) {
                for (int p = 0; p < SAM2_PORT_MAX; p++) {
                    if (   new_room_state.peer_ids[p] > SAM2_PORT_SENTINELS_MAX
                        && new_room_state.peer_ids[p] != session->our_peer_id
                        && new_room_state.peer_ids[p] != session->agent_peer_ids[p]) {

                        ulnet_disconnect_peer(session, p);
                        session->agent[p] = NULL;

                        // Convention: The peer with the lesser ID initiates ICE
                        if (session->our_peer_id < new_room_state.peer_ids[p]) {
                            ulnet_startup_ice_for_peer(session, new_room_state.peer_ids[p], p, NULL);
                        }
                    }
                }
            }

            for (int p = 0; p < SAM2_TOTAL_PEERS; p++) {
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

        if (session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
            session->state[ulnet_our_port(session)].save_state_frame = save_state_frame;
            session->state[ulnet_our_port(session)].save_state_hash[save_state_frame % ULNET_DELAY_BUFFER_SIZE] = ZSTD_XXH64(save_state, save_state_size, 0);
            //session->desync_debug_packet.input_state_hash[save_state_frame % ULNET_DELAY_BUFFER_SIZE] = ZSTD_XXH64(g_libretro_context.InputState, sizeof(g_libretro_context.InputState));
        }

        // Ideally I'd place this right after ticking the core, but we need to update the room state first
        session->frame_counter++;
    }

    return status;
}

ULNET_LINKAGE void ulnet__swap_agent(ulnet_session_t *session, int peer_existing_port, int peer_new_port) {
    if (peer_existing_port == peer_new_port) return;
    juice_agent_t *temp_agent = session->agent[peer_existing_port];
    struct reliable_endpoint_t *temp_reliable = session->reliable_endpoint[peer_existing_port];
    int64_t temp_agent_peer_ids = session->agent_peer_ids[peer_existing_port];
    uint16_t temp_reliable_greatest_sequence = session->reliable_greatest_sequence[peer_existing_port];

    session->agent[peer_existing_port] = session->agent[peer_new_port];
    session->reliable_endpoint[peer_existing_port] = session->reliable_endpoint[peer_new_port];
    session->agent_peer_ids[peer_existing_port] = session->agent_peer_ids[peer_new_port];
    session->reliable_greatest_sequence[peer_existing_port] = session->reliable_greatest_sequence[peer_new_port];

    session->agent[peer_new_port] = temp_agent;
    session->reliable_endpoint[peer_new_port] = temp_reliable;
    session->agent_peer_ids[peer_new_port] = temp_agent_peer_ids;
    session->reliable_greatest_sequence[peer_new_port] = temp_reliable_greatest_sequence;

    // Swap reliable_pending_header
    for (int i = 0; i < ULNET_RELIABLE_ACK_BUFFER_SIZE; i++) {
        uint8_t temp_header = session->reliable_pending_header[peer_existing_port][i];
        session->reliable_pending_header[peer_existing_port][i] = session->reliable_pending_header[peer_new_port][i];
        session->reliable_pending_header[peer_new_port][i] = temp_header;

        // Swap reliable_pending_metadata using memcpy for the union
        char temp_metadata[sizeof(session->reliable_pending_metadata[0][0])];

        memcpy(temp_metadata, &session->reliable_pending_metadata[peer_existing_port][i], sizeof(temp_metadata));
        memcpy(&session->reliable_pending_metadata[peer_existing_port][i], &session->reliable_pending_metadata[peer_new_port][i], sizeof(temp_metadata));
        memcpy(&session->reliable_pending_metadata[peer_new_port][i], temp_metadata, sizeof(temp_metadata));
    }
}

static void ulnet_peer_init_defaulted(ulnet_session_t *session, int peer_port) {
    // If this is set to 0 then we implicitly ack the first packet, UINT16_MAX is
    // safe because it acts like -1 which will never be sent nor acked
    session->reliable_greatest_sequence[peer_port] = UINT16_MAX;
    session->agent_peer_ids[peer_port] = SAM2_PORT_AVAILABLE;
}

ULNET_LINKAGE void ulnet_disconnect_peer(ulnet_session_t *session, int peer_port) {
    session->peer_pending_disconnect_bitfield &= ~(1ULL << peer_port);

    if (peer_port > SAM2_AUTHORITY_INDEX) {
        SAM2_LOG_INFO("Disconnecting spectator %05" PRId16, session->room_we_are_in.peer_ids[peer_port]);
    } else {
        SAM2_LOG_INFO("Disconnecting Peer %05" PRId16, session->room_we_are_in.peer_ids[peer_port]);
    }

    assert(session->reliable_endpoint[peer_port] != NULL);
    reliable_endpoint_destroy(session->reliable_endpoint[peer_port]);
    session->reliable_endpoint[peer_port] = NULL;

    assert(session->agent[peer_port] != NULL);
    juice_destroy(session->agent[peer_port]);
    session->agent[peer_port] = NULL;

    ulnet_peer_init_defaulted(session, peer_port);
}

static inline void ulnet__reset_save_state_bookkeeping(ulnet_session_t *session) {
    session->remote_packet_groups = FEC_PACKET_GROUPS_MAX;
    session->remote_savestate_transfer_offset = 0;
    memset(session->fec_index_counter, 0, sizeof(session->fec_index_counter));
}

ULNET_LINKAGE void ulnet_session_tear_down(ulnet_session_t *session) {
    ulnet__udp_send(session, SAM2_AUTHORITY_INDEX, ulnet_exit_header, SAM2_HEADER_SIZE);

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
        assert(session->reliable_endpoint[i] == NULL);

        ulnet_peer_init_defaulted(session, SAM2_AUTHORITY_INDEX);
    }

    memset(&session->state, 0, sizeof(session->state));
    memset(&session->state_packet_history, 0, sizeof(session->state_packet_history));

    memset(&session->arena, 0, sizeof(session->arena));
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

// MARK: UDP Packet Processing
static void ulnet_receive_packet_callback(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
    ulnet_session_t *session = (ulnet_session_t *) user_ptr;

    if (rand() / ((float) RAND_MAX) < session->debug_udp_recv_drop_rate) {
        SAM2_LOG_DEBUG("Intentionally dropped a received UDP packet");
        return;
    }

    if (session->flags & ULNET_SESSION_FLAG_READY_TO_TICK_SET) {
        SAM2_LOG_ERROR("Received a UDP packet while we were ready to tick. Set a breakpoint here to investigate");
    }

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

    uint8_t channel_and_flags = data[0];
    if ((channel_and_flags & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_RELIABLE) {
        int header_size = 1;
        reliable_endpoint_receive_packet(
            session->reliable_endpoint[p],
            (uint8_t*)&data[header_size],
            (int)size-header_size
        ); // Recursively recalls this function with the unwrapped packet
        return;
    }

    if (   p >= SAM2_PORT_MAX+1
        && (data[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_SPECTATOR_INPUT
        && (data[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_ASCII_1
        && (data[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_ASCII_2) {
        SAM2_LOG_WARN("A spectator sent us a UDP packet for unsupported channel %" PRIx8 " for some reason", data[0] & ULNET_CHANNEL_MASK);
        return;
    }

    switch (channel_and_flags & ULNET_CHANNEL_MASK) {
    case ULNET_CHANNEL_EXTRA: {
        assert(!"This is an error currently\n");
        break;
    }
    case ULNET_CHANNEL_ASCII_1:
    case ULNET_CHANNEL_ASCII_2: {
        if (size < SAM2_HEADER_SIZE) {
            SAM2_LOG_ERROR("Message doesn't even have complete header %zu/%zu bytes", size, SAM2_HEADER_SIZE);
            break;
        }

        SAM2_LOG_INFO("Received message with header '%.8s' from peer %05" PRId16 " on channel 0x%" PRIx8 " with %zu bytes", data, session->agent_peer_ids[p], channel_and_flags & ULNET_CHANNEL_MASK, size);

        if (memcmp(data, ulnet_exit_header, SAM2_HEADER_TAG_SIZE) == 0) {
            if (p > SAM2_AUTHORITY_INDEX) {
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
        } else if (memcmp(data, sam2_join_header, SAM2_HEADER_TAG_SIZE) == 0) {
            // @todo This can be much simpler
            sam2_room_join_message_t join_message;
            memcpy(&join_message, data, sizeof(join_message));
            join_message.peer_id = session->agent_peer_ids[p];
            if (ulnet_process_message(session, &join_message) != 0) {
                SAM2_LOG_ERROR("Failed to process join message");
            }
        }

        break;
    }
    case ULNET_CHANNEL_RELIABLE: {
        SAM2_LOG_ERROR("Reliable packets should have been unwrapped already");
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

            ulnet_update_state_history(session, (uint8_t *) input_packet, size);

            // Broadcast the input packet to spectators
            if (ulnet_is_authority(session)) {
                for (int s = SAM2_SPECTATOR_START; s < SAM2_TOTAL_PEERS; s++) {
                    juice_agent_t *spectator_agent = session->agent[s];
                    if (spectator_agent) {
                        if (   juice_get_state(spectator_agent) == JUICE_STATE_CONNECTED
                            || juice_get_state(spectator_agent) == JUICE_STATE_COMPLETED) {
                            int status = ulnet__udp_send(session, s, data, size);
                            assert(status == 0);
                        }
                    }
                }
            }
        }

        // Check for desync
        ulnet_state_t *their_desync_debug_packet = &session->state[original_sender_port];
        ulnet_state_t *our_desync_debug_packet = &session->state[ulnet_our_port(session)];

        int64_t latest_common_frame = SAM2_MIN(our_desync_debug_packet->save_state_frame, their_desync_debug_packet->save_state_frame);
        int64_t frame_difference = SAM2_ABS(our_desync_debug_packet->save_state_frame - their_desync_debug_packet->save_state_frame);
        int64_t total_frames_to_compare = ULNET_DELAY_BUFFER_SIZE - frame_difference;
        for (int f = total_frames_to_compare-1; f >= 0 ; f--) {
            int64_t frame_to_compare = latest_common_frame - f;
            int64_t frame_index = frame_to_compare % ULNET_DELAY_BUFFER_SIZE;

            if (our_desync_debug_packet->input_state_hash[frame_index] != their_desync_debug_packet->input_state_hash[frame_index]) {
                SAM2_LOG_ERROR("Input state hash mismatch for frame %" PRId64 " Our hash: %" PRIx64 " Their hash: %" PRIx64 "",
                    frame_to_compare, our_desync_debug_packet->input_state_hash[frame_index], their_desync_debug_packet->input_state_hash[frame_index]);
            } else if (   our_desync_debug_packet->save_state_hash[frame_index]
                       && their_desync_debug_packet->save_state_hash[frame_index]) {

                if (our_desync_debug_packet->save_state_hash[frame_index] != their_desync_debug_packet->save_state_hash[frame_index]) {
                    if (!session->peer_desynced_frame[p]) {
                        session->peer_desynced_frame[p] = frame_to_compare;
                    }

                    SAM2_LOG_ERROR("Save state hash mismatch for frame %" PRId64 " Our hash: %016" PRIx64 " Their hash: %016" PRIx64 "",
                        frame_to_compare, our_desync_debug_packet->save_state_hash[frame_index], their_desync_debug_packet->save_state_hash[frame_index]);
                } else if (session->peer_desynced_frame[p]) {
                    session->peer_desynced_frame[p] = 0;
                    SAM2_LOG_INFO("Peer resynced frame on frame %" PRId64 "", frame_to_compare);
                }
            }
        }

        break;
    }
    case ULNET_CHANNEL_SPECTATOR_INPUT: {
        rle8_decode(
            (const uint8_t *) &data[1], size - 1,
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

        if (session->agent[SAM2_AUTHORITY_INDEX] != agent) {
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

        if (session->fec_index_counter[sequence_hi] == k) {
            // We already have received enough Reed-Solomon blocks to decode the payload; we can ignore this packet
            break;
        }

        if (sequence_hi >= FEC_PACKET_GROUPS_MAX) {
            SAM2_LOG_WARN("Received savestate transfer packet with sequence_hi >= FEC_PACKET_GROUPS_MAX");
            break;
        }

        uint8_t sequence_lo = savestate_transfer_header.sequence_lo;

        SAM2_LOG_DEBUG("Received savestate packet sequence_hi: %hhu sequence_lo: %hhu", sequence_hi, sequence_lo);

        size_t payload_size = size - sizeof(ulnet_save_state_packet_header_t);
        arena_reference_t ref = arena_allocate(&session->arena, payload_size);
        memcpy(arena_dereference(&session->arena, ref), data + sizeof(ulnet_save_state_packet_header_t), payload_size);

        session->remote_savestate_transfer_offset += size;

        session->packet_reference[sequence_hi][session->fec_index_counter[sequence_hi]] = ref;
        session->fec_index [sequence_hi][session->fec_index_counter[sequence_hi]++] = sequence_lo;

        if (session->fec_index_counter[sequence_hi] == k) {
            SAM2_LOG_DEBUG("Received all the savestate data for packet group: %hhu", sequence_hi);
            void *fec_packet[GF_SIZE - FEC_REDUNDANT_BLOCKS];
            for (int i = 0; i < k; i++) {
                fec_packet[i] = arena_dereference(&session->arena, session->packet_reference[sequence_hi][k]);
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
                uint64_t their_savestate_transfer_payload_xxhash = 0;
                uint64_t   our_savestate_transfer_payload_xxhash = 0;
                unsigned char *save_state_data = NULL;
                savestate_transfer_payload_t *savestate_transfer_payload = (savestate_transfer_payload_t *) malloc(sizeof(savestate_transfer_payload_t) /* Fixed size header */ + k * session->remote_packet_groups * rs_block_size);

                int64_t remote_payload_size = 0;
                for (int i = 0; i < k; i++) {
                    for (int j = 0; j < session->remote_packet_groups; j++) {
                        void *decoded_packet = arena_dereference(&session->arena, session->packet_reference[j][i]);
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
                savestate_transfer_payload->xxhash = 0;
                our_savestate_transfer_payload_xxhash = ZSTD_XXH64(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes, 0);

                if (their_savestate_transfer_payload_xxhash != our_savestate_transfer_payload_xxhash) {
                    SAM2_LOG_ERROR("Savestate transfer payload hash mismatch: %" PRIx64 " != %" PRIx64 "", their_savestate_transfer_payload_xxhash, our_savestate_transfer_payload_xxhash);
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
                        if (!session->retro_unserialize(save_state_data, save_state_size)) {
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
        SAM2_LOG_WARN("Unknown channel: %d", channel_and_flags);
    }
}

static void reliable_transmit_packet(void* ctx, uint64_t id, uint16_t seq, uint8_t* data, int size) {
    ulnet_session_t* session = (ulnet_session_t *) ctx;
    juice_agent_t* agent = (juice_agent_t *) id;

    // @todo Kind of slow
    int port;
    SAM2_LOCATE(session->agent, agent, port);

    if (port == -1) {
        SAM2_LOG_ERROR("Could not find port for agent in reliable_transmit_packet");
        return;
    }

    // Construct reliable packet with header
    struct {
        uint8_t header;
        uint8_t data[ULNET_PACKET_SIZE_BYTES_MAX - 1];
    } packet = { ULNET_CHANNEL_RELIABLE };

    if (size <= sizeof(packet.data)) {
        memcpy(packet.data, data, size);

        int result = ulnet__udp_send(session, port, (const char*)&packet, sizeof(packet.header) + size);
        if (result != 0) {
            SAM2_LOG_ERROR("Failed to transmit reliable packet to peer %d", port);
        }
    } else {
        SAM2_LOG_ERROR("Reliable packet too large to send (size: %d, max: %zu)", size, sizeof(packet.data));
        return;
    }
}

static int reliable_process_packet(void* ctx, uint64_t id, uint16_t seq, uint8_t* data, int size) {
    ulnet_session_t *session = (ulnet_session_t *) ctx;
    juice_agent_t* agent = (juice_agent_t *) id;

    ulnet_receive_packet_callback(agent, (const char*)data, size, session);
    return RELIABLE_OK;
}

ULNET_LINKAGE void ulnet_startup_ice_for_peer(ulnet_session_t *session, uint64_t peer_id, int p, const char *remote_description) {
    SAM2_LOG_INFO("Starting Interactive-Connectivity-Establishment for peer %04" PRIx64, peer_id);

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

    // Initialize reliable endpoint
    struct reliable_config_t reliable_config;
    reliable_default_config(&reliable_config);
    snprintf(reliable_config.name, sizeof(reliable_config.name), "peer_%" PRIu64, peer_id);
    reliable_config.context = session;
    reliable_config.id = (uint64_t) session->agent[p];
    reliable_config.max_packet_size = ULNET_PACKET_SIZE_BYTES_MAX;
    reliable_config.rtt_history_size = ULNET_RELIABLE_ACK_BUFFER_SIZE;
    reliable_config.ack_buffer_size = ULNET_RELIABLE_ACK_BUFFER_SIZE;
    reliable_config.transmit_packet_function = reliable_transmit_packet;
    reliable_config.process_packet_function = reliable_process_packet;

    session->reliable_endpoint[p] = reliable_endpoint_create(&reliable_config,
        get_unix_time_microseconds() / 1e6);
}

sam2_room_t ulnet__infer_future_room_we_are_in(ulnet_session_t *session) {
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

int ulnet_process_message(ulnet_session_t *session, const void *response) {

    if (sam2_get_metadata((char *) response) == NULL) {
        return -1;
    }

    if (memcmp(response, sam2_conn_header, SAM2_HEADER_TAG_SIZE) == 0) {
        sam2_connect_message_t *connect_message = (sam2_connect_message_t *) response;
        SAM2_LOG_INFO("We were assigned the peer id %05" PRId16, (uint16_t) connect_message->peer_id);

        session->our_peer_id = connect_message->peer_id;
        session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session->our_peer_id;
    } else if (memcmp(response, sam2_make_header, SAM2_HEADER_TAG_SIZE) == 0) {
        sam2_room_make_message_t *room_make = (sam2_room_make_message_t *) response;

        if (!(session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)) {
            session->room_we_are_in = room_make->room;
        }
    } else if (memcmp(response, sam2_join_header, SAM2_HEADER_TAG_SIZE) == 0) {
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

                futureer_room_we_are_in.peer_ids[current_port] = SAM2_PORT_AVAILABLE;
                futureer_room_we_are_in.peer_ids[ulnet_locate_spectator(&futureer_room_we_are_in, SAM2_PORT_AVAILABLE)] = room_join->peer_id;
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
    }  else if (memcmp(response, sam2_sign_header, SAM2_HEADER_TAG_SIZE) == 0) {
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
                p = ulnet_locate_spectator(&future_room_we_are_in, SAM2_PORT_AVAILABLE);

                if (p == -1) {
                    SAM2_LOG_WARN("We can't let them in as a spectator there are too many spectators");

                    static sam2_error_message_t error = {
                        SAM2_FAIL_HEADER, 0,
                        "Authority has reached the maximum number of spectators",
                        SAM2_RESPONSE_AUTHORITY_ERROR
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
                    SAM2_RESPONSE_AUTHORITY_ERROR
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
    savestate_transfer_payload->xxhash = ZSTD_XXH64(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes, 0);
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

            int status = ulnet__udp_send(session, port, (char *) &packet, sizeof(ulnet_save_state_packet_header_t) + packet_payload_size_bytes);
            assert(status == 0);
        }
    }

    free(savestate_transfer_payload);
}
#endif
#endif

int ulnet__test_reliable_retransmit() {
    ulnet_session_t session;
    ulnet_session_init_defaulted(&session);

    return 0;
}
