//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
	// [ File path ]
//
//---------------------------------------------------------------------------

#if !defined(filepath_h)
#define filepath_h

#include "os.h"
#include "xm6.h"


typedef struct {
	DWORD low;
	DWORD high;
} filepath_time_t;

//---------------------------------------------------------------------------
//
//	Constants
//
//---------------------------------------------------------------------------
#define FILEPATH_MAX		_MAX_PATH

//===========================================================================
//
//	File path
//	Class for managing file path operations
//
//===========================================================================
class Filepath
{
public:
	// System file type
	enum SysFileType {
		IPL,							// IPL(version 1.00)
		IPLXVI,							// IPL(version 1.10)
		IPLCompact,						// IPL(version 1.20)
		IPL030,							// IPL(version 1.30) and later
		ROM030,							// IPL(version 1.30) and earlier
		CG,								// CG
		CGTMP,							// CG(Win temporary)
		SCSIInt,						// SCSI(internal)
		SCSIExt,						// SCSI(external)
		SRAM							// SRAM
	};

public:
	Filepath();
										// Constructor
	virtual ~Filepath();
										// Destructor
	Filepath& operator=(const Filepath& path);
										// Assignment

	void FASTCALL Clear();
										// Clear
	void FASTCALL SysFile(SysFileType sys);
										// Set file (system)
	void FASTCALL SetPath(const TCHAR* lpszPath);
										// Set file (user)
	void FASTCALL SetBaseDir();
										// Set base directory
	void FASTCALL SetBaseFile(const TCHAR* lpszName);
										// Set base directory with filename

	BOOL FASTCALL IsClear() const;
										// Is cleared
	const TCHAR* FASTCALL GetPath() const	{ return m_szPath; }
										// Get path
	const char* FASTCALL GetShort() const;
										// Get short path (const char*)
	const TCHAR* FASTCALL GetFileExt() const;
										// Get extension (LPCTSTR)
	const TCHAR* FASTCALL GetFileName() const { return m_szFile; }
										// Get filename without extension (LPCTSTR)
	BOOL FASTCALL CmpPath(const Filepath& path) const;
										// Compare path

	static void FASTCALL ClearDefaultDir();
										// Clear default directory
	static void FASTCALL SetDefaultDir(const TCHAR* lpszPath);
										// Set default directory
	static const TCHAR* FASTCALL GetDefaultDir();
	static void FASTCALL ClearSystemDir();
	static void FASTCALL SetSystemDir(const TCHAR* lpszPath);
	static const TCHAR* FASTCALL GetSystemDir();
										// Get default directory

	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load

private:
	void FASTCALL Split();
										// Split path
	void FASTCALL Make();
										// Make path
	void FASTCALL SetCurDir();
										// Set current directory
	BOOL FASTCALL IsUpdate() const;
										// Is updated since save
	void FASTCALL GetUpdateTime(filepath_time_t *pSaved, filepath_time_t *pCurrent ) const;
										// Get update times for save
	TCHAR m_szPath[_MAX_PATH];
										// File path
	TCHAR m_szDrive[_MAX_DRIVE];
										// Drive
	TCHAR m_szDir[_MAX_DIR];
										// Directory
	TCHAR m_szFile[_MAX_FNAME];
										// File name
	TCHAR m_szExt[_MAX_EXT];
										// Extension
	BOOL m_bUpdate;
										// Update flag since save
	filepath_time_t m_SavedTime;
										// Save date/time
	filepath_time_t m_CurrentTime;
										// Current date/time
	static const TCHAR* SystemFile[];
										// System file
	static char ShortName[_MAX_FNAME + _MAX_DIR];
										// Short name (char)
	static TCHAR FileExt[_MAX_FNAME + _MAX_DIR];
										// Short name (TCHAR)
	static TCHAR DefaultDir[_MAX_PATH];
	static TCHAR SystemDir[_MAX_PATH];
										// Default directory
};

#endif	// filepath_h
