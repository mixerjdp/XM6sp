//---------------------------------------------------------------------------
//
//  X68000 EMULATOR "XM6"
//
//  [ File I/O ]
//
//---------------------------------------------------------------------------

#if !defined(fileio_h)
#define fileio_h

#include "os.h"
#include "xm6.h"

class Fileio
{
public:
	enum OpenMode {
		ReadOnly,
		WriteOnly,
		ReadWrite,
		Append
	};

public:
	Fileio();
	virtual ~Fileio();
	BOOL FASTCALL Load(const Filepath& path, void *buffer, int size);
	BOOL FASTCALL Save(const Filepath& path, void *buffer, int size);

#if defined(_WIN32)
	BOOL FASTCALL Open(const TCHAR* fname, OpenMode mode);
#endif	// _WIN32
	BOOL FASTCALL Open(const Filepath& path, OpenMode mode);
	BOOL FASTCALL OpenMemoryRead(const void *buffer, unsigned int size);
	BOOL FASTCALL OpenMemoryWrite(void *buffer, unsigned int size);
	BOOL FASTCALL OpenMemoryWriteDynamic(unsigned int initial_size = 0);
	BOOL FASTCALL Seek(long offset);
	BOOL FASTCALL Read(void *buffer, int size);
	BOOL FASTCALL Write(const void *buffer, int size);
	DWORD FASTCALL GetFileSize() const;
	DWORD FASTCALL GetFilePos() const;
	void FASTCALL Close();
	BOOL FASTCALL IsValid() const { return (BOOL)(memory_mode || handle != -1); }
	int FASTCALL GetHandle() const { return memory_mode ? -1 : handle; }
	const void* FASTCALL GetMemoryData() const { return memory_mode ? memory_buffer : NULL; }

private:
	int handle;
	BOOL memory_mode;
	BOOL memory_read_only;
	BOOL memory_dynamic;
	BYTE *memory_buffer;
	DWORD memory_size;
	DWORD memory_pos;
	DWORD memory_capacity;
};

#endif	// fileio_h
