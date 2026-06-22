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
#define SAM2__PASTE_(a, b) a##b
#define SAM2__PASTE(a, b) SAM2__PASTE_(a, b)
#define SAM2_STATIC_ASSERT(cond, _) extern int SAM2__PASTE(sam2__static_assertion_, __COUNTER__)[(cond) ? 1 : -1]
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
#define SAM2_FLAG_ROOM_IS_NETWORK_HOSTED   0b01000000U

#define SAM2_FLAG_PORT0_CAN_SET_ALL_INPUTS (0b00000001U << 8)
//#define SAM2_FLAG_PORT1_CAN_SET_ALL_INPUTS (0b00000010U << 8)
// etc...

#define SAM2_FLAG_PORT0_PEER_IS_INACTIVE (0b00000001U << 16)
//#define SAM2_FLAG_PORT1_PEER_IS_INACTIVE (0b00000010U << 16)
// etc...

#define SAM2_FLAG_AUTHORITY_IS_INACTIVE (0b00000001U << 24)

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
#define SAM2_AUTHORITY_INDEX 0 // The authority always occupies port 0
#define SAM2_TOTAL_PEERS 64

// All data is sent in little-endian format
// Packing of structs is asserted at compile time since packing directives are compiler specific
typedef struct sam2_room {
    char name[64];
    char core_and_version[32];
    uint32_t rom_hash;
    uint32_t flags;
    uint16_t peer_ids[SAM2_TOTAL_PEERS]; // Port 0 is the authority; any port may host a peer. Must be unique per port
    uint64_t peer_topology; // Bit p set => port p is a p2p player (direct mesh + deterministic input). 0 => client-server (relayed spectator)
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

typedef struct sam2_room_list_message {
    char header[8];
    sam2_room_t room; // Request/response for the room hosted by room.peer_ids[SAM2_AUTHORITY_INDEX] or greater peer id
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
    return memcmp(message, header, 4 /* Tag */ + 1 /* Major version */) == 0;
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
typedef uintptr_t sam2_socket_t;
#else
typedef int sam2_socket_t;
#endif

#define SAM2__INDEX_NULL ((uint16_t) 0x0000U)

typedef struct sam2_server {
    sam2_socket_t stun_socket;

    // Socket 0 is the server listen socket. Peer sockets start after the sentinel peer IDs.
    sam2_socket_t sockets[65536];
    sam2_room_t rooms[65536];
    uint16_t num_client;
} sam2_server_t;

SAM2_LINKAGE int sam2_socket_buffer_size_client_to_server;
SAM2_LINKAGE int sam2_socket_buffer_size_server_to_client;

// ===============================================
// == Server interface                          ==
// ===============================================

// Note: You must allocate the memory for `server`
// ```c
// // Init server
// sam2_server_t *server = malloc(sizeof(sam2_server_t));
// sam2_server_init(&server, SAM2_SERVER_DEFAULT_PORT);
//
// // Process all pending events
// sam2_server_poll(server);
//
// sam2_server_destroy(server);
// ```
SAM2_LINKAGE int sam2_server_init(sam2_server_t *server, int port);
SAM2_LINKAGE int sam2_server_poll(sam2_server_t *server);
SAM2_LINKAGE void sam2_server_destroy(sam2_server_t *server);

// ===============================================
// == Client interface                          ==
// ===============================================

// Connects to host which is either an IPv4/IPv6 Address or domain name
// Will bias IPv6 if connecting via domain name and also block
SAM2_LINKAGE int sam2_client_connect(sam2_socket_t *sockfd_ptr, const char *host, int port);
SAM2_LINKAGE int sam2_client_poll_connection(sam2_socket_t sockfd, int timeout_ms);
SAM2_LINKAGE int sam2_client_poll(sam2_socket_t sockfd, sam2_message_u *message);
SAM2_LINKAGE int sam2_client_send(sam2_socket_t sockfd, char *message);

SAM2_LINKAGE int64_t rle8_encode_capped(const uint8_t *input, int64_t input_size, uint8_t *output, int64_t output_capacity);
SAM2_LINKAGE int64_t rle8_decode_extra(const uint8_t* input, int64_t input_size, int64_t *input_consumed, uint8_t* output, int64_t output_capacity);
SAM2_LINKAGE int64_t rle8_decode(const uint8_t* input, int64_t input_size, uint8_t* output, int64_t output_capacity);
SAM2_LINKAGE int64_t rle8_decode_size(const uint8_t* input, int64_t input_size);

#if defined(__GNUC__) || defined(__clang__)
    #define SAM2_FORMAT_ATTRIBUTE(format_idx, arg_idx) __attribute__((format(printf, format_idx, arg_idx)))
#else
    #define SAM2_FORMAT_ATTRIBUTE(format_idx, arg_idx)
#endif

// ===============================================
// == Logging                                   ==
// ===============================================

// Logging is optional and can be enabled by defining `SAM2_ENABLE_LOGGING` before including this header.
// `sam2_log_write` is resolved via external linkage, so you must implement it in your own codebase.
//
// Example implementation:
// ```c
// // Inside your_source_file.c
// #define SAM2_ENABLE_LOGGING
// #define SAM2_IMPLEMENTATION
// #include "sam2.h"
// void sam2_log_write(int level, const char *file, int line, const char *format, ...) {
//     va_list args;
//     va_start(args, format);
//     vfprintf(stdout, format, args);
//     va_end(args);
//     fprintf(stdout, "\n");
// }
// ```
SAM2_LINKAGE void sam2_log_write(int level, const char *file, int line, const char *format, ...) SAM2_FORMAT_ATTRIBUTE(4, 5);

#if defined(SAM2_ENABLE_LOGGING)
#define SAM2_LOG_DEBUG(...) sam2_log_write(0, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_INFO(...)  sam2_log_write(1, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_WARN(...)  sam2_log_write(2, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_ERROR(...) sam2_log_write(3, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_FATAL(...) sam2_log_write(4, __FILE__, __LINE__, __VA_ARGS__)
#else
#define SAM2_LOG_DEBUG(...)
#define SAM2_LOG_INFO(...)
#define SAM2_LOG_WARN(...)
#define SAM2_LOG_ERROR(...)
#define SAM2_LOG_FATAL(...)
#endif

#endif // SAM2_H

#if defined(SAM2_IMPLEMENTATION)
#ifndef SAM2_CLIENT_C
#define SAM2_CLIENT_C

#include <errno.h>
#include <stdarg.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#endif

int sam2_socket_buffer_size_client_to_server = 8192;
int sam2_socket_buffer_size_server_to_client = 8192;

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

int64_t rle8_pack_message(const void *message, int64_t message_size, void *packed) {
    int64_t message_size_rle8 = rle8_encode_capped((const uint8_t *)message, message_size, (uint8_t *)packed, message_size - 1);

    if (message_size_rle8 == -1) {
        memcpy(packed, message, message_size);
        ((char *) packed)[7] = 'r';
        return message_size;
    } else {
        ((char *) packed)[7] = 'z';
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
    struct addrinfo hints, *res, *p;
    int family = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    SAM2_LOG_INFO("Resolving hostname: %s", hostname);
    // I knew this could block but it just hangs on Windows at least for a very long time before timing out @todo
    int getaddrinfo_status = getaddrinfo(hostname, NULL, &hints, &res);
    if (getaddrinfo_status) {
        SAM2_LOG_ERROR("Address resolution failed for %s: %d", hostname, getaddrinfo_status);
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        char ipvx[INET6_ADDRSTRLEN];

        if (p->ai_family != AF_INET && p->ai_family != AF_INET6) {
            continue;
        }

        int getnameinfo_status = getnameinfo(p->ai_addr, (socklen_t)p->ai_addrlen, ipvx, sizeof(ipvx), NULL, 0, NI_NUMERICHOST);
        if (getnameinfo_status != 0) {
            SAM2_LOG_ERROR("Couldn't convert IP Address to string: %d", getnameinfo_status);
            continue;
        }

        SAM2_LOG_INFO("URL %s hosted on IPv%d address: %s", hostname, p->ai_family == AF_INET6 ? 6 : 4, ipvx);
        if (family != AF_INET6) {
            memcpy(ip, ipvx, INET6_ADDRSTRLEN);
            family = p->ai_family;
        }
    }

    freeaddrinfo(res);

    return family;
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
        return -1;
    }
#endif

    char ip[INET6_ADDRSTRLEN];
    int family = sam2__resolve_hostname(host, ip); // This blocks
    if (family != AF_INET && family != AF_INET6) {
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

    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char*)&sam2_socket_buffer_size_client_to_server, sizeof(int)) < 0) {
        SAM2_LOG_WARN("Failed to set socket send buffer size");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&sam2_socket_buffer_size_server_to_client, sizeof(int)) < 0) {
        SAM2_LOG_WARN("Failed to set socket recv buffer size");
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

    if (sockfd != SAM2_SOCKET_INVALID) {
        if (SAM2_CLOSESOCKET(sockfd) == SAM2_SOCKET_ERROR) {
            SAM2_LOG_ERROR("close failed: %s", strerror(errno));
            status = -1;
        }
    }

#ifdef _WIN32
    if (WSACleanup() == SOCKET_ERROR) {
        SAM2_LOG_ERROR("WSACleanup failed: %d", WSAGetLastError());
        status = -1;
    }
#endif

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

static int sam2__frame_message(sam2_message_u *message, char *buffer, int length) {
    if (length < SAM2_HEADER_SIZE) return 0;
    sam2_message_metadata_t *metadata = sam2_get_metadata(buffer);

    if (metadata == NULL)                      return SAM2_RESPONSE_INVALID_HEADER;
    if (buffer[4] != SAM2_VERSION_MAJOR + '0') return SAM2_RESPONSE_VERSION_MISMATCH;

    int64_t message_bytes_read = 0;
    int64_t input_consumed = 0;

    if (buffer[7] == 'z') {
        message_bytes_read = rle8_decode_extra(
            (uint8_t *) buffer,
            length,
            &input_consumed,
            (uint8_t *) message,
            metadata->message_size
        );
    } else if (buffer[7] == 'r') {
            if (length < metadata->message_size) return 0;

            memcpy(message, buffer, metadata->message_size);
            message_bytes_read = metadata->message_size;
            input_consumed = metadata->message_size;
    } else {
            return SAM2_RESPONSE_INVALID_ENCODE_TYPE;
    }

    if (message_bytes_read != metadata->message_size) {
        return 0;
    } else {
        return (int)input_consumed;
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
        SAM2_LOG_WARN("Connection closed");
        return -1;
    }

    // See if we can frame a whole message
    int consumed = sam2__frame_message(message, temp_buf, peeked);

    if (consumed == 0) {
        // Not yet a complete message.
        return 0;
    } else if (consumed < 0) {
        SAM2_LOG_ERROR("Message framing failed with code (%d)", consumed);
        if (consumed == SAM2_RESPONSE_INVALID_HEADER) {
            SAM2_LOG_WARN("Invalid header received '%.4s'", temp_buf);
        }

        return consumed;
    } else {
        // Complete message was framed.
        int received = recv(sockfd, temp_buf, consumed, 0);
        if (received != consumed) {
            SAM2_LOG_ERROR("Socket receive consumed %d/%d bytes", received, consumed);
            return -1;
        }
        SAM2_LOG_DEBUG("Received complete message with header '%.8s'", (char *)message);
        ((char *) message)[7] = 'R';
        sam2__sanitize_message((const char *)message);

        return 1;
    }
}

SAM2_LINKAGE int sam2_client_send(sam2_socket_t sockfd, char *message) {
    sam2_message_metadata_t *message_metadata = sam2_get_metadata(message);
    if (message_metadata == NULL) return -1;

    sam2_message_u packed;
    int message_size = (int)rle8_pack_message(message, message_metadata->message_size, &packed);

    int bytes_written = send(sockfd, (char *)&packed, message_size, 0);
    if (bytes_written == message_size) {
        SAM2_LOG_INFO("Message with header '%.8s' and size %d bytes sent successfully", (char *)&packed, message_size);
        return 0;
    } else if (bytes_written < 0 && (SAM2_SOCKERRNO == SAM2_EAGAIN || SAM2_SOCKERRNO == EWOULDBLOCK)) {
        SAM2_LOG_DEBUG("Socket is non-blocking and the requested operation would block");
        return -1;
    } else if (bytes_written < 0) {
        SAM2_LOG_ERROR("Error writing to socket");
        return -1;
    } else {
        SAM2_LOG_ERROR("Partial send: %d/%d bytes", bytes_written, message_size);
        return -1;
    }
}
#endif // SAM2_CLIENT_C

#ifndef SAM2_SERVER_C
#define SAM2_SERVER_C

#include <time.h>

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

static void sam2__close_socket(sam2_socket_t sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

static int sam2__would_block(void) {
#ifdef _WIN32
    return SAM2_SOCKERRNO == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

#define SAM2__STUN_BINDING_REQUEST  0x0001
#define SAM2__STUN_BINDING_RESPONSE 0x0101
#define SAM2__STUN_MAGIC_COOKIE     0x2112A442u
#define SAM2__STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020

static uint16_t sam2__read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t sam2__read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void sam2__write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void sam2__write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void sam2__poll_stun(sam2_server_t *server) {
    if (server->stun_socket == SAM2_SOCKET_INVALID) return;

    for (;;) {
        uint8_t request[512];
        uint8_t response[64];
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);
        int n = (int)recvfrom(server->stun_socket, (char *)request, sizeof(request), 0, (struct sockaddr *)&from, &from_len);
        if (n < 0) {
            if (!sam2__would_block()) {
                SAM2_LOG_WARN("STUN recvfrom failed: %d", SAM2_SOCKERRNO);
            }
            return;
        }

        if (n < 20
            || sam2__read_be16(request) != SAM2__STUN_BINDING_REQUEST
            || sam2__read_be32(request + 4) != SAM2__STUN_MAGIC_COOKIE) {
            continue;
        }

        memset(response, 0, sizeof(response));
        sam2__write_be16(response, SAM2__STUN_BINDING_RESPONSE);
        memcpy(response + 4, request + 4, 16);
        sam2__write_be16(response + 20, SAM2__STUN_ATTR_XOR_MAPPED_ADDRESS);

        if (from.ss_family == AF_INET) {
            struct sockaddr_in *a4 = (struct sockaddr_in *)&from;
            uint16_t xport = (uint16_t)(ntohs(a4->sin_port) ^ (SAM2__STUN_MAGIC_COOKIE >> 16));
            uint32_t xaddr = ntohl(a4->sin_addr.s_addr) ^ SAM2__STUN_MAGIC_COOKIE;
            sam2__write_be16(response + 2, 12);
            sam2__write_be16(response + 22, 8);
            response[25] = 0x01;
            sam2__write_be16(response + 26, xport);
            sam2__write_be32(response + 28, xaddr);
            sendto(server->stun_socket, (const char *)response, 32, 0, (struct sockaddr *)&from, from_len);
        } else if (from.ss_family == AF_INET6) {
            struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)&from;
            uint16_t xport = (uint16_t)(ntohs(a6->sin6_port) ^ (SAM2__STUN_MAGIC_COOKIE >> 16));

            if (IN6_IS_ADDR_V4MAPPED(&a6->sin6_addr)) {
                uint32_t mapped_addr = ((uint32_t)a6->sin6_addr.s6_addr[12] << 24)
                    | ((uint32_t)a6->sin6_addr.s6_addr[13] << 16)
                    | ((uint32_t)a6->sin6_addr.s6_addr[14] << 8)
                    | (uint32_t)a6->sin6_addr.s6_addr[15];
                sam2__write_be16(response + 2, 12);
                sam2__write_be16(response + 22, 8);
                response[25] = 0x01;
                sam2__write_be16(response + 26, xport);
                sam2__write_be32(response + 28, mapped_addr ^ SAM2__STUN_MAGIC_COOKIE);
                sendto(server->stun_socket, (const char *)response, 32, 0, (struct sockaddr *)&from, from_len);
            } else {
                sam2__write_be16(response + 2, 24);
                sam2__write_be16(response + 22, 20);
                response[25] = 0x02;
                sam2__write_be16(response + 26, xport);
                for (int i = 0; i < 16; i++) {
                    response[28 + i] = a6->sin6_addr.s6_addr[i] ^ request[4 + i];
                }
                sendto(server->stun_socket, (const char *)response, 44, 0, (struct sockaddr *)&from, from_len);
            }
        }
    }
}



static uint16_t sam2__client_get_peer_id(sam2_server_t *server, sam2_socket_t *client) {
    uint16_t peer_id = client - server->sockets;
    return peer_id;
}

static void sam2__client_destroy(sam2_server_t *server, sam2_socket_t *client) {
    uint16_t peer_id = sam2__client_get_peer_id(server, client);

    if (peer_id <= SAM2_PORT_SENTINELS_MAX) {
        SAM2_LOG_ERROR("Tried to free sentinel peer id %05d", peer_id);
        return;
    } else if (*client == SAM2_SOCKET_INVALID) {
        SAM2_LOG_ERROR("Tried to free already freed client %05d", peer_id);
        return;
    }

    sam2__close_socket(*client);
    *client = SAM2_SOCKET_INVALID;

    memset(&server->rooms[peer_id], 0, sizeof(sam2_room_t));

    server->num_client--;
    SAM2_LOG_INFO("Client %05" PRIu16 " disconnected", sam2__client_get_peer_id(server, client));
}

static int sam2__write_message(sam2_server_t *server, sam2_socket_t *client, char *message) {
    sam2_message_metadata_t *metadata = sam2_get_metadata((char*)message);
    if (!metadata) {
        SAM2_LOG_ERROR("Invalid message header '%.8s'", (char*)message);
        return -1;
    }

    sam2_message_u packed;
    int message_size = (int)rle8_pack_message(message, metadata->message_size, &packed);

    // Try to send immediately - OS will buffer if needed
    int n = send(*client, (char *)&packed, message_size, 0);

    if (n == message_size) {
        // Success - entire message sent
        return 0;
    } else if (n < 0 && sam2__would_block()) {
        // OS buffer is full - log and drop message or close connection
        SAM2_LOG_WARN("Client %05" PRIu16 " send buffer full, dropping message", sam2__client_get_peer_id(server, client));
        // @todo: Handle this more gracefully, the best thing is to peek and always leave space for an error along the lines of "server overload" or something
        return -1;
    } else if (n < 0) {
        // Real error
        SAM2_LOG_ERROR("Send error for client %05" PRIu16 ": %d", sam2__client_get_peer_id(server, client), SAM2_SOCKERRNO);
        sam2__client_destroy(server, client);
        return -1;
    } else {
        // Partial send - this shouldn't happen with small messages and proper buffer sizes
        SAM2_LOG_ERROR("Partial send for client %05" PRIu16 ": %d/%d bytes", sam2__client_get_peer_id(server, client), n, message_size);
        sam2__client_destroy(server, client);
        return -1;
    }
}

static void sam2__write_error(sam2_socket_t *client, const char *error_text, int error_code) {
    sam2_error_message_t response = { SAM2_FAIL_HEADER, 0, "", error_code };
    strncpy(response.description, error_text, sizeof(response.description) - 1);

    sam2_message_metadata_t *metadata = sam2_get_metadata((char*)&response);
    int message_size = metadata->message_size;

    // For errors, we send directly without allocating from pool
    send(*client, (char*)&response, message_size, 0);
}

// Process client messages
static sam2_socket_t *sam2__process_message(sam2_server_t *server, sam2_socket_t *client, sam2_message_u *message) {
    if (sam2_header_matches((const char*)message, sam2_conn_header)) {
        sam2_connect_message_t *request = &message->connect_message;

        if (server->sockets[request->peer_id] != SAM2_SOCKET_INVALID && request->peer_id != sam2__client_get_peer_id(server, client)) {
            sam2__write_error(client, "Peer id is already in use", SAM2_RESPONSE_INVALID_ARGS);
            return client;
        }

        if (server->rooms[sam2__client_get_peer_id(server, client)].flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
            sam2__write_error(client, "Can't change peer id while hosting a room", SAM2_RESPONSE_ALREADY_IN_ROOM);
            return client;
        }

        if (request->peer_id <= SAM2_PORT_SENTINELS_MAX) {
            sam2__write_error(client, "Can't change peer id to port sentinels", SAM2_RESPONSE_INVALID_ARGS);
            return client;
        }

        // Change peer ID
        uint16_t new_peer_id = request->peer_id;
        uint16_t old_peer_id = sam2__client_get_peer_id(server, client);

        SAM2_LOG_INFO("Changing peer id from %05d to %05d", old_peer_id, new_peer_id);

        if (new_peer_id != old_peer_id) {
            server->sockets[new_peer_id] = server->sockets[old_peer_id];
            server->sockets[old_peer_id] = SAM2_SOCKET_INVALID;
            client = &server->sockets[new_peer_id];
        }

        sam2_connect_message_t response = { SAM2_CONN_HEADER, new_peer_id, 0 };
        sam2__write_message(server, client, (char *)&response);
        if (*client == SAM2_SOCKET_INVALID) return NULL;

    } else if (sam2_header_matches((const char *)message, sam2_list_header)) {
        sam2_room_list_message_t *request = &message->room_list_response;
        uint16_t authority_peer_id_min = request->room.peer_ids[SAM2_AUTHORITY_INDEX];
        sam2_room_list_message_t response = { SAM2_LIST_HEADER, {0} };

        for (int i = authority_peer_id_min; i < SAM2_ARRAY_LENGTH(server->rooms); i++) {
            if (server->rooms[i].flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
                response.room = server->rooms[i];
                break;
            }
        }

        sam2__write_message(server, client, (char *)&response);
        if (*client == SAM2_SOCKET_INVALID) return NULL;

    } else if (sam2_header_matches((const char*)message, sam2_make_header)) {
        sam2_room_make_message_t *request = &message->room_make_response;
        request->room.peer_ids[SAM2_AUTHORITY_INDEX] = sam2__client_get_peer_id(server, client);
        server->rooms[sam2__client_get_peer_id(server, client)] = request->room;

        SAM2_LOG_INFO("Client %05d updated room '%s'", sam2__client_get_peer_id(server, client), request->room.name);

        sam2_room_make_message_t response = { SAM2_MAKE_HEADER, request->room };
        sam2__write_message(server, client, (char *)&response);
        if (*client == SAM2_SOCKET_INVALID) return NULL;

    } else if (sam2_header_matches((const char*)message, sam2_sign_header)) {
        sam2_signal_message_t request = message->signal_message;

        if (request.peer_id == sam2__client_get_peer_id(server, client)) {
            sam2__write_error(client, "Cannot signal self", SAM2_RESPONSE_CANNOT_SIGNAL_SELF);
            return client;
        }

        sam2_socket_t *peer = NULL;
        if (server->sockets[request.peer_id] != SAM2_SOCKET_INVALID) {
            peer = &server->sockets[request.peer_id];
        }

        if (!peer) {
            sam2__write_error(client, "Peer not found", SAM2_RESPONSE_PEER_DOES_NOT_EXIST);
            return client;
        }

        SAM2_LOG_INFO("Forwarding signal from %05d to %05d", sam2__client_get_peer_id(server, client), request.peer_id);
        request.peer_id = sam2__client_get_peer_id(server, client);
        sam2__write_message(server, peer, (char *)&request);
    }

    return client;
}

static void sam2__process_client_read(sam2_server_t *server, sam2_socket_t *client) {
    for (int _prevent_infinite_loop_counter = 0; _prevent_infinite_loop_counter < 64; _prevent_infinite_loop_counter++) {
        sam2_message_u message;
        int status = sam2_client_poll(*client, &message);

        if (status < 0) {
            SAM2_LOG_ERROR("Client %05" PRIu16 " error: %d", sam2__client_get_peer_id(server, client), status);
            sam2__client_destroy(server, client);
            return;
        } else if (status == 0) {
            break;
        } else {
            SAM2_LOG_INFO("Client %05" PRIu16 " sent '%.8s'", sam2__client_get_peer_id(server, client), (char*)&message);
            client = sam2__process_message(server, client, &message);
            if (*client == SAM2_SOCKET_INVALID) return;
        }
    }
}

static void sam2__accept_connections(sam2_server_t *server) {
    int potential_free_peer_id = SAM2_PORT_SENTINELS_MAX + 1; // Resume peer ID search from here to avoid scanning from zero for each connection

    while (1) {
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof(addr);
        sam2_socket_t client_socket = accept(server->sockets[0], (struct sockaddr*)&addr, &addrlen);

        if (client_socket == SAM2_SOCKET_INVALID) {
            if (!sam2__would_block()) {
                SAM2_LOG_ERROR("Accept error: %d", SAM2_SOCKERRNO);
            }
            break;
        }

        uint16_t peer_id = SAM2_PORT_UNAVAILABLE;
        for (; potential_free_peer_id < SAM2_ARRAY_LENGTH(server->sockets); potential_free_peer_id++) {
            if (server->sockets[potential_free_peer_id] == SAM2_SOCKET_INVALID) {
                peer_id = (uint16_t)potential_free_peer_id;
                potential_free_peer_id++;
                break;
            }
        }

        if (peer_id == SAM2_PORT_UNAVAILABLE) {
            SAM2_LOG_WARN("No peer IDs available");
            sam2_socket_t rejected_client = client_socket;
            sam2__set_nonblocking(rejected_client);
            sam2__write_error(&rejected_client, "No peer IDs available", SAM2_RESPONSE_PORT_NOT_AVAILABLE);
            sam2__close_socket(client_socket);
            continue;
        }

        sam2_socket_t *client = &server->sockets[peer_id];
        *client = client_socket;

        sam2__set_nonblocking(client_socket);

        if (setsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, (const char*)&sam2_socket_buffer_size_server_to_client, sizeof(int)) < 0) {
            SAM2_LOG_WARN("Failed to set socket send buffer size");
        }
        if (setsockopt(client_socket, SOL_SOCKET, SO_RCVBUF, (const char*)&sam2_socket_buffer_size_client_to_server, sizeof(int)) < 0) {
            SAM2_LOG_WARN("Failed to set socket recv buffer size");
        }

        server->num_client++;
        SAM2_LOG_INFO("Client %05" PRIu16 " connected", peer_id);

        sam2_connect_message_t connect_msg = { SAM2_CONN_HEADER, peer_id, {0} };
        sam2__write_message(server, client, (char *)&connect_msg);
    }
}

SAM2_LINKAGE int sam2_server_poll(sam2_server_t *server) {
    sam2__poll_stun(server);

    int sockets_queued = 0;
    int sockets_to_queue = server->num_client + 1 /* For the server listen socket */;
    int socket_scan = 0;

    while (sockets_queued < sockets_to_queue) {
        enum { SAM2__POLL_BATCH_SIZE = 256 };
        struct pollfd pollfds[SAM2__POLL_BATCH_SIZE];
        uint16_t peer_ids[SAM2__POLL_BATCH_SIZE];
        int nfds = 0;

        for (; nfds < SAM2__POLL_BATCH_SIZE && sockets_queued < sockets_to_queue && socket_scan < SAM2_ARRAY_LENGTH(server->sockets); socket_scan++) {
            if (server->sockets[socket_scan] == SAM2_SOCKET_INVALID) continue;
            pollfds[nfds].fd = server->sockets[socket_scan];
            pollfds[nfds].events = POLLIN;
            pollfds[nfds].revents = 0;
            peer_ids[nfds++] = (uint16_t)socket_scan;
            sockets_queued++;
        }

        if (nfds == 0) {
            break;
        }

        int n_events;
#ifdef _WIN32
        n_events = WSAPoll(pollfds, nfds, 0);
#else
        n_events = poll(pollfds, nfds, 0);
#endif

        if (n_events < 0) {
            SAM2_LOG_ERROR("Poll error: %d", SAM2_SOCKERRNO);
            return -1;
        } else if (n_events == 0) {
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            if (pollfds[i].revents == 0) continue;
            uint16_t peer_id = peer_ids[i];
            sam2_socket_t *socket = &server->sockets[peer_id];
            if (*socket != pollfds[i].fd) continue;
            if (peer_id == 0) {
                sam2__accept_connections(server);
            } else {
                sam2__process_client_read(server, socket);
            }
        }
    }

    return 0;
}

SAM2_LINKAGE int sam2_server_init(sam2_server_t *server, int port) {
    memset(server, 0, sizeof(*server));
    server->stun_socket = SAM2_SOCKET_INVALID;

    // Init sockets to invalid
    for (int i = 0; i < SAM2_ARRAY_LENGTH(server->sockets); i++) {
        server->sockets[i] = SAM2_SOCKET_INVALID;
    }

    // Initialize sockets on Windows
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        SAM2_LOG_ERROR("WSAStartup failed");
        return -1;
    }
#endif

    // Create listen socket - IPv6 with IPv4 support
    server->sockets[0] = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (server->sockets[0] == SAM2_SOCKET_INVALID) {
        SAM2_LOG_ERROR("Failed to create socket: %d", SAM2_SOCKERRNO);
        goto err;
    }

    // Disable IPv6-only to allow IPv4 connections on the same socket
    int v6only;
    v6only = 0;
    if (setsockopt(server->sockets[0], IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&v6only, sizeof(v6only)) == SAM2_SOCKET_ERROR) {
        SAM2_LOG_ERROR("Failed to set IPV6_V6ONLY: %d", SAM2_SOCKERRNO);
        goto err;
    }

#if !defined(_WIN32)
    // Set socket options for reusability
    int optval;
    optval = 1;
    if (setsockopt(server->sockets[0], SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) == SAM2_SOCKET_ERROR) {
        SAM2_LOG_ERROR("Failed to set SO_REUSEADDR: %d", SAM2_SOCKERRNO);
        goto err;
    }
    if (setsockopt(server->sockets[0], SOL_SOCKET, SO_REUSEPORT, (const char*)&optval, sizeof(optval)) == SAM2_SOCKET_ERROR) {
        SAM2_LOG_ERROR("Failed to set SO_REUSEPORT: %d", SAM2_SOCKERRNO);
        goto err;
    }
#endif

    // Set non-blocking
    if (sam2__set_nonblocking(server->sockets[0]) != 0) {
        SAM2_LOG_ERROR("Failed to set non-blocking");
        goto err;
    }

    // Bind to all interfaces (IPv6 and IPv4)
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any;

    if (bind(server->sockets[0], (struct sockaddr*)&addr, sizeof(addr)) == SAM2_SOCKET_ERROR) {
        SAM2_LOG_ERROR("Bind failed on port %d: %d", port, SAM2_SOCKERRNO);
        goto err;
    }

    // Listen
    if (listen(server->sockets[0], SAM2_DEFAULT_BACKLOG) == SAM2_SOCKET_ERROR) {
        SAM2_LOG_ERROR("Listen failed: %d", SAM2_SOCKERRNO);
        goto err;
    }

    server->stun_socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (server->stun_socket != SAM2_SOCKET_INVALID) {
        int stun_v6only = 0;
        setsockopt(server->stun_socket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&stun_v6only, sizeof(stun_v6only));
#if !defined(_WIN32)
        int stun_reuse = 1;
        setsockopt(server->stun_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&stun_reuse, sizeof(stun_reuse));
#endif
        sam2__set_nonblocking(server->stun_socket);
        if (bind(server->stun_socket, (struct sockaddr*)&addr, sizeof(addr)) == SAM2_SOCKET_ERROR) {
            SAM2_LOG_WARN("STUN UDP bind failed on port %d: %d", port, SAM2_SOCKERRNO);
            sam2__close_socket(server->stun_socket);
            server->stun_socket = SAM2_SOCKET_INVALID;
        }
    } else {
        SAM2_LOG_WARN("Failed to create STUN UDP socket: %d", SAM2_SOCKERRNO);
    }

    SAM2_LOG_INFO("Server listening on port %d (IPv4 and IPv6)", port);
    return 0;

err:if (server->sockets[0] != SAM2_SOCKET_INVALID) {
        sam2__close_socket(server->sockets[0]);
    }
    if (server->stun_socket != SAM2_SOCKET_INVALID) {
        sam2__close_socket(server->stun_socket);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return -1;
}

// Destroy server
SAM2_LINKAGE void sam2_server_destroy(sam2_server_t *server) {
    // Close all clients
    for (int i = SAM2_PORT_SENTINELS_MAX + 1; i < SAM2_ARRAY_LENGTH(server->sockets); i++) {
        if (server->sockets[i] != SAM2_SOCKET_INVALID) {
            sam2__client_destroy(server, &server->sockets[i]);
        }
    }

    // Close listen socket
    if (server->sockets[0] != SAM2_SOCKET_INVALID) {
        sam2__close_socket(server->sockets[0]);
    }

    if (server->stun_socket != SAM2_SOCKET_INVALID) {
        sam2__close_socket(server->stun_socket);
    }

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
#       define SAM2_BYTEORDER_ENDIAN SAM2_BYTEORDER_LITTLE_ENDIAN
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
SAM2_STATIC_ASSERT(sizeof(sam2_room_t) == sizeof(char[64]) + sizeof(char[32]) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t[SAM2_TOTAL_PEERS]) + sizeof(uint64_t), "sam2_room_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_make_message_t) == 8 + sizeof(sam2_room_t), "sam2_room_make_message_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_list_message_t) == 8 + sizeof(sam2_room_t), "sam2_room_list_message_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_join_message_t) == 8 + 8 + sizeof(sam2_room_t), "sam2_room_join_message_t is not packed");
