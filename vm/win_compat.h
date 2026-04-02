#if !defined(XM6_WIN_COMPAT_H)
#define XM6_WIN_COMPAT_H

#if defined(_WIN32)

#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

#if !defined(_WINDOWS_) && !defined(_INC_WINDOWS)
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef int BOOL;
typedef char TCHAR;
typedef const char* LPCTSTR;
typedef char* LPTSTR;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef void* LPVOID;
typedef long LONG;

struct _RTL_CRITICAL_SECTION;
typedef struct _RTL_CRITICAL_SECTION CRITICAL_SECTION;
#endif

#if !defined(TRUE)
#define TRUE 1
#endif
#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(MAX_PATH)
#define MAX_PATH 260
#endif
#if !defined(_MAX_PATH)
#define _MAX_PATH MAX_PATH
#endif
#if !defined(_MAX_DRIVE)
#define _MAX_DRIVE 16
#endif
#if !defined(_MAX_DIR)
#define _MAX_DIR _MAX_PATH
#endif
#if !defined(_MAX_FNAME)
#define _MAX_FNAME _MAX_PATH
#endif
#if !defined(_MAX_EXT)
#define _MAX_EXT 64
#endif

#if !defined(S_ISDIR)
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif

#if !defined(XM6_BASIC_TYPES_DEFINED)
#define XM6_BASIC_TYPES_DEFINED 1
#endif

#if !defined(PATH_SEPARATOR_CHAR)
#define PATH_SEPARATOR_CHAR '\\'
#endif
#if !defined(PATH_SEPARATOR_STR)
#define PATH_SEPARATOR_STR "\\"
#endif

#else

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int BOOL;
typedef char TCHAR;
typedef const char* LPCTSTR;
typedef char* LPTSTR;

#if !defined(XM6_BASIC_TYPES_DEFINED)
#define XM6_BASIC_TYPES_DEFINED 1
#endif

#if !defined(TRUE)
#define TRUE 1
#endif
#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(_MAX_PATH)
#define _MAX_PATH PATH_MAX
#endif
#if !defined(_MAX_DRIVE)
#define _MAX_DRIVE 16
#endif
#if !defined(_MAX_DIR)
#define _MAX_DIR PATH_MAX
#endif
#if !defined(_MAX_FNAME)
#define _MAX_FNAME PATH_MAX
#endif
#if !defined(_MAX_EXT)
#define _MAX_EXT 64
#endif
#if !defined(MAX_PATH)
#define MAX_PATH PATH_MAX
#endif

#if !defined(INVALID_FILE_ATTRIBUTES)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

#if !defined(_T)
#define _T(x) x
#endif

#if !defined(__stdcall)
#define __stdcall
#endif
#if !defined(__cdecl)
#define __cdecl
#endif
#if !defined(__fastcall)
#define __fastcall
#endif
#if !defined(WINAPI)
#define WINAPI
#endif
#if !defined(CALLBACK)
#define CALLBACK
#endif
#if !defined(APIENTRY)
#define APIENTRY
#endif

#define _topen open
#define _taccess access
#define _read read
#define _write write
#define _close close
#define _lseek lseek
#define _tell(fd) ((long)lseek((fd), 0, SEEK_CUR))

static inline int64_t _filelengthi64(int fd)
{
	struct stat st;
	if (fstat(fd, &st) != 0) {
		return -1;
	}
	return (int64_t)st.st_size;
}

#if !defined(_O_RDONLY)
#define _O_RDONLY O_RDONLY
#endif
#if !defined(_O_WRONLY)
#define _O_WRONLY O_WRONLY
#endif
#if !defined(_O_RDWR)
#define _O_RDWR O_RDWR
#endif
#if !defined(_O_CREAT)
#define _O_CREAT O_CREAT
#endif
#if !defined(_O_TRUNC)
#define _O_TRUNC O_TRUNC
#endif
#if !defined(_O_APPEND)
#define _O_APPEND O_APPEND
#endif
#if !defined(_O_BINARY)
#define _O_BINARY 0
#endif
#if !defined(_O_TEXT)
#define _O_TEXT 0
#endif

#if !defined(_S_IWRITE)
#define _S_IWRITE S_IWUSR
#endif
#if !defined(_S_IREAD)
#define _S_IREAD S_IRUSR
#endif

#define _tcslen strlen
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscat strcat
#define _tcscmp strcmp
#define _tcsrchr strrchr
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _vsnprintf vsnprintf
#define _snprintf snprintf

static inline char* xm6_lstrcpyn(char* dst, const char* src, int count)
{
	if (!dst || count <= 0) {
		return dst;
	}
	size_t n = (size_t)(count - 1);
	if (src && n > 0) {
		size_t len = strlen(src);
		if (len < n) {
			n = len;
		}
		memcpy(dst, src, n);
	}
	dst[n] = '\0';
	return dst;
}

#define lstrcpyn xm6_lstrcpyn

#if !defined(PATH_SEPARATOR_CHAR)
#define PATH_SEPARATOR_CHAR '/'
#endif
#if !defined(PATH_SEPARATOR_STR)
#define PATH_SEPARATOR_STR "/"
#endif

static inline void xm6_copy_part(char* dst, size_t dst_cap, const char* src, size_t src_len)
{
	if (!dst || dst_cap == 0) {
		return;
	}
	size_t n = (src_len < (dst_cap - 1)) ? src_len : (dst_cap - 1);
	if (src && n > 0) {
		memcpy(dst, src, n);
	}
	dst[n] = '\0';
}

static inline void xm6_tsplitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
	if (drive) drive[0] = '\0';
	if (dir) dir[0] = '\0';
	if (fname) fname[0] = '\0';
	if (ext) ext[0] = '\0';
	if (!path || !*path) {
		return;
	}

	const char* slash = strrchr(path, '/');
	const char* bslash = strrchr(path, '\\');
	if (!slash || (bslash && bslash > slash)) {
		slash = bslash;
	}

	const char* name = slash ? (slash + 1) : path;
	const char* dot = strrchr(name, '.');
	if (dot == name) {
		dot = NULL;
	}

	if (dir) {
		if (slash) {
			xm6_copy_part(dir, _MAX_DIR, path, (size_t)(slash - path + 1));
		}
	}
	if (fname) {
		const size_t len = dot ? (size_t)(dot - name) : strlen(name);
		xm6_copy_part(fname, _MAX_FNAME, name, len);
	}
	if (ext && dot) {
		xm6_copy_part(ext, _MAX_EXT, dot, strlen(dot));
	}
}

static inline void xm6_tmakepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
	(void)drive;
	if (!path) {
		return;
	}
	path[0] = '\0';
	if (dir && *dir) {
		strncat(path, dir, _MAX_PATH - strlen(path) - 1);
	}
	if (fname && *fname) {
		strncat(path, fname, _MAX_PATH - strlen(path) - 1);
	}
	if (ext && *ext) {
		if (ext[0] != '.') {
			strncat(path, ".", _MAX_PATH - strlen(path) - 1);
		}
		strncat(path, ext, _MAX_PATH - strlen(path) - 1);
	}
}

#define _tsplitpath xm6_tsplitpath
#define _tmakepath xm6_tmakepath

typedef union _LARGE_INTEGER {
	struct {
		uint32_t LowPart;
		int32_t HighPart;
	};
	int64_t QuadPart;
} LARGE_INTEGER;

static inline BOOL QueryPerformanceCounter(LARGE_INTEGER *out)
{
	if (!out) {
		return FALSE;
	}
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return FALSE;
	}
	out->QuadPart = (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
	return TRUE;
}

static inline BOOL QueryPerformanceFrequency(LARGE_INTEGER *out)
{
	if (!out) {
		return FALSE;
	}
	out->QuadPart = 1000000000LL;
	return TRUE;
}

#endif // _WIN32

#endif // XM6_WIN_COMPAT_H
