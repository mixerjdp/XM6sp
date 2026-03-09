//---------------------------------------------------------------------------
//
//  X68000 EMULATOR "XM6"
//
//  [ File I/O ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"
#include <malloc.h>
#include <string.h>

#if defined(_WIN32)

Fileio::Fileio()
{
	handle = -1;
	memory_mode = FALSE;
	memory_read_only = TRUE;
	memory_dynamic = FALSE;
	memory_buffer = NULL;
	memory_size = 0;
	memory_pos = 0;
	memory_capacity = 0;
}

Fileio::~Fileio()
{
	Close();
	ASSERT(!memory_mode);
	ASSERT(handle == -1);
}

BOOL FASTCALL Fileio::Load(const Filepath& path, void *buffer, int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(!IsValid());

	if (!Open(path, ReadOnly)) {
		return FALSE;
	}
	if (!Read(buffer, size)) {
		Close();
		return FALSE;
	}
	Close();
	return TRUE;
}

BOOL FASTCALL Fileio::Save(const Filepath& path, void *buffer, int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(!IsValid());

	if (!Open(path, WriteOnly)) {
		return FALSE;
	}
	if (!Write(buffer, size)) {
		Close();
		return FALSE;
	}
	Close();
	return TRUE;
}

BOOL FASTCALL Fileio::Open(const TCHAR* fname, OpenMode mode)
{
	ASSERT(this);
	ASSERT(fname);
	ASSERT(!IsValid());

	if (fname[0] == _T('\0')) {
		handle = -1;
		return FALSE;
	}

	switch (mode) {
		case ReadOnly:
			handle = _topen(fname, _O_BINARY | _O_RDONLY);
			break;
		case WriteOnly:
			handle = _topen(fname, _O_BINARY | _O_CREAT | _O_WRONLY | _O_TRUNC, _S_IWRITE);
			break;
		case ReadWrite:
			if (_taccess(fname, 0x06) != 0) {
				return FALSE;
			}
			handle = _topen(fname, _O_BINARY | _O_RDWR);
			break;
		case Append:
			handle = _topen(fname, _O_BINARY | _O_CREAT | _O_WRONLY | _O_APPEND, _S_IWRITE);
			break;
		default:
			ASSERT(FALSE);
			break;
	}

	if (handle == -1) {
		return FALSE;
	}
	return TRUE;
}

BOOL FASTCALL Fileio::Open(const Filepath& path, OpenMode mode)
{
	ASSERT(this);
	return Open(path.GetPath(), mode);
}

BOOL FASTCALL Fileio::OpenMemoryRead(const void *buffer, unsigned int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(!IsValid());

	memory_mode = TRUE;
	memory_read_only = TRUE;
	memory_dynamic = FALSE;
	memory_buffer = (BYTE*)buffer;
	memory_size = size;
	memory_pos = 0;
	memory_capacity = size;
	handle = -1;
	return TRUE;
}

BOOL FASTCALL Fileio::OpenMemoryWrite(void *buffer, unsigned int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(!IsValid());

	memory_mode = TRUE;
	memory_read_only = FALSE;
	memory_dynamic = FALSE;
	memory_buffer = (BYTE*)buffer;
	memory_size = size;
	memory_pos = 0;
	memory_capacity = size;
	handle = -1;
	return TRUE;
}

BOOL FASTCALL Fileio::OpenMemoryWriteDynamic(unsigned int initial_size)
{
	ASSERT(this);
	ASSERT(!IsValid());

	if (initial_size == 0) {
		initial_size = 4096;
	}

	memory_buffer = (BYTE*)malloc(initial_size);
	if (!memory_buffer) {
		return FALSE;
	}

	memory_mode = TRUE;
	memory_read_only = FALSE;
	memory_dynamic = TRUE;
	memory_size = 0;
	memory_pos = 0;
	memory_capacity = initial_size;
	handle = -1;
	return TRUE;
}

BOOL FASTCALL Fileio::Read(void *buffer, int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);

	if (memory_mode) {
		if ((DWORD)size > memory_size || memory_pos > memory_size - (DWORD)size) {
			return FALSE;
		}
		::memcpy(buffer, memory_buffer + memory_pos, size);
		memory_pos += (DWORD)size;
		return TRUE;
	}

	ASSERT(handle >= 0);
	return (_read(handle, buffer, size) == size);
}

BOOL FASTCALL Fileio::Write(const void *buffer, int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);

	if (memory_mode) {
		DWORD required;
		ASSERT(!memory_read_only);
		if (memory_read_only) {
			return FALSE;
		}
		required = memory_pos + (DWORD)size;
		if (required > memory_capacity) {
			if (!memory_dynamic) {
				return FALSE;
			}
			DWORD new_capacity = memory_capacity ? memory_capacity : 4096;
			while (new_capacity < required) {
				if (new_capacity >= 0x80000000UL) {
					new_capacity = required;
					break;
				}
				new_capacity *= 2;
			}
			BYTE *new_buffer = (BYTE*)realloc(memory_buffer, new_capacity);
			if (!new_buffer) {
				return FALSE;
			}
			memory_buffer = new_buffer;
			memory_capacity = new_capacity;
		}
		::memcpy(memory_buffer + memory_pos, buffer, size);
		memory_pos = required;
		if (memory_pos > memory_size) {
			memory_size = memory_pos;
		}
		return TRUE;
	}

	ASSERT(handle >= 0);
	return (_write(handle, buffer, size) == size);
}

BOOL FASTCALL Fileio::Seek(long offset)
{
	ASSERT(this);
	ASSERT(offset >= 0);

	if (memory_mode) {
		if ((DWORD)offset > memory_size && !memory_dynamic) {
			return FALSE;
		}
		if ((DWORD)offset > memory_capacity && memory_dynamic) {
			return FALSE;
		}
		memory_pos = (DWORD)offset;
		return TRUE;
	}

	ASSERT(handle >= 0);
	return (_lseek(handle, offset, SEEK_SET) == offset);
}

DWORD FASTCALL Fileio::GetFileSize() const
{
	ASSERT(this);
	if (memory_mode) {
		return memory_size;
	}
#if defined(_MSC_VER)
	__int64 len;
	ASSERT(handle >= 0);
	len = _filelengthi64(handle);
	if (len >= 0x100000000i64) {
		return 0xffffffff;
	}
	return (DWORD)len;
#else
	ASSERT(handle >= 0);
	return (DWORD)filelength(handle);
#endif
}

DWORD FASTCALL Fileio::GetFilePos() const
{
	ASSERT(this);
	if (memory_mode) {
		return memory_pos;
	}
	ASSERT(handle >= 0);
	return _tell(handle);
}

void FASTCALL Fileio::Close()
{
	ASSERT(this);

	if (handle != -1) {
		_close(handle);
		handle = -1;
	}
	if (memory_mode && memory_dynamic && memory_buffer) {
		free(memory_buffer);
	}
	memory_mode = FALSE;
	memory_read_only = TRUE;
	memory_dynamic = FALSE;
	memory_buffer = NULL;
	memory_size = 0;
	memory_pos = 0;
	memory_capacity = 0;
}

#endif	// _WIN32
