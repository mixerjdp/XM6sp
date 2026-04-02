//----------------------------------------------------------------------------
//
//	XM6 X68Sound platform shim
//
//----------------------------------------------------------------------------

#ifndef X68SOUND_PLATFORM_H
#define X68SOUND_PLATFORM_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <new>

#if defined(_WIN32)
#include <windows.h>
#include <windowsx.h>
#include <intrin.h>

static inline std::uint32_t x68sound_bswap32(std::uint32_t value)
{
	return _byteswap_ulong(value);
}

static inline std::uint16_t x68sound_bswap16(std::uint16_t value)
{
	return _byteswap_ushort(value);
}

#else

#include <thread>

using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using UINT = unsigned int;
using DWORD = std::uint32_t;
using LPVOID = void *;
using LPSTR = char *;

#ifndef CALLBACK
#define CALLBACK
#endif

#ifndef WINAPI
#define WINAPI
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static inline std::uint32_t x68sound_bswap32(std::uint32_t value)
{
	return __builtin_bswap32(value);
}

static inline std::uint16_t x68sound_bswap16(std::uint16_t value)
{
	return __builtin_bswap16(value);
}

static inline long _InterlockedCompareExchange(volatile long *destination, long exchange, long comparand)
{
	long expected = comparand;
	__atomic_compare_exchange_n(const_cast<long *>(destination), &expected, exchange, false,
		__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return expected;
}

static inline long _InterlockedIncrement(volatile long *destination)
{
	return __atomic_add_fetch(const_cast<long *>(destination), 1, __ATOMIC_SEQ_CST);
}

static inline long _InterlockedDecrement(volatile long *destination)
{
	return __atomic_sub_fetch(const_cast<long *>(destination), 1, __ATOMIC_SEQ_CST);
}

static inline void Sleep(unsigned int milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

static inline DWORD timeGetTime()
{
	static const auto kStart = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	return static_cast<DWORD>(
		std::chrono::duration_cast<std::chrono::milliseconds>(now - kStart).count());
}

#endif

#ifdef GlobalAllocPtr
#undef GlobalAllocPtr
#endif
#ifdef GlobalFreePtr
#undef GlobalFreePtr
#endif

static inline void *GlobalAllocPtr(unsigned int, std::size_t bytes)
{
	return std::calloc(1, bytes);
}

static inline void GlobalFreePtr(void *ptr)
{
	std::free(ptr);
}

#endif  // X68SOUND_PLATFORM_H
