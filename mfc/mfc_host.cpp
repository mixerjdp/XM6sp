
//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[MFC application]
//
//---------------------------------------------------------------------------


#if defined(_WIN32)

#include "mfc.h"

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "host_fs.h"
#include "memory.h"
#include "mfc_com.h"
#include "mfc_cfg.h"
#include "mfc_host.h"

#include <shlobj.h>
#include <winioctl.h>

// Constructor.
// Windows file drive.
// Constructor.

// Constructor.
CWinFileDrv::CWinFileDrv()
{
// Initialize.
	m_bWriteProtect = FALSE;
	m_bSlow = FALSE;
	m_bRemovable = FALSE;
	m_bManual = FALSE;
	m_bEnable = FALSE;
	m_bDevice = FALSE;
	m_hDevice = INVALID_HANDLE_VALUE;
	m_szDevice[0] = _T('\0');
	m_szDrive[0] = _T('\0');
	memset(&m_capCache, 0, sizeof(m_capCache));
	m_bVolumeCache = FALSE;
	m_szVolumeCache[0] = _T('\0');
	m_nUpdate = 0;
	m_bUpdateFile = FALSE;
	m_nUpdateFile = 0;
#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	m_nUpdateMedia = 0;
#endif
#ifdef XM6_HOST_UPDATE_BY_FREQUENCY
	m_nUpdateCount = 0;
	memset(m_nUpdateBuffer, 0, sizeof(m_nUpdateBuffer));
#endif
	m_szBase[0] = _T('\0');
}

// Destructor.
CWinFileDrv::~CWinFileDrv()
{
#ifdef XM6_HOST_KEEP_OPEN_ERROR
// Close the device.
	CloseDevice();
#endif
}

// Open the device.
void FASTCALL CWinFileDrv::OpenDevice()
{
	ASSERT(this);
	ASSERT(m_hDevice == INVALID_HANDLE_VALUE);

	m_hDevice = ::CreateFile(
		m_szDevice,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
}

// Close the device.
void FASTCALL CWinFileDrv::CloseDevice()
{
	ASSERT(this);

	if (m_hDevice != INVALID_HANDLE_VALUE) {
		CloseHandle(m_hDevice);
		m_hDevice = INVALID_HANDLE_VALUE;
	}
}

// Initialize.
void FASTCALL CWinFileDrv::Init(LPCTSTR szBase, DWORD nFlag)
{
	ASSERT(this);
	ASSERT(szBase);
	ASSERT(_tcslen(szBase) < _MAX_PATH);
	ASSERT(m_bWriteProtect == FALSE);
	ASSERT(m_bSlow == FALSE);
	ASSERT(m_bRemovable == FALSE);
	ASSERT(m_bManual == FALSE);
	ASSERT(m_bEnable == FALSE);
	ASSERT(m_bDevice == FALSE);
	ASSERT(m_hDevice == INVALID_HANDLE_VALUE);
	ASSERT(m_szDevice[0] == _T('\0'));
	ASSERT(m_szDrive[0] == _T('\0'));
	ASSERT(m_capCache.sectors == 0);
	ASSERT(m_bVolumeCache == FALSE);
	ASSERT(m_szVolumeCache[0] == _T('\0'));
	ASSERT(m_nUpdate == 0);
	ASSERT(m_bUpdateFile == FALSE);
	ASSERT(m_nUpdateFile == 0);
#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	ASSERT(m_nUpdateMedia == 0);
#endif
#ifdef XM6_HOST_UPDATE_BY_FREQUENCY
	ASSERT(m_nUpdateCount == 0);
	ASSERT(m_szBase[0] == _T('\0'));
#endif

	if (nFlag & FSFLAG_WRITE_PROTECT) m_bWriteProtect = TRUE;
	if (nFlag & FSFLAG_SLOW) m_bSlow = TRUE;
	if (nFlag & FSFLAG_REMOVABLE) m_bRemovable = TRUE;
	if (nFlag & FSFLAG_MANUAL) m_bManual = TRUE;
	_tcscpy(m_szBase, szBase);

	TCHAR* pClear = NULL;
	TCHAR* p = m_szBase;
	for (;;) {
		TCHAR c = *p;
		if (c == _T('\0')) break;
		if (c == '/' || c== '\\') {
			pClear = p;
		} else {
			pClear = NULL;
		}
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// File-system detail.
			p++;
			if (*p == _T('\0')) break;
		}
		p++;
	}
	if (pClear) *pClear = _T('\0');

	DWORD n = m_szBase[0] & 0xDF;
	if (n < 'A' || 'Z' < n || m_szBase[1] != ':') {
		m_bSlow = TRUE;
		CheckMedia();
		return;
	}

	_stprintf(m_szDrive, _T("%c:\\"), n);

	DWORD nResult = ::GetDriveType(m_szDrive);
	switch (nResult) {
	case DRIVE_REMOVABLE:
	{
		m_bSlow = TRUE;
		m_bRemovable = TRUE;

		SHFILEINFO shfi;
		nResult = ::SHGetFileInfo(m_szDrive, 0, &shfi, sizeof(shfi), SHGFI_TYPENAME);
		if (nResult) {
			if (_tcsstr(shfi.szTypeName, _T("フロッピー")) != NULL ||
				_tcsstr(shfi.szTypeName, _T("Floppy")) != NULL)
				m_bManual = TRUE;
		}
		break;
	}

	case DRIVE_CDROM:
		m_bSlow = TRUE;
		m_bRemovable = TRUE;
		break;

	case DRIVE_FIXED:
	case DRIVE_RAMDISK:
		break;

	default:
		m_bSlow = TRUE;
		break;
	}


	OSVERSIONINFO v;
	memset(&v, 0, sizeof(v));
	v.dwOSVersionInfoSize = sizeof(v);
	::GetVersionEx(&v);
	if (v.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		m_bDevice = TRUE;
		_stprintf(m_szDevice, _T("\\\\.\\%c:"), n);
	}

#ifdef XM6_HOST_KEEP_OPEN_ERROR

// Open the device.
	OpenDevice();
#endif

	if (m_bManual == FALSE) CheckMedia();
}

DWORD FASTCALL CWinFileDrv::GetStatus() const
{
	ASSERT(this);

	return
		(m_bRemovable ? 0 : 0x40) |
		(m_bEnable ? (m_bWriteProtect ? 0x08 : 0) | 0x02 : 0);
}

// Set the media state.
void FASTCALL CWinFileDrv::SetEnable(BOOL bEnable)
{
	ASSERT(this);

	m_bEnable = bEnable;

	if (bEnable == FALSE) {
		memset(&m_capCache, 0, sizeof(m_capCache));
		m_bVolumeCache = FALSE;
		m_szVolumeCache[0] = _T('\0');
	}
}

void FASTCALL CWinFileDrv::SetTimeout()
{
	ASSERT(this);

	if (m_bRemovable == FALSE) return;

	m_bUpdateFile = TRUE;
	m_nUpdateFile = ::GetTickCount();
}

BOOL FASTCALL CWinFileDrv::CheckTimeout()
{
	ASSERT(this);

	if (m_bRemovable == FALSE) return FALSE;

	DWORD nCount = ::GetTickCount();
	DWORD nDelta = nCount - m_nUpdateFile;
	if (nDelta < XM6_HOST_REMOVABLE_CACHE_TIME) return FALSE;	// File-system detail.

	if (m_bUpdateFile == FALSE) return FALSE;	// File-system detail.
	m_bUpdateFile = FALSE;

	return TRUE;	// File-system detail.
}

#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
void FASTCALL CWinFileDrv::SetMediaUpdate(BOOL bDisable)
{
	ASSERT(this);
	if (bDisable) {
		m_nUpdateMedia = 2;
	} else {
		if (m_nUpdateMedia > 1) {
			m_nUpdateMedia = 0;
		} else {
			m_nUpdateMedia = 1;
		}
	}
}
#endif

BOOL FASTCALL CWinFileDrv::CheckMediaUpdate()
{
	ASSERT(this);

	if (m_bRemovable == FALSE) return FALSE;

#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	if (m_nUpdateMedia == 1) {
		m_nUpdateMedia = 0;
		return TRUE;	// File-system detail.
	}
	m_nUpdateMedia = 0;	// File-system detail.
#endif

#ifdef XM6_HOST_UPDATE_BY_FREQUENCY
	DWORD n = ::GetTickCount();
	DWORD nOld = m_nUpdateBuffer[m_nUpdateCount];
	m_nUpdateBuffer[m_nUpdateCount] = n;
	m_nUpdateCount++;
	m_nUpdateCount %= XM6_HOST_REMOVABLE_RELOAD_COUNT;

	if (n - nOld <= XM6_HOST_REMOVABLE_RELOAD_TIME) return TRUE;	// File-system detail.
#endif

#ifdef XM6_HOST_UPDATE_ALWAYS
	return TRUE;	// File-system detail.
#endif

	return FALSE;	// File-system detail.
}

BOOL FASTCALL CWinFileDrv::CheckMediaAccess(BOOL bManual)
{
	ASSERT(this);

	if (bManual) {
		if (m_bManual == FALSE) return FALSE;
	} else {
		if (m_bRemovable == FALSE) return FALSE;
	}

	DWORD nCount = ::GetTickCount();
	DWORD nDelta = nCount - m_nUpdate;
	if (nDelta < XM6_HOST_REMOVABLE_GUARD_TIME) return FALSE;	// File-system detail.
	m_nUpdate = nCount;

	return TRUE;	// File-system detail.
}

BOOL FASTCALL CWinFileDrv::CheckMedia()
{
	ASSERT(this);

	BOOL bEnable = TRUE;

	if (m_bDevice) {
		bEnable = FALSE;

// Open the device.
		OpenDevice();

		if (m_hDevice != INVALID_HANDLE_VALUE) {
			DWORD nSize;
			bEnable = ::DeviceIoControl(
				m_hDevice,
				IOCTL_STORAGE_CHECK_VERIFY,
				NULL,
				0,
				NULL,
				0,
				&nSize,
				NULL);
		}

// Close the device.
		CloseDevice();
	}

	SetEnable(bEnable);

	m_nUpdate = ::GetTickCount();

	return bEnable;
}

BOOL FASTCALL CWinFileDrv::Eject()
{
	ASSERT(this);

	if (m_bRemovable == FALSE) return FALSE;

	if (m_bEnable == FALSE) return FALSE;

	if (m_bDevice == FALSE) return FALSE;

// Open the device.
	OpenDevice();
	if (m_hDevice == INVALID_HANDLE_VALUE) return FALSE;

	DWORD nSize;
	BOOL bResult = DeviceIoControl(
		m_hDevice,
		FSCTL_DISMOUNT_VOLUME,
		NULL,
		0,
		NULL,
		0,
		&nSize,
		NULL);

	bResult = ::DeviceIoControl(
		m_hDevice,
		IOCTL_STORAGE_EJECT_MEDIA,
		NULL,
		0,
		NULL,
		0,
		&nSize,
		NULL);

// Close the device.
	CloseDevice();

	return bResult;
}

void FASTCALL CWinFileDrv::GetVolume(TCHAR* szLabel)
{
	ASSERT(this);
	ASSERT(szLabel);

#if 0	// File-system detail.
	if (m_bEnable == FALSE) {
		m_bVolumeCache = FALSE;
		m_szVolumeCache[0] = _T('\0');
		szLabel[0] = _T('\0');
		return;
	}
#endif

	DWORD dwMaximumComponentLength;
	DWORD dwFileSystemFlags;
	BOOL bResult = ::GetVolumeInformation(
		m_szDrive,
		m_szVolumeCache,
		sizeof(m_szVolumeCache) / sizeof(TCHAR),
		NULL,
		&dwMaximumComponentLength,
		&dwFileSystemFlags,
		NULL,
		0);
	if (bResult == 0) {
		m_bVolumeCache = FALSE;
		m_szVolumeCache[0] = _T('\0');
		szLabel[0] = _T('\0');
		return;
	}

	m_bVolumeCache = TRUE;
	_tcscpy(szLabel, m_szVolumeCache);
}

BOOL FASTCALL CWinFileDrv::GetVolumeCache(TCHAR* szLabel) const
{
	ASSERT(this);
	ASSERT(szLabel);

	_tcscpy(szLabel, m_szVolumeCache);

	if (m_bEnable == FALSE) return TRUE;

	return m_bVolumeCache;
}

DWORD FASTCALL CWinFileDrv::GetCapacity(Human68k::capacity_t* pCapacity)
{
	ASSERT(this);
	ASSERT(pCapacity);

#if 0	// File-system detail.
	if (m_bEnable == FALSE) {
		memset(&m_capCache, 0, sizeof(m_capCache));
		memset(pCapacity, 0, sizeof(*pCapacity));
		return 0;
	}
#endif

	ULARGE_INTEGER ulFree;
	ULARGE_INTEGER ulTotal;
	ULARGE_INTEGER ulTotalFree;
	BOOL bResult = ::GetDiskFreeSpaceEx(m_szDrive, &ulFree, &ulTotal, &ulTotalFree);
	if (bResult == FALSE) {
		memset(&m_capCache, 0, sizeof(m_capCache));
		memset(pCapacity, 0, sizeof(*pCapacity));
		return 0;
	}

	DWORD nFree = 0x7FFFFFFF;
	if (ulFree.HighPart == 0 && ulFree.LowPart < 0x80000000) {
		nFree = ulFree.LowPart;
	}

	DWORD free;
	DWORD clusters;
	DWORD sectors;
	DWORD bytes;
	if (ulTotal.HighPart == 0 && ulTotal.LowPart < 0x80000000) {
		bResult = ::GetDiskFreeSpace(m_szDrive, &sectors, &bytes, &free, &clusters);
		if (bResult == FALSE) {	// File-system detail.
			free = nFree;
			clusters = ulTotal.LowPart;
			sectors = 1;
			bytes = 512;
		}

		while (bytes > 512) {
			bytes >>= 1;
			sectors <<= 1;

			free <<= 1;
		}

		while (1 <= bytes && bytes <= 256) {
			bytes <<= 1;
			if (sectors > 1) {
				sectors >>= 1;
			} else {
				clusters >>= 1;
			}

			free >>= 1;
		}

		while (clusters >= 0x10000) {
			clusters >>= 1;
			sectors <<= 1;

			free >>= 1;
		}
	} else {
		sectors = 0x80;
		clusters = 0x8000;
		bytes = 512;

		free = (WORD)(nFree >> 16);
		if (nFree & 0xFFFF) free++;
	}

	ASSERT(free < 0x10000);
	ASSERT(clusters < 0x10000);
	ASSERT(sectors < 0x10000);
	ASSERT(bytes == 512);

	m_capCache.free = (WORD)free;
	m_capCache.clusters = (WORD)clusters;
	m_capCache.sectors = (WORD)sectors;
	m_capCache.bytes = 512;

	memcpy(pCapacity, &m_capCache, sizeof(m_capCache));

	return nFree;
}

BOOL FASTCALL CWinFileDrv::GetCapacityCache(Human68k::capacity_t* pCapacity) const
{
	ASSERT(this);
	ASSERT(pCapacity);

	memcpy(pCapacity, &m_capCache, sizeof(m_capCache));

	if (m_bEnable == FALSE) return TRUE;

	return (m_capCache.sectors != 0);
}


DWORD CHostFilename::g_nOption;			// File-system detail.

// Constructor.
CHostFilename::CHostFilename()
{
	m_pszWin32 = NULL;
	m_bCorrect = FALSE;
	m_szHuman[0] = '\0';
	m_pszHumanLast = m_szHuman;
	m_pszHumanExt = m_szHuman;
	m_pEntry = NULL;
	m_pChild = NULL;
}

// Destructor.
CHostFilename::~CHostFilename()
{
	FreeWin32();
	DeleteCache();
}

void FASTCALL CHostFilename::SetWin32(const TCHAR* szWin32)
{
	ASSERT(this);
	ASSERT(szWin32);
	ASSERT(_tcslen(szWin32) < _MAX_PATH);
	ASSERT(m_pszWin32 == NULL);

	m_pszWin32 = (TCHAR*)malloc(_tcslen(szWin32) + 1);
	ASSERT(m_pszWin32);
	_tcscpy(m_pszWin32, szWin32);
}

BYTE* FASTCALL CHostFilename::CopyName(BYTE* pWrite, const BYTE* pFirst, const BYTE* pLast)
{
	ASSERT(this);
	ASSERT(pWrite);
	ASSERT(pFirst);
	ASSERT(pLast);

	for (const BYTE* p = pFirst; p < pLast; p++) {
		*pWrite++ = *p;
	}

	return pWrite;
}

void FASTCALL CHostFilename::SetHuman(int nCount)
{
	ASSERT(this);
	ASSERT(m_pszWin32);
	ASSERT(_tcslen(m_pszWin32) < _MAX_PATH);

	if (m_pszWin32[0] == '.') {
		if (m_pszWin32[1] == '\0' || (m_pszWin32[1] == '.' && m_pszWin32[2] == '\0')) {
			strcpy((char*)m_szHuman, (char*)m_pszWin32);	// File-system detail.

			m_bCorrect = TRUE;
			m_pszHumanLast = m_szHuman + strlen((char*)m_szHuman);
			m_pszHumanExt = m_pszHumanLast;
			return;
		}
	}

	DWORD nMax = 18;	// File-system detail.
	if (g_nOption & WINDRV_OPT_CONVERT_LENGTH) nMax = 8;

	BYTE szNumber[8];
	BYTE* pNumber = NULL;
	if (nCount >= 0) {
		pNumber = &szNumber[8];
		for (int i = 0; i < 5; i++) {	// File-system detail.
			int n = nCount % 36;
			nMax--;
			pNumber--;
			*pNumber = (BYTE)n + (n < 10 ? '0' : 'A' - 10);
			nCount /= 36;
			if (nCount == 0) break;
		}
		nMax--;
		pNumber--;
		BYTE c = (BYTE)(g_nOption >> 24) & 0x7F;
		if (c == 0) c = XM6_HOST_FILENAME_MARK;
		*pNumber = c;
	}

	BYTE szHuman[_MAX_PATH];
	const BYTE* pFirst = szHuman;
	const BYTE* pLast;
	const BYTE* pExt = NULL;

	{
		const BYTE* pRead = (const BYTE*)m_pszWin32;
		BYTE* pWrite = szHuman;
		const BYTE* pPeriod = SeparateExt(pRead);

		for (bool bFirst = true; ; bFirst = false) {
			BYTE c = *pRead++;
			switch (c) {
			case ' ':
				if (g_nOption & WINDRV_OPT_REDUCED_SPACE) continue;
				if (g_nOption & WINDRV_OPT_CONVERT_SPACE) c = '_';
				else if (pWrite == szHuman) continue;	// File-system detail.
				break;
			case '=':
			case '+':
				if (g_nOption & WINDRV_OPT_REDUCED_BADCHAR) continue;
				if (g_nOption & WINDRV_OPT_CONVERT_BADCHAR) c = '_';
				break;
			case '-':
				if (bFirst) {
					if (g_nOption & WINDRV_OPT_REDUCED_HYPHEN) continue;
					if (g_nOption & WINDRV_OPT_CONVERT_HYPHEN) c = '_';
					break;
				}
				if (g_nOption & WINDRV_OPT_REDUCED_HYPHENS) continue;
				if (g_nOption & WINDRV_OPT_CONVERT_HYPHENS) c = '_';
				break;
			case '.':
				if (pRead - 1 == pPeriod) {		// File-system detail.
					pExt = pWrite;
					break;
				}
				if (bFirst) {
					if (g_nOption & WINDRV_OPT_REDUCED_PERIOD) continue;
					if (g_nOption & WINDRV_OPT_CONVERT_PERIOD) c = '_';
					break;
				}
				if (g_nOption & WINDRV_OPT_REDUCED_PERIODS) continue;
				if (g_nOption & WINDRV_OPT_CONVERT_PERIODS) c = '_';
				break;
			}
			*pWrite++ = c;
			if (c == '\0') break;
		}

		pLast = pWrite - 1;
	}

	if (pExt) {
		while(pExt < pLast - 1 && *(pLast - 1) == ' ') {
			pLast--;
			BYTE* p = (BYTE*)pLast;
			*p = '\0';
		}

		if (pExt + 1 >= pLast) {
			pLast = pExt;
			BYTE* p = (BYTE*)pLast;
			*p = '\0';		// File-system detail.
		}
	} else {
		pExt = pLast;
	}


	const BYTE* pCut = pFirst;
	const BYTE* pStop = pExt - nMax;	// File-system detail.
	if (pFirst < pExt) {
		pCut++;		// File-system detail.
		BYTE c = *pFirst;
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// File-system detail.
			pCut++;		// File-system detail.
			pStop++;	// File-system detail.
		}
	}
	if (pStop < pFirst) pStop = pFirst;

	pCut = (BYTE*)strchr((char*)pCut, '.');	// File-system detail.
	if (pCut == NULL) pCut = pLast;
	if ((DWORD)(pCut - pFirst) > nMax) {
		pCut = pFirst + nMax;	// File-system detail.
	}

	const BYTE* pSecond = pExt;
	for (const BYTE* p = pExt - 1; pStop < p; p--) {
		if (*p == '.') pSecond = p;	// File-system detail.
	}

	DWORD nExt = pExt - pSecond;	// File-system detail.
	if ((DWORD)(pCut - pFirst) + nExt > nMax) pCut = pFirst + nMax - nExt;
	
	for (const BYTE* p = pFirst; p < pCut; p++) {
		BYTE c = *p;
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// File-system detail.
			p++;
			if (p >= pCut) {
				pCut--;
				break;
			}
		}
	}

	BYTE* pWrite = m_szHuman;
	pWrite = CopyName(pWrite, pFirst, pCut);	// File-system detail.
	if (pNumber) pWrite = CopyName(pWrite, pNumber, &szNumber[8]);	// File-system detail.
	pWrite = CopyName(pWrite, pSecond, pExt);	// File-system detail.
	m_pszHumanExt = pWrite;						// File-system detail.
	pWrite = CopyName(pWrite, pExt, pLast);		// File-system detail.
	m_pszHumanLast = pWrite;					// File-system detail.
	*pWrite = '\0';

	m_bCorrect = TRUE;
	int nSize = m_pszHumanExt - m_szHuman;				// File-system detail.

	if (nSize <= 0) m_bCorrect = FALSE;

	else if (m_szHuman[nSize - 1] == ' ') m_bCorrect = FALSE;

	if (m_szHuman[0] == '.') {
		if (m_szHuman[1] == '\0' || (m_szHuman[1] == '.' && m_szHuman[2] == '\0')) {
			m_bCorrect = FALSE;
		}
	}
}

void FASTCALL CHostFilename::CopyHuman(const BYTE* szHuman)
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(strlen((char*)szHuman) < 23);

	strcpy((char*)m_szHuman, (char*)szHuman);
	m_bCorrect = TRUE;
	m_pszHumanLast = m_szHuman + strlen((char*)m_szHuman);
	m_pszHumanExt = (BYTE*)SeparateExt(m_szHuman);
}

BOOL FASTCALL CHostFilename::SetEntry(const WIN32_FIND_DATA* pWin32Find)
{
	ASSERT(this);
	ASSERT(pWin32Find);

	BYTE* p = m_szHuman;
	for (int i = 0; i < 8; i++) {
		if (p < m_pszHumanExt)
			m_dirEntry.name[i] = *p++;
		else
			m_dirEntry.name[i] = ' ';
	}

	for (int i = 0; i < 10; i++) {
		if (p < m_pszHumanExt)
			m_dirEntry.add[i] = *p++;
		else
			m_dirEntry.add[i] = '\0';
	}

	if (*p == '.') p++;
	for (int i = 0; i < 3; i++) {
		BYTE c = *p;
		if (c) p++;
		m_dirEntry.ext[i] = c;
	}

	DWORD n = pWin32Find->dwFileAttributes;
	BYTE nHumanAttribute = Human68k::AT_ARCHIVE;
	if ((n & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		nHumanAttribute = Human68k::AT_DIRECTORY;
	}
	if ((n & FILE_ATTRIBUTE_SYSTEM) != 0) nHumanAttribute |= Human68k::AT_SYSTEM;
	if ((n & FILE_ATTRIBUTE_HIDDEN) != 0) nHumanAttribute |= Human68k::AT_HIDDEN;
	if ((n & FILE_ATTRIBUTE_READONLY) != 0) nHumanAttribute |= Human68k::AT_READONLY;
	m_dirEntry.attr = nHumanAttribute;

	m_dirEntry.size = pWin32Find->nFileSizeLow;
	if (pWin32Find->nFileSizeHigh > 0 || pWin32Find->nFileSizeLow > XM6_HOST_FILESIZE_MAX)
		m_dirEntry.size = XM6_HOST_FILESIZE_MAX;

	m_dirEntry.date = 0;
	m_dirEntry.time = 0;
	FILETIME ft = pWin32Find->ftLastWriteTime;
	if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) {
		ft = pWin32Find->ftCreationTime;
	}
	FILETIME lt;
	if (::FileTimeToLocalFileTime(&ft, &lt) == 0) return FALSE;
	if (::FileTimeToDosDateTime(&lt, &m_dirEntry.date, &m_dirEntry.time) == 0) return FALSE;

	m_dirEntry.cluster = 0;

	return TRUE;
}

BOOL FASTCALL CHostFilename::isReduce() const
{
	ASSERT(this);
	ASSERT(m_pszWin32);

	return strcmp((char*)m_pszWin32, (char*)m_szHuman) != 0; // File-system detail.
}

void FASTCALL CHostFilename::DeleteCache()
{
	ASSERT(this);

	if (m_pEntry) m_pEntry->DeleteCache(m_pChild);
}

const BYTE* FASTCALL CHostFilename::SeparateExt(const BYTE* szHuman)
{
	int nLength = strlen((char*)szHuman);
	const BYTE* pFirst = szHuman;
	const BYTE* pLast = pFirst + nLength;

	const BYTE* pExt = (BYTE*)strrchr((char*)pFirst, '.');	// File-system detail.
	if (pExt == NULL) pExt = pLast;
	if (20 <= nLength && nLength <= 22 && pFirst[18] == '.' && pFirst[nLength-1] == '.')
		pExt = pFirst + 18;
	int nExt = pLast - pExt - 1;
	if (pExt == pFirst || nExt < 1 || nExt > 3) pExt = pLast;

	return pExt;
}


DWORD CHostPath::g_nId;				// File-system detail.
DWORD CHostPath::g_nOption;			// File-system detail.

// Constructor.
CHostPath::CHostPath()
{
	m_nHumanUnit = 0;
	m_pszHuman = NULL;
	m_pszWin32 = NULL;
	m_hChange = INVALID_HANDLE_VALUE;
	m_nId = g_nId++;
}

// Destructor.
CHostPath::~CHostPath()
{
#if 0
	OutputDebugString("Cache Delete ");
	OutputDebugString(m_pszWin32);
	OutputDebugString("\r\n");
#endif

	Clean();
}

void FASTCALL CHostPath::SetHuman(DWORD nUnit, BYTE* szHuman)
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(strlen((char*)szHuman) < HUMAN68K_MAX_PATH);
	ASSERT(m_pszHuman == NULL);

	m_pszHuman = (BYTE*)malloc(strlen((char*)szHuman) + 1);
	ASSERT(m_pszHuman);
	strcpy((char*)m_pszHuman, (char*)szHuman);
	m_nHumanUnit = nUnit;
}

void FASTCALL CHostPath::SetWin32(TCHAR* szWin32)
{
	ASSERT(this);
	ASSERT(szWin32);
	ASSERT(_tcslen(szWin32) < _MAX_PATH);
	ASSERT(m_pszWin32 == NULL);

	m_pszWin32 = (TCHAR*)malloc(_tcslen(szWin32) + 1);
	ASSERT(m_pszWin32);
	_tcscpy(m_pszWin32, szWin32);
}

int FASTCALL CHostPath::Compare(const BYTE* pFirst, const BYTE* pLast, const BYTE* pBufFirst, const BYTE* pBufLast)
{
	ASSERT(pFirst);
	ASSERT(pLast);
	ASSERT(pBufFirst);
	ASSERT(pBufLast);

	BOOL bSkip0 = FALSE;
	BOOL bSkip1 = FALSE;
	for (const BYTE* p = pFirst; p < pLast; p++) {
		BYTE c = *p;
		BYTE d = '\0';
		if (pBufFirst < pBufLast) d = *pBufFirst++;

		if (bSkip0 == FALSE) {
			if (bSkip1 == FALSE) {	// File-system detail.
				if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// File-system detail.
					bSkip0 = TRUE;
				}
				if ((0x80 <= d && d <= 0x9F) || 0xE0 <= d) {	// File-system detail.
					bSkip1 = TRUE;
				}
				if (c == d) continue;	// File-system detail.
				if ((g_nOption & WINDRV_OPT_ALPHABET) == 0) {
					if ('A' <= c && c <= 'Z') c += 'a' - 'A';	// File-system detail.
					if ('A' <= d && d <= 'Z') d += 'a' - 'A';	// File-system detail.
				}
			} else {		// File-system detail.
				if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// File-system detail.
					bSkip0 = TRUE;
				}
				bSkip1 = FALSE;
			}
		} else {
			if (bSkip1 == FALSE) {	// File-system detail.
				bSkip0 = FALSE;
				if ((0x80 <= d && d <= 0x9F) || 0xE0 <= d) {	// File-system detail.
					bSkip1 = TRUE;
				}
			} else {		// File-system detail.
				bSkip0 = FALSE;
				bSkip1 = FALSE;
			}
		}

		if (c == d) continue;
		if (c == '?') continue;
		return 1;
	}
	if (pBufFirst < pBufLast) return 2;

	return 0;
}

BOOL FASTCALL CHostPath::isSameHuman(DWORD nUnit, const BYTE* szHuman) const
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(m_pszHuman);

	if (m_nHumanUnit != nUnit) return FALSE;

	const BYTE* pFirst = m_pszHuman;
	DWORD nLength = strlen((char*)pFirst);
	const BYTE* pLast = pFirst + nLength;

	const BYTE* pBufFirst = szHuman;
	DWORD nBufLength = strlen((char*)pBufFirst);
	const BYTE* pBufLast = pBufFirst + nBufLength;

	if (nLength != nBufLength) return FALSE;

	return Compare(pFirst, pLast, pBufFirst, pBufLast) == 0;
}

CHostFilename* FASTCALL CHostPath::FindFilename(BYTE* szHuman, DWORD nHumanAttribute)
{
	ASSERT(this);
	ASSERT(szHuman);

	const BYTE* pFirst = szHuman;
	DWORD nLength = strlen((char*)pFirst);
	const BYTE* pLast = pFirst + nLength;

	for (CHostFilename* p = (CHostFilename*)m_cRingFilename.Next();
		 p != &m_cRingFilename; p = (CHostFilename*)p->Next()) {
		if (p->CheckAttribute(nHumanAttribute) == 0) continue;
		const BYTE* pBufFirst = p->GetHuman();
		const BYTE* pBufLast = p->GetHumanLast();
		DWORD nBufLength = pBufLast - pBufFirst;
		if (nLength != nBufLength) continue;
		if (Compare(pFirst, pLast, pBufFirst, pBufLast) == 0) return p;
	}

	return NULL;
}

CHostFilename* FASTCALL CHostPath::FindFilenameWildcard(BYTE* szHuman, find_t* pFind, DWORD nHumanAttribute)
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(pFind);

	const BYTE* pFirst = szHuman;
	const BYTE* pLast = pFirst + strlen((char*)pFirst);
	const BYTE* pExt = CHostFilename::SeparateExt(pFirst);

	CHostFilename* p = (CHostFilename*)m_cRingFilename.Next();
	if (pFind->count > 0) {
		if (pFind->id == m_nId) {
			p = pFind->pos;
		} else {
			DWORD n = 0;
			for (; p != &m_cRingFilename; p = (CHostFilename*)p->Next()) {
				if (p->isSameEntry(&pFind->entry)) {
					pFind->count = n;
					break;
				}
				n++;
			}

			if (p == &m_cRingFilename) {
				CHostFilename* p = (CHostFilename*)m_cRingFilename.Next();
				n = 0;
				for (; p != &m_cRingFilename; p = (CHostFilename*)p->Next()) {
					if (n >= pFind->count) break;
					n++;
				}
			}
		}
	}

// File search.
	for (; p != &m_cRingFilename; p = (CHostFilename*)p->Next()) {
		pFind->count++;

		if (p->CheckAttribute(nHumanAttribute) == 0) continue;

		const BYTE* pBufFirst = p->GetHuman();
		const BYTE* pBufLast = p->GetHumanLast();
		const BYTE* pBufExt = p->GetHumanExt();

		if (Compare(pFirst, pExt, pBufFirst, pBufExt) != 0) continue;

		if (strcmp((char*)pExt, ".???") == 0 ||	// Implementation detail.
			Compare(pExt, pLast, pBufExt, pBufLast) == 0) {
			CHostFilename* pNext = (CHostFilename*)p->Next();
			pFind->id = m_nId;
			pFind->pos = pNext;
			if (pNext != &m_cRingFilename) {
				memcpy(&pFind->entry, pNext->GetEntry(), sizeof(pFind->entry));
			} else {
				memset(&pFind->entry, 0, sizeof(pFind->entry));
			}
			return p;
		}
	}

	pFind->id = m_nId;
	pFind->pos = p;
	memset(&pFind->entry, 0, sizeof(pFind->entry));
	return NULL;
}

void FASTCALL CHostPath::Clean()
{
	ASSERT(this);

	if (m_hChange != INVALID_HANDLE_VALUE) {
		::FindCloseChangeNotification(m_hChange);
		m_hChange = INVALID_HANDLE_VALUE;
	}

	CleanFilename();

	FreeHuman();
	FreeWin32();
}

void FASTCALL CHostPath::CleanFilename()
{
	CHostFilename* p;
	while ((p = (CHostFilename*)m_cRingFilename.Next()) != &m_cRingFilename) {
		delete p;
	}
}

BOOL FASTCALL CHostPath::isRefresh()
{
	ASSERT(this);

	if (m_hChange == INVALID_HANDLE_VALUE) return TRUE;		// File-system detail.

	DWORD nResult = WaitForSingleObject(m_hChange, 0);
	if (nResult == WAIT_TIMEOUT) return FALSE;

	return TRUE;
}

BOOL FASTCALL CHostPath::Refresh()
{
	ASSERT(this);
	ASSERT(m_pszWin32);
	ASSERT(_tcslen(m_pszWin32) + 22 < _MAX_PATH);
#if 0
	OutputDebugString("Cache Refresh ");
	OutputDebugString(m_pszWin32);
	OutputDebugString("\r\n");
#endif

	TCHAR szPath[_MAX_PATH];
	_tcscpy(szPath, m_pszWin32);
	_tcscat(szPath, _T("\\"));

	if (m_hChange == INVALID_HANDLE_VALUE) {
		DWORD nFlag =
			FILE_NOTIFY_CHANGE_FILE_NAME |
			FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_ATTRIBUTES |
			FILE_NOTIFY_CHANGE_SIZE |
			FILE_NOTIFY_CHANGE_LAST_WRITE;
		m_hChange = ::FindFirstChangeNotification(szPath, FALSE, nFlag);
	} else {
		for (int i = 0; i < 3; i++) {	// File-system detail.
			DWORD nResult = WaitForSingleObject(m_hChange, 0);
			if (nResult == WAIT_TIMEOUT) break;

			BOOL bResult = ::FindNextChangeNotification(m_hChange);
			if (bResult == FALSE) {
				::FindCloseChangeNotification(m_hChange);
				m_hChange = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}

	CRing cRingBackup;
	m_cRingFilename.InsertRing(&cRingBackup);

	_tcscat(szPath, _T("*.*"));

	BOOL bUpdate = FALSE;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	for (int i = 0; i < XM6_HOST_FILENAME_CACHE_MAX; i++) {
		WIN32_FIND_DATA w32Find;
		if (hFind == INVALID_HANDLE_VALUE) {
			hFind = ::FindFirstFile(szPath, &w32Find);
			if (hFind == INVALID_HANDLE_VALUE) break;
		} else {
			BOOL bResult = ::FindNextFile(hFind, &w32Find);
			if (bResult == FALSE) break;
		}

		CHostFilename* pFilename = new CHostFilename;
		ASSERT(pFilename);
		pFilename->SetWin32((TCHAR*)w32Find.cFileName);

		CHostFilename* pCache = (CHostFilename*)cRingBackup.Next();
		for (;;) {
			if (pCache == &cRingBackup) {
				pCache = NULL;			// File-system detail.
				pFilename->SetHuman();
				break;
			}
			if (_tcscmp(pFilename->GetWin32(), pCache->GetWin32()) == 0) {
				pFilename->CopyHuman(pCache->GetHuman());	// File-system detail.
				break;
			}
			pCache = (CHostFilename*)pCache->Next();
		}

		if (pFilename->isReduce()) {	// File-system detail.
			for (int n = 0; n < 36 * 36 * 36 * 36 * 36; n++) {	// File-system detail.
				if (pFilename->isCorrect()) {
					CHostFilename* pCheck = FindFilename(pFilename->GetHuman());
					if (pCheck == NULL) {
						TCHAR szBuf[_MAX_PATH];
						_tcscpy(szBuf, m_pszWin32);
						_tcscat(szBuf, _T("\\"));
						_tcscat(szBuf, (TCHAR*)pFilename->GetHuman());	// File-system detail.
						WIN32_FIND_DATA w32Check;
						HANDLE hCheck = ::FindFirstFile(szBuf, &w32Check);
						if (hCheck == INVALID_HANDLE_VALUE) break;	// File-system detail.
						FindClose(hCheck);
					}
				}
				pFilename->SetHuman(n);
			}
		}

		pFilename->SetEntry(&w32Find);

		if (pCache) {
			if (pCache->isSameEntry(pFilename->GetEntry())) {
				delete pFilename;		// File-system detail.
				pFilename = pCache;		// File-system detail.
			} else {
				bUpdate = TRUE;			// File-system detail.
				delete pCache;			// File-system detail.
			}
		} else {
			bUpdate = TRUE;				// File-system detail.
		}

		pFilename->InsertTail(&m_cRingFilename);
	}

	if (hFind != INVALID_HANDLE_VALUE) FindClose(hFind);

	CHostFilename* p;
	while ((p = (CHostFilename*)cRingBackup.Next()) != &cRingBackup) {
		bUpdate = TRUE;			// File-system detail.
		delete p;
	}

	if (bUpdate) m_nId = g_nId++;

	if (m_hChange == INVALID_HANDLE_VALUE) return FALSE;

	return TRUE;
}


CHostEntry::CHostEntry()
{
#ifdef XM6_HOST_STRICT_TIMEOUT
	m_hEvent = NULL;
	m_hThread = NULL;
#else
	m_nTimeout = 0;
#endif
	m_nRingPath = 0;

	InitializeCriticalSection(&m_csAccess);
}

CHostEntry::~CHostEntry()
{
	Clean();

	DeleteCriticalSection(&m_csAccess);
}

void CHostEntry::Init(CWinFileDrv** ppBase)
{
	ASSERT(this);
	ASSERT(ppBase);

	m_ppBase = ppBase;

#ifdef XM6_HOST_STRICT_TIMEOUT
	ASSERT(m_hEvent == NULL);
	ASSERT(m_hThread == NULL);

	m_hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);		// File-system detail.
	ASSERT(m_hEvent);

	DWORD nThread;
	m_hThread = ::CreateThread(NULL, 0, Run, this, 0, &nThread);
	ASSERT(m_hThread);
#endif
}

void CHostEntry::Clean()
{
	ASSERT(this);

#ifdef XM6_HOST_STRICT_TIMEOUT
	if (m_hThread) {
		ASSERT(m_hEvent);

		::SetEvent(m_hEvent);

		DWORD nResult;
		nResult = ::WaitForSingleObject(m_hThread, 30 * 1000);	// File-system detail.
		if (nResult != WAIT_OBJECT_0) {
			ASSERT(FALSE);	// File-system detail.
			::TerminateThread(m_hThread, (DWORD)-1);
			nResult = ::WaitForSingleObject(m_hThread, 100);
		}

		::CloseHandle(m_hThread);
		m_hThread = NULL;
	}

	if (m_hEvent) {
		::CloseHandle(m_hEvent);
		m_hEvent = NULL;
	}
#endif

	LockCache();

	CleanCache();

	UnlockCache();
}

#ifdef XM6_HOST_STRICT_TIMEOUT
DWORD WINAPI CHostEntry::Run(VOID* pThis)
{
	ASSERT(pThis);

	((CHostEntry*)pThis)->Runner();

	::ExitThread(0);
	return 0;
}

void FASTCALL CHostEntry::Runner()
{
	ASSERT(this);
	ASSERT(m_hEvent);

	for (;;) {
		DWORD nResult = ::WaitForSingleObject(m_hEvent, XM6_HOST_REMOVABLE_CHECK_TIME);
		if (nResult == WAIT_OBJECT_0) break;

		CheckTimeout();
	}
}
#endif

void FASTCALL CHostEntry::CleanCache()
{
	ASSERT(this);

	CHostPath* p;
	while ((p = (CHostPath*)m_cRingPath.Next()) != &m_cRingPath) {
		delete p;
		m_nRingPath--;
	}
	ASSERT(m_nRingPath == 0);

	CHostPath::InitId();
}

void FASTCALL CHostEntry::EraseCache(DWORD nUnit)
{
	ASSERT(this);

	for (CHostPath* p = (CHostPath*)m_cRingPath.Next(); p != &m_cRingPath;) {
		if (p->isSameUnit(nUnit)) {
			delete p;
			m_nRingPath--;

			p = (CHostPath*)m_cRingPath.Next();
			continue;
		}
		p = (CHostPath*)p->Next();
	}
}

void FASTCALL CHostEntry::DeleteCache(CHostPath* pPath)
{
	ASSERT(this);
	ASSERT(pPath);

	for (CHostPath* p = (CHostPath*)m_cRingPath.Next();
		 p != &m_cRingPath; p = (CHostPath*)p->Next()) {
		if (p == pPath) {
			delete p;
			m_nRingPath--;
			break;
		}
	}
}

CHostPath* FASTCALL CHostEntry::FindCache(DWORD nUnit, const BYTE* szHuman)
{
	ASSERT(this);
	ASSERT(szHuman);

	for (CHostPath* p = (CHostPath*)m_cRingPath.Next();
		 p != &m_cRingPath; p = (CHostPath*)p->Next()) {
		if (p->isSameHuman(nUnit, szHuman)) {
			return p;
		}
	}

	return NULL;
}

CHostPath* FASTCALL CHostEntry::CopyCache(DWORD nUnit, const BYTE* szHuman, TCHAR* szWin32Buffer)
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(strlen((char*)szHuman) < HUMAN68K_MAX_PATH);

	const BYTE* pSeparate = SeparatePath(szHuman);
	if (pSeparate == NULL) return NULL;	// File-system detail.

	BYTE szHumanPath[HUMAN68K_MAX_PATH];
	int nPath = pSeparate - szHuman;
	strncpy((char*)szHumanPath, (char*)szHuman, nPath);
	szHumanPath[nPath] = '\0';

	BYTE szHumanFilename[_MAX_PATH];
	strcpy((char*)szHumanFilename, (char*)pSeparate + 1);

	CHostPath* pPath = FindCache(nUnit, szHumanPath);
	if (pPath == NULL) return NULL;	// File-system detail.

	pPath->Insert(&m_cRingPath);

	if (pPath->isRefresh()) return NULL;	// File-system detail.

	if (szWin32Buffer) {
		_tcscpy(szWin32Buffer, pPath->GetWin32());
		_tcscat(szWin32Buffer, _T("\\"));
	}

	if (szHumanFilename[0] == '\0') {
		return pPath;	// File-system detail.
	}

	CHostFilename* pFilename = pPath->FindFilename(szHumanFilename);
	if (pFilename == NULL) return NULL;	// File-system detail.

	if (szWin32Buffer) {
		_tcscat(szWin32Buffer, pFilename->GetWin32());
	}

	return pPath;
}

CHostPath* FASTCALL CHostEntry::MakeCache(WindrvContext* ps, DWORD nUnit, const BYTE* szHuman, TCHAR* szWin32Buffer)
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(strlen((char*)szHuman) < HUMAN68K_MAX_PATH);

	const TCHAR* szWin32Base = GetBase(nUnit);
	ASSERT(szWin32Base);
	ASSERT(_tcslen(szWin32Base) < _MAX_PATH);

	BYTE szHumanPath[HUMAN68K_MAX_PATH];	// File-system detail.
	szHumanPath[0] = '\0';
	int nHumanPath = 0;

	TCHAR szWin32Path[_MAX_PATH];
	_tcscpy(szWin32Path, szWin32Base);
	int nWin32Path = _tcslen(szWin32Path);

	CHostFilename* pFilename = NULL;	// File-system detail.
	CHostPath* pPath;
	const BYTE* p = szHuman;
	for (;;) {
		BYTE szHumanFilename[24];			// File-system detail.
		p = SeparateCopyFilename(p, szHumanFilename);
		if (p == NULL) return NULL;		// File-system detail.

		pPath = FindCache(nUnit, szHumanPath);
		if (pPath == NULL) {
			if (m_nRingPath >= XM6_HOST_DIRENTRY_CACHE_MAX) {
				pPath = (CHostPath*)m_cRingPath.Prev();
				pPath->Clean();		// File-system detail.
			} else {
				pPath = new CHostPath;
				ASSERT(pPath);
				m_nRingPath++;
			}
			pPath->SetHuman(nUnit, szHumanPath);
			pPath->SetWin32(szWin32Path);
		}

		if (pPath->isRefresh()) {
			ps->Ready();
			BOOL bResult = CheckMediaAccess(nUnit, FALSE);
			if (bResult) bResult = pPath->Refresh();	// File-system detail.
			if (bResult == FALSE) {
				delete pPath;
				m_nRingPath--;
				EraseCache(nUnit);
				return NULL;		// File-system detail.
			}
		}

		pPath->Insert(&m_cRingPath);

		if (pFilename) pFilename->SetChild(this, pPath);

		if (szHumanFilename[0] == '\0') {
			if (nHumanPath + 1 + 1 > HUMAN68K_MAX_PATH) return NULL;	// File-system detail.
			szHumanPath[nHumanPath++] = '/';
			szHumanPath[nHumanPath] = '\0';
			if (nWin32Path + 1 + 1 > _MAX_PATH) return NULL;	// File-system detail.
			szWin32Path[nWin32Path++] = '\\';
			szWin32Path[nWin32Path] = '\0';
			break;	// File-system detail.
		}

		if (*p != '\0') {
			pFilename = pPath->FindFilename(szHumanFilename, Human68k::AT_DIRECTORY);
		} else {
			pFilename = pPath->FindFilename(szHumanFilename);
		}
		if (pFilename == NULL) return NULL;	// File-system detail.

		int n = strlen((char*)szHumanFilename);
		if (nHumanPath + n + 1 > HUMAN68K_MAX_PATH) return NULL;	// File-system detail.
		szHumanPath[nHumanPath++] = '/';
		strcpy((char*)szHumanPath + nHumanPath, (char*)szHumanFilename);
		nHumanPath += n;

		n = _tcslen(pFilename->GetWin32());
		if (nWin32Path + n + 1 > _MAX_PATH) return NULL;	// File-system detail.
		szWin32Path[nWin32Path++] = '\\';
		_tcscpy(szWin32Path + nWin32Path, pFilename->GetWin32());
		nWin32Path += n;

		if (*p == '\0') break;
	}

	if (szWin32Buffer) {
		_tcscpy(szWin32Buffer, szWin32Path);
	}

	return pPath;
}

TCHAR* FASTCALL CHostEntry::GetBase(DWORD nUnit) const
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->GetBase();
}

BOOL FASTCALL CHostEntry::isWriteProtect(WindrvContext* ps) const
{
	ASSERT(this);
	ASSERT(ps);
	ASSERT(m_ppBase);

	DWORD nUnit = ps->GetUnit();
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->isWriteProtect();
}

BOOL FASTCALL CHostEntry::isMediaOffline(WindrvContext* ps, BOOL bMedia)
{
	ASSERT(this);
	ASSERT(ps);
	ASSERT(m_ppBase);

	DWORD nUnit = ps->GetUnit();
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	if (m_ppBase[nUnit]->isSlow()) ps->Ready();

	if (bMedia) CheckMediaAccess(nUnit, TRUE);

	m_ppBase[nUnit]->SetTimeout();

	if (m_ppBase[nUnit]->isManual()) return FALSE;

	return m_ppBase[nUnit]->isEnable() == FALSE;
}

BYTE FASTCALL CHostEntry::GetMediaByte(DWORD nUnit) const
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	if (m_ppBase[nUnit]->isRemovable()) {
		if (m_ppBase[nUnit]->isManual()) {
			return 0xF1;
		}
		return 0xF2;
	}
	return 0xF3;
}

DWORD FASTCALL CHostEntry::GetStatus(DWORD nUnit) const
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->GetStatus();
}

void FASTCALL CHostEntry::CheckTimeout()
{
	ASSERT(this);
	ASSERT(m_ppBase);

#ifndef XM6_HOST_STRICT_TIMEOUT
	DWORD nCount = ::GetTickCount();
	DWORD nDelta = nCount - m_nTimeout;
	if (nDelta < XM6_HOST_REMOVABLE_CHECK_TIME) return;
	m_nTimeout = nCount;
#endif

	for (DWORD n = 0; n < CWinFileSys::DrvMax; n++) {
		if (m_ppBase[n]) {
			BOOL bResult = m_ppBase[n]->CheckTimeout();
			if (bResult) {
				LockCache();
				EraseCache(n);
				UnlockCache();
			}
		}
	}
}

#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
void FASTCALL CHostEntry::SetMediaUpdate(WindrvContext* ps, BOOL bDisable)
{
	ASSERT(this);
	ASSERT(ps);
	ASSERT(m_ppBase);

	DWORD nUnit = ps->GetUnit();
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	m_ppBase[nUnit]->SetMediaUpdate(bDisable);
}
#endif

BOOL FASTCALL CHostEntry::CheckMediaUpdate(WindrvContext* ps)
{
	ASSERT(this);
	ASSERT(ps);
	ASSERT(m_ppBase);

	DWORD nUnit = ps->GetUnit();
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	BOOL bResult = m_ppBase[nUnit]->CheckMediaUpdate();
	if (bResult) {
		bResult = m_ppBase[nUnit]->CheckMediaAccess(FALSE);
		if (bResult) {
			if (m_ppBase[nUnit]->isSlow()) ps->Ready();

			bResult = m_ppBase[nUnit]->CheckMedia();
			if (bResult == FALSE) {
				LockCache();
				EraseCache(nUnit);
				UnlockCache();
			}
		}
	}

	return m_ppBase[nUnit]->isEnable();
}

BOOL FASTCALL CHostEntry::CheckMediaAccess(DWORD nUnit, BOOL bErase)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	BOOL bResult = m_ppBase[nUnit]->CheckMediaAccess(TRUE);
	if (bResult) {
		bResult = m_ppBase[nUnit]->CheckMedia();
		if (bResult == FALSE) {
			if (bErase) {
				LockCache();
				EraseCache(nUnit);
				UnlockCache();
			}
		}
	}

	return m_ppBase[nUnit]->isEnable();
}

BOOL FASTCALL CHostEntry::Eject(DWORD nUnit)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	BOOL bResult = m_ppBase[nUnit]->Eject();
	if (bResult) {
		m_ppBase[nUnit]->CheckMedia();
	}

	return bResult;
}

void FASTCALL CHostEntry::GetVolume(DWORD nUnit, TCHAR* szLabel)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	m_ppBase[nUnit]->GetVolume(szLabel);
}

BOOL FASTCALL CHostEntry::GetVolumeCache(DWORD nUnit, TCHAR* szLabel)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->GetVolumeCache(szLabel);
}

DWORD FASTCALL CHostEntry::GetCapacity(DWORD nUnit, Human68k::capacity_t* pCapacity)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->GetCapacity(pCapacity);
}

BOOL FASTCALL CHostEntry::GetCapacityCache(DWORD nUnit, Human68k::capacity_t* pCapacity)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->GetCapacityCache(pCapacity);
}

void FASTCALL CHostEntry::ShellNotify(DWORD nEvent, const TCHAR* szPath)
{
	ASSERT(this);
	ASSERT(m_ppBase);

	for (int n = 0; n < CWinFileSys::DrvMax; n++) {
		if (m_ppBase[n]) {
			if (m_ppBase[n]->isSameDrive(szPath)) {
				if (nEvent & (SHCNE_MEDIAREMOVED | SHCNE_DRIVEREMOVED)) {
					m_ppBase[n]->SetEnable(FALSE);
				}

				if (nEvent & (SHCNE_MEDIAINSERTED | SHCNE_DRIVEADD)) {
					m_ppBase[n]->SetEnable(TRUE);
				}

				LockCache();
				EraseCache(n);
				UnlockCache();
			}
		}
	}
}

const BYTE* FASTCALL CHostEntry::SeparatePath(const BYTE* szHuman)
{
	ASSERT(szHuman);

	DWORD nMax = 22;
	const BYTE* p = szHuman;

	BYTE c = *p;
	if (c != '/' && c != '\\') return NULL;	// File-system detail.

	const BYTE* pCut = NULL;
	for (;;) {
		c = *p;
		if (c == '\0') break;
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// File-system detail.
			p++;
			if (*p == '\0') break;
		}
		if (c == '/' || c == '\\') {
			if (pCut == p - 1) return NULL;	// File-system detail.
			pCut = p;
		}
		p++;
	}

	if (pCut == NULL) return NULL;			// File-system detail.
	if (strlen((char*)pCut + 1) > nMax) return NULL;	// File-system detail.

	return pCut;
}

const BYTE* FASTCALL CHostEntry::SeparateCopyFilename(const BYTE* szHuman, BYTE* szBuffer)
{
	ASSERT(szHuman);
	ASSERT(szBuffer);

	DWORD nMax = 22;
	const BYTE* p = szHuman;

	BYTE c = *p;
	if (c != '/' && c != '\\') return NULL;	// File-system detail.
	p++;

	BYTE* pWrite = szBuffer;
	DWORD i = 0;
	for (;;) {
		c = *p;								// File-system detail.
		if (c == '\0') break;				// File-system detail.
		if (c == '/' || c == '\\') {
			if (pWrite == szBuffer) return NULL;	// File-system detail.
			break;	// File-system detail.
		}
		p++;

		if (i >= nMax) return NULL;	// File-system detail.
		i++;								// File-system detail.
		*pWrite++ = c;						// File-system detail.

		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// File-system detail.
			c = *p;							// File-system detail.
			if (c < 0x40) return NULL;		// File-system detail.
			p++;

			if (i >= nMax) return NULL;	// File-system detail.
			i++;							// File-system detail.
			*pWrite++ = c;					// File-system detail.
		}
	}
	*pWrite = '\0';

	return p;
}


void FASTCALL CHostFiles::Clear()
{
	ASSERT(this);

	m_nKey = 0;
#if 0
	m_nHumanUnit = 0;
	m_szHumanPath[0] = '\0';
	m_szHumanFilename[0] = '\0';
	m_nHumanWildcard = 0;
	m_nHumanAttribute = 0;
	m_findNext.Clear();
#endif
	memset(&m_dirEntry, 0, sizeof(m_dirEntry));
	m_szHumanResult[0] = '\0';
	m_szWin32Result[0] = _T('\0');
}

void FASTCALL CHostFiles::SetPath(DWORD nUnit, const Human68k::namests_t* pNamests)
{
	ASSERT(this);
	ASSERT(pNamests);

	m_nHumanUnit = nUnit;
	pNamests->GetCopyPath(m_szHumanPath);
	pNamests->GetCopyFilename(m_szHumanFilename);
	m_nHumanWildcard = 0;
	m_nHumanAttribute = Human68k::AT_ARCHIVE;
	m_findNext.Clear();
}

BOOL FASTCALL CHostFiles::Find(WindrvContext* ps, CHostEntry* pEntry, BOOL bRemove)
{
	ASSERT(this);
	ASSERT(pEntry);

	pEntry->LockCache();

	CHostPath* pPath;
#ifdef XM6_HOST_STRICT_CACHE_CHECK
	pPath = pEntry->MakeCache(ps, m_nHumanUnit, m_szHumanPath, m_szWin32Result);
#else
	pPath = pEntry->CopyCache(m_nHumanUnit, m_szHumanPath, m_szWin32Result);
	if (pPath == NULL) {
		pPath = pEntry->MakeCache(ps, m_nHumanUnit, m_szHumanPath, m_szWin32Result);
	}
#endif
	if (pPath == NULL) {
		pEntry->UnlockCache();
		return FALSE;	// File-system detail.
	}

	if (m_nHumanWildcard == 0xFF) {
		_tcscpy(m_szWin32Result, pPath->GetWin32());
		_tcscat(m_szWin32Result, _T("\\"));
		pEntry->UnlockCache();
		return TRUE;	// File-system detail.
	}

	CHostFilename* pFilename;
	if (m_nHumanWildcard == 0) {
		pFilename = pPath->FindFilename(m_szHumanFilename, m_nHumanAttribute);
	} else {
		pFilename = pPath->FindFilenameWildcard(m_szHumanFilename, &m_findNext, m_nHumanAttribute);
	}
	if (pFilename == NULL) {
		pEntry->UnlockCache();
		return FALSE;	// File-system detail.
	}

	if (bRemove) pFilename->DeleteCache();

	memcpy(&m_dirEntry, pFilename->GetEntry(), sizeof(m_dirEntry));

	strcpy((char*)m_szHumanResult, (char*)pFilename->GetHuman());

	_tcscpy(m_szWin32Result, pPath->GetWin32());
	_tcscat(m_szWin32Result, _T("\\"));
	_tcscat(m_szWin32Result, pFilename->GetWin32());

	pEntry->UnlockCache();

	return TRUE;
}

void FASTCALL CHostFiles::AddFilename()
{
	ASSERT(this);
	ASSERT(_tcslen(m_szWin32Result) + strlen((char*)m_szHumanFilename) < _MAX_PATH);

	_tcscat(m_szWin32Result, (TCHAR*)m_szHumanFilename);
}


CHostFilesManager::CHostFilesManager()
{
}

CHostFilesManager::~CHostFilesManager()
{
#ifdef _DEBUG
	ASSERT(m_cRingFiles.Next() == &m_cRingFiles);
	ASSERT(m_cRingFiles.Prev() == &m_cRingFiles);

	Clean();
#endif
}

void FASTCALL CHostFilesManager::Init()
{
	ASSERT(this);

	ASSERT(m_cRingFiles.Next() == &m_cRingFiles);
	ASSERT(m_cRingFiles.Prev() == &m_cRingFiles);

	for (int i = 0; i < XM6_HOST_FILES_MAX; i++) {
		CHostFiles* p = new CHostFiles;
		ASSERT(p);
		p->InsertTail(&m_cRingFiles);
	}
}

void FASTCALL CHostFilesManager::Clean()
{
	ASSERT(this);

	CHostFiles* p;
	while ((p = (CHostFiles*)m_cRingFiles.Next()) != &m_cRingFiles) {
		delete p;
	}
}

CHostFiles* FASTCALL CHostFilesManager::Alloc(DWORD nKey)
{
	ASSERT(this);
	ASSERT(nKey != 0);

	CHostFiles* p = (CHostFiles*)m_cRingFiles.Prev();

	p->Insert(&m_cRingFiles);

	p->SetKey(nKey);

	return p;
}

CHostFiles* FASTCALL CHostFilesManager::Search(DWORD nKey)
{
	ASSERT(this);
	ASSERT(nKey != 0);

	for (CHostFiles* p = (CHostFiles*)m_cRingFiles.Next();
		 p != &m_cRingFiles; p = (CHostFiles*)p->Next()) {
		if (p->isSameKey(nKey)) {
			p->Insert(&m_cRingFiles);
			return p;
		}
	}

	return NULL;
}

void FASTCALL CHostFilesManager::Free(CHostFiles* p)
{
	ASSERT(this);
	ASSERT(p);

	p->Free();

	p->InsertTail(&m_cRingFiles);
}


void FASTCALL CHostFcb::Clear()
{
	ASSERT(this);

	m_nKey = 0;
	m_nMode = 0;
	m_hFile = INVALID_HANDLE_VALUE;
	m_szFilename[0] = _T('\0');
}

BOOL FASTCALL CHostFcb::SetOpenMode(DWORD nHumanMode)
{
	ASSERT(this);

	switch (nHumanMode & 0x0F) {
	case Human68k::OP_READ:
		m_nMode = GENERIC_READ;
		break;
	case Human68k::OP_WRITE:
		m_nMode = GENERIC_WRITE;
		break;
	case Human68k::OP_READWRITE:
		m_nMode = GENERIC_READ | GENERIC_WRITE;
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

void FASTCALL CHostFcb::SetFilename(const TCHAR* szFilename)
{
	ASSERT(this);
	ASSERT(szFilename);
	ASSERT(_tcslen(szFilename) < _MAX_PATH);

	_tcscpy(m_szFilename, szFilename);
}

// Create the file.
HANDLE FASTCALL CHostFcb::Create(DWORD nHumanAttribute, BOOL bForce)
{
	ASSERT(this);
	ASSERT(_tcslen(m_szFilename) > 0);
	ASSERT(m_hFile == INVALID_HANDLE_VALUE);

	DWORD nAttribute = 0;
	if ((nHumanAttribute & Human68k::AT_DIRECTORY) != 0) return INVALID_HANDLE_VALUE;
	if ((nHumanAttribute & Human68k::AT_SYSTEM) != 0) nAttribute |= FILE_ATTRIBUTE_SYSTEM;
	if ((nHumanAttribute & Human68k::AT_HIDDEN) != 0) nAttribute |= FILE_ATTRIBUTE_HIDDEN;
	if ((nHumanAttribute & Human68k::AT_READONLY) != 0) nAttribute |= FILE_ATTRIBUTE_READONLY;
	if (nAttribute == 0) nAttribute = FILE_ATTRIBUTE_NORMAL;

	DWORD nShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	DWORD nCreation = bForce ? CREATE_ALWAYS : CREATE_NEW;

// Open the file.
	m_hFile = ::CreateFile(m_szFilename, m_nMode, nShare, NULL, nCreation, nAttribute, NULL);
	return m_hFile;
}

HANDLE FASTCALL CHostFcb::Open()
{
	ASSERT(this);
	ASSERT(_tcslen(m_szFilename) > 0);

// Open the file.
	if (m_hFile == INVALID_HANDLE_VALUE) {
		DWORD nShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		DWORD nCreation = OPEN_EXISTING;
		DWORD nAttribute = FILE_ATTRIBUTE_NORMAL;
		m_hFile = ::CreateFile(m_szFilename, m_nMode, nShare, NULL, nCreation, nAttribute, NULL);
	}
	return m_hFile;
}

// Read file data.
DWORD FASTCALL CHostFcb::ReadFile(void* pBuffer, DWORD nSize)
{
	ASSERT(this);
	ASSERT(pBuffer);
	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	DWORD nResult;
	BOOL bResult = ::ReadFile(m_hFile, pBuffer, nSize, &nResult, NULL);
	if (bResult == FALSE) nResult = (DWORD)-1;

	return nResult;
}

// Write file data.
DWORD FASTCALL CHostFcb::WriteFile(void* pBuffer, DWORD nSize)
{
	ASSERT(this);
	ASSERT(pBuffer);
	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	DWORD nResult;
	BOOL bResult = ::WriteFile(m_hFile, pBuffer, nSize, &nResult, NULL);
	if (bResult == FALSE) nResult = (DWORD)-1;

	return nResult;
}

DWORD FASTCALL CHostFcb::SetFilePointer(DWORD nOffset, DWORD nMode)
{
	ASSERT(this);
	ASSERT(nMode == FILE_BEGIN || nMode == FILE_CURRENT || nMode == FILE_END);
	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	return ::SetFilePointer(m_hFile, nOffset, NULL, nMode);
}

DWORD FASTCALL CHostFcb::SetFileTime(WORD nFatDate, WORD nFatTime)
{
	ASSERT(this);
	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	FILETIME lt;
	if (::DosDateTimeToFileTime(nFatDate, nFatTime, &lt) == 0) return (DWORD)-1;
	FILETIME ft;
	if (::LocalFileTimeToFileTime(&lt, &ft) == 0) return (DWORD)-1;

	if (::SetFileTime(m_hFile, NULL, &ft, &ft) == 0) return (DWORD)-1;
	return 0;
}

// Close the file.
BOOL FASTCALL CHostFcb::Close()
{
	ASSERT(this);

	BOOL bResult = TRUE;

// Close the file.
	if (m_hFile != INVALID_HANDLE_VALUE) {
		bResult = ::CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}

	return bResult;
}


CHostFcbManager::CHostFcbManager()
{
}

CHostFcbManager::~CHostFcbManager()
{
#ifdef _DEBUG
	ASSERT(m_cRingFcb.Next() == &m_cRingFcb);
	ASSERT(m_cRingFcb.Prev() == &m_cRingFcb);

	Clean();
#endif
}

void FASTCALL CHostFcbManager::Init()
{
	ASSERT(this);

	ASSERT(m_cRingFcb.Next() == &m_cRingFcb);
	ASSERT(m_cRingFcb.Prev() == &m_cRingFcb);

	for (int i = 0; i < XM6_HOST_FCB_MAX; i++) {
		CHostFcb* p = new CHostFcb;
		ASSERT(p);
		p->InsertTail(&m_cRingFcb);
	}
}

void CHostFcbManager::Clean()
{
	ASSERT(this);

	CHostFcb* p;
	while ((p = (CHostFcb*)m_cRingFcb.Next()) != &m_cRingFcb) {
		delete p;
	}
}

CHostFcb* FASTCALL CHostFcbManager::Alloc(DWORD nKey)
{
	ASSERT(this);
	ASSERT(nKey != 0);

	CHostFcb* p = (CHostFcb*)m_cRingFcb.Prev();

	if (p->isSameKey(0) == FALSE) return NULL;

	p->Insert(&m_cRingFcb);

	p->SetKey(nKey);

	return p;
}

CHostFcb* FASTCALL CHostFcbManager::Search(DWORD nKey)
{
	ASSERT(this);
	ASSERT(nKey != 0);

	for (CHostFcb* p = (CHostFcb*)m_cRingFcb.Next();
		 p != &m_cRingFcb; p = (CHostFcb*)p->Next()) {
		if (p->isSameKey(nKey)) {
			p->Insert(&m_cRingFcb);
			return p;
		}
	}

	return NULL;
}

void FASTCALL CHostFcbManager::Free(CHostFcb* p)
{
	ASSERT(this);
	ASSERT(p);

	p->Free();

	p->InsertTail(&m_cRingFcb);
}


// Constructor.
CWinFileSys::CWinFileSys()
{
// Initialize the drive object.
	for (int n = 0; n < DrvMax; n++) {
		m_pDrv[n] = NULL;
	}

	m_bResume = FALSE;
	m_nDrives = 0;

	for (int n = 0; n < DrvMax; n++) {
		m_nFlag[n] = 0;
		m_szBase[n][0] = _T('\0');
	}

	m_nKernel = 0;
	m_nKernelSearch = 0;

// Initialize operation flags.
	m_nOptionDefault = 0;
	m_nOption = 0;
	CHostFilename::SetOption(0);
	CHostPath::SetOption(0);
}

// Destructor.
CWinFileSys::~CWinFileSys()
{
#ifdef _DEBUG
	int nDrv;

	for (nDrv=0; nDrv<DrvMax; nDrv++) {
		ASSERT(!m_pDrv[nDrv]);

		if (m_pDrv[nDrv]) {
			delete m_pDrv[nDrv];
			m_pDrv[nDrv] = NULL;
		}
	}
#endif
}

void FASTCALL CWinFileSys::ApplyCfg(const Config* pConfig)
{
	ASSERT(this);
	ASSERT(pConfig);

	m_bResume = pConfig->host_resume;
	m_nDrives = pConfig->host_drives;

	for (int n = 0; n < DrvMax; n++) {
		m_nFlag[n] = pConfig->host_flag[n];
		ASSERT(_tcslen(pConfig->host_path[n]) < _MAX_PATH);
		_tcscpy(m_szBase[n], pConfig->host_path[n]);
	}

	m_nOptionDefault = pConfig->host_option;
}

// $40 - Initialize.
DWORD FASTCALL CWinFileSys::Init(WindrvContext* ps, DWORD nDriveMax, const BYTE* pOption)
{
	ASSERT(this);
	ASSERT(nDriveMax < 26);

	ps->Ready();

	::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

	m_cFiles.Init();

	m_cFcb.Init();

	m_cEntry.Init(m_pDrv);

	InitOption(pOption);

	DWORD nConfigDrives = m_nDrives;
	if (m_bResume == FALSE) {
		for (DWORD i = 0; i < DrvMax; i++) {
			m_nFlag[i] = 0;
			m_szBase[i][0] = _T('\0');
		}

		nConfigDrives = 0;
		DWORD nBits = ::GetLogicalDrives();
		for (DWORD n = 0; n < 26; n++) {
			if (nBits & 1) {
				_stprintf(m_szBase[nConfigDrives], _T("%c:\\"), 'A' + n);

				m_nFlag[nConfigDrives] = 0;

				nConfigDrives++;

				if (nConfigDrives >= DrvMax) break;
			}

			nBits >>= 1;
		}
	}

	DWORD nDrives = 0;
	for (DWORD n = 0; n < nConfigDrives; n++) {	// File-system detail.
		if (m_szBase[n][0] == _T('\0')) continue;

		if (nDrives >= nDriveMax) break;

		ASSERT(m_pDrv[nDrives] == NULL);
		m_pDrv[nDrives] = new CWinFileDrv;
		ASSERT(m_pDrv[nDrives]);

// Initialize.
		m_pDrv[nDrives]->Init(m_szBase[n], m_nFlag[n]);

		nDrives++;
	}

	return nDrives;
}

void FASTCALL CWinFileSys::Reset()
{
	int nDrv;

	ASSERT(this);

	m_nHostSectorCount = 0;
	memset(m_nHostSectorBuffer, 0, sizeof(m_nHostSectorBuffer));

	m_cFiles.Clean();

	m_cFcb.Clean();

	m_cEntry.Clean();

	for (nDrv=0; nDrv<DrvMax; nDrv++) {
		if (m_pDrv[nDrv]) {
			delete m_pDrv[nDrv];
			m_pDrv[nDrv] = NULL;
		}
	}

	m_nKernel = 0;
	m_nKernelSearch = 0;
}

// $41 - Check the directory.
int FASTCALL CWinFileSys::CheckDir(WindrvContext* ps, const Human68k::namests_t* pNamests)
{
	ASSERT(this);
	ASSERT(pNamests);


#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	m_cEntry.SetMediaUpdate(ps, FALSE);
#endif

	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);
	if (f.isRootPath()) return 0;
	f.SetPathOnly();
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_DIRNOTFND;

	return 0;
}

// $42 - Create the directory.
int FASTCALL CWinFileSys::MakeDir(WindrvContext* ps, const Human68k::namests_t* pNamests)
{
	ASSERT(this);
	ASSERT(pNamests);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);
	f.SetPathOnly();
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_INVALIDPATH;
	f.AddFilename();

// Create the directory.
	BOOL bResult = ::CreateDirectory(f.GetPath(), NULL);
	if (bResult == FALSE) return FS_INVALIDPATH;

	return 0;
}

// $43 - Delete the directory.
int FASTCALL CWinFileSys::RemoveDir(WindrvContext* ps, const Human68k::namests_t* pNamests)
{
	ASSERT(this);
	ASSERT(pNamests);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);

// Check the directory.
	f.SetAttribute(Human68k::AT_DIRECTORY);
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_DIRNOTFND;

// Delete the directory.
	if ((m_nOption & WINDRV_OPT_REMOVE) == 0) {
		BOOL bResult = ::RemoveDirectory(f.GetPath());
		if (bResult == FALSE) return FS_CANTDELETE;
	} else {
		TCHAR szBuf[_MAX_PATH];
		_tcscpy(szBuf, f.GetPath());
		_tcscat(szBuf, "\\*.*");
		WIN32_FIND_DATA w32Find;
		HANDLE hFind = ::FindFirstFile(szBuf, &w32Find);
		if (hFind != INVALID_HANDLE_VALUE) {
			for (;;) {
				if (strcmp(w32Find.cFileName, ".") != 0 &&
					strcmp(w32Find.cFileName, "..") != 0) {
					FindClose(hFind);
					return FS_CANTDELETE;
				}
				BOOL bResult = ::FindNextFile(hFind, &w32Find);
				if (bResult == FALSE) break;
			}
			FindClose(hFind);
		}

		char szBuffer[_MAX_PATH + 1];
		strcpy(szBuffer, f.GetPath());
		szBuffer[strlen(szBuffer) + 1] = '\0';

		SHFILEOPSTRUCT sop;
		sop.hwnd = NULL;
		sop.wFunc = FO_DELETE;
		sop.pFrom = szBuffer;
		sop.pTo = NULL;
		sop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
		sop.fAnyOperationsAborted = TRUE;
		sop.hNameMappings = NULL;
		sop.lpszProgressTitle = NULL;

		int nResult = ::SHFileOperation(&sop);
		if (nResult != 0) return FS_CANTDELETE;
	}

	f.Find(ps, &m_cEntry, TRUE);

	return 0;
}

int FASTCALL CWinFileSys::Rename(WindrvContext* ps, const Human68k::namests_t* pNamests, const Human68k::namests_t* pNamestsNew)
{
	ASSERT(this);
	ASSERT(pNamests);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_FILENOTFND;

	CHostFiles fNew;
	fNew.SetPath(nUnit, pNamestsNew);
	fNew.SetPathOnly();
	if (fNew.Find(ps, &m_cEntry) == FALSE) return FS_INVALIDPATH;
	fNew.AddFilename();

	BOOL bResult = ::MoveFile(f.GetPath(), fNew.GetPath());
	if (bResult == FALSE) return FS_FILENOTFND;

	return 0;
}

// $45 - Delete the file.
int FASTCALL CWinFileSys::Delete(WindrvContext* ps, const Human68k::namests_t* pNamests)
{
	ASSERT(this);
	ASSERT(pNamests);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);

	if (f.Find(ps, &m_cEntry) == FALSE) return FS_FILENOTFND;

// Delete the file.
	if ((m_nOption & WINDRV_OPT_REMOVE) == 0) {
		BOOL bResult = ::DeleteFile(f.GetPath());
		if (bResult == FALSE) return FS_CANTDELETE;
	} else {
		char szBuffer[_MAX_PATH + 1];
		strcpy(szBuffer, f.GetPath());
		szBuffer[strlen(szBuffer) + 1] = '\0';

		SHFILEOPSTRUCT sop;
		sop.hwnd = NULL;
		sop.wFunc = FO_DELETE;
		sop.pFrom = szBuffer;
		sop.pTo = NULL;
		sop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
		sop.fAnyOperationsAborted = TRUE;
		sop.hNameMappings = NULL;
		sop.lpszProgressTitle = NULL;

		int nResult = ::SHFileOperation(&sop);
		if (nResult != 0) return FS_CANTDELETE;
	}

	return 0;
}

int FASTCALL CWinFileSys::Attribute(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nHumanAttribute)
{
	ASSERT(this);
	ASSERT(pNamests);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_FILENOTFND;

	if (nHumanAttribute == 0xFF) {
		return f.GetAttribute();
	}

	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

#if 0
	if (f.GetAttribute() & Human68k::AT_DIRECTORY) {
		if ((nHumanAttribute & Human68k::AT_DIRECTORY) == 0) {
			OutputDebugString("Warning: ディレクトリの属性をファイルに変更できません\r\n");
		}
	}

	if ((f.GetAttribute() & Human68k::AT_DIRECTORY) == 0) {
		if (nHumanAttribute & Human68k::AT_DIRECTORY) {
			OutputDebugString("Warning: ファイルの属性をディレクトリに変更できません\r\n");
		}
	}
#endif

	DWORD nAttribute = 0;
	if ((nHumanAttribute & Human68k::AT_SYSTEM) != 0) nAttribute |= FILE_ATTRIBUTE_SYSTEM;
	if ((nHumanAttribute & Human68k::AT_HIDDEN) != 0) nAttribute |= FILE_ATTRIBUTE_HIDDEN;
	if ((nHumanAttribute & Human68k::AT_READONLY) != 0) nAttribute |= FILE_ATTRIBUTE_READONLY;
	if (nAttribute == 0) nAttribute = FILE_ATTRIBUTE_NORMAL;

	BOOL bResult = ::SetFileAttributes(f.GetPath(), nAttribute);
	if (bResult == FALSE) return FS_FILENOTFND;

	if (f.Find(ps, &m_cEntry) == FALSE) return FS_FILENOTFND;
	return f.GetAttribute();
}

// $47 - Search files (first match).
int FASTCALL CWinFileSys::Files(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::files_t* pFiles)
{
	ASSERT(this);
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFiles);

	CHostFiles* pHostFiles = m_cFiles.Search(nKey);
	if (pHostFiles != NULL) {
		m_cFiles.Free(pHostFiles);
	}

	if (pFiles->fatr == Human68k::AT_VOLUME) {
		if (FilesVolume(ps, pFiles) == FALSE) {
			return FS_FILENOTFND;
		}
		return 0;
	}

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	pHostFiles = m_cFiles.Alloc(nKey);
	if (pHostFiles == NULL) {
		return FS_OUTOFMEM;
	}

// Check the directory.
	DWORD nUnit = ps->GetUnit();
	pHostFiles->SetPath(nUnit, pNamests);
	if (pHostFiles->isRootPath() == FALSE) {
		pHostFiles->SetPathOnly();
		if (pHostFiles->Find(ps, &m_cEntry) == FALSE) {
			m_cFiles.Free(pHostFiles);
			return FS_DIRNOTFND;
		}
	}

	pHostFiles->SetPathWildcard();
	pHostFiles->SetAttribute(pFiles->fatr);

// File search.
	if (pHostFiles->Find(ps, &m_cEntry) == FALSE) {
		m_cFiles.Free(pHostFiles);
		return FS_FILENOTFND;
	}

	pFiles->attr = (BYTE)pHostFiles->GetAttribute();
	pFiles->date = pHostFiles->GetDate();
	pFiles->time = pHostFiles->GetTime();
	pFiles->size = pHostFiles->GetSize();
	strcpy((char*)pFiles->full, (char*)pHostFiles->GetHumanResult());

	pFiles->sector = nKey;
	pFiles->offset = 0;

	if (pNamests->wildcard == 0) {
	}

	return 0;
}

// $48 - Search files (next match).
int FASTCALL CWinFileSys::NFiles(WindrvContext* ps, DWORD nKey, Human68k::files_t* pFiles)
{
	ASSERT(this);
	ASSERT(nKey);
	ASSERT(pFiles);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	CHostFiles* pHostFiles = m_cFiles.Search(nKey);
	if (pHostFiles == NULL) {
		return FS_FILENOTFND;
	}

// File search.
	if (pHostFiles->Find(ps, &m_cEntry) == FALSE) {
		m_cFiles.Free(pHostFiles);
		return FS_FILENOTFND;
	}

	pFiles->attr = (BYTE)pHostFiles->GetAttribute();
	pFiles->date = pHostFiles->GetDate();
	pFiles->time = pHostFiles->GetTime();
	pFiles->size = pHostFiles->GetSize();
	strcpy((char*)pFiles->full, (char*)pHostFiles->GetHumanResult());

	return 0;
}

int FASTCALL CWinFileSys::Create(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nHumanAttribute, BOOL bForce)
{
	ASSERT(this);
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFcb);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb != NULL) return FS_FILEEXIST;

	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);

	f.SetPathOnly();
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_INVALIDPATH;
	f.AddFilename();

	pHostFcb = m_cFcb.Alloc(nKey);
	if (pHostFcb == NULL) return FS_OUTOFMEM;
	pHostFcb->SetFilename(f.GetPath());

	pFcb->mode = (pFcb->mode & ~0x0F) | Human68k::OP_READWRITE;
	if (pHostFcb->SetOpenMode(pFcb->mode) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_ILLEGALMOD;
	}

// Create the file.
	HANDLE hFile = pHostFcb->Create(nHumanAttribute, bForce);
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_FILEEXIST;
	}

#ifdef XM6_HOST_STRICT_CLOSE
	pHostFcb->Close();
#endif

	return 0;
}

// $4A - Open the file.
int FASTCALL CWinFileSys::Open(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::fcb_t* pFcb)
{
	ASSERT(this);
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFcb);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	switch (pFcb->mode) {
	case Human68k::OP_WRITE:
	case Human68k::OP_READWRITE:
		if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;
	}

	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb != NULL) return FS_FILEEXIST;

	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);

	if (f.Find(ps, &m_cEntry) == FALSE) return FS_FILENOTFND;

	pFcb->date = f.GetDate();
	pFcb->time = f.GetTime();

	pFcb->size = f.GetSize();

	pHostFcb = m_cFcb.Alloc(nKey);
	if (pHostFcb == NULL) return FS_OUTOFMEM;
	pHostFcb->SetFilename(f.GetPath());

	if (pHostFcb->SetOpenMode(pFcb->mode) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_ILLEGALMOD;
	}

// Open the file.
	HANDLE hFile = pHostFcb->Open();
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDPATH;
	}

#ifdef XM6_HOST_STRICT_CLOSE
	pHostFcb->Close();
#endif

	return 0;
}

// $4B - Close the file.
int FASTCALL CWinFileSys::Close(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb)
{
	ASSERT(this);
	ASSERT(nKey);
	ASSERT(pFcb);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL) return FS_INVALIDPRM;

	m_cFcb.Free(pHostFcb);

	pFcb = NULL;

	return 0;
}

// $4C - Read file data.
int FASTCALL CWinFileSys::Read(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nAddress, DWORD nSize)
{
	ASSERT(this);
	ASSERT(nKey);
	ASSERT(pFcb);
	ASSERT(nAddress);

	Memory* pMemory = ps->GetMemory();
	ASSERT(pMemory);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL) return FS_NOTOPENED;

	HANDLE hFile = pHostFcb->Open();
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_NOTOPENED;
	}

	DWORD nResult;
#ifdef XM6_HOST_STRICT_CLOSE
	nResult = pHostFcb->SetFilePointer(pFcb->fileptr);
	if (nResult == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_CANTSEEK;
	}
#endif

// Read file data.
	DWORD nTotal = 0;
	BYTE chBuffer[XM6_HOST_FILE_BUFFER_READ];
	for (DWORD nOffset = 0; nOffset < nSize; nOffset += XM6_HOST_FILE_BUFFER_READ) {
		if (nOffset == XM6_HOST_FILE_BUFFER_READ) ps->Ready();

		DWORD n = nSize - nOffset;
		if (n > XM6_HOST_FILE_BUFFER_READ) n = XM6_HOST_FILE_BUFFER_READ;

		nResult = pHostFcb->ReadFile(chBuffer, n);
		if (nResult == (DWORD)-1) {
			m_cFcb.Free(pHostFcb);
			return FS_INVALIDFUNC;
		}

		ps->LockXM();

#if 0
		for (DWORD i = 0; i < nResult; i++) pMemory->WriteByte(nAddress++, chBuffer[i]);
#else
		BYTE* pBuffer = chBuffer;
		DWORD nEnd = nAddress + nResult;
		if (nAddress < nEnd && (nAddress & 1) != 0)
			pMemory->WriteByte(nAddress++, *pBuffer++);

		DWORD nEndWord = nEnd & ~1;
		while (nAddress < nEndWord) {
			DWORD nData = (*pBuffer++)<<8;
			nData |= *pBuffer++;
			pMemory->WriteWord(nAddress, nData);
			nAddress += 2;
		}

		if (nAddress < nEnd) pMemory->WriteByte(nAddress++, *pBuffer);
#endif

		ps->UnlockXM();

		nTotal += nResult;

		if (nResult != n) break;
	}

	pFcb->fileptr += nTotal;

#ifdef XM6_HOST_STRICT_CLOSE
	pHostFcb->Close();
#endif

	return nTotal;
}

// $4D - Write file data.
int FASTCALL CWinFileSys::Write(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nAddress, DWORD nSize)
{
	ASSERT(this);
	ASSERT(nKey);
	ASSERT(pFcb);
	ASSERT(nAddress);

	Memory* pMemory = ps->GetMemory();
	ASSERT(pMemory);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL) return FS_NOTOPENED;

	HANDLE hFile = pHostFcb->Open();
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_NOTOPENED;
	}

	DWORD nResult;
#ifdef XM6_HOST_STRICT_CLOSE
	nResult = pHostFcb->SetFilePointer(pFcb->fileptr);
	if (nResult == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_CANTSEEK;
	}
#endif

// Write file data.
	DWORD nTotal = 0;
	BYTE chBuffer[XM6_HOST_FILE_BUFFER_WRITE];
	for (DWORD nOffset = 0; nOffset < nSize; nOffset += XM6_HOST_FILE_BUFFER_WRITE) {
		if (nOffset == XM6_HOST_FILE_BUFFER_WRITE) ps->Ready();

		DWORD n = nSize - nOffset;
		if (n > XM6_HOST_FILE_BUFFER_WRITE) n = XM6_HOST_FILE_BUFFER_WRITE;

		ps->LockXM();

#if 0
		for (DWORD i = 0; i < n; i++) chBuffer[i] = (BYTE)pMemory->ReadOnly(nAddress++);
#else
		BYTE* pBuffer = chBuffer;
		DWORD nEnd = nAddress + n;
		if (nAddress < nEnd && (nAddress & 1) != 0)
			*pBuffer++ = (BYTE)pMemory->ReadOnly(nAddress++);

		DWORD nEndWord = nEnd & ~1;
		while (nAddress < nEndWord) {
			DWORD nData = pMemory->ReadWord(nAddress);
			*pBuffer++ = (BYTE)(nData>>8);
			*pBuffer++ = (BYTE)nData;
			nAddress += 2;
		}

		if (nAddress < nEnd) *pBuffer = (BYTE)pMemory->ReadOnly(nAddress++);
#endif

		ps->UnlockXM();

		nResult = pHostFcb->WriteFile(chBuffer, n);
		if (nResult == (DWORD)-1) {
			m_cFcb.Free(pHostFcb);
			return FS_CANTWRITE;
		}

		nTotal += nResult;

		if (nResult != n) break;
	}

	pFcb->fileptr += nTotal;

	if (pFcb->size < pFcb->fileptr) pFcb->size = pFcb->fileptr;

#ifdef XM6_HOST_STRICT_CLOSE
	pHostFcb->Close();
#endif

	return nTotal;
}

// $4E - Seek within the file.
int FASTCALL CWinFileSys::Seek(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nMode, int nOffset)
{
	ASSERT(this);
	ASSERT(pFcb);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL) return FS_NOTOPENED;

	DWORD nSeek;
	switch (nMode) {
	case Human68k::SK_BEGIN:
		nSeek = FILE_BEGIN;
		break;
	case Human68k::SK_CURRENT:
#ifdef XM6_HOST_STRICT_CLOSE
		nSeek = FILE_BEGIN;
		nOffset += pFcb->fileptr;
#else
		nSeek = FILE_CURRENT;
#endif
		break;
	case Human68k::SK_END:
		nSeek = FILE_END;
		break;
	default:
		m_cFcb.Free(pHostFcb);
		return FS_CANTSEEK;
	}

	HANDLE hFile = pHostFcb->Open();
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_NOTOPENED;
	}

	DWORD nResult = pHostFcb->SetFilePointer(nOffset, nSeek);
	if (nResult == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_CANTSEEK;
	}

	pFcb->fileptr = nResult;

#ifdef XM6_HOST_STRICT_CLOSE
	pHostFcb->Close();
#endif

	return nResult;
}

DWORD FASTCALL CWinFileSys::TimeStamp(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, WORD nFatDate, WORD nFatTime)
{
	ASSERT(this);
	ASSERT(nKey);
	ASSERT(pFcb);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	if (nFatDate == 0 && nFatTime == 0) {
		return ((DWORD)pFcb->date << 16) | pFcb->time;
	}

	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL) return FS_NOTOPENED;

	BOOL bReopen = FALSE;
	if ((pFcb->mode & 0x0F) == Human68k::OP_READ) {
		bReopen = TRUE;
#ifndef XM6_HOST_STRICT_CLOSE
		pHostFcb->Close();
#endif
		pHostFcb->SetOpenMode(Human68k::OP_READWRITE);
	}

	HANDLE hFile = pHostFcb->Open();
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_NOTOPENED;
	}

	if (pHostFcb->SetFileTime(nFatDate, nFatTime) == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_CANTWRITE;
	}

	if (bReopen) {
		pHostFcb->SetOpenMode(pFcb->mode);
#ifndef XM6_HOST_STRICT_CLOSE
		pHostFcb->Close();
		hFile = pHostFcb->Open();
		if (hFile == INVALID_HANDLE_VALUE) {
			m_cFcb.Free(pHostFcb);
			return FS_NOTOPENED;
		}
		DWORD nResult = pHostFcb->SetFilePointer(pFcb->fileptr);
		if (nResult == (DWORD)-1) {
			m_cFcb.Free(pHostFcb);
			return FS_CANTSEEK;
		}
#endif
	}

#ifdef XM6_HOST_STRICT_CLOSE
	pHostFcb->Close();
#endif

	return 0;
}

int FASTCALL CWinFileSys::GetCapacity(WindrvContext* ps, Human68k::capacity_t* pCapacity)
{
	ASSERT(this);
	ASSERT(pCapacity);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	DWORD nUnit = ps->GetUnit();
	return m_cEntry.GetCapacity(nUnit, pCapacity);
}

int FASTCALL CWinFileSys::CtrlDrive(WindrvContext* ps, Human68k::ctrldrive_t* pCtrlDrive)
{
	ASSERT(this);
	ASSERT(pCtrlDrive);

	DWORD nUnit = ps->GetUnit();

	switch (pCtrlDrive->status) {
	case 0:		// File-system detail.
	case 9:		// File-system detail.
		pCtrlDrive->status = (BYTE)m_cEntry.GetStatus(nUnit);
		return pCtrlDrive->status;

	case 1:		// Implementation detail.
		m_cEntry.isMediaOffline(ps, FALSE);	// File-system detail.
		m_cEntry.Eject(nUnit);
		return 0;

	case 2:		// File-system detail.
	case 3:		// File-system detail.
	case 4:		// File-system detail.
	case 5:		// File-system detail.
	case 6:		// File-system detail.
	case 7:		// File-system detail.
		return 0;

	case 8:		// File-system detail.
		return 1;
	}

	return FS_INVALIDFUNC;
}

// $52 - Get the DPB.
int FASTCALL CWinFileSys::GetDPB(WindrvContext* ps, Human68k::dpb_t* pDpb)
{
	ASSERT(this);
	ASSERT(pDpb);

	DWORD nUnit = ps->GetUnit();

	Human68k::capacity_t cap;
	BOOL bResult = m_cEntry.GetCapacityCache(nUnit, &cap);
	if (bResult == FALSE) {
		m_cEntry.isMediaOffline(ps);

		m_cEntry.GetCapacity(nUnit, &cap);
	}

	DWORD nSize = 1;
	DWORD nShift = 0;
	for (;;) {
		if (nSize >= cap.sectors) break;
		nSize <<= 1;
		nShift++;
	}

	DWORD nFat = 1 * cap.sectors;
	DWORD nRoot = 2 * cap.sectors;
	DWORD nData = 3 * cap.sectors;

	pDpb->sector_size = (WORD)cap.bytes;		// File-system detail.
	pDpb->cluster_size = (BYTE)cap.sectors - 1;	// File-system detail.
	pDpb->shift = (BYTE)nShift;					// File-system detail.
	pDpb->fat_sector = (WORD)nFat;				// File-system detail.
	pDpb->fat_max = 1;							// File-system detail.
	pDpb->fat_size = (BYTE)cap.sectors;			// File-system detail.
	pDpb->file_max =							// File-system detail.
		(WORD)(cap.sectors * cap.bytes / 0x20);
	pDpb->data_sector = (WORD)nData;		   	// File-system detail.
	pDpb->cluster_max =	(WORD)cap.clusters;		// File-system detail.
	pDpb->root_sector = (WORD)nRoot;			// File-system detail.
	pDpb->media = 0xF3;							// File-system detail.

	if (m_nOption & WINDRV_OPT_MEDIABYTE) {
		pDpb->media = m_cEntry.GetMediaByte(nUnit);
	}

	return 0;
}

// $53 - Read sectors.
int FASTCALL CWinFileSys::DiskRead(WindrvContext* ps, DWORD nAddress, DWORD nSector, DWORD nSize)
{
	ASSERT(this);
	ASSERT(nAddress);

	Memory* pMemory = ps->GetMemory();
	ASSERT(pMemory);

	DWORD nUnit = ps->GetUnit();

	if (nSize != 1) return FS_NOTIOCTRL;

	Human68k::capacity_t cap;
	BOOL bResult = m_cEntry.GetCapacityCache(nUnit, &cap);
	if (bResult == FALSE) {
		m_cEntry.isMediaOffline(ps);

		m_cEntry.GetCapacity(nUnit, &cap);
	}

	CHostFiles* pHostFiles = m_cFiles.Search(nSector);
	if (pHostFiles) {
		Human68k::dirent_t dir;
		memcpy(&dir, pHostFiles->GetEntry(), sizeof(dir));

		dir.cluster = (WORD)(m_nHostSectorCount + 2);	// File-system detail.
		m_nHostSectorBuffer[m_nHostSectorCount] = nSector;	// File-system detail.
		m_nHostSectorCount++;
		m_nHostSectorCount %= XM6_HOST_PSEUDO_CLUSTER_MAX;

		ps->LockXM();

		BYTE* p = (BYTE*)&dir;
		for (int i = 0; i < 0x20; i++) pMemory->WriteByte(nAddress++, *p++);
		for (int i = 0x20; i < 0x200; i++) pMemory->WriteByte(nAddress++, 0xFF);

		ps->UnlockXM();

		return 0;
	}

	DWORD n = nSector - (3 * cap.sectors);
	DWORD nMod = 1;
	if (cap.sectors) {
		nMod = n % cap.sectors;
		n /= cap.sectors;
	}

	if (nMod == 0 && n < XM6_HOST_PSEUDO_CLUSTER_MAX) {
		pHostFiles = m_cFiles.Search(m_nHostSectorBuffer[n]);	// File-system detail.
		if (pHostFiles) {
			if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

			CHostFcb f;
			f.SetFilename(pHostFiles->GetPath());
			f.SetOpenMode(Human68k::OP_READ);
			HANDLE hFile = f.Open();
			if (hFile == INVALID_HANDLE_VALUE) return FS_NOTIOCTRL;
			BYTE chBuffer[512];
			memset(chBuffer, 0, sizeof(chBuffer));
			DWORD nResult = f.ReadFile(chBuffer, 512);
			f.Close();
			if (nResult == (DWORD)-1) return FS_NOTIOCTRL;

			ps->LockXM();

			for (int i = 0; i < 0x200; i++) pMemory->WriteByte(nAddress++, chBuffer[i]);

			ps->UnlockXM();

			return 0;
		}
	}

	return FS_NOTIOCTRL;
}

// $54 - Write sectors.
int FASTCALL CWinFileSys::DiskWrite(WindrvContext* ps, DWORD nAddress, DWORD nSector, DWORD nSize)
{
	ASSERT(this);
	ASSERT(nAddress);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	printf("%d %d %d", nAddress, nSector, nSize);
	return FS_NOTIOCTRL;
}

// $55 - I/O control.
int FASTCALL CWinFileSys::IoControl(WindrvContext* ps, Human68k::ioctrl_t* pIoctrl, DWORD nFunction)
{
	ASSERT(this);

	switch (nFunction) {
	case 0:
		pIoctrl->media = 0xF3;
		if (m_nOption & WINDRV_OPT_MEDIABYTE) {
			DWORD nUnit = ps->GetUnit();
			pIoctrl->media = m_cEntry.GetMediaByte(nUnit);
		}
		return 0;

	case 1:
		pIoctrl->param = (DWORD)-1;
		return 0;

	case 2:
		switch (pIoctrl->param) {
		case -1:
			m_cEntry.isMediaOffline(ps);
			return 0;

		case 0:
		case 1:
			return 0;
		}
		break;

	case -1:
		memcpy(pIoctrl->buffer, "WindrvXM", 8);
		return 0;

	case -2:
		SetOption(pIoctrl->param);
		return 0;

	case -3:
		pIoctrl->param = GetOption();
		return 0;
	}

	return FS_NOTIOCTRL;
}

int FASTCALL CWinFileSys::Flush(WindrvContext* ps)
{
	ASSERT(this);

	DWORD nUnit = ps->GetUnit();

	m_cEntry.LockCache();
	m_cEntry.EraseCache(nUnit);
	m_cEntry.UnlockCache();

	return 0;
}

int FASTCALL CWinFileSys::CheckMedia(WindrvContext* ps)
{
	ASSERT(this);

	CheckKernel(ps);

#ifndef XM6_HOST_STRICT_TIMEOUT
	m_cEntry.CheckTimeout();
#endif

	BOOL bResult = m_cEntry.CheckMediaUpdate(ps);

	if (bResult == FALSE) {
#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
		m_cEntry.SetMediaUpdate(ps, TRUE);
#endif

		return FS_INVALIDFUNC;
	}

	return 0;
}

int FASTCALL CWinFileSys::Lock(WindrvContext* ps)
{
	ASSERT(this);

	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	return 0;
}

#if 0
DWORD FASTCALL CWinFileSys::GetLastError(DWORD nUnit) const
{
	ASSERT(this);
	ASSERT(m_cEntry.GetBase(nUnit));

	return FS_INVALIDFUNC;
}
#endif

void FASTCALL CWinFileSys::SetOption(DWORD nOption)
{
	ASSERT(this);

	DWORD nDiff = m_nOption ^ nOption;
	if (nDiff & 0x7F3F1F3F) {
		m_cEntry.LockCache();
		m_cEntry.CleanCache();
		m_cEntry.UnlockCache();
	}

	m_nOption = nOption;
	CHostFilename::SetOption(nOption);
	CHostPath::SetOption(nOption);
}

void FASTCALL CWinFileSys::InitOption(const BYTE* pOption)
{
	pOption += strlen((char*)pOption) + 1;

	DWORD nOption = m_nOptionDefault;
	for (;;) {
		const BYTE* p = pOption;
		BYTE c = *p++;
		if (c == '\0') break;

		DWORD nMode;
		if (c == '+') {
			nMode = 1;
		}
		else if (c == '-') {
			nMode = 0;
		} else {
			pOption += strlen((char*)pOption) + 1;
			continue;
		}

		for (;;) {
			c = *p++;
			if (c == '\0') break;

			DWORD nBit = 0;
			switch (c) {
			case 'D': case 'd': nBit = WINDRV_OPT_REMOVE; break;
			case 'K': case 'k': nBit = WINDRV_OPT_TWENTYONE; break;
			case 'M': case 'm': nBit = WINDRV_OPT_MEDIABYTE; break;
			case 'A': case 'a': nBit = WINDRV_OPT_CONVERT_LENGTH; break;
			case 'T': case 't': nBit = WINDRV_OPT_COMPARE_LENGTH; nMode ^= 1; break;
			case 'C': case 'c': nBit = WINDRV_OPT_ALPHABET; break;

			case 'E': nBit = WINDRV_OPT_CONVERT_PERIOD; break;
			case 'P': nBit = WINDRV_OPT_CONVERT_PERIODS; break;
			case 'N': nBit = WINDRV_OPT_CONVERT_HYPHEN; break;
			case 'H': nBit = WINDRV_OPT_CONVERT_HYPHENS; break;
			case 'X': nBit = WINDRV_OPT_CONVERT_BADCHAR; break;
			case 'S': nBit = WINDRV_OPT_CONVERT_SPACE; break;

			case 'e': nBit = WINDRV_OPT_REDUCED_PERIOD; break;
			case 'p': nBit = WINDRV_OPT_REDUCED_PERIODS; break;
			case 'n': nBit = WINDRV_OPT_REDUCED_HYPHEN; break;
			case 'h': nBit = WINDRV_OPT_REDUCED_HYPHENS; break;
			case 'x': nBit = WINDRV_OPT_REDUCED_BADCHAR; break;
			case 's': nBit = WINDRV_OPT_REDUCED_SPACE; break;
			}

			if (nMode) {
				nOption |= nBit;
			} else {
				nOption &= ~nBit;
			}
		}

		pOption = p;
	}

	if (nOption != m_nOption) {
		SetOption(nOption);
	}
}

// Read the volume label.
BOOL FASTCALL CWinFileSys::FilesVolume(WindrvContext* ps, Human68k::files_t* pFiles)
{
	ASSERT(this);
	ASSERT(pFiles);

	DWORD nUnit = ps->GetUnit();

// Read the volume label.
	TCHAR szVolume[32];
	BOOL bResult = m_cEntry.GetVolumeCache(nUnit, szVolume);
	if (bResult == FALSE) {
		m_cEntry.isMediaOffline(ps);

// Read the volume label.
		m_cEntry.GetVolume(nUnit, szVolume);
	}
	if (szVolume[0] == _T('\0')) return FALSE;

	pFiles->attr = Human68k::AT_VOLUME;
	pFiles->time = 0;
	pFiles->date = 0;
	pFiles->size = 0;

	CHostFilename fname;
	fname.SetWin32(szVolume);
	fname.SetHuman();
	strcpy((char*)pFiles->full, (char*)fname.GetHuman());

	return TRUE;
}

void FASTCALL CWinFileSys::CheckKernel(WindrvContext* ps)
{
	ASSERT(this);

	if ((m_nOption & WINDRV_OPT_TWENTYONE) == 0) return;

	ps->LockXM();

	Memory* pMemory = ps->GetMemory();
	ASSERT(pMemory);

	if (m_nKernel < 1024) {
		if (m_nKernel > 0) {
			m_nKernel--;		// File-system detail.
			goto CheckKernelExit;
		}

		if (m_nKernelSearch == 0) {
			DWORD n = 0x6800;
			for (;;) {
				DWORD nData = pMemory->ReadWord(n);
				if (nData == 'NU') {
					if (pMemory->ReadWord(n + 2) == 'L ') break;
				}
				n += 2;
				if (n >= 0x20000 - 2) {
					m_nKernel = 0xFFFFFFFF;		// File-system detail.
					goto CheckKernelExit;
				}
			}
			n -= 14;
			m_nKernelSearch = n;
		}

		DWORD n = m_nKernelSearch;
		for (;;) {
			n = (pMemory->ReadWord(n) << 16) | pMemory->ReadWord(n + 2);
			if (n == 0xFFFFFFFF) {
				m_nKernel = XM6_HOST_TWENTYONE_CHECK_COUNT;	// File-system detail.
				goto CheckKernelExit;
			}

			DWORD x1 = (pMemory->ReadWord(n + 14) << 16) | pMemory->ReadWord(n + 16);

			if (x1 == '*Twe') {
				DWORD x2 = (pMemory->ReadWord(n + 18) << 16) | pMemory->ReadWord(n + 20);
				if (x2 == 'nty*') {
					m_nKernel = 0xFFFFFFFF;		// File-system detail.
					goto CheckKernelExit;
				}
				continue;
			}

			if (x1 == '?Twe') {
				DWORD x2 = (pMemory->ReadWord(n + 18) << 16) | pMemory->ReadWord(n + 20);
				if (x2 == 'nty?' || x2 == 'ntyE') {
					break;
				}
				continue;
			}
		}

		m_nKernel = n + 22;
	} else {
		if (m_nKernel == 0xFFFFFFFF) {
			goto CheckKernelExit;
		}
	}

	{
		DWORD nKernelOption =
			(pMemory->ReadWord(m_nKernel) << 16) | pMemory->ReadWord(m_nKernel + 2);

		DWORD nOption = m_nOption &
			~(WINDRV_OPT_ALPHABET | WINDRV_OPT_COMPARE_LENGTH | WINDRV_OPT_CONVERT_LENGTH);

		if (nKernelOption & 0x40000000) {	// Implementation detail.
			nOption |= WINDRV_OPT_ALPHABET;
		}
		if (nKernelOption & 0x08000000) {	// Implementation detail.
			nOption |= WINDRV_OPT_COMPARE_LENGTH;
		}
		if (nKernelOption & 0x00400000) {	// Implementation detail.
			nOption |= WINDRV_OPT_CONVERT_LENGTH;
		}

		if (nOption != m_nOption) {
			SetOption(nOption);
		}
	}

CheckKernelExit:
	ps->UnlockXM();
}


// Constructor.
CHost::CHost(CFrmWnd *pWnd) : CComponent(pWnd)
{
	m_dwID = MAKEID('H', 'O', 'S', 'T');
	m_strDesc = _T("Host FileSystem");

}

// Initialize.
BOOL FASTCALL CHost::Init()
{
	ASSERT(this);

	if (!CComponent::Init()) {
		return FALSE;
	}

	::GetVM()->SetHostFileSystem(&m_WinFileSys);

	return TRUE;
}

void FASTCALL CHost::Cleanup()
{
	ASSERT(this);

	::GetVM()->SetHostFileSystem(NULL);

	m_WinFileSys.Reset();

	CComponent::Cleanup();
}

void FASTCALL CHost::ApplyCfg(const Config* pConfig)
{
	ASSERT(this);
	ASSERT(pConfig);

	m_WinFileSys.ApplyCfg(pConfig);
}

#endif
