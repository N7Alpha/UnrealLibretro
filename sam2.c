// MIT License
// 
// Copyright (c) 2024 John Rehbein
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// I think using a splay tree would be slightly better, but this implmentation of an AVL tree was really good
//#include "kavl-lite.h"
// Copyright (c) 2018 by Attractive Chaos <attractor@live.co.uk> provided under the MIT License
#ifndef KAVL_LITE_H
#define KAVL_LITE_H

#ifdef __STRICT_ANSI__
#define inline __inline__
#endif

#define KAVLL_MAX_DEPTH 64

#define KAVLL_HEAD(__type) \
	struct { \
		__type *p[2]; \
		signed char balance; /* balance factor */ \
	}

#define __KAVLL_FIND(pre, __scope, __type, __head,  __cmp) \
	__scope __type *pre##_find(const __type *root, const __type *x) { \
		const __type *p = root; \
		while (p != 0) { \
			int cmp; \
			cmp = __cmp(x, p); \
			if (cmp < 0) p = p->__head.p[0]; \
			else if (cmp > 0) p = p->__head.p[1]; \
			else break; \
		} \
		return (__type*)p; \
	}

#define __KAVLL_ROTATE(pre, __type, __head) \
	/* one rotation: (a,(b,c)q)p => ((a,b)p,c)q */ \
	static inline __type *pre##_rotate1(__type *p, int dir) { /* dir=0 to left; dir=1 to right */ \
		int opp = 1 - dir; /* opposite direction */ \
		__type *q = p->__head.p[opp]; \
		p->__head.p[opp] = q->__head.p[dir]; \
		q->__head.p[dir] = p; \
		return q; \
	} \
	/* two consecutive rotations: (a,((b,c)r,d)q)p => ((a,b)p,(c,d)q)r */ \
	static inline __type *pre##_rotate2(__type *p, int dir) { \
		int b1, opp = 1 - dir; \
		__type *q = p->__head.p[opp], *r = q->__head.p[dir]; \
		p->__head.p[opp] = r->__head.p[dir]; \
		r->__head.p[dir] = p; \
		q->__head.p[dir] = r->__head.p[opp]; \
		r->__head.p[opp] = q; \
		b1 = dir == 0? +1 : -1; \
		if (r->__head.balance == b1) q->__head.balance = 0, p->__head.balance = -b1; \
		else if (r->__head.balance == 0) q->__head.balance = p->__head.balance = 0; \
		else q->__head.balance = b1, p->__head.balance = 0; \
		r->__head.balance = 0; \
		return r; \
	}

#define __KAVLL_INSERT(pre, __scope, __type, __head, __cmp) \
	__scope __type *pre##_insert(__type **root_, __type *x) { \
		unsigned char stack[KAVLL_MAX_DEPTH]; \
		__type *path[KAVLL_MAX_DEPTH]; \
		__type *bp, *bq; \
		__type *p, *q, *r = 0; /* _r_ is potentially the new root */ \
		int which = 0, top, b1, path_len; \
		bp = *root_, bq = 0; \
		/* find the insertion location */ \
		for (p = bp, q = bq, top = path_len = 0; p; q = p, p = p->__head.p[which]) { \
			int cmp; \
			cmp = __cmp(x, p); \
			if (cmp == 0) return p; \
			if (p->__head.balance != 0) \
				bq = q, bp = p, top = 0; \
			stack[top++] = which = (cmp > 0); \
			path[path_len++] = p; \
		} \
		x->__head.balance = 0, x->__head.p[0] = x->__head.p[1] = 0; \
		if (q == 0) *root_ = x; \
		else q->__head.p[which] = x; \
		if (bp == 0) return x; \
		for (p = bp, top = 0; p != x; p = p->__head.p[stack[top]], ++top) /* update balance factors */ \
			if (stack[top] == 0) --p->__head.balance; \
			else ++p->__head.balance; \
		if (bp->__head.balance > -2 && bp->__head.balance < 2) return x; /* no re-balance needed */ \
		/* re-balance */ \
		which = (bp->__head.balance < 0); \
		b1 = which == 0? +1 : -1; \
		q = bp->__head.p[1 - which]; \
		if (q->__head.balance == b1) { \
			r = pre##_rotate1(bp, which); \
			q->__head.balance = bp->__head.balance = 0; \
		} else r = pre##_rotate2(bp, which); \
		if (bq == 0) *root_ = r; \
		else bq->__head.p[bp != bq->__head.p[0]] = r; \
		return x; \
	}

#define __KAVLL_ERASE(pre, __scope, __type, __head, __cmp) \
	__scope __type *pre##_erase(__type **root_, const __type *x) { \
		__type *p, *path[KAVLL_MAX_DEPTH], fake; \
		unsigned char dir[KAVLL_MAX_DEPTH]; \
		int d = 0, cmp; \
		fake.__head.p[0] = *root_, fake.__head.p[1] = 0; \
		if (x) { \
			for (cmp = -1, p = &fake; cmp; cmp = __cmp(x, p)) { \
				int which = (cmp > 0); \
				dir[d] = which; \
				path[d++] = p; \
				p = p->__head.p[which]; \
				if (p == 0) return 0; \
			} \
		} else { \
			for (p = &fake; p; p = p->__head.p[0]) \
				dir[d] = 0, path[d++] = p; \
			p = path[--d]; \
		} \
		if (p->__head.p[1] == 0) { /* ((1,.)2,3)4 => (1,3)4; p=2 */ \
			path[d-1]->__head.p[dir[d-1]] = p->__head.p[0]; \
		} else { \
			__type *q = p->__head.p[1]; \
			if (q->__head.p[0] == 0) { /* ((1,2)3,4)5 => ((1)2,4)5; p=3 */ \
				q->__head.p[0] = p->__head.p[0]; \
				q->__head.balance = p->__head.balance; \
				path[d-1]->__head.p[dir[d-1]] = q; \
				path[d] = q, dir[d++] = 1; \
			} else { /* ((1,((.,2)3,4)5)6,7)8 => ((1,(2,4)5)3,7)8; p=6 */ \
				__type *r; \
				int e = d++; /* backup _d_ */\
				for (;;) { \
					dir[d] = 0; \
					path[d++] = q; \
					r = q->__head.p[0]; \
					if (r->__head.p[0] == 0) break; \
					q = r; \
				} \
				r->__head.p[0] = p->__head.p[0]; \
				q->__head.p[0] = r->__head.p[1]; \
				r->__head.p[1] = p->__head.p[1]; \
				r->__head.balance = p->__head.balance; \
				path[e-1]->__head.p[dir[e-1]] = r; \
				path[e] = r, dir[e] = 1; \
			} \
		} \
		while (--d > 0) { \
			__type *q = path[d]; \
			int which, other, b1 = 1, b2 = 2; \
			which = dir[d], other = 1 - which; \
			if (which) b1 = -b1, b2 = -b2; \
			q->__head.balance += b1; \
			if (q->__head.balance == b1) break; \
			else if (q->__head.balance == b2) { \
				__type *r = q->__head.p[other]; \
				if (r->__head.balance == -b1) { \
					path[d-1]->__head.p[dir[d-1]] = pre##_rotate2(q, which); \
				} else { \
					path[d-1]->__head.p[dir[d-1]] = pre##_rotate1(q, which); \
					if (r->__head.balance == 0) { \
						r->__head.balance = -b1; \
						q->__head.balance = b1; \
						break; \
					} else r->__head.balance = q->__head.balance = 0; \
				} \
			} \
		} \
		*root_ = fake.__head.p[0]; \
		return p; \
	}

#define kavll_free(__type, __head, __root, __free) do { \
		__type *_p, *_q; \
		for (_p = __root; _p; _p = _q) { \
			if (_p->__head.p[0] == 0) { \
				_q = _p->__head.p[1]; \
				__free(_p); \
			} else { \
				_q = _p->__head.p[0]; \
				_p->__head.p[0] = _q->__head.p[1]; \
				_q->__head.p[1] = _p; \
			} \
		} \
	} while (0)

#define kavll_size(__type, __head, __root, __cnt) do { \
		__type *_p, *_q; \
		*(__cnt) = 0; \
		for (_p = __root; _p; _p = _q) { \
			if (_p->__head.p[0] == 0) { \
				_q = _p->__head.p[1]; \
				++*(__cnt); \
			} else { \
				_q = _p->__head.p[0]; \
				_p->__head.p[0] = _q->__head.p[1]; \
				_q->__head.p[1] = _p; \
			} \
		} \
	} while (0)

#define __KAVLL_ITR(pre, __scope, __type, __head, __cmp) \
	typedef struct pre##_itr_t { \
		const __type *stack[KAVLL_MAX_DEPTH], **top, *right; /* _right_ points to the right child of *top */ \
	} pre##_itr_t; \
	__scope void pre##_itr_first(const __type *root, struct pre##_itr_t *itr) { \
		const __type *p; \
		for (itr->top = itr->stack - 1, p = root; p; p = p->__head.p[0]) \
			*++itr->top = p; \
		itr->right = (*itr->top)->__head.p[1]; \
	} \
	__scope int pre##_itr_find(const __type *root, const __type *x, struct pre##_itr_t *itr) { \
		const __type *p = root; \
		itr->top = itr->stack - 1; \
		while (p != 0) { \
			int cmp; \
			cmp = __cmp(x, p); \
			if (cmp < 0) *++itr->top = p, p = p->__head.p[0]; \
			else if (cmp > 0) p = p->__head.p[1]; \
			else break; \
		} \
		if (p) { \
			*++itr->top = p; \
			itr->right = p->__head.p[1]; \
			return 1; \
		} else if (itr->top >= itr->stack) { \
			itr->right = (*itr->top)->__head.p[1]; \
			return 0; \
		} else return 0; \
	} \
	__scope int pre##_itr_next(struct pre##_itr_t *itr) { \
		for (;;) { \
			const __type *p; \
			for (p = itr->right, --itr->top; p; p = p->__head.p[0]) \
				*++itr->top = p; \
			if (itr->top < itr->stack) return 0; \
			itr->right = (*itr->top)->__head.p[1]; \
			return 1; \
		} \
	}

#define kavll_at(itr) ((itr)->top < (itr)->stack? 0 : *(itr)->top)

#define KAVLL_INIT2(pre, __scope, __type, __head, __cmp) \
	__KAVLL_FIND(pre, __scope, __type, __head,  __cmp) \
	__KAVLL_ROTATE(pre, __type, __head) \
	__KAVLL_INSERT(pre, __scope, __type, __head, __cmp) \
	__KAVLL_ERASE(pre, __scope, __type, __head, __cmp) \
	__KAVLL_ITR(pre, __scope, __type, __head, __cmp)

#define KAVLL_INIT(pre, __type, __head, __cmp) \
	KAVLL_INIT2(pre,, __type, __head, __cmp)

#endif
// Manual include of kavl-lite.h ends here

// Signaling Server and a Match Maker
#ifndef SAM2_H
#define SAM2_H
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define SAM2__STR(s) _SAM2__STR(s)
#define _SAM2__STR(s) #s

#define SAM2_VERSION_MAJOR 1
#define SAM2_VERSION_MINOR 0
#define SAM2_PROTOCOL_STRING "v" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR)


#define SAM2_HEADER_SIZE 8

#define sam2_make_header "MAKE" SAM2_PROTOCOL_STRING
#define sam2_list_header "LIST" SAM2_PROTOCOL_STRING
#define sam2_join_header "JOIN" SAM2_PROTOCOL_STRING
#define sam2_ackj_header "ACKJ" SAM2_PROTOCOL_STRING
#define sam2_sync_header "SYNC" SAM2_PROTOCOL_STRING
#define sam2_conn_header "CONN" SAM2_PROTOCOL_STRING
#define sam2_sign_header "SIGN" SAM2_PROTOCOL_STRING
#define sam2_fail_header "FAIL" SAM2_PROTOCOL_STRING

// Although this literal is less flexible it explicitly tells the compiler that you're making a non-null terminated string so it doesn't throw up warnings
//#define SAM2__ERROR_HEADER { sam2_fail_header[0], 'A', 'I', 'L', SAM2_PROTOCOL_STRING[0], SAM2_PROTOCOL_STRING[1], SAM2_PROTOCOL_STRING[2], SAM2_PROTOCOL_STRING[3] }
#define SAM2__ERROR_HEADER { sam2_fail_header[0], sam2_fail_header[1], sam2_fail_header[2], sam2_fail_header[3], sam2_fail_header[4], sam2_fail_header[5], sam2_fail_header[6], sam2_fail_header[7] }
#define SAM2__SYNC_HEADER { sam2_sync_header[0], sam2_sync_header[1], sam2_sync_header[2], sam2_sync_header[3], sam2_sync_header[4], sam2_sync_header[5], sam2_sync_header[6], sam2_sync_header[7] }
#define SAM2__CONN_HEADER { sam2_conn_header[0], sam2_conn_header[1], sam2_conn_header[2], sam2_conn_header[3], sam2_conn_header[4], sam2_conn_header[5], sam2_conn_header[6], sam2_conn_header[7] }

#ifndef SAM2_LINKAGE
#ifdef __cplusplus
#define SAM2_LINKAGE extern "C"
#else
#define SAM2_LINKAGE extern
#endif
#endif

#define SAM2_MALLOC malloc
#define SAM2_FREE free

#if defined(__cplusplus) && (__cplusplus >= 201103L)
#define SAM2_STATIC_ASSERT(cond, message) static_assert(cond, message)
#elif defined(_MSVC_LANG) && (_MSVC_LANG >= 201103L)
#define SAM2_STATIC_ASSERT(cond, message) static_assert(cond, message)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define SAM2_STATIC_ASSERT(cond, message) _Static_assert(cond, message)
#else
#error "static_assert can't be properly defined in this language or compiler version"
#endif

#define SAM2_ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SAM2_MAX(a,b) ((a) < (b) ? (b) : (a))

#define SAM2_MIN(a, b) ((a) < (b) ? (a) : (b))

#define SAM2_ABS(a) ((a) < 0 ? -(a) : (a))

#define SAM2_SERVER_DEFAULT_PORT 9218
#define SAM2_DEFAULT_BACKLOG 128

// @todo move some of these into the UDP netcode file
#define SAM2_FLAG_NO_FIXED_PORT            0b00000001ULL // Clients aren't limited to setting input on bound port
#define SAM2_FLAG_ALLOW_SHOW_IP            0b00000010ULL
#define SAM2_FLAG_FORCE_TURN               0b00000100ULL
#define SAM2_FLAG_SPECTATOR                0b00001000ULL
#define SAM2_FLAG_ROOM_NEEDS_AUTHORIZATION 0b00010000ULL
#define SAM2_FLAG_AUTHORITY_IPv6           0b00100000ULL
#define SAM2_FLAG_ROOM_IS_INITIALIZED      0b01000000ULL // This should probably actually be IS_NETWORK_HOSTED

#define SAM2_FLAG_PORT0_CAN_SET_ALL_INPUTS (0b00000001ULL << 8)
#define SAM2_FLAG_PORT1_CAN_SET_ALL_INPUTS (0b00000010ULL << 8)
#define SAM2_FLAG_PORT2_CAN_SET_ALL_INPUTS (0b00000100ULL << 8)
#define SAM2_FLAG_PORT3_CAN_SET_ALL_INPUTS (0b00001000ULL << 8)
#define SAM2_FLAG_PORT4_CAN_SET_ALL_INPUTS (0b00010000ULL << 8)
#define SAM2_FLAG_PORT5_CAN_SET_ALL_INPUTS (0b00100000ULL << 8)
#define SAM2_FLAG_PORT6_CAN_SET_ALL_INPUTS (0b01000000ULL << 8)
#define SAM2_FLAG_PORT7_CAN_SET_ALL_INPUTS (0b10000000ULL << 8)

#define SAM2_FLAG_PORT0_PEER_IS_INACTIVE (0b00000001ULL << 16)
#define SAM2_FLAG_PORT1_PEER_IS_INACTIVE (0b00000010ULL << 16)
#define SAM2_FLAG_PORT2_PEER_IS_INACTIVE (0b00000100ULL << 16)
#define SAM2_FLAG_PORT3_PEER_IS_INACTIVE (0b00001000ULL << 16)
#define SAM2_FLAG_PORT4_PEER_IS_INACTIVE (0b00010000ULL << 16)
#define SAM2_FLAG_PORT5_PEER_IS_INACTIVE (0b00100000ULL << 16)
#define SAM2_FLAG_PORT6_PEER_IS_INACTIVE (0b01000000ULL << 16)
#define SAM2_FLAG_PORT7_PEER_IS_INACTIVE (0b10000000ULL << 16)

#define SAM2_FLAG_AUTHORITY_IS_INACTIVE (0b00000001ULL << 24)

#define SAM2_FLAG_SERVER_PERMISSION_MASK (SAM2_FLAG_AUTHORITY_IPv6)
#define SAM2_FLAG_AUTHORITY_PERMISSION_MASK (SAM2_FLAG_NO_FIXED_PORT | SAM2_FLAG_ALLOW_SHOW_IP)
#define SAM2_FLAG_CLIENT_PERMISSION_MASK (SAM2_FLAG_SPECTATOR)

#define SAM2_RESPONSE_SUCCESS                 0
#define SAM2_RESPONSE_SERVER_ERROR            1  // Emitted by signaling server when there was an internal error
#define SAM2_RESPONSE_AUTHORITY_ERROR         2  // Emitted by authority when there isn't a code for what went wrong
#define SAM2_RESPONSE_PEER_ERROR              3  // Emitted by a peer when there isn't a code for what went wrong
#define SAM2_RESPONSE_INVALID_ARGS            4  // Emitted by signaling server when arguments are invalid
#define SIG_RESPONSE_ROOM_ALREADY_EXISTS      5  // Emitted by signaling server when trying to create a room that already exists
#define SAM2_RESPONSE_ROOM_DOES_NOT_EXIST     6  // Emitted by signaling server when a room does not exist
#define SIG_RESPONSE_ROOM_FULL                7  // Emitted by signaling server or authority when it can't allow more connections for players or spectators
#define SIG_RESPONSE_ROOM_PASSWORD_WRONG      8  // Emitted by signaling server when the password is wrong
#define SIG_RESPONSE_INVALID_HEADER           9  // Emitted by signaling server when the header is invalid
#define SIG_RESPONSE_INVALID_BODY             10 // Emitted by signaling server when the header is valid but the body is invalid
#define SIG_RESPONSE_PARTIAL_RESPONSE_TIMEOUT 11
#define SAM2_RESPONSE_PORT_NOT_AVAILABLE      12 // Emitted by signaling server when a client tries to reserve a port that is already occupied
#define SAM2_RESPONSE_ALREADY_IN_ROOM         13
#define SAM2_RESPONSE_PEER_DOES_NOT_EXIST     14
#define SAM2_RESPONSE_PEER_NOT_IN_ROOM        15
#define SAM2_RESPONSE_CANNOT_SIGNAL_SELF      16
#define SAM2_RESPONSE_CONNECTION_INVALID      17

#define SAM2_PORT_UNAVAILABLE                 0
#define SAM2_PORT_AVAILABLE                   1
#define SAM2_PORT_SENTINELS_MAX               SAM2_PORT_AVAILABLE

#define SAM2_PORT_MAX 8
#define SAM2_AUTHORITY_INDEX SAM2_PORT_MAX

#define LOG_FATAL(...) printf(__FILE__ ":" SAM2__STR(__LINE__) ": " __VA_ARGS__); exit(1);
#define LOG_ERROR(...) printf(__FILE__ ":" SAM2__STR(__LINE__) ": " __VA_ARGS__)
#define LOG_WARN(...) printf(__FILE__ ":" SAM2__STR(__LINE__) ": " __VA_ARGS__)
#define LOG_INFO(...) printf(__FILE__ ":" SAM2__STR(__LINE__) ": " __VA_ARGS__)
#define LOG_VERBOSE(...) printf(__FILE__ ":" SAM2__STR(__LINE__) ": " __VA_ARGS__)

// All data is sent in little-endian format
// All strings are utf-8 encoded unless stated otherwise... @todo Actually I should just add _utf8 if the field isn't ascii
// Packing of structs is asserted at compile time since packing directives are compiler specific

typedef struct sig_room {
    char name[64]; // Unique name that identifies the room
    char turn_hostname[64]; // Idea: Use proof-of-work to limit unauthorized access to the TURN server force clients to occasionally hash savestates to maintain access... you have to come up with some way where the work is "bandwidth hard"
    uint64_t peer_ids[SAM2_PORT_MAX+1]; // Must be unique per port (including authority)
    //uint64_t authority_peer_id; // Set by sam2 server
    uint64_t flags;
} sam2_room_t;

// This is a test for identity not equality
SAM2_LINKAGE int sam2_same_room(sam2_room_t *a, sam2_room_t *b) {
    return    a->peer_ids[SAM2_AUTHORITY_INDEX] == b->peer_ids[SAM2_AUTHORITY_INDEX]
           && strcmp(a->name, b->name) == 0;
}

static void sanitize_room(sam2_room_t *associated_room, sam2_room_t *room, uint64_t peer_id) {
    room->flags &= ~SAM2_FLAG_SERVER_PERMISSION_MASK;
    // The logic in here doesn't work for disabled flags
    if (peer_id == room->peer_ids[SAM2_AUTHORITY_INDEX]) {
        room->flags |= SAM2_FLAG_AUTHORITY_PERMISSION_MASK;
    } else {
        room->flags |= SAM2_FLAG_CLIENT_PERMISSION_MASK;
    }

}

typedef struct sam2_room_acknowledge_join_message {
    char header[8];
    sam2_room_t room;

    uint64_t sender_peer_id; // Set by sam2
    uint64_t joiner_peer_id; // Set by sender
    int64_t frame_counter; // Frame that sender stopped on
} sam2_room_acknowledge_join_message_t;

typedef struct sig_room_make_message {
    char header[8];
    sam2_room_t room;

    // char crypto_signature[64]; // optional
    // uint8_t rom_siphash[16]; // optional
} sam2_room_make_message_t;

typedef struct sig_room_list_request {
    char header[8];
} sig_room_list_request_t;

// These are sent at a fixed rate until the client receives all the messages
typedef struct sig_room_list_response {
    char header[8];

    int64_t server_room_count;  // Set by server to total number of rooms listed on the server
    int64_t room_count; // Actual number of rooms inside of rooms
    sam2_room_t rooms[8];
} sam2_room_list_response_t;

typedef struct sam2_signal_message {
    char header[8];

    uint64_t peer_id;
    char ice_sdp[1024];
} sam2_signal_message_t;

typedef struct sam2_room_join_message {
    char header[8];

    uint64_t peer_id; // Peer id of sender set by sam2 server
    char room_secret[64]; // optional peers know this so use it to determine authorization?
    sam2_room_t room; // Set desired ports to PORT_RESERVE to request a port from the server
} sam2_room_join_message_t;

typedef struct sam2_connect_message {
    char header[8];
    uint64_t peer_id;
    uint64_t flags;
} sam2_connect_message_t;

typedef struct sam2_error_response {
    char header[8];
    int64_t code;
    char description[128];
    uint64_t peer_id;
} sam2_error_response_t;

typedef union sam2_response {
    union sam2_response *next; // Points to next element in freelist

    char buffer[sizeof(sam2_room_list_response_t)]; // @todo Remove
    sam2_room_make_message_t room_make_response;
    sam2_room_list_response_t room_list_response;
    sam2_room_join_message_t room_join_response;
    sam2_room_acknowledge_join_message_t room_acknowledge_join_message;
    sam2_connect_message_t connect_message;
    sam2_signal_message_t signal_message;
    sam2_error_response_t error_response;
} sam2_response_u;
SAM2_STATIC_ASSERT(sizeof(sam2_response_u) >= sizeof(sam2_room_list_response_t), "sam2_response_u::buffer is too small");

typedef union sam2_request {
    char buffer[sizeof(sam2_room_make_message_t)]; // @todo Remove
    sam2_room_make_message_t room_make_request;
    sig_room_list_request_t room_list_request;
    sam2_room_join_message_t room_join_request;
    sam2_signal_message_t signal_message;
} sam2_request_u;

typedef int sam2_message_e;
#define SAM2_EMESSAGE_INVALID -2
#define SAM2_EMESSAGE_PART  -1
#define SAM2_EMESSAGE_NONE   0
#define SAM2_EMESSAGE_MAKE   1
#define SAM2_EMESSAGE_LIST   2
#define SAM2_EMESSAGE_JOIN   3
#define SAM2_EMESSAGE_ACKJ   4
#define SAM2_EMESSAGE_CONN   5
#define SAM2_EMESSAGE_SIGNAL 6
#define SAM2_EMESSAGE_ERROR  7
#define SAM2_EMESSAGE_VOID   8

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <winsock2.h>
//#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sam2_socket_t;
#else
typedef int sam2_socket_t;
#endif

#define SAM2_SERVER
#if defined(SAM2_SERVER)
#include <uv.h>

typedef struct sam2_node {
    uint64_t key;
    uv_tcp_t *client;

    KAVLL_HEAD(struct sam2_node) head;
} sam2_avl_node_t;

typedef struct sam2_server {
    int64_t room_capacity; // Capacity of rooms and rooms_internal array
    //sig_room_internal_t *rooms_internal;
    sam2_response_u *response_freelist;

    struct client_data *clients;

    sam2_avl_node_t *_debug_allocated_response_set;

    sam2_avl_node_t *peer_id_map;
    int64_t room_count; 
    sam2_room_t rooms[];
} sam2_server_t;

typedef struct client_data {
    sam2_server_t *sig_server;
    uint64_t peer_id;

    uv_timer_t *timer;
    int64_t list_request_rooms_sent_so_far;

    sam2_room_t *hosted_room;
    sam2_message_e request_tag;
    union {
        char buffer[sizeof(sam2_room_make_message_t)]; // @todo Remove this
        sam2_room_make_message_t room_make_request;
        sig_room_list_request_t room_list_request;
        sam2_room_join_message_t room_join_request;
        sam2_room_acknowledge_join_message_t acknowledge_room_join_message;
        sam2_signal_message_t signal_message;
        sam2_error_response_t error_response;
    };
    int64_t length;
} client_data_t;

#define sam2__cmp(p, q) (((q)->key < (p)->key) - ((p)->key < (q)->key))
KAVLL_INIT2(sam2_avl, static, struct sam2_node, head, sam2__cmp)

static uv_tcp_t* sam2__find_client(sam2_server_t *server, uint64_t peer_id) {
    sam2_avl_node_t key_only_node = { peer_id };
    sam2_avl_node_t *node = sam2_avl_find(server->peer_id_map, &key_only_node);

    if (node) {
        return node->client;
    } else {
        return NULL;
    }
}

static client_data_t* sam2__find_client_data(sam2_server_t *server, uint64_t peer_id) {
    uv_tcp_t *client = sam2__find_client(server, peer_id);

    if (client) {
        return (client_data_t *) client->data;
    } else {
        return NULL;
    }
}

static sam2_room_t* sam2__find_hosted_room(sam2_server_t *server, sam2_room_t *room) {
    client_data_t *client_data = sam2__find_client_data(server, room->peer_ids[SAM2_AUTHORITY_INDEX]);

    if (client_data) {
        if (sam2_same_room(room, client_data->hosted_room)) {
            return client_data->hosted_room;
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

// ===============================================
// == Server interface - Depends on libuv       ==
// ===============================================

SAM2_LINKAGE int sam2_server_create(struct sam2_server **server, int64_t room_size) {return 0;}
SAM2_LINKAGE int sam2_server_destroy(struct sam2_server *server);
#endif


// ===============================================
// == Client interface                          ==
// ===============================================

// NOTE: Initialize response_tag to SAM2_EMESSAGE_NONE and response_length to 0 before calling sam2_client_poll 
//       and only ever read from it however response can be safely modified once you have a complete message
// Non-blocking trys to read a response sent by the server
// Returns negative on error, positive if there are more messages to read, and zero when you've processed the last message
// Errors can't be recovered from you must call sam2_client_disconnect and then sam2_client_connect again
SAM2_LINKAGE int sam2_client_poll(sam2_socket_t sockfd, sam2_response_u *response, sam2_message_e *response_tag, int *response_length);

// Connnects to host which is either an IPv4/IPv6 Address or domain name
// Will bias IPv6 if connecting via domain name and also block
SAM2_LINKAGE int sam2_client_connect(sam2_socket_t *sockfd_ptr, const char *host, int port);

#if defined(SAM2_EXECUTABLE)
    #define SAM2_IMPLEMENTATION
#endif

#if defined(SAM2_IMPLEMENTATION)

#include <errno.h> // for errno
#if defined(_WIN32)
#if _MT_ERRNO == 1
    #error "errno_t is not thread-safe";
#endif
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#endif


static int sam2__addr_is_numeric_hostname(const char *hostname) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICHOST;
    struct addrinfo *ai_list = NULL;
    if (getaddrinfo(hostname, NULL, &hints, &ai_list)) {
        return 0;
    }

    freeaddrinfo(ai_list);
    return 1;
}

// Resolve hostname with DNS query and prioritize IPv6
static int sam2__resolve_hostname(const char *hostname, char *ip) {
    struct addrinfo hints, *res, *p, desired_address;
    void *ptr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    memset(&desired_address, 0, sizeof(desired_address));
    desired_address.ai_family = AF_UNSPEC;

    if (getaddrinfo(hostname, NULL, &hints, &res)) {
        LOG_ERROR("Address resolution failed for %s", hostname);
        return -1;
    }

    LOG_INFO("Host: %s\n", hostname);

    for(p = res; p != NULL; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            ptr = &((struct sockaddr_in *) p->ai_addr)->sin_addr;
        } else if (p->ai_family == AF_INET6) {
            ptr = &((struct sockaddr_in6 *) p->ai_addr)->sin6_addr;
        } else {
            continue;
        }

        char ipvx[INET6_ADDRSTRLEN];
        if (inet_ntop(p->ai_family, ptr, ipvx, INET6_ADDRSTRLEN) == NULL) {
            LOG_ERROR("Couldn't convert IP Address to string\n");
            continue;
        }

        LOG_INFO("%s hosted on IPv%d address: %s\n", hostname, p->ai_family == AF_INET6 ? 6 : 4, ipvx);
        if (desired_address.ai_family != AF_INET6) {
            memcpy(ip, ipvx, INET6_ADDRSTRLEN);
            memcpy(&desired_address, p, sizeof(desired_address));
        }
    }

    freeaddrinfo(res);

    return desired_address.ai_family;
}

#ifdef _WIN32
    #define SAM2_SOCKET_ERROR (SOCKET_ERROR)
    #define SAM2_SOCKET_INVALID (INVALID_SOCKET)
    #define SAM2_CLOSESOCKET closesocket
    #define SAM2_SOCKERRNO ((int)WSAGetLastError())
    #define SAM2_EINPROGRESS WSAEWOULDBLOCK
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #define SAM2_SOCKET_ERROR (-1)
    #define SAM2_SOCKET_INVALID (-1)
    #define SAM2_CLOSESOCKET close
    #define SAM2_SOCKERRNO errno
    #define SAM2_EINPROGRESS EINPROGRESS
#endif

SAM2_LINKAGE int sam2_client_connect(sam2_socket_t *sockfd_ptr, const char *host, int port) {
    // Initialize winsock / Increment winsock reference count
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_ERROR("WSAStartup failed!\n");
        return -1;
    }
#endif

    // Resolve host through DNS if it's not a numeric address
    char ip[INET6_ADDRSTRLEN];
    int family = AF_INET;
    if (!sam2__addr_is_numeric_hostname(host)) {
        family = sam2__resolve_hostname(host, ip); // This blocks
        if (family < 0) {
            LOG_ERROR("Failed to resolve hostname");
            return -1;
        }
        host = ip;
    }

    // Create a socket
    sam2_socket_t sockfd = socket(family, SOCK_STREAM, 0);
    if (sockfd == SAM2_SOCKET_INVALID) {
        LOG_ERROR("Failed to create socket");
        return -1;
    }

    // Set the socket to non-blocking mode
    #ifdef _WIN32
    u_long flags = 1; // 1 for non-blocking, 0 for blocking
    if (ioctlsocket(sockfd, FIONBIO, &flags) < 0) {
        LOG_ERROR("Failed to set socket to non-blocking mode\n");
        SAM2_CLOSESOCKET(sockfd);
        return -1;
    }
    #else
    int current_flags = fcntl(sockfd, F_GETFL, 0);
    if (current_flags < 0) {
        LOG_ERROR("Failed to get current socket flags\n");
        SAM2_CLOSESOCKET(sockfd);
        return -1;
    }

    if (fcntl(sockfd, F_SETFL, current_flags | O_NONBLOCK) < 0) {
        LOG_ERROR("Failed to set socket to non-blocking mode\n");
        SAM2_CLOSESOCKET(sockfd);
        return -1;
    }
    #endif

    // Specify the numerical address of the server we're trying to connnect to
    if (family == AF_INET) {
        struct sockaddr_in server_addr = {0};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
            LOG_ERROR("Failed to convert IPv4 address");
            SAM2_CLOSESOCKET(sockfd);
            return -1;
        }
        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            if (SAM2_SOCKERRNO != SAM2_EINPROGRESS) {
                LOG_ERROR("Failed to connect to server");
                SAM2_CLOSESOCKET(sockfd);
                return -1;
            }
        }
    } else if (family == AF_INET6) {
        struct sockaddr_in6 server_addr = {0};
        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_port = htons(port);
        if (inet_pton(AF_INET6, host, &server_addr.sin6_addr) <= 0) {
            LOG_ERROR("Failed to convert IPv6 address");
            SAM2_CLOSESOCKET(sockfd);
            return -1;
        }
        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            if (SAM2_SOCKERRNO != SAM2_EINPROGRESS) {
                LOG_ERROR("Failed to connect to server");
                SAM2_CLOSESOCKET(sockfd);
                return -1;
            }
        }
    } else {
        LOG_ERROR("Unknown address family");
        SAM2_CLOSESOCKET(sockfd);
        return -1;
    }

    *sockfd_ptr = sockfd;
    return 0;
}

SAM2_LINKAGE int sam2_client_disconnect(sam2_socket_t *sockfd_ptr, const char *host) {
    int status = 0;

    #ifdef _WIN32
    if (WSACleanup() == SOCKET_ERROR) {
        LOG_ERROR("WSACleanup failed: %d\n", WSAGetLastError());
        status = -1;
    }
    #endif

    if (*sockfd_ptr != SAM2_SOCKET_INVALID) {
        if (SAM2_CLOSESOCKET(*sockfd_ptr) == SAM2_SOCKET_ERROR) {
            LOG_ERROR("close failed: %s\n", strerror(errno));
            status = -1;
        }

        *sockfd_ptr = SAM2_SOCKET_INVALID;
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
        LOG_ERROR("Error occurred while polling the socket");
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
            LOG_ERROR("Error in getsockopt");
            return 0;
        }

        if (optval) {
            // Error in delayed connection
            LOG_ERROR("Error in delayed connection");
            return 0;
        }

        // Socket is ready
        return 1;
    } else {
        // Timeout
        //LOG_VERBOSE("Timeout while waiting for the socket to be ready\n");
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

static struct {
    const char *header;
    const int message_size;
} sam2__request_map[] = {
    {"GOTOFAIL", SAM2_HEADER_SIZE}, /* SAM2_EMESSAGE_NONE */
    {sam2_make_header, sizeof(sam2_room_make_message_t)},
    {sam2_list_header, sizeof(sig_room_list_request_t)},
    {sam2_join_header, sizeof(sam2_room_join_message_t)},
    {sam2_ackj_header, sizeof(sam2_room_acknowledge_join_message_t)},
    {sam2_conn_header, sizeof(sam2_connect_message_t)},
    {sam2_sign_header, sizeof(sam2_signal_message_t)},
    {sam2_fail_header, sizeof(sam2_error_response_t)},
};

static struct {
    const char *header;
    const int message_size;
} sam2__response_map[] = {
    {"GOTOFAIL", SAM2_HEADER_SIZE}, /* SAM2_EMESSAGE_NONE */
    {sam2_make_header, sizeof(sam2_room_make_message_t)},
    {sam2_list_header, sizeof(sam2_room_list_response_t)},
    {sam2_join_header, sizeof(sam2_room_join_message_t)},
    {sam2_ackj_header, sizeof(sam2_room_acknowledge_join_message_t)},
    {sam2_conn_header, sizeof(sam2_connect_message_t)},
    {sam2_sign_header, sizeof(sam2_signal_message_t)},
    {sam2_fail_header, sizeof(sam2_error_response_t)},
};

// NOTE: Initialize response_tag to SAM2_EMESSAGE_NONE and response_length to 0 before calling sam2_client_poll and only ever read from it
// Non-blocking trys to read a response sent by the server
// Returns negative on error, positive if there are more messages to read, and zero when you've processed the last message
SAM2_LINKAGE int sam2_client_poll(sam2_socket_t sockfd, sam2_response_u *response, sam2_message_e *response_tag, int *response_length) {
    if (*response_tag == SAM2_EMESSAGE_VOID) return -1; // We can't recover from receiving a bad header
    if (*response_tag >  SAM2_EMESSAGE_VOID) return -1; // Not in range of valid tags
    if (*response_tag <  SAM2_EMESSAGE_PART) return -1; // Not in range of valid tags

    // If the last message was complete setup to read a new one
    if (*response_tag != SAM2_EMESSAGE_PART) {
        *response_tag = SAM2_EMESSAGE_NONE;
        *response_length = 0;
    }

    // The logic for reading a complete message is tricky since you can
    // potentially get a fraction of it due to the streaming nature of TCP
    // This loop is at max 2 iterations 1 reading the header and 1 reading the body
    while (   *response_tag == SAM2_EMESSAGE_NONE 
           || *response_tag == SAM2_EMESSAGE_PART) {

        int bytes_desired;
        int bytes_read = 0;
        sam2_message_e header_tag = SAM2_EMESSAGE_NONE;
        if (*response_length < SAM2_HEADER_SIZE) {
            bytes_desired = SAM2_HEADER_SIZE - *response_length;
        } else {
            for (header_tag = SAM2_EMESSAGE_NONE+1; header_tag < SAM2_EMESSAGE_VOID; header_tag++) {
                if (memcmp(response->buffer, sam2__response_map[header_tag].header, SAM2_HEADER_SIZE) == 0) {
                    break;
                }
            }

            if (header_tag == SAM2_EMESSAGE_VOID) {
                *response_tag = SAM2_EMESSAGE_ERROR;
                LOG_ERROR("Received invalid header\n");
                return -1;
            }

            bytes_desired = sam2__response_map[header_tag].message_size - *response_length;
        }

        if (bytes_desired == 0) goto successful_read; // Trying to read zero bytes from a socket will close it
        bytes_read = recv(sockfd, ((char *) response) + *response_length, bytes_desired, 0);
        
        if (bytes_read < 0) {
            if (SAM2_SOCKERRNO == SAM2_EAGAIN || SAM2_SOCKERRNO == EWOULDBLOCK) {
                //LOG_VERBOSE("No more datagrams to receive\n");
                return 0;
            } else if (SAM2_SOCKERRNO == SAM2_ENOTCONN) {
                LOG_INFO("Socket not connected\n");
                return 0;
            }
            // @todo Get rid of \n from LOG messages
            LOG_ERROR("Error reading from socket");//, strerror(errno));
            *response_tag = SAM2_EMESSAGE_ERROR;
            return -1;
        } else if (bytes_read == 0) {
            LOG_WARN("Server closed connection\n");
            *response_tag = SAM2_EMESSAGE_ERROR;
            return -1;
        } else {
successful_read:
            *response_tag = SAM2_EMESSAGE_PART;
            *response_length += bytes_read;

            if (header_tag == SAM2_EMESSAGE_NONE) continue; // Go back to the top of the loop to determine header tag and read message body

            if (*response_length < SAM2_HEADER_SIZE) {
                LOG_VERBOSE("Received %d/%d bytes of header\n", *response_length, SAM2_HEADER_SIZE);
                return 0;
            } else {
                // If the total number of bytes read so far is equal to the size of the message,
                // this indicates that a full message has been received. In this case, we update
                // the response_tag to the current header_tag, indicating a complete message of this type.
                if (*response_length == sam2__response_map[header_tag].message_size) {
                    *response_tag = header_tag;
                    LOG_VERBOSE("Received complete message with header '%.8s'\n", (char *) response);
                    return 1;
                } else {
                    LOG_VERBOSE("Received %d/%d bytes of message\n", *response_length, sam2__response_map[header_tag].message_size);
                    return 0;
                }
                
            }
        }
    }

    return 1;
}

SAM2_LINKAGE int sam2_client_send(sam2_socket_t sockfd, char *headerless_request, sam2_message_e request_tag) {
    if (request_tag <= SAM2_EMESSAGE_NONE) return -1; // Not in range of valid tags
    if (request_tag >= SAM2_EMESSAGE_VOID) return -1; // Not in range of valid tags

    //// If headerless_request is NULL
    //if (headerless_request) {
    //    
    //}

    // Copy the header into the request... we do it this way since type punning in C++ is UB
    memcpy(headerless_request, sam2__request_map[request_tag].header, SAM2_HEADER_SIZE);

    // Get the size of the message to be sent
    int message_size = sam2__request_map[request_tag].message_size;

    // Write the message to the socket
    int total_bytes_written = 0;
    while (total_bytes_written < message_size) {
        int bytes_written = send(sockfd, (char *) headerless_request + total_bytes_written, message_size - total_bytes_written, 0);
        if (bytes_written < 0) {
            // @todo This will busy wait
            if (SAM2_SOCKERRNO == SAM2_EAGAIN || SAM2_SOCKERRNO == EWOULDBLOCK) {
                LOG_VERBOSE("Socket is non-blocking and the requested operation would block\n");
                continue;
            } else {
                LOG_ERROR("Error writing to socket\n");
                return -1;
            }
        }
        total_bytes_written += bytes_written;
    }

    LOG_INFO("Message with header '%.8s' and size %d bytes sent successfully\n", headerless_request, message_size);
    fflush(stdout);
    return 0;
}
#endif



#if defined(SAM2_IMPLEMENTATION) && defined(SAM2_SERVER)

#define FNV_OFFSET_BASIS_64 0xCBF29CE484222325
#define FNV_PRIME_64 0x100000001B3

static uint64_t fnv1a_hash(void* data, size_t len) {
    uint64_t hash = FNV_OFFSET_BASIS_64;
    unsigned char* byte = (unsigned char*)data;
    for (size_t i = 0; i < len; i++) {
        hash ^= byte[i];
        hash *= FNV_PRIME_64;
    }
    return hash;
}

sam2_response_u *sam2__alloc_response(sam2_server_t *server, sam2_message_e tag) {
    sam2_response_u *response = (sam2_response_u *) calloc(1, sizeof(sam2_response_u));

    // @todo
    //sam2_response_u *response = NULL;
    //if (response_freelist) {
    //    response = response_freelist;
    //    response_freelist = response_freelist->next;
    //} else {
    //    assert(0); // Just check we have ample responses to send out >~32 in case of broadcast. This beats checking for NULL from an allocator every single time we make an allocation
    //}

    sam2_avl_node_t *response_node = (sam2_avl_node_t *) SAM2_MALLOC(sizeof(sam2_avl_node_t));
    response_node->key = (uint64_t) response;

    if (response_node != sam2_avl_insert(&server->_debug_allocated_response_set, response_node)) {
        SAM2_FREE(response_node);
        LOG_ERROR(
            "Somehow we allocated the same block of memory for different responses twice in one on_recv call."
            " Probably this debug bookkeeping logic is broken or the allocator."
        );
    }

    memcpy(response->buffer, sam2__response_map[tag].header, SAM2_HEADER_SIZE);

    return response;
}

static void sam2__free_response(sam2_server_t *server, void *response) {
    //response->next = response_freelist;
    //response_freelist = response;
    free(response);

    sam2_avl_node_t key_only_node = { (uint64_t) response };
    sam2_avl_node_t *node = sam2_avl_erase(&server->_debug_allocated_response_set, &key_only_node);
    if (node) {
        SAM2_FREE(node);
    }
}

int sam2__get_port_of_peer(sam2_room_t *room, uint64_t peer_id) {
    for (int i = 0; i < SAM2_PORT_MAX; i++) {
        if (room->peer_ids[i] == peer_id) {
            return i;
        }
    }

    return -1;
}

static inline void on_close_handle(uv_handle_t *handle) {
    free(handle);
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    #ifdef _WIN32
    typedef ULONG buf_len_t;
    #else
    typedef size_t buf_len_t;
    #endif

    buf->base = (char*) malloc(suggested_size);
    buf->len = (buf_len_t) suggested_size;
}

// Remove room from array by replacing with last element
static void sam2__remove_room(sam2_server_t *server_data, sam2_room_t *room) {
    int64_t i = room - server_data->rooms;

    // This check avoids aliasing issues with memcpy which clang swaps in here
    if (i != server_data->room_count - 1) {
        server_data->rooms[i] = server_data->rooms[server_data->room_count - 1];
    }

    // @todo Kick everyone in room

    --server_data->room_count;
}

static void sam2__client_destroy(uv_handle_t *client) {
    client_data_t *client_data = (client_data_t *) client->data;
    sam2_server_t *server_data = client_data->sig_server;

    if (client_data->timer) {
        uv_close((uv_handle_t *) client_data->timer, on_close_handle);
        client_data->timer = NULL;
    }

    free(client_data);
    client->data = NULL;

    SAM2_FREE(client);
}

static void on_socket_closed(uv_handle_t *handle) {
    LOG_INFO("A socket closed\n");
    uv_tcp_t *client = (uv_tcp_t *) handle;

    if (client->data) {
        client_data_t *client_data = (client_data_t *) client->data;
        sam2_server_t *server_data = client_data->sig_server;

        if (client_data->hosted_room) {
            sam2__remove_room(server_data, client_data->hosted_room);
            LOG_INFO("Removed room '%s' owner %" PRIx64 " disconnected\n", client_data->hosted_room->name, client_data->peer_id);
        }

        struct sam2_node key_only_node = { client_data->peer_id };
        struct sam2_node *node = sam2_avl_erase(&server_data->peer_id_map, &key_only_node);

        if (node) {
            SAM2_FREE(node);
        }
    } else {
        LOG_ERROR("A client didn't have any data pointer associated with it\n");
    }

    sam2__client_destroy(handle);
}

typedef struct {
    uv_write_t req;
    sam2_server_t *server;
} sam2_ext_write_t;

static void on_write(uv_write_t *req, int status) {
    if (status) {
        fprintf(stderr, "uv_write error: %s\n", uv_strerror(status));
    }

    // Generally this is a legal cast even in old compilers [ISO C11, section 6.5.2.3]
    sam2_ext_write_t *ext_req = (sam2_ext_write_t *) req;

    sam2__free_response(ext_req->server, req->data);

    free(req);
}

// This procedure owns the lifetime of response
static void sam2__write_response(uv_stream_t *client, sam2_response_u *response) {
    client_data_t *client_data = (client_data_t *) client->data;
    sam2_server_t *server = client_data->sig_server;

    sam2_avl_node_t key_only_node = { (uint64_t) response };

    sam2_avl_node_t *node = sam2_avl_erase(&server->_debug_allocated_response_set, &key_only_node);
    if (node) {
        SAM2_FREE(node);
    } else {
        LOG_ERROR(
            "The memory for a response sent was reused within the same on_recv call. This is almost certainly an error. If you are"
            " broadcasting a message you have to individually allocate each response since libuv sends them asynchronously\n"
        );
    }

    sam2_message_e tag = SAM2_EMESSAGE_MAKE;
    for (; tag < SAM2_EMESSAGE_VOID; tag++) {
        int found_matching_header = memcmp(response->buffer, sam2__response_map[tag].header, SAM2_HEADER_SIZE) == 0;
        if (found_matching_header) break;
    }

    if (tag == SAM2_EMESSAGE_VOID) {
        LOG_ERROR("We tried to send a response with invalid header to a client '%.8s'\n", (char *) response);
        sam2__free_response(server, response);
        return;
    }

    uv_buf_t buffer;
    buffer.len = sam2__response_map[tag].message_size;
    buffer.base = (char *) response;

    sam2_ext_write_t *write_req = (sam2_ext_write_t*) malloc(sizeof(sam2_ext_write_t));
    write_req->req.data = response;
    write_req->server = server;

    int status = uv_write((uv_write_t *) write_req, client, &buffer, 1, on_write);
    if (status < 0) {
        LOG_ERROR("uv_write error: %s\n", uv_strerror(status));
    }
}

static void on_write_error(uv_write_t *req, int status) {
    uv_stream_t *client = (uv_stream_t *) req->data;
    client_data_t *client_data = (client_data_t *) client->data;

    if (status < 0) {
        LOG_WARN("Failed to send error message to client: %s\n", uv_strerror(status));
        // @todo Probably just close the connection here
    } else {
        LOG_INFO("Sent error response to client %016" PRIx64 "\n", client_data->peer_id);
    }

    free(req);
}

// The lifetime of response is managed by the caller
static void write_error(uv_stream_t *client, sam2_error_response_t *response) {
    //client_data_t *client_data = (client_data_t *) client->data;

    if (memcmp(sam2__response_map[SAM2_EMESSAGE_ERROR].header, response->header, SAM2_HEADER_SIZE) != 0) {
        LOG_ERROR("We tried to send a error response with invalid header to a client '%.8s'\n", (char *) response->header);
        return;
    }

    uv_buf_t buffer;
    buffer.len = sam2__response_map[SAM2_EMESSAGE_ERROR].message_size;
    buffer.base = (char *) response;
    uv_write_t *write_req = (uv_write_t*) malloc(sizeof(uv_write_t));
    write_req->data = client;
    int status = uv_write(write_req, client, &buffer, 1, on_write_error);
    if (status < 0) {
        LOG_ERROR("uv_write error: %s\n", uv_strerror(status));
    }
}

static void sam2__write_fatal_error(uv_stream_t *client, sam2_error_response_t *response) {
    write_error(client, response);
    uv_close((uv_handle_t *) client, on_socket_closed);
}

// Function to dump data to a file
static void sam2__dump_data_to_file(const char *prefix, void* data, size_t len) {
    uint64_t hash = fnv1a_hash(data, len);

    char filename[64];
    snprintf(filename, sizeof(filename), "%s_%016" PRIx64 ".txt", prefix, hash);

    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open file for writing");
        return;
    }

    fwrite(data, 1, len, file);

    fclose(file);

    LOG_INFO("Data dumped to file: %s\n", filename);
}

static void on_timeout(uv_timer_t *handle) {
    // Dereferencing client should be fine here since before we free the client we close the timer which prevents this callback from triggering
    uv_stream_t *client = (uv_stream_t *) handle->data;
    client_data_t *client_data = (client_data_t *) client->data;

    // Check if client connection is still open
    if (uv_is_closing((uv_handle_t*) client)) {
        LOG_INFO("Client %" PRIx64 " connection is already closing or closed\n", client_data->peer_id);
    } else {
        LOG_WARN("Client %" PRIx64 " sent incomplete message with header '%.*s' and size %" PRId64 "\n",
            client_data->peer_id, (int)SAM2_MIN(client_data->length, SAM2_HEADER_SIZE), client_data->buffer, client_data->length);

        static sam2_error_response_t response = { 
            SAM2__ERROR_HEADER, SIG_RESPONSE_INVALID_HEADER,
             "An incomplete TCP message was received before timing out"
        };

        sam2__dump_data_to_file("IncompleteMessage", client_data->buffer, client_data->length);

        write_error(client, &response);
    }
}

static void on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    client_data_t *client_data = (client_data_t *) client->data;
    sam2_server_t *sig_server = (sam2_server_t *) client_data->sig_server;

    if (sig_server->_debug_allocated_response_set != NULL) {
        LOG_ERROR("We had allocated unsent responses this means we probably leaked memory handling the last response\n");
        kavll_free(sam2_avl_node_t, head, sig_server->_debug_allocated_response_set, SAM2_FREE);
        sig_server->_debug_allocated_response_set = NULL;
    }

    LOG_VERBOSE("nread=%lld\n", (long long int)nread);
    if (nread < 0) {
        // If the client closed the socket
        if (nread != UV_EOF) {
            LOG_INFO("Read error %s\n", uv_err_name((int) nread));
        }

        LOG_VERBOSE("Got EOF\n");
        uv_timer_stop(client_data->timer);

        uv_close((uv_handle_t *) client, on_socket_closed);

        goto cleanup;
    }

    for (int64_t remaining = nread; remaining > 0;) {
        client_data->request_tag = SAM2_EMESSAGE_PART;

        // We first need to read the header as that is how we infer the total size of the message
        if (client_data->length < SAM2_HEADER_SIZE) {
            int64_t consumed = SAM2_MIN(remaining, SAM2_HEADER_SIZE - client_data->length);
            memcpy(client_data->buffer + client_data->length, buf->base + (nread - remaining), consumed);

            remaining -= consumed;
            client_data->length += consumed;
        }

        // Read as much data as we can for the associated message for the given header
        if (client_data->length >= SAM2_HEADER_SIZE) {
            sam2_message_e tag;
            for (tag = SAM2_EMESSAGE_MAKE; tag < SAM2_EMESSAGE_VOID; tag++) {
                if (memcmp(client_data->buffer, sam2__request_map[tag].header, 8) == 0) {
                    int64_t num_bytes_remaining_in_message_size = sam2__request_map[tag].message_size - client_data->length;
                    int64_t consumed = SAM2_MIN(num_bytes_remaining_in_message_size, remaining);
                    memcpy(client_data->buffer + client_data->length, buf->base + (nread - remaining), consumed);

                    remaining -= consumed;
                    client_data->length += consumed;

                    if (client_data->length > sam2__request_map[tag].message_size) {
                        LOG_ERROR("Message framing failed when parsing message with header '%.8s'\n", client_data->buffer);

                        static sam2_error_response_t response = { 
                            SAM2__ERROR_HEADER,
                            SAM2_RESPONSE_SERVER_ERROR,
                            "Message framing failed on the server this is bad"
                        };

                        sam2__write_fatal_error(client, &response);
                        goto cleanup;
                    } else if (client_data->length == sam2__request_map[tag].message_size) {
                        client_data->request_tag = tag;
                    } else {
                        client_data->request_tag = client_data->length == sam2__request_map[tag].message_size ? tag : SAM2_EMESSAGE_PART;
                    }

                    break;
                }
            }

            // If no associated header
            if (tag == SAM2_EMESSAGE_VOID) {
                LOG_INFO("Client %" PRIx64 " sent invalid header '%.8s'\n", client_data->peer_id, client_data->buffer);
                static sam2_error_response_t response = { SAM2__ERROR_HEADER, SIG_RESPONSE_INVALID_HEADER };
                sam2__write_fatal_error(client, &response);
                goto cleanup;
            }
        }

        LOG_VERBOSE("client_data->length=%" PRId64 "\n", client_data->length);

        // If we have a complete request to process
        if (client_data->request_tag != SAM2_EMESSAGE_PART) {
            LOG_INFO("Client %" PRIx64 " sent message with header '%.8s'\n", client_data->peer_id, client_data->buffer);

            // Send the appropriate response
            switch(client_data->request_tag) {
            case SAM2_EMESSAGE_LIST: {
                sam2_room_list_response_t *response = &sam2__alloc_response(sig_server, client_data->request_tag)->room_list_response;

                response->server_room_count = sig_server->room_count;
                response->room_count = SAM2_MIN((int64_t) SAM2_ARRAY_LENGTH(response->rooms), sig_server->room_count); 
                memcpy(response->rooms, sig_server->rooms, sig_server->room_count * sizeof(sig_server->rooms[0]));

                client_data->list_request_rooms_sent_so_far += response->room_count;
                // @todo Send remaining rooms if there are more than 128 bookkeep in client data accordingly

                sam2__write_response(client, (sam2_response_u *) response);
                break;
            }
            case SAM2_EMESSAGE_MAKE: {
                sam2_room_make_message_t *request = &client_data->room_make_request;

                if (sig_server->room_count + 1 > sig_server->room_capacity) {
                    LOG_WARN("Out of rooms\n");
                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_SERVER_ERROR, "Out of rooms"};
                    write_error((uv_stream_t *) client, &response);
                    goto finished_processing_last_message;
                }

                // @todo Add validation for room uniqueness and data permissions on flags, not having multiple rooms with the same name, etc
                if (client_data->hosted_room) {
                    LOG_INFO("Client %" PRIx64 " attempted to make a room while already hosting one replacing it with the new requested one\n", client_data->peer_id);

                    sam2__remove_room(sig_server, client_data->hosted_room);
                }

                sam2_room_t *new_room = &sig_server->rooms[sig_server->room_count++];
                client_data->hosted_room = new_room;

                request->room.peer_ids[SAM2_AUTHORITY_INDEX] = client_data->peer_id;

                LOG_VERBOSE("Copying &request->room:%p into room+sig_server->room_count:%p room_count+1:%lld\n", &request->room, new_room, (long long int)sig_server->room_count);
                memcpy(new_room, &request->room, sizeof(*new_room));
                new_room->name[sizeof(new_room->name) - 1] = '\0';
                new_room->turn_hostname[sizeof(new_room->turn_hostname) - 1] = '\0';
                new_room->flags |= SAM2_FLAG_ROOM_IS_INITIALIZED;

                sam2_room_make_message_t *response = (sam2_room_make_message_t *) sam2__alloc_response(sig_server, client_data->request_tag);
                memcpy(&response->room, new_room, sizeof(*new_room));

                sam2__write_response(client, (sam2_response_u *) response);

                break;
            }
            case SAM2_EMESSAGE_JOIN: {
                // The logic in here is complicated because this message aliases many different operations...
                // keeping the message structure uniform simplifies the client perspective in my opinion

                sam2_room_join_message_t *request = &client_data->room_join_request;

                request->room.name[sizeof(request->room.name) - 1] = '\0';
                //memcpy(buffer.base, request, sizeof(*request));

                sam2_room_t *associated_room = sam2__find_hosted_room(sig_server, &request->room);

                if (!associated_room) {
                    LOG_INFO("Client attempted to join non-existent room with authority %" PRIx64 "\n", request->room.peer_ids[SAM2_AUTHORITY_INDEX]);
                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_ROOM_DOES_NOT_EXIST, "Room not found"};
                    write_error((uv_stream_t *) client, &response);
                    goto finished_processing_last_message;
                }

//                for (int p = 0; p < SAM2_ARRAY_LENGTH(associated_room->peer_ids); p++) {
//                    if (associated_room->peer_ids[p] == client_p) {
//
//                    }
//                }

                if (client_data->peer_id == associated_room->peer_ids[SAM2_AUTHORITY_INDEX]) {
                    // If Authority made a state change
                    int p_modified = -1;
                    for (int p = 0; p < SAM2_PORT_MAX; p++) {
                        if (request->room.peer_ids[p] != associated_room->peer_ids[p]) {
                            LOG_INFO("Peer on port %d for room '%s' changed from %" PRIx64 " to %" PRIx64 "\n",
                                p, associated_room->name, associated_room->peer_ids[p], request->room.peer_ids[p]);

                            if (p_modified != -1) {
                                // @todo This might be a bit too strict
                                LOG_INFO("Authority %" PRIx64 " attempted to change more than one port at a time\n", client_data->peer_id);
                                static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_SERVER_ERROR, "Can only change one port at a time"};
                                write_error((uv_stream_t *) client, &response);
                                goto finished_processing_last_message;
                            }

                            p_modified = p;
                            associated_room->peer_ids[p] = request->room.peer_ids[p];
                        }

                        if (request->room.peer_ids[p] == request->room.peer_ids[SAM2_AUTHORITY_INDEX]) {
                            LOG_INFO("Authority %" PRIx64 " erroneously requested to join on port %d for their own room '%s'\n", client_data->peer_id, p, associated_room->name);
                            static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_SERVER_ERROR, "Authority cannot join their own room"};
                            write_error((uv_stream_t *) client, &response);
                            goto finished_processing_last_message;
                        }
                    }

                    if (p_modified == -1) {
                        LOG_INFO("Authority %" PRIx64 " requested to change state but no ports were modified\n", client_data->peer_id);
                        static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_INVALID_ARGS, "Requested to change state but no ports were modified"};
                        write_error((uv_stream_t *) client, &response);
                        goto finished_processing_last_message;
                    }

                    if (request->room.peer_ids[SAM2_AUTHORITY_INDEX] == SAM2_PORT_AVAILABLE) {
                        LOG_INFO("Authority %" PRIx64 " abandoned the room '%s'\n", client_data->peer_id, associated_room->name);
                        for (int p = 0; p < SAM2_PORT_MAX; p++) {
                            request->room.peer_ids[SAM2_AUTHORITY_INDEX] = SAM2_PORT_AVAILABLE;
                        }

                        sam2__remove_room(sig_server, associated_room);
                    }

                    LOG_INFO("Broadcasting new room state for room %s\n", associated_room->name);
                    for (int p = 0; p < SAM2_PORT_MAX; p++) {
                        if (associated_room->peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;

                        LOG_INFO("Sending room state change for room '%s' to peer %" PRIx64 "\n", associated_room->name, associated_room->peer_ids[p]);

                        uv_tcp_t *client_socket = sam2__find_client(sig_server, request->room.peer_ids[p]);

                        if (client_socket) {
                            sam2_room_join_message_t *response = (sam2_room_join_message_t *) sam2__alloc_response(sig_server, client_data->request_tag);
                            response->peer_id = client_data->peer_id;
                            memcpy(&response->room, &request->room, sizeof(*associated_room));
                            //memcpy(response->room_secret, request->room_secret, sizeof(response->room_secret));

                            sam2__write_response((uv_stream_t*) client_socket, (sam2_response_u *) response);
                        } else {
                            // @todo
                            LOG_WARN("No peer found for id %" PRIx64 "\n", request->room.peer_ids[p]);
                        }
                    }
                } else {
                    // Client requests state change by authority
                    int p_join = sam2__get_port_of_peer(&request->room, client_data->peer_id);
                    {
                        int p_in = sam2__get_port_of_peer(associated_room, client_data->peer_id);
                        if (p_join == -1) {
                            if (p_in == -1) {
                                LOG_WARN("Client sent state change request for a room they are not in\n"); // @todo Add generic check and change this to an assert
                                static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_INVALID_ARGS, "Invalid state change request"};
                                write_error((uv_stream_t *) client, &response);
                                goto finished_processing_last_message;
                            } else {
                                // Peer left
                                if (request->room.peer_ids[p_in] != SAM2_PORT_AVAILABLE) {
                                    LOG_WARN("Convention violation: Client did not set the port to SAM2_PORT_AVAILABLE when leaving\n");
                                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_INVALID_ARGS, "When leaving a room, set the port to SAM2_PORT_AVAILABLE"};
                                    write_error((uv_stream_t *) client, &response);
                                    goto finished_processing_last_message;
                                }
                            }
                        } else {
                            if (p_in == -1) {
                                // Peer joined
                                if (associated_room->peer_ids[p_join] != SAM2_PORT_AVAILABLE) {
                                    LOG_WARN("Client attempted to join on an unavailable port\n");
                                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_PORT_NOT_AVAILABLE, "Port is currently unavailable"};
                                    write_error((uv_stream_t *) client, &response);
                                    goto finished_processing_last_message;
                                }
                            } else {
                                if (p_in != p_join) {
                                    LOG_WARN("Client changed port, which is not allowed\n");
                                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_INVALID_ARGS, "Peer cannot change ports; Instead leave and rejoin"};
                                    write_error((uv_stream_t *) client, &response);
                                    goto finished_processing_last_message;
                                }
                            }
                        }

                        // Check that the client didn't change any ports other than the one they joined on or left on
                        for (int p = 0; p < SAM2_PORT_MAX; p++) {
                            if (p != p_join && p != p_in && request->room.peer_ids[p] != associated_room->peer_ids[p]) {
                                LOG_WARN("Client attempted to change ports other than the one they joined on or left on\n");
                                static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_INVALID_ARGS, "Invalid state change request"};
                                write_error((uv_stream_t *) client, &response);
                                goto finished_processing_last_message;
                            }
                        }
                    }

                    LOG_INFO("Forwarding join request to room authority\n");
                    uv_tcp_t *authority = sam2__find_client(sig_server, associated_room->peer_ids[SAM2_AUTHORITY_INDEX]);
                    if (!authority) {
                        LOG_ERROR("Room authority not found even though room was associated. This is a bug\n");
                        static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_SERVER_ERROR, "Room authority not found"};
                        write_error((uv_stream_t *) client, &response);
                        goto finished_processing_last_message;
                    }

                    sam2_room_join_message_t *response = (sam2_room_join_message_t *) sam2__alloc_response(sig_server, client_data->request_tag);
                    memcpy(&response->room, associated_room, sizeof(*associated_room));
                    response->peer_id = client_data->peer_id;
                    if (p_join != -1) {
                        response->room.peer_ids[p_join] = client_data->peer_id;
                    }
                    memcpy(response->room_secret, request->room_secret, sizeof(response->room_secret));
                    sam2__write_response((uv_stream_t*) authority, (sam2_response_u *) response);
                }

                break;
            }
            case SAM2_EMESSAGE_ACKJ: {
                sam2_room_acknowledge_join_message_t *request = &client_data->acknowledge_room_join_message;

                sam2_room_t *associated_room = sam2__find_hosted_room(sig_server, &request->room);
                client_data_t *joiner_client_data = (client_data_t *) sam2__find_client_data(sig_server, request->joiner_peer_id);

                // @todo Some of these should just always be checked for any request I think I can write generic code if I reorder some of the fields in the structs
                if (!associated_room) {
                    LOG_INFO("Client attempted to acknowledge peer %" PRIx64 " in non-existent room\n", request->joiner_peer_id);
                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_ROOM_DOES_NOT_EXIST, "Acknowledged peer joining non-existant room"};
                    write_error((uv_stream_t *) client, &response);
                    goto finished_processing_last_message;
                } else if (!joiner_client_data) {
                    LOG_INFO("Client attempted to acknowledge non-existent peer %" PRIx64 " joining room '%s'\n", request->joiner_peer_id, associated_room->name);
                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_PEER_DOES_NOT_EXIST, "Acknowledged non-existant peer joining room"};
                    write_error((uv_stream_t *) client, &response);
                    goto finished_processing_last_message;
                } else if (sam2__get_port_of_peer(associated_room, request->sender_peer_id) == -1) {
                    LOG_INFO("Client attempted to acknowledge peer %" PRIx64 " joining room '%s', but %" PRIx64 " is not in the room\n", request->joiner_peer_id, associated_room->name, request->sender_peer_id);
                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_PEER_NOT_IN_ROOM, "Acknowledged join while not in room"};
                    write_error((uv_stream_t *) client, &response);
                    goto finished_processing_last_message;
                } else if (sam2__get_port_of_peer(associated_room, request->joiner_peer_id) == -1) {
                    LOG_INFO("Client attempted to acknowledge peer %" PRIx64 " joining room '%s', but %" PRIx64 " is not in the room\n", request->joiner_peer_id, associated_room->name, request->joiner_peer_id);
                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_PEER_NOT_IN_ROOM, "Acknowledged join of peer who is not in the room"};
                    write_error((uv_stream_t *) client, &response);
                    goto finished_processing_last_message;
                } else {
                    LOG_INFO("Client acknowledged peer %" PRIx64 " joining room '%s'\n", request->joiner_peer_id, associated_room->name);
                    sam2_room_acknowledge_join_message_t response_value = {0};
                    memcpy(&response_value, request, sizeof(response_value));

                    response_value.sender_peer_id = client_data->peer_id;

                    if (client_data->peer_id == associated_room->peer_ids[SAM2_AUTHORITY_INDEX]) {
                        LOG_INFO("Authority %" PRIx64 " received all acknowledgements for %" PRIx64 " joining room '%s'. Broadcasting this to all peers\n", client_data->peer_id, request->joiner_peer_id, associated_room->name);
                        for (int p = 0; p < SAM2_PORT_MAX; p++) {
                            if (associated_room->peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;

                            LOG_INFO("Sending room state change for room %s to peer %" PRIx64 "\n", associated_room->name, associated_room->peer_ids[p]);

                            uv_tcp_t *peer_socket = sam2__find_client(sig_server, associated_room->peer_ids[p]);

                            if (peer_socket) {
                                sam2_room_acknowledge_join_message_t *response = (sam2_room_acknowledge_join_message_t *) sam2__alloc_response(sig_server, client_data->request_tag);
                                memcpy(response, &response_value, sizeof(*response));

                                sam2__write_response((uv_stream_t*) peer_socket, (sam2_response_u *) response);
                            } else {
                                // @todo This should just kick everyone and delete the room
                                LOG_ERROR("No peer found for id %" PRIx64 "\n", associated_room->peer_ids[p]);
                                sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_SERVER_ERROR, "Could not find peer this is a sam2 bug"};
                                write_error((uv_stream_t *) client, &response);
                                goto finished_processing_last_message;
                            }
                        }
                    } else {
                        LOG_INFO("Client %" PRIx64 " acknowledged peer %" PRIx64 " joining room '%s'\n", client_data->peer_id, request->joiner_peer_id, associated_room->name);
                        uv_tcp_t *authority = sam2__find_client(sig_server, associated_room->peer_ids[SAM2_AUTHORITY_INDEX]);
                        sam2_room_acknowledge_join_message_t *response = (sam2_room_acknowledge_join_message_t *) sam2__alloc_response(sig_server, client_data->request_tag);
                        memcpy(response, &response_value, sizeof(*response));
                        sam2__write_response((uv_stream_t *) authority, (sam2_response_u *) response);
                    }
                }
                break;
            }
            case SAM2_EMESSAGE_SIGNAL: {
                // Clients forwarding sdp's between eachother
                sam2_signal_message_t *request = &client_data->signal_message;

                if (request->peer_id == client_data->peer_id) {
                    LOG_INFO("Client attempted to send sdp information to themselves\n");
                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_CANNOT_SIGNAL_SELF, "Cannot signal self"};
                    write_error((uv_stream_t *) client, &response);
                    goto finished_processing_last_message;
                }

                uv_tcp_t *peer = sam2__find_client(sig_server, request->peer_id);

                LOG_INFO("Forwarding sdp information from peer %" PRIx64 " to peer %" PRIx64 "\n", client_data->peer_id, request->peer_id);
                if (!peer) {
                    LOG_WARN("Forwarding failed Client attempted to send sdp information to non-existent peer\n");
                    static sam2_error_response_t response = { SAM2__ERROR_HEADER, SAM2_RESPONSE_PEER_DOES_NOT_EXIST, "Peer not found"};
                    write_error((uv_stream_t *) client, &response);
                    goto finished_processing_last_message;
                }

                sam2_signal_message_t *response = (sam2_signal_message_t *) sam2__alloc_response(sig_server, client_data->request_tag);

                memcpy(response, request, sizeof(*response));
                response->peer_id = client_data->peer_id;

                sam2__write_response((uv_stream_t *) peer, (sam2_response_u *) response);
                break;
            }
            case SAM2_EMESSAGE_ERROR: {
                LOG_INFO("Peer %" PRIx64 " sent error message to Peer %" PRIx64 "\n", client_data->peer_id, client_data->error_response.peer_id);
                uv_tcp_t *target_peer = sam2__find_client(sig_server, client_data->error_response.peer_id);

                if (target_peer) {
                    sam2_error_response_t *response = (sam2_error_response_t *) sam2__alloc_response(sig_server, client_data->request_tag);
                    memcpy(response, &client_data->error_response, sizeof(*response));
                    sam2__write_response((uv_stream_t *) target_peer, (sam2_response_u *) response);
                } else {
                    LOG_WARN("Peer %" PRIx64 " not found\n", client_data->error_response.peer_id);
                }
                break;
            }
            default:
                LOG_FATAL("A dumb programming logic error was made or something got corrupted if you ever get here");
                //__builtin_unreachable();
            }

finished_processing_last_message:
            uv_timer_stop(client_data->timer);

            client_data->request_tag = SAM2_EMESSAGE_NONE;
            client_data->length = 0;
        }
    }

cleanup:

    // This is basically a courtesy for clients to warn them they only sent a partial message
    if (client_data->request_tag == SAM2_EMESSAGE_PART) {
        uv_timer_start(client_data->timer, on_timeout, 500, 0);  // 0.5 seconds
    }

    if (buf->base) {
        LOG_VERBOSE("Freeing buf\n");
        free(buf->base);
    }
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        // error handling
        return;
    }

    uv_tcp_t *client = (uv_tcp_t*) calloc(1, sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), client);

    if (uv_accept(server, (uv_stream_t*) client) == 0) {

        client_data_t *client_data = (client_data_t *) calloc(1, sizeof(client_data_t));
        sam2_server_t *server_data = (sam2_server_t *) server->data;
        
        { // Create a peer id based on hashed IP address mixed with a counter
            struct sockaddr_storage name;
            int len = sizeof(name);
            uv_tcp_getpeername(client, (struct sockaddr*) &name, &len);

            if (name.ss_family == AF_INET) { // IPv4
                struct sockaddr_in* s = (struct sockaddr_in*)&name;
                client_data->peer_id = fnv1a_hash(&s->sin_addr, sizeof(s->sin_addr));
            } else if (name.ss_family == AF_INET6) { // IPv6
                struct sockaddr_in6* s = (struct sockaddr_in6*)&name;
                client_data->peer_id = fnv1a_hash(&s->sin6_addr, sizeof(s->sin6_addr));
            }

            static uint64_t counter = 0;
            client_data->peer_id ^= counter++;
        }

        client_data->timer = (uv_timer_t *) malloc(sizeof(uv_timer_t));
        client_data->timer->data = client;

        client_data->sig_server = (sam2_server_t *) server->data;
        client->data = client_data;

        sam2_avl_node_t *node = (sam2_avl_node_t *) calloc(1, sizeof(sam2_avl_node_t));
        node->key = client_data->peer_id;
        node->client = client;
        sam2_avl_insert(&server_data->peer_id_map, node);

        uv_timer_init(uv_default_loop(), client_data->timer);
        // Reading the request sent by the client
        int status = uv_read_start((uv_stream_t*) client, alloc_buffer, on_read);
        if (status < 0) {
            LOG_WARN("Failed to connnect to client %" PRIx64 " uv_read_start error: %s\n", client_data->peer_id, uv_strerror(status));
        } else {
            LOG_INFO("Successfully connected to client %" PRIx64 "\n", client_data->peer_id);

            sam2_connect_message_t *connect_message = (sam2_connect_message_t *) sam2__alloc_response(server_data, SAM2_EMESSAGE_CONN);
            memcpy(connect_message->header, sam2_conn_header, SAM2_HEADER_SIZE);

            connect_message->peer_id = client_data->peer_id;

            sam2__write_response((uv_stream_t*) client, (sam2_response_u *) connect_message);
        }
    } else {
        uv_close((uv_handle_t*) client, on_socket_closed);
    }
}

#if defined(SAM2_EXECUTABLE)
static void sam2__kavll_free_node_and_data(sam2_avl_node_t *node) {
    uv_close((uv_handle_t *) node->client, sam2__client_destroy);
    SAM2_FREE(node);
}

static void sam2__server_on_close(uv_handle_t* handle) {
    LOG_INFO("Server closing\n");

    sam2_server_t *server_data = (sam2_server_t *) handle->data;
    free(server_data);

    //free(handle); // @todo This is on main()'s stack
}

static void on_signal(uv_signal_t *handle, int signum) {
    uv_tcp_t *server = (uv_tcp_t *) handle->data;
    sam2_server_t *server_data = (sam2_server_t *) server->data;
    kavll_free(sam2_avl_node_t, head, server_data->peer_id_map, sam2__kavll_free_node_and_data);
    server_data->peer_id_map = NULL;
    uv_close((uv_handle_t*) server, sam2__server_on_close);
    uv_stop(handle->loop);
}

// Secret knowledge hidden within libuv's test folder
#define ASSERT(expr) if ((expr)) exit(1);
static void close_walk_cb(uv_handle_t* handle, void* arg) {
    if (!uv_is_closing(handle)) {
        uv_close(handle, NULL);
    }
}

static void close_loop(uv_loop_t* loop) {
    uv_walk(loop, close_walk_cb, NULL);
    uv_run(loop, UV_RUN_DEFAULT);
}

/* This macro cleans up the event loop. This is used to avoid valgrind
 * warnings about memory being "leaked" by the event loop.
 */
#define MAKE_VALGRIND_HAPPY(loop)                   \
  do {                                              \
    close_loop(loop);                               \
    ASSERT(0 == uv_loop_close(loop));               \
    uv_library_shutdown();                          \
  } while (0)

int main() {
    uv_loop_t *loop = uv_default_loop();

    int64_t room_capacity = 65536;
    sam2_server_t *sig_server = (sam2_server_t *) calloc(1, sizeof(sam2_server_t) + sizeof(sam2_room_t) * room_capacity);
    sig_server->room_capacity = room_capacity;
    //sig_server->rooms_internal = calloc(room_capacity, sizeof(sig_room_internal_t));
    uv_tcp_t server;
    server.data = sig_server;
    uv_tcp_init(loop, &server);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", SAM2_SERVER_DEFAULT_PORT, &addr);

    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    int r = uv_listen((uv_stream_t*) &server, SAM2_DEFAULT_BACKLOG, on_new_connection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }

    uv_signal_t sig;
    uv_signal_init(loop, &sig);
    sig.data = &server;
    uv_signal_start(&sig, on_signal, SIGINT);

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);
    MAKE_VALGRIND_HAPPY(uv_default_loop());
    return 0;
}

#endif
#endif



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


// You can't use packing pragmas portably this is the next best thing
// If these fail then this server won't be binary compatible with the protocol and would fail horrendously
// Resort to packing pragmas until these succeed if you run into this issue yourself
SAM2_STATIC_ASSERT(SAM2_BYTEORDER_ENDIAN == SAM2_BYTEORDER_LITTLE_ENDIAN, "Platform is big-endian which is unsupported");
SAM2_STATIC_ASSERT(sizeof(sam2_room_t) == 64 + 64 + sizeof(uint64_t) + 8*sizeof(uint64_t) + sizeof(uint64_t), "sam2_room_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_make_message_t) == 8 + sizeof(sam2_room_t), "sam2_room_make_message_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sig_room_list_request_t) == 8, "sig_room_list_request_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_list_response_t) == 8 + sizeof(int64_t) + sizeof(int64_t) + 8 * sizeof(sam2_room_t), "sam2_room_list_response_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_join_message_t) == 8 + 8 + 64 + sizeof(sam2_room_t), "sam2_room_join_message_t is not packed");
#endif
