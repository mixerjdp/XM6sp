//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//	[MFC smoke tests]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_smoke_h)
#define mfc_smoke_h

BOOL SmokeIsSaveStateCommand();
BOOL SmokeIsVisibleCommand();
BOOL SmokeVisibleShouldPoll(DWORD dwNow);
void SmokeLogLine(LPCTSTR msg);
void SmokeLogFormat(LPCTSTR fmt, LPCTSTR value);
BOOL SmokeReadOptionValue(LPCTSTR cmd, LPCTSTR opt, LPCTSTR *after, TCHAR *value, int valueCount);

#endif	// mfc_smoke_h
#endif	// _WIN32
