#ifndef ULNET_H
#define ULNET_H

#include "sam2.h"

typedef struct juice_agent juice_agent_t;

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

#ifndef ULNET_LINKAGE
#ifdef __cplusplus
#define ULNET_LINKAGE extern "C"
#else
#define ULNET_LINKAGE extern
#endif
#endif

#ifndef ULNET_DEFLATE_COMPRESS_BOUND
#define ULNET_DEFLATE_COMPRESS_BOUND(src_size) ulnet__stb_deflate_compress_bound(src_size)
#endif

#ifndef ULNET_DEFLATE_COMPRESS
#define ULNET_DEFLATE_COMPRESS(dst, dst_capacity, src, src_size, quality) \
    ulnet__stb_deflate_compress(dst, dst_capacity, src, src_size, quality)
#endif

#ifndef ULNET_DEFLATE_DECOMPRESS
#define ULNET_DEFLATE_DECOMPRESS(dst, dst_capacity, src, src_size) \
    ulnet__stb_deflate_decompress(dst, dst_capacity, src, src_size)
#endif

#define ULNET_DEFLATE_STATUS_ERROR        -1
#define ULNET_DEFLATE_STATUS_OK            0
#define ULNET_DEFLATE_STATUS_NEEDS_INPUT   1
#define ULNET_DEFLATE_STATUS_NEEDS_OUTPUT  2
#define ULNET_DEFLATE_STATUS_DONE          3

#define ULNET_DEFLATE_FLUSH_NONE           0
#define ULNET_DEFLATE_FLUSH_FINISH         1

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

#define ULNET_RELIABLE_FLAG_ACK_ONLY             0b00010000

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
    uint8_t sequence_le[2];
    uint8_t ack_sequence_le[2];
    uint8_t payload[];
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

typedef struct ulnet_deflate_stream {
    uint32_t bitbuf;
    uint32_t adler_s1;
    uint32_t adler_s2;
    int bitcount;
    int quality;
    int phase;
    int header_pos;
    int block_header_pos;
    int close_block_pos;
    int trailer_pos;
    int block_final;
    int block_open;
    uint8_t trailer[4];
} ulnet_deflate_stream_t;


typedef struct ulnet_session {
    int64_t frame_counter;
    int64_t delay_frames;
    int64_t core_wants_tick_at_unix_usec;
    int64_t flags;
    uint16_t our_peer_id;

    sam2_room_t room_we_are_in;

    sam2_room_t next_room_xor_delta;
    ulnet_input_state_t next_input_state[SAM2_PORT_MAX]; // This is the next input state that will be buffered, it is not yet applied to the state buffer
    ulnet_core_option_t next_core_option;

    ulnet_core_option_t core_options[ULNET_CORE_OPTIONS_MAX]; // @todo I don't like this here

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
    ulnet_packet_ref_t state_packet_history[SAM2_TOTAL_PEERS][ULNET_STATE_PACKET_HISTORY_SIZE]; // Indexable by (frame / ULNET_DELAY_BUFFER_SIZE) % ULNET_STATE_PACKET_HISTORY_SIZE
    ulnet_packet_ref_t packet_history[SAM2_TOTAL_PEERS][256]; // All packets circular buffer in order they were sent/recv
    uint8_t packet_history_next[SAM2_TOTAL_PEERS];
    int64_t reliable_retransmit_delay_microseconds;
    int64_t reliable_last_transmit_time[SAM2_TOTAL_PEERS];
    ulnet_packet_ref_t reliable_tx_packet_history[SAM2_TOTAL_PEERS][ULNET_RELIABLE_ACK_BUFFER_SIZE]; // Indexable by sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE
    ulnet_packet_ref_t reliable_rx_packet_history[SAM2_TOTAL_PEERS][ULNET_RELIABLE_ACK_BUFFER_SIZE]; // Indexable by sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE
    uint16_t reliable_tx_next_seq[SAM2_TOTAL_PEERS]; // Greatest sequence we have sent
    uint16_t reliable_tx_head[SAM2_TOTAL_PEERS];     // Greatest sequence we have sent and received an ack for
    uint16_t reliable_rx_head[SAM2_TOTAL_PEERS];     // Next sequence we expect to receive

    // MARK: Save state transfer
    int deflate_quality;
    int64_t remote_savestate_transfer_offset;
    uint8_t remote_packet_groups; // This is used to bookkeep how much data we actually need to receive to reform the complete savestate
    ulnet_packet_ref_t packet_reference[FEC_PACKET_GROUPS_MAX][ULNET_RS_TOTAL_BLOCKS_MAX];
    uint64_t fec_received_bits[FEC_PACKET_GROUPS_MAX][4];
    int fec_index_counter[FEC_PACKET_GROUPS_MAX]; // Counts unique packets received in each packet group

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
ULNET_LINKAGE void ulnet_session_tear_down(ulnet_session_t *session);
ULNET_LINKAGE int ulnet_deflate_stream_init(ulnet_deflate_stream_t *stream, int quality);
ULNET_LINKAGE int ulnet_deflate_stream_update(ulnet_deflate_stream_t *stream, const uint8_t **src, size_t *src_size,
    uint8_t **dst, size_t *dst_size, int flush);
ULNET_LINKAGE void ulnet_deflate_stream_end(ulnet_deflate_stream_t *stream);
ULNET_LINKAGE int64_t ulnet__get_unix_time_microseconds();
ULNET_LINKAGE uint32_t ulnet_xxh32(const void* data, size_t len, uint32_t seed);

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

#define IMH(statement) if (session->flags & ULNET_SESSION_FLAG_DRAW_IMGUI) { statement }
#else
#define IMH(statement) do {} while (0);
#endif

#include "juice/juice.h"
#include <assert.h>
#include <time.h>

static void *ulnet__stb_realloc_sized(void *old_ptr, size_t old_size, size_t new_size) {
    void *new_ptr = ULNET_MALLOC(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }

    if (old_ptr != NULL) {
        size_t copy_size = old_size < new_size ? old_size : new_size;
        memcpy(new_ptr, old_ptr, copy_size);
        ULNET_FREE(old_ptr);
    }

    return new_ptr;
}

static int ulnet__stb_err(const char *reason, const char *detail) {
    (void)reason;
    (void)detail;
    return 0;
}

#define ULNET__STBI_ASSERT(x) assert(x)
#define ULNET__STBI_NOTUSED(x) ((void)(x))
#define ULNET__STBI_REALLOC_SIZED(p, oldsz, newsz) ulnet__stb_realloc_sized(p, oldsz, newsz)
#define ULNET__STBI_FREE(p) ULNET_FREE(p)
#define ULNET__STBI_MALLOC(sz) ULNET_MALLOC(sz)
#define ULNET__STBIW_ASSERT(x) assert(x)
#define ULNET__STBIW_MALLOC(sz) ULNET_MALLOC(sz)
#define ULNET__STBIW_FREE(p) ULNET_FREE(p)
#define ULNET__STBIW_REALLOC_SIZED(p, oldsz, newsz) ulnet__stb_realloc_sized(p, oldsz, newsz)
#define ULNET__STBIW_MEMMOVE(a, b, sz) memmove(a, b, sz)
#define ULNET__STBIW_UCHAR(x) ((unsigned char)((x) & 0xff))

// stretchy buffer; ulnet__stbiw__sbpush() == vector<>::push_back() -- ulnet__stbiw__sbcount() == vector<>::size()
#define ulnet__stbiw__sbraw(a) ((int *) (void *) (a) - 2)
#define ulnet__stbiw__sbm(a)   ulnet__stbiw__sbraw(a)[0]
#define ulnet__stbiw__sbn(a)   ulnet__stbiw__sbraw(a)[1]

#define ulnet__stbiw__sbneedgrow(a,n)  ((a)==0 || ulnet__stbiw__sbn(a)+n >= ulnet__stbiw__sbm(a))
#define ulnet__stbiw__sbmaybegrow(a,n) (ulnet__stbiw__sbneedgrow(a,(n)) ? ulnet__stbiw__sbgrow(a,n) : 0)
#define ulnet__stbiw__sbgrow(a,n)  ulnet__stbiw__sbgrowf((void **) &(a), (n), sizeof(*(a)))

#define ulnet__stbiw__sbpush(a, v)      (ulnet__stbiw__sbmaybegrow(a,1), (a)[ulnet__stbiw__sbn(a)++] = (v))
#define ulnet__stbiw__sbcount(a)        ((a) ? ulnet__stbiw__sbn(a) : 0)
#define ulnet__stbiw__sbfree(a)         ((a) ? ULNET__STBIW_FREE(ulnet__stbiw__sbraw(a)),0 : 0)

static void *ulnet__stbiw__sbgrowf(void **arr, int increment, int itemsize)
{
   int m = *arr ? 2*ulnet__stbiw__sbm(*arr)+increment : increment+1;
   void *p = ULNET__STBIW_REALLOC_SIZED(*arr ? ulnet__stbiw__sbraw(*arr) : 0, *arr ? (ulnet__stbiw__sbm(*arr)*itemsize + sizeof(int)*2) : 0, itemsize * m + sizeof(int)*2);
   ULNET__STBIW_ASSERT(p);
   if (p) {
      if (!*arr) ((int *) p)[1] = 0;
      *arr = (void *) ((int *) p + 2);
      ulnet__stbiw__sbm(*arr) = m;
   }
   return *arr;
}

static unsigned char *ulnet__stbiw__zlib_flushf(unsigned char *data, unsigned int *bitbuffer, int *bitcount)
{
   while (*bitcount >= 8) {
      ulnet__stbiw__sbpush(data, ULNET__STBIW_UCHAR(*bitbuffer));
      *bitbuffer >>= 8;
      *bitcount -= 8;
   }
   return data;
}

static int ulnet__stbiw__zlib_bitrev(int code, int codebits)
{
   int res=0;
   while (codebits--) {
      res = (res << 1) | (code & 1);
      code >>= 1;
   }
   return res;
}

static unsigned int ulnet__stbiw__zlib_countm(unsigned char *a, unsigned char *b, int limit)
{
   int i;
   for (i=0; i < limit && i < 258; ++i)
      if (a[i] != b[i]) break;
   return i;
}

static unsigned int ulnet__stbiw__zhash(unsigned char *data)
{
   uint32_t hash = data[0] + (data[1] << 8) + (data[2] << 16);
   hash ^= hash << 3;
   hash += hash >> 5;
   hash ^= hash << 4;
   hash += hash >> 17;
   hash ^= hash << 25;
   hash += hash >> 6;
   return hash;
}

#define ulnet__stbiw__zlib_flush() (out = ulnet__stbiw__zlib_flushf(out, &bitbuf, &bitcount))
#define ulnet__stbiw__zlib_add(code,codebits) \
      (bitbuf |= (code) << bitcount, bitcount += (codebits), ulnet__stbiw__zlib_flush())
#define ulnet__stbiw__zlib_huffa(b,c)  ulnet__stbiw__zlib_add(ulnet__stbiw__zlib_bitrev(b,c),c)
// default huffman tables
#define ulnet__stbiw__zlib_huff1(n)  ulnet__stbiw__zlib_huffa(0x30 + (n), 8)
#define ulnet__stbiw__zlib_huff2(n)  ulnet__stbiw__zlib_huffa(0x190 + (n)-144, 9)
#define ulnet__stbiw__zlib_huff3(n)  ulnet__stbiw__zlib_huffa(0 + (n)-256,7)
#define ulnet__stbiw__zlib_huff4(n)  ulnet__stbiw__zlib_huffa(0xc0 + (n)-280,8)
#define ulnet__stbiw__zlib_huff(n)  ((n) <= 143 ? ulnet__stbiw__zlib_huff1(n) : (n) <= 255 ? ulnet__stbiw__zlib_huff2(n) : (n) <= 279 ? ulnet__stbiw__zlib_huff3(n) : ulnet__stbiw__zlib_huff4(n))
#define ulnet__stbiw__zlib_huffb(n) ((n) <= 143 ? ulnet__stbiw__zlib_huff1(n) : ulnet__stbiw__zlib_huff2(n))

#define ulnet__stbiw__ZHASH   16384

static unsigned char * ulnet__stbi_zlib_compress(unsigned char *data, int data_len, int *out_len, int quality)
{
   static unsigned short lengthc[] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258, 259 };
   static unsigned char  lengtheb[]= { 0,0,0,0,0,0,0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5,  0 };
   static unsigned short distc[]   = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577, 32768 };
   static unsigned char  disteb[]  = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
   unsigned int bitbuf=0;
   int i,j, bitcount=0;
   unsigned char *out = NULL;
   unsigned char ***hash_table = (unsigned char***) ULNET__STBIW_MALLOC(ulnet__stbiw__ZHASH * sizeof(unsigned char**));
   if (hash_table == NULL)
      return NULL;
   if (quality < 5) quality = 5;

   ulnet__stbiw__sbpush(out, 0x78);   // DEFLATE 32K window
   ulnet__stbiw__sbpush(out, 0x5e);   // FLEVEL = 1
   ulnet__stbiw__zlib_add(1,1);  // BFINAL = 1
   ulnet__stbiw__zlib_add(1,2);  // BTYPE = 1 -- fixed huffman

   for (i=0; i < ulnet__stbiw__ZHASH; ++i)
      hash_table[i] = NULL;

   i=0;
   while (i < data_len-3) {
      // hash next 3 bytes of data to be compressed
      int h = ulnet__stbiw__zhash(data+i)&(ulnet__stbiw__ZHASH-1), best=3;
      unsigned char *bestloc = 0;
      unsigned char **hlist = hash_table[h];
      int n = ulnet__stbiw__sbcount(hlist);
      for (j=0; j < n; ++j) {
         if (hlist[j]-data > i-32768) { // if entry lies within window
            int d = ulnet__stbiw__zlib_countm(hlist[j], data+i, data_len-i);
            if (d >= best) { best=d; bestloc=hlist[j]; }
         }
      }
      // when hash table entry is too long, delete half the entries
      if (hash_table[h] && ulnet__stbiw__sbn(hash_table[h]) == 2*quality) {
         ULNET__STBIW_MEMMOVE(hash_table[h], hash_table[h]+quality, sizeof(hash_table[h][0])*quality);
         ulnet__stbiw__sbn(hash_table[h]) = quality;
      }
      ulnet__stbiw__sbpush(hash_table[h],data+i);

      if (bestloc) {
         // "lazy matching" - check match at *next* byte, and if it's better, do cur byte as literal
         h = ulnet__stbiw__zhash(data+i+1)&(ulnet__stbiw__ZHASH-1);
         hlist = hash_table[h];
         n = ulnet__stbiw__sbcount(hlist);
         for (j=0; j < n; ++j) {
            if (hlist[j]-data > i-32767) {
               int e = ulnet__stbiw__zlib_countm(hlist[j], data+i+1, data_len-i-1);
               if (e > best) { // if next match is better, bail on current match
                  bestloc = NULL;
                  break;
               }
            }
         }
      }

      if (bestloc) {
         int d = (int) (data+i - bestloc); // distance back
         ULNET__STBIW_ASSERT(d <= 32767 && best <= 258);
         for (j=0; best > lengthc[j+1]-1; ++j);
         ulnet__stbiw__zlib_huff(j+257);
         if (lengtheb[j]) ulnet__stbiw__zlib_add(best - lengthc[j], lengtheb[j]);
         for (j=0; d > distc[j+1]-1; ++j);
         ulnet__stbiw__zlib_add(ulnet__stbiw__zlib_bitrev(j,5),5);
         if (disteb[j]) ulnet__stbiw__zlib_add(d - distc[j], disteb[j]);
         i += best;
      } else {
         ulnet__stbiw__zlib_huffb(data[i]);
         ++i;
      }
   }
   // write out final bytes
   for (;i < data_len; ++i)
      ulnet__stbiw__zlib_huffb(data[i]);
   ulnet__stbiw__zlib_huff(256); // end of block
   // pad with 0 bits to byte boundary
   while (bitcount)
      ulnet__stbiw__zlib_add(0,1);

   for (i=0; i < ulnet__stbiw__ZHASH; ++i)
      (void) ulnet__stbiw__sbfree(hash_table[i]);
   ULNET__STBIW_FREE(hash_table);

   // store uncompressed instead if compression was worse
   if (ulnet__stbiw__sbn(out) > data_len + 2 + ((data_len+32766)/32767)*5) {
      ulnet__stbiw__sbn(out) = 2;  // truncate to DEFLATE 32K window and FLEVEL = 1
      for (j = 0; j < data_len;) {
         int blocklen = data_len - j;
         if (blocklen > 32767) blocklen = 32767;
         ulnet__stbiw__sbpush(out, data_len - j == blocklen); // BFINAL = ?, BTYPE = 0 -- no compression
         ulnet__stbiw__sbpush(out, ULNET__STBIW_UCHAR(blocklen)); // LEN
         ulnet__stbiw__sbpush(out, ULNET__STBIW_UCHAR(blocklen >> 8));
         ulnet__stbiw__sbpush(out, ULNET__STBIW_UCHAR(~blocklen)); // NLEN
         ulnet__stbiw__sbpush(out, ULNET__STBIW_UCHAR(~blocklen >> 8));
         memcpy(out+ulnet__stbiw__sbn(out), data+j, blocklen);
         ulnet__stbiw__sbn(out) += blocklen;
         j += blocklen;
      }
   }

   {
      // compute adler32 on input
      unsigned int s1=1, s2=0;
      int blocklen = (int) (data_len % 5552);
      j=0;
      while (j < data_len) {
         for (i=0; i < blocklen; ++i) { s1 += data[j+i]; s2 += s1; }
         s1 %= 65521; s2 %= 65521;
         j += blocklen;
         blocklen = 5552;
      }
      ulnet__stbiw__sbpush(out, ULNET__STBIW_UCHAR(s2 >> 8));
      ulnet__stbiw__sbpush(out, ULNET__STBIW_UCHAR(s2));
      ulnet__stbiw__sbpush(out, ULNET__STBIW_UCHAR(s1 >> 8));
      ulnet__stbiw__sbpush(out, ULNET__STBIW_UCHAR(s1));
   }
   *out_len = ulnet__stbiw__sbn(out);
   // make returned pointer freeable
   ULNET__STBIW_MEMMOVE(ulnet__stbiw__sbraw(out), out, *out_len);
   return (unsigned char *) ulnet__stbiw__sbraw(out);
}


// fast-way is faster to check than jpeg huffman, but slow way is slower
#define ULNET__STBI__ZFAST_BITS  9 // accelerate all cases in default tables
#define ULNET__STBI__ZFAST_MASK  ((1 << ULNET__STBI__ZFAST_BITS) - 1)
#define ULNET__STBI__ZNSYMS 288 // number of symbols in literal/length alphabet

// zlib-style huffman encoding
// (jpegs packs from left, zlib from right, so can't share code)
typedef struct
{
   uint16_t fast[1 << ULNET__STBI__ZFAST_BITS];
   uint16_t firstcode[16];
   int maxcode[17];
   uint16_t firstsymbol[16];
   uint8_t  size[ULNET__STBI__ZNSYMS];
   uint16_t value[ULNET__STBI__ZNSYMS];
} ulnet__stbi__zhuffman;

inline static int ulnet__stbi__bitreverse16(int n)
{
  n = ((n & 0xAAAA) >>  1) | ((n & 0x5555) << 1);
  n = ((n & 0xCCCC) >>  2) | ((n & 0x3333) << 2);
  n = ((n & 0xF0F0) >>  4) | ((n & 0x0F0F) << 4);
  n = ((n & 0xFF00) >>  8) | ((n & 0x00FF) << 8);
  return n;
}

inline static int ulnet__stbi__bit_reverse(int v, int bits)
{
   ULNET__STBI_ASSERT(bits <= 16);
   // to bit reverse n bits, reverse 16 and shift
   // e.g. 11 bits, bit reverse and shift away 5
   return ulnet__stbi__bitreverse16(v) >> (16-bits);
}

static int ulnet__stbi__zbuild_huffman(ulnet__stbi__zhuffman *z, const uint8_t *sizelist, int num)
{
   int i,k=0;
   int code, next_code[16], sizes[17];

   // DEFLATE spec for generating codes
   memset(sizes, 0, sizeof(sizes));
   memset(z->fast, 0, sizeof(z->fast));
   for (i=0; i < num; ++i)
      ++sizes[sizelist[i]];
   sizes[0] = 0;
   for (i=1; i < 16; ++i)
      if (sizes[i] > (1 << i))
         return ulnet__stb_err("bad sizes", "Corrupt PNG");
   code = 0;
   for (i=1; i < 16; ++i) {
      next_code[i] = code;
      z->firstcode[i] = (uint16_t) code;
      z->firstsymbol[i] = (uint16_t) k;
      code = (code + sizes[i]);
      if (sizes[i])
         if (code-1 >= (1 << i)) return ulnet__stb_err("bad codelengths","Corrupt PNG");
      z->maxcode[i] = code << (16-i); // preshift for inner loop
      code <<= 1;
      k += sizes[i];
   }
   z->maxcode[16] = 0x10000; // sentinel
   for (i=0; i < num; ++i) {
      int s = sizelist[i];
      if (s) {
         int c = next_code[s] - z->firstcode[s] + z->firstsymbol[s];
         uint16_t fastv = (uint16_t) ((s << 9) | i);
         z->size [c] = (uint8_t     ) s;
         z->value[c] = (uint16_t) i;
         if (s <= ULNET__STBI__ZFAST_BITS) {
            int j = ulnet__stbi__bit_reverse(next_code[s],s);
            while (j < (1 << ULNET__STBI__ZFAST_BITS)) {
               z->fast[j] = fastv;
               j += (1 << s);
            }
         }
         ++next_code[s];
      }
   }
   return 1;
}

// zlib-from-memory implementation for PNG reading
//    because PNG allows splitting the zlib stream arbitrarily,
//    and it's annoying structurally to have PNG call ZLIB call PNG,
//    we require PNG read all the IDATs and combine them into a single
//    memory buffer

typedef struct
{
   uint8_t *zbuffer, *zbuffer_end;
   int num_bits;
   int hit_zeof_once;
   uint32_t code_buffer;

   char *zout;
   char *zout_start;
   char *zout_end;
   int   z_expandable;

   ulnet__stbi__zhuffman z_length, z_distance;
} ulnet__stbi__zbuf;

inline static int ulnet__stbi__zeof(ulnet__stbi__zbuf *z)
{
   return (z->zbuffer >= z->zbuffer_end);
}

inline static uint8_t ulnet__stbi__zget8(ulnet__stbi__zbuf *z)
{
   return ulnet__stbi__zeof(z) ? 0 : *z->zbuffer++;
}

static void ulnet__stbi__fill_bits(ulnet__stbi__zbuf *z)
{
   do {
      if (z->code_buffer >= (1U << z->num_bits)) {
        z->zbuffer = z->zbuffer_end;  /* treat this as EOF so we fail. */
        return;
      }
      z->code_buffer |= (unsigned int) ulnet__stbi__zget8(z) << z->num_bits;
      z->num_bits += 8;
   } while (z->num_bits <= 24);
}

inline static unsigned int ulnet__stbi__zreceive(ulnet__stbi__zbuf *z, int n)
{
   unsigned int k;
   if (z->num_bits < n) ulnet__stbi__fill_bits(z);
   k = z->code_buffer & ((1 << n) - 1);
   z->code_buffer >>= n;
   z->num_bits -= n;
   return k;
}

static int ulnet__stbi__zhuffman_decode_slowpath(ulnet__stbi__zbuf *a, ulnet__stbi__zhuffman *z)
{
   int b,s,k;
   // not resolved by fast table, so compute it the slow way
   // use jpeg approach, which requires MSbits at top
   k = ulnet__stbi__bit_reverse(a->code_buffer, 16);
   for (s=ULNET__STBI__ZFAST_BITS+1; ; ++s)
      if (k < z->maxcode[s])
         break;
   if (s >= 16) return -1; // invalid code!
   // code size is s, so:
   b = (k >> (16-s)) - z->firstcode[s] + z->firstsymbol[s];
   if (b >= ULNET__STBI__ZNSYMS) return -1; // some data was corrupt somewhere!
   if (z->size[b] != s) return -1;  // was originally an assert, but report failure instead.
   a->code_buffer >>= s;
   a->num_bits -= s;
   return z->value[b];
}

inline static int ulnet__stbi__zhuffman_decode(ulnet__stbi__zbuf *a, ulnet__stbi__zhuffman *z)
{
   int b,s;
   if (a->num_bits < 16) {
      if (ulnet__stbi__zeof(a)) {
         if (!a->hit_zeof_once) {
            // This is the first time we hit eof, insert 16 extra padding btis
            // to allow us to keep going; if we actually consume any of them
            // though, that is invalid data. This is caught later.
            a->hit_zeof_once = 1;
            a->num_bits += 16; // add 16 implicit zero bits
         } else {
            // We already inserted our extra 16 padding bits and are again
            // out, this stream is actually prematurely terminated.
            return -1;
         }
      } else {
         ulnet__stbi__fill_bits(a);
      }
   }
   b = z->fast[a->code_buffer & ULNET__STBI__ZFAST_MASK];
   if (b) {
      s = b >> 9;
      a->code_buffer >>= s;
      a->num_bits -= s;
      return b & 511;
   }
   return ulnet__stbi__zhuffman_decode_slowpath(a, z);
}

static int ulnet__stbi__zexpand(ulnet__stbi__zbuf *z, char *zout, int n)  // need to make room for n bytes
{
   char *q;
   unsigned int cur, limit, old_limit;
   z->zout = zout;
   if (!z->z_expandable) return ulnet__stb_err("output buffer limit","Corrupt PNG");
   cur   = (unsigned int) (z->zout - z->zout_start);
   limit = old_limit = (unsigned) (z->zout_end - z->zout_start);
   if (~0U - cur < (unsigned) n) return ulnet__stb_err("outofmem", "Out of memory");
   while (cur + n > limit) {
      if(limit > ~0U / 2) return ulnet__stb_err("outofmem", "Out of memory");
      limit *= 2;
   }
   q = (char *) ULNET__STBI_REALLOC_SIZED(z->zout_start, old_limit, limit);
   ULNET__STBI_NOTUSED(old_limit);
   if (q == NULL) return ulnet__stb_err("outofmem", "Out of memory");
   z->zout_start = q;
   z->zout       = q + cur;
   z->zout_end   = q + limit;
   return 1;
}

static const int ulnet__stbi__zlength_base[31] = {
   3,4,5,6,7,8,9,10,11,13,
   15,17,19,23,27,31,35,43,51,59,
   67,83,99,115,131,163,195,227,258,0,0 };

static const int ulnet__stbi__zlength_extra[31]=
{ 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,0,0 };

static const int ulnet__stbi__zdist_base[32] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,0,0};

static const int ulnet__stbi__zdist_extra[32] =
{ 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int ulnet__stbi__parse_huffman_block(ulnet__stbi__zbuf *a)
{
   char *zout = a->zout;
   for(;;) {
      int z = ulnet__stbi__zhuffman_decode(a, &a->z_length);
      if (z < 256) {
         if (z < 0) return ulnet__stb_err("bad huffman code","Corrupt PNG"); // error in huffman codes
         if (zout >= a->zout_end) {
            if (!ulnet__stbi__zexpand(a, zout, 1)) return 0;
            zout = a->zout;
         }
         *zout++ = (char) z;
      } else {
         uint8_t *p;
         int len,dist;
         if (z == 256) {
            a->zout = zout;
            if (a->hit_zeof_once && a->num_bits < 16) {
               // The first time we hit zeof, we inserted 16 extra zero bits into our bit
               // buffer so the decoder can just do its speculative decoding. But if we
               // actually consumed any of those bits (which is the case when num_bits < 16),
               // the stream actually read past the end so it is malformed.
               return ulnet__stb_err("unexpected end","Corrupt PNG");
            }
            return 1;
         }
         if (z >= 286) return ulnet__stb_err("bad huffman code","Corrupt PNG"); // per DEFLATE, length codes 286 and 287 must not appear in compressed data
         z -= 257;
         len = ulnet__stbi__zlength_base[z];
         if (ulnet__stbi__zlength_extra[z]) len += ulnet__stbi__zreceive(a, ulnet__stbi__zlength_extra[z]);
         z = ulnet__stbi__zhuffman_decode(a, &a->z_distance);
         if (z < 0 || z >= 30) return ulnet__stb_err("bad huffman code","Corrupt PNG"); // per DEFLATE, distance codes 30 and 31 must not appear in compressed data
         dist = ulnet__stbi__zdist_base[z];
         if (ulnet__stbi__zdist_extra[z]) dist += ulnet__stbi__zreceive(a, ulnet__stbi__zdist_extra[z]);
         if (zout - a->zout_start < dist) return ulnet__stb_err("bad dist","Corrupt PNG");
         if (len > a->zout_end - zout) {
            if (!ulnet__stbi__zexpand(a, zout, len)) return 0;
            zout = a->zout;
         }
         p = (uint8_t *) (zout - dist);
         if (dist == 1) { // run of one byte; common in images.
            uint8_t v = *p;
            if (len) { do *zout++ = v; while (--len); }
         } else {
            if (len) { do *zout++ = *p++; while (--len); }
         }
      }
   }
}

static int ulnet__stbi__compute_huffman_codes(ulnet__stbi__zbuf *a)
{
   static const uint8_t length_dezigzag[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
   ulnet__stbi__zhuffman z_codelength;
   uint8_t lencodes[286+32+137];//padding for maximum single op
   uint8_t codelength_sizes[19];
   int i,n;

   int hlit  = ulnet__stbi__zreceive(a,5) + 257;
   int hdist = ulnet__stbi__zreceive(a,5) + 1;
   int hclen = ulnet__stbi__zreceive(a,4) + 4;
   int ntot  = hlit + hdist;

   memset(codelength_sizes, 0, sizeof(codelength_sizes));
   for (i=0; i < hclen; ++i) {
      int s = ulnet__stbi__zreceive(a,3);
      codelength_sizes[length_dezigzag[i]] = (uint8_t) s;
   }
   if (!ulnet__stbi__zbuild_huffman(&z_codelength, codelength_sizes, 19)) return 0;

   n = 0;
   while (n < ntot) {
      int c = ulnet__stbi__zhuffman_decode(a, &z_codelength);
      if (c < 0 || c >= 19) return ulnet__stb_err("bad codelengths", "Corrupt PNG");
      if (c < 16)
         lencodes[n++] = (uint8_t) c;
      else {
         uint8_t fill = 0;
         if (c == 16) {
            c = ulnet__stbi__zreceive(a,2)+3;
            if (n == 0) return ulnet__stb_err("bad codelengths", "Corrupt PNG");
            fill = lencodes[n-1];
         } else if (c == 17) {
            c = ulnet__stbi__zreceive(a,3)+3;
         } else if (c == 18) {
            c = ulnet__stbi__zreceive(a,7)+11;
         } else {
            return ulnet__stb_err("bad codelengths", "Corrupt PNG");
         }
         if (ntot - n < c) return ulnet__stb_err("bad codelengths", "Corrupt PNG");
         memset(lencodes+n, fill, c);
         n += c;
      }
   }
   if (n != ntot) return ulnet__stb_err("bad codelengths","Corrupt PNG");
   if (!ulnet__stbi__zbuild_huffman(&a->z_length, lencodes, hlit)) return 0;
   if (!ulnet__stbi__zbuild_huffman(&a->z_distance, lencodes+hlit, hdist)) return 0;
   return 1;
}

static int ulnet__stbi__parse_uncompressed_block(ulnet__stbi__zbuf *a)
{
   uint8_t header[4];
   int len,nlen,k;
   if (a->num_bits & 7)
      ulnet__stbi__zreceive(a, a->num_bits & 7); // discard
   // drain the bit-packed data into header
   k = 0;
   while (a->num_bits > 0) {
      header[k++] = (uint8_t) (a->code_buffer & 255); // suppress MSVC run-time check
      a->code_buffer >>= 8;
      a->num_bits -= 8;
   }
   if (a->num_bits < 0) return ulnet__stb_err("zlib corrupt","Corrupt PNG");
   // now fill header the normal way
   while (k < 4)
      header[k++] = ulnet__stbi__zget8(a);
   len  = header[1] * 256 + header[0];
   nlen = header[3] * 256 + header[2];
   if (nlen != (len ^ 0xffff)) return ulnet__stb_err("zlib corrupt","Corrupt PNG");
   if (a->zbuffer + len > a->zbuffer_end) return ulnet__stb_err("read past buffer","Corrupt PNG");
   if (a->zout + len > a->zout_end)
      if (!ulnet__stbi__zexpand(a, a->zout, len)) return 0;
   memcpy(a->zout, a->zbuffer, len);
   a->zbuffer += len;
   a->zout += len;
   return 1;
}

static int ulnet__stbi__parse_zlib_header(ulnet__stbi__zbuf *a)
{
   int cmf   = ulnet__stbi__zget8(a);
   int cm    = cmf & 15;
   /* int cinfo = cmf >> 4; */
   int flg   = ulnet__stbi__zget8(a);
   if (ulnet__stbi__zeof(a)) return ulnet__stb_err("bad zlib header","Corrupt PNG"); // zlib spec
   if ((cmf*256+flg) % 31 != 0) return ulnet__stb_err("bad zlib header","Corrupt PNG"); // zlib spec
   if (flg & 32) return ulnet__stb_err("no preset dict","Corrupt PNG"); // preset dictionary not allowed in png
   if (cm != 8) return ulnet__stb_err("bad compression","Corrupt PNG"); // DEFLATE required for png
   // window = 1 << (8 + cinfo)... but who cares, we fully buffer output
   return 1;
}

static const uint8_t ulnet__stbi__zdefault_length[ULNET__STBI__ZNSYMS] =
{
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8
};
static const uint8_t ulnet__stbi__zdefault_distance[32] =
{
   5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5
};
/*
Init algorithm:
{
   int i;   // use <= to match clearly with spec
   for (i=0; i <= 143; ++i)     ulnet__stbi__zdefault_length[i]   = 8;
   for (   ; i <= 255; ++i)     ulnet__stbi__zdefault_length[i]   = 9;
   for (   ; i <= 279; ++i)     ulnet__stbi__zdefault_length[i]   = 7;
   for (   ; i <= 287; ++i)     ulnet__stbi__zdefault_length[i]   = 8;

   for (i=0; i <=  31; ++i)     ulnet__stbi__zdefault_distance[i] = 5;
}
*/

static int ulnet__stbi__parse_zlib(ulnet__stbi__zbuf *a, int parse_header)
{
   int final, type;
   if (parse_header)
      if (!ulnet__stbi__parse_zlib_header(a)) return 0;
   a->num_bits = 0;
   a->code_buffer = 0;
   a->hit_zeof_once = 0;
   do {
      final = ulnet__stbi__zreceive(a,1);
      type = ulnet__stbi__zreceive(a,2);
      if (type == 0) {
         if (!ulnet__stbi__parse_uncompressed_block(a)) return 0;
      } else if (type == 3) {
         return 0;
      } else {
         if (type == 1) {
            // use fixed code lengths
            if (!ulnet__stbi__zbuild_huffman(&a->z_length  , ulnet__stbi__zdefault_length  , ULNET__STBI__ZNSYMS)) return 0;
            if (!ulnet__stbi__zbuild_huffman(&a->z_distance, ulnet__stbi__zdefault_distance,  32)) return 0;
         } else {
            if (!ulnet__stbi__compute_huffman_codes(a)) return 0;
         }
         if (!ulnet__stbi__parse_huffman_block(a)) return 0;
      }
   } while (!final);
   return 1;
}

static int ulnet__stbi__do_zlib(ulnet__stbi__zbuf *a, char *obuf, int olen, int exp, int parse_header)
{
   a->zout_start = obuf;
   a->zout       = obuf;
   a->zout_end   = obuf + olen;
   a->z_expandable = exp;

   return ulnet__stbi__parse_zlib(a, parse_header);
}

static int ulnet__stbi_zlib_decode_buffer(char *obuffer, int olen, char const *ibuffer, int ilen)
{
   ulnet__stbi__zbuf a;
   a.zbuffer = (uint8_t *) ibuffer;
   a.zbuffer_end = (uint8_t *) ibuffer + ilen;
   if (ulnet__stbi__do_zlib(&a, obuffer, olen, 0, 1))
      return (int) (a.zout - a.zout_start);
   else
      return -1;
}


static size_t ulnet__stb_deflate_compress_bound(size_t src_size) {
    return src_size + (src_size / 16383) + 64;
}

static int64_t ulnet__stb_deflate_compress(void *dst, size_t dst_capacity, const void *src, size_t src_size, int quality) {
    if (src_size > (size_t)INT32_MAX || dst_capacity > (size_t)INT32_MAX) {
        return -1;
    }

    if (src_size == 0) {
        static const uint8_t empty_zlib_stream[] = {0x78, 0x5e, 0x01, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01};
        if (dst_capacity < sizeof(empty_zlib_stream)) {
            return -1;
        }
        memcpy(dst, empty_zlib_stream, sizeof(empty_zlib_stream));
        return sizeof(empty_zlib_stream);
    }

    int compressed_size = 0;
    unsigned char *compressed = ulnet__stbi_zlib_compress((unsigned char *)src, (int)src_size, &compressed_size, quality);
    if (compressed == NULL) {
        return -1;
    }

    if (compressed_size < 0 || (size_t)compressed_size > dst_capacity) {
        ULNET_FREE(compressed);
        return -1;
    }

    memcpy(dst, compressed, (size_t)compressed_size);
    ULNET_FREE(compressed);
    return compressed_size;
}

static int64_t ulnet__stb_deflate_decompress(void *dst, size_t dst_capacity, const void *src, size_t src_size) {
    if (src_size > (size_t)INT32_MAX || dst_capacity > (size_t)INT32_MAX) {
        return -1;
    }

    return ulnet__stbi_zlib_decode_buffer((char *)dst, (int)dst_capacity, (const char *)src, (int)src_size);
}

#define ULNET__DEFLATE_STREAM_PHASE_HEADER       0
#define ULNET__DEFLATE_STREAM_PHASE_READY        1
#define ULNET__DEFLATE_STREAM_PHASE_BLOCK_HEADER 2
#define ULNET__DEFLATE_STREAM_PHASE_LITERALS     3
#define ULNET__DEFLATE_STREAM_PHASE_CLOSE_BLOCK  4
#define ULNET__DEFLATE_STREAM_PHASE_PAD          5
#define ULNET__DEFLATE_STREAM_PHASE_TRAILER      6
#define ULNET__DEFLATE_STREAM_PHASE_DONE         7
#define ULNET__DEFLATE_STREAM_PHASE_ERROR        8

static int ulnet__deflate_stream_put_byte(uint8_t **dst, size_t *dst_size, uint8_t byte) {
    if (*dst_size == 0) {
        return 0;
    }

    **dst = byte;
    (*dst)++;
    (*dst_size)--;
    return 1;
}

static int ulnet__deflate_stream_flush_bits(ulnet_deflate_stream_t *stream, uint8_t **dst, size_t *dst_size) {
    while (stream->bitcount >= 8) {
        if (!ulnet__deflate_stream_put_byte(dst, dst_size, (uint8_t)(stream->bitbuf & 0xff))) {
            return 0;
        }

        stream->bitbuf >>= 8;
        stream->bitcount -= 8;
    }

    return 1;
}

static int ulnet__deflate_stream_add_bits(ulnet_deflate_stream_t *stream, uint8_t **dst, size_t *dst_size, int code, int codebits) {
    stream->bitbuf |= ((uint32_t)code) << stream->bitcount;
    stream->bitcount += codebits;
    return ulnet__deflate_stream_flush_bits(stream, dst, dst_size);
}

static int ulnet__deflate_stream_huffa(ulnet_deflate_stream_t *stream, uint8_t **dst, size_t *dst_size, int code, int codebits) {
    return ulnet__deflate_stream_add_bits(stream, dst, dst_size, ulnet__stbiw__zlib_bitrev(code, codebits), codebits);
}

static int ulnet__deflate_stream_huff(ulnet_deflate_stream_t *stream, uint8_t **dst, size_t *dst_size, int n) {
    if (n <= 143) {
        return ulnet__deflate_stream_huffa(stream, dst, dst_size, 0x30 + n, 8);
    } else if (n <= 255) {
        return ulnet__deflate_stream_huffa(stream, dst, dst_size, 0x190 + n - 144, 9);
    } else if (n <= 279) {
        return ulnet__deflate_stream_huffa(stream, dst, dst_size, n - 256, 7);
    } else {
        return ulnet__deflate_stream_huffa(stream, dst, dst_size, 0xc0 + n - 280, 8);
    }
}

static int ulnet__deflate_stream_huff_literal(ulnet_deflate_stream_t *stream, uint8_t **dst, size_t *dst_size, int n) {
    if (n <= 143) {
        return ulnet__deflate_stream_huffa(stream, dst, dst_size, 0x30 + n, 8);
    } else {
        return ulnet__deflate_stream_huffa(stream, dst, dst_size, 0x190 + n - 144, 9);
    }
}

static void ulnet__deflate_stream_adler_update(ulnet_deflate_stream_t *stream, uint8_t byte) {
    stream->adler_s1 += byte;
    if (stream->adler_s1 >= 65521) {
        stream->adler_s1 -= 65521;
    }

    stream->adler_s2 += stream->adler_s1;
    stream->adler_s2 %= 65521;
}

ULNET_LINKAGE int ulnet_deflate_stream_init(ulnet_deflate_stream_t *stream, int quality) {
    if (stream == NULL) {
        return ULNET_DEFLATE_STATUS_ERROR;
    }

    memset(stream, 0, sizeof(*stream));
    stream->quality = quality < 5 ? 5 : quality;
    stream->adler_s1 = 1;
    stream->adler_s2 = 0;
    stream->phase = ULNET__DEFLATE_STREAM_PHASE_HEADER;
    return ULNET_DEFLATE_STATUS_OK;
}

ULNET_LINKAGE int ulnet_deflate_stream_update(ulnet_deflate_stream_t *stream, const uint8_t **src, size_t *src_size,
    uint8_t **dst, size_t *dst_size, int flush) {
    static const uint8_t zlib_header[] = {0x78, 0x5e};

    if (   stream == NULL
        || src == NULL || src_size == NULL
        || dst == NULL || dst_size == NULL
        || *dst == NULL
        || flush < ULNET_DEFLATE_FLUSH_NONE
        || flush > ULNET_DEFLATE_FLUSH_FINISH) {
        return ULNET_DEFLATE_STATUS_ERROR;
    }

    if (stream->phase == ULNET__DEFLATE_STREAM_PHASE_ERROR) {
        return ULNET_DEFLATE_STATUS_ERROR;
    }

    if (stream->phase == ULNET__DEFLATE_STREAM_PHASE_DONE) {
        return ULNET_DEFLATE_STATUS_DONE;
    }

    for (;;) {
        if (stream->bitcount >= 8) {
            if (!ulnet__deflate_stream_flush_bits(stream, dst, dst_size)) {
                return ULNET_DEFLATE_STATUS_NEEDS_OUTPUT;
            }
        }

        switch (stream->phase) {
        case ULNET__DEFLATE_STREAM_PHASE_HEADER:
            while (stream->header_pos < (int)sizeof(zlib_header)) {
                if (!ulnet__deflate_stream_put_byte(dst, dst_size, zlib_header[stream->header_pos])) {
                    return ULNET_DEFLATE_STATUS_NEEDS_OUTPUT;
                }
                stream->header_pos++;
            }
            stream->phase = ULNET__DEFLATE_STREAM_PHASE_READY;
            break;

        case ULNET__DEFLATE_STREAM_PHASE_READY:
            if (*src_size > 0 || flush == ULNET_DEFLATE_FLUSH_FINISH) {
                stream->block_final = (*src_size > 0 && flush == ULNET_DEFLATE_FLUSH_FINISH) || (flush == ULNET_DEFLATE_FLUSH_FINISH && !stream->block_open);
                stream->phase = ULNET__DEFLATE_STREAM_PHASE_BLOCK_HEADER;
                break;
            }
            return ULNET_DEFLATE_STATUS_NEEDS_INPUT;

        case ULNET__DEFLATE_STREAM_PHASE_BLOCK_HEADER:
            stream->block_open = 1;
            if (stream->block_header_pos == 0) {
                stream->block_header_pos = 1;
                if (!ulnet__deflate_stream_add_bits(stream, dst, dst_size, stream->block_final ? 1 : 0, 1)) {
                    return ULNET_DEFLATE_STATUS_NEEDS_OUTPUT;
                }
            }
            if (stream->block_header_pos == 1) {
                stream->block_header_pos = 2;
                if (!ulnet__deflate_stream_add_bits(stream, dst, dst_size, 1, 2)) {
                    return ULNET_DEFLATE_STATUS_NEEDS_OUTPUT;
                }
            }
            stream->block_header_pos = 0;
            stream->phase = ULNET__DEFLATE_STREAM_PHASE_LITERALS;
            break;

        case ULNET__DEFLATE_STREAM_PHASE_LITERALS:
            while (*src_size > 0) {
                uint8_t byte = **src;
                if (!ulnet__deflate_stream_huff_literal(stream, dst, dst_size, byte)) {
                    (*src)++;
                    (*src_size)--;
                    ulnet__deflate_stream_adler_update(stream, byte);
                    return ULNET_DEFLATE_STATUS_NEEDS_OUTPUT;
                }

                (*src)++;
                (*src_size)--;
                ulnet__deflate_stream_adler_update(stream, byte);
            }

            if (flush == ULNET_DEFLATE_FLUSH_FINISH) {
                stream->phase = ULNET__DEFLATE_STREAM_PHASE_CLOSE_BLOCK;
                break;
            }

            return ULNET_DEFLATE_STATUS_NEEDS_INPUT;

        case ULNET__DEFLATE_STREAM_PHASE_CLOSE_BLOCK:
            if (stream->close_block_pos == 0) {
                stream->close_block_pos = 1;
                if (!ulnet__deflate_stream_huff(stream, dst, dst_size, 256)) {
                    stream->block_open = 0;
                    return ULNET_DEFLATE_STATUS_NEEDS_OUTPUT;
                }
            }

            stream->close_block_pos = 0;
            stream->block_open = 0;
            if (stream->block_final) {
                stream->phase = ULNET__DEFLATE_STREAM_PHASE_PAD;
            } else {
                stream->phase = ULNET__DEFLATE_STREAM_PHASE_READY;
            }
            break;

        case ULNET__DEFLATE_STREAM_PHASE_PAD:
            while (stream->bitcount > 0) {
                if (stream->bitcount >= 8) {
                    if (!ulnet__deflate_stream_flush_bits(stream, dst, dst_size)) {
                        return ULNET_DEFLATE_STATUS_NEEDS_OUTPUT;
                    }
                } else {
                    if (!ulnet__deflate_stream_add_bits(stream, dst, dst_size, 0, 1)) {
                        return ULNET_DEFLATE_STATUS_NEEDS_OUTPUT;
                    }
                }
            }

            stream->trailer[0] = (uint8_t)(stream->adler_s2 >> 8);
            stream->trailer[1] = (uint8_t)(stream->adler_s2);
            stream->trailer[2] = (uint8_t)(stream->adler_s1 >> 8);
            stream->trailer[3] = (uint8_t)(stream->adler_s1);
            stream->phase = ULNET__DEFLATE_STREAM_PHASE_TRAILER;
            break;

        case ULNET__DEFLATE_STREAM_PHASE_TRAILER:
            while (stream->trailer_pos < (int)sizeof(stream->trailer)) {
                if (!ulnet__deflate_stream_put_byte(dst, dst_size, stream->trailer[stream->trailer_pos])) {
                    return ULNET_DEFLATE_STATUS_NEEDS_OUTPUT;
                }
                stream->trailer_pos++;
            }

            stream->phase = ULNET__DEFLATE_STREAM_PHASE_DONE;
            return ULNET_DEFLATE_STATUS_DONE;

        default:
            stream->phase = ULNET__DEFLATE_STREAM_PHASE_ERROR;
            return ULNET_DEFLATE_STATUS_ERROR;
        }
    }
}

ULNET_LINKAGE void ulnet_deflate_stream_end(ulnet_deflate_stream_t *stream) {
    if (stream != NULL) {
        memset(stream, 0, sizeof(*stream));
    }
}

#undef ulnet__stbiw__zlib_flush
#undef ulnet__stbiw__zlib_add
#undef ulnet__stbiw__zlib_huffa
#undef ulnet__stbiw__zlib_huff1
#undef ulnet__stbiw__zlib_huff2
#undef ulnet__stbiw__zlib_huff3
#undef ulnet__stbiw__zlib_huff4
#undef ulnet__stbiw__zlib_huff
#undef ulnet__stbiw__zlib_huffb


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

double core_wants_tick_in_seconds(int64_t core_wants_tick_at_unix_usec) {
    double seconds = (core_wants_tick_at_unix_usec - ulnet__get_unix_time_microseconds()) / 1000000.0;
    return seconds;
}

static void ulnet_update_state_history(ulnet_session_t *session, const uint8_t *packet, size_t packet_size) {
    // Only store every 8th packet... frame 7, 15, 23, etc.
    if ((packet[0] & ULNET_CHANNEL_MASK) != ULNET_CHANNEL_INPUT) {
        SAM2_LOG_ERROR("Attempt to store non-input packet in state history");
    }

    int port = packet[0] & ULNET_FLAGS_MASK;
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

    bool is_reliable_data =    (packet[0] & ULNET_CHANNEL_MASK) == ULNET_CHANNEL_RELIABLE
                            && !(packet[0] & ULNET_RELIABLE_FLAG_ACK_ONLY);
    uint16_t sequence = is_reliable_data ? (((uint16_t)packet[2] << 8) | packet[1]) : 0;

    ulnet_packet_ref_set(&session->packet_history[port][session->packet_history_next[port]++], packet, size, ULNET_PACKET_FLAG_TX);

    if (is_reliable_data) {
        ulnet_packet_ref_set(&session->reliable_tx_packet_history[port][sequence % ULNET_RELIABLE_ACK_BUFFER_SIZE], packet, size, ULNET_PACKET_FLAG_TX);

        if (sequence != session->reliable_tx_head[port]) {
            SAM2_LOG_INFO("Not sending reliable packet since it is not head of queue");
            return 0; // @todo move this to the reliable send function
        } else {
            session->reliable_last_transmit_time[port] = ulnet__get_unix_time_microseconds();
        }
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
    }

    uint8_t packet_to_send[ULNET_PACKET_SIZE_BYTES_MAX];
    memcpy(packet_to_send, packet, packet_size);
    packet = (ulnet_reliable_packet_t *) packet_to_send;

    memcpy(&packet->ack_sequence_le, &session->reliable_rx_head[port], sizeof(packet->ack_sequence_le));

    SAM2_LOG_INFO("%s reliable packet with sequence %d",
        retransmit ? "Retransmitting" : "Sending queued", head_sequence);

    int status = ulnet_udp_send(session, port, packet_to_send, packet_size);
    if (status != 0) {
        SAM2_LOG_ERROR("Failed to send reliable packet with sequence %d", head_sequence);
    } else if (retransmit) {
        session->packet_history[port][(uint8_t)(session->packet_history_next[port] - 1)].flags |= ULNET_PACKET_FLAG_TX_RELIABLE_RETRANSMIT;
    }

    return status;
}

// Simplified reliable send
ULNET_LINKAGE int ulnet_reliable_send(ulnet_session_t *session, int port, const uint8_t *packet, int size) {
    uint8_t tmp[ULNET_PACKET_SIZE_BYTES_MAX];

    tmp[0] = ULNET_CHANNEL_RELIABLE;
    if ((uint16_t)(session->reliable_tx_next_seq[port] - session->reliable_tx_head[port]) >= ULNET_RELIABLE_ACK_BUFFER_SIZE) {
        SAM2_LOG_ERROR("Reliable send queue is full for port %d", port);
        return -1;
    }

    uint16_t sequence = session->reliable_tx_next_seq[port]++;
    uint16_t ack_sequence = session->reliable_rx_head[port];

    int maybe_wrapped_size = ulnet__wrap_packet(packet, size, sequence, ack_sequence, tmp);
    if (maybe_wrapped_size < 0) {
        return maybe_wrapped_size;
    } else {
        return ulnet_udp_send(session, port, tmp, maybe_wrapped_size);
    }
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


static void ulnet__memor(void *dst, const void *src, size_t n) {
    int16_t *d = (int16_t *)dst;
    const int16_t *s = (const int16_t *)src;
    size_t words = n / sizeof(int16_t);

    for (size_t i = 0; i < words; i++) {
        d[i] |= s[i];
    }
}

#define ULNET_POLL_SESSION_SAVED_STATE    0b00000001
#define ULNET_POLL_SESSION_TICKED         0b00000010
#define ULNET_POLL_SESSION_BUFFERED_INPUT 0b00000100
ULNET_LINKAGE int ulnet_poll_session(ulnet_session_t *session, bool force_save_state_on_tick, uint8_t *save_state, size_t save_state_capacity,
    double frame_rate, double max_sleeping_allowed_when_polling_network_seconds) {

    int our_port = sam2_get_port_of_peer(&session->room_we_are_in, session->our_peer_id);

    IMH(ImGui::Begin("P2P UDP Netplay", NULL, ImGuiWindowFlags_AlwaysAutoResize);)
    int status = 0;

    // Poll input with buffering for netplay
    if (our_port == -1) {
        SAM2_LOG_WARN("No port associated for our peer_id=%d, skipping input polling", session->our_peer_id);
    } else if (   our_port < SAM2_SPECTATOR_START
               && session->state[our_port].frame < session->frame_counter + session->delay_frames) {
        status |= ULNET_POLL_SESSION_BUFFERED_INPUT;
        // @todo The preincrement does not make sense to me here, but things have been working
        int64_t next_buffer_index = ++session->state[our_port].frame % ULNET_DELAY_BUFFER_SIZE;

        session->state[our_port].core_option[next_buffer_index] = session->next_core_option;

        //if (ulnet_is_authority(session)) {
            session->state[our_port].room_xor_delta[next_buffer_index] = session->next_room_xor_delta;
            memset(&session->next_room_xor_delta, 0, sizeof(session->next_room_xor_delta));

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
        //}

        ulnet__memor(session->state[our_port].input_state[next_buffer_index], session->next_input_state, sizeof(ulnet_input_state_t[SAM2_PORT_MAX]));
        // Only send every 8th packet reliably... frame 7, 15, 23, etc.
        if ((session->state[our_port].frame + 1) % ULNET_DELAY_BUFFER_SIZE == 0) {
            uint8_t packet[ULNET_PACKET_SIZE_BYTES_MAX];
            int64_t packet_size;
            packet[0] = ULNET_CHANNEL_INPUT | our_port;
            packet_size = sizeof(ulnet_state_packet_t) + rle8_encode_capped(
                (uint8_t *) &session->state[our_port],
                sizeof(session->state[0]),
                &packet[sizeof(ulnet_state_packet_t)],
                sizeof(packet) - sizeof(ulnet_state_packet_t)
            );

            ulnet_update_state_history(session, packet, packet_size);

            for (int p = 0; p < SAM2_PORT_MAX; p++) {
                if (!session->agent[p]) continue;
                if (juice_get_state(session->agent[p]) != JUICE_STATE_COMPLETED) continue;
                ulnet_reliable_send(session, p, packet, packet_size);
            }
        }

    } else if (our_port >= SAM2_SPECTATOR_START) {
        memcpy(session->spectator_suggested_input_state[63], session->next_input_state, sizeof(session->spectator_suggested_input_state[63]));
    }

    if (our_port != -1) {
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
                ulnet_reliable_send_with_acks_only(session, p, packet, packet_size);

                if (our_port < SAM2_SPECTATOR_START) {
                    SAM2_LOG_DEBUG("Sent input packet for frame %" PRId64 " dest peer_ids[%d]=%05" PRId16,
                        session->state[our_port].frame, p, session->room_we_are_in.peer_ids[p]);
                } else {
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
        // Get rid of dead agents first
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

    for (int port = 0; port < SAM2_TOTAL_PEERS; port++) {
        if (!session->agent[port]) continue;
        if (ulnet__get_unix_time_microseconds() < session->reliable_last_transmit_time[port] + session->reliable_retransmit_delay_microseconds) continue;

        ulnet__reliable_send_head(session, port, true);
    }

    // Reconstruct input required for next tick if we're spectating
    if (ulnet_is_spectator(session, session->our_peer_id)) {
        for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
            if (session->room_we_are_in.peer_ids[p] > SAM2_PORT_SENTINELS_MAX) {
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

ULNET_LINKAGE void ulnet_swap_agent(ulnet_session_t *session, int peer_existing_port, int peer_new_port) {
    if (peer_existing_port == peer_new_port) return;

    #define ULNET__SWAP(x, y, T) do { T temp = (x); (x) = (y); (y) = temp; } while(0)
    ULNET__SWAP(session->agent[peer_existing_port], session->agent[peer_new_port], juice_agent_t *);
    ULNET__SWAP(session->reliable_tx_next_seq[peer_existing_port], session->reliable_tx_next_seq[peer_new_port], uint16_t);
    ULNET__SWAP(session->reliable_tx_head[peer_existing_port], session->reliable_tx_head[peer_new_port], uint16_t);
    ULNET__SWAP(session->reliable_rx_head[peer_existing_port], session->reliable_rx_head[peer_new_port], uint16_t);
    ULNET__SWAP(session->agent_peer_ids[peer_existing_port], session->agent_peer_ids[peer_new_port], int64_t);
}

static void ulnet_peer_init_defaulted(ulnet_session_t *session, int peer_port) {
    session->agent_peer_ids       [peer_port] = 0;
    session->reliable_tx_next_seq [peer_port] = 0;
    session->reliable_tx_head[peer_port] = 0;
    session->reliable_rx_head[peer_port] = 0;
}

static void ulnet_clear_peer_packet_history(ulnet_session_t *session, int peer_port) {
    ulnet_packet_ref_clear_many(session->state_packet_history[peer_port], ULNET_STATE_PACKET_HISTORY_SIZE);
    ulnet_packet_ref_clear_many(session->packet_history[peer_port], 256);
    ulnet_packet_ref_clear_many(session->reliable_tx_packet_history[peer_port], ULNET_RELIABLE_ACK_BUFFER_SIZE);
    ulnet_packet_ref_clear_many(session->reliable_rx_packet_history[peer_port], ULNET_RELIABLE_ACK_BUFFER_SIZE);
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

    ulnet_clear_peer_packet_history(session, peer_port);
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
    ulnet_packet_ref_clear_many(&session->packet_reference[0][0], FEC_PACKET_GROUPS_MAX * ULNET_RS_TOTAL_BLOCKS_MAX);
    session->remote_packet_groups = FEC_PACKET_GROUPS_MAX;
    session->remote_savestate_transfer_offset = 0;
    memset(session->fec_received_bits, 0, sizeof(session->fec_received_bits));
    memset(session->fec_index_counter, 0, sizeof(session->fec_index_counter));
}

ULNET_LINKAGE void ulnet_session_tear_down(ulnet_session_t *session) {
    if (session->agent[SAM2_AUTHORITY_INDEX]) {
        ulnet_message_send(session, SAM2_AUTHORITY_INDEX, (const uint8_t *) ulnet_exit_header);
    }

    for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
        if (session->agent[i]) {
            ulnet_disconnect_peer(session, i);
        } else {
            ulnet_clear_peer_packet_history(session, i);
        }
    }
    ulnet__reset_save_state_bookkeeping(session);

    session->room_we_are_in.flags &= ~SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session->our_peer_id;
    session->frame_counter = 0;
    session->state[SAM2_AUTHORITY_INDEX].frame = 0;
}

ULNET_LINKAGE void ulnet_session_init_defaulted(ulnet_session_t *session) {
    for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
        assert(session->agent[i] == NULL);

        ulnet_clear_peer_packet_history(session, i);
        ulnet_peer_init_defaulted(session, i);
    }

    memset(&session->state, 0, sizeof(session->state));
    memset(session->packet_history_next, 0, sizeof(session->packet_history_next));

    session->frame_counter = 0;
    session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = session->our_peer_id;
    session->reliable_retransmit_delay_microseconds = 50000; // 50 milliseconds
    session->deflate_quality = 8;

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

static void ulnet__process_udp_packet(ulnet_session_t *session, int p, const uint8_t *data, size_t size);
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

    ulnet_packet_ref_set(&session->packet_history[p][session->packet_history_next[p]++], packet, size, 0);

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
}

static void ulnet__process_udp_packet(ulnet_session_t *session, int p, const uint8_t *data, size_t size) {
    if (size == 0) {
        SAM2_LOG_WARN("Received a UDP packet with no payload");
        return;
    }

    uint8_t channel_and_flags = data[0];

    switch (channel_and_flags & ULNET_CHANNEL_MASK) {
    case ULNET_CHANNEL_EXTRA: {
        SAM2_LOG_WARN("Received packet for unsupported channel ULNET_CHANNEL_EXTRA");
        break;
    }
    case ULNET_CHANNEL_ASCII: {
        SAM2_LOG_INFO("Received message with header '%.*s' from peer %05" PRIu16 " on channel 0x%" PRIx8 " with %zu bytes",
            (int)SAM2_MIN(size, SAM2_HEADER_SIZE), data, session->agent_peer_ids[p], channel_and_flags & ULNET_CHANNEL_MASK, size);

        if (sam2_header_matches((const char *) data, ulnet_exit_header)) {
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
        } else if (sam2_header_matches((const char *) data, sam2_join_header)) {
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
        if (size < sizeof(ulnet_reliable_packet_t)) {
            SAM2_LOG_WARN("Reliable packet missing header");
            break;
        }

        ulnet_reliable_packet_t *reliable_packet = (ulnet_reliable_packet_t *) data;

        uint16_t old_tx_head = session->reliable_tx_head[p];
        uint16_t ack_sequence = (reliable_packet->ack_sequence_le[1] << 8) | reliable_packet->ack_sequence_le[0];
        if (ulnet__sequence_greater_than(ack_sequence, old_tx_head)) {
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

            ulnet_update_state_history(session, data, size);

            // Broadcast the input packet to spectators
            if (ulnet_is_authority(session)) {
                for (int s = SAM2_SPECTATOR_START; s < SAM2_TOTAL_PEERS; s++) {
                    ulnet_reliable_send_with_acks_only(session, s, (const uint8_t *)data, size);
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
        uint64_t sequence_lo_bit = 1ULL << (sequence_lo & 63);
        uint64_t *sequence_lo_bits = &session->fec_received_bits[sequence_hi][sequence_lo >> 6];
        if ((*sequence_lo_bits & sequence_lo_bit) != 0) {
            break;
        }

        SAM2_LOG_DEBUG("Received savestate packet sequence_hi: %hhu sequence_lo: %hhu", sequence_hi, sequence_lo);

        size_t payload_size = size - sizeof(ulnet_save_state_packet_header_t);
        session->remote_savestate_transfer_offset += size;

        if (ulnet_packet_ref_set(
            &session->packet_reference[sequence_hi][sequence_lo],
            data + sizeof(ulnet_save_state_packet_header_t),
            payload_size,
            0
        ) != 0) {
            SAM2_LOG_ERROR("Failed to store savestate transfer packet");
            break;
        }
        *sequence_lo_bits |= sequence_lo_bit;
        session->fec_index_counter[sequence_hi]++;

        if (session->fec_index_counter[sequence_hi] == k) {
            SAM2_LOG_DEBUG("Received all the savestate data for packet group: %hhu", sequence_hi);
            int rs_block_size = (int) (size - sizeof(ulnet_save_state_packet_header_t));
            int status = ulnet__rs_decode(session->packet_reference[sequence_hi], k, n, rs_block_size);
            if (status != 0) {
                SAM2_LOG_ERROR("Failed to decode savestate transfer packet group");
                ulnet__reset_save_state_bookkeeping(session);
                break;
            }

            bool all_data_decoded = true;
            for (int i = 0; i < session->remote_packet_groups; i++) {
                all_data_decoded &= session->fec_index_counter[i] >= k;
            }

            if (all_data_decoded) {
                int64_t ret = 0;
                uint32_t their_savestate_transfer_payload_xxhash = 0;
                uint32_t   our_savestate_transfer_payload_xxhash = 0;
                unsigned char *save_state_data = NULL;
                savestate_transfer_payload_t *savestate_transfer_payload = (savestate_transfer_payload_t *) ULNET_MALLOC(sizeof(savestate_transfer_payload_t) /* Fixed size header */ + k * session->remote_packet_groups * rs_block_size);

                int32_t remote_payload_size = 0;
                for (int i = 0; i < k; i++) {
                    for (int j = 0; j < session->remote_packet_groups; j++) {
                        void *decoded_packet = session->packet_reference[j][i].data;
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

                ret = ULNET_DEFLATE_DECOMPRESS(
                    session->core_options, sizeof(session->core_options),
                    savestate_transfer_payload->compressed_data + savestate_transfer_payload->compressed_savestate_size,
                    savestate_transfer_payload->compressed_options_size
                );

                if (ret < 0) {
                    SAM2_LOG_ERROR("Error decompressing core options with DEFLATE");
                } else {
                    session->flags |= ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY;
                    //session.retro_run(); // Apply options before loading savestate; Lets hope this isn't necessary

                    save_state_data = (unsigned char *) ULNET_MALLOC(savestate_transfer_payload->decompressed_savestate_size);

                    int64_t save_state_size = ULNET_DEFLATE_DECOMPRESS(
                        save_state_data,
                        savestate_transfer_payload->decompressed_savestate_size,
                        savestate_transfer_payload->compressed_data,
                        savestate_transfer_payload->compressed_savestate_size
                    );

                    if (save_state_size < 0) {
                        SAM2_LOG_ERROR("Error decompressing savestate with DEFLATE");
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
                    ULNET_FREE(save_state_data);
                }

                ULNET_FREE(savestate_transfer_payload);

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

    int64_t save_state_transfer_payload_compressed_bound_size_bytes = (int64_t)ULNET_DEFLATE_COMPRESS_BOUND(save_state_size)
        + (int64_t)ULNET_DEFLATE_COMPRESS_BOUND(sizeof(session->core_options));
    ulnet__logical_partition(sizeof(savestate_transfer_payload_t) /* Header */ + save_state_transfer_payload_compressed_bound_size_bytes,
                      FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size_bytes, &packet_groups);

    size_t savestate_transfer_payload_plus_parity_bound_bytes = packet_groups * n * packet_payload_size_bytes;

    // This points to the savestate transfer payload, but also the remaining bytes at the end hold our parity blocks
    // Having this data in a single contiguous buffer makes indexing easier
    savestate_transfer_payload_t *savestate_transfer_payload = (savestate_transfer_payload_t *) ULNET_MALLOC(savestate_transfer_payload_plus_parity_bound_bytes);

    savestate_transfer_payload->decompressed_savestate_size = save_state_size;
    int64_t compressed_savestate_size = ULNET_DEFLATE_COMPRESS(
        savestate_transfer_payload->compressed_data,
        save_state_transfer_payload_compressed_bound_size_bytes,
        save_state, save_state_size, session->deflate_quality
    );

    if (compressed_savestate_size < 0 || compressed_savestate_size > INT32_MAX) {
        SAM2_LOG_ERROR("DEFLATE compression failed for savestate");
        assert(0);
    }
    savestate_transfer_payload->compressed_savestate_size = (int32_t)compressed_savestate_size;

    int64_t compressed_options_size = ULNET_DEFLATE_COMPRESS(
        savestate_transfer_payload->compressed_data + savestate_transfer_payload->compressed_savestate_size,
        save_state_transfer_payload_compressed_bound_size_bytes - savestate_transfer_payload->compressed_savestate_size,
        session->core_options, sizeof(session->core_options), session->deflate_quality
    );

    if (compressed_options_size < 0 || compressed_options_size > INT32_MAX) {
        SAM2_LOG_ERROR("DEFLATE compression failed for core options");
        assert(0);
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

    savestate_transfer_payload->xxhash = 0;
    savestate_transfer_payload->xxhash = ulnet_xxh32(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes, 0);
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

            int status = ulnet_udp_send(session, port, (const uint8_t *) &packet, sizeof(ulnet_save_state_packet_header_t) + packet_payload_size_bytes);
            assert(status == 0);
        }
    }

    ULNET_FREE(savestate_transfer_payload);
}

#if defined(ULNET_IMGUI)
void ulnet_imgui_show_room(const sam2_room_t& room, int our_peer_id = -1) {
    const ImVec4 WHITE(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 GOLD(1.0f, 0.843f, 0.0f, 1.0f);
    ImGui::Text("Room: %s", room.name);
    ImGui::Text("Flags: %016" PRIx64, room.flags);
    ImGui::Text("Core: %s", room.core_and_version);
    ImGui::Text("ROM Hash: %016" PRIx64, room.rom_hash_xxh64);

    for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
        if (p == SAM2_AUTHORITY_INDEX) {
            ImGui::Text("Authority Peer ID: ");
        } else {
            ImGui::Text("Port %d Peer ID: ", p);
        }

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

    if (ImGui::BeginTable("PacketHistory", SAM2_PORT_MAX + 1,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
                          ImVec2(0.0f, 300.0f))) {

        // Header row
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        for (int port = 0; port < SAM2_PORT_MAX + 1; port++) {
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

            for (int port = 0; port < SAM2_PORT_MAX + 1; port++) {
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

            for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
                if (session->room_we_are_in.peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;

                ulnet_packet_ref_t ref = session->state_packet_history[p][session->frame_counter % ULNET_STATE_PACKET_HISTORY_SIZE];
                uint8_t *peer_packet = ref.data;
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
                    ImGui::Text("Transmit: Next Seq=%u, Greatest Acked=%u", session->reliable_tx_next_seq[p], session->reliable_tx_head[p]);
                    ImGui::Text("Receive: Greatest Seq=%u", session->reliable_rx_head[p]);
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

        uint8_t *payload_start = ((ulnet_reliable_packet_t *)packet_data)->payload;
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
        size_t payload_size = packet_size - sizeof(ulnet_reliable_packet_t);
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
