


// Macro to suppress compiler-specific warnings
//THIRD_PARTY_INCLUDES_START
// Microsoft Visual C++ Compiler
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4125)  // decimal digit terminates octal escape sequence
#pragma warning(disable: 4456)  // declaration of 'variable' hides previous local declaration
#pragma warning(disable: 4510)  // default constructor could not be generated
#pragma warning(disable: 4610)  // object can never be instantiated - user-defined constructor required
#pragma warning(disable: 4668)
#pragma warning(disable: 4800)  // Implicit conversion from 'type' to bool. Possible information loss
#pragma warning(disable: 4946)  // reinterpret_cast used between related classes
#pragma warning(disable: 4996)  // 'item' was declared deprecated
#pragma warning(disable: 6011)  // Dereferencing NULL pointer
#pragma warning(disable: 6101)  // Returning uninitialized memory
#pragma warning(disable: 6287)  // Redundant code: the left and right sub-expressions are identical
#pragma warning(disable: 6308)  // 'realloc' might return null pointer
#pragma warning(disable: 6326)  // Potential comparison of a constant with another constant
#pragma warning(disable: 6340)  // Mismatch on sign: Incorrect type passed as parameter in call to function
#pragma warning(disable: 6385)  // Reading invalid data
#pragma warning(disable: 6386)  // Buffer overrun while writing to
#pragma warning(disable: 6553)  // The annotation for function does not apply to a value type
#pragma warning(disable: 28182) // Dereferencing NULL pointer
#pragma warning(disable: 28251) // Inconsistent annotation for function
#pragma warning(disable: 28252) // Inconsistent annotation for function
#pragma warning(disable: 28253) // Inconsistent annotation for function
#pragma warning(disable: 28301) // No annotations for first declaration of function

// GCC and Clang Compiler
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
// Add other relevant warning suppressions for GCC/Clang here

// Add specific options for Clang if needed
#ifdef __clang__
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

// Fallback for other compilers
#else
// No known warning suppressions available for other compilers
#endif

// Considering all things this is somehow the most practical way to build external libraries in my mind lol
// This is basically just statically linking everything but in a roundabout fashion
// Static linking is the most practical thing to do since:
//   - We don't have to worry about loading the dll
//   - We aren't at risk of polluting the symbol-space
// This may end up being unmaintainable in which case I'll do something more sane
// MARK: libjuice
#pragma push
#define __STDC_VERSION__ 0
#define RELEASE 0
#define JUICE_ENABLE_LOCALHOST_ADDRESS 0
#define JUICE_STATIC
#ifdef _WIN32
#pragma comment(lib, "bcrypt.lib")
#endif
//#include "Windows/AllowWindowsPlatformTypes.h"
#include "../../ThirdParty/libjuice/src/addr.c"
#include "../../ThirdParty/libjuice/src/agent.c"
#define alloc_string_copy alloc_string_copy2
#include "../../ThirdParty/libjuice/src/base64.c"
#include "../../ThirdParty/libjuice/src/conn.c"
// The following contain conflicting static definitions so they have dedicated translation units
#if 0
#include "../../ThirdParty/libjuice/src/conn_mux.c"
#include "../../ThirdParty/libjuice/src/conn_poll.c"
#include "../../ThirdParty/libjuice/src/conn_thread.c"
#include "../../ThirdParty/libjuice/src/conn_user.c"
#endif
#include "../../ThirdParty/libjuice/src/const_time.c"
#include "../../ThirdParty/libjuice/src/crc32.c"
#include "../../ThirdParty/libjuice/src/hash.c"
#include "../../ThirdParty/libjuice/src/hmac.c"
#include "../../ThirdParty/libjuice/src/ice.c"
#include "../../ThirdParty/libjuice/src/juice.c"
#include "../../ThirdParty/libjuice/src/log.c"
#include "../../ThirdParty/libjuice/src/random.c"
#include "../../ThirdParty/libjuice/src/server.c"
#include "../../ThirdParty/libjuice/src/stun.c"
#include "../../ThirdParty/libjuice/src/timestamp.c"
#include "../../ThirdParty/libjuice/src/turn.c"
#include "../../ThirdParty/libjuice/src/udp.c"

// MARK: zstd
#define ZSTD_LEGACY_SUPPORT 0
#define ZSTDLIB_STATIC_API
#define ZDICTLIB_STATIC_API
#define ZDICTLIB_VISIBLE
#include "../../ThirdParty/zstd/lib/common/debug.c"
#include "../../ThirdParty/zstd/lib/common/entropy_common.c"
#include "../../ThirdParty/zstd/lib/common/error_private.c"
#include "../../ThirdParty/zstd/lib/common/fse_decompress.c"
#include "../../ThirdParty/zstd/lib/common/pool.c"
#include "../../ThirdParty/zstd/lib/common/threading.c"
#include "../../ThirdParty/zstd/lib/common/xxhash.c"
#include "../../ThirdParty/zstd/lib/common/zstd_common.c"
#include "../../ThirdParty/zstd/lib/compress/fse_compress.c"
#include "../../ThirdParty/zstd/lib/compress/hist.c"
#include "../../ThirdParty/zstd/lib/compress/huf_compress.c"
#include "../../ThirdParty/zstd/lib/compress/zstdmt_compress.c"
#include "../../ThirdParty/zstd/lib/compress/zstd_compress.c"
#include "../../ThirdParty/zstd/lib/compress/zstd_compress_literals.c"
#include "../../ThirdParty/zstd/lib/compress/zstd_compress_sequences.c"
#include "../../ThirdParty/zstd/lib/compress/zstd_compress_superblock.c"
#include "../../ThirdParty/zstd/lib/compress/zstd_double_fast.c"
#include "../../ThirdParty/zstd/lib/compress/zstd_fast.c"
#include "../../ThirdParty/zstd/lib/compress/zstd_lazy.c"
#include "../../ThirdParty/zstd/lib/compress/zstd_ldm.c"
#include "../../ThirdParty/zstd/lib/compress/zstd_opt.c"
#include "../../ThirdParty/zstd/lib/decompress/huf_decompress.c"
#include "../../ThirdParty/zstd/lib/decompress/zstd_ddict.c"
#include "../../ThirdParty/zstd/lib/decompress/zstd_decompress.c"
#include "../../ThirdParty/zstd/lib/decompress/zstd_decompress_block.c"
// Don't need these right now
#if 0
#include "../../ThirdParty/zstd/lib/deprecated/zbuff_common.c"
#include "../../ThirdParty/zstd/lib/deprecated/zbuff_compress.c"
#include "../../ThirdParty/zstd/lib/deprecated/zbuff_decompress.c"
#include "../../ThirdParty/zstd/lib/dictBuilder/cover.c"
#include "../../ThirdParty/zstd/lib/dictBuilder/divsufsort.c"
#include "../../ThirdParty/zstd/lib/dictBuilder/fastcover.c"
#include "../../ThirdParty/zstd/lib/dictBuilder/zdict.c"
#endif

// MARK: libuv
// Non platform-specific files
#include "../../ThirdParty/libuv/src/fs-poll.c"
#include "../../ThirdParty/libuv/src/idna.c"
#include "../../ThirdParty/libuv/src/inet.c"
#include "../../ThirdParty/libuv/src/random.c"
#include "../../ThirdParty/libuv/src/strscpy.c"
#include "../../ThirdParty/libuv/src/strtok.c"
#include "../../ThirdParty/libuv/src/thread-common.c"
#include "../../ThirdParty/libuv/src/timer.c"
#include "../../ThirdParty/libuv/src/uv-common.c"
#include "../../ThirdParty/libuv/src/uv-data-getter-setters.c"
#include "../../ThirdParty/libuv/src/version.c"

// Windows-specific files
#ifdef _WIN32
#pragma comment(lib, "Userenv.lib")
#include "../../ThirdParty/libuv/src/win/async.c"
#include "../../ThirdParty/libuv/src/win/core.c"
#include "../../ThirdParty/libuv/src/win/detect-wakeup.c"
#include "../../ThirdParty/libuv/src/win/dl.c"
#include "../../ThirdParty/libuv/src/win/error.c"
#include "../../ThirdParty/libuv/src/win/fs.c"
#include "../../ThirdParty/libuv/src/win/fs-event.c"
#include "../../ThirdParty/libuv/src/win/getaddrinfo.c"
#include "../../ThirdParty/libuv/src/win/getnameinfo.c"
#include "../../ThirdParty/libuv/src/win/handle.c"
#include "../../ThirdParty/libuv/src/win/loop-watcher.c"
#include "../../ThirdParty/libuv/src/win/pipe.c"
#include "../../ThirdParty/libuv/src/win/poll.c"
#include "../../ThirdParty/libuv/src/win/process.c"
#include "../../ThirdParty/libuv/src/win/process-stdio.c"
#include "../../ThirdParty/libuv/src/win/signal.c"
#include "../../ThirdParty/libuv/src/win/snprintf.c"
#include "../../ThirdParty/libuv/src/win/stream.c"
#define uv_zero_ uv_zero_2
#include "../../ThirdParty/libuv/src/win/tcp.c"
#include "../../ThirdParty/libuv/src/win/thread.c"
#define uv_null_buf_ uv_null_buf_2
#include "../../ThirdParty/libuv/src/win/tty.c"
#define uv_zero_ uv_zero_3
#include "../../ThirdParty/libuv/src/win/udp.c"
#include "../../ThirdParty/libuv/src/win/util.c"
#include "../../ThirdParty/libuv/src/win/winapi.c"
#include "../../ThirdParty/libuv/src/win/winsock.c"
#endif

// Unix-specific files common to Linux, macOS, and BSD
#if defined(__unix__) || defined(__APPLE__)
#include "../../ThirdParty/libuv/src/unix/async.c"
#include "../../ThirdParty/libuv/src/unix/core.c"
#include "../../ThirdParty/libuv/src/unix/dl.c"
#include "../../ThirdParty/libuv/src/unix/fs.c"
#include "../../ThirdParty/libuv/src/unix/getaddrinfo.c"
#include "../../ThirdParty/libuv/src/unix/getnameinfo.c"
#include "../../ThirdParty/libuv/src/unix/loop-watcher.c"
#include "../../ThirdParty/libuv/src/unix/loop.c"
#include "../../ThirdParty/libuv/src/unix/pipe.c"
#include "../../ThirdParty/libuv/src/unix/poll.c"
#include "../../ThirdParty/libuv/src/unix/process.c"
#include "../../ThirdParty/libuv/src/unix/signal.c"
#include "../../ThirdParty/libuv/src/unix/stream.c"
#include "../../ThirdParty/libuv/src/unix/tcp.c"
#include "../../ThirdParty/libuv/src/unix/thread.c"
#include "../../ThirdParty/libuv/src/unix/tty.c"
#include "../../ThirdParty/libuv/src/unix/udp.c"
#endif

// Additional files specific to macOS (Darwin)
#ifdef __APPLE__
#include "../../ThirdParty/libuv/src/unix/darwin.c"
#include "../../ThirdParty/libuv/src/unix/darwin-proctitle.c"
#include "../../ThirdParty/libuv/src/unix/fsevents.c"
#endif

// Additional files specific to Linux (Including Android)
#ifdef __linux__
#include "../../ThirdParty/libuv/src/unix/linux.c"
#include "../../ThirdParty/libuv/src/unix/procfs-exepath.c"
#include "../../ThirdParty/libuv/src/unix/random-getrandom.c"
#include "../../ThirdParty/libuv/src/unix/random-sysctl-linux.c"
#endif

// Additional files specific to Android
#if defined(__ANDROID__)
#include "../../ThirdParty/libuv/src/unix/random-getentropy.c"
#include "../../ThirdParty/libuv/src/unix/proctitle.c"
#endif

// Additional files specific to FreeBSD
#ifdef __FreeBSD__
#include "../../ThirdParty/libuv/src/unix/freebsd.c"
#endif

// Additional files specific to OpenBSD
#ifdef __OpenBSD__
#include "../../ThirdParty/libuv/src/unix/openbsd.c"
#endif

// Additional files specific to NetBSD
#ifdef __NetBSD__
#include "../../ThirdParty/libuv/src/unix/netbsd.c"
#endif

// Additional files specific to BSDs
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include "../../ThirdParty/libuv/src/unix/bsd-ifaddrs.c"
#include "../../ThirdParty/libuv/src/unix/kqueue.c"
#endif

// Generic Unix files not specific to any above category
#if defined(__unix__) || defined(__APPLE__)
#include "../../ThirdParty/libuv/src/unix/posix-hrtime.c"
#include "../../ThirdParty/libuv/src/unix/posix-poll.c"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4459)
#endif

// This source file defines static variables with names like mutex and cond which are very
// likely to conflict with local definitions so I stuck it at the bottom here
#include "../../ThirdParty/libuv/src/threadpool.c"

#ifdef _MSC_VER
#pragma warning(pop)
#endif
