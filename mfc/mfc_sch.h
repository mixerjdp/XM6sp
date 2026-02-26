//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ‚o‚hD(ytanaka@ipc-tokai.or.jp)
//	[ MFC ƒXƒPƒWƒ…[ƒ‰ ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_sch_h)
#define mfc_sch_h

//===========================================================================
//
//	ƒXƒPƒWƒ…[ƒ‰
//
//===========================================================================
class CScheduler : public CComponent
{
public:
	// Šî–{ƒtƒ@ƒ“ƒNƒVƒ‡ƒ“
	CScheduler(CFrmWnd *pFrmWnd);
										// ƒRƒ“ƒXƒgƒ‰ƒNƒ^
	BOOL FASTCALL Init();
										// ‰Šú‰»
	void FASTCALL Cleanup();
										// ƒNƒŠ[ƒ“ƒAƒbƒv
	void FASTCALL ApplyCfg(const Config *pConfig);
										// Ý’è“K—p
#if defined(_DEBUG)
	void AssertValid() const;
										// f’f
#endif	// _DEBUG

	// ŽÀs§Œä
	void FASTCALL Reset();
										// ŽžŠÔ‚ðƒŠƒZƒbƒg
	void FASTCALL Run();
										// ŽÀs
	void FASTCALL Stop();
										// ƒXƒPƒWƒ…[ƒ‰’âŽ~

	// ƒZ[ƒuEƒ[ƒh
	BOOL FASTCALL Save(Fileio *pFio, int nVer);
										// ƒZ[ƒu
	BOOL FASTCALL Load(Fileio *pFio, int nVer);
										// ƒ[ƒh
	BOOL FASTCALL HasSavedEnable() const { return m_bSavedValid; }
										// ƒZ[ƒuŽž‚ÉEnableó‘Ô‚ð•Û‘¶‚µ‚Ä‚¢‚é‚©
	BOOL FASTCALL GetSavedEnable() const { return m_bSavedEnable; }
										// ƒZ[ƒuŽž‚ÉEnableó‘Ô‚¾‚Á‚½‚©
	void FASTCALL SetSavedEnable(BOOL bEnable) { m_bSavedEnable = bEnable; }
										// ƒZ[ƒuŽž‚Ìó‘Ô‚ðÝ’è

	// ‚»‚Ì‘¼
	void FASTCALL Menu(BOOL bMenu)		{ m_bMenu = bMenu; }
										// ƒƒjƒ…[’Ê’m
	void FASTCALL Activate(BOOL bAct)	{ m_bActivate = bAct; }
										// ƒAƒNƒeƒBƒu’Ê’m
	void FASTCALL SyncDisasm();
										// ‹tƒAƒZƒ“ƒuƒ‹“¯Šú
	int FASTCALL GetFrameRate();
										// ƒtƒŒ [ƒ€ƒŒ [ƒgŽæ“¾
	void FASTCALL OnMainFramePresented();
										// Confirmacion asincrona de frame consumido por UI

private:
	static UINT ThreadFunc(LPVOID pParam);
										// ƒXƒŒƒbƒhŠÖ”
	unsigned __int64 FASTCALL GetTimeMicro();
	unsigned __int64 FASTCALL GetTimeMilli();
	DWORD FASTCALL GetTime()			{ return (DWORD)GetTimeMilli(); }
										// ŽžŠÔŽæ“¾
	void FASTCALL Lock()				{ ::LockVM(); }
										// VMƒƒbƒN
	void FASTCALL Unlock()				{ ::UnlockVM(); }
										// VMƒAƒ“ƒƒbƒN
	void FASTCALL Refresh();
										// ƒŠƒtƒŒƒbƒVƒ…
	CPU *m_pCPU;
										// CPU
	Render *m_pRender;
										// ƒŒƒ“ƒ_ƒ‰
	CWinThread *m_pThread;
										// ƒXƒŒƒbƒhƒ|ƒCƒ“ƒ^
	CSound *m_pSound;
										// ƒTƒEƒ“ƒh
	CInput *m_pInput;
										// ƒCƒ“ƒvƒbƒg
	BOOL m_bExitReq;
										// ƒXƒŒƒbƒhI—¹—v‹
	DWORD m_dwExecTime;
										// ƒ^ƒCƒ}[ƒJƒEƒ“ƒg(ŽÀs)
	int m_nSubWndNum;
										// ƒTƒuƒEƒBƒ“ƒhƒE‚ÌŒÂ”
	int m_nSubWndDisp;
										// ƒTƒuƒEƒBƒ“ƒhƒE‚Ì•\Ž¦(-1:ƒƒCƒ“‰æ–Ê)
	BOOL m_bMPUFull;
										// MPU‚‘¬ƒtƒ‰ƒO
	BOOL m_bVMFull;
										// VM‚‘¬ƒtƒ‰ƒO
	DWORD m_dwDrawCount;
										// ƒƒCƒ“ƒEƒBƒ“ƒhƒE•\Ž¦‰ñ”
	DWORD m_dwDrawPrev;
										// ƒƒCƒ“ƒEƒBƒ“ƒhƒE•\Ž¦‰ñ”(‘O)
	DWORD m_dwDrawTime;
										// ƒƒCƒ“ƒEƒBƒ“ƒhƒE•\Ž¦ŽžŠÔ
	DWORD m_dwDrawBackup;
										// ƒƒCƒ“ƒEƒBƒ“ƒhƒE•\Ž¦‰ñ”(‘O)
	BOOL m_bMenu;
										// ƒƒjƒ…[ƒtƒ‰ƒO
	BOOL m_bActivate;
										// ƒAƒNƒeƒBƒuƒtƒ‰ƒO
	BOOL m_bBackup;
										// Enableƒtƒ‰ƒOƒoƒbƒNƒAƒbƒv
	BOOL m_bSavedValid;
										// ƒZ[ƒuŽž‚ÉEnableó‘Ô‚ð•Û‘¶‚µ‚Ä‚¢‚é‚©
	BOOL m_bSavedEnable;
										// ƒZ[ƒuŽž‚ÉEnable‚¾‚Á‚½‚©
	LARGE_INTEGER m_liFreq;
										// Frecuencia del contador de alto rendimiento
};

#endif	// mfc_sch_h
#endif	// _WIN32
