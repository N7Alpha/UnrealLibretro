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
#include <stdlib.h> // @todo Remove when I remove malloc and free stuff
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
#define sam2_sigx_header  "S" "I" "G" "X" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"
#define SAM2_SIGX_HEADER {'S','I','G','X',    '0' + SAM2_VERSION_MAJOR, '.',    '0' + SAM2_VERSION_MINOR, 'r'}
#define sam2_fail_header  "F" "A" "I" "L" SAM2__STR(SAM2_VERSION_MAJOR) "." SAM2__STR(SAM2_VERSION_MINOR) "r"
#define SAM2_FAIL_HEADER {'F','A','I','L',    '0' + SAM2_VERSION_MAJOR, '.',    '0' + SAM2_VERSION_MINOR, 'r'}

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
#define SAM2_STATIC_ASSERT(cond, _) extern int sam2__static_assertion_##__COUNTER__[(cond) ? 1 : -1]
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    // C99 or later
    #define SAM2_RESTRICT restrict
#elif defined(__cplusplus) && __cplusplus >= 201103L
    // C++11 or later
    #define SAM2_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
    // GCC or Clang
    #define SAM2_RESTRICT __restrict__
#elif defined(_MSC_VER) && _MSC_VER >= 1400
    // Microsoft Visual C++ (MSVC)
    #define SAM2_RESTRICT __restrict
#elif defined(__INTEL_COMPILER)
    // Intel C++ Compiler (ICC)
    #define SAM2_RESTRICT restrict
#else
    // restrict keyword not available
    #define SAM2_RESTRICT
#endif

#if defined(_MSC_VER)
#define SAM2_UNUSED __pragma(warning(suppress: 4505))
#elif defined(__GNUC__) || defined(__clang__)
#define SAM2_UNUSED __attribute__((unused))
#else
#define SAM2_UNUSED
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
#define SAM2_FLAG_NO_FIXED_PORT            0b00000001ULL // Clients aren't limited to setting input on bound port
#define SAM2_FLAG_ALLOW_SHOW_IP            0b00000010ULL
#define SAM2_FLAG_FORCE_TURN               0b00000100ULL
#define SAM2_FLAG_SPECTATOR                0b00001000ULL
#define SAM2_FLAG_ROOM_NEEDS_AUTHORIZATION 0b00010000ULL
#define SAM2_FLAG_AUTHORITY_IPv6           0b00100000ULL
#define SAM2_FLAG_ROOM_IS_NETWORK_HOSTED   0b01000000ULL

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
#define SAM2_RESPONSE_PEER_NOT_IN_ROOM         -15
#define SAM2_RESPONSE_CANNOT_SIGNAL_SELF       -16
#define SAM2_RESPONSE_VERSION_MISMATCH         -17
#define SAM2_RESPONSE_INVALID_ENCODE_TYPE      -18

#define SAM2_PORT_AVAILABLE                   0
#define SAM2_PORT_UNAVAILABLE                 1
#define SAM2_PORT_SENTINELS_MAX               SAM2_PORT_UNAVAILABLE

#define SAM2_PORT_MAX 8
#define SAM2_AUTHORITY_INDEX SAM2_PORT_MAX

// All data is sent in little-endian format
// All strings are utf-8 encoded unless stated otherwise... @todo Actually I should just add _utf8 if the field isn't ascii
// Packing of structs is asserted at compile time since packing directives are compiler specific

typedef struct sam2_room {
    char name[64]; // Unique name that identifies the room
    uint64_t flags;
    char core_and_version[32];
    uint64_t rom_hash_xxh64;
    uint64_t peer_ids[SAM2_PORT_MAX+1]; // Must be unique per port (including authority)
} sam2_room_t;

// This is a test for identity not equality
static int sam2_same_room(sam2_room_t *a, sam2_room_t *b) {
    return a && b && a->peer_ids[SAM2_AUTHORITY_INDEX] == b->peer_ids[SAM2_AUTHORITY_INDEX]
           && strncmp(a->name, b->name, sizeof(a->name)) == 0;
}

static int sam2_get_port_of_peer(sam2_room_t *room, uint64_t peer_id) {
    for (int i = 0; i < SAM2_PORT_MAX+1; i++) {
        if (room->peer_ids[i] == peer_id) {
            return i;
        }
    }

    return -1;
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
    sam2_room_t room;

    uint64_t peer_id; // Peer id of sender set by sam2 server
    char room_secret[64]; // optional peers know this so use it to determine authorization?
} sam2_room_join_message_t;

typedef struct sam2_connect_message {
    char header[8];
    uint64_t peer_id;

    uint64_t flags;
} sam2_connect_message_t;

typedef struct sam2_signal_message {
    char header[8];
    uint64_t peer_id;

    char ice_sdp[496];
} sam2_signal_message_t;

typedef struct sam2_error_message {
    char header[8];

    int64_t code;
    char description[128];
    uint64_t peer_id;
} sam2_error_message_t;

typedef union sam2_message {
    union sam2_message *next; // Points to next element in freelist

    sam2_room_make_message_t room_make_response;
    sam2_room_list_message_t room_list_response;
    sam2_room_join_message_t room_join_response;
    sam2_connect_message_t connect_message;
    sam2_signal_message_t signal_message;
    sam2_error_message_t error_response;
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
    {sam2_sigx_header, sizeof(sam2_signal_message_t)},
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
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <io.h>
#include <windows.h>
#include <winsock2.h>
//#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sam2_socket_t;
#else
typedef int sam2_socket_t;
#endif

#if defined(SAM2_SERVER)
#include <uv.h>

typedef struct sam2_node {
    uint64_t key;
    uv_tcp_t *client;

    KAVLL_HEAD(struct sam2_node) head;
} sam2_avl_node_t;

#define SAM2__CLIENT_FLAG_CLIENT_CLOSE_DONE 0b00000001
#define SAM2__CLIENT_FLAG_TIMER_CLOSE_DONE  0b00000010
#define SAM2__CLIENT_FLAG_MASK_CLOSE_DONE   0b00000011

typedef struct sam2_client {
    uv_tcp_t tcp;

    uint64_t peer_id;

    int flags;
    uv_timer_t timer;
    int64_t rooms_sent;

    sam2_room_t *hosted_room;

    char buffer[sizeof(sam2_message_u)];
    int length;
} sam2_client_t;
SAM2_STATIC_ASSERT(offsetof(sam2_client_t, tcp) == 0, "We need this so we can cast between sam2_client_t and uv_tcp_t");

typedef struct sam2_server {
    uv_tcp_t tcp;
    uv_loop_t loop;

    sam2_message_u *message_freelist;
    int64_t _debug_allocated_messages;

    sam2_avl_node_t *_debug_allocated_message_set;

    sam2_avl_node_t *peer_id_map;
    int64_t room_count;
    int64_t room_capacity;
    sam2_room_t rooms[/*room_capacity*/];
} sam2_server_t;
SAM2_STATIC_ASSERT(offsetof(sam2_server_t, tcp) == 0, "We need this so we can cast between sam2_server_t and uv_tcp_t");

#if defined(_MSC_VER)
__pragma(warning(push))
__pragma(warning(disable: 4101 4189 4505 4127))
#elif defined(__GNUC__) || defined(__clang__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wunused-variable\"") \
_Pragma("GCC diagnostic ignored \"-Wunused-function\"") \
_Pragma("GCC diagnostic ignored \"-Wunused-but-set-variable\"")
#endif

#define sam2__cmp(p, q) (((q)->key < (p)->key) - ((p)->key < (q)->key))
KAVLL_INIT2(sam2_avl, static, struct sam2_node, head, sam2__cmp)

#if defined(_MSC_VER)
__pragma(warning(pop))
#elif defined(__GNUC__) || defined(__clang__)
_Pragma("GCC diagnostic pop")
#endif

static sam2_client_t* sam2__find_client(sam2_server_t *server, uint64_t peer_id) {
    sam2_avl_node_t key_only_node = { peer_id };
    sam2_avl_node_t *node = sam2_avl_find(server->peer_id_map, &key_only_node);

    if (node) {
        return (sam2_client_t *) node->client;
    } else {
        return NULL;
    }
}

static sam2_room_t* sam2__find_hosted_room(sam2_server_t *server, sam2_room_t *room) {
    sam2_client_t *client = sam2__find_client(server, room->peer_ids[SAM2_AUTHORITY_INDEX]);

    if (client) {
        if (sam2_same_room(room, client->hosted_room)) {
            return client->hosted_room;
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

// Note: You must allocate the memory for `server`
// ```c
// int server_size_bytes = sam2_server_create(NULL, 0);
//
// // Create server
// sam2_server_t *server = malloc(server_size_bytes);
// sam2_server_create(server, 1234);
//
// // Wait for some events
// for (int i = 0; i < 10; i++) {
//     uv_run(&server->loop, UV_RUN_NOWAIT);
// }
//
// // Start asynchronous destruction
// sam2_server_begin_destroy(server);
//
// // Do the needful
// uv_run(&server->loop, UV_RUN_DEFAULT);
// uv_loop_close(&server->loop);
// free(server);
// ```
SAM2_LINKAGE int sam2_server_create(sam2_server_t *server, int port);

SAM2_LINKAGE int sam2_server_begin_destroy(sam2_server_t *server);
#endif

// ===============================================
// == Client interface                          ==
// ===============================================

// Non-blocking trys to read a response sent by the server
// Returns negative on error, positive if there are more messages to read, and zero when you've processed the last message
// Errors can't be recovered from you must call sam2_client_disconnect and then sam2_client_connect again
SAM2_LINKAGE int sam2_client_poll(sam2_socket_t sockfd, sam2_message_u *response, char *buffer, int *buffer_length);

// Connnects to host which is either an IPv4/IPv6 Address or domain name
// Will bias IPv6 if connecting via domain name and also block
SAM2_LINKAGE int sam2_client_connect(sam2_socket_t *sockfd_ptr, const char *host, int port);

SAM2_LINKAGE int sam2_client_poll_connection(sam2_socket_t sockfd, int timeout_ms);

SAM2_LINKAGE int sam2_client_send(sam2_socket_t sockfd, char *message);
#endif // SAM2_H

#if defined(SAM2_IMPLEMENTATION)
#ifndef SAM2_CLIENT_C
#define SAM2_CLIENT_C

#include <errno.h>
#include <time.h>
#include <stdarg.h>
#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <unistd.h>
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

            if (output_size >= output_capacity-2) return -1;
            output[output_size++] = 0; // Mark the start of a zero run
            // Encode count as little endian
            output[output_size++] = (uint8_t)(count & 0xFF);
            output[output_size++] = (uint8_t)((count >> 8) & 0xFF);
        } else {
            if (output_size >= output_capacity) return -1;
            output[output_size++] = input[i]; // Copy non-zero values directly
        }
    }
    return output_size; // Return the size of the encoded data
}

// Encodes input array of uint8_t into a byte stream with RLE for zeros.
int64_t rle8_encode(const uint8_t *input, int64_t input_size, uint8_t *output) {
    return rle8_encode_capped(input, input_size, output, RLE8_ENCODE_UPPER_BOUND(input_size));
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

// Calculates the decoded size from the encoded byte stream. @todo Remove you can just roll these checks into the actual decoder
int64_t rle8_decode_size(const uint8_t* input, int64_t input_size) {
    int64_t decoded_size = 0;
    int64_t i = 0;
    while (i < input_size) {
        if (input[i] == 0) {
            i++; // Move past the zero marker
            uint16_t count = input[i] | (input[i + 1] << 8); // Decode count as little endian
            i += 2; // Move past the count bytes

            decoded_size += count;
        } else {
            decoded_size++;
            i++;
        }
    }
    return decoded_size;
}

#define SAM2__GREY    "\x1B[90m"
#define SAM2__DEFAULT "\x1B[39m"
#define SAM2__YELLOW  "\x1B[93m"
#define SAM2__RED     "\x1B[91m"
#define SAM2__WHITE   "\x1B[97m"
#define SAM2__BG_RED  "\x1B[41m"
#define SAM2__RESET   "\x1B[0m"

int sam2__terminal_supports_ansi_colors() {
#if defined(_WIN32)
    // Windows
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return 0;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) {
        return 0;
    }
#else
    // POSIX
    if (!isatty(STDOUT_FILENO)) {
        return 0;
    }

    const char *term = getenv("TERM");
    if (term == NULL || strcmp(term, "dumb") == 0) {
        return 0;
    }
#endif

    return 1;
}

//#define SAM2_LOG_WRITE(level, file, line, ...) do { printf(__VA_ARGS__); printf("\n"); } while (0); // Ex. Use print
#ifndef SAM2_LOG_WRITE
#define SAM2_LOG_WRITE_DEFINITION
#define SAM2_LOG_WRITE sam2__log_write
#endif

#define SAM2_LOG_DEBUG(...) SAM2_LOG_WRITE(0, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_INFO(...)  SAM2_LOG_WRITE(1, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_WARN(...)  SAM2_LOG_WRITE(2, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_ERROR(...) SAM2_LOG_WRITE(3, __FILE__, __LINE__, __VA_ARGS__)
#define SAM2_LOG_FATAL(...) SAM2_LOG_WRITE(4, __FILE__, __LINE__, __VA_ARGS__)

static int sam2__get_localtime(const time_t *t, struct tm *buf) {
#ifdef _WIN32
    // Windows does not have POSIX localtime_r...
    return localtime_s(buf, t) == 0 ? 0 : -1;
#else // POSIX
    return localtime_r(t, buf) != NULL ? 0 : -1;
#endif
}

// @todo This is kind of weird but I needed to be able to hide this to get Unreal Build Tool to compile it
#if defined(SAM2_LOG_WRITE_DEFINITION)
// @todo Logging is slower than I'd like it to be. I want some simple solution to this that at least applies to non-debug builds since I think those should be fast
// @enhancement Maybe use stb_sprintf instead to avoid malloc calls? Couple this with a platform write function instead of printf
static void SAM2_UNUSED sam2__log_write(int level, const char *file, int line, const char *fmt, ...) {
    const char *filename = file + strlen(file);
    while (filename != file && *filename != '/' && *filename != '\\') {
        --filename;
    }
    if (filename != file) {
        ++filename;
    }

    time_t t = time(NULL);
    struct tm lt;
    char timestamp[16];
    if (sam2__get_localtime(&t, &lt) != 0 || strftime(timestamp, 16, "%H:%M:%S", &lt) == 0) {
        timestamp[0] = '\0';
    }

    const char *prefix_fmt;
    switch (level) {
    default: //          ANSI color escape-codes  HH:MM:SS level     filename:line  |
    case 4: prefix_fmt = SAM2__WHITE SAM2__BG_RED "%s "    "FATAL "  "%11s:%-5d"   "| "; break;
    case 0: prefix_fmt = SAM2__GREY               "%s "    "DEBUG "  "%11s:%-5d"   "| "; break;
    case 1: prefix_fmt = SAM2__DEFAULT            "%s "    "INFO  "  "%11s:%-5d"   "| "; break;
    case 2: prefix_fmt = SAM2__YELLOW             "%s "    "WARN  "  "%11s:%-5d"   "| "; break;
    case 3: prefix_fmt = SAM2__RED                "%s "    "ERROR "  "%11s:%-5d"   "| "; break;
    }

    if (!sam2__terminal_supports_ansi_colors()) while (*prefix_fmt == '\x1B') prefix_fmt += 5; // Skip ANSI color-escape codes

    printf(prefix_fmt, timestamp, filename, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);

    fprintf(stdout, sam2__terminal_supports_ansi_colors() ? SAM2__RESET "\n" : "\n");
    fflush(stdout);

    if (level >= 4) {
#ifdef _WIN32
        // Break into the debugger on Windows
        if (IsDebuggerPresent()) {
            __debugbreak(); // HIT FATAL ERROR
        }
#elif defined(__has_builtin)
#if __has_builtin(__builtin_trap)
        // Break into the debugger on POSIX systems
        if (signal(SIGTRAP, SIG_IGN) != SIG_IGN) {
            __builtin_trap(); // HIT FATAL ERROR
        }
#endif
#endif
        exit(EXIT_FAILURE);
    }
}
#endif

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

    SAM2_LOG_INFO("Getting IP Address for %s...", hostname);
    // I knew this could block but it just hangs on Windows at least for a very long time before timing out @todo
    if (getaddrinfo(hostname, NULL, &hints, &res)) {
        SAM2_LOG_ERROR("Address resolution failed for %s", hostname);
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        char ipvx[INET6_ADDRSTRLEN];
        socklen_t addrlen = sizeof(struct sockaddr_storage);

        if (getnameinfo(p->ai_addr, addrlen, ipvx, sizeof(ipvx), NULL, 0, NI_NUMERICHOST) != 0) {
            SAM2_LOG_ERROR("Couldn't convert IP Address to string: ", SAM2_SOCKERRNO);
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
    struct sockaddr_storage server_addr = {0};
    // Initialize winsock / Increment winsock reference count
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        SAM2_LOG_ERROR("WSAStartup failed!");
        return -1;
    }
#endif

    char ip[INET6_ADDRSTRLEN];
    int family = sam2__resolve_hostname(host, ip); // This blocks
    if (family < 0) {
        SAM2_LOG_ERROR("Failed to resolve hostname for '%s'", host);
        return -1;
    }
    host = ip;

    sam2_socket_t sockfd = socket(family, SOCK_STREAM, 0);
    if (sockfd == SAM2_SOCKET_INVALID) {
        SAM2_LOG_ERROR("Failed to create socket");
        return -1;
    }

#ifdef _WIN32
    u_long flags = 1; // 1 for non-blocking, 0 for blocking
    if (ioctlsocket(sockfd, FIONBIO, &flags) < 0) {
        SAM2_LOG_ERROR("Failed to set socket to non-blocking mode");
        goto fail;
    }
#else
    int current_flags = fcntl(sockfd, F_GETFL, 0);
    if (current_flags < 0 || fcntl(sockfd, F_SETFL, current_flags | O_NONBLOCK) < 0) {
        SAM2_LOG_ERROR("Failed to set socket to non-blocking mode");
        goto fail;
    }
#endif

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
    return -1;
}

SAM2_LINKAGE int sam2_client_disconnect(sam2_socket_t *sockfd_ptr) {
    int status = 0;

    #ifdef _WIN32
    // Cleanup winsock / Decrement winsock reference count
    if (WSACleanup() == SOCKET_ERROR) {
        SAM2_LOG_ERROR("WSACleanup failed: %d", WSAGetLastError());
        status = -1;
    }
    #endif

    if (*sockfd_ptr != SAM2_SOCKET_INVALID) {
        if (SAM2_CLOSESOCKET(*sockfd_ptr) == SAM2_SOCKET_ERROR) {
            SAM2_LOG_ERROR("close failed: %s", strerror(errno));
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

static void sam2__sanitize_message(void *message) {
    if (!message) return;

    // Sanitize C-Strings. This will also clear extra uninitialized bytes past the null terminator
    if (memcmp(message, sam2_make_header, SAM2_HEADER_TAG_SIZE) == 0) {
        sam2_room_make_message_t *make_message = (sam2_room_make_message_t *)message;
        SAM2__SANITIZE_STRING(make_message->room.name);
    } else if (memcmp(message, sam2_list_header, SAM2_HEADER_TAG_SIZE) == 0) {
        sam2_room_list_message_t *list_message = (sam2_room_list_message_t *)message;
        SAM2__SANITIZE_STRING(list_message->room.name);
    } else if (memcmp(message, sam2_join_header, SAM2_HEADER_TAG_SIZE) == 0) {
        sam2_room_join_message_t *join_message = (sam2_room_join_message_t *)message;
        SAM2__SANITIZE_STRING(join_message->room.name);
        SAM2__SANITIZE_STRING(join_message->room_secret);
    } else if (   memcmp(message, sam2_sign_header, SAM2_HEADER_TAG_SIZE) == 0
               || memcmp(message, sam2_sigx_header, SAM2_HEADER_TAG_SIZE) == 0) {
        sam2_signal_message_t *signal_message = (sam2_signal_message_t *)message;
        SAM2__SANITIZE_STRING(signal_message->ice_sdp);
    } else if (memcmp(message, sam2_fail_header, 8) == 0) {
        sam2_error_message_t *error_message = (sam2_error_message_t *)message;
        SAM2__SANITIZE_STRING(error_message->description);
    }
}

SAM2_LINKAGE int sam2_client_poll(sam2_socket_t sockfd, sam2_message_u *message, char *buffer, int *buffer_length) {
    int bytes_desired = sizeof(sam2_message_u) - *buffer_length;
    int bytes_read = 0;

    // Trying to read zero bytes from a socket will close it
    if (bytes_desired > 0) {
        bytes_read = recv(sockfd, ((char *) buffer) + *buffer_length, bytes_desired, 0);

        if (bytes_read < 0) {
            if (SAM2_SOCKERRNO == SAM2_EAGAIN || SAM2_SOCKERRNO == EWOULDBLOCK) {
                //SAM2_LOG_DEBUG("No more datagrams to receive");
            } else if (SAM2_SOCKERRNO == SAM2_ENOTCONN) {
                SAM2_LOG_INFO("Socket not connected");
                return 0;
            } else {
                SAM2_LOG_ERROR("Error reading from socket");//, strerror(errno));
                return -1;
            }
        } else if (bytes_read == 0) {
            SAM2_LOG_WARN("Server closed connection");
            return -1;
        } else {
            *buffer_length += bytes_read;
        }
    }

    int message_frame_status = sam2__frame_message(message, buffer, buffer_length);

    if (message_frame_status == 0) {
        //SAM2_LOG_DEBUG("Received %d/%d bytes of header", *buffer_length, SAM2_HEADER_SIZE);
        return 0;
    } else if (message_frame_status < 0) {
        SAM2_LOG_ERROR("Message framing failed code (%d)", message_frame_status);
        if (message_frame_status == SAM2_RESPONSE_INVALID_HEADER) {
            SAM2_LOG_WARN("Invalid header received '%.4s'", buffer);
        }
        return -1;
    } else {
        SAM2_LOG_DEBUG("Received complete message with header '%.8s'", (char *) message);
        ((char *) message)[7] = 'R';
        sam2__sanitize_message(message);

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



#if defined(SAM2_IMPLEMENTATION) && defined(SAM2_SERVER)
#ifndef SAM2_SERVER_C
#define SAM2_SERVER_C
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

static sam2_message_u *sam2__alloc_message_raw(sam2_server_t *server) {
    // @todo
    //sam2_message_u *response = NULL;
    //if (response_freelist) {
    //    response = response_freelist;
    //    response_freelist = response_freelist->next;
    //} else {
    //    assert(0); // Just check we have ample responses to send out >~32 in case of broadcast. This beats checking for NULL from an allocator every single time we make an allocation
    //}
    server->_debug_allocated_messages++;

    return (sam2_message_u *) calloc(1, sizeof(sam2_message_u));
}

static void sam2__free_message_raw(sam2_server_t *server, void *message) {
    //response->next = response_freelist;
    //response_freelist = response;
    server->_debug_allocated_messages--;
    free(message);
}

static sam2_message_u *sam2__alloc_message(sam2_server_t *server, const char *header) {
    sam2_message_u *message = sam2__alloc_message_raw(server);

    sam2_avl_node_t *message_node = (sam2_avl_node_t *) SAM2_MALLOC(sizeof(sam2_avl_node_t));
    message_node->key = (uint64_t) message;

    if (message_node != sam2_avl_insert(&server->_debug_allocated_message_set, message_node)) {
        SAM2_FREE(message_node);
        SAM2_LOG_ERROR(
            "Somehow we allocated the same block of memory for different responses twice in one on_recv call."
            " Probably this debug bookkeeping logic is broken or the allocator."
        );
    }

    if (message == NULL) {
        SAM2_LOG_FATAL("Out of memory");
    } else {
        memcpy((void*)message, header, SAM2_HEADER_SIZE);
    }

    return message;
}

static void sam2__free_response(sam2_server_t *server, void *message) {
    sam2__free_message_raw(server, message);

    sam2_avl_node_t key_only_node = { (uint64_t) message };
    sam2_avl_node_t *node = sam2_avl_erase(&server->_debug_allocated_message_set, &key_only_node);
    if (node) {
        SAM2_FREE(node);
    }
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    #ifdef _WIN32
    typedef ULONG buf_len_t;
    #else
    typedef size_t buf_len_t;
    #endif

#if 1
    buf->base = (char*) malloc(suggested_size);
    buf->len = (buf_len_t) suggested_size;
#else
    // @todo Make this a zero-copy with no allocations
    //       The solution looks something like this:
    buf->base = client->buffer + client->length;
    buf->len = sizeof(client->buffer) - client->length;
#endif
}

typedef struct {
    uv_write_t req;
    sam2_server_t *server;
} sam2_ext_write_t;

static void on_write(uv_write_t *req, int status) {
    if (status) {
        SAM2_LOG_ERROR("uv_write error: %s", uv_strerror(status));
    }

    sam2_ext_write_t *ext_req = (sam2_ext_write_t *) req;

    sam2__free_response(ext_req->server, req->data);

    free(req);
}

// This procedure owns the lifetime of response
static void sam2__write_response(uv_stream_t *client_tcp, sam2_message_u *message) {
    sam2_server_t *server = (sam2_server_t *) client_tcp->data;

    sam2_avl_node_t key_only_node = { (uint64_t) message };

    sam2_avl_node_t *node = sam2_avl_erase(&server->_debug_allocated_message_set, &key_only_node);
    if (node) {
        SAM2_FREE(node);
    } else {
        SAM2_LOG_ERROR(
            "The memory for a response sent was reused within the same on_recv call. This is almost certainly an error. If you are"
            " broadcasting a message you have to individually allocate each response since libuv sends them asynchronously"
        );
    }

    sam2_message_metadata_t *metadata = sam2_get_metadata((char *) message);

    if (metadata == NULL) {
        SAM2_LOG_ERROR("We tried to send a response with invalid header to a client '%.8s'", (char *) message);
        sam2__free_response(server, message);
        return;
    }

    sam2_message_u *message_rle8 = sam2__alloc_message_raw(server);
    int64_t message_size_rle8 = rle8_encode_capped((uint8_t *)message, metadata->message_size, (uint8_t *) message_rle8, sizeof(*message_rle8));

    // If this fails, we just send the message uncompressed
    int64_t message_size;
    if (message_size_rle8 != -1) {
        ((char *) message_rle8)[7] = 'z';
        sam2__free_response(server, message);
        message = message_rle8;
        message_size = message_size_rle8;
    } else {
        ((char *) message)[7] = 'r';
        sam2__free_response(server, message_rle8);
        message_size = metadata->message_size;
    }

    uv_buf_t buffer;
    buffer.len = message_size;
    buffer.base = (char *) message;

    sam2_ext_write_t *write_req = (sam2_ext_write_t*) malloc(sizeof(sam2_ext_write_t));
    write_req->req.data = message;
    write_req->server = server;

    int status = uv_write((uv_write_t *) write_req, client_tcp, &buffer, 1, on_write);
    if (status < 0) {
        SAM2_LOG_ERROR("uv_write error: %s", uv_strerror(status));
        free(write_req);
        sam2__free_response(server, message);
    }
}

static void on_write_error(uv_write_t *req, int status) {
    sam2_client_t *client = (sam2_client_t *) req->data;

    if (status < 0) {
        SAM2_LOG_WARN("Failed to send error message to client: %s", uv_strerror(status));
        // @todo Probably just close the connection here
    } else {
        SAM2_LOG_INFO("Sent error response to client %016" PRIx64 "", client->peer_id);
    }

    free(req);
}

// The lifetime of response is managed by the caller
static void write_error(uv_stream_t *client, sam2_error_message_t *response) {
    if (memcmp(sam2_fail_header, response->header, SAM2_HEADER_SIZE) != 0) {
        SAM2_LOG_ERROR("We tried to send a error response with invalid header to a client '%.8s'", (char *) response->header);
        return;
    }

    uv_buf_t buffer;
    buffer.len = sam2_get_metadata((char *) response)->message_size;
    buffer.base = (char *) response;
    uv_write_t *write_req = (uv_write_t*) malloc(sizeof(uv_write_t));
    write_req->data = client;
    int status = uv_write(write_req, client, &buffer, 1, on_write_error);
    if (status < 0) {
        SAM2_LOG_ERROR("uv_write error: %s", uv_strerror(status));
    }
}

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

    SAM2_LOG_INFO("Data dumped to file: %s", filename);
}

static void sam2__remove_room(sam2_server_t *server, sam2_room_t *room) {
    room = sam2__find_hosted_room(server, room);
    if (room) {
        sam2_client_t *client = sam2__find_client(server, room->peer_ids[SAM2_AUTHORITY_INDEX]);

        if (client) {
            client->hosted_room = NULL;
        } else {
            SAM2_LOG_WARN("Failed to find client data for room %016" PRIx64 ":'%s'",
                room->peer_ids[SAM2_AUTHORITY_INDEX], room->name);
        }

        int64_t i = room - server->rooms;

        // This check avoids aliasing issues with memcpy which clang swaps in here
        if (i != server->room_count - 1) {
            server->rooms[i] = server->rooms[server->room_count - 1];
        }

        --server->room_count;
    } else {
        SAM2_LOG_WARN("Tried to delist non-existent room");
    }
}

static void sam2__on_client_destroy(uv_handle_t *handle) {
    sam2_client_t *client = (sam2_client_t *) handle;
    sam2_server_t *server = (sam2_server_t *) handle->data;
    SAM2_LOG_INFO("Socket for client %016" PRIx64 " closed", client->peer_id);

    if (client->hosted_room) {
        SAM2_LOG_INFO("Removing room %016" PRIx64 ":'%s' its owner disconnected", client->peer_id, client->hosted_room->name);
        sam2__remove_room(server, client->hosted_room);
    }

    // In theory this should be performed in a async callback before this one
    struct sam2_node key_only_node = { client->peer_id };
    struct sam2_node *node = sam2_avl_erase(&server->peer_id_map, &key_only_node);

    if (node) {
        SAM2_FREE(node);
    }

    client->flags |= SAM2__CLIENT_FLAG_CLIENT_CLOSE_DONE;

    if ((client->flags & SAM2__CLIENT_FLAG_MASK_CLOSE_DONE) == SAM2__CLIENT_FLAG_MASK_CLOSE_DONE) {
        SAM2_FREE(client);
    }
}

static void sam2__on_client_timer_close(uv_handle_t *handle) {
    sam2_client_t *client = (sam2_client_t *) handle->data;
    client->flags |= SAM2__CLIENT_FLAG_TIMER_CLOSE_DONE;

    if ((client->flags & SAM2__CLIENT_FLAG_MASK_CLOSE_DONE) == SAM2__CLIENT_FLAG_MASK_CLOSE_DONE) {
        SAM2_FREE(client);
    }
}

static void sam2__client_destroy(sam2_client_t *client) {
    // These aren't closed in order... ask me how I know
    if (client->timer.data != NULL) {
        uv_close((uv_handle_t *) &client->timer, sam2__on_client_timer_close);
    }

    uv_close((uv_handle_t *) &client->tcp, sam2__on_client_destroy);
}

static void sam2__write_fatal_error(uv_stream_t *client, sam2_error_message_t *response) {
    write_error(client, response);
    sam2__client_destroy((sam2_client_t *) client);
}

static void on_timeout(uv_timer_t *handle) {
    // Dereferencing client should be fine here since before we free the client we close the timer which prevents this callback from triggering
    uv_stream_t *client_tcp = (uv_stream_t *) handle->data;
    sam2_client_t *client = (sam2_client_t *) client_tcp;

    // Check if client connection is still open
    if (uv_is_closing((uv_handle_t*) client_tcp)) {
        SAM2_LOG_INFO("Client %" PRIx64 " connection is already closing or closed", client->peer_id);
    } else {
        SAM2_LOG_WARN("Client %" PRIx64 " sent incomplete message with header '%.*s' and size %d",
            client->peer_id, (int)SAM2_MIN(client->length, SAM2_HEADER_SIZE), client->buffer, client->length);

        static sam2_error_message_t response = {
            SAM2_FAIL_HEADER, SAM2_RESPONSE_INVALID_HEADER,
             "An incomplete TCP message was received before timing out"
        };

        sam2__dump_data_to_file("IncompleteMessage", client->buffer, client->length);

        write_error(client_tcp, &response);
    }
}

// Sanus is latin for healthy
static int sam2__sanity_check_message(sam2_message_u *message, sam2_room_t *associated_room) {

    if (memcmp(message, sam2_make_header, SAM2_HEADER_TAG_SIZE) == 0) {

    } else if (memcmp(message, sam2_join_header, SAM2_HEADER_TAG_SIZE) == 0) {
        sam2_room_join_message_t *join_message = (sam2_room_join_message_t *) message;
        join_message->room.name[sizeof(join_message->room.name) - 1] = '\0';
        // @todo Issue a conventional warning to console log if more than one flag or peer_id has changed
        //       Even one flag and one peer_id's changing simulaneously is a violation
    }

    return 0;
}

static void on_read(uv_stream_t *client_tcp, ssize_t nread, const uv_buf_t *buf) {
    sam2_client_t *client = (sam2_client_t *) client_tcp;
    sam2_server_t *server = (sam2_server_t *) client->tcp.data;

    if (server->_debug_allocated_message_set != NULL) {
        SAM2_LOG_ERROR("We had allocated unsent responses this means we probably leaked memory handling the last response");
        kavll_free(sam2_avl_node_t, head, server->_debug_allocated_message_set, SAM2_FREE);
        server->_debug_allocated_message_set = NULL;
    }

    SAM2_LOG_DEBUG("nread=%lld", (long long int)nread);
    if (nread < 0) {
        // If the client closed the socket
        if (nread != UV_EOF) {
            SAM2_LOG_INFO("Read error %s", uv_err_name((int) nread));
        }

        SAM2_LOG_DEBUG("Got EOF");
        uv_timer_stop(&client->timer);

        sam2__client_destroy(client);

        goto cleanup;
    }

    for (int64_t remaining = nread;;) {
        int64_t num_bytes_to_move_into_buffer = SAM2_MIN(((int) sizeof(client->buffer)) - client->length, remaining);
        memcpy(client->buffer + client->length, buf->base + (nread - remaining), num_bytes_to_move_into_buffer);
        remaining -= num_bytes_to_move_into_buffer;
        client->length += num_bytes_to_move_into_buffer;

        sam2_message_u message;
        int frame_message_status = sam2__frame_message(&message, client->buffer, &client->length);

        if (frame_message_status == 0) {
            if (client->length > 0) {
                // This is basically a courtesy for clients to warn them they only sent a partial message
                uv_timer_start(&client->timer, on_timeout, 500, 0);  // 0.5 seconds
            }

            goto cleanup; // We've processed the last message
        } else if (frame_message_status == SAM2_RESPONSE_VERSION_MISMATCH) {
            SAM2_LOG_WARN("Version mismatch. Client: %c Server: %c", client->buffer[4], SAM2_VERSION_MAJOR + '0');
            static sam2_error_message_t response = {
                SAM2_FAIL_HEADER,
                SAM2_RESPONSE_VERSION_MISMATCH,
                "Version mismatch"
            };

            sam2__write_fatal_error(client_tcp, &response);
            goto cleanup;
        } else if (frame_message_status == SAM2_RESPONSE_INVALID_HEADER) {
            SAM2_LOG_INFO("Client %" PRIx64 " sent invalid header tag '%.4s'", client->peer_id, client->buffer);
            static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_INVALID_HEADER, "Invalid header" };

            sam2__write_fatal_error(client_tcp, &response);
            goto cleanup;
        } else if (frame_message_status == SAM2_RESPONSE_INVALID_ENCODE_TYPE) {
            static sam2_error_message_t response = {
                SAM2_FAIL_HEADER,
                SAM2_RESPONSE_INVALID_ARGS,
                "Invalid message encode type"
            };

            sam2__write_fatal_error(client_tcp, &response);
            goto cleanup;
        }

        SAM2_LOG_DEBUG("client->length=%d", client->length);

        SAM2_LOG_INFO("Client %" PRIx64 " sent message with header '%.8s'", client->peer_id, (char *) &message);

        // Send the appropriate response
        if (memcmp(&message, sam2_list_header, SAM2_HEADER_TAG_SIZE) == 0) {
            for (const int64_t stop_at = SAM2_MIN(client->rooms_sent + 128, server->room_count); client->rooms_sent < stop_at;) {
                sam2_room_list_message_t *response = &sam2__alloc_message(server, sam2_list_header)->room_list_response;
                response->room = server->rooms[client->rooms_sent++];
                sam2__write_response(client_tcp, (sam2_message_u *) response);
            }

            if (client->rooms_sent >= server->room_count) {
                client->rooms_sent = 0;
                sam2_room_list_message_t *response = &sam2__alloc_message(server, sam2_list_header)->room_list_response;
                memset(&response->room, 0, sizeof(response->room));
                sam2__write_response(client_tcp, (sam2_message_u *) response);
            }

            // @todo Send remaining rooms if there are more than 128
        } else if (memcmp(&message, sam2_make_header, SAM2_HEADER_TAG_SIZE) == 0) {
            sam2_room_make_message_t *request = (sam2_room_make_message_t *) &message;

            if (server->room_count + 1 > server->room_capacity) {
                SAM2_LOG_WARN("Out of rooms");
                static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_SERVER_ERROR, "Out of rooms"};
                write_error((uv_stream_t *) client, &response);
                goto finished_processing_last_message;
            }

            if (client->hosted_room) {
                if (request->room.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
                    SAM2_LOG_INFO("Client %" PRIx64 " updated the state of room '%s'", client->peer_id, client->hosted_room->name);
                    *client->hosted_room = request->room;
                } else {
                    SAM2_LOG_INFO("Client %" PRIx64 " abandoned the room '%s'", client->peer_id, client->hosted_room->name);
                    sam2__remove_room(server, client->hosted_room);
                }
            } else {
                sam2_room_t *new_room = &server->rooms[server->room_count++];
                client->hosted_room = new_room;

                request->room.peer_ids[SAM2_AUTHORITY_INDEX] = client->peer_id;

                SAM2_LOG_DEBUG("Copying &request->room:%p into room+server->room_count:%p room_count+1:%lld", &request->room, new_room, (long long int)server->room_count);
                memcpy(new_room, &request->room, sizeof(*new_room));
                new_room->name[sizeof(new_room->name) - 1] = '\0';
                new_room->flags |= SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;

                sam2_room_make_message_t *response = (sam2_room_make_message_t *) sam2__alloc_message(server, sam2_make_header);
                memcpy(&response->room, new_room, sizeof(*new_room));

                sam2__write_response(client_tcp, (sam2_message_u *) response);
            }
        } else if (memcmp(&message, sam2_join_header, SAM2_HEADER_TAG_SIZE) == 0) {
            // The logic in here is complicated because this message aliases many different operations...
            // keeping the message structure uniform simplifies the client perspective in my opinion
            sam2_room_join_message_t *request = (sam2_room_join_message_t *) &message;

            request->room.name[sizeof(request->room.name) - 1] = '\0';
            //memcpy(buffer.base, request, sizeof(*request));

            sam2_room_t *associated_room = sam2__find_hosted_room(server, &request->room);

            if (!associated_room) {
                SAM2_LOG_INFO("Client attempted to join non-existent room with authority %" PRIx64 "", request->room.peer_ids[SAM2_AUTHORITY_INDEX]);
                static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_ROOM_DOES_NOT_EXIST, "Room not found"};
                write_error((uv_stream_t *) client, &response);
                goto finished_processing_last_message;
            }

            if (   request->room.peer_ids[SAM2_AUTHORITY_INDEX] == client->peer_id
                && !(request->room.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)) {
                SAM2_LOG_INFO("Authority %" PRIx64 " abandoned the room '%s'", client->peer_id, associated_room->name);
                sam2__remove_room(server, associated_room);
                goto finished_processing_last_message;
            }

            // Client requests state change by authority
            int p_join = sam2_get_port_of_peer(&request->room, client->peer_id);
            {
                int p_in = sam2_get_port_of_peer(associated_room, client->peer_id);
                if (p_join == -1) {
                    if (p_in == -1) {
                        SAM2_LOG_WARN("Client sent state change request for a room they are not in"); // @todo Add generic check and change this to an assert
                        static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_INVALID_ARGS, "Invalid state change request"};
                        write_error((uv_stream_t *) client, &response);
                        goto finished_processing_last_message;
                    } else {
                        // Peer left
                        if (request->room.peer_ids[p_in] != SAM2_PORT_AVAILABLE) {
                            SAM2_LOG_WARN("Convention violation: Client did not set the port to SAM2_PORT_AVAILABLE when leaving");
                            static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_INVALID_ARGS, "When leaving a room, set the port to SAM2_PORT_AVAILABLE"};
                            write_error((uv_stream_t *) client, &response);
                            goto finished_processing_last_message;
                        }
                    }
                } else {
                    if (p_in == -1) {
                        // Peer joined
                        if (associated_room->peer_ids[p_join] != SAM2_PORT_AVAILABLE) {
                            SAM2_LOG_WARN("Client attempted to join on an unavailable port");
                            static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_PORT_NOT_AVAILABLE, "Port is currently unavailable"};
                            write_error((uv_stream_t *) client, &response);
                            goto finished_processing_last_message;
                        }
                    } else {
                        if (p_in != p_join) {
                            SAM2_LOG_WARN("Client changed port, which is not allowed");
                            static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_INVALID_ARGS, "Peer cannot change ports; Instead leave and rejoin"};
                            write_error((uv_stream_t *) client, &response);
                            goto finished_processing_last_message;
                        }
                    }
                }

                // Check that the client didn't change any ports other than the one they joined on or left on
                for (int p = 0; p < SAM2_PORT_MAX; p++) {
                    if (p != p_join && p != p_in && request->room.peer_ids[p] != associated_room->peer_ids[p]) {
                        SAM2_LOG_WARN("Client %" PRIx64 " attempted to change ports other than the one they joined on or left on", client->peer_id);
                        static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_INVALID_ARGS, "Invalid state change request"};
                        write_error((uv_stream_t *) client, &response);
                        goto finished_processing_last_message;
                    }
                }
            }

            SAM2_LOG_INFO("Forwarding join request to room authority");
            uv_tcp_t *authority = (uv_tcp_t *) sam2__find_client(server, associated_room->peer_ids[SAM2_AUTHORITY_INDEX]);
            if (!authority) {
                SAM2_LOG_ERROR("Room authority not found even though room was associated. This is a bug");
                static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_SERVER_ERROR, "Room authority not found"};
                write_error((uv_stream_t *) client, &response);
                goto finished_processing_last_message;
            }

            sam2_room_join_message_t *response = (sam2_room_join_message_t *) sam2__alloc_message(server, sam2_join_header);
            memcpy(response, request, sizeof(sam2_room_join_message_t));
            response->peer_id = client->peer_id;
            sam2__write_response((uv_stream_t*) authority, (sam2_message_u *) response);
        } else if (   memcmp(&message, sam2_sign_header, SAM2_HEADER_TAG_SIZE) == 0
                   || memcmp(&message, sam2_sigx_header, SAM2_HEADER_TAG_SIZE) == 0) {
            // Clients forwarding sdp's between eachother
            sam2_signal_message_t *request = (sam2_signal_message_t *) &message;

            if (request->peer_id == client->peer_id) {
                SAM2_LOG_INFO("Client attempted to send sdp information to themselves");
                static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_CANNOT_SIGNAL_SELF, "Cannot signal self"};
                write_error((uv_stream_t *) client, &response);
                goto finished_processing_last_message;
            }

            uv_tcp_t *peer_tcp = (uv_tcp_t *) sam2__find_client(server, request->peer_id);

            SAM2_LOG_INFO("Forwarding sdp information from peer %" PRIx64 " to peer %" PRIx64 " it contains '%s'", client->peer_id, request->peer_id, request->ice_sdp);
            if (!peer_tcp) {
                SAM2_LOG_WARN("Forwarding failed Client attempted to send sdp information to non-existent peer");
                static sam2_error_message_t response = { SAM2_FAIL_HEADER, SAM2_RESPONSE_PEER_DOES_NOT_EXIST, "Peer not found"};
                write_error((uv_stream_t *) client, &response);
                goto finished_processing_last_message;
            }

            sam2_signal_message_t *response = (sam2_signal_message_t *) sam2__alloc_message(server, sam2_sign_header);

            memcpy(response, request, sizeof(*response));
            response->peer_id = client->peer_id;

            sam2__write_response((uv_stream_t *) peer_tcp, (sam2_message_u *) response);
        } else if (memcmp(&message, sam2_fail_header, SAM2_HEADER_TAG_SIZE) == 0) {
            sam2_error_message_t *error_message = (sam2_error_message_t *) &message;
            uv_tcp_t *target_peer = (uv_tcp_t *) sam2__find_client(server, error_message->peer_id);

            if (target_peer) {
                SAM2_LOG_INFO("Peer %" PRIx64 " sent error message to Peer %" PRIx64 "", client->peer_id, error_message->peer_id);
                sam2_error_message_t *response = (sam2_error_message_t *) sam2__alloc_message(server, sam2_fail_header);
                memcpy(response, error_message, sizeof(*response));
                sam2__write_response((uv_stream_t *) target_peer, (sam2_message_u *) response);
            } else {
                SAM2_LOG_WARN("Peer %" PRIx64 " tried to send error message to peer %" PRIx64 " but they were not found", client->peer_id, error_message->peer_id);
            }
        } else {
            SAM2_LOG_FATAL("A dumb programming logic error was made or something got corrupted if you ever get here");
        }

finished_processing_last_message:
        uv_timer_stop(&client->timer);
    }

cleanup:

    if (buf->base) {
        SAM2_LOG_DEBUG("Freeing buf");
        free(buf->base);
    }
}

void on_new_connection(uv_stream_t *server_tcp, int status) {
    if (status < 0) {
        SAM2_LOG_ERROR("New connection error %s", uv_strerror(status));
        return;
    }

    sam2_client_t *client = (sam2_client_t *) calloc(1, sizeof(sam2_client_t));
    uv_tcp_t *client_tcp = &client->tcp;
    uv_tcp_init(server_tcp->loop, client_tcp);

    if (uv_accept(server_tcp, (uv_stream_t*) client_tcp) == 0) {
        sam2_client_t *client = (sam2_client_t *) client_tcp;
        sam2_server_t *server = (sam2_server_t *) server_tcp;

        { // Create a peer id based on hashed IP address mixed with a counter
            struct sockaddr_storage name;
            int len = sizeof(name);
            uv_tcp_getpeername(client_tcp, (struct sockaddr*) &name, &len);

            if (name.ss_family == AF_INET) { // IPv4
                struct sockaddr_in* s = (struct sockaddr_in*)&name;
                client->peer_id = fnv1a_hash(&s->sin_addr, sizeof(s->sin_addr));
            } else if (name.ss_family == AF_INET6) { // IPv6
                struct sockaddr_in6* s = (struct sockaddr_in6*)&name;
                client->peer_id = fnv1a_hash(&s->sin6_addr, sizeof(s->sin6_addr));
            }

            static uint64_t counter = 0;
            client->peer_id ^= counter++;
        }

        client->timer.data = client;
        client_tcp->data = server;

        sam2_avl_node_t *node = (sam2_avl_node_t *) calloc(1, sizeof(sam2_avl_node_t));
        node->key = client->peer_id;
        node->client = client_tcp;
        sam2_avl_insert(&server->peer_id_map, node);

        uv_timer_init(&server->loop, &client->timer);
        // Reading the request sent by the client
        int status = uv_read_start((uv_stream_t*) client_tcp, alloc_buffer, on_read);
        if (status < 0) {
            SAM2_LOG_WARN("Failed to connnect to client %" PRIx64 " uv_read_start error: %s", client->peer_id, uv_strerror(status));
        } else {
            SAM2_LOG_INFO("Successfully connected to client %" PRIx64 "", client->peer_id);

            sam2_connect_message_t *connect_message = (sam2_connect_message_t *) sam2__alloc_message(server, sam2_conn_header);
            connect_message->peer_id = client->peer_id;

            sam2__write_response((uv_stream_t*) client_tcp, (sam2_message_u *) connect_message);
        }
    } else {
        sam2__client_destroy((sam2_client_t *) client_tcp);
    }
}

static void sam2__kavll_free_node_and_data(sam2_avl_node_t *node) {
    // This next call doesn't free the hosted room, but that's okay since it's not malloced
    sam2__client_destroy((sam2_client_t *) node->client);
    SAM2_FREE(node);
}

// Secret knowledge hidden within libuv's test folder
#define ASSERT(expr) if (!(expr)) exit(69);
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

SAM2_LINKAGE int sam2_server_create(sam2_server_t *server, int port) {
    int64_t room_capacity = 65536;
    int server_size_bytes = sizeof(sam2_server_t) + sizeof(sam2_room_t) * room_capacity;
    if (server == NULL) {
        return server_size_bytes;
    }

    memset(server, 0, server_size_bytes);

    int err = uv_loop_init(&server->loop);
    if (err) {
        SAM2_LOG_ERROR("Loop initialization failed: %s", uv_strerror(err));
        goto _30;
    }

    server->room_capacity = room_capacity;

    err = uv_tcp_init(&server->loop, &server->tcp);
    if (err) {
        SAM2_LOG_ERROR("TCP initialization failed: %s", uv_strerror(err));
        goto _20;
    }

    struct sockaddr_in6 addr6;
    err = uv_ip6_addr("::", port, &addr6);
    if (err) {
        SAM2_LOG_ERROR("Bind error: %s", uv_strerror(err));
        goto _10;
    }

    err = uv_tcp_bind(&server->tcp, (const struct sockaddr*)&addr6, 0);
    if (err) {
        SAM2_LOG_ERROR("Bind error: %s", uv_strerror(err));
        goto _10;
    }

    err = uv_listen((uv_stream_t*)&server->tcp, SAM2_DEFAULT_BACKLOG, on_new_connection);
    if (err) {
        SAM2_LOG_ERROR("Listen error: %s", uv_strerror(err));
        goto _10;
    }

    return 0;

_10: uv_close((uv_handle_t*)&server->tcp, NULL);
_20: uv_loop_close(&server->loop);
_30: return err;
}

SAM2_LINKAGE int sam2_server_begin_destroy(sam2_server_t *server) {
    // Kill all clients first since they might be reading from the server
    kavll_free(sam2_avl_node_t, head, server->peer_id_map, sam2__kavll_free_node_and_data);
    server->peer_id_map = NULL;

    uv_stop(&server->loop); // This will cause the event loop to exit uv_run once all events are processed
    return 0;
}

static void on_signal(uv_signal_t *handle, int signum) {
    sam2_server_begin_destroy((sam2_server_t *) handle->data);
    uv_close((uv_handle_t*) handle->data, NULL);
}
#endif // SAM2_SERVER_C
#endif // SAM2_SERVER && SAM2_IMPLEMENTATION

#if defined(SAM2_EXECUTABLE)
#ifndef SAM2_MAIN_C
#define SAM2_MAIN_C
int main() {
    sam2_server_t *server = NULL;

    int ret = sam2_server_create(server, SAM2_SERVER_DEFAULT_PORT);

    if (ret < 0) {
        SAM2_LOG_FATAL("Error while getting server memory size");
        return ret;
    }

    server = malloc(ret);
    if (server == NULL) {
        SAM2_LOG_FATAL("Error while allocating %d bytes of server memory", ret);
        return 1;
    }

    ret = sam2_server_create(server, SAM2_SERVER_DEFAULT_PORT);

    if (ret < 0) {
        SAM2_LOG_FATAL("Error while initializing server");
        free(server);
        return ret;
    }

    uv_signal_t sig;
    uv_signal_init(&server->loop, &sig);
    sig.data = server;
    uv_signal_start(&sig, on_signal, SIGINT);

    uv_run(&server->loop, UV_RUN_DEFAULT);

    MAKE_VALGRIND_HAPPY(&server->loop);
    free(server);

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
SAM2_STATIC_ASSERT(sizeof(sam2_room_t) == 64 + sizeof(uint64_t) + 32 + (SAM2_PORT_MAX+1)*sizeof(uint64_t) + sizeof(uint64_t), "sam2_room_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_make_message_t) == 8 + sizeof(sam2_room_t), "sam2_room_make_message_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_list_message_t) == 8 + sizeof(sam2_room_t), "sam2_room_list_message_t is not packed");
SAM2_STATIC_ASSERT(sizeof(sam2_room_join_message_t) == 8 + 8 + 64 + sizeof(sam2_room_t), "sam2_room_join_message_t is not packed");
