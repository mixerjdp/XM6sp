//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ‚o‚hD(ytanaka@ipc-tokai.or.jp)
//	[ MFC ƒtƒŒ[ƒ€ƒEƒBƒ“ƒhƒE ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_frm_h)
#define mfc_frm_h

//---------------------------------------------------------------------------
//
//	ƒEƒBƒ“ƒhƒEƒƒbƒZ[ƒW
//
//---------------------------------------------------------------------------
#define WM_KICK			WM_APP				// ƒGƒ~ƒ…ƒŒ[ƒ^ƒXƒ^[ƒg
#define WM_SHELLNOTIFY	(WM_USER + 5)		// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ó‘Ô•Ï‰»

//===========================================================================
//
//	ƒtƒŒ[ƒ€ƒEƒBƒ“ƒhƒE
//
//===========================================================================
class CFrmWnd : public CFrameWnd
{
public:
	// ‰Šú‰»
	CFrmWnd();
										// ƒRƒ“ƒXƒgƒ‰ƒNƒ^
	BOOL Init();
										// ‰Šú‰»

	// Žæ“¾
	CDrawView* FASTCALL GetView() const;
										// •`‰æƒrƒ…[Žæ“¾
	CComponent* FASTCALL GetFirstComponent() const;
										// Å‰‚ÌƒRƒ“ƒ|[ƒlƒ“ƒg‚ðŽæ“¾
	CScheduler* FASTCALL GetScheduler() const;
										// ƒXƒPƒWƒ…[ƒ‰Žæ“¾
	CSound* FASTCALL GetSound() const;
										// ƒTƒEƒ“ƒhŽæ“¾
	CInput* FASTCALL GetInput() const;
										// ƒCƒ“ƒvƒbƒgŽæ“¾
	CPort* FASTCALL GetPort() const;
										// ƒ|[ƒgŽæ“¾
	CMIDI* FASTCALL GetMIDI() const;
										// MIDIŽæ“¾
	CTKey* FASTCALL GetTKey() const;
										// TrueKeyŽæ“¾
	CHost* FASTCALL GetHost() const;
										// HostŽæ“¾
	CInfo* FASTCALL GetInfo() const;
										// InfoŽæ“¾
	CConfig* FASTCALL GetConfig() const;
										// ƒRƒ“ƒtƒBƒOŽæ“¾

	// ƒXƒe[ƒ^ƒXƒrƒ…[ƒTƒ|[ƒg
	void FASTCALL RecalcStatusView();
										// ƒXƒe[ƒ^ƒXƒrƒ…[Ä”z’u

	// ƒTƒuƒEƒBƒ“ƒhƒEƒTƒ|[ƒg
	LPCTSTR FASTCALL GetWndClassName() const;
										// ƒEƒBƒ“ƒhƒEƒNƒ‰ƒX–¼Žæ“¾
	BOOL FASTCALL IsPopupSWnd() const;
										// ƒ|ƒbƒvƒAƒbƒvƒTƒuƒEƒBƒ“ƒhƒE‚©

	// ƒhƒ‰ƒbƒO•ƒhƒƒbƒvƒTƒ|[ƒg
	BOOL FASTCALL InitCmdSub(int nDrive, LPCTSTR lpszPath);
									// ƒRƒ}ƒ“ƒhƒ‰ƒCƒ“ˆ— ƒTƒu
	BOOL m_bFullScreen;
	BOOL m_bBorderless;
	BOOL m_bVSyncEnabled;
	WINDOWPLACEMENT m_wpPrev;
	DWORD m_dwPrevStyle;
	DWORD m_dwPrevExStyle;

	void EnterBorderlessFullscreen();
	void ExitBorderlessFullscreen();
	void OnToggleRenderer();
	void OnToggleVSync();
	void OnToggleOSD();

	// Nombre de Archivo XM6   *-*
	CString NombreArchivoXM6;
	CString RutaCompletaArchivoXM6;
	CString RutaSaveStates;

	// Guardar estado de ventana
	void SaveFrameWnd();
	// Guardar Estado de disco
	void SaveDiskState();


protected:
	// ƒI[ƒo[ƒ‰ƒCƒh
	BOOL PreCreateWindow(CREATESTRUCT& cs);
										// ƒEƒBƒ“ƒhƒEì¬€”õ
	void GetMessageString(UINT nID, CString& rMessage) const;
										// ƒƒbƒZ[ƒW•¶Žš—ñ’ñ‹Ÿ

	// WMƒƒbƒZ[ƒW
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// ƒEƒBƒ“ƒhƒEì¬
	afx_msg void OnClose();
										// ƒEƒBƒ“ƒhƒEƒNƒ[ƒY
	afx_msg void OnDestroy();
										// ƒEƒBƒ“ƒhƒEíœ
	afx_msg void OnMove(int x, int y);
										// ƒEƒBƒ“ƒhƒEˆÚ“®
	afx_msg LRESULT OnDisplayChange(UINT uParam, LONG lParam);
										// ƒfƒBƒXƒvƒŒƒC•ÏX
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
										// ƒEƒBƒ“ƒhƒE”wŒi•`‰æ
	afx_msg void OnPaint();
										// ƒEƒBƒ“ƒhƒE•`‰æ
	afx_msg void OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized);
										// ƒAƒNƒeƒBƒx[ƒg
#if _MFC_VER >= 0x700
	afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
#else
	afx_msg void OnActivateApp(BOOL bActive, HTASK hTask);
#endif
										// ƒ^ƒXƒNØ‚è‘Ö‚¦
	afx_msg void OnEnterMenuLoop(BOOL bTrackPopup);
										// ƒƒjƒ…[ƒ‹[ƒvŠJŽn
	afx_msg void OnExitMenuLoop(BOOL bTrackPopup);
										// ƒƒjƒ…[ƒ‹[ƒvI—¹
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
										// eƒEƒBƒ“ƒhƒE’Ê’m
	afx_msg LONG OnKick(UINT uParam, LONG lParam);
										// ƒLƒbƒN
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS);
										// ƒI[ƒi[ƒhƒ[
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint pos);
										// ƒRƒ“ƒeƒLƒXƒgƒƒjƒ…[
	afx_msg LONG OnPowerBroadCast(UINT uParam, LONG lParam);
										// “dŒ¹•ÏX’Ê’m
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
										// ƒVƒXƒeƒ€ƒRƒ}ƒ“ƒh
#if _MFC_VER >= 0x700
	afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
#else
	afx_msg LONG OnCopyData(UINT uParam, LONG lParam);
										// ƒf[ƒ^“]‘—
#endif
	afx_msg void OnEndSession(BOOL bEnding);
										// ƒZƒbƒVƒ‡ƒ“I—¹
	afx_msg LONG OnShellNotify(UINT uParam, LONG lParam);
										// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ó‘Ô•Ï‰»

	// ƒRƒ}ƒ“ƒhˆ—
	afx_msg void OnOpen();

	afx_msg void OnFastOpen();
										// ŠJ‚­
	afx_msg void OnOpenUI(CCmdUI *pCmdUI);
										// ŠJ‚­ UI
	afx_msg void OnSave();
										// ã‘‚«•Û‘¶
	afx_msg void OnSaveUI(CCmdUI *pCmdUI);
										// ã‘‚«•Û‘¶ UI
	afx_msg void OnSaveAs();
										// –¼‘O‚ð•t‚¯‚Ä•Û‘¶
	afx_msg void OnSaveAsUI(CCmdUI *pCmdUI);
										// –¼‘O‚ð•t‚¯‚Ä•Û‘¶ UI
	afx_msg void OnMRU(UINT uID);
										// MRU
	afx_msg void OnMRUUI(CCmdUI *pCmdUI);
										// MRU UI
	afx_msg void OnReset();
	afx_msg void OnResetNuevo();
										// ƒŠƒZƒbƒg
	afx_msg void OnResetUI(CCmdUI *pCmdUI);
										// ƒŠƒZƒbƒg UI

	afx_msg void OnScc(); // Configuracion personalizada
	
	afx_msg void OnSccUI(CCmdUI* pCmdUI);

	afx_msg void OnSgc(); // Configuracion global
	
	afx_msg void OnSgcUI(CCmdUI* pCmdUI);

	afx_msg void OnSgcr(); // Configuracion global y reiniciar

	afx_msg void OnSgcrUI(CCmdUI* pCmdUI);

	afx_msg void OnInterrupt();
										// ƒCƒ“ƒ^ƒ‰ƒvƒg
	afx_msg void OnInterruptUI(CCmdUI *pCmdUI);
										// ƒCƒ“ƒ^ƒ‰ƒvƒg UI
	afx_msg void OnPower();
										// “dŒ¹ƒXƒCƒbƒ`
	afx_msg void OnPowerUI(CCmdUI *pCmdUI);
										// “dŒ¹ƒXƒCƒbƒ` UI
	afx_msg void OnExit();
										// I—¹

	afx_msg void OnFD(UINT uID);
										// ƒtƒƒbƒs[ƒfƒBƒXƒNƒRƒ}ƒ“ƒh
	afx_msg void OnFDOpenUI(CCmdUI *pCmdUI);
										// ƒtƒƒbƒs[ƒI[ƒvƒ“ UI
	afx_msg void OnFDEjectUI(CCmdUI *pCmdUI);
										// ƒtƒƒbƒs[ƒCƒWƒFƒNƒg UI
	afx_msg void OnFDWritePUI(CCmdUI *pCmdUI);
										// ƒtƒƒbƒs[‘‚«ž‚Ý•ÛŒì UI
	afx_msg void OnFDForceUI(CCmdUI *pCmdUI);
										// ƒtƒƒbƒs[‹­§ƒCƒWƒFƒNƒg UI
	afx_msg void OnFDInvalidUI(CCmdUI *pCmdUI);
										// ƒtƒƒbƒs[Œë‘}“ü UI
	afx_msg void OnFDMediaUI(CCmdUI *pCmdUI);
										// ƒtƒƒbƒs[ƒƒfƒBƒA UI
	afx_msg void OnFDMRUUI(CCmdUI *pCmdUI);
										// ƒtƒƒbƒs[MRU UI

	afx_msg void OnMOOpen();
										// MOƒI[ƒvƒ“
	afx_msg void OnMOOpenUI(CCmdUI *pCmdUI);
										// MOƒI[ƒvƒ“ UI
	afx_msg void OnMOEject();
										// MOƒCƒWƒFƒNƒg
	afx_msg void OnMOEjectUI(CCmdUI *pCmdUI);
										// MOƒCƒWƒFƒNƒg UI
	afx_msg void OnMOWriteP();
										// MO‘‚«ž‚Ý•ÛŒì
	afx_msg void OnMOWritePUI(CCmdUI *pCmdUI);
										// MO‘‚«ž‚Ý•ÛŒì UI
	afx_msg void OnMOForce();
										// MO‹­§ƒCƒWƒFƒNƒg
	afx_msg void OnMOForceUI(CCmdUI *pCmdUI);
										// MO‹­§ƒCƒWƒFƒNƒg UI
	afx_msg void OnMOMRU(UINT uID);
										// MOMRU
	afx_msg void OnMOMRUUI(CCmdUI *pCmdUI);
										// MOMRU UI

	afx_msg void OnCDOpen();
										// CDƒI[ƒvƒ“
	afx_msg void OnCDOpenUI(CCmdUI *pCmdUI);
										// CDƒI[ƒvƒ“ UI
	afx_msg void OnCDEject();
										// CDƒCƒWƒFƒNƒg
	afx_msg void OnCDEjectUI(CCmdUI *pCmdUI);
										// CDƒCƒWƒFƒNƒg UI
	afx_msg void OnCDForce();
										// CD‹­§ƒCƒWƒFƒNƒg
	afx_msg void OnCDForceUI(CCmdUI *pCmdUI);
										// CD‹­§ƒCƒWƒFƒNƒg UI
	afx_msg void OnCDMRU(UINT nID);
										// CDMRU
	afx_msg void OnCDMRUUI(CCmdUI *pCmdUI);
										// CDMRU UI

	afx_msg void OnLog();
										// ƒƒO
	afx_msg void OnLogUI(CCmdUI *pCmdUI);
										// ƒƒO UI
	afx_msg void OnScheduler();
										// ƒXƒPƒWƒ…[ƒ‰
	afx_msg void OnSchedulerUI(CCmdUI *pCmdUI);
										// ƒXƒPƒWƒ…[ƒ‰ UI
	afx_msg void OnDevice();
										// ƒfƒoƒCƒX
	afx_msg void OnDeviceUI(CCmdUI *pCmdUI);
										// ƒfƒoƒCƒX UI
	afx_msg void OnCPUReg();
										// CPUƒŒƒWƒXƒ^
	afx_msg void OnCPURegUI(CCmdUI *pCmdUI);
										// CPUƒŒƒWƒXƒ^ UI
	afx_msg void OnInt();
										// Š„‚èž‚Ý
	afx_msg void OnIntUI(CCmdUI *pCmdUI);
										// Š„‚èž‚Ý UI
	afx_msg void OnDisasm();
										// ‹tƒAƒZƒ“ƒuƒ‹
	afx_msg void OnDisasmUI(CCmdUI *pCmdUI);
										// ‹tƒAƒZƒ“ƒuƒ‹ UI
	afx_msg void OnMemory();
										// ƒƒ‚ƒŠ
	afx_msg void OnMemoryUI(CCmdUI *pCmdUI);
										// ƒƒ‚ƒŠ UI
	afx_msg void OnBreakP();
										// ƒuƒŒ[ƒNƒ|ƒCƒ“ƒg
	afx_msg void OnBreakPUI(CCmdUI *pCmdUI);
										// ƒuƒŒ[ƒNƒ|ƒCƒ“ƒg UI
	afx_msg void OnMFP();
										// MFP
	afx_msg void OnMFPUI(CCmdUI *pCmdUI);
										// MFP UI
	afx_msg void OnDMAC();
										// DMAC
	afx_msg void OnDMACUI(CCmdUI *pCmdUI);
										// DMAC UI
	afx_msg void OnCRTC();
										// CRTC
	afx_msg void OnCRTCUI(CCmdUI *pCmdUI);
										// CRTC UI
	afx_msg void OnVC();
										// VC
	afx_msg void OnVCUI(CCmdUI *pCmdUI);
										// VC UI
	afx_msg void OnRTC();
										// RTC
	afx_msg void OnRTCUI(CCmdUI *pCmdUI);
										// RTC UI
	afx_msg void OnOPM();
										// OPM
	afx_msg void OnOPMUI(CCmdUI *pCmdUI);
										// OPM UI
	afx_msg void OnKeyboard();
										// ƒL[ƒ{[ƒh
	afx_msg void OnKeyboardUI(CCmdUI *pCmdUI);
										// ƒL[ƒ{[ƒh UI
	afx_msg void OnFDD();
										// FDD
	afx_msg void OnFDDUI(CCmdUI *pCmdUI);
										// FDD UI
	afx_msg void OnFDC();
										// FDC
	afx_msg void OnFDCUI(CCmdUI *pCmdUI);
										// FDC UI
	afx_msg void OnSCC();
										// SCC
	afx_msg void OnSCCUI(CCmdUI *pCmdUI);
										// SCC UI
	afx_msg void OnCynthia();
										// CYNTHIA
	afx_msg void OnCynthiaUI(CCmdUI *pCmdUI);
										// CYNTHIA UI
	afx_msg void OnSASI();
										// SASI
	afx_msg void OnSASIUI(CCmdUI *pCmdUI);
										// SASI UI
	afx_msg void OnMIDI();
										// MIDI
	afx_msg void OnMIDIUI(CCmdUI *pCmdUI);
										// MIDI UI
	afx_msg void OnSCSI();
										// SCSI
	afx_msg void OnSCSIUI(CCmdUI *pCmdUI);
										// SCSI UI
	afx_msg void OnTVRAM();
										// ƒeƒLƒXƒg‰æ–Ê
	afx_msg void OnTVRAMUI(CCmdUI *pCmdUI);
										// ƒeƒLƒXƒg‰æ–Ê UI
	afx_msg void OnG1024();
										// ƒOƒ‰ƒtƒBƒbƒN‰æ–Ê1024~1024
	afx_msg void OnG1024UI(CCmdUI *pCmdUI);
										// ƒOƒ‰ƒtƒBƒbƒN‰æ–Ê1024~1024 UI
	afx_msg void OnG16(UINT uID);
										// ƒOƒ‰ƒtƒBƒbƒN‰æ–Ê16F
	afx_msg void OnG16UI(CCmdUI *pCmdUI);
										// ƒOƒ‰ƒtƒBƒbƒN‰æ–Ê16F UI
	afx_msg void OnG256(UINT uID);
										// ƒOƒ‰ƒtƒBƒbƒN‰æ–Ê256F
	afx_msg void OnG256UI(CCmdUI *pCmdUI);
										// ƒOƒ‰ƒtƒBƒbƒN‰æ–Ê256F UI
	afx_msg void OnG64K();
										// ƒOƒ‰ƒtƒBƒbƒN‰æ–Ê65536F
	afx_msg void OnG64KUI(CCmdUI *pCmdUI);
										// ƒOƒ‰ƒtƒBƒbƒN‰æ–Ê65536F UI
	afx_msg void OnPCG();
										// PCG
	afx_msg void OnPCGUI(CCmdUI *pCmdUI);
										// PCG UI
	afx_msg void OnBG(UINT uID);
										// BG‰æ–Ê
	afx_msg void OnBGUI(CCmdUI *pCmdUI);
										// BG‰æ–Ê UI
	afx_msg void OnPalet();
										// ƒpƒŒƒbƒg
	afx_msg void OnPaletUI(CCmdUI *pCmdUI);
										// ƒpƒŒƒbƒg UI
	afx_msg void OnTextBuf();
										// ƒeƒLƒXƒgƒoƒbƒtƒ@
	afx_msg void OnTextBufUI(CCmdUI *pCmdUI);
										// ƒeƒLƒXƒgƒoƒbƒtƒ@ UI
	afx_msg void OnGrpBuf(UINT uID);
										// ƒOƒ‰ƒtƒBƒbƒNƒoƒbƒtƒ@
	afx_msg void OnGrpBufUI(CCmdUI *pCmdUI);
										// ƒOƒ‰ƒtƒBƒbƒNƒoƒbƒtƒ@ UI
	afx_msg void OnPCGBuf();
										// PCGƒoƒbƒtƒ@
	afx_msg void OnPCGBufUI(CCmdUI *pCmdUI);
										// PCGƒoƒbƒtƒ@ UI
	afx_msg void OnBGSpBuf();
										// BG/ƒXƒvƒ‰ƒCƒgƒoƒbƒtƒ@
	afx_msg void OnBGSpBufUI(CCmdUI *pCmdUI);
										// BG/ƒXƒvƒ‰ƒCƒgƒoƒbƒtƒ@ UI
	afx_msg void OnPaletBuf();
										// ƒpƒŒƒbƒgƒoƒbƒtƒ@
	afx_msg void OnPaletBufUI(CCmdUI *pCmdUI);
										// ƒpƒŒƒbƒgƒoƒbƒtƒ@ UI
	afx_msg void OnMixBuf();
										// ‡¬ƒoƒbƒtƒ@
	afx_msg void OnMixBufUI(CCmdUI *pCmdUI);
										// ‡¬ƒoƒbƒtƒ@ UI
	afx_msg void OnComponent();
										// ƒRƒ“ƒ|[ƒlƒ“ƒg
	afx_msg void OnComponentUI(CCmdUI *pCmdUI);
										// ƒRƒ“ƒ|[ƒlƒ“ƒg UI
	afx_msg void OnOSInfo();
										// OSî•ñ
	afx_msg void OnOSInfoUI(CCmdUI *pCmdUI);
										// OSî•ñ UI
	afx_msg void OnSound();
										// ƒTƒEƒ“ƒh
	afx_msg void OnSoundUI(CCmdUI *pCmdUI);
										// ƒTƒEƒ“ƒh UI
	afx_msg void OnInput();
										// ƒCƒ“ƒvƒbƒg
	afx_msg void OnInputUI(CCmdUI *pCmdUI);
										// ƒCƒ“ƒvƒbƒg UI
	afx_msg void OnPort();
										// ƒ|[ƒg
	afx_msg void OnPortUI(CCmdUI *pCmdUI);
										// ƒ|[ƒg UI
	afx_msg void OnBitmap();
										// ƒrƒbƒgƒ}ƒbƒv
	afx_msg void OnBitmapUI(CCmdUI *pCmdUI);
										// ƒrƒbƒgƒ}ƒbƒv UI
	afx_msg void OnMIDIDrv();
										// MIDIƒhƒ‰ƒCƒo
	afx_msg void OnMIDIDrvUI(CCmdUI *pCmdUI);
										// MIDIƒhƒ‰ƒCƒo UI
	afx_msg void OnCaption();
										// ƒLƒƒƒvƒVƒ‡ƒ“
	afx_msg void OnCaptionUI(CCmdUI *pCmdUI);
										// ƒLƒƒƒvƒVƒ‡ƒ“ UI
	afx_msg void OnMenu();
										// ƒƒjƒ…[ƒo[
	afx_msg void OnMenuUI(CCmdUI *pCmdUI);
										// ƒƒjƒ…[ƒo[ UI
	afx_msg void OnStatus();
										// ƒXƒe[ƒ^ƒXƒo[
	afx_msg void OnStatusUI(CCmdUI *pCmdUI);
										// ƒXƒe[ƒ^ƒXƒo[ UI
	afx_msg void OnRefresh();
										// ƒŠƒtƒŒƒbƒVƒ…
	afx_msg void OnStretch();
										// Šg‘å
	afx_msg void OnStretchUI(CCmdUI *pCmdUI);
										// Šg‘å UI
	afx_msg void OnFullScreen();
										// ƒtƒ‹ƒXƒNƒŠ[ƒ“
	afx_msg void OnFullScreenUI(CCmdUI *pCmdUI);
										// ƒtƒ‹ƒXƒNƒŠ[ƒ“UI

	afx_msg void OnExec();
										// ŽÀs
	afx_msg void OnExecUI(CCmdUI *pCmdUI);
										// ŽÀs UI
	afx_msg void OnBreak();
										// ’âŽ~
	afx_msg void OnBreakUI(CCmdUI *pCmdUI);
										// ’âŽ~ UI
	afx_msg void OnTrace();
										// ƒgƒŒ[ƒX
	afx_msg void OnTraceUI(CCmdUI *pCmdUI);
										// ƒgƒŒ[ƒX UI

	afx_msg void OnMouseMode();
										// ƒ}ƒEƒXƒ‚[ƒh
	afx_msg void OnSoftKey();
										// ƒ\ƒtƒgƒEƒFƒAƒL[ƒ{[ƒh
	afx_msg void OnSoftKeyUI(CCmdUI *pCmdUI);
										// ƒ\ƒtƒgƒEƒFƒAƒL[ƒ{[ƒh UI
	afx_msg void OnTimeAdj();
										// Žž‡‚í‚¹
	afx_msg void OnTrap();
										// trap#0
	afx_msg void OnTrapUI(CCmdUI *pCmdUI);
										// trap#0 UI
	afx_msg void OnSaveWav();
										// WAVƒLƒƒƒvƒ`ƒƒ
	afx_msg void OnSaveWavUI(CCmdUI *pCmdUI);
										// WAVƒLƒƒƒvƒ`ƒƒ UI
	afx_msg void OnNewFD();
										// V‚µ‚¢ƒtƒƒbƒs[ƒfƒBƒXƒN
	afx_msg void OnNewDisk(UINT uID);
										// V‚µ‚¢‘å—e—ÊƒfƒBƒXƒN
	afx_msg void OnOptions();
										// ƒIƒvƒVƒ‡ƒ“

	afx_msg void OnCascade();
										// d‚Ë‚Ä•\Ž¦
	afx_msg void OnCascadeUI(CCmdUI *pCmdUI);
										// d‚Ë‚Ä•\Ž¦ UI
	afx_msg void OnTile();
										// •À‚×‚Ä•\Ž¦
	afx_msg void OnTileUI(CCmdUI *pCmdUI);
										// •À‚×‚Ä•\Ž¦ UI
	afx_msg void OnIconic();
										// ‘S‚ÄƒAƒCƒRƒ“‰»
	afx_msg void OnIconicUI(CCmdUI *pCmdUI);
										// ‘S‚ÄƒAƒCƒRƒ“‰» UI
	afx_msg void OnArrangeIcon();
										// ƒAƒCƒRƒ“‚Ì®—ñ
	afx_msg void OnArrangeIconUI(CCmdUI *pCmdUI);
										// ƒAƒCƒRƒ“‚Ì®—ñ UI
	afx_msg void OnHide();
										// ‘S‚Ä‰B‚·
	afx_msg void OnHideUI(CCmdUI *pCmdUI);
										// ‘S‚Ä‰B‚· UI
	afx_msg void OnRestore();
										// ‘S‚Ä•œŒ³
	afx_msg void OnRestoreUI(CCmdUI *pCmdUI);
										// ‘S‚Ä•œŒ³ UI
	afx_msg void OnWindow(UINT uID);
										// ƒEƒBƒ“ƒhƒE‘I‘ð
	afx_msg void OnAbout();
										// ƒo[ƒWƒ‡ƒ“î•ñ

private:
	// ‰Šú‰»
	BOOL FASTCALL InitChild();
										// ƒ`ƒƒƒCƒ‹ƒhƒEƒBƒ“ƒhƒE‰Šú‰»
	void FASTCALL InitPos(BOOL bStart = TRUE);
										// ˆÊ’uE‹éŒ`‰Šú‰»
	void FASTCALL InitShell();
										// ƒVƒFƒ‹˜AŒg‰Šú‰»
	BOOL FASTCALL InitVM();
										// VM‰Šú‰»
	BOOL FASTCALL InitComponent();
										// ƒRƒ“ƒ|[ƒlƒ“ƒg‰Šú‰»
	void FASTCALL InitVer();
										// ƒo[ƒWƒ‡ƒ“‰Šú‰»
	void FASTCALL InitCmd(LPCTSTR lpszCmd);
										// ƒRƒ}ƒ“ƒhƒ‰ƒCƒ“ˆ—

	void FASTCALL ReadFile(LPCTSTR pszFileName, CString& str);
	CString FASTCALL CFrmWnd::ProcesarM3u(CString str);
	
	void FASTCALL ApplyCfg();
										// Ý’è“K—p
	void FASTCALL SizeStatus();
										// ƒXƒe[ƒ^ƒXƒo[ƒTƒCƒY•ÏX
	void FASTCALL HideTaskBar(BOOL bHide, BOOL bFore);
										// ƒ^ƒXƒNƒo[‰B‚·
	BOOL RestoreFrameWnd(BOOL bFullScreen);
										// ƒEƒBƒ“ƒhƒE•œŒ³
	void RestoreDiskState();
										// ƒfƒBƒXƒNEƒXƒe[ƒg•œŒ³
	int m_nStatus;
										// ƒXƒe[ƒ^ƒXƒR[ƒh
	static const DWORD SigTable[];
										// SRAMƒVƒOƒlƒ`ƒƒƒe[ƒuƒ‹

	
										// ƒfƒBƒXƒNEƒXƒe[ƒg•Û‘¶
	void FASTCALL CleanSub();
										// ƒNƒŠ[ƒ“ƒAƒbƒv
	BOOL m_bExit;
										// I—¹ƒtƒ‰ƒO
	BOOL m_bSaved;
										// ƒtƒŒ[ƒ€EƒfƒBƒXƒNEƒXƒe[ƒg•Û‘¶ƒtƒ‰ƒO

	// ƒZ[ƒuEƒ[ƒh
	BOOL FASTCALL SaveComponent(const Filepath& path, DWORD dwPos);
										// ƒZ[ƒu
	BOOL FASTCALL LoadComponent(const Filepath& path, DWORD dwPos);
										// ƒ[ƒh

	// ƒRƒ}ƒ“ƒhƒnƒ“ƒhƒ‰ƒTƒu
	BOOL FASTCALL OnOpenSub(const Filepath& path);
										// ƒI[ƒvƒ“ƒTƒu
	BOOL FASTCALL OnOpenPrep(const Filepath& path, BOOL bWarning = TRUE);
										// ƒI[ƒvƒ“ƒ`ƒFƒbƒN
	void FASTCALL OnSaveSub(const Filepath& path);
										// •Û‘¶ƒTƒu
	void FASTCALL OnFDOpen(int nDrive);
										// ƒtƒƒbƒs[ƒI[ƒvƒ“
	void FASTCALL OnFDEject(int nDrive);
										// ƒtƒƒbƒs[ƒCƒWƒFƒNƒg
	void FASTCALL OnFDWriteP(int nDrive);
										// ƒtƒƒbƒs[‘‚«ž‚Ý•ÛŒì
	void FASTCALL OnFDForce(int nDrive);
										// ƒtƒƒbƒs[‹­§ƒCƒWƒFƒNƒg
	void FASTCALL OnFDInvalid(int nDrive);
										// ƒtƒƒbƒs[Œë‘}“ü
	void FASTCALL OnFDMedia(int nDrive, int nMedia);
										// ƒtƒƒbƒs[ƒƒfƒBƒA
	void FASTCALL OnFDMRU(int nDrive, int nMRU);
										// ƒtƒƒbƒs[MRU
	int m_nFDDStatus[2];
										// ƒtƒƒbƒs[ƒXƒe[ƒ^ƒX

	// ƒfƒoƒCƒXEƒrƒ…[EƒRƒ“ƒ|[ƒlƒ“ƒg
	FDD *m_pFDD;
										// FDD
	SASI *m_pSASI;
										// SASI
	SCSI *m_pSCSI;
										// SCSI
	Scheduler *m_pScheduler;
										// Scheduler
	Keyboard *m_pKeyboard;
										// Keyboard
	Mouse *m_pMouse;
										// Mouse
	CDrawView *m_pDrawView;
										// •`‰æƒrƒ…[
	CComponent *m_pFirstComponent;
										// Å‰‚ÌƒRƒ“ƒ|[ƒlƒ“ƒg
	CScheduler *m_pSch;
										// ƒXƒPƒWƒ…[ƒ‰
	CSound *m_pSound;
										// ƒTƒEƒ“ƒh
	CInput *m_pInput;
										// ƒCƒ“ƒvƒbƒg
	CPort *m_pPort;
										// ƒ|[ƒg
	CMIDI *m_pMIDI;
										// MIDI
	CTKey *m_pTKey;
										// TrueKey
	CHost *m_pHost;
										// Host
	CInfo *m_pInfo;
										// Info
	CConfig *m_pConfig;
										// ƒRƒ“ƒtƒBƒO

	// ƒtƒ‹ƒXƒNƒŠ[ƒ“
	
										// ƒtƒ‹ƒXƒNƒŠ[ƒ“ƒtƒ‰ƒO
	DEVMODE m_DevMode;
										// ƒXƒNƒŠ[ƒ“ƒpƒ‰ƒ[ƒ^‹L‰¯
	HWND m_hTaskBar;
										// ƒ^ƒXƒNƒo[
	int m_nWndLeft;
										// ƒEƒBƒ“ƒhƒEƒ‚[ƒhŽžx
	int m_nWndTop;
										// ƒEƒBƒ“ƒhƒEƒ‚[ƒhŽžy

	// ƒTƒuƒEƒBƒ“ƒhƒE
	CString m_strWndClsName;
										// ƒEƒBƒ“ƒhƒEƒNƒ‰ƒX–¼

	// ƒXƒe[ƒ^ƒXƒrƒ…[
	void FASTCALL CreateStatusView();
										// ƒXƒe[ƒ^ƒXƒrƒ…[ì¬
	void FASTCALL DestroyStatusView();
										// ƒXƒe[ƒ^ƒXƒrƒ…[íœ
	CStatusView *m_pStatusView;
										// ƒXƒe[ƒ^ƒXƒrƒ…[

	// ƒXƒe[ƒ^ƒXƒo[
	void FASTCALL ShowStatus();
										// ƒXƒe[ƒ^ƒXƒo[•\Ž¦
	void FASTCALL ResetStatus();
										// ƒXƒe[ƒ^ƒXƒo[ƒŠƒZƒbƒg
	CStatusBar m_StatusBar;
										// ƒXƒe[ƒ^ƒXƒo[
	BOOL m_bStatusBar;
										// ƒXƒe[ƒ^ƒXƒo[•\Ž¦ƒtƒ‰ƒO

	// ƒƒjƒ…[
	void FASTCALL ShowMenu();
										// ƒƒjƒ…[ƒo[•\Ž¦
	CMenu m_Menu;
										// ƒƒCƒ“ƒƒjƒ…[
	BOOL m_bMenuBar;
										// ƒƒjƒ…[ƒo[•\Ž¦ƒtƒ‰ƒO
	CMenu m_PopupMenu;
										// ƒ|ƒbƒvƒAƒbƒvƒƒjƒ…[
	BOOL m_bPopupMenu;
										// ƒ|ƒbƒvƒAƒbƒvƒƒjƒ…[ŽÀs’†

	// ƒLƒƒƒvƒVƒ‡ƒ“
	void FASTCALL ShowCaption();
										// ƒLƒƒƒvƒVƒ‡ƒ“•\Ž¦
	void FASTCALL ResetCaption();
										// ƒLƒƒƒvƒVƒ‡ƒ“ƒŠƒZƒbƒg
	BOOL m_bCaption;
										// ƒLƒƒƒvƒVƒ‡ƒ“•\Ž¦ƒtƒ‰ƒO

	// î•ñ
	void FASTCALL SetInfo(CString& strInfo);
										// î•ñ•¶Žš—ñƒZƒbƒg

	// ƒVƒFƒ‹˜AŒg
	ULONG m_uNotifyId;
										// ƒVƒFƒ‹’Ê’mID
	SHChangeNotifyEntry m_fsne[1];
										// ƒVƒFƒ‹’Ê’mƒGƒ“ƒgƒŠ

	// ƒXƒe[ƒgƒtƒ@ƒCƒ‹
	void FASTCALL UpdateExec();
										// XV(ŽÀs)
	DWORD m_dwExec;
										// ƒZ[ƒuŒãŽÀsƒJƒEƒ“ƒ^

	// ƒRƒ“ƒtƒBƒMƒ…ƒŒ[ƒVƒ‡ƒ“
	BOOL m_bMouseMid;
										// ƒ}ƒEƒX’†ƒ{ƒ^ƒ“—LŒø
	BOOL m_bPopup;
										// ƒ|ƒbƒvƒAƒbƒvƒ‚[ƒh
	BOOL m_bAutoMouse;
										// Ž©“®ƒ}ƒEƒXƒ‚[ƒh

	DECLARE_MESSAGE_MAP()
										// ƒƒbƒZ[ƒW ƒ}ƒbƒv‚ ‚è
};

#endif	// mfc_frm_h
#endif	// _WIN32
