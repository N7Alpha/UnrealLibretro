// Signaling Server and a Match Maker
#ifndef SAM2_H
#define SAM2_H
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#define SAM2__STR(s) _SAM2__STR(s)
#define _SAM2__STR(s) #s

#define SAM2_VERSION_MAJOR 1
#define SAM2_VERSION_MINOR 0

#define SAM2_HEADER_TAG_SIZE 4
#define SAM2_HEADER_SIZE 8

// MSVC likes to complain (C2117) so we have to define a C-String version (lower-case) of this macro for list initializers and a char array literal version (ALL-CAPS)
#define sam2_make_header  "M" "A" "K" "E" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"
#define SAM2_MAKE_HEADER {'M','A','K','E',    '0' + SAM2_VERSION_MAJOR, '.',    '0' + SAM2_VERSION_MINOR, 'r'}
#define sam2_list_header  "L" "I" "S" "T" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"
#define SAM2_LIST_HEADER {'L','I','S','T',    '0' + SAM2_VERSION_MAJOR, '.',    '0' + SAM2_VERSION_MINOR, 'r'}
#define sam2_join_header  "J" "O" "I" "N" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"
#define SAM2_JOIN_HEADER {'J','O','I','N',    '0' + SAM2_VERSION_MAJOR, '.',    '0' + SAM2_VERSION_MINOR, 'r'}
#define sam2_conn_header  "C" "O" "N" "N" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"
#define SAM2_CONN_HEADER {'C','O','N','N',    '0' + SAM2_VERSION_MAJOR, '.',    '0' + SAM2_VERSION_MINOR, 'r'}
#define sam2_sign_header  "S" "I" "G" "N" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"
#define SAM2_SIGN_HEADER {'S','I','G','N',    '0' + SAM2_VERSION_MAJOR, '.',    '0' + SAM2_VERSION_MINOR, 'r'}
#define sam2_fail_header  "F" "A" "I" "L" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"
#define SAM2_FAIL_HEADER {'F','A','I','L',    '0' + SAM2_VERSION_MAJOR, '.',    '0' + SAM2_VERSION_MINOR, 'r'}

#ifndef SAM2_LINKAGE
#ifdef __cplusplus
#define SAM2_LINKAGE extern "C"
#else
#define SAM2_LINKAGE extern
#endif
#endif

#if defined(__cplusplus) && (__cplusplus >= 201103L)
#define SAM2_STATIC_ASSERT(cond, message) static_assert(cond, message)
#elif defined(_MSVC_LANG) && (_MSVC_LANG >= 201103L)
#define SAM2_STATIC_ASSERT(cond, message) static_assert(cond, message)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define SAM2_STATIC_ASSERT(cond, message) _Static_assert(cond, message)
#else
#define SAM2_STATIC_ASSERT(cond, _) extern int sam2__static_assertion_##__COUNTER__[(cond) ? 1 : -1]
#endif

#if defined(_MSC_VER)
    // Microsoft Visual C++ (MSVC)
    #define SAM2_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    // GCC or Clang
    #define SAM2_FORCEINLINE __attribute__((always_inline)) inline
#elif defined(__INTEL_COMPILER)
    // Intel C++ Compiler (ICC)
    #define SAM2_FORCEINLINE __forceinline
#else
    // fallback to regular inline
    #define SAM2_FORCEINLINE inline
#endif

#define SAM2_ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SAM2_MAX(a,b) ((a) < (b) ? (b) : (a))

#define SAM2_MIN(a, b) ((a) < (b) ? (a) : (b))

#define SAM2_ABS(a) ((a) < 0 ? -(a) : (a))

#define SAM2_LOCATE(fixed_size_array, element, out_index) do { \
    out_index = -1; \
    for (int i = 0; i < SAM2_ARRAY_LENGTH(fixed_size_array); i++) { \
        if ((fixed_size_array)[i] == (element)) { \
            (out_index) = i; \
            break; \
        } \
    } \
} while (0)

#define SAM2_SERVER_DEFAULT_PORT 9218
#define SAM2_DEFAULT_BACKLOG 128

// @todo move some of these into the UDP netcode file
#define SAM2_FLAG_ROOM_IS_NETWORK_HOSTED   0b01000000ULL

#define SAM2_FLAG_PORT0_CAN_SET_ALL_INPUTS (0b00000001ULL << 8)
//#define SAM2_FLAG_PORT1_CAN_SET_ALL_INPUTS (0b00000010ULL << 8)
// etc...

#define SAM2_FLAG_PORT0_PEER_IS_INACTIVE (0b00000001ULL << 16)
//#define SAM2_FLAG_PORT1_PEER_IS_INACTIVE (0b00000010ULL << 16)
// etc...

#define SAM2_FLAG_AUTHORITY_IS_INACTIVE (0b00000001ULL << 24)

#define SAM2_FLAG_SERVER_PERMISSION_MASK (SAM2_FLAG_AUTHORITY_IPv6)
#define SAM2_FLAG_AUTHORITY_PERMISSION_MASK (SAM2_FLAG_NO_FIXED_PORT | SAM2_FLAG_ALLOW_SHOW_IP)
#define SAM2_FLAG_CLIENT_PERMISSION_MASK (SAM2_FLAG_SPECTATOR)

#define SAM2_RESPONSE_SUCCESS                  0
#define SAM2_RESPONSE_SERVER_ERROR             -1  // Emitted by signaling server when there was an internal error
#define SAM2_RESPONSE_AUTHORITY_ERROR          -2  // Emitted by authority when there isn't a code for what went wrong
#define SAM2_RESPONSE_PEER_ERROR               -3  // Emitted by a peer when there isn't a code for what went wrong
#define SAM2_RESPONSE_INVALID_ARGS             -4  // Emitted by signaling server when arguments are invalid
#define SAM2_RESPONSE_ROOM_DOES_NOT_EXIST      -6  // Emitted by signaling server when a room does not exist
#define SAM2_RESPONSE_ROOM_FULL                -7  // Emitted by signaling server or authority when it can't allow more connections for players or spectators
#define SAM2_RESPONSE_INVALID_HEADER           -9  // Emitted by signaling server when the header is invalid
#define SAM2_RESPONSE_PARTIAL_RESPONSE_TIMEOUT -11
#define SAM2_RESPONSE_PORT_NOT_AVAILABLE       -12 // Emitted by signaling server when a client tries to reserve a port that is already occupied
#define SAM2_RESPONSE_ALREADY_IN_ROOM          -13
#define SAM2_RESPONSE_PEER_DOES_NOT_EXIST      -14
#define SAM2_RESPONSE_CANNOT_SIGNAL_SELF       -16
#define SAM2_RESPONSE_VERSION_MISMATCH         -17
#define SAM2_RESPONSE_INVALID_ENCODE_TYPE      -18

#define SAM2_PORT_AVAILABLE                   0
#define SAM2_PORT_UNAVAILABLE                 1
#define SAM2_PORT_SENTINELS_MAX               SAM2_PORT_UNAVAILABLE

#define SAM2_PORT_MAX 8
#define SAM2_AUTHORITY_INDEX SAM2_PORT_MAX
#define SAM2_TOTAL_PEERS 64
#define SAM2_SPECTATOR_START (SAM2_PORT_MAX + 1)

// sam2_test.c
SAM2_LINKAGE int sam2_test_all(void);

// All data is sent in little-endian format
// All strings are utf-8 encoded unless stated otherwise... @todo Actually I should just add _utf8 if the field isn't ascii
// Packing of structs is asserted at compile time since packing directives are compiler specific
typedef struct sam2_room {
    char name[64]; // Unique name that identifies the room
    uint64_t flags;
    char core_and_version[32];
    uint64_t rom_hash_xxh64;
    uint16_t peer_ids[SAM2_TOTAL_PEERS]; // 0-7 p2p, 8 authority, 9-63 spectator; Must be unique per port (including authority and spectators)
} sam2_room_t;

// This is a test for identity not equality
static int sam2_same_room(sam2_room_t *a, sam2_room_t *b) {
    return a && b && a->peer_ids[SAM2_AUTHORITY_INDEX] == b->peer_ids[SAM2_AUTHORITY_INDEX];
}

static int sam2_get_port_of_peer(sam2_room_t *room, uint16_t peer_id) {
    for (int i = 0; i < SAM2_ARRAY_LENGTH(room->peer_ids); i++) {
        if (room->peer_ids[i] == peer_id) {
            return i;
        }
    }

    return -1;
}

typedef struct sam2_room_make_message {
    char header[8];
    sam2_room_t room;
} sam2_room_make_message_t;

// These are sent at a fixed rate until the client receives all the messages
typedef struct sam2_room_list_message {
    char header[8];
    sam2_room_t room; // Server indicates finished sending rooms by sending a room with room.peer_id[AUTHORITY_INDEX] == SAM2_PORT_UNAVAILABLE
} sam2_room_list_message_t;

typedef struct sam2_room_join_message {
    char header[8];
    uint64_t peer_id; // Peer id of sender set by sam2 server

    sam2_room_t room;
} sam2_room_join_message_t;

typedef struct sam2_connect_message {
    char header[8];
    uint16_t peer_id;

    uint16_t flags[3];
} sam2_connect_message_t;

typedef struct sam2_signal_message {
    char header[8];
    uint16_t peer_id;

    char ice_sdp[246];
} sam2_signal_message_t;

typedef struct sam2_error_message {
    char header[8];
    uint16_t peer_id;

    char description[238];
    int64_t code;
} sam2_error_message_t;

typedef union sam2_message {
    union sam2_message *next; // Points to next element in freelist @todo I should refactor the freelist code so I actually use this

    sam2_room_make_message_t room_make_response;
    sam2_room_list_message_t room_list_response;
    sam2_room_join_message_t room_join_response;
    sam2_connect_message_t connect_message;
    sam2_signal_message_t signal_message;
    sam2_error_message_t error_message;
} sam2_message_u;

typedef struct sam2_message_metadata {
    const char *header;
    const int message_size;
} sam2_message_metadata_t;

static sam2_message_metadata_t sam2__message_metadata[] = {
    {sam2_make_header, sizeof(sam2_room_make_message_t)},
    {sam2_list_header, sizeof(sam2_room_list_message_t)},
    {sam2_join_header, sizeof(sam2_room_join_message_t)},
    {sam2_conn_header, sizeof(sam2_connect_message_t)},
    {sam2_sign_header, sizeof(sam2_signal_message_t)},
    {sam2_fail_header, sizeof(sam2_error_message_t)},
};

static sam2_message_metadata_t *sam2_get_metadata(const char *message) {
    for (int i = 0; i < SAM2_ARRAY_LENGTH(sam2__message_metadata); i++) {
        if (memcmp(message, sam2__message_metadata[i].header, SAM2_HEADER_TAG_SIZE) == 0) {
            return &sam2__message_metadata[i];
        }
    }

    return NULL; // No matching header found
}

static SAM2_FORCEINLINE int sam2_header_matches(const char *message, const char *header) {
    return memcmp(message, header, 4 /* Header */ + 1 /* Major version */) == 0;
}

static int sam2_format_core_version(sam2_room_t *room, const char *name, const char *vers) {
    int i = 0;
    int version_len = strlen(vers);
    char *dst = room->core_and_version;
    int dst_size = sizeof(room->core_and_version) - 1;

    const char     *srcp = name; while (i < dst_size - version_len - 1 && *srcp != '\0') dst[i++] = *(srcp++);
    dst[i++] = ' '; srcp = vers; while (i < dst_size                   && *srcp != '\0') dst[i++] = *(srcp++);
    dst[i] = '\0';

    return i;
}

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
//#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sam2_socket_t;
#else
typedef int sam2_socket_t;
#endif

#define SAM2__INDEX_NULL ((uint16_t) 0x0000U)

#define SAM2__MEDIUM_POOL_SIZE 2048
#define SAM2__LARGE_POOL_SIZE 65536
typedef struct sam2__pool_node {
    uint16_t next;
    uint16_t prev;
} sam2__pool_node_t;

typedef struct sam2__pool {
    uint16_t free_list;
    uint16_t free_list_tail;
    uint16_t used_list;
    uint16_t used;
    uint16_t capacity;
    //sam2__pool_node_t node[n]; // capacity == n - 1 because 0-index reserved for null
} sam2__pool_t;

static SAM2_FORCEINLINE sam2__pool_node_t *sam2__pool_node(sam2__pool_t *pool) {
    char *node = ((char *) pool) + sizeof(pool[0]);
    return (sam2__pool_node_t *) node;
}

static SAM2_FORCEINLINE int sam2__pool_is_free(sam2__pool_t *pool, uint16_t idx) {
    sam2__pool_node_t *node = sam2__pool_node(pool);

    return idx == pool->free_list || node[idx].prev != SAM2__INDEX_NULL;
}

static void sam2__pool_init(sam2__pool_t *pool, int n) {
    sam2__pool_node_t *node = sam2__pool_node(pool);

    for (int i = 1; i < n; i++) {
        node[i].next = i + 1;
        node[i].prev = i - 1;
    }

    node[0].prev = SAM2__INDEX_NULL;
    node[n - 1].next = SAM2__INDEX_NULL;
    pool->free_list = 1;
    pool->free_list_tail = n - 1;
    pool->used_list = SAM2__INDEX_NULL;
    pool->used = 0;
    pool->capacity = n - 1;
}

static uint16_t sam2__pool_alloc_at_index(sam2__pool_t *pool, uint16_t idx) {
    sam2__pool_node_t *node = sam2__pool_node(pool);

    if ((uint16_t)(idx-1) >= pool->capacity) {
        return SAM2__INDEX_NULL; // Invalid index
    }

    if (!sam2__pool_is_free(pool, idx)) {
        return SAM2__INDEX_NULL; // Already allocated
    }

    // Remove from free list
    if (idx == pool->free_list) {
        pool->free_list = node[idx].next;
        if (pool->free_list != SAM2__INDEX_NULL) {
            node[pool->free_list].prev = SAM2__INDEX_NULL;
        } else {
            pool->free_list_tail = SAM2__INDEX_NULL; // Freelist becomes empty
        }
    } else if (idx == pool->free_list_tail) {
        pool->free_list_tail = node[idx].prev;
        node[node[idx].prev].next = SAM2__INDEX_NULL;
    } else {
        node[node[idx].prev].next = node[idx].next;
        node[node[idx].next].prev = node[idx].prev;
    }

    // Add to used list
    node[idx].next = pool->used_list;
    node[idx].prev = SAM2__INDEX_NULL;
    pool->used_list = idx;
    pool->used++;

    return idx;
}

static SAM2_FORCEINLINE uint16_t sam2__pool_alloc(sam2__pool_t *pool) {
    return sam2__pool_alloc_at_index(pool, pool->free_list);
}

static const char *sam2__pool_free(sam2__pool_t *pool, uint16_t idx) {
    sam2__pool_node_t *node = sam2__pool_node(pool);

    if ((uint16_t)(idx-1) >= pool->capacity) {
        return "Invalid index";
    }

    if (sam2__pool_is_free(pool, idx)) {
        return "Already free";
    }

    // Remove from used list
    if (idx == pool->used_list) {
        pool->used_list = node[idx].next;
    } else {
        node[node[idx].prev].next = node[idx].next;
    }

    // Add to free list at tail
    node[idx].next = SAM2__INDEX_NULL;
    node[idx].prev = pool->free_list_tail;
    if (pool->free_list_tail != SAM2__INDEX_NULL) {
        node[pool->free_list_tail].next = idx;
    } else {
        pool->free_list = idx; // List was empty, set head
    }
    pool->free_list_tail = idx;
    pool->used--;

    return NULL;
}

#ifdef __linux__
#include <sys/epoll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#include <sys/time.h>
#endif

typedef struct sam2_client {
    sam2_socket_t socket;
    uint16_t peer_id;

    char buffer[sizeof(sam2_message_u)];
    int length;

    uint16_t rooms_sent;
    int64_t last_activity;
} sam2_client_t;

typedef struct sam2_server {
    sam2_socket_t listen_socket;

    // Client management
    sam2_client_t clients[SAM2__LARGE_POOL_SIZE];
    sam2_room_t rooms[SAM2__LARGE_POOL_SIZE];
    uint16_t peer_id_map[SAM2__LARGE_POOL_SIZE];

    struct {
        sam2__pool_t client_pool;
        sam2__pool_node_t client_pool_node[SAM2__LARGE_POOL_SIZE];
    };

    struct {
        sam2__pool_t peer_id_pool;
        sam2__pool_node_t peer_id_pool_node[SAM2__LARGE_POOL_SIZE];
    };

    // Platform-specific polling
#ifdef _WIN32
    struct pollfd pollfds[SAM2__LARGE_POOL_SIZE];
#elif defined(__linux__) || defined(__ANDROID__)
    int epoll_fd;
    struct epoll_event events[SAM2__LARGE_POOL_SIZE];
#elif defined(__APPLE__) || defined(__FreeBSD__)
    int kqueue_fd;
    struct kevent events[SAM2__LARGE_POOL_SIZE];
    struct kevent changelist[SAM2__LARGE_POOL_SIZE];
    int changelist_count;
#else
    struct pollfd pollfds[SAM2__LARGE_POOL_SIZE];
#endif

    int poll_count;
    int64_t current_time;
} sam2_server_t;

static sam2_client_t* sam2__find_client(sam2_server_t *server, uint16_t peer_id) {
    uint16_t client_index = server->peer_id_map[peer_id];

    if (client_index != SAM2__INDEX_NULL) {
        return &server->clients[client_index];
    } else {
        return NULL;
    }
}

static sam2_room_t* sam2__find_hosted_room(sam2_server_t *server, sam2_room_t *room) {
    sam2_room_t *hosted_room = &server->rooms[room->peer_ids[SAM2_AUTHORITY_INDEX]];

    if (hosted_room->flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
        return hosted_room;
    } else {
        return NULL;
    }
}

// ===============================================
// == Server interface - Depends on libuv       ==
// ===============================================

// Note: You must allocate the memory for `server`
// ```c
// // Init server
// sam2_server_t server;
// sam2_server_init(&server, SAM2_SERVER_DEFAULT_PORT);
//
// // Wait for some events
// for (int i = 0; i < 10; i++) {
//     sam2_server_poll(&server.loop);
// }
//
// // Start asynchronous destruction
// sam2_server_begin_destroy(server);
//
// // Do the needful
// uv_run(&server.loop, UV_RUN_DEFAULT);
// uv_loop_close(&server.loop);
// ```
SAM2_LINKAGE int sam2_server_init(sam2_server_t *server, int port);
SAM2_LINKAGE int sam2_server_poll(sam2_server_t *server);
SAM2_LINKAGE void sam2_server_destroy(sam2_server_t *server);

// ===============================================
// == Client interface                          ==
// ===============================================

// Non-blocking trys to read a response sent by the server
// Returns negative on error, positive if there are more messages to read, and zero when you've processed the last message
// Errors can't be recovered from you must call sam2_client_disconnect and then sam2_client_connect again
SAM2_LINKAGE int sam2_client_poll(sam2_socket_t sockfd, sam2_message_u *message);

// Connects to host which is either an IPv4/IPv6 Address or domain name
// Will bias IPv6 if connecting via domain name and also block
SAM2_LINKAGE int sam2_client_connect(sam2_socket_t *sockfd_ptr, const char *host, int port);

SAM2_LINKAGE int sam2_client_poll_connection(sam2_socket_t sockfd, int timeout_ms);

SAM2_LINKAGE int sam2_client_send(sam2_socket_t sockfd, char *message);

//#include <stdio.h>
//#define SAM2_LOG_WRITE(level, file, line, ...) do { printf(__VA_ARGS__); printf("\n"); } while (0); // Ex. Use print
#ifndef SAM2_LOG_WRITE
#define SAM2_LOG_WRITE_DEFINITION
#define SAM2_LOG_WRITE sam2_log_write
#endif

#define SAM2_LOG_DEBUG(...) SAM2_LOG_WRITE(0, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_INFO(...)  SAM2_LOG_WRITE(1, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_WARN(...)  SAM2_LOG_WRITE(2, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_ERROR(...) SAM2_LOG_WRITE(3, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_FATAL(...) SAM2_LOG_WRITE(4, __FILE__, __LINE__, __VA_ARGS__)

#if defined(__GNUC__) || defined(__clang__)
    #define SAM2_FORMAT_ATTRIBUTE(format_idx, arg_idx) __attribute__((format(printf, format_idx, arg_idx)))
#else
    #define SAM2_FORMAT_ATTRIBUTE(format_idx, arg_idx)
#endif

SAM2_LINKAGE void sam2_log_write(int level, const char *file, int line, const char *format, ...) SAM2_FORMAT_ATTRIBUTE(4, 5);
#endif // SAM2_H

#if defined(SAM2_IMPLEMENTATION)
#ifndef SAM2_CLIENT_C
#define SAM2_CLIENT_C

#include <errno.h>
#include <stdarg.h>
#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#endif

#define RLE8_ENCODE_UPPER_BOUND(N) (3 * ((N+1) / 2) + (N) / 2)

int64_t rle8_encode_capped(const uint8_t *input, int64_t input_size, uint8_t *output, int64_t output_capacity) {
    int64_t output_size = 0;
    for (int64_t i = 0; i < input_size; ++i) {
        if (input[i] == 0) {
            uint16_t count = 1;
            while (i + 1 < input_size && input[i + 1] == 0) {
                count++;
                i++;
            }

            if (output_size >= output_capacity-2) goto err;
            output[output_size++] = 0; // Mark the start of a zero run
            // Encode count as little endian
            output[output_size++] = (uint8_t)(count & 0xFF);
            output[output_size++] = (uint8_t)((count >> 8) & 0xFF);
        } else {
            if (output_size >= output_capacity) goto err;
            output[output_size++] = input[i]; // Copy non-zero values directly
        }
    }

    return output_size; // Return the size of the encoded data
err:return -1;
}

int64_t rle8_decode_extra(const uint8_t* input, int64_t input_size, int64_t *input_consumed, uint8_t* output, int64_t output_capacity) {
    int64_t output_index = 0;
    while (*input_consumed < input_size) {
        if (output_index >= output_capacity) return output_index;
        if (input[*input_consumed] == 0) {
            if (input_size - *input_consumed < 3) return output_index;
            (*input_consumed)++; // Move past the zero marker
            uint16_t count = input[*input_consumed] | (input[*input_consumed + 1] << 8); // Decode count as little endian
            (*input_consumed) += 2; // Move past the count bytes

            while (count-- > 0) {
                if (output_index >= output_capacity) return output_index;
                output[output_index++] = 0;
            }
        } else {
            output[output_index++] = input[(*input_consumed)++];
        }
    }
    return output_index; // Return the size of the decoded data
}

// Decodes the encoded byte stream back into uint8_t values.
int64_t rle8_decode(const uint8_t* input, int64_t input_size, uint8_t* output, int64_t output_capacity) {
    int64_t input_consumed = 0;
    return rle8_decode_extra(input, input_size, &input_consumed, output, output_capacity);
}

int64_t rle8_decode_size(const uint8_t* input, int64_t input_size) {
    int64_t output_size = 0;
    int64_t input_consumed = 0;

    while (input_consumed < input_size) {
        if (input[input_consumed] == 0) {
            // Need at least 3 bytes for a run-length encoding (zero marker + 2 bytes for count)
            if (input_size - input_consumed < 3) {
                return -1; // Incomplete/truncated encoding
            }

            input_consumed++; // Skip zero marker
            // Extract 16-bit count in little-endian format
            uint16_t count = input[input_consumed] | (input[input_consumed + 1] << 8);
            input_consumed += 2;

            output_size += count; // Add zeros to output size
        } else {
            // Regular byte - copied directly
            output_size++;
            input_consumed++;
        }
    }

    return output_size;
}

int64_t rle8_pack_message(void *message, int64_t message_size) {
    char message_rle8[1408];

    int64_t message_size_rle8 = rle8_encode_capped((uint8_t *)message, message_size, (uint8_t *) message_rle8, sizeof(message_rle8));

    int compressed_message_is_larger = message_size_rle8 >= message_size;
    int compression_failed = message_size_rle8 == -1;

    if (compressed_message_is_larger || compression_failed) {
        ((char *) message)[7] = 'r';
        return message_size;
    } else {
        memcpy(message, message_rle8, message_size_rle8);
        memset((char *) message + message_size_rle8, 0, message_size - message_size_rle8);
        ((char *) message)[7] = 'z';
        return message_size_rle8;
    }
}

void rle8_unpack_message(uint8_t *message, int64_t message_size, void *message_rle8, int64_t message_size_rle8) {
    if (((char *) message)[7] == 'z' || ((char *) message)[7] == 'Z') {
        rle8_decode(message, message_size, (uint8_t *) message_rle8, message_size_rle8);
    }

    ((char *) message)[7] = 'R';
}

#ifdef _WIN32
    #define SAM2_SOCKET_ERROR (SOCKET_ERROR)
    #define SAM2_SOCKET_INVALID (INVALID_SOCKET)
    #define SAM2_CLOSESOCKET closesocket
    #define SAM2_SOCKERRNO ((int)WSAGetLastError())
    #define SAM2_EINPROGRESS WSAEWOULDBLOCK
#else
    #include <unistd.h>
    #define SAM2_SOCKET_ERROR (-1)
    #define SAM2_SOCKET_INVALID (-1)
    #define SAM2_CLOSESOCKET close
    #define SAM2_SOCKERRNO errno
    #define SAM2_EINPROGRESS EINPROGRESS
#endif

// Resolve hostname with DNS query
static int sam2__resolve_hostname(const char *hostname, char *ip) {
    struct addrinfo hints, *res, *p, desired_address;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    memset(&desired_address, 0, sizeof(desired_address));
    desired_address.ai_family = AF_UNSPEC;

    SAM2_LOG_INFO("Resolving hostname: %s", hostname);
    // I knew this could block but it just hangs on Windows at least for a very long time before timing out @todo
    if (getaddrinfo(hostname, NULL, &hints, &res)) {
        SAM2_LOG_ERROR("Address resolution failed for %s", hostname);
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        char ipvx[INET6_ADDRSTRLEN];
        socklen_t addrlen = sizeof(struct sockaddr_storage);

        if (getnameinfo(p->ai_addr, addrlen, ipvx, sizeof(ipvx), NULL, 0, NI_NUMERICHOST) != 0) {
            SAM2_LOG_ERROR("Couldn't convert IP Address to string: %d", SAM2_SOCKERRNO);
            continue;
        }

        SAM2_LOG_INFO("URL %s hosted on IPv%d address: %s", hostname, p->ai_family == AF_INET6 ? 6 : 4, ipvx);
        if (desired_address.ai_family != AF_INET6) {
            memcpy(ip, ipvx, INET6_ADDRSTRLEN);
            memcpy(&desired_address, p, sizeof(desired_address));
        }
    }

    freeaddrinfo(res);

    return desired_address.ai_family;
}

SAM2_LINKAGE int sam2_client_connect(sam2_socket_t *sockfd_ptr, const char *host, int port) {
    sam2_socket_t sockfd = SAM2_SOCKET_INVALID;
    struct sockaddr_storage server_addr = {0};
    // Initialize winsock / Increment winsock reference count
#ifdef _WIN32
    WSADATA wsaData;
    int wsa_status = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_status != 0) {
        SAM2_LOG_ERROR("WSAStartup failed!");
        goto fail;
    }
#endif

    char ip[INET6_ADDRSTRLEN];
    int family = sam2__resolve_hostname(host, ip); // This blocks
    if (family < 0) {
        SAM2_LOG_ERROR("Failed to resolve hostname for '%s'", host);
        goto fail;
    }
    host = ip;

    sockfd = socket(family, SOCK_STREAM, 0);
    if (sockfd == SAM2_SOCKET_INVALID) {
        SAM2_LOG_ERROR("Failed to create socket");
        goto fail;
    }

    { // Scope flags
#ifdef _WIN32
    u_long flags = 1; // 1 for non-blocking, 0 for blocking
    if (ioctlsocket(sockfd, FIONBIO, &flags) < 0) {
        SAM2_LOG_ERROR("Failed to set socket to non-blocking mode");
        goto fail;
    }
#else
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        SAM2_LOG_ERROR("Failed to set socket to non-blocking mode");
        goto fail;
    }
#endif
    }

    if (family == AF_INET) {
        ((struct sockaddr_in *)&server_addr)->sin_family = AF_INET;
        ((struct sockaddr_in *)&server_addr)->sin_port = htons(port);
        if (inet_pton(AF_INET, host, &((struct sockaddr_in *)&server_addr)->sin_addr) <= 0) {
            SAM2_LOG_ERROR("Failed to convert IPv4 address");
            goto fail;
        }
    } else if (family == AF_INET6) {
        ((struct sockaddr_in6 *)&server_addr)->sin6_family = AF_INET6;
        ((struct sockaddr_in6 *)&server_addr)->sin6_port = htons(port);
        if (inet_pton(AF_INET6, host, &((struct sockaddr_in6 *)&server_addr)->sin6_addr) <= 0) {
            SAM2_LOG_ERROR("Failed to convert IPv6 address");
            goto fail;
        }
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) < 0) {
        if (SAM2_SOCKERRNO != SAM2_EINPROGRESS) {
            SAM2_LOG_ERROR("connect returned error (%d)", SAM2_SOCKERRNO);
            goto fail;
        }
    }

    *sockfd_ptr = sockfd;
    return 0;

fail:
    if (sockfd != SAM2_SOCKET_INVALID) {
        SAM2_CLOSESOCKET(sockfd);
    }
#ifdef _WIN32
    if (wsa_status == 0) { // Only cleanup if WSAStartup was successful
        WSACleanup();
    }
#endif
    return -1;
}

SAM2_LINKAGE int sam2_client_disconnect(sam2_socket_t sockfd) {
    int status = 0;

    #ifdef _WIN32
    // Cleanup winsock / Decrement winsock reference count
    if (WSACleanup() == SOCKET_ERROR) {
        SAM2_LOG_ERROR("WSACleanup failed: %d", WSAGetLastError());
        status = -1;
    }
    #endif

    if (sockfd != SAM2_SOCKET_INVALID) {
        if (SAM2_CLOSESOCKET(sockfd) == SAM2_SOCKET_ERROR) {
            SAM2_LOG_ERROR("close failed: %s", strerror(errno));
            status = -1;
        }
    }

    return status;
}

SAM2_LINKAGE int sam2_client_poll_connection(sam2_socket_t sockfd, int timeout_ms) {
    fd_set fdset;
    struct timeval timeout;

    // Initialize fd_set
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);

    // Set timeout
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    // Use select() to poll the socket
#if _WIN32
    int nfds = 0; // Ignored on Windows
#else
    int nfds = sockfd + 1;
#endif
    int result = select(nfds, NULL, &fdset, NULL, &timeout);

    if (result < 0) {
        // Error occurred
        SAM2_LOG_ERROR("Error occurred while polling the socket");
        return 0;
    } else if (result > 0) {
        // Socket might be ready. Check for errors.
        int optval;
#ifdef _WIN32
        int optlen = sizeof(int);
#else
        socklen_t optlen = sizeof(int);
#endif

        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) < 0) {
            // Error in getsockopt
            SAM2_LOG_ERROR("Error in getsockopt");
            return 0;
        }

        if (optval) {
            // Error in delayed connection
            SAM2_LOG_ERROR("Error in delayed connection");
            return 0;
        }

        // Socket is ready
        return 1;
    } else {
        // Timeout
        //SAM2_LOG_DEBUG("Timeout while waiting for the socket to be ready");
        return 0;
    }
}

#ifdef _WIN32
//    #define SAM2_READ(sockfd, buf, len) recv(sockfd, buf, len, 0)
    #define SAM2_EAGAIN WSAEWOULDBLOCK
    #define SAM2_ENOTCONN WSAENOTCONN
#else
//    #define SAM2_READ read
    #define SAM2_EAGAIN EAGAIN
    #define SAM2_ENOTCONN ENOTCONN
#endif

static int sam2__frame_message(sam2_message_u *message, char *buffer, int *length) {
    if (*length < SAM2_HEADER_SIZE) return 0;
    sam2_message_metadata_t *metadata = sam2_get_metadata(buffer);

    if (metadata == NULL)                      return SAM2_RESPONSE_INVALID_HEADER;
    if (buffer[4] != SAM2_VERSION_MAJOR + '0') return SAM2_RESPONSE_VERSION_MISMATCH;

    int64_t message_bytes_read = 0;
    int64_t input_consumed = 0;

    if (buffer[7] == 'z') {
        message_bytes_read = rle8_decode_extra(
            (uint8_t *) buffer,
            *length,
            &input_consumed,
            (uint8_t *) message,
            metadata->message_size
        );
    } else if (buffer[7] == 'r') {
        if (*length >= metadata->message_size) {
            memcpy(message, buffer, metadata->message_size);
            message_bytes_read = input_consumed = metadata->message_size;
        }
    } else {
        return SAM2_RESPONSE_INVALID_ENCODE_TYPE;
    }

    if (message_bytes_read == metadata->message_size) {
        // Theoretically the memmove here is inefficient, but it shouldn't actually matter
        memmove(buffer, buffer + input_consumed, *length - input_consumed);
        *length -= input_consumed;

        return 1;
    } else {
        return 0;
    }
}

#define SAM2__SANITIZE_STRING(string) do { \
    int i = 0; \
    for (; i < SAM2_ARRAY_LENGTH(string) - 1; i++) if (string[i] == '\0') break; \
    for (; i < SAM2_ARRAY_LENGTH(string)    ; i++) string[i] = '\0'; \
} while (0)

static void sam2__sanitize_message(const char *message) {
    if (!message) return;

    // Sanitize C-Strings. This will also clear extra uninitialized bytes past the null terminator
    if (sam2_header_matches(message, sam2_make_header)) {
        sam2_room_make_message_t *make_message = (sam2_room_make_message_t *)message;
        SAM2__SANITIZE_STRING(make_message->room.name);
    } else if (sam2_header_matches(message, sam2_list_header)) {
        sam2_room_list_message_t *list_message = (sam2_room_list_message_t *)message;
        SAM2__SANITIZE_STRING(list_message->room.name);
    } else if (sam2_header_matches(message, sam2_join_header)) {
        sam2_room_join_message_t *join_message = (sam2_room_join_message_t *)message;
        SAM2__SANITIZE_STRING(join_message->room.name);
    } else if (sam2_header_matches(message, sam2_sign_header)) {
        sam2_signal_message_t *signal_message = (sam2_signal_message_t *)message;
        SAM2__SANITIZE_STRING(signal_message->ice_sdp);
    } else if (memcmp(message, sam2_fail_header, 8) == 0) {
        sam2_error_message_t *error_message = (sam2_error_message_t *)message;
        SAM2__SANITIZE_STRING(error_message->description);
    }
}

SAM2_LINKAGE int sam2_client_poll(sam2_socket_t sockfd, sam2_message_u *message) {
    char temp_buf[sizeof(sam2_message_u)];
    int peeked = recv(sockfd, temp_buf, sizeof(temp_buf), MSG_PEEK);
    if (peeked < 0) {
        if (SAM2_SOCKERRNO == SAM2_EAGAIN || SAM2_SOCKERRNO == EWOULDBLOCK) {
            return 0;  // No data available now.
        } else if (SAM2_SOCKERRNO == SAM2_ENOTCONN) {
            SAM2_LOG_INFO("Socket not connected");
            return 0;
        } else {
            SAM2_LOG_ERROR("Error peeking into socket");
            return -1;
        }
    } else if (peeked == 0) {
        SAM2_LOG_WARN("Server closed connection");
        return -1;
    }

    // See if we can frame a whole message
    int buf_len = peeked;
    int frame_status = sam2__frame_message(message, temp_buf, &buf_len);

    if (frame_status == 0) {
        // Not yet a complete message.
        return 0;
    } else if (frame_status < 0) {
        SAM2_LOG_ERROR("Message framing failed with code (%d)", frame_status);
        if (frame_status == SAM2_RESPONSE_INVALID_HEADER) {
            SAM2_LOG_WARN("Invalid header received '%.4s'", temp_buf);
        }

        return frame_status;
    } else {
        // Complete message was framed.
        // Calculate how many bytes were consumed by the framing routine
        int consumed = peeked - buf_len;
        // Now remove exactly the consumed bytes from the socket
        recv(sockfd, temp_buf, consumed, 0);
        SAM2_LOG_DEBUG("Received complete message with header '%.8s'", (char *)message);
        ((char *) message)[7] = 'R';
        sam2__sanitize_message((const char *)message);

        return 1;
    }
}

SAM2_LINKAGE int sam2_client_send(sam2_socket_t sockfd, char *message) {
    sam2_message_metadata_t *message_metadata = sam2_get_metadata(message);
    if (message_metadata == NULL) return -1;

    // Get the size of the message to be sent
    int message_size = message_metadata->message_size;

    char message_rle8[sizeof(sam2_message_u)];
    int64_t message_size_rle8 = rle8_encode_capped((uint8_t *) message, message_size, (uint8_t *) message_rle8, sizeof(message_rle8));

    // If this fails, we just send the message uncompressed
    if (message_size_rle8 != -1) {
        message_rle8[7] = 'z';
        message = message_rle8;
        message_size = message_size_rle8;
    }

    // Write the message to the socket
    int total_bytes_written = 0;
    while (total_bytes_written < message_size) {
        int bytes_written = send(sockfd, message + total_bytes_written, message_size - total_bytes_written, 0);
        if (bytes_written < 0) {
            // @todo This will busy wait
            if (SAM2_SOCKERRNO == SAM2_EAGAIN || SAM2_SOCKERRNO == EWOULDBLOCK) {
                SAM2_LOG_DEBUG("Socket is non-blocking and the requested operation would block");
                continue;
            } else {
                SAM2_LOG_ERROR("Error writing to socket");
                return -1;
            }
        }
        total_bytes_written += bytes_written;
    }

    SAM2_LOG_INFO("Message with header '%.8s' and size %d bytes sent successfully", message, message_size);
    return 0;
}
#endif // SAM2_CLIENT_C
#endif // SAM2_IMPLEMENTATION

#if defined(SAM2_IMPLEMENTATION)
#ifndef SAM2_SERVER_C
#define SAM2_SERVER_C

static int sam2__set_nonblocking(sam2_socket_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

static int sam2__set_socket_buffer_size(sam2_socket_t sock) {
    int size = 16384; // 16 KiB
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(size)) < 0) {
        SAM2_LOG_WARN("Failed to set send buffer size");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(size)) < 0) {
        SAM2_LOG_WARN("Failed to set recv buffer size");
    }
    return 0;
}

static void sam2__close_socket(sam2_socket_t sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

static int sam2__would_block() {
#ifdef _WIN32
    return SAM2_SOCKERRNO == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

static int64_t sam2__get_time_ms() {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (count.QuadPart * 1000) / freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

static void sam2__client_destroy(sam2_server_t *server, sam2_client_t *client) {
    if (!client || client->socket == SAM2_SOCKET_INVALID) return;

    uint16_t client_index = (uint16_t)(client - server->clients);
    uint16_t peer_id = client->peer_id;

    if (peer_id <= SAM2_PORT_SENTINELS_MAX) {
        SAM2_LOG_ERROR("Tried to free sentinel peer id %05d", peer_id);
        return;
    }

    // Remove from polling
#if defined(__linux__) || defined(__ANDROID__)
    epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, client->socket, NULL);
#elif defined(__APPLE__) || defined(__FreeBSD__)
    struct kevent ev;
    EV_SET(&ev, client->socket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(server->kqueue_fd, &ev, 1, NULL, 0, NULL);
    EV_SET(&ev, client->socket, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(server->kqueue_fd, &ev, 1, NULL, 0, NULL);
#endif

    sam2__close_socket(client->socket);
    client->socket = SAM2_SOCKET_INVALID;

    server->peer_id_map[peer_id] = SAM2__INDEX_NULL;
    sam2__pool_free(&server->peer_id_pool, peer_id);
    sam2__pool_free(&server->client_pool, client_index);

    server->rooms[peer_id].flags &= ~SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;

    SAM2_LOG_INFO("Client %05d disconnected", peer_id);
}

// Message sending - let OS handle buffering
static int sam2__write_message(sam2_server_t *server, sam2_client_t *client, char *message) {
    sam2_message_metadata_t *metadata = sam2_get_metadata((char*)message);
    if (!metadata) {
        SAM2_LOG_ERROR("Invalid message header '%.8s'", (char*)message);
        return -1;
    }

    int message_size = rle8_pack_message(message, metadata->message_size);

    // Try to send immediately - OS will buffer if needed
    int n = send(client->socket, (char*)message, message_size, 0);

    if (n == message_size) {
        // Success - entire message sent
        return 0;
    } else if (n < 0 && sam2__would_block()) {
        // OS buffer is full - log and drop message or close connection
        SAM2_LOG_WARN("Client %05d send buffer full, dropping message", client->peer_id);
        // @todo: Handle this more gracefully, the best thing is to peek and always leave space for an error along the lines of "server overload" or something
        return -1;
    } else if (n < 0) {
        // Real error
        SAM2_LOG_ERROR("Send error for client %05d: %d", client->peer_id, SAM2_SOCKERRNO);
        sam2__client_destroy(server, client);
        return -1;
    } else {
        // Partial send - this shouldn't happen with small messages and proper buffer sizes
        SAM2_LOG_ERROR("Partial send for client %05d: %d/%d bytes", client->peer_id, n, message_size);
        sam2__client_destroy(server, client);
        return -1;
    }
}

static void sam2__write_error(sam2_client_t *client, const char *error_text, int error_code) {
    sam2_error_message_t response = { SAM2_FAIL_HEADER, 0, "", error_code };
    strncpy(response.description, error_text, sizeof(response.description) - 1);

    sam2_message_metadata_t *metadata = sam2_get_metadata((char*)&response);
    int message_size = metadata->message_size;

    // For errors, we send directly without allocating from pool
    send(client->socket, (char*)&response, message_size, 0);
}

// Process client messages
static void sam2__process_message(sam2_server_t *server, sam2_client_t *client, sam2_message_u *message) {
    if (sam2_header_matches((const char*)message, sam2_conn_header)) {
        sam2_connect_message_t *request = &message->connect_message;

        if (server->peer_id_map[request->peer_id] != SAM2__INDEX_NULL && request->peer_id != client->peer_id) {
            sam2__write_error(client, "Peer id is already in use", SAM2_RESPONSE_INVALID_ARGS);
            return;
        }

        if (server->rooms[client->peer_id].flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
            sam2__write_error(client, "Can't change peer id while hosting a room", SAM2_RESPONSE_ALREADY_IN_ROOM);
            return;
        }

        if (request->peer_id <= SAM2_PORT_SENTINELS_MAX) {
            sam2__write_error(client, "Can't change peer id to port sentinels", SAM2_RESPONSE_INVALID_ARGS);
            return;
        }

        // Change peer ID
        uint16_t old_peer_id = client->peer_id;
        sam2__pool_free(&server->peer_id_pool, old_peer_id);
        uint16_t new_peer_id = sam2__pool_alloc_at_index(&server->peer_id_pool, request->peer_id);

        if (new_peer_id == SAM2__INDEX_NULL) {
            sam2__write_error(client, "Requested invalid peer id", SAM2_RESPONSE_INVALID_ARGS);
            return;
        }

        SAM2_LOG_INFO("Changing peer id from %05d to %05d", old_peer_id, new_peer_id);

        server->peer_id_map[new_peer_id] = server->peer_id_map[old_peer_id];
        server->peer_id_map[old_peer_id] = SAM2__INDEX_NULL;
        client->peer_id = new_peer_id;

        sam2_connect_message_t response = { SAM2_CONN_HEADER, new_peer_id, 0 };
        sam2__write_message(server, client, (char *)&response);

    } else if (sam2_header_matches((const char *)message, sam2_list_header)) {
        // Send all hosted rooms
        for (int i = 0; i < SAM2_ARRAY_LENGTH(server->rooms); i++) {
            if (server->rooms[i].flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
                sam2_room_list_message_t response = { SAM2_LIST_HEADER, server->rooms[i] };
                sam2__write_message(server, client, (char*)&response);
            }
        }

        // Send terminator
        sam2_room_list_message_t response = { SAM2_LIST_HEADER, { 0 } };
        sam2__write_message(server, client, (char *)&response);

    } else if (sam2_header_matches((const char*)message, sam2_make_header)) {
        sam2_room_make_message_t *request = &message->room_make_response;
        request->room.peer_ids[SAM2_AUTHORITY_INDEX] = client->peer_id;
        server->rooms[client->peer_id] = request->room;

        SAM2_LOG_INFO("Client %05d updated room '%s'", client->peer_id, request->room.name);

        sam2_room_make_message_t response = { SAM2_MAKE_HEADER, request->room };
        sam2__write_message(server, client, (char *)&response);

    } else if (sam2_header_matches((const char*)message, sam2_sign_header)) {
        sam2_signal_message_t request = message->signal_message;

        if (request.peer_id == client->peer_id) {
            sam2__write_error(client, "Cannot signal self", SAM2_RESPONSE_CANNOT_SIGNAL_SELF);
            return;
        }

        sam2_client_t *peer = sam2__find_client(server, request.peer_id);
        if (!peer) {
            sam2__write_error(client, "Peer not found", SAM2_RESPONSE_PEER_DOES_NOT_EXIST);
            return;
        }

        SAM2_LOG_INFO("Forwarding signal from %05d to %05d", client->peer_id, request.peer_id);
        sam2__write_message(server, peer, (char *)&request);
    }
}

// Socket I/O
static void sam2__process_client_read(sam2_server_t *server, sam2_client_t *client) {
    while (1) {
        int space = sizeof(client->buffer) - client->length;
        if (space <= 0) break;

        int n = recv(client->socket, client->buffer + client->length, space, 0);

        if (n > 0) {
            client->length += n;
            client->last_activity = server->current_time;

            // Process complete messages
            while (1) {
                sam2_message_u message;
                int old_length = client->length;
                int status = sam2__frame_message(&message, client->buffer, &client->length);

                if (status == 0) {
                    break; // Need more data
                } else if (status < 0) {
                    SAM2_LOG_ERROR("Client %05d framing error: %d", client->peer_id, status);
                    sam2__write_error(client, "Invalid message format", SAM2_RESPONSE_INVALID_ARGS);
                    sam2__client_destroy(server, client);
                    return;
                } else {
                    SAM2_LOG_INFO("Client %05d sent '%.8s'", client->peer_id, (char*)&message);
                    sam2__process_message(server, client, &message);
                }
            }
        } else if (n == 0) {
            // Connection closed
            sam2__client_destroy(server, client);
            return;
        } else {
            if (!sam2__would_block()) {
                SAM2_LOG_ERROR("Client %05d recv error: %d", client->peer_id, SAM2_SOCKERRNO);
                sam2__client_destroy(server, client);
            }
            return;
        }
    }
}

// Accept new connections
static void sam2__accept_connections(sam2_server_t *server) {
    while (1) {
        struct sockaddr_in6 addr;
        socklen_t addrlen = sizeof(addr);

        sam2_socket_t client_sock = accept(server->listen_socket, (struct sockaddr*)&addr, &addrlen);

        if (client_sock == SAM2_SOCKET_INVALID) {
            if (!sam2__would_block()) {
                SAM2_LOG_ERROR("Accept error: %d", SAM2_SOCKERRNO);
            }
            break;
        }

        // Allocate client slot
        uint16_t client_index = sam2__pool_alloc(&server->client_pool);
        if (client_index == SAM2__INDEX_NULL) {
            SAM2_LOG_WARN("No client slots available");
            sam2__close_socket(client_sock);
            continue;
        }

        // Allocate peer ID
        uint16_t peer_id = sam2__pool_alloc(&server->peer_id_pool);
        if (peer_id == SAM2__INDEX_NULL) {
            SAM2_LOG_ERROR("No peer IDs available");
            sam2__pool_free(&server->client_pool, client_index);
            sam2__close_socket(client_sock);
            continue;
        }

        // Initialize client
        sam2_client_t *client = &server->clients[client_index];
        memset(client, 0, sizeof(*client));
        client->socket = client_sock;
        client->peer_id = peer_id;
        client->last_activity = server->current_time;
        server->peer_id_map[peer_id] = client_index;

        // Configure socket
        sam2__set_nonblocking(client_sock);
        sam2__set_socket_buffer_size(client_sock);

        // Add to polling
#if defined(__linux__) || defined(__ANDROID__)
        struct epoll_event ev = {0};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = client;
        epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, client_sock, &ev);
#elif defined(__APPLE__) || defined(__FreeBSD__)
        struct kevent ev[2];
        EV_SET(&ev[0], client_sock, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, client);
        EV_SET(&ev[1], client_sock, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, client); // Disabled until needed
        kevent(server->kqueue_fd, ev, 2, NULL, 0, NULL);
#endif

        SAM2_LOG_INFO("Client %05d connected", peer_id);

        // Send connect message
        sam2_connect_message_t connect_msg = { SAM2_CONN_HEADER, peer_id, 0 };
        sam2__write_message(server, client, (char *)&connect_msg);
    }
}

// Platform-specific polling
#if defined(__linux__) || defined(__ANDROID__)
static int sam2__poll_sockets(sam2_server_t *server) {
    return epoll_wait(server->epoll_fd, server->events, SAM2__LARGE_POOL_SIZE, 0);
}
#elif defined(__APPLE__) || defined(__FreeBSD__)
static int sam2__poll_sockets(sam2_server_t *server) {
    struct timespec ts = {0, 0};
    return kevent(server->kqueue_fd, NULL, 0, server->events, SAM2__LARGE_POOL_SIZE, &ts);
}
#else
static int sam2__poll_sockets(sam2_server_t *server) {
    // Build pollfd array
    int nfds = 0;
    server->pollfds[nfds].fd = server->listen_socket;
    server->pollfds[nfds].events = POLLIN;
    server->pollfds[nfds].revents = 0;
    nfds++;

    for (uint16_t i = server->client_pool.used_list; i != SAM2__INDEX_NULL; ) {
        sam2__pool_node_t *node = sam2__pool_node(&server->client_pool);
        sam2_client_t *client = &server->clients[i];

        if (client->socket != SAM2_SOCKET_INVALID) {
            server->pollfds[nfds].fd = client->socket;
            server->pollfds[nfds].events = POLLIN;
            server->pollfds[nfds].revents = 0;
            nfds++;
        }

        i = node[i].next;
    }

    server->poll_count = nfds;

#ifdef _WIN32
    return WSAPoll(server->pollfds, nfds, 0);
#else
    return poll(server->pollfds, nfds, 0);
#endif
}
#endif

// Main poll function
SAM2_LINKAGE int sam2_server_poll(sam2_server_t *server) {
    server->current_time = sam2__get_time_ms();

#if defined(__linux__) || defined(__ANDROID__)
    int n_events = sam2__poll_sockets(server);

    // Process events on listening socket first
    sam2__accept_connections(server);

    for (int i = 0; i < n_events; i++) {
        sam2_client_t *client = (sam2_client_t*)server->events[i].data.ptr;

        if (server->events[i].events & (EPOLLERR | EPOLLHUP)) {
            sam2__client_destroy(server, client);
            continue;
        }

        if (server->events[i].events & EPOLLIN) {
            sam2__process_client_read(server, client);
        }
    }

#elif defined(__APPLE__) || defined(__FreeBSD__)
    int n_events = sam2__poll_sockets(server);

    for (int i = 0; i < n_events; i++) {
        if (server->events[i].ident == (uintptr_t)server->listen_socket) {
            sam2__accept_connections(server);
        } else {
            sam2_client_t *client = (sam2_client_t*)server->events[i].udata;

            if (server->events[i].flags & EV_EOF) {
                sam2__client_destroy(server, client);
                continue;
            }

            if (server->events[i].filter == EVFILT_READ) {
                sam2__process_client_read(server, client);
            }
        }
    }

#else
    int n_events = sam2__poll_sockets(server);

    if (n_events > 0) {
        // Check listen socket
        if (server->pollfds[0].revents & POLLIN) {
            sam2__accept_connections(server);
        }

        // Check client sockets
        int poll_idx = 1;
        for (uint16_t i = server->client_pool.used_list; i != SAM2__INDEX_NULL; ) {
            sam2__pool_node_t *node = sam2__pool_node(&server->client_pool);
            sam2_client_t *client = &server->clients[i];
            uint16_t next = node[i].next;

            if (poll_idx < server->poll_count && client->socket != SAM2_SOCKET_INVALID) {
                if (server->pollfds[poll_idx].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    sam2__client_destroy(server, client);
                } else if (server->pollfds[poll_idx].revents & POLLIN) {
                    sam2__process_client_read(server, client);
                }
                poll_idx++;
            }

            i = next;
        }
    }
#endif

    // Handle timeouts
    const int64_t TIMEOUT_MS = 30000; // 30 seconds
    for (uint16_t i = server->client_pool.used_list; i != SAM2__INDEX_NULL; ) {
        sam2__pool_node_t *node = sam2__pool_node(&server->client_pool);
        sam2_client_t *client = &server->clients[i];
        uint16_t next = node[i].next;

        if (client->socket != SAM2_SOCKET_INVALID &&
            server->current_time - client->last_activity > TIMEOUT_MS) {
            SAM2_LOG_INFO("Client %05d timed out", client->peer_id);
            sam2__client_destroy(server, client);
        }

        i = next;
    }

    return 0;
}

// Initialize server
SAM2_LINKAGE int sam2_server_init(sam2_server_t *server, int port) {
    memset(server, 0, sizeof(*server));

    // Initialize pools
    sam2__pool_init(&server->client_pool, SAM2_ARRAY_LENGTH(server->clients));
    sam2__pool_init(&server->peer_id_pool, SAM2_ARRAY_LENGTH(server->peer_id_pool_node));
    server->peer_id_pool.free_list = SAM2_PORT_SENTINELS_MAX + 1;

    // Initialize sockets on Windows
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        SAM2_LOG_ERROR("WSAStartup failed");
        return -1;
    }
#endif

    // Create listen socket - IPv6 with IPv4 support
    server->listen_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (server->listen_socket == SAM2_SOCKET_INVALID) {
        SAM2_LOG_ERROR("Failed to create socket: %d", SAM2_SOCKERRNO);
        return -1;
    }

    // Allow address reuse
    int reuse = 1;
    setsockopt(server->listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    // Disable IPv6-only to allow IPv4 connections on the same socket
    int v6only = 0;
    setsockopt(server->listen_socket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&v6only, sizeof(v6only));

    // Set non-blocking
    sam2__set_nonblocking(server->listen_socket);

    // Bind to all interfaces (IPv6 and IPv4)
    struct sockaddr_in6 addr = {0};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any;

    if (bind(server->listen_socket, (struct sockaddr*)&addr, sizeof(addr)) == SAM2_SOCKET_ERROR) {
        SAM2_LOG_ERROR("Bind failed: %d", SAM2_SOCKERRNO);
        sam2__close_socket(server->listen_socket);
        return -1;
    }

    // Listen
    if (listen(server->listen_socket, SAM2_DEFAULT_BACKLOG) == SAM2_SOCKET_ERROR) {
        SAM2_LOG_ERROR("Listen failed: %d", SAM2_SOCKERRNO);
        sam2__close_socket(server->listen_socket);
        return -1;
    }

    // Initialize platform-specific polling
#if defined(__linux__) || defined(__ANDROID__)
    server->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (server->epoll_fd == -1) {
        SAM2_LOG_ERROR("epoll_create1 failed: %d", errno);
        sam2__close_socket(server->listen_socket);
        return -1;
    }

    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.ptr = NULL; // NULL means listen socket
    epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, server->listen_socket, &ev);
#elif defined(__APPLE__) || defined(__FreeBSD__)
    server->kqueue_fd = kqueue();
    if (server->kqueue_fd == -1) {
        SAM2_LOG_ERROR("kqueue failed: %d", errno);
        sam2__close_socket(server->listen_socket);
        return -1;
    }

    struct kevent ev;
    EV_SET(&ev, server->listen_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    kevent(server->kqueue_fd, &ev, 1, NULL, 0, NULL);
#endif

    SAM2_LOG_INFO("Server listening on port %d (IPv4 and IPv6)", port);
    return 0;
}

// Destroy server
SAM2_LINKAGE void sam2_server_destroy(sam2_server_t *server) {
    // Close all clients
    for (uint16_t i = server->client_pool.used_list; i != SAM2__INDEX_NULL; ) {
        sam2__pool_node_t *node = sam2__pool_node(&server->client_pool);
        uint16_t next = node[i].next;
        sam2__client_destroy(server, &server->clients[i]);
        i = next;
    }

    // Close listen socket
    if (server->listen_socket != SAM2_SOCKET_INVALID) {
        sam2__close_socket(server->listen_socket);
    }

    // Close platform-specific resources
#if defined(__linux__) || defined(__ANDROID__)
    if (server->epoll_fd != -1) {
        close(server->epoll_fd);
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
    if (server->kqueue_fd != -1) {
        close(server->kqueue_fd);
    }
#endif

#ifdef _WIN32
    WSACleanup();
#endif
}

#endif // SAM2_SERVER_C
#endif // SAM2_IMPLEMENTATION


//=============================================================================
//== The following code just guarantees the C structs we're sending over     ==
//== the network will be binary compatible (packed and little-endian)        ==
//=============================================================================

// A fairly exhaustive macro for getting platform endianess taken from rapidjson which is also MIT licensed
#define SAM2_BYTEORDER_LITTLE_ENDIAN 0 // Little endian machine.
#define SAM2_BYTEORDER_BIG_ENDIAN 1 // Big endian machine.

#ifndef SAM2_BYTEORDER_ENDIAN
    // Detect with GCC 4.6's macro.
#   if defined(__BYTE_ORDER__)
#       if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#           define SAM2_BYTEORDER_ENDIAN SAM2_BYTEORDER_LITTLE_ENDIAN
#       elif (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#           define SAM2_BYTEORDER_ENDIAN SAM2_BYTEORDER_BIG_ENDIAN
#       else
#           error "Unknown machine byteorder endianness detected. User needs to define SAM2_BYTEORDER_ENDIAN."
#       endif
    // Detect with GLIBC's endian.h.
#   elif defined(__GLIBC__)
#       include <endian.h>
#       if (__BYTE_ORDER == __LITTLE_ENDIAN)
#           define SAM2_BYTEORDER_ENDIAN SAM2_BYTEORDER_LITTLE_ENDIAN
#       elif (__BYTE_ORDER == __BIG_ENDIAN)
#           define SAM2_BYTEORDER_ENDIAN SAM2_BYTEORDER_BIG_ENDIAN
#       else
#           error "Unknown machine byteorder endianness detected. User needs to define SAM2_BYTEORDER_ENDIAN."
#       endif
    // Detect with _LITTLE_ENDIAN and _BIG_ENDIAN macro.
#   elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#       define SAM2_BYTEORDER_ENDIAN SAM62_BYTEORDER_LITTLE_ENDIAN
#   elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
#       define SAM2_BYTEORDER_ENDIAN SAM2_BYTEORDER_BIG_ENDIAN
    // Detect with architecture macros.
#   elif defined(__sparc) || defined(__sparc__) || defined(_POWER) || defined(__powerpc__) || defined(__ppc__) || defined(__hpux) || defined(__hppa) || defined(_MIPSEB) || defined(_POWER) || defined(__s390__)
#       define SAM2_BYTEORDER_ENDIAN SAM2_BYTEORDER_BIG_ENDIAN
#   elif defined(__i386__) || defined(__alpha__) || defined(__ia64) || defined(__ia64__) || defined(_M_IX86) || defined(_M_IA64) || defined(_M_ALPHA) || defined(__amd64) || defined(__amd64__) || defined(_M_AMD64) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__bfin__)
#       define SAM2_BYTEORDER_ENDIAN SAM2_BYTEORDER_LITTLE_ENDIAN
#   elif defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARM64))
#       define SAM2_BYTEORDER_ENDIAN SAM2_BYTEORDER_LITTLE_ENDIAN
#   else
#       error "Unknown machine byteorder endianness detected. User needs to define SAM2_BYTEORDER_ENDIAN."
#   endif
#endif

// A static assert macro that works in C
// You can't use packing pragmas portably this is the next best thing
// If these fail then this server won't be binary compatible with the protocol and would fail horrendously
// Resort to packing pragmas until these succeed if you run into this issue yourself
SAM2_STATIC_ASSERT(SAM2_BYTEORDER_ENDIAN == SAM2_BYTEORDER_LITTLE_ENDIAN, "Platform is big-endian which is unsupported");
SAM2_STATIC_ASSERT(sizeof(sam2_room_t) == 64 + sizeof(uint64_t) + 32 + 64*sizeof(uint16_t) + sizeof(uint64_t), "sam2_room_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_make_message_t) == 8 + sizeof(sam2_room_t), "sam2_room_make_message_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_list_message_t) == 8 + sizeof(sam2_room_t), "sam2_room_list_message_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_join_message_t) == 8 + 8 + sizeof(sam2_room_t), "sam2_room_join_message_t is not packed");
