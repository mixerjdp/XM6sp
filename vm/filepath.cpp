//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	[ Filepath ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)
#include <windows.h>
#endif
#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"

#include <string.h>
#include <sys/stat.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

static uint64_t filepath_pack_time(const filepath_time_t& t)
{
	return (static_cast<uint64_t>(t.high) << 32) | static_cast<uint64_t>(t.low);
}

static filepath_time_t filepath_unpack_time(uint64_t v)
{
	filepath_time_t t;
	t.low = static_cast<DWORD>(v & 0xffffffffu);
	t.high = static_cast<DWORD>((v >> 32) & 0xffffffffu);
	return t;
}

static bool filepath_query_timestamp(const TCHAR* path, filepath_time_t* out)
{
	if (!path || !out) {
		return false;
	}

	struct stat st;
	if (stat(path, &st) != 0) {
		return false;
	}

	uint64_t ns = static_cast<uint64_t>(st.st_mtime) * 1000000000ULL;
#if defined(__APPLE__)
	ns += static_cast<uint64_t>(st.st_mtimespec.tv_nsec);
#elif defined(__linux__) || defined(__unix__)
	ns += static_cast<uint64_t>(st.st_mtim.tv_nsec);
#endif

	*out = filepath_unpack_time(ns);
	return true;
}

static bool filepath_get_executable_path(TCHAR* out_path, size_t out_cap)
{
	if (!out_path || out_cap == 0) {
		return false;
	}
	out_path[0] = _T('\0');

#if defined(_WIN32)
	DWORD len = GetModuleFileNameA(NULL, out_path, static_cast<DWORD>(out_cap));
	if (len == 0 || len >= out_cap) {
		out_path[0] = _T('\0');
		return false;
	}
	return true;
#else
	ssize_t len = readlink("/proc/self/exe", out_path, out_cap - 1);
	if (len <= 0 || static_cast<size_t>(len) >= out_cap) {
		return false;
	}
	out_path[len] = '\0';
	return true;
#endif
}

Filepath::Filepath()
{
	Clear();
	m_bUpdate = FALSE;
	m_SavedTime.low = 0;
	m_SavedTime.high = 0;
	m_CurrentTime.low = 0;
	m_CurrentTime.high = 0;
}

Filepath::~Filepath()
{
}

Filepath& Filepath::operator=(const Filepath& path)
{
	SetPath(path.GetPath());

	m_bUpdate = FALSE;
	if (path.IsUpdate()) {
		m_bUpdate = TRUE;
		path.GetUpdateTime(&m_SavedTime, &m_CurrentTime);
	}

	return *this;
}

void FASTCALL Filepath::Clear()
{
	ASSERT(this);

	m_szPath[0] = _T('\0');
	m_szDrive[0] = _T('\0');
	m_szDir[0] = _T('\0');
	m_szFile[0] = _T('\0');
	m_szExt[0] = _T('\0');
}

void FASTCALL Filepath::SysFile(SysFileType sys)
{
	int nFile;

	ASSERT(this);

	nFile = (int)sys;
	_tcscpy(m_szPath, SystemFile[nFile]);
	Split();

	if (SystemDir[0] != _T('\0')) {
		_tsplitpath(SystemDir, m_szDrive, m_szDir, NULL, NULL);
		Make();
	} else {
		SetBaseDir();
	}
}

void FASTCALL Filepath::SetPath(const TCHAR* lpszPath)
{
	ASSERT(this);
	ASSERT(lpszPath);
	ASSERT(_tcslen(lpszPath) < _MAX_PATH);

	_tcscpy(m_szPath, lpszPath);
	Split();

	if (_tcslen(m_szPath) > 0) {
		if (_tcslen(m_szDrive) == 0) {
			if (_tcslen(m_szDir) == 0) {
				SetCurDir();
			}
		}
	}
}

void FASTCALL Filepath::Split()
{
	ASSERT(this);

	m_szDrive[0] = _T('\0');
	m_szDir[0] = _T('\0');
	m_szFile[0] = _T('\0');
	m_szExt[0] = _T('\0');

	_tsplitpath(m_szPath, m_szDrive, m_szDir, m_szFile, m_szExt);
}

void FASTCALL Filepath::Make()
{
	ASSERT(this);

	_tmakepath(m_szPath, m_szDrive, m_szDir, m_szFile, m_szExt);
}

void FASTCALL Filepath::SetBaseDir()
{
	TCHAR szModule[_MAX_PATH];

	ASSERT(this);

	if (!filepath_get_executable_path(szModule, _MAX_PATH)) {
		SetCurDir();
		return;
	}

	_tsplitpath(szModule, m_szDrive, m_szDir, NULL, NULL);
	Make();
}

void FASTCALL Filepath::SetBaseFile(const TCHAR* lpszName)
{
	TCHAR szModule[_MAX_PATH];

	ASSERT(this);
	ASSERT(lpszName);
	ASSERT(_tcslen(m_szPath) > 0);

	if (!filepath_get_executable_path(szModule, _MAX_PATH)) {
		SetCurDir();
	}

	_tsplitpath(szModule, m_szDrive, m_szDir, m_szFile, NULL);
	_tcscpy(m_szFile, lpszName);
	Make();
}

void FASTCALL Filepath::SetCurDir()
{
	TCHAR szCurDir[_MAX_PATH];

	ASSERT(this);
	ASSERT(_tcslen(m_szPath) > 0);

#if defined(_WIN32)
	if (::GetCurrentDirectoryA(_MAX_PATH, szCurDir) == 0) {
		szCurDir[0] = _T('\0');
	}
#else
	if (!getcwd(szCurDir, _MAX_PATH)) {
		szCurDir[0] = _T('\0');
	}
#endif

	_tsplitpath(szCurDir, m_szDrive, m_szDir, NULL, NULL);
	Make();
}

BOOL FASTCALL Filepath::IsClear() const
{
	if ((m_szPath[0] == _T('\0')) &&
		(m_szDrive[0] == _T('\0')) &&
		(m_szDir[0] == _T('\0')) &&
		(m_szFile[0] == _T('\0')) &&
		(m_szExt[0] == _T('\0'))) {
		return TRUE;
	}

	return FALSE;
}

const char* FASTCALL Filepath::GetShort() const
{
	ASSERT(this);

#if defined(UNICODE)
	char file[_MAX_FNAME];
	char ext[_MAX_EXT];
	if (::WideCharToMultiByte(CP_ACP, 0, m_szFile, -1, file, sizeof(file), NULL, NULL) <= 0) {
		file[0] = '\0';
	}
	if (::WideCharToMultiByte(CP_ACP, 0, m_szExt, -1, ext, sizeof(ext), NULL, NULL) <= 0) {
		ext[0] = '\0';
	}
	strcpy(ShortName, file);
	strcat(ShortName, ext);
#else
	strcpy(ShortName, m_szFile);
	strcat(ShortName, m_szExt);
#endif

	ShortName[59] = '\0';
	return (const char*)ShortName;
}

const TCHAR* FASTCALL Filepath::GetFileExt() const
{
	ASSERT(this);

	_tcscpy(FileExt, m_szFile);
	_tcscat(FileExt, m_szExt);
	return (const TCHAR*)FileExt;
}

BOOL FASTCALL Filepath::CmpPath(const Filepath& path) const
{
	if (_tcscmp(path.GetPath(), GetPath()) == 0) {
		return TRUE;
	}

	return FALSE;
}

void FASTCALL Filepath::ClearDefaultDir()
{
	DefaultDir[0] = _T('\0');
}

void FASTCALL Filepath::SetDefaultDir(const TCHAR* lpszPath)
{
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];

	ASSERT(lpszPath);

	_tsplitpath(lpszPath, szDrive, szDir, NULL, NULL);
	_tcscpy(DefaultDir, szDrive);
	_tcscat(DefaultDir, szDir);
}

const TCHAR* FASTCALL Filepath::GetDefaultDir()
{
	return (const TCHAR*)DefaultDir;
}

void FASTCALL Filepath::ClearSystemDir()
{
	SystemDir[0] = _T('\0');
}

void FASTCALL Filepath::SetSystemDir(const TCHAR* lpszPath)
{
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	if (!lpszPath) {
		ClearSystemDir();
		return;
	}
	_tsplitpath(lpszPath, szDrive, szDir, NULL, NULL);
	_tcscpy(SystemDir, szDrive);
	_tcscat(SystemDir, szDir);
}

const TCHAR* FASTCALL Filepath::GetSystemDir()
{
	return (const TCHAR*)SystemDir;
}

BOOL FASTCALL Filepath::Save(Fileio *fio, int /*ver*/)
{
	TCHAR szPath[_MAX_PATH];
	filepath_time_t saved;

	ASSERT(this);
	ASSERT(fio);

	memset(szPath, 0, sizeof(szPath));
	_tcscpy(szPath, m_szPath);

	if (!fio->Write(szPath, sizeof(szPath))) {
		return FALSE;
	}

	saved.low = 0;
	saved.high = 0;
	filepath_query_timestamp(szPath, &saved);

	if (!fio->Write(&saved, sizeof(saved))) {
		return FALSE;
	}

	return TRUE;
}

BOOL FASTCALL Filepath::Load(Fileio *fio, int /*ver*/)
{
	TCHAR szPath[_MAX_PATH];

	ASSERT(this);
	ASSERT(fio);

	if (!fio->Read(szPath, sizeof(szPath))) {
		return FALSE;
	}

	SetPath(szPath);

	if (!fio->Read(&m_SavedTime, sizeof(m_SavedTime))) {
		return FALSE;
	}

	if (!filepath_query_timestamp(szPath, &m_CurrentTime)) {
		return TRUE;
	}

	m_bUpdate = (filepath_pack_time(m_CurrentTime) > filepath_pack_time(m_SavedTime)) ? TRUE : FALSE;
	return TRUE;
}

BOOL FASTCALL Filepath::IsUpdate() const
{
	ASSERT(this);
	return m_bUpdate;
}

void FASTCALL Filepath::GetUpdateTime(filepath_time_t *pSaved, filepath_time_t *pCurrent) const
{
	ASSERT(this);
	ASSERT(m_bUpdate);
	*pSaved = m_SavedTime;
	*pCurrent = m_CurrentTime;
}

const TCHAR* Filepath::SystemFile[] = {
	_T("IPLROM.DAT"),
	_T("IPLROMXV.DAT"),
	_T("IPLROMCO.DAT"),
	_T("IPLROM30.DAT"),
	_T("ROM30.DAT"),
	_T("CGROM.DAT"),
	_T("CGROM.TMP"),
	_T("SCSIINROM.DAT"),
	_T("SCSIEXROM.DAT"),
	_T("SRAM.DAT")
};

char Filepath::ShortName[_MAX_FNAME + _MAX_DIR];
TCHAR Filepath::FileExt[_MAX_FNAME + _MAX_DIR];
TCHAR Filepath::DefaultDir[_MAX_PATH];
TCHAR Filepath::SystemDir[_MAX_PATH];
