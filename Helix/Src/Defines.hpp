#pragma once

#include <stdint.h>

#if !defined(_MSC_VER)
#include <signal.h>
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#define HLX_PLATFORM_WINDOWS 1

#ifndef _WIN64
#error "64-bit is required on Windows"
#endif // _WIN64

#else
#error "Unsupported Platform!"
#endif // WIN32 || _WIN32 || __WIN32__

// Macros ////////////////////////////////////////////////////////////////

#define ArraySize(array) (sizeof(array) / sizeof((array)[0]))

#if defined(_MSC_VER)
#define HLX_INLINE inline
#define HLX_FINLINE __forceinline
#define HLX_DEBUG_BREAK __debugbreak();
#define HLX_DISABLE_WARNING(warning_number)                                    \
  __pragma(warning(disable : warning_number))
#define HLX_CONCAT_OPERATOR(x, y) x##y
#else
#define HLX_INLINE inline
#define HLX_FINLINE always_inline
#define HLX_DEBUG_BREAK raise(SIGTRAP);
#define HLX_CONCAT_OPERATOR(x, y) x y
#endif // MSVC

#define HLX_STRINGIZE(L) #L
#define HLX_MAKESTRING(L) HLX_STRINGIZE(L)
#define HLX_CONCAT(x, y) HLX_CONCAT_OPERATOR(x, y)
#define HLX_LINE_STRING HLX_MAKESTRING(__LINE__)
#define HLX_FILELINE(MESSAGE) __FILE__ "(" HLX_LINE_STRING ") : " MESSAGE

// GLM Defines
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

// Unique names
#define HLX_UNIQUE_SUFFIX(PARAM) HLX_CONCAT(PARAM, __LINE__)

// Native types typedefs /////////////////////////////////////////////////
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef const char *cstring;

static const u64 u64_max = UINT64_MAX;
static const i64 i64_max = INT64_MAX;
static const u32 u32_max = UINT32_MAX;
static const i32 i32_max = INT32_MAX;
static const u16 u16_max = UINT16_MAX;
static const i16 i16_max = INT16_MAX;
static const u8 u8_max = UINT8_MAX;
static const i8 i8_max = INT8_MAX;

#if defined(_WIN32)
#ifdef HELIX_EXPORT
#ifdef HELIX_SHARED
#define HLX_API __declspec(dllexport)
#else
#define HLX_API __declspec(dllimport)
#endif
#else
#define HLX_API
#endif
#else
#define HLX_API
#endif
