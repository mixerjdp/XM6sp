
#if defined(_WIN32)

#if !defined(mfc_host_h)
#define mfc_host_h

#include "mfc.h"

#include "device.h"
#include "host_fs.h"



#define XM6_HOST_STRICT_TIMEOUT

// Maximum file size returned by file search.
#define XM6_HOST_FILESIZE_MAX 0xFFFFFF7F

// Buffer size for file reads.
#define XM6_HOST_FILE_BUFFER_READ 0x10000
#define XM6_HOST_FILE_BUFFER_WRITE 0x10000

#define XM6_HOST_FILES_MAX 20

#define XM6_HOST_FCB_MAX 100

#define XM6_HOST_PSEUDO_CLUSTER_MAX 10

#define XM6_HOST_DIRENTRY_CACHE_MAX 30

#define XM6_HOST_FILENAME_CACHE_MAX (36 * 36 * 36 * 36 * 36)

#define XM6_HOST_FILENAME_MARK '@'

#define XM6_HOST_UPDATE_BY_SEQUENCE



#ifdef XM6_HOST_UPDATE_BY_FREQUENCY
#define XM6_HOST_REMOVABLE_RELOAD_TIME 100
#define XM6_HOST_REMOVABLE_RELOAD_COUNT 8
#endif

#define XM6_HOST_REMOVABLE_GUARD_TIME 3000

#define XM6_HOST_REMOVABLE_CACHE_TIME 3000

#define XM6_HOST_REMOVABLE_CHECK_TIME 1000

#define XM6_HOST_TWENTYONE_CHECK_COUNT 10

// Option flags.
enum {
	WINDRV_OPT_CONVERT_SPACE =		0x00000001,
	WINDRV_OPT_CONVERT_BADCHAR =	0x00000002,
	WINDRV_OPT_CONVERT_HYPHENS =	0x00000004,
	WINDRV_OPT_CONVERT_HYPHEN =		0x00000008,
	WINDRV_OPT_CONVERT_PERIODS =	0x00000010,
	WINDRV_OPT_CONVERT_PERIOD =		0x00000020,

	WINDRV_OPT_ALPHABET =			0x00000100,
	WINDRV_OPT_COMPARE_LENGTH =		0x00000200,
	WINDRV_OPT_CONVERT_LENGTH =		0x00000400,

	WINDRV_OPT_MEDIABYTE =			0x00002000,
	WINDRV_OPT_TWENTYONE =			0x00004000,
	WINDRV_OPT_REMOVE =				0x00008000,

	WINDRV_OPT_REDUCED_SPACE =		0x00010000,
	WINDRV_OPT_REDUCED_BADCHAR =	0x00020000,
	WINDRV_OPT_REDUCED_HYPHENS =	0x00040000,
	WINDRV_OPT_REDUCED_HYPHEN =		0x00080000,
	WINDRV_OPT_REDUCED_PERIODS =	0x00100000,
	WINDRV_OPT_REDUCED_PERIOD =		0x00200000,
};

// Option flags.
// Option flags.
// Option flags.
enum {
	FSFLAG_WRITE_PROTECT =			0x00000001,
	FSFLAG_SLOW =					0x00000002,
	FSFLAG_REMOVABLE =				0x00000004,
	FSFLAG_MANUAL =					0x00000008,
};

// Windows file drive.
class CWinFileDrv
{
public:
	CWinFileDrv();
// Constructor.
	virtual ~CWinFileDrv();
// Destructor.
	void FASTCALL Init(LPCTSTR szBase, DWORD nFlag);
// Initialize.

	TCHAR* FASTCALL GetBase() const { ASSERT(this); return (TCHAR*)m_szBase; }
	BOOL FASTCALL isWriteProtect() const { ASSERT(this); return m_bWriteProtect; }
	BOOL FASTCALL isSlow() const { ASSERT(this); return m_bSlow; }
	BOOL FASTCALL isRemovable() const { ASSERT(this); return m_bRemovable; }
	BOOL FASTCALL isManual() const { ASSERT(this); return m_bManual; }
	BOOL FASTCALL isEnable() const { ASSERT(this); return m_bEnable; }
	BOOL FASTCALL isSameDrive(const TCHAR* szDrive) const { ASSERT(this); return _tcscmp(m_szDrive, szDrive) == 0; }
	DWORD FASTCALL GetStatus() const;
	void FASTCALL SetEnable(BOOL bEnable);
// Set the media state.
	void FASTCALL SetTimeout();
	BOOL FASTCALL CheckTimeout();
#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	void FASTCALL SetMediaUpdate(BOOL bDisable);
#endif
	BOOL FASTCALL CheckMediaUpdate();
	BOOL FASTCALL CheckMediaAccess(BOOL bManual);
	BOOL FASTCALL CheckMedia();
	BOOL FASTCALL Eject();
	void FASTCALL GetVolume(TCHAR* szLabel);
	BOOL FASTCALL GetVolumeCache(TCHAR* szLabel) const;
	DWORD FASTCALL GetCapacity(Human68k::capacity_t* pCapacity);
	BOOL FASTCALL GetCapacityCache(Human68k::capacity_t* pCapacity) const;

private:
	void FASTCALL OpenDevice();
// Open the device.
	void FASTCALL CloseDevice();
// Close the device.

	BOOL m_bWriteProtect;
	BOOL m_bSlow;
	BOOL m_bRemovable;
	BOOL m_bManual;
	BOOL m_bEnable;
	BOOL m_bDevice;
	HANDLE m_hDevice;
	TCHAR m_szDevice[8];
	TCHAR m_szDrive[4];
	Human68k::capacity_t m_capCache;
	BOOL m_bVolumeCache;
	TCHAR m_szVolumeCache[24];
	DWORD m_nUpdate;
	BOOL m_bUpdateFile;
	DWORD m_nUpdateFile;
#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	DWORD m_nUpdateMedia;
#endif
#ifdef XM6_HOST_UPDATE_BY_FREQUENCY
	DWORD m_nUpdateCount;
	DWORD m_nUpdateBuffer[XM6_HOST_REMOVABLE_RELOAD_COUNT];
#endif

	TCHAR m_szBase[_MAX_PATH];
};

// Doubly linked ring list.
class CRing {
public:
	CRing() { next = prev = this; }
// Constructor.
	virtual ~CRing() { Remove(); }
	CRing* FASTCALL Next() const { ASSERT(this); return next; }
	CRing* FASTCALL Prev() const { ASSERT(this); return prev; }

	void FASTCALL Insert(class CRing* pRoot)
	{
		ASSERT(this);
		ASSERT(next);
		ASSERT(prev);
		next->prev = prev;
		prev->next = next;
		ASSERT(pRoot);
		ASSERT(pRoot->next);
		next = pRoot->next;
		prev = pRoot;
		pRoot->next->prev = this;
		pRoot->next = this;
	}

	void FASTCALL InsertTail(class CRing* pRoot)
	{
		ASSERT(this);
		ASSERT(next);
		ASSERT(prev);
		next->prev = prev;
		prev->next = next;
		ASSERT(pRoot);
		ASSERT(pRoot->prev);
		next = pRoot;
		prev = pRoot->prev;
		pRoot->prev->next = this;
		pRoot->prev = this;
	}

	void FASTCALL InsertRing(class CRing* pRoot)
	{
		ASSERT(this);
		if (next == prev) return;

		ASSERT(pRoot);
		ASSERT(pRoot->next);
		pRoot->next->prev = prev;
		prev->next = pRoot->next;
		pRoot->next = next;
		next->prev = pRoot;

		next = prev = this;
	}

	void FASTCALL Remove()
	{
		ASSERT(this);
		ASSERT(next);
		ASSERT(prev);
		next->prev = prev;
		prev->next = next;
		next = prev = this;
	}

	friend void FASTCALL Insert(class CRing* pRoot);
	friend void FASTCALL InsertTail(class CRing* pRoot);
	friend void FASTCALL Remove();

private:
	class CRing* next;
// Doubly linked ring list.
	class CRing* prev;
};

// Doubly linked ring list.
class CHostFilename: public CRing {
public:
	CHostFilename();
// Constructor.
	virtual ~CHostFilename();
// Destructor.
	void FASTCALL FreeWin32() { ASSERT(this); if(m_pszWin32) { free(m_pszWin32); m_pszWin32 = NULL; } }
	void FASTCALL SetWin32(const TCHAR* szWin32);
	void FASTCALL SetHuman(int nCount = -1);
	void FASTCALL CopyHuman(const BYTE* szHuman);
	BOOL FASTCALL SetEntry(const WIN32_FIND_DATA* pWin32Find);
	inline BOOL FASTCALL isReduce() const;
	BOOL FASTCALL isCorrect() const { ASSERT(this); return m_bCorrect; }
	BYTE* FASTCALL GetHuman() const { ASSERT(this); return (BYTE*)m_szHuman; }
	BYTE* FASTCALL GetHumanLast() const { ASSERT(this); return m_pszHumanLast; }
	BYTE* FASTCALL GetHumanExt() const { ASSERT(this); return m_pszHumanExt; }
	BOOL FASTCALL CheckAttribute(DWORD nHumanAttribute) const { ASSERT(this); return (m_dirEntry.attr & nHumanAttribute); }
	Human68k::dirent_t* FASTCALL GetEntry() const { ASSERT(this); return (Human68k::dirent_t*)&m_dirEntry; }
	TCHAR* FASTCALL GetWin32() const { ASSERT(this); return m_pszWin32; }
	void FASTCALL SetChild(class CHostEntry* pEntry, class CHostPath* pChild) { ASSERT(this); m_pEntry = pEntry; m_pChild = pChild; }
	BOOL FASTCALL isSameEntry(Human68k::dirent_t* pdirEntry) const { ASSERT(this); ASSERT(pdirEntry); return memcmp(&m_dirEntry, pdirEntry, sizeof(m_dirEntry)) == 0; }

	void FASTCALL DeleteCache();

	static void FASTCALL SetOption(DWORD nOption) { g_nOption = nOption; }

	static const BYTE* FASTCALL SeparateExt(const BYTE* szHuman);

private:
	inline BYTE* FASTCALL CopyName(BYTE* pWrite, const BYTE* pFirst, const BYTE* pLast);

	TCHAR* m_pszWin32;
	BOOL m_bCorrect;
	BYTE m_szHuman[24];
	Human68k::dirent_t m_dirEntry;

	BYTE* m_pszHumanLast;
	BYTE* m_pszHumanExt;

	class CHostEntry* m_pEntry;
	class CHostPath* m_pChild;

	static DWORD g_nOption;
};

// Doubly linked ring list.
class CHostPath: public CRing {
public:
	typedef struct {
		DWORD count;
		DWORD id;
		CHostFilename* pos;
		Human68k::dirent_t entry;

		void FASTCALL Clear() { count = 0; }
// Initialize.
	} find_t;

public:
	CHostPath();
// Constructor.
	virtual ~CHostPath();
// Destructor.
	void FASTCALL FreeHuman() { ASSERT(this); m_nHumanUnit = 0; if(m_pszHuman) { free(m_pszHuman); m_pszHuman = NULL; } }
	void FASTCALL FreeWin32() { ASSERT(this); if(m_pszWin32) { free(m_pszWin32); m_pszWin32 = NULL; } }
	void FASTCALL SetHuman(DWORD nUnit, BYTE* szHuman);
	void FASTCALL SetWin32(TCHAR* szWin32);
	BOOL FASTCALL isSameUnit(DWORD nUnit) const { ASSERT(this); return m_nHumanUnit == nUnit; }
	BOOL FASTCALL isSameHuman(DWORD nUnit, const BYTE* szHuman) const;
	TCHAR* FASTCALL GetWin32() const { ASSERT(this); return m_pszWin32; }
	CHostFilename* FASTCALL FindFilename(BYTE* szHuman, DWORD nHumanAttribute = Human68k::AT_ALL);
	CHostFilename* FASTCALL FindFilenameWildcard(BYTE* szHuman, find_t* pFind, DWORD nHumanAttribute = Human68k::AT_ALL);
	void FASTCALL Clean();
	void FASTCALL CleanFilename();
	BOOL FASTCALL isRefresh();
	BOOL FASTCALL Refresh();

	static void FASTCALL InitId() { g_nId = 0; }

	static void FASTCALL SetOption(DWORD nOption) { g_nOption = nOption; }

private:
	static inline int FASTCALL Compare(const BYTE* pFirst, const BYTE* pLast, const BYTE* pBufFirst, const BYTE* pBufLast);

	DWORD m_nHumanUnit;
	BYTE* m_pszHuman;
	TCHAR* m_pszWin32;
	HANDLE m_hChange;
	DWORD m_nId;
	CRing m_cRingFilename;

	static DWORD g_nId;

	static DWORD g_nOption;
};

class CHostEntry {
public:
	CHostEntry();
	virtual ~CHostEntry();
	void FASTCALL Init(CWinFileDrv** ppBase);
	void FASTCALL Clean();

#ifdef XM6_HOST_STRICT_TIMEOUT
	static DWORD WINAPI Run(VOID* pThis);
	void FASTCALL Runner();
#endif

	void FASTCALL LockCache() { ASSERT(this); EnterCriticalSection(&m_csAccess); }
	void FASTCALL UnlockCache() { ASSERT(this); LeaveCriticalSection(&m_csAccess); }

	void FASTCALL CleanCache();
	void FASTCALL EraseCache(DWORD nUnit);
	void FASTCALL DeleteCache(CHostPath* pPath);
	CHostPath* FASTCALL FindCache(DWORD nUnit, const BYTE* szHuman);
	CHostPath* FASTCALL CopyCache(DWORD nUnit, const BYTE* szHuman, TCHAR* szWin32Buffer = NULL);
	CHostPath* FASTCALL MakeCache(WindrvContext* ps, DWORD nUnit, const BYTE* szHuman, TCHAR* szWin32Buffer = NULL);

	TCHAR* FASTCALL GetBase(DWORD nUnit) const;
	BOOL FASTCALL isWriteProtect(WindrvContext* ps) const;
	BOOL FASTCALL isMediaOffline(WindrvContext* ps, BOOL bMedia = TRUE);
	BYTE FASTCALL GetMediaByte(DWORD nUnit) const;
	DWORD FASTCALL GetStatus(DWORD nUnit) const;
	void FASTCALL CheckTimeout();
#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	void FASTCALL SetMediaUpdate(WindrvContext* ps, BOOL bDisable);
#endif
	BOOL FASTCALL CheckMediaUpdate(WindrvContext* ps);
	BOOL FASTCALL CheckMediaAccess(DWORD nUnit, BOOL bErase);
	BOOL FASTCALL Eject(DWORD nUnit);
	void FASTCALL GetVolume(DWORD nUnit, TCHAR* szLabel);
	BOOL FASTCALL GetVolumeCache(DWORD nUnit, TCHAR* szLabel);
	DWORD FASTCALL GetCapacity(DWORD nUnit, Human68k::capacity_t* pCapacity);
	BOOL FASTCALL GetCapacityCache(DWORD nUnit, Human68k::capacity_t* pCapacity);

	void FASTCALL ShellNotify(DWORD nEvent, const TCHAR* szPath);

private:
	static inline const BYTE* FASTCALL SeparatePath(const BYTE* szHuman);
	static inline const BYTE* FASTCALL SeparateCopyFilename(const BYTE* szHuman, BYTE* szBuffer);

	CRITICAL_SECTION m_csAccess;
	CWinFileDrv** m_ppBase;

#ifdef XM6_HOST_STRICT_TIMEOUT
	HANDLE m_hEvent;
	HANDLE m_hThread;
#else
	DWORD m_nTimeout;
#endif

	DWORD m_nRingPath;
	CRing m_cRingPath;
};

// Doubly linked ring list.
class CHostFiles: public CRing {
public:
	CHostFiles() { Clear(); }
// Constructor.
	virtual ~CHostFiles() { Free(); }
// Destructor.
	void FASTCALL Free() { ASSERT(this); Clear(); }
	void FASTCALL Clear();
// Initialize.
	void FASTCALL SetKey(DWORD nKey) { ASSERT(this); m_nKey = nKey; }
	BOOL FASTCALL isSameKey(DWORD nKey) const { ASSERT(this); return m_nKey == nKey; }
	void FASTCALL SetPath(DWORD nUnit, const Human68k::namests_t* pNamests);
	BOOL FASTCALL isRootPath() const { return (strlen((char*)m_szHumanPath) == 1); }
// Determine whether this is the root directory.
	void FASTCALL SetPathWildcard() { m_nHumanWildcard = 1; }
	void FASTCALL SetPathOnly() { m_nHumanWildcard = 0xFF; }
	void FASTCALL SetAttribute(DWORD nHumanAttribute) { m_nHumanAttribute = nHumanAttribute; }
	BOOL FASTCALL Find(WindrvContext* ps, CHostEntry* pEntry, BOOL bRemove = FALSE);
	void FASTCALL AddFilename();
	const TCHAR* FASTCALL GetPath() const { ASSERT(this); return m_szWin32Result; }
	Human68k::dirent_t* FASTCALL GetEntry() const { ASSERT(this); return (Human68k::dirent_t*)&m_dirEntry; }
	DWORD FASTCALL GetAttribute() const { ASSERT(this); return m_dirEntry.attr; }
	WORD FASTCALL GetDate() const { ASSERT(this); return m_dirEntry.date; }
	WORD FASTCALL GetTime() const { ASSERT(this); return m_dirEntry.time; }
	DWORD FASTCALL GetSize() const { ASSERT(this); return m_dirEntry.size; }
	const BYTE* FASTCALL GetHumanResult() const { ASSERT(this); return m_szHumanResult; }

private:
	DWORD m_nKey;
	DWORD m_nHumanUnit;
	BYTE m_szHumanPath[HUMAN68K_MAX_PATH];
	BYTE m_szHumanFilename[24];
	DWORD m_nHumanWildcard;
	DWORD m_nHumanAttribute;
	CHostPath::find_t m_findNext;
	Human68k::dirent_t m_dirEntry;
	BYTE m_szHumanResult[24];
	TCHAR m_szWin32Result[_MAX_PATH];
};

// File search handling.
class CHostFilesManager {
public:
	CHostFilesManager();
	virtual ~CHostFilesManager();
	void FASTCALL Init();
	void FASTCALL Clean();
	CHostFiles* FASTCALL Alloc(DWORD nKey);
	CHostFiles* FASTCALL Search(DWORD nKey);
	void FASTCALL Free(CHostFiles* p);
private:
	CRing m_cRingFiles;
};

// Doubly linked ring list.
class CHostFcb: public CRing {
public:
	CHostFcb() { Clear(); }
// Constructor.
	virtual ~CHostFcb() { Free(); }
// Destructor.
	void FASTCALL Free() { ASSERT(this); Close(); Clear(); }
	void FASTCALL Clear();
	void FASTCALL SetKey(DWORD nKey) { ASSERT(this); m_nKey = nKey; }
	BOOL FASTCALL SetOpenMode(DWORD nHumanMode);
	void FASTCALL SetFilename(const TCHAR* szFilename);
	HANDLE FASTCALL Create(DWORD nHumanAttribute, BOOL bForce);
// Create the file.
	HANDLE FASTCALL Open();
	DWORD FASTCALL ReadFile(void* pBuffer, DWORD nSize);
// Read file data.
	DWORD FASTCALL WriteFile(void* pBuffer, DWORD nSize);
// Write file data.
	DWORD FASTCALL SetFilePointer(DWORD nOffset, DWORD nMode = FILE_BEGIN);
	DWORD FASTCALL SetFileTime(WORD nFatDate, WORD nFatTime);
	BOOL FASTCALL Close();
// Close the file.
	BOOL FASTCALL isSameKey(DWORD nKey) const { ASSERT(this); return m_nKey == nKey; }

private:
	DWORD m_nKey;
	DWORD m_nMode;
	HANDLE m_hFile;
	TCHAR m_szFilename[_MAX_PATH];
};

class CHostFcbManager {
public:
	CHostFcbManager();
	virtual ~CHostFcbManager();
	void FASTCALL Init();
	void FASTCALL Clean();
	CHostFcb* FASTCALL Alloc(DWORD nKey);
	CHostFcb* FASTCALL Search(DWORD nKey);
	void FASTCALL Free(CHostFcb* p);

private:
	CRing m_cRingFcb;
};

// Windows file system.
class CWinFileSys : public FileSys
{
public:
	CWinFileSys();
// Constructor.
	virtual ~CWinFileSys();
// Destructor.
	void FASTCALL ApplyCfg(const Config *pConfig);

	DWORD FASTCALL Init(WindrvContext* ps, DWORD nDriveMax, const BYTE* pOption);
// $40 - Initialize.
	void FASTCALL Reset();

	int FASTCALL CheckDir(WindrvContext* ps, const Human68k::namests_t* pNamests);
// $41 - Check the directory.
	int FASTCALL MakeDir(WindrvContext* ps, const Human68k::namests_t* pNamests);
// $42 - Create the directory.
	int FASTCALL RemoveDir(WindrvContext* ps, const Human68k::namests_t* pNamests);
// $43 - Delete the directory.
	int FASTCALL Rename(WindrvContext* ps, const Human68k::namests_t* pNamests, const Human68k::namests_t* pNamestsNew);
	int FASTCALL Delete(WindrvContext* ps, const Human68k::namests_t* pNamests);
// $45 - Delete the file.
	int FASTCALL Attribute(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nHumanAttribute);
	int FASTCALL Files(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::files_t* pFiles);
// $47 - Search files (first match).
	int FASTCALL NFiles(WindrvContext* ps, DWORD nKey, Human68k::files_t* pFiles);
// $48 - Search files (next match).
	int FASTCALL Create(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nHumanAttribute, BOOL bForce);
// $49 - Create the file.
	int FASTCALL Open(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::fcb_t* pFcb);
// $4A - Open the file.
	int FASTCALL Close(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb);
// $4B - Close the file.
	int FASTCALL Read(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nAddress, DWORD nSize);
// $4C - Read file data.
	int FASTCALL Write(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nAddress, DWORD nSize);
// $4D - Write file data.
	int FASTCALL Seek(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nMode, int nOffset);
// $4E - Seek within the file.
	DWORD FASTCALL TimeStamp(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, WORD nFatDate, WORD nFatTime);
	int FASTCALL GetCapacity(WindrvContext* ps, Human68k::capacity_t *pCapacity);
	int FASTCALL CtrlDrive(WindrvContext* ps, Human68k::ctrldrive_t* pCtrlDrive);
	int FASTCALL GetDPB(WindrvContext* ps, Human68k::dpb_t* pDpb);
// $52 - Get the DPB.
	int FASTCALL DiskRead(WindrvContext* ps, DWORD nAddress, DWORD nSector, DWORD nSize);
// $53 - Read sectors.
	int FASTCALL DiskWrite(WindrvContext* ps, DWORD nAddress, DWORD nSector, DWORD nSize);
// $54 - Write sectors.
	int FASTCALL IoControl(WindrvContext* ps, Human68k::ioctrl_t* pIoctrl, DWORD nFunction);
// $55 - I/O control.
	int FASTCALL Flush(WindrvContext* ps);
	int FASTCALL CheckMedia(WindrvContext* ps);
	int FASTCALL Lock(WindrvContext* ps);

	void FASTCALL ShellNotify(DWORD nEvent, const TCHAR* szPath) { ASSERT(this); m_cEntry.ShellNotify(nEvent, szPath); }
// Option flags.

	enum {
		DrvMax = 10						// File-system detail.
	};

private:

	DWORD FASTCALL GetOption() const { ASSERT(this); return m_nOption; }
	void FASTCALL SetOption(DWORD nOption);
	void FASTCALL InitOption(const BYTE* pOption);
	inline BOOL FASTCALL FilesVolume(WindrvContext* ps, Human68k::files_t* pFiles);
// Read the volume label.
	void FASTCALL CheckKernel(WindrvContext* ps);

	DWORD m_bResume;
	DWORD m_nDrives;
	DWORD m_nFlag[DrvMax];
	TCHAR m_szBase[DrvMax][_MAX_PATH];

	CWinFileDrv* m_pDrv[DrvMax];

	CHostFilesManager m_cFiles;
	CHostFcbManager m_cFcb;
	CHostEntry m_cEntry;
// Keep the directory-entry object alive.

	DWORD m_nHostSectorCount;
	DWORD m_nHostSectorBuffer[XM6_HOST_PSEUDO_CLUSTER_MAX];

	DWORD m_nKernel;
	DWORD m_nKernelSearch;

	DWORD m_nOptionDefault;
	DWORD m_nOption;
};

// Host component.
// Host component.
class CHost : public CComponent
{
public:
	CHost(CFrmWnd *pWnd);
// Constructor.
	BOOL FASTCALL Init();
// Initialize.
	void FASTCALL Cleanup();
	void FASTCALL ApplyCfg(const Config *pConfig);

	void FASTCALL ShellNotify(DWORD nEvent, const TCHAR* szPath) { ASSERT(this); m_WinFileSys.ShellNotify(nEvent, szPath); }

private:
	CWinFileSys m_WinFileSys;
};

#endif
#endif
