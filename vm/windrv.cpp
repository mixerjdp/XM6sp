//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 ‚o‚hD(ytanaka@ipc-tokai.or.jp)
//	Modified (C) 2006 co (cogood—gmail.com)
//	[ Windrv ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "memory.h"
#include "cpu.h"
#include "config.h"
#include "windrv.h"

//===========================================================================
//
//	Human68k
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒpƒXæ“¾
//
//---------------------------------------------------------------------------
void FASTCALL Human68k::namests_t::GetCopyPath(BYTE* szPath) const
{
	ASSERT(this);
	ASSERT(szPath);
	// WARNING: Unicode‘Î‰—vC³

	BYTE* p = szPath;
	for (int i = 0; i < 65; i++) {
		BYTE c = path[i];
		if (c == '\0') break;
		if (c == 0x09) {
			c = '/';
		}
		*p++ = c;
	}

	*p = '\0';
}

//---------------------------------------------------------------------------
//
//	ƒtƒ@ƒCƒ‹–¼æ“¾
//
//	TwentyOne‚Å¶¬‚µ‚½ƒGƒ“ƒgƒŠ‚Æ“¯“™‚Ìƒtƒ@ƒCƒ‹–¼ÄŒ»”\—Í‚ğ‚½‚¹‚éB
//
//	‰¼‘zƒfƒBƒŒƒNƒgƒŠƒGƒ“ƒgƒŠ‹@”\‚ª–³Œø‚Èê‡:
//	Human68k“à•”‚ÅƒƒCƒ‹ƒhƒJ[ƒh•¶š‚ª‚·‚×‚Ä'?'‚É“WŠJ‚³‚ê‚Ä‚µ‚Ü‚¤‚½
//	‚ßA18 + 3•¶š‚æ‚è’·‚¢ƒtƒ@ƒCƒ‹–¼‚ÌŒŸõ‚ª•s‰Â”\‚Æ‚È‚éB‚±‚Ì‚æ‚¤‚È
//	ê‡‚ÍAƒƒCƒ‹ƒhƒJ[ƒh•¶š•ÏŠ·‚ğs‚È‚¤‚±‚Æ‚ÅAFILES‚É‚æ‚é‘Sƒtƒ@
//	ƒCƒ‹‚ÌŒŸõ‚ğ‰Â”\‚Æ‚È‚éB‚È‚¨AŒŸõ‚ª‰Â”\‚É‚È‚é‚¾‚¯‚ÅƒI[ƒvƒ“‚Í‚Å
//	‚«‚È‚¢‚Ì‚Å’ˆÓB
//
//	‚·‚×‚Ä‚Ìƒtƒ@ƒCƒ‹‚ğƒI[ƒvƒ“‚µ‚½‚¢ê‡‚ÍƒfƒBƒŒƒNƒgƒŠƒGƒ“ƒgƒŠƒLƒƒƒb
//	ƒVƒ…‚ğ—LŒø‚É‚·‚é‚±‚ÆBƒtƒ@ƒCƒ‹–¼•ÏŠ·‚ª•s—v‚É‚È‚è‘f’¼‚É“®ì‚·‚éB
//
//---------------------------------------------------------------------------
void FASTCALL Human68k::namests_t::GetCopyFilename(BYTE* szFilename) const
{
	ASSERT(this);
	ASSERT(szFilename);
	// WARNING: Unicode‘Î‰—vC³

	int i;
	BYTE* p = szFilename;
#ifdef XM6_WINDRV_WILDCARD
	BYTE* pWild = NULL;
#endif // XM6_WINDRV_WILDCARD

	// ƒtƒ@ƒCƒ‹–¼–{‘Ì“]‘—
	for (i = 0; i < 8; i++) {
		BYTE c = name[i];
		if (c == ' ') {
			BOOL bTerminate = TRUE;
			// ƒtƒ@ƒCƒ‹–¼’†‚ÉƒXƒy[ƒX‚ªoŒ»‚µ‚½ê‡AˆÈ~‚ÌƒGƒ“ƒgƒŠ‚ª‘±‚¢‚Ä‚¢‚é‚©‚Ç‚¤‚©Šm”F
			// add[0] ‚ª—LŒø‚È•¶š‚È‚ç‘±‚¯‚é
			if (add[0] != '\0') {
				bTerminate = FALSE;
			} else {
				// name[i] ‚æ‚èŒã‚É‹ó”’ˆÈŠO‚Ì•¶š‚ª‘¶İ‚·‚é‚È‚ç‘±‚¯‚é
				for (int j = i + 1; j < 8; j++) {
					if (name[j] != ' ') {
						bTerminate = FALSE;
						break;
					}
				}
			}
			// ƒtƒ@ƒCƒ‹–¼I’[‚È‚ç“]‘—I—¹
			if (bTerminate) break;
		}
#ifdef XM6_WINDRV_WILDCARD
		if (c == '?') {
			if (pWild == NULL) pWild = p;
		} else {
			pWild = NULL;
		}
#endif // XM6_WINDRV_WILDCARD
		*p++ = c;
	}
	// ‘S‚Ä‚Ì•¶š‚ğ“Ç‚İ‚Ş‚ÆA‚±‚±‚Å i >= 8 ‚Æ‚È‚é

	// ƒtƒ@ƒCƒ‹–¼–{‘Ì‚ª8•¶šˆÈã‚È‚ç’Ç‰Á•”•ª‚à‰Á‚¦‚é
	if (i >= 8) {
		// ƒtƒ@ƒCƒ‹–¼’Ç‰Á•”•ª“]‘—
		for (i = 0; i < 10; i++) {
			BYTE c = add[i];
			if (c == '\0') break;
#ifdef XM6_WINDRV_WILDCARD
			if (c == '?') {
				if (pWild == NULL) pWild = p;
			} else {
				pWild = NULL;
			}
#endif // XM6_WINDRV_WILDCARD
			*p++ = c;
		}
		// ‘S‚Ä‚Ì•¶š‚ğ“Ç‚İ‚Ş‚ÆA‚±‚±‚Å i >= 10 ‚Æ‚È‚é
	}

#ifdef XM6_WINDRV_WILDCARD
	// ƒtƒ@ƒCƒ‹–¼––”ö‚ªƒƒCƒ‹ƒhƒJ[ƒh‚ÅA‚©‚ÂŒã”¼10•¶š‚Ì‚·‚×‚Ä‚ğg—p‚µ‚Ä‚¢‚é‚©H
	if (pWild && i >= 10) {
		p = pWild;
		*p++ = '*';
	}
#endif // XM6_WINDRV_WILDCARD

	// ƒtƒ@ƒCƒ‹–¼–{‘Ì‚ÍI—¹AŠg’£q‚Ìˆ—ŠJn
#ifdef XM6_WINDRV_WILDCARD
	pWild = NULL;
#endif // XM6_WINDRV_WILDCARD
	i = 0;

	// Šg’£q‚ª‘¶İ‚·‚é‚©H
	if (ext[0] == ' ' && ext[1] == ' ' && ext[2] == ' ') {
		// Šg’£q‚È‚µ
	} else {
		// Šg’£q“]‘—
		*p++ = '.';
		for (i = 0; i < 3; i++) {
			BYTE c = ext[i];
			if (c == ' ') {
				BOOL bTerminate = TRUE;
				// Šg’£q’†‚ÉƒXƒy[ƒX‚ªoŒ»‚µ‚½ê‡AˆÈ~‚ÌƒGƒ“ƒgƒŠ‚ª‘±‚¢‚Ä‚¢‚é‚©‚Ç‚¤‚©Šm”F
				// ext[i] ‚æ‚èŒã‚É‹ó”’ˆÈŠO‚Ì•¶š‚ª‘¶İ‚·‚é‚È‚ç‘±‚¯‚é
				for (int j = i + 1; j < 3; j++) {
					if (ext[j] != ' ') {
						bTerminate = FALSE;
						break;
					}
				}
				// Šg’£qI’[‚È‚ç“]‘—I—¹
				if (bTerminate) break;
			}
#ifdef XM6_WINDRV_WILDCARD
			if (c == '?') {
				if (pWild == NULL) pWild = p;
			} else {
				pWild = NULL;
			}
#endif // XM6_WINDRV_WILDCARD
			*p++ = c;
		}
		// ‘S‚Ä‚Ì•¶š‚ğ“Ç‚İ‚Ş‚ÆA‚±‚±‚Å i >= 3 ‚Æ‚È‚é
	}

#ifdef XM6_WINDRV_WILDCARD
	// Šg’£q––”ö‚ªƒƒCƒ‹ƒhƒJ[ƒh‚ÅA‚©‚ÂŠg’£q3•¶š‚·‚×‚Ä‚ğg—p‚µ‚Ä‚¢‚é‚©H
	if (pWild && i >= 3) {
		p = pWild;
		*p++ = '*';
	}
#endif // XM6_WINDRV_WILDCARD

	// ”Ô•º’Ç‰Á
	*p = '\0';
}

//===========================================================================
//
//	ƒRƒ}ƒ“ƒhƒnƒ“ƒhƒ‰
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
CWindrv::CWindrv(Windrv* pWindrv, Memory* pMemory, DWORD nHandle)
{
	windrv = pWindrv;
	memory = pMemory;
	a5 = 0;
	unit = 0;
	command = 0;

	m_nHandle = nHandle;
	m_bAlloc = FALSE;
	m_bExecute = FALSE;
	m_bReady = FALSE;
	m_bTerminate = FALSE;
	m_hThread = NULL;
	m_hEventStart = NULL;
	m_hEventReady = NULL;
}

//---------------------------------------------------------------------------
//
//	ƒfƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
CWindrv::~CWindrv()
{
	Terminate();
}

//---------------------------------------------------------------------------
//
//	ƒXƒŒƒbƒh‹N“®
//
//---------------------------------------------------------------------------
BOOL FASTCALL CWindrv::Start()
{
	ASSERT(this);
	ASSERT(m_bAlloc == FALSE);
	ASSERT(m_bExecute == FALSE);
	ASSERT(m_bTerminate == FALSE);
	ASSERT(m_hThread == NULL);
	ASSERT(m_hEventStart == NULL);
	ASSERT(m_hEventReady == NULL);

	// ƒnƒ“ƒhƒ‹Šm•Û
	m_hEventStart = ::CreateEvent(NULL, FALSE, FALSE, NULL);	// ©“®ƒŠƒZƒbƒg
	ASSERT(m_hEventStart);

	m_hEventReady = ::CreateEvent(NULL, TRUE, FALSE, NULL);	// è“®ƒŠƒZƒbƒg ’ˆÓ
	ASSERT(m_hEventReady);

	// ƒXƒŒƒbƒh¶¬
	DWORD nThread;
	m_hThread = ::CreateThread(NULL, 0, Run, this, 0, &nThread);
	ASSERT(m_hThread);

	// ƒGƒ‰[ƒ`ƒFƒbƒN
	if (m_hEventStart == NULL ||
		m_hEventReady == NULL ||
		m_hThread == NULL) return FALSE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒXƒŒƒbƒh’â~
//
//---------------------------------------------------------------------------
BOOL FASTCALL CWindrv::Terminate()
{
	ASSERT(this);

	BOOL bResult = TRUE;

	if (m_hThread) {
		ASSERT(m_bTerminate == FALSE);
		ASSERT(m_hEventStart);

		// ƒXƒŒƒbƒh‚Ö’â~—v‹
		m_bTerminate = TRUE;
		::SetEvent(m_hEventStart);

		// ƒXƒŒƒbƒhI—¹‘Ò‚¿
		DWORD nResult;
		nResult = ::WaitForSingleObject(m_hThread, 30 * 1000);	// —P—\ŠúŠÔ 30•b
		if (nResult != WAIT_OBJECT_0) {
			// ‹­§’â~
			ASSERT(FALSE);	// ”O‚Ì‚½‚ß
			::TerminateThread(m_hThread, (DWORD)-1);
			nResult = ::WaitForSingleObject(m_hThread, 100);
			bResult = FALSE;
		}

		// ƒXƒŒƒbƒhƒnƒ“ƒhƒ‹ŠJ•ú
		::CloseHandle(m_hThread);
		m_hThread = NULL;

		// ’â~—v‹‚ğæ‚è‰º‚°‚é
		m_bTerminate = FALSE;
	}

	// ƒCƒxƒ“ƒgƒnƒ“ƒhƒ‹ŠJ•ú
	if (m_hEventStart) {
		::CloseHandle(m_hEventStart);
		m_hEventStart = NULL;
	}

	if (m_hEventReady) {
		::CloseHandle(m_hEventReady);
		m_hEventReady = NULL;
	}

	// ‚»‚Ì‚Ü‚Ü“¯‚¶ƒIƒuƒWƒFƒNƒg‚ÅƒXƒŒƒbƒh‹N“®‚Å‚«‚é‚æ‚¤‰Šú‰»
	m_bAlloc = FALSE;
	m_bExecute = FALSE;
	m_bReady = FALSE;
	m_bTerminate = FALSE;

	return bResult;
}

//---------------------------------------------------------------------------
//
//	ƒRƒ}ƒ“ƒhÀs
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Execute(DWORD nA5)
{
	ASSERT(this);
	ASSERT(m_bExecute == FALSE);
	ASSERT(m_bTerminate == FALSE);
	ASSERT(m_hThread);
	ASSERT(m_hEventStart);
	ASSERT(m_hEventReady);
	ASSERT(nA5 <= 0xFFFFFF);

	// ÀsŠJnƒtƒ‰ƒOƒZƒbƒg
	m_bExecute = TRUE;

	// ƒŠƒNƒGƒXƒgƒwƒbƒ_‚ÌƒAƒhƒŒƒX
	a5 = nA5;

	// ƒGƒ‰[î•ñ‚ğƒNƒŠƒA
	SetByte(a5 + 3, 0);
	SetByte(a5 + 4, 0);

	// ƒ†ƒjƒbƒg”Ô†Šl“¾
	unit = GetByte(a5 + 1);

	// ƒRƒ}ƒ“ƒhŠl“¾
	command = GetByte(a5 + 2);

	// ƒXƒŒƒbƒh‘Ò‹@€”õ
	m_bReady = FALSE;
	::ResetEvent(m_hEventReady);

	// ÀsŠJnƒCƒxƒ“ƒgƒZƒbƒg
	::SetEvent(m_hEventStart);

	// ƒ}ƒ‹ƒ`ƒXƒŒƒbƒhŠJn—v‹‚ª—ˆ‚é‚Ü‚Å‚ÍÀ¿ƒVƒ“ƒOƒ‹ƒXƒŒƒbƒh‚Æ‚µ‚ÄU‚é•‘‚¤
	DWORD nResult = ::WaitForSingleObject(m_hEventReady, INFINITE);
	ASSERT(nResult == WAIT_OBJECT_0);	// ”O‚Ì‚½‚ß
}

#ifdef WINDRV_SUPPORT_COMPATIBLE
//---------------------------------------------------------------------------
//
//	ƒRƒ}ƒ“ƒhÀs (WINDRVŒİŠ·)
//
//---------------------------------------------------------------------------
DWORD FASTCALL CWindrv::ExecuteCompatible(DWORD nA5)
{
	ASSERT(this);
	ASSERT(nA5 <= 0xFFFFFF);

	// ƒŠƒNƒGƒXƒgƒwƒbƒ_‚ÌƒAƒhƒŒƒX
	a5 = nA5;

	// ƒGƒ‰[î•ñ‚ğƒNƒŠƒA
	SetByte(a5 + 3, 0);
	SetByte(a5 + 4, 0);

	// ƒ†ƒjƒbƒg”Ô†Šl“¾
	unit = GetByte(a5 + 1);

	// ƒRƒ}ƒ“ƒhŠl“¾
	command = GetByte(a5 + 2);

	// ƒRƒ}ƒ“ƒhÀs
	ExecuteCommand();

	return GetByte(a5 + 3) | (GetByte(a5 + 4) << 8);
}
#endif // WINDRV_SUPPORT_COMPATIBLE

//---------------------------------------------------------------------------
//
//	ƒRƒ}ƒ“ƒhŠ®—¹‚ğ‘Ò‚½‚¸‚ÉVMƒXƒŒƒbƒhÀsÄŠJ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Ready()
{
	ASSERT(this);
	ASSERT(m_hEventReady);

	m_bReady = TRUE;
	::SetEvent(m_hEventReady);
}

//---------------------------------------------------------------------------
//
//	ƒXƒŒƒbƒhÀsŠJnƒ|ƒCƒ“ƒg
//
//---------------------------------------------------------------------------
DWORD WINAPI CWindrv::Run(VOID* pThis)
{
	ASSERT(pThis);

	((CWindrv*)pThis)->Runner();

	::ExitThread(0);
	return 0;
}

//---------------------------------------------------------------------------
//
//	ƒXƒŒƒbƒhÀ‘Ì
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Runner()
{
	ASSERT(this);
	ASSERT(m_hEventStart);
	ASSERT(m_hEventReady);
	//ASSERT(m_bTerminate == FALSE);
	// ƒXƒŒƒbƒhŠJn’¼Œã‚É‚¢‚«‚È‚èTerminate‚·‚é‚ÆA‚±‚Ì“_‚Å m_bTerminate ‚ª
	// TRUE‚Æ‚È‚é‰Â”\«‚ª‚‚¢BŠÔŒo‰ß‚É‚æ‚Á‚ÄI—¹‚µAI—¹‚ÌuŠÔ‚É“¯Šú‚µ‚Ä
	// ‚¢‚é‚½‚ßASSERT‚È‚µ‚Å‚à–â‘è‚È‚¢B

	for (;;) {
		DWORD nResult = ::WaitForSingleObject(m_hEventStart, INFINITE);
		if (nResult != WAIT_OBJECT_0) {
			ASSERT(FALSE);	// ”O‚Ì‚½‚ß
			break;
		}
		if (m_bTerminate) break;

		// Às
		ExecuteCommand();

		// ÀsI—¹‘O‚É’Ê’m
		Ready();

		// ÀsŠ®—¹
		m_bExecute = FALSE;
	}

	Ready();	// ”O‚Ì‚½‚ß
}

//===========================================================================
//
//	ƒRƒ}ƒ“ƒhƒnƒ“ƒhƒ‰ŠÇ—
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	‰Šú‰»
//
//	‘SƒXƒŒƒbƒhŠm•Û‚µA‘Ò‹@ó‘Ô‚É‚·‚éB
//
//---------------------------------------------------------------------------
void FASTCALL CWindrvManager::Init(Windrv* pWindrv, Memory* pMemory)
{
	ASSERT(this);
	ASSERT(pWindrv);
	ASSERT(pMemory);

	for (int i = 0; i < WINDRV_THREAD_MAX; i++) {
		m_pThread[i] = new CWindrv(pWindrv, pMemory, i);
		m_pThread[i]->Start();
	}
}

//---------------------------------------------------------------------------
//
//	I—¹
//
//	‘S‚Ä‚ÌƒXƒŒƒbƒh‚ğ’â~‚³‚¹‚éB
//
//---------------------------------------------------------------------------
void FASTCALL CWindrvManager::Clean()
{
	ASSERT(this);

	for (int i = 0; i < WINDRV_THREAD_MAX; i++) {
		ASSERT(m_pThread[i]);	// ”O‚Ì‚½‚ß
		delete m_pThread[i];
		m_pThread[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	ƒŠƒZƒbƒg
//
//	Às’†‚ÌƒXƒŒƒbƒh‚Ì‚İÄ‹N“®‚³‚¹‚éB
//
//---------------------------------------------------------------------------
void FASTCALL CWindrvManager::Reset()
{
	ASSERT(this);

	for (int i = 0; i < WINDRV_THREAD_MAX; i++) {
		ASSERT(m_pThread[i]);	// ”O‚Ì‚½‚ß
		if (m_pThread[i]->isAlloc()) {
			m_pThread[i]->Terminate();
			m_pThread[i]->Start();
		}
	}
}

//---------------------------------------------------------------------------
//
//	‹ó‚«ƒXƒŒƒbƒhŠm•Û
//
//---------------------------------------------------------------------------
CWindrv* FASTCALL CWindrvManager::Alloc()
{
	ASSERT(this);

	for (int i = 0; i < WINDRV_THREAD_MAX; i++) {
		if (m_pThread[i]->isAlloc() == FALSE) {
			m_pThread[i]->SetAlloc(TRUE);
			return m_pThread[i];
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------
//
//	ƒXƒŒƒbƒhŒŸõ
//
//---------------------------------------------------------------------------
CWindrv* FASTCALL CWindrvManager::Search(DWORD nHandle)
{
	ASSERT(this);

	if (nHandle >= WINDRV_THREAD_MAX) return NULL;
	return m_pThread[nHandle];
}

//---------------------------------------------------------------------------
//
//	ƒXƒŒƒbƒhŠJ•ú
//
//---------------------------------------------------------------------------
void FASTCALL CWindrvManager::Free(CWindrv* p)
{
	ASSERT(this);
	ASSERT(p);

	p->SetAlloc(FALSE);
}

//===========================================================================
//
//	Windrv
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
Windrv::Windrv(VM *p) : MemDevice(p)
{
	// ƒfƒoƒCƒXID‚ğ‰Šú‰»
	dev.id = MAKEID('W', 'D', 'R', 'V');
	dev.desc = "Windrv";

	// ŠJnƒAƒhƒŒƒXAI—¹ƒAƒhƒŒƒX
	memdev.first = 0xe9e000;
	memdev.last = 0xe9ffff;
}

//---------------------------------------------------------------------------
//
//	‰Šú‰»
//
//---------------------------------------------------------------------------
BOOL FASTCALL Windrv::Init()
{
	ASSERT(this);

	// Šî–{ƒNƒ‰ƒX
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// ƒOƒ[ƒoƒ‹ƒtƒ‰ƒO‚ğ‰Šú‰»
	windrv.enable = WINDRV_MODE_NONE;

	// ƒhƒ‰ƒCƒu0Aƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€–³‚µ
	windrv.drives = 0;
	windrv.fs = NULL;

	// ƒXƒŒƒbƒh‰Šú‰»
	Memory* memory = (Memory*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(memory);
	m_cThread.Init(this, memory);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒNƒŠ[ƒ“ƒAƒbƒv
//
//---------------------------------------------------------------------------
void FASTCALL Windrv::Cleanup()
{
	ASSERT(this);

	// ƒXƒŒƒbƒh‘SŠJ•ú
	m_cThread.Clean();

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€‚ª‚ ‚ê‚ÎAƒŠƒZƒbƒg
	if (windrv.fs) {
		windrv.fs->Reset();
	}

	// Šî–{ƒNƒ‰ƒX‚Ö
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	ƒŠƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL Windrv::Reset()
{
	ASSERT(this);

	LOG(Log::Normal, "ƒŠƒZƒbƒg");

	// ƒXƒŒƒbƒh‚ªÀs’†‚Å‚ ‚ê‚ÎAƒŠƒZƒbƒg
	m_cThread.Reset();

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€‚ª‚ ‚ê‚ÎAƒŠƒZƒbƒg
	if (windrv.fs) {
		windrv.fs->Reset();
	}

	// ƒhƒ‰ƒCƒu”‰Šú‰»
	windrv.drives = 0;
}

//---------------------------------------------------------------------------
//
//	ƒZ[ƒu
//
//---------------------------------------------------------------------------
BOOL FASTCALL Windrv::Save(Fileio *fio, int ver)
{
	ASSERT(this);
	LOG(Log::Normal, "ƒZ[ƒu");
	
	printf("%d %d", fio, ver);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh
//
//---------------------------------------------------------------------------
BOOL FASTCALL Windrv::Load(Fileio *fio, int ver)
{
	ASSERT(this);
	LOG(Log::Normal, "ƒ[ƒh");
	printf("%d %d", fio, ver);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	İ’è“K—p
//
//---------------------------------------------------------------------------
void FASTCALL Windrv::ApplyCfg(const Config* pConfig)
{
	ASSERT(this);
	ASSERT(pConfig);
	LOG(Log::Normal, "İ’è“K—p");

	// “®ìƒtƒ‰ƒO‚ğİ’è
	windrv.enable = pConfig->windrv_enable;
}

//---------------------------------------------------------------------------
//
//	ƒoƒCƒg“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
DWORD FASTCALL Windrv::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// ƒ|[ƒg1
	if (addr == 0xE9F001) {
		return StatusAsynchronous();
	}

	DWORD result = ReadOnly(addr);

#ifdef WINDRV_LOG
	switch (result) {
	case 'X':
		LOG(Log::Normal, "Windrvƒ|[ƒg –¢ƒTƒ|[ƒg");
		break;
	case 'Y':
		LOG(Log::Normal, "Windrvƒ|[ƒg —˜—p‰Â”");
		break;
#ifdef WINDRV_SUPPORT_COMPATIBLE
	case 'W':
		LOG(Log::Normal, "Windrvƒ|[ƒg ŒİŠ·“®ì");
		break;
#endif // WINDRV_SUPPORT_COMPATIBLE
	}
#endif // WINDRV_LOG

	if (result == 0xFF) {
		cpu->BusErr(addr, TRUE);
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	“Ç‚İ‚İ‚Ì‚İ
//
//---------------------------------------------------------------------------
DWORD FASTCALL Windrv::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// ¯•Êƒ|[ƒgˆÈŠO‚Í-1
	if (addr != 0xE9F000) {
		return 0xFF;
	}

	switch (windrv.enable) {
	case WINDRV_MODE_DUAL:
	case WINDRV_MODE_ENABLE:
		return 'Y';			// WindrvXM

	case WINDRV_MODE_COMPATIBLE:
#ifdef WINDRV_SUPPORT_COMPATIBLE
		return 'W';			// WINDRVŒİŠ·
#endif // WINDRV_SUPPORT_COMPATIBLE

	case WINDRV_MODE_NONE:
		return 'X';			// g—p•s‰Â (I/O‹óŠÔ‚Í‘¶İ‚·‚é)
	}

	// I/O‹óŠÔ‚ª‘¶İ‚µ‚È‚¢ê‡‚Í-1
	return 0xFF;
}

//---------------------------------------------------------------------------
//
//	ƒoƒCƒg‘‚«‚İ
//
//	windrvƒ|[ƒg‚Ö‚Ì‘‚«‚İŒãAd0.w‚ÅƒŠƒUƒ‹ƒg‚ğ’Ê’m‚·‚éB
//
//	ãˆÊ8bit‚ÍŸ‚Ì’Ê‚èB
//		$1x	ƒGƒ‰[(’†~‚Ì‚İ)
//		$2x	ƒGƒ‰[(ÄÀs‚Ì‚İ)
//		$3x	ƒGƒ‰[(ÄÀs•’†~)
//		$4x	ƒGƒ‰[(–³‹‚Ì‚İ)
//		$5x	ƒGƒ‰[(–³‹•’†~)
//		$6x	ƒGƒ‰[(–³‹•ÄÀs)
//		$7x	ƒGƒ‰[(–³‹•ÄÀs•’†~)
//	‰ºˆÊ8bit‚ÍŸ‚Ì’Ê‚èB
//		$01	–³Œø‚Èƒ†ƒjƒbƒg”Ô†‚ğw’è‚µ‚½.
//		$02	ƒfƒBƒXƒN‚ª“ü‚Á‚Ä‚¢‚È‚¢.
//		$03	ƒfƒoƒCƒXƒhƒ‰ƒCƒo‚É–³Œø‚ÈƒRƒ}ƒ“ƒh‚ğw’è‚µ‚½.
//		$04	CRC ƒGƒ‰[.
//		$05	ƒfƒBƒXƒN‚ÌŠÇ——Ìˆæ‚ª”j‰ó‚³‚ê‚Ä‚¢‚é.
//		$06	ƒV[ƒNƒGƒ‰[.
//		$07	–³Œø‚ÈƒƒfƒBƒA.
//		$08	ƒZƒNƒ^‚ªŒ©‚Â‚©‚ç‚È‚¢.
//		$09	ƒvƒŠƒ“ƒ^‚ªŒq‚ª‚Á‚Ä‚¢‚È‚¢.
//		$0a	‘‚«‚İƒGƒ‰[.
//		$0b	“Ç‚İ‚İƒGƒ‰[.
//		$0c	‚»‚Ì‘¼‚ÌƒGƒ‰[.
//		$0d	ƒ‰ƒCƒgƒvƒƒeƒNƒg(ƒvƒƒeƒNƒg‚ğŠO‚µ‚Ä“¯‚¶ƒfƒBƒXƒN‚ğ“ü‚ê‚é).
//		$0e	‘‚«‚İ•s‰Â”\.
//		$0f	ƒtƒ@ƒCƒ‹‹¤—Lˆá”½.
//
//---------------------------------------------------------------------------
void FASTCALL Windrv::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// ƒTƒ|[ƒg‚Ì‚İƒ|[ƒg”»’è
	switch (windrv.enable) {
	case WINDRV_MODE_DUAL:
	case WINDRV_MODE_ENABLE:
		switch (addr) {
		case 0xE9F000:
#ifdef WINDRV_SUPPORT_COMPATIBLE
			if (windrv.enable == WINDRV_MODE_DUAL) {
				Execute();	// ‰Â”\‚È‚ç‚±‚±‚¾‚¯‚Å‚àc‚µ‚½‚¢
			}
#endif // WINDRV_SUPPORT_COMPATIBLE
			return;		// ƒ|[ƒg–¢À‘•‚Å‚àƒoƒXƒGƒ‰[‚É‚µ‚È‚¢(d—v)
		case 0xE9F001:
			switch (data) {
			case 0x00: ExecuteAsynchronous(); return;
			case 0xFF: ReleaseAsynchronous(); return;
			}
			return;		// •s³‚È’l‚Ì‘‚«‚İ‚Å‚àƒoƒXƒGƒ‰[‚É‚µ‚È‚¢(Šg’£—p)
		}
		break;

	case WINDRV_MODE_COMPATIBLE:
#ifdef WINDRV_SUPPORT_COMPATIBLE
		switch (addr) {
		case 0xE9F000:
			Execute();
			return;
		}
		break;
#endif // WINDRV_SUPPORT_COMPATIBLE

	case WINDRV_MODE_NONE:
		switch (addr) {
		case 0xE9F000:
		case 0xE9F001:
			return;		// ƒ|[ƒgƒAƒhƒŒƒX‚Ì‚İÀ‘• (‹Œƒo[ƒWƒ‡ƒ“‚ÌXM6‚Æ‚ÌŒİŠ·«‚Ì‚½‚ß)
		}
		break;
	}

	// ‘‚«‚İæ‚ª‚È‚¯‚ê‚ÎƒoƒXƒGƒ‰[
	cpu->BusErr(addr, FALSE);
	return;
}

#ifdef WINDRV_SUPPORT_COMPATIBLE
//---------------------------------------------------------------------------
//
//	ƒRƒ}ƒ“ƒhƒnƒ“ƒhƒ‰
//
//---------------------------------------------------------------------------
void FASTCALL Windrv::Execute()
{
	ASSERT(this);

	// ƒŠƒNƒGƒXƒgƒwƒbƒ_ó‚¯æ‚è
	CPU::cpu_t reg;
	cpu->GetCPU(&reg);
	DWORD a5 = reg.areg[5];
	ASSERT(a5 <= 0xFFFFFF);

	// Às
	CWindrv* ps = m_cThread.Alloc();
	ASSERT(ps);
	DWORD result = ps->ExecuteCompatible(a5);
	m_cThread.Free(ps);

	// WINDRVŒİŠ·“®ì‚Ì‚½‚ßD0.Lİ’è
	reg.dreg[0] = result;
	cpu->SetCPU(&reg);

	// “¯ŠúŒ^‚Í‘S‚Ä‚ÌƒRƒ}ƒ“ƒh‚Å128us‚ğÁ”ï
	scheduler->Wait(128);	// TODO: ³Šm‚È’l‚ğZo‚·‚é
}
#endif // WINDRV_SUPPORT_COMPATIBLE

//---------------------------------------------------------------------------
//
//	ƒRƒ}ƒ“ƒhÀsŠJn ”ñ“¯Šú
//
//---------------------------------------------------------------------------
void FASTCALL Windrv::ExecuteAsynchronous()
{
	ASSERT(this);

	// ƒŠƒNƒGƒXƒgƒwƒbƒ_ó‚¯æ‚è
	CPU::cpu_t reg;
	cpu->GetCPU(&reg);
	DWORD a5 = reg.areg[5];
	ASSERT(a5 <= 0xFFFFFF);

	// ‹ó‚«ƒXƒŒƒbƒh‚ğ‘{‚·
	CWindrv* thread = m_cThread.Alloc();
	if (thread) {
		// ƒXƒŒƒbƒhÀsŠJn
		thread->Execute(a5);
		reg.dreg[0] = thread->GetHandle();
	} else {
		// ƒXƒŒƒbƒh‚ªŒ©‚Â‚©‚ç‚È‚¯‚ê‚Î-1
		reg.dreg[0] = (DWORD)-1;
		::Sleep(0);		// ƒXƒŒƒbƒhÀsŒ ‚ğˆêuˆÚ‚·
	}

	// ƒnƒ“ƒhƒ‹‚ğƒZƒbƒg‚µ‚Ä‘¦I—¹
	cpu->SetCPU(&reg);
}

//---------------------------------------------------------------------------
//
//	ƒXƒe[ƒ^ƒXŠl“¾ ”ñ“¯Šú
//
//---------------------------------------------------------------------------
DWORD FASTCALL Windrv::StatusAsynchronous()
{
	ASSERT(this);

	// ƒnƒ“ƒhƒ‹ó‚¯æ‚è
	CPU::cpu_t reg;
	cpu->GetCPU(&reg);
	DWORD handle = reg.dreg[0];

	// ŠY“–‚·‚éƒnƒ“ƒhƒ‹‚ÌƒXƒe[ƒ^ƒX‚ğŠl“¾
	CWindrv* thread = m_cThread.Search(handle);
	if (thread) {
		if (thread->isExecute()) {
			::Sleep(0);		// ƒXƒŒƒbƒhÀsŒ ‚ğˆêuˆÚ‚·
			return 0;		// ‚Ü‚¾Às’†‚È‚ç0
		}

		return 1;			// ÀsŠ®—¹‚È‚ç1
	}

	return 0xFF;			// •s³‚Èƒnƒ“ƒhƒ‹‚È‚ç-1
}

//---------------------------------------------------------------------------
//
//	ƒnƒ“ƒhƒ‹ŠJ•ú ”ñ“¯Šú
//
//---------------------------------------------------------------------------
void FASTCALL Windrv::ReleaseAsynchronous()
{
	ASSERT(this);

	// D0.L(ƒnƒ“ƒhƒ‹)ó‚¯æ‚è
	CPU::cpu_t reg;
	cpu->GetCPU(&reg);
	DWORD handle = reg.dreg[0];

	// ŠY“–‚·‚éƒnƒ“ƒhƒ‹‚ğŠJ•ú
	CWindrv* thread = m_cThread.Search(handle);
	if (thread) {
		m_cThread.Free(thread);
	}
}

//---------------------------------------------------------------------------
//
//	ƒRƒ}ƒ“ƒhÀs
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::ExecuteCommand()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);

	if (command == 0x40) {
		// $40 - ‰Šú‰»
		InitDrive();
		return;
	}

	if (windrv->isInvalidUnit(unit)) {
		LockXM();

		// –³Œø‚Èƒ†ƒjƒbƒg”Ô†‚ğw’è‚µ‚½
		SetResult(FS_FATAL_INVALIDUNIT);

		UnlockXM();
		return;
	}

	// ƒRƒ}ƒ“ƒh•ªŠò
	switch (command & 0x7F) {
		case 0x41: CheckDir(); return;		// $41 - ƒfƒBƒŒƒNƒgƒŠƒ`ƒFƒbƒN
		case 0x42: MakeDir(); return;		// $42 - ƒfƒBƒŒƒNƒgƒŠì¬
		case 0x43: RemoveDir(); return;		// $43 - ƒfƒBƒŒƒNƒgƒŠíœ
		case 0x44: Rename(); return;		// $44 - ƒtƒ@ƒCƒ‹–¼•ÏX
		case 0x45: Delete(); return;		// $45 - ƒtƒ@ƒCƒ‹íœ
		case 0x46: Attribute(); return;		// $46 - ƒtƒ@ƒCƒ‹‘®«æ“¾/İ’è
		case 0x47: Files(); return;			// $47 - ƒtƒ@ƒCƒ‹ŒŸõ(First)
		case 0x48: NFiles(); return;		// $48 - ƒtƒ@ƒCƒ‹ŒŸõ(Next);
		case 0x49: Create(); return;		// $49 - ƒtƒ@ƒCƒ‹ì¬
		case 0x4A: Open(); return;			// $4A - ƒtƒ@ƒCƒ‹ƒI[ƒvƒ“
		case 0x4B: Close(); return;			// $4B - ƒtƒ@ƒCƒ‹ƒNƒ[ƒY
		case 0x4C: Read(); return;			// $4C - ƒtƒ@ƒCƒ‹“Ç‚İ‚İ
		case 0x4D: Write(); return;			// $4D - ƒtƒ@ƒCƒ‹‘‚«‚İ
		case 0x4E: Seek(); return;			// $4E - ƒtƒ@ƒCƒ‹ƒV[ƒN
		case 0x4F: TimeStamp(); return;		// $4F - ƒtƒ@ƒCƒ‹XV‚Ìæ“¾/İ’è
		case 0x50: GetCapacity(); return;	// $50 - —e—Êæ“¾
		case 0x51: CtrlDrive(); return;		// $51 - ƒhƒ‰ƒCƒu§Œä/ó‘ÔŒŸ¸
		case 0x52: GetDPB(); return;		// $52 - DPBæ“¾
		case 0x53: DiskRead(); return;		// $53 - ƒZƒNƒ^“Ç‚İ‚İ
		case 0x54: DiskWrite(); return;		// $54 - ƒZƒNƒ^‘‚«‚İ
		case 0x55: IoControl(); return;		// $55 - IOCTRL
		case 0x56: Flush(); return;			// $56 - ƒtƒ‰ƒbƒVƒ…
		case 0x57: CheckMedia(); return;	// $57 - ƒƒfƒBƒAŒğŠ·ƒ`ƒFƒbƒN
		case 0x58: Lock(); return;			// $58 - ”r‘¼§Œä
	}


	LockXM();

	// ƒfƒoƒCƒXƒhƒ‰ƒCƒo‚É–³Œø‚ÈƒRƒ}ƒ“ƒh‚ğw’è‚µ‚½
	SetResult(FS_FATAL_INVALIDCOMMAND);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€İ’è
//
//---------------------------------------------------------------------------
void FASTCALL Windrv::SetFileSys(FileSys *fs)
{
	ASSERT(this);

	// Œ»İ‘¶İ‚µ‚½ê‡Aˆê’v‚µ‚È‚¯‚ê‚Îˆê’U•Â‚¶‚é
	if (windrv.fs) {
		if (windrv.fs != fs) {
			// •Â‚¶‚é
			Reset();
		}
	}

	// NULL‚Ü‚½‚ÍÀ‘Ì
	windrv.fs = fs;
}

//---------------------------------------------------------------------------
//
//	ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€İ’è
//
//---------------------------------------------------------------------------
BOOL FASTCALL Windrv::isInvalidUnit(DWORD unit) const
{
	ASSERT(this);

	if (unit >= windrv.drives) return TRUE;	// ‹«ŠE‚æ‚è‘å‚«‚¯‚ê‚Î•s³‚Èƒ†ƒjƒbƒg”Ô†

	ASSERT(windrv.fs);
	// Œµ–§‚É‚â‚é‚È‚çA‘ÎÛ‚Æ‚È‚éFileSys‚Ì“®ìƒ‚[ƒh‚ğ–â‚¢‡‚í‚¹‚é‚×‚«‚¾‚ªA
	// ƒAƒNƒZƒX•s‰Â”\‚Èƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€‚Í‚»‚à‚»‚à“o˜^‚³‚ê‚È‚¢‚Ì‚Åƒ`ƒFƒbƒN•s—v
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	$40 - ‰Šú‰»
//
//	in	(offset	size)
//		 0	1.b	’è”(22)
//		 2	1.b	ƒRƒ}ƒ“ƒh($40/$c0)
//		18	1.l	ƒpƒ‰ƒ[ƒ^ƒAƒhƒŒƒX
//		22	1.b	ƒhƒ‰ƒCƒu”Ô†
//	out	(offset	size)
//		 3	1.b	ƒGƒ‰[ƒR[ƒh(‰ºˆÊ)
//		 4	1.b	V	    (ãˆÊ)
//		13	1.b	ƒ†ƒjƒbƒg”
//		14	1.l	ƒfƒoƒCƒXƒhƒ‰ƒCƒo‚ÌI—¹ƒAƒhƒŒƒX + 1
//
//	@ƒ[ƒJƒ‹ƒhƒ‰ƒCƒu‚ÌƒRƒ}ƒ“ƒh 0 ‚Æ“¯—l‚É‘g‚İ‚İ‚ÉŒÄ‚Î‚ê‚é‚ªABPB ‹y
//	‚Ñ‚»‚Ìƒ|ƒCƒ“ƒ^‚Ì”z—ñ‚ğ—pˆÓ‚·‚é•K—v‚Í‚È‚¢.
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::InitDrive()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->GetFilesystem());

#ifdef WINDRV_LOG
	Log(Log::Normal, "$40 - ‰Šú‰»");
#endif // WINDRV_LOG

	LockXM();

	// ƒx[ƒXƒhƒ‰ƒCƒu–¼‚ğŠl“¾
	DWORD base = GetByte(a5 + 22);
	ASSERT(base < 26);
	// DRIVEƒRƒ}ƒ“ƒh‚Å‡˜‚ª•Ï‚í‚é‚Æ’l‚ª–³Œø‚É‚È‚é‚½‚ßAwindrv.base‚Í”p~

	// ƒIƒvƒVƒ‡ƒ““à—e‚ğŠl“¾
	BYTE chOption[256];
	GetParameter(GetAddr(a5 + 18), chOption, sizeof(chOption));

	UnlockXM();

	// Human68k‘¤‚Å—˜—p‰Â”\‚Èƒhƒ‰ƒCƒu”‚Ì”ÍˆÍ‚ÅAƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€‚ğ\’z
	DWORD max_unit = windrv->GetFilesystem()->Init(this, 26 - base, chOption);

	// ÀÛ‚ÉŠm•Û‚³‚ê‚½ƒ†ƒjƒbƒg”‚ğ•Û‘¶
	windrv->SetUnitMax(max_unit);

#ifdef WINDRV_LOG
	Log(Log::Detail, "ƒx[ƒXƒhƒ‰ƒCƒu:%c ƒTƒ|[ƒgƒhƒ‰ƒCƒu”:%d",
					'A' + base, max_unit);
#endif // WINDRV_LOG

	LockXM();

	// ƒhƒ‰ƒCƒu”‚ğ•ÔM
	SetByte(a5 + 13, max_unit);

	// I—¹’l
	SetResult(0);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$41 - ƒfƒBƒŒƒNƒgƒŠƒ`ƒFƒbƒN
//
//	in
//		14	1.L	NAMESTS\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::CheckDir()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ŒŸõ‘ÎÛƒtƒ@ƒCƒ‹–¼Šl“¾
	Human68k::namests_t ns;
	GetNameStsPath(GetAddr(a5 + 14), &ns);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$41 %d ƒfƒBƒŒƒNƒgƒŠƒ`ƒFƒbƒN", unit);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->CheckDir(this, &ns);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$42 - ƒfƒBƒŒƒNƒgƒŠì¬
//
//	in
//		14	1.L	NAMESTS\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::MakeDir()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ŒŸõ‘ÎÛƒtƒ@ƒCƒ‹–¼Šl“¾
	Human68k::namests_t ns;
	GetNameSts(GetAddr(a5 + 14), &ns);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$42 %d ƒfƒBƒŒƒNƒgƒŠì¬", unit);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->MakeDir(this, &ns);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$43 - ƒfƒBƒŒƒNƒgƒŠíœ
//
//	in
//		14	1.L	NAMESTS\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::RemoveDir()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ŒŸõ‘ÎÛƒtƒ@ƒCƒ‹–¼Šl“¾
	Human68k::namests_t ns;
	GetNameSts(GetAddr(a5 + 14), &ns);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$43 %d ƒfƒBƒŒƒNƒgƒŠíœ", unit);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->RemoveDir(this, &ns);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$44 - ƒtƒ@ƒCƒ‹–¼•ÏX
//
//	in
//		14	1.L	NAMESTS\‘¢‘ÌƒAƒhƒŒƒX ‹Œƒtƒ@ƒCƒ‹–¼
//		18	1.L	NAMESTS\‘¢‘ÌƒAƒhƒŒƒX Vƒtƒ@ƒCƒ‹–¼
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Rename()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ŒŸõ‘ÎÛƒtƒ@ƒCƒ‹–¼Šl“¾
	Human68k::namests_t ns;
	GetNameSts(GetAddr(a5 + 14), &ns);
	Human68k::namests_t ns_new;
	GetNameSts(GetAddr(a5 + 18), &ns_new);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$44 %d ƒtƒ@ƒCƒ‹–¼•ÏX", unit);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->Rename(this, &ns, &ns_new);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$45 - ƒtƒ@ƒCƒ‹íœ
//
//	in
//		14	1.L	NAMESTS\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Delete()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ŒŸõ‘ÎÛƒtƒ@ƒCƒ‹–¼Šl“¾
	Human68k::namests_t ns;
	GetNameSts(GetAddr(a5 + 14), &ns);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$45 %d ƒtƒ@ƒCƒ‹íœ", unit);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->Delete(this, &ns);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$46 - ƒtƒ@ƒCƒ‹‘®«æ“¾/İ’è
//
//	in
//		12	1.B	“Ç‚İo‚µ‚É0x01‚É‚È‚é‚Ì‚Å’ˆÓ
//		13	1.B	‘®« $FF‚¾‚Æ“Ç‚İo‚µ
//		14	1.L	NAMESTS\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Attribute()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ŒŸõ‘ÎÛƒtƒ@ƒCƒ‹–¼Šl“¾
	Human68k::namests_t ns;
	GetNameSts(GetAddr(a5 + 14), &ns);

	// ‘ÎÛ‘®«
	DWORD attr = GetByte(a5 + 13);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$46 %d ƒtƒ@ƒCƒ‹‘®«æ“¾/İ’è ‘®«:%02X", unit, attr);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->Attribute(this, &ns, attr);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$47 - ƒtƒ@ƒCƒ‹ŒŸõ(First)
//
//	in	(offset	size)
//		 0	1.b	’è”(26)
//		 1	1.b	ƒ†ƒjƒbƒg”Ô†
//		 2	1.b	ƒRƒ}ƒ“ƒh($47/$c7)
//		13	1.b	ŒŸõ‘®«
//		14	1.l	ƒtƒ@ƒCƒ‹–¼ƒoƒbƒtƒ@(namests Œ`®)
//		18	1.l	ŒŸõƒoƒbƒtƒ@(files Œ`®) ‚±‚Ìƒoƒbƒtƒ@‚ÉŒŸõ“r’†î•ñ‚ÆŒŸõŒ‹‰Ê‚ğ‘‚«‚Ş
//	out	(offset	size)
//		 3	1.b	ƒGƒ‰[ƒR[ƒh(‰ºˆÊ)
//		 4	1.b	V	    (ãˆÊ)
//		18	1.l	ƒŠƒUƒ‹ƒgƒXƒe[ƒ^ƒX
//
//	@ƒfƒBƒŒƒNƒgƒŠ‚©‚çw’èƒtƒ@ƒCƒ‹‚ğŒŸõ‚·‚é. DOS _FILES ‚©‚çŒÄ‚Ño‚³‚ê‚é.
//	@ŒŸõ‚É¸”s‚µ‚½ê‡Aá‚µ‚­‚ÍŒŸõ‚É¬Œ÷‚µ‚Ä‚àƒƒCƒ‹ƒhƒJ[ƒh‚ªg‚í‚ê‚Ä
//	‚¢‚È‚¢ê‡‚ÍAŸ‰ñŒŸõ‚É•K‚¸¸”s‚³‚¹‚éˆ×‚ÉŒŸõƒoƒbƒtƒ@‚ÌƒIƒtƒZƒbƒg‚É
//	-1 ‚ğ‘‚«‚Ş. ŒŸõ‚ª¬Œ÷‚µ‚½ê‡‚ÍŒ©‚Â‚©‚Á‚½ƒtƒ@ƒCƒ‹‚Ìî•ñ‚ğİ’è‚·‚é
//	‚Æ‹¤‚ÉAŸŒŸõ—p‚Ìî•ñ‚ÌƒZƒNƒ^”Ô†AƒIƒtƒZƒbƒgAƒ‹[ƒgƒfƒBƒŒƒNƒgƒŠ‚Ìê
//	‡‚ÍX‚Éc‚èƒZƒNƒ^”‚ğİ’è‚·‚é. ŒŸõƒhƒ‰ƒCƒuE‘®«AƒpƒX–¼‚Í DOS ƒR[
//	ƒ‹ˆ—“à‚Åİ’è‚³‚ê‚é‚Ì‚Å‘‚«‚Ş•K—v‚Í‚È‚¢.
//
//	<NAMESTS\‘¢‘Ì>
//	offset	size
//	0	1.b	NAMWLD	0:ƒƒCƒ‹ƒhƒJ[ƒh‚È‚µ -1:ƒtƒ@ƒCƒ‹w’è‚È‚µ
//				(ƒƒCƒ‹ƒhƒJ[ƒh‚Ì•¶š”)
//	1	1.b	NAMDRV	ƒhƒ‰ƒCƒu”Ô†(A=0,B=1,c,Z=25)
//	2	65.b	NAMPTH	ƒpƒX('\'{‚ ‚ê‚ÎƒTƒuƒfƒBƒŒƒNƒgƒŠ–¼{'\')
//	67	8.b	NAMNM1	ƒtƒ@ƒCƒ‹–¼(æ“ª 8 •¶š)
//	75	3.b	NAMEXT	Šg’£q
//	78	10.b	NAMNM2	ƒtƒ@ƒCƒ‹–¼(c‚è‚Ì 10 •¶š)
//
// ƒpƒX‹æØ‚è•¶š‚Í0x2F(/)‚â0x5C(\)‚Å‚Í‚È‚­0x09(TAB)‚ğg‚Á‚Ä‚¢‚é‚Ì‚Å’ˆÓ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Files()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ŒŸõ“r’†Œo‰ßŠi”[—Ìˆæ
	DWORD nFiles = GetAddr(a5 + 18);
	Human68k::files_t info;
	GetFiles(nFiles, &info);
	//info.fatr =  (BYTE)GetByte(a5 + 13);	// ŒŸõ‘®«•Û‘¶(Šù‚É‘‚«‚Ü‚ê‚Ä‚¢‚é)

	// ŒŸõ‘ÎÛƒtƒ@ƒCƒ‹–¼Šl“¾
	Human68k::namests_t ns;
	GetNameSts(GetAddr(a5 + 14), &ns);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$47 %d ƒtƒ@ƒCƒ‹ŒŸõ(First) Info:%X ‘®«:%02X", unit, nFiles, info.fatr);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->Files(this, &ns, nFiles, &info);

	LockXM();

	// ŒŸõŒ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetFiles(nFiles, &info);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$48 - ƒtƒ@ƒCƒ‹ŒŸõ(Next)
//
//	in
//		18	1.L	FILES\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::NFiles()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ƒ[ƒN—Ìˆæ‚Ì“Ç‚İ‚İ
	DWORD nFiles = GetAddr(a5 + 18);
	Human68k::files_t info;
	GetFiles(nFiles, &info);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$48 - ƒtƒ@ƒCƒ‹ŒŸõ(Next) Info:%X", nFiles);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->NFiles(this, nFiles, &info);

	LockXM();

	// ŒŸõŒ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetFiles(nFiles, &info);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$49 - ƒtƒ@ƒCƒ‹ì¬(Create)
//
//	in
//		 1	1.B	ƒ†ƒjƒbƒg”Ô†
//		13	1.B	‘®«
//		14	1.L	NAMESTS\‘¢‘ÌƒAƒhƒŒƒX
//		18	1.L	ƒ‚[ƒh (0:_NEWFILE 1:_CREATE)
//		22	1.L	FCB\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Create()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ‘ÎÛƒtƒ@ƒCƒ‹–¼Šl“¾
	Human68k::namests_t ns;
	GetNameSts(GetAddr(a5 + 14), &ns);

	// FCBŠl“¾
	DWORD nFcb = GetAddr(a5 + 22);	// FCBƒAƒhƒŒƒX•Û‘¶
	Human68k::fcb_t fcb;
	GetFcb(nFcb, &fcb);

	// ‘®«
	DWORD attr = GetByte(a5 + 13);

	// ‹­§ã‘‚«ƒ‚[ƒh
	BOOL force = (BOOL)GetLong(a5 + 18);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$49 %d ƒtƒ@ƒCƒ‹ì¬ FCB:%X ‘®«:%02X Mode:%d", unit, nFcb, attr, force);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->Create(this, &ns, nFcb, &fcb, attr, force);

	LockXM();

	// Œ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetFcb(nFcb, &fcb);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$4A - ƒtƒ@ƒCƒ‹ƒI[ƒvƒ“
//
//	in
//		 1	1.B	ƒ†ƒjƒbƒg”Ô†
//		14	1.L	NAMESTS\‘¢‘ÌƒAƒhƒŒƒX
//		22	1.L	FCB\‘¢‘ÌƒAƒhƒŒƒX
//				Šù‚ÉFCB‚É‚Í‚Ù‚Æ‚ñ‚Ç‚Ìƒpƒ‰ƒ[ƒ^‚ªİ’èÏ‚İ
//				E“ú•t‚ÍƒI[ƒvƒ“‚µ‚½uŠÔ‚Ì‚à‚Ì‚É‚È‚Á‚Ä‚é‚Ì‚Åã‘‚«
//				ƒTƒCƒY‚Í0‚É‚È‚Á‚Ä‚¢‚é‚Ì‚Åã‘‚«
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Open()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ‘ÎÛƒtƒ@ƒCƒ‹–¼Šl“¾
	Human68k::namests_t ns;
	GetNameSts(GetAddr(a5 + 14), &ns);

	// FCBŠl“¾
	DWORD nFcb = GetAddr(a5 + 22);	// FCBƒAƒhƒŒƒX•Û‘¶
	Human68k::fcb_t fcb;
	GetFcb(nFcb, &fcb);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$4A %d ƒtƒ@ƒCƒ‹ƒI[ƒvƒ“ FCB:%X Mode:%X", unit, nFcb, fcb.mode);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->Open(this, &ns, nFcb, &fcb);

	LockXM();

	// Œ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetFcb(nFcb, &fcb);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$4B - ƒtƒ@ƒCƒ‹ƒNƒ[ƒY
//
//	in
//		22	1.L	FCB\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Close()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// FCBŠl“¾
	DWORD nFcb = GetAddr(a5 + 22);	// FCBƒAƒhƒŒƒX•Û‘¶
	Human68k::fcb_t fcb;
	GetFcb(nFcb, &fcb);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$4B - ƒtƒ@ƒCƒ‹ƒNƒ[ƒY FCB:%X", nFcb);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->Close(this, nFcb, &fcb);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$4C - ƒtƒ@ƒCƒ‹“Ç‚İ‚İ
//
//	in
//		14	1.L	“Ç‚İ‚İƒoƒbƒtƒ@ ‚±‚±‚Éƒtƒ@ƒCƒ‹“à—e‚ğ“Ç‚İ‚Ş
//		18	1.L	ƒTƒCƒY •‰‚Ì”‚È‚çƒtƒ@ƒCƒ‹ƒTƒCƒY‚ğw’è‚µ‚½‚Ì‚Æ“¯‚¶
//		22	1.L	FCB\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Read()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// FCBŠl“¾
	DWORD nFcb = GetAddr(a5 + 22);	// FCBƒAƒhƒŒƒX•Û‘¶
	Human68k::fcb_t fcb;
	GetFcb(nFcb, &fcb);

	// “Ç‚İ‚İƒoƒbƒtƒ@
	DWORD nAddress = GetAddr(a5 + 14);

	// “Ç‚İ‚İƒTƒCƒY
	DWORD nSize = GetLong(a5 + 18);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$4C - ƒtƒ@ƒCƒ‹“Ç‚İ‚İ FCB:%X ƒAƒhƒŒƒX:%X ƒTƒCƒY:%d", nFcb, nAddress, nSize);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->Read(this, nFcb, &fcb, nAddress, nSize);

	LockXM();

	// Œ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetFcb(nFcb, &fcb);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$4D - ƒtƒ@ƒCƒ‹‘‚«‚İ
//
//	in
//		14	1.L	‘‚«‚İƒoƒbƒtƒ@ ‚±‚±‚Éƒtƒ@ƒCƒ‹“à—e‚ğ‘‚«‚Ş
//		18	1.L	ƒTƒCƒY •‰‚Ì”‚È‚çƒtƒ@ƒCƒ‹ƒTƒCƒY‚ğw’è‚µ‚½‚Ì‚Æ“¯‚¶
//		22	1.L	FCB\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Write()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// FCBŠl“¾
	DWORD nFcb = GetAddr(a5 + 22);	// FCBƒAƒhƒŒƒX•Û‘¶
	Human68k::fcb_t fcb;
	GetFcb(nFcb, &fcb);

	// ‘‚«‚İƒoƒbƒtƒ@
	DWORD nAddress = GetAddr(a5 + 14);

	// ‘‚«‚İƒTƒCƒY
	DWORD nSize = GetLong(a5 + 18);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$4D - ƒtƒ@ƒCƒ‹‘‚«‚İ FCB:%X ƒAƒhƒŒƒX:%X ƒTƒCƒY:%d", nFcb, nAddress, nSize);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->Write(this, nFcb, &fcb, nAddress, nSize);

	LockXM();

	// Œ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetFcb(nFcb, &fcb);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$4E - ƒtƒ@ƒCƒ‹ƒV[ƒN
//
//	in
//		12	1.B	0x2B ‚É‚È‚Á‚Ä‚é‚Æ‚«‚ª‚ ‚é 0‚Ì‚Æ‚«‚à‚ ‚é
//		13	1.B	ƒ‚[ƒh
//		18	1.L	ƒIƒtƒZƒbƒg
//		22	1.L	FCB\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Seek()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// FCBŠl“¾
	DWORD nFcb = GetAddr(a5 + 22);	// FCBƒAƒhƒŒƒX•Û‘¶
	Human68k::fcb_t fcb;
	GetFcb(nFcb, &fcb);

	// ƒV[ƒNƒ‚[ƒh
	DWORD nMode = GetByte(a5 + 13);

	// ƒV[ƒNƒIƒtƒZƒbƒg
	DWORD nOffset = GetLong(a5 + 18);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$4E - ƒtƒ@ƒCƒ‹ƒV[ƒN FCB:%X ƒ‚[ƒh:%d ƒIƒtƒZƒbƒg:%d", nFcb, nMode, nOffset);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->Seek(this, nFcb, &fcb, nMode, nOffset);

	LockXM();

	// Œ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetFcb(nFcb, &fcb);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$4F - ƒtƒ@ƒCƒ‹æ“¾/İ’è
//
//	in
//		18	1.W	DATE
//		20	1.W	TIME
//		22	1.L	FCB\‘¢‘ÌƒAƒhƒŒƒX
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::TimeStamp()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// FCBŠl“¾
	DWORD nFcb = GetAddr(a5 + 22);	// FCBƒAƒhƒŒƒX•Û‘¶
	Human68k::fcb_t fcb;
	GetFcb(nFcb, &fcb);

	// Šl“¾
	WORD nFatDate = (WORD)GetWord(a5 + 18);
	WORD nFatTime = (WORD)GetWord(a5 + 20);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$4F - ƒtƒ@ƒCƒ‹æ“¾/İ’è FCB:%X :%04X%04X", nFcb, nFatDate, nFatTime);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->TimeStamp(this, nFcb, &fcb, nFatDate, nFatTime);

	LockXM();

	// Œ‹‰Ê‚Ì”½‰f
	if ((nResult >> 16) != 0xFFFF) {
		SetFcb(nFcb, &fcb);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$50 - —e—Êæ“¾
//
//	in	(offset	size)
//		 0	1.b	’è”(26)
//		 1	1.b	ƒ†ƒjƒbƒg”Ô†
//		 2	1.b	ƒRƒ}ƒ“ƒh($50/$d0)
//		14	1.l	ƒoƒbƒtƒ@ƒAƒhƒŒƒX
//	out	(offset	size)
//		 3	1.b	ƒGƒ‰[ƒR[ƒh(‰ºˆÊ)
//		 4	1.b	V	    (ãˆÊ)
//		18	1.l	ƒŠƒUƒ‹ƒgƒXƒe[ƒ^ƒX
//
//	@ƒƒfƒBƒA‚Ì‘—e—Ê/‹ó‚«—e—ÊAƒNƒ‰ƒXƒ^/ƒZƒNƒ^ƒTƒCƒY‚ğû“¾‚·‚é. ƒoƒbƒtƒ@
//	‚É‘‚«‚Ş“à—e‚ÍˆÈ‰º‚Ì’Ê‚è. ƒŠƒUƒ‹ƒgƒXƒe[ƒ^ƒX‚Æ‚µ‚Äg—p‰Â”\‚ÈƒoƒCƒg”
//	‚ğ•Ô‚·‚±‚Æ.
//
//	offset	size
//	0	1.w	g—p‰Â”\‚ÈƒNƒ‰ƒXƒ^”
//	2	1.w	‘ƒNƒ‰ƒXƒ^”
//	4	1.w	1 ƒNƒ‰ƒXƒ^“–‚è‚ÌƒZƒNƒ^”
//	6	1.w	1 ƒZƒNƒ^“–‚è‚ÌƒoƒCƒg”
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::GetCapacity()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

#ifdef WINDRV_LOG
	Log(Log::Normal, "$50 %d —e—Êæ“¾", unit);
#endif // WINDRV_LOG

	LockXM();

	// ƒoƒbƒtƒ@æ“¾
	DWORD nCapacity = GetAddr(a5 + 14);
	Human68k::capacity_t cap;
	//GetCapacity(nCapacity, &cap);	// æ“¾‚Í•s—v ‰Šú‰»‚à•s—v o‘Ov‘¬ —‘‚«–³—p

	UnlockXM();

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->GetCapacity(this, &cap);

	LockXM();

	// Œ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetCapacity(nCapacity, &cap);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Detail, "ƒ†ƒjƒbƒg%d ‹ó‚«—e—Ê %d", unit, nResult);
#endif // WINDRV_LOG
}

//---------------------------------------------------------------------------
//
//	$51 - ƒhƒ‰ƒCƒuó‘ÔŒŸ¸/§Œä
//
//	in	(offset	size)
//		 1	1.B	ƒ†ƒjƒbƒg”Ô†
//		13	1.B	ó‘Ô	0: ó‘ÔŒŸ¸ 1: ƒCƒWƒFƒNƒg
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::CtrlDrive()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// ƒhƒ‰ƒCƒuó‘Ôæ“¾
	Human68k::ctrldrive_t ctrl;
	ctrl.status = (BYTE)GetByte(a5 + 13);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$51 %d ƒhƒ‰ƒCƒuó‘ÔŒŸ¸/§Œä Mode:%d", unit, ctrl.status);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->CtrlDrive(this, &ctrl);

	LockXM();

	// Œ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetByte(a5 + 13, ctrl.status);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$52 - DPBæ“¾
//
//	in	(offset	size)
//		 0	1.b	’è”(26)
//		 1	1.b	ƒ†ƒjƒbƒg”Ô†
//		 2	1.b	ƒRƒ}ƒ“ƒh($52/$d2)
//		14	1.l	ƒoƒbƒtƒ@ƒAƒhƒŒƒX(æ“ªƒAƒhƒŒƒX + 2 ‚ğw‚·)
//	out	(offset	size)
//		 3	1.b	ƒGƒ‰[ƒR[ƒh(‰ºˆÊ)
//		 4	1.b	V	    (ãˆÊ)
//		18	1.l	ƒŠƒUƒ‹ƒgƒXƒe[ƒ^ƒX
//
//	@w’èƒƒfƒBƒA‚Ìî•ñ‚ğ v1 Œ`® DPB ‚Å•Ô‚·. ‚±‚ÌƒRƒ}ƒ“ƒh‚Åİ’è‚·‚é•K—v
//	‚ª‚ ‚éî•ñ‚ÍˆÈ‰º‚Ì’Ê‚è(Š‡ŒÊ“à‚Í DOS ƒR[ƒ‹‚ªİ’è‚·‚é). ‚½‚¾‚µAƒoƒbƒt
//	ƒ@ƒAƒhƒŒƒX‚ÍƒIƒtƒZƒbƒg 2 ‚ğw‚µ‚½ƒAƒhƒŒƒX‚ª“n‚³‚ê‚é‚Ì‚Å’ˆÓ‚·‚é‚±‚Æ.
//
//	offset	size
//	 0	1.b	(ƒhƒ‰ƒCƒu”Ô†)
//	 1	1.b	(ƒ†ƒjƒbƒg”Ô†)
//	 2	1.w	1 ƒZƒNƒ^“–‚è‚ÌƒoƒCƒg”
//	 4	1.b	1 ƒNƒ‰ƒXƒ^“–‚è‚ÌƒZƒNƒ^” - 1
//	 5	1.b	ƒNƒ‰ƒXƒ^¨ƒZƒNƒ^‚ÌƒVƒtƒg”
//			bit 7 = 1 ‚Å MS-DOS Œ`® FAT(16bit Intel ”z—ñ)
//	 6	1.w	FAT ‚Ìæ“ªƒZƒNƒ^”Ô†
//	 8	1.b	FAT —Ìˆæ‚ÌŒÂ”
//	 9	1.b	FAT ‚Ìè‚ß‚éƒZƒNƒ^”(•¡Ê•ª‚ğœ‚­)
//	10	1.w	ƒ‹[ƒgƒfƒBƒŒƒNƒgƒŠ‚É“ü‚éƒtƒ@ƒCƒ‹‚ÌŒÂ”
//	12	1.w	ƒf[ƒ^—Ìˆæ‚Ìæ“ªƒZƒNƒ^”Ô†
//	14	1.w	‘ƒNƒ‰ƒXƒ^” + 1
//	16	1.w	ƒ‹[ƒgƒfƒBƒŒƒNƒgƒŠ‚Ìæ“ªƒZƒNƒ^”Ô†
//	18	1.l	(ƒhƒ‰ƒCƒoƒwƒbƒ_‚ÌƒAƒhƒŒƒX)
//	22	1.b	(¬•¶š‚Ì•¨—ƒhƒ‰ƒCƒu–¼)
//	23	1.b	(DPB g—pƒtƒ‰ƒO:í‚É 0)
//	24	1.l	(Ÿ‚Ì DPB ‚ÌƒAƒhƒŒƒX)
//	28	1.w	(ƒJƒŒƒ“ƒgƒfƒBƒŒƒNƒgƒŠ‚ÌƒNƒ‰ƒXƒ^”Ô†:í‚É 0)
//	30	64.b	(ƒJƒŒƒ“ƒgƒfƒBƒŒƒNƒgƒŠ–¼)
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::GetDPB()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// DPBæ“¾
	DWORD nDpb = GetAddr(a5 + 14);
	Human68k::dpb_t dpb;
	//GetDpb(nDpb, &dpb);	// æ“¾‚Í•s—v ‰Šú‰»‚à•s—v o‘Ov‘¬ —‘‚«–³—p

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$52 %d DPBæ“¾ DPB:%X", unit, nDpb);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->GetDPB(this, &dpb);

	LockXM();

	// Œ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetDpb(nDpb, &dpb);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$53 - ƒZƒNƒ^“Ç‚İ‚İ
//
//	in	(offset	size)
//		 1	1.B	ƒ†ƒjƒbƒg”Ô†
//		14	1.L	ƒoƒbƒtƒ@ƒAƒhƒŒƒX
//		18	1.L	ƒZƒNƒ^”
//		22	1.L	ƒZƒNƒ^”Ô†
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::DiskRead()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	DWORD nAddress = GetAddr(a5 + 14);	// ƒAƒhƒŒƒX(ãˆÊƒrƒbƒg‚ªŠg’£ƒtƒ‰ƒO)
	DWORD nSize = GetLong(a5 + 18);		// ƒZƒNƒ^”
	DWORD nSector = GetLong(a5 + 22);	// ƒZƒNƒ^”Ô†

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$53 %d ƒZƒNƒ^“Ç‚İ‚İ ƒAƒhƒŒƒX:%X ƒZƒNƒ^:%X ŒÂ”:%X", unit, nAddress, nSector, nSize);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->DiskRead(this, nAddress, nSector, nSize);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$54 - ƒZƒNƒ^‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::DiskWrite()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	DWORD nAddress = GetAddr(a5 + 14);	// ƒAƒhƒŒƒX(ãˆÊƒrƒbƒg‚ªŠg’£ƒtƒ‰ƒO)
	DWORD nSize = GetLong(a5 + 18);		// ƒZƒNƒ^”
	DWORD nSector = GetLong(a5 + 22);	// ƒZƒNƒ^”Ô†

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$53 %d ƒZƒNƒ^‘‚«‚İ ƒAƒhƒŒƒX:%X ƒZƒNƒ^:%X ŒÂ”:%X", unit, nAddress, nSector, nSize);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->DiskWrite(this, nAddress, nSector, nSize);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$55 - IOCTRL
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::IoControl()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

	LockXM();

	// IOCTRLæ“¾
	DWORD nParameter = GetLong(a5 + 14);	// ƒpƒ‰ƒ[ƒ^
	DWORD nFunction = GetWord(a5 + 18);		// ‹@”\”Ô†
	Human68k::ioctrl_t ioctrl;
	GetIoctrl(nParameter, nFunction, &ioctrl);

	UnlockXM();

#ifdef WINDRV_LOG
	Log(Log::Normal, "$55 %d IOCTRL ƒpƒ‰ƒ[ƒ^:%X ‹@”:%X", unit, nParameter, nFunction);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	int nResult = windrv->GetFilesystem()->IoControl(this, &ioctrl, nFunction);

	LockXM();

	// Œ‹‰Ê‚Ì”½‰f
	if (nResult >= 0) {
		SetIoctrl(nParameter, nFunction, &ioctrl);
	}

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$56 - ƒtƒ‰ƒbƒVƒ…
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Flush()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

#ifdef WINDRV_LOG
	Log(Log::Normal, "$56 %d ƒtƒ‰ƒbƒVƒ…", unit);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->Flush(this);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$57 - ƒƒfƒBƒAŒğŠ·ƒ`ƒFƒbƒN
//
//	in	(offset	size)
//		 0	1.b	’è”(26)
//		 1	1.b	ƒ†ƒjƒbƒg”Ô†
//		 2	1.b	ƒRƒ}ƒ“ƒh($57/$d7)
//	out	(offset	size)
//		 3	1.b	ƒGƒ‰[ƒR[ƒh(‰ºˆÊ)
//		 4	1.b	V	    (ãˆÊ)
//		18	1.l	ƒŠƒUƒ‹ƒgƒXƒe[ƒ^ƒX
//
//	@ƒƒfƒBƒA‚ªŒğŠ·‚³‚ê‚½‚©”Û‚©‚ğ’²‚×‚é. ŒğŠ·‚³‚ê‚Ä‚¢‚½ê‡‚ÌƒtƒH[ƒ}ƒbƒg
//	Šm”F‚Í‚±‚ÌƒRƒ}ƒ“ƒh“à‚Ås‚¤‚±‚Æ.
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::CheckMedia()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

#ifdef WINDRV_LOG
	Log(Log::Normal, "$57 %d ƒƒfƒBƒAŒğŠ·ƒ`ƒFƒbƒN", unit);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->CheckMedia(this);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	$58 - ”r‘¼§Œä
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Lock()
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);
	ASSERT(windrv);
	ASSERT(windrv->isValidUnit(unit));
	ASSERT(windrv->GetFilesystem());

#ifdef WINDRV_LOG
	Log(Log::Normal, "$58 %d ”r‘¼§Œä", unit);
#endif // WINDRV_LOG

	// ƒtƒ@ƒCƒ‹ƒVƒXƒeƒ€ŒÄ‚Ño‚µ
	DWORD nResult = windrv->GetFilesystem()->Lock(this);

	LockXM();

	// I—¹’l
	SetResult(nResult);

	UnlockXM();
}

//---------------------------------------------------------------------------
//
//	I—¹’l‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::SetResult(DWORD result)
{
	ASSERT(this);
	ASSERT(a5 <= 0xFFFFFF);

	// ’v–½“IƒGƒ‰[”»’è
	DWORD fatal = 0;
	switch (result) {
	case FS_FATAL_INVALIDUNIT: fatal = 0x5001; break;
	case FS_FATAL_INVALIDCOMMAND: fatal = 0x5003; break;
	case FS_FATAL_WRITEPROTECT: fatal = 0x700D; break;
	case FS_FATAL_MEDIAOFFLINE: fatal = 0x7002; break;
	}

	// Œ‹‰Ê‚ğŠi”[
	if (fatal) {
		// ƒŠƒgƒ‰ƒC‰Â”\‚ğ•Ô‚·‚Æ‚«‚ÍA(a5 + 18)‚ğ‘‚«Š·‚¦‚Ä‚Í‚¢‚¯‚È‚¢B
		// ‚»‚ÌŒã”’‘Ñ‚Å Retry ‚ğ‘I‘ğ‚µ‚½ê‡A‘‚«Š·‚¦‚½’l‚ğ“Ç‚İ‚ñ‚ÅŒë“®ì‚µ‚Ä‚µ‚Ü‚¤B
		result = FS_INVALIDFUNC;
		SetByte(a5 + 3, fatal & 255);
		SetByte(a5 + 4, fatal >> 8);
		if ((fatal & 0x2000) == 0) {
			SetLong(a5 + 18, result);
		}
	} else {
		SetLong(a5 + 18, result);
	}

#if defined(WINDRV_LOG)
	// ƒGƒ‰[‚ª‚È‚¯‚ê‚ÎI—¹
	if (result < 0xFFFFFF80) return;

	// ƒGƒ‰[ƒƒbƒZ[ƒW
	TCHAR* szError;
	switch (result) {
	case FS_INVALIDFUNC: szError = "–³Œø‚Èƒtƒ@ƒ“ƒNƒVƒ‡ƒ“ƒR[ƒh‚ğÀs‚µ‚Ü‚µ‚½"; break;
	case FS_FILENOTFND: szError = "w’è‚µ‚½ƒtƒ@ƒCƒ‹‚ªŒ©‚Â‚©‚è‚Ü‚¹‚ñ"; break;
	case FS_DIRNOTFND: szError = "w’è‚µ‚½ƒfƒBƒŒƒNƒgƒŠ‚ªŒ©‚Â‚©‚è‚Ü‚¹‚ñ"; break;
	case FS_OVEROPENED: szError = "ƒI[ƒvƒ“‚µ‚Ä‚¢‚éƒtƒ@ƒCƒ‹‚ª‘½‚·‚¬‚Ü‚·"; break;
	case FS_CANTACCESS: szError = "ƒfƒBƒŒƒNƒgƒŠ‚âƒ{ƒŠƒ…[ƒ€ƒ‰ƒxƒ‹‚ÍƒAƒNƒZƒX•s‰Â‚Å‚·"; break;
	case FS_NOTOPENED: szError = "w’è‚µ‚½ƒnƒ“ƒhƒ‹‚ÍƒI[ƒvƒ“‚³‚ê‚Ä‚¢‚Ü‚¹‚ñ"; break;
	case FS_INVALIDMEM: szError = "ƒƒ‚ƒŠŠÇ——Ìˆæ‚ª”j‰ó‚³‚ê‚Ü‚µ‚½"; break;
	case FS_OUTOFMEM: szError = "Às‚É•K—v‚Èƒƒ‚ƒŠ‚ª‚ ‚è‚Ü‚¹‚ñ"; break;
	case FS_INVALIDPTR: szError = "–³Œø‚Èƒƒ‚ƒŠŠÇ—ƒ|ƒCƒ“ƒ^‚ğw’è‚µ‚Ü‚µ‚½"; break;
	case FS_INVALIDENV: szError = "•s³‚ÈŠÂ‹«‚ğw’è‚µ‚Ü‚µ‚½"; break;
	case FS_ILLEGALFMT: szError = "Àsƒtƒ@ƒCƒ‹‚ÌƒtƒH[ƒ}ƒbƒg‚ªˆÙí‚Å‚·"; break;
	case FS_ILLEGALMOD: szError = "ƒI[ƒvƒ“‚ÌƒAƒNƒZƒXƒ‚[ƒh‚ªˆÙí‚Å‚·"; break;
	case FS_INVALIDPATH: szError = "ƒtƒ@ƒCƒ‹–¼‚Ìw’è‚ÉŒë‚è‚ª‚ ‚è‚Ü‚·"; break;
	case FS_INVALIDPRM: szError = "–³Œø‚Èƒpƒ‰ƒ[ƒ^‚ÅƒR[ƒ‹‚µ‚Ü‚µ‚½"; break;
	case FS_INVALIDDRV: szError = "ƒhƒ‰ƒCƒuw’è‚ÉŒë‚è‚ª‚ ‚è‚Ü‚·"; break;
	case FS_DELCURDIR: szError = "ƒJƒŒƒ“ƒgƒfƒBƒŒƒNƒgƒŠ‚Ííœ‚Å‚«‚Ü‚¹‚ñ"; break;
	case FS_NOTIOCTRL: szError = "IOCTRL‚Å‚«‚È‚¢ƒfƒoƒCƒX‚Å‚·"; break;
	case FS_LASTFILE: szError = "‚±‚êˆÈãƒtƒ@ƒCƒ‹‚ªŒ©‚Â‚©‚è‚Ü‚¹‚ñ"; break;
	case FS_CANTWRITE: szError = "w’è‚Ìƒtƒ@ƒCƒ‹‚Í‘‚«‚İ‚Å‚«‚Ü‚¹‚ñ"; break;
	case FS_DIRALREADY: szError = "w’è‚ÌƒfƒBƒŒƒNƒgƒŠ‚ÍŠù‚É“o˜^‚³‚ê‚Ä‚¢‚Ü‚·"; break;
	case FS_CANTDELETE: szError = "ƒtƒ@ƒCƒ‹‚ª‚ ‚é‚Ì‚Åíœ‚Å‚«‚Ü‚¹‚ñ"; break;
	case FS_CANTRENAME: szError = "ƒtƒ@ƒCƒ‹‚ª‚ ‚é‚Ì‚ÅƒŠƒl[ƒ€‚Å‚«‚Ü‚¹‚ñ"; break;
	case FS_DISKFULL: szError = "ƒfƒBƒXƒN‚ªˆê”t‚Åƒtƒ@ƒCƒ‹‚ªì‚ê‚Ü‚¹‚ñ"; break;
	case FS_DIRFULL: szError = "ƒfƒBƒŒƒNƒgƒŠ‚ªˆê”t‚Åƒtƒ@ƒCƒ‹‚ªì‚ê‚Ü‚¹‚ñ"; break;
	case FS_CANTSEEK: szError = "w’è‚ÌˆÊ’u‚É‚ÍƒV[ƒN‚Å‚«‚Ü‚¹‚ñ"; break;
	case FS_SUPERVISOR: szError = "ƒX[ƒp[ƒoƒCƒUó‘Ô‚ÅƒX[ƒpƒoƒCƒUw’è‚µ‚Ü‚µ‚½"; break;
	case FS_THREADNAME: szError = "“¯‚¶ƒXƒŒƒbƒh–¼‚ª‘¶İ‚µ‚Ü‚·"; break;
	case FS_BUFWRITE: szError = "ƒvƒƒZƒXŠÔ’ÊM‚Ìƒoƒbƒtƒ@‚ª‘‚İ‹Ö~‚Å‚·"; break;
	case FS_BACKGROUND: szError = "ƒoƒbƒNƒOƒ‰ƒEƒ“ƒhƒvƒƒZƒX‚ğ‹N“®‚Å‚«‚Ü‚¹‚ñ"; break;
	case FS_OUTOFLOCK: szError = "ƒƒbƒN—Ìˆæ‚ª‘«‚è‚Ü‚¹‚ñ"; break;
	case FS_LOCKED: szError = "ƒƒbƒN‚³‚ê‚Ä‚¢‚ÄƒAƒNƒZƒX‚Å‚«‚Ü‚¹‚ñ"; break;
	case FS_DRIVEOPENED: szError = "w’è‚Ìƒhƒ‰ƒCƒu‚Íƒnƒ“ƒhƒ‰‚ªƒI[ƒvƒ“‚³‚ê‚Ä‚¢‚Ü‚·"; break;
	case FS_LINKOVER: szError = "ƒVƒ“ƒ{ƒŠƒbƒNƒŠƒ“ƒNƒlƒXƒg‚ª16‰ñ‚ğ’´‚¦‚Ü‚µ‚½"; break;
	case FS_FILEEXIST: szError = "ƒtƒ@ƒCƒ‹‚ª‘¶İ‚µ‚Ü‚·"; break;
	default: szError = "–¢’è‹`‚ÌƒGƒ‰[‚Å‚·"; break;
	}
	switch (fatal & 255) {
	case 0: break;
	case 1: szError = "–³Œø‚Èƒ†ƒjƒbƒg”Ô†‚ğw’è‚µ‚Ü‚µ‚½"; break;
	case 2: szError = "ƒfƒBƒXƒN‚ª“ü‚Á‚Ä‚¢‚Ü‚¹‚ñ"; break;
	case 3: szError = "–³Œø‚ÈƒRƒ}ƒ“ƒh”Ô†‚ğw’è‚µ‚Ü‚µ‚½"; break;
	case 4: szError = "CRCƒGƒ‰[‚ª”­¶‚µ‚Ü‚µ‚½"; break;
	case 5: szError = "ƒfƒBƒXƒN‚ÌŠÇ——Ìˆæ‚ª”j‰ó‚³‚ê‚Ä‚¢‚Ü‚·"; break;
	case 6: szError = "ƒV[ƒN‚É¸”s‚µ‚Ü‚µ‚½"; break;
	case 7: szError = "–³Œø‚ÈƒƒfƒBƒA‚ğg—p‚µ‚Ü‚µ‚½"; break;
	case 8: szError = "ƒZƒNƒ^‚ªŒ©‚Â‚©‚è‚Ü‚¹‚ñ"; break;
	case 9: szError = "ƒvƒŠƒ“ƒ^‚ª‚Â‚È‚ª‚Á‚Ä‚¢‚Ü‚¹‚ñ"; break;
	case 10: szError = "‘‚«‚İƒGƒ‰[‚ª”­¶‚µ‚Ü‚µ‚½"; break;
	case 11: szError = "“Ç‚İ‚İƒGƒ‰[‚ª”­¶‚µ‚Ü‚µ‚½"; break;
	case 12: szError = "‚»‚Ì‘¼‚ÌƒGƒ‰[‚ª”­¶‚µ‚Ü‚µ‚½"; break;
	case 13: szError = "‘‚«‚İ‹Ö~‚Ì‚½‚ßÀs‚Å‚«‚Ü‚¹‚ñ"; break;
	case 14: szError = "‘‚«‚İ‚Å‚«‚Ü‚¹‚ñ"; break;
	case 15: szError = "ƒtƒ@ƒCƒ‹‹¤—Lˆá”½‚ª”­¶‚µ‚Ü‚µ‚½"; break;
	default: szError = "–¢’è‹`‚Ì’v–½“IƒGƒ‰[‚Å‚·"; break;
	}

	Log(Log::Warning, "$%X %d %s(%d)", command, unit, szError, result);
#endif // WINDRV_LOG
}

//---------------------------------------------------------------------------
//
//	ƒoƒCƒg“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
DWORD FASTCALL CWindrv::GetByte(DWORD addr) const
{
	ASSERT(this);
	ASSERT(memory);
	ASSERT(addr <= 0xffffff);

	// 8bit
	return memory->ReadOnly(addr);
}

//---------------------------------------------------------------------------
//
//	ƒoƒCƒg‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::SetByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(memory);
	ASSERT(addr <= 0xffffff);
	ASSERT(data < 0x100);

	// 8bit
	memory->WriteByte(addr, data);
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
DWORD FASTCALL CWindrv::GetWord(DWORD addr) const
{
	DWORD data;

	ASSERT(this);
	ASSERT(memory);
	ASSERT(addr <= 0xffffff);

	// 16bit
	data = memory->ReadOnly(addr);
	data <<= 8;
	data |= memory->ReadOnly(addr + 1);

	return data;
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::SetWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(memory);
	ASSERT(addr <= 0xffffff);
	ASSERT(data < 0x10000);

	// 16bit
	memory->WriteWord(addr, data);
}

//---------------------------------------------------------------------------
//
//	ƒƒ“ƒO“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
DWORD FASTCALL CWindrv::GetLong(DWORD addr) const
{
	DWORD data;

	ASSERT(this);
	ASSERT(memory);
	ASSERT(addr <= 0xffffff);

	// 32bit
	data = memory->ReadOnly(addr);
	data <<= 8;
	data |= memory->ReadOnly(addr + 1);
	data <<= 8;
	data |= memory->ReadOnly(addr + 2);
	data <<= 8;
	data |= memory->ReadOnly(addr + 3);

	return data;
}

//---------------------------------------------------------------------------
//
//	ƒƒ“ƒO‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::SetLong(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(memory);
	ASSERT(addr <= 0xffffff);

	// 32bit
	memory->WriteWord(addr, (WORD)(data >> 16));
	memory->WriteWord(addr + 2, (WORD)data);
}

//---------------------------------------------------------------------------
//
//	ƒAƒhƒŒƒX“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
DWORD FASTCALL CWindrv::GetAddr(DWORD addr) const
{
	DWORD data;

	ASSERT(this);
	ASSERT(memory);
	ASSERT(addr <= 0xffffff);

	// 24bit(Å‰‚Ì1ƒoƒCƒg‚Í–³‹)
	data = memory->ReadOnly(addr + 1);
	data <<= 8;
	data |= memory->ReadOnly(addr + 2);
	data <<= 8;
	data |= memory->ReadOnly(addr + 3);

	return data;
}

//---------------------------------------------------------------------------
//
//	ƒAƒhƒŒƒX‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::SetAddr(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(memory);
	ASSERT(addr <= 0xffffff);
	ASSERT(data <= 0xffffff);

	// 24bit(Å‰‚Ì1ƒoƒCƒg‚Í•K‚¸0)
	data &= 0xffffff;
	memory->WriteWord(addr, (WORD)(data >> 16));
	memory->WriteWord(addr + 2, (WORD)data);
}

//---------------------------------------------------------------------------
//
//	NAMESTSƒpƒX–¼“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::GetNameStsPath(DWORD addr, Human68k::namests_t *pNamests) const
{
	DWORD offset;

	ASSERT(this);
	ASSERT(pNamests);
	ASSERT(addr <= 0xFFFFFF);

	// ƒƒCƒ‹ƒhƒJ[ƒhî•ñ
	pNamests->wildcard = (BYTE)GetByte(addr + 0);

	// ƒhƒ‰ƒCƒu”Ô†
	pNamests->drive = (BYTE)GetByte(addr + 1);

	// ƒpƒX–¼
	for (offset=0; offset<sizeof(pNamests->path); offset++) {
		pNamests->path[offset] = (BYTE)GetByte(addr + 2 + offset);
	}

	// ƒtƒ@ƒCƒ‹–¼1
	memset(pNamests->name, 0x20, sizeof(pNamests->name));

	// Šg’£q
	memset(pNamests->ext, 0x20, sizeof(pNamests->ext));

	// ƒtƒ@ƒCƒ‹–¼2
	memset(pNamests->add, 0, sizeof(pNamests->add));

#if defined(WINDRV_LOG)
{
	TCHAR szPath[_MAX_PATH];
	pNamests->GetCopyPath((BYTE*)szPath);	// WARNING: Unicode—vC³

	Log(Log::Detail, "Path %s", szPath);
}
#endif // WINDRV_LOG
}

//---------------------------------------------------------------------------
//
//	NAMESTS“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::GetNameSts(DWORD addr, Human68k::namests_t *pNamests) const
{
	DWORD offset;

	ASSERT(this);
	ASSERT(pNamests);
	ASSERT(addr <= 0xFFFFFF);

	// ƒƒCƒ‹ƒhƒJ[ƒhî•ñ
	pNamests->wildcard = (BYTE)GetByte(addr + 0);

	// ƒhƒ‰ƒCƒu”Ô†
	pNamests->drive = (BYTE)GetByte(addr + 1);

	// ƒpƒX–¼
	for (offset=0; offset<sizeof(pNamests->path); offset++) {
		pNamests->path[offset] = (BYTE)GetByte(addr + 2 + offset);
	}

	// ƒtƒ@ƒCƒ‹–¼1
	for (offset=0; offset<sizeof(pNamests->name); offset++) {
		pNamests->name[offset] = (BYTE)GetByte(addr + 67 + offset);
	}

	// Šg’£q
	for (offset=0; offset<sizeof(pNamests->ext); offset++) {
		pNamests->ext[offset] = (BYTE)GetByte(addr + 75 + offset);
	}

	// ƒtƒ@ƒCƒ‹–¼2
	for (offset=0; offset<sizeof(pNamests->add); offset++) {
		pNamests->add[offset] = (BYTE)GetByte(addr + 78 + offset);
	}

#if defined(WINDRV_LOG)
{
	TCHAR szPath[_MAX_PATH];
	pNamests->GetCopyPath((BYTE*)szPath);	// WARNING: Unicode—vC³

	TCHAR szFilename[_MAX_PATH];
	pNamests->GetCopyFilename((BYTE*)szFilename);	// WARNING: Unicode—vC³

	if (pNamests->wildcard == 0xFF) {
		Log(Log::Detail, "Path %s", szPath);
	} else {
		Log(Log::Detail, "Filename %s%s", szPath, szFilename);
	}
}
#endif // WINDRV_LOG
}

//---------------------------------------------------------------------------
//
//	FILES“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::GetFiles(DWORD addr, Human68k::files_t* pFiles) const
{
	ASSERT(this);
	ASSERT(pFiles);
	ASSERT(addr <= 0xFFFFFF);

	// ŒŸõî•ñ
	pFiles->fatr = (BYTE)GetByte(addr + 0);		// Read only
	pFiles->drive = (BYTE)GetByte(addr + 1);	// Read only
	pFiles->sector = GetLong(addr + 2);
	//pFiles->sector2 = (WORD)GetWord(addr + 6);	// Read only
	pFiles->offset = (WORD)GetWord(addr + 8);

#if 0
	DWORD offset;

	// ƒtƒ@ƒCƒ‹–¼
	for (offset=0; offset<sizeof(pFiles->name); offset++) {
		pFiles->name[offset] = (BYTE)GetByte(addr + 10 + offset);
	}

	// Šg’£q
	for (offset=0; offset<sizeof(pFiles->ext); offset++) {
		pFiles->ext[offset] = (BYTE)GetByte(addr + 18 + offset);
	}
#endif

#if 0
	// ƒtƒ@ƒCƒ‹î•ñ
	pFiles->attr = (BYTE)GetByte(addr + 21);
	pFiles->time = (WORD)GetWord(addr + 22);
	pFiles->date = (WORD)GetWord(addr + 24);
	pFiles->size = GetLong(addr + 26);

	// ƒtƒ‹ƒtƒ@ƒCƒ‹–¼
	for (offset=0; offset<sizeof(pFiles->full); offset++) {
		pFiles->full[offset] = (BYTE)GetByte(addr + 30 + offset);
	}
#else
	pFiles->attr = 0;
	pFiles->time = 0;
	pFiles->date = 0;
	pFiles->size = 0;
	memset(pFiles->full, 0, sizeof(pFiles->full));
#endif
}

//---------------------------------------------------------------------------
//
//	FILES‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::SetFiles(DWORD addr, const Human68k::files_t* pFiles)
{
	DWORD offset;

	ASSERT(this);
	ASSERT(pFiles);
	ASSERT(addr <= 0xFFFFFF);

	SetLong(addr + 2, pFiles->sector);
	SetWord(addr + 8, pFiles->offset);

#if 0
	// ŒŸõî•ñ
	SetByte(addr + 0, pFiles->fatr);	// Read only
	SetByte(addr + 1, pFiles->drive);	// Read only
	SetWord(addr + 6, pFiles->sector2);	// Read only

	// ƒtƒ@ƒCƒ‹–¼
	for (offset=0; offset<sizeof(pFiles->name); offset++) {
		SetByte(addr + 10 + offset, pFiles->name[offset]);
	}

	// Šg’£q
	for (offset=0; offset<sizeof(pFiles->ext); offset++) {
		SetByte(addr + 18 + offset, pFiles->ext[offset]);
	}
#endif

	// ƒtƒ@ƒCƒ‹î•ñ
	SetByte(addr + 21, pFiles->attr);
	SetWord(addr + 22, pFiles->time);
	SetWord(addr + 24, pFiles->date);
	SetLong(addr + 26, pFiles->size);

	// ƒtƒ‹ƒtƒ@ƒCƒ‹–¼
	for (offset=0; offset<sizeof(pFiles->full); offset++) {
		SetByte(addr + 30 + offset, pFiles->full[offset]);
	}

#if defined(WINDRV_LOG)
{
	TCHAR szAttribute[16];
	BYTE nAttribute = pFiles->attr;
	for (int i = 0; i < 8; i++) {
		TCHAR c = '-';
		if ((nAttribute & 0x80) != 0)
			c = "XLADVSHR"[i];
		szAttribute[i] = c;
		nAttribute <<= 1;
	}
	szAttribute[8] = '\0';

	Log(Log::Detail, "%s %s %d", szAttribute, pFiles->full, pFiles->size);
}
#endif // WINDRV_LOG
}

//---------------------------------------------------------------------------
//
//	FCB“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::GetFcb(DWORD addr, Human68k::fcb_t* pFcb) const
{
	ASSERT(this);
	ASSERT(pFcb);
	ASSERT(addr <= 0xFFFFFF);

	// FCBî•ñ
	pFcb->fileptr = GetLong(addr + 6);
	pFcb->mode = (WORD)GetWord(addr + 14);
	//pFcb->zero = GetLong(addr + 32);

#if 0
	DWORD i;

	// ƒtƒ@ƒCƒ‹–¼
	for (i = 0; i < sizeof(pFcb->name); i++) {
		pFcb->name[i] = (BYTE)GetByte(addr + 36 + i);
	}

	// Šg’£q
	for (i = 0; i < sizeof(pFcb->ext); i++) {
		pFcb->ext[i] = (BYTE)GetByte(addr + 44 + i);
	}

	// ƒtƒ@ƒCƒ‹–¼2
	for (i = 0; i < sizeof(pFcb->add); i++) {
		pFcb->add[i] = (BYTE)GetByte(addr + 48 + i);
	}
#endif

	// ‘®«
	pFcb->attr = (BYTE)GetByte(addr + 47);

	// FCBî•ñ
	pFcb->time = (WORD)GetWord(addr + 58);
	pFcb->date = (WORD)GetWord(addr + 60);
	//pFcb->cluster = (WORD)GetWord(addr + 62);
	pFcb->size = GetLong(addr + 64);
}

//---------------------------------------------------------------------------
//
//	FCB‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::SetFcb(DWORD addr, const Human68k::fcb_t* pFcb)
{
	ASSERT(this);
	ASSERT(pFcb);
	ASSERT(addr <= 0xFFFFFF);

	// FCBî•ñ
	SetLong(addr + 6, pFcb->fileptr);
	SetWord(addr + 14, pFcb->mode);
	// SetLong(addr + 32, pFcb->zero);

#if 0
	DWORD i;

	// ƒtƒ@ƒCƒ‹–¼
	for (i = 0; i < sizeof(pFcb->name); i++) {
		SetByte(addr + 36 + i, pFcb->name[i]);
	}

	// Šg’£q
	for (i = 0; i < sizeof(pFcb->ext); i++) {
		SetByte(addr + 44 + i, pFcb->ext[i]);
	}

	// ƒtƒ@ƒCƒ‹–¼2
	for (i = 0; i < sizeof(pFcb->add); i++) {
		SetByte(addr + 48 + i, pFcb->add[i]);
	}
#endif

	// ‘®«
	SetByte(addr + 47, pFcb->attr);

	// FCBî•ñ
	SetWord(addr + 58, pFcb->time);
	SetWord(addr + 60, pFcb->date);
	//SetWord(addr + 62, pFcb->cluster);
	SetLong(addr + 64, pFcb->size);
}

//---------------------------------------------------------------------------
//
//	CAPACITY‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::SetCapacity(DWORD addr, const Human68k::capacity_t* pCapacity)
{
	ASSERT(this);
	ASSERT(pCapacity);
	ASSERT(addr <= 0xffffff);

#ifdef WINDRV_LOG
	Log(Log::Detail, "Free:%d Clusters:%d Sectors:%d Bytes:%d",
		pCapacity->free, pCapacity->clusters, pCapacity->sectors, pCapacity->bytes);
#endif // WINDRV_LOG

	SetWord(addr + 0, pCapacity->free);
	SetWord(addr + 2, pCapacity->clusters);
	SetWord(addr + 4, pCapacity->sectors);
	SetWord(addr + 6, pCapacity->bytes);
}

//---------------------------------------------------------------------------
//
//	DPB‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::SetDpb(DWORD addr, const Human68k::dpb_t* pDpb)
{
	ASSERT(this);
	ASSERT(pDpb);
	ASSERT(addr <= 0xFFFFFF);

	// DPBî•ñ
	SetWord(addr + 0, pDpb->sector_size);
	SetByte(addr + 2, pDpb->cluster_size);
	SetByte(addr + 3, pDpb->shift);
	SetWord(addr + 4, pDpb->fat_sector);
	SetByte(addr + 6, pDpb->fat_max);
	SetByte(addr + 7, pDpb->fat_size);
	SetWord(addr + 8, pDpb->file_max);
	SetWord(addr + 10, pDpb->data_sector);
	SetWord(addr + 12, pDpb->cluster_max);
	SetWord(addr + 14, pDpb->root_sector);
	SetByte(addr + 20, pDpb->media);
}

//---------------------------------------------------------------------------
//
//	IOCTRL“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::GetIoctrl(DWORD param, DWORD func, Human68k::ioctrl_t* pIoctrl)
{
	ASSERT(this);
	ASSERT(pIoctrl);

	switch (func) {
	case 2:
		// ƒƒfƒBƒAÄ”F¯
		pIoctrl->param = param;
		return;

	case -2:
		// ƒIƒvƒVƒ‡ƒ“İ’è
		ASSERT(param <= 0xFFFFFF);
		pIoctrl->param = GetLong(param);
		return;

#if 0
	case 0:
		// ƒƒfƒBƒAID‚ÌŠl“¾
		return;

	case 1:
		// Human68kŒİŠ·‚Ì‚½‚ß‚Ìƒ_ƒ~[
		return;

	case -1:
		// í’“”»’è
		return;

	case -3:
		// ƒIƒvƒVƒ‡ƒ“Šl“¾
		return;
#endif
	}
}

//---------------------------------------------------------------------------
//
//	IOCTRL‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::SetIoctrl(DWORD param, DWORD func, const Human68k::ioctrl_t* pIoctrl)
{
	ASSERT(this);
	ASSERT(pIoctrl);

	switch (func) {
	case 0:
		// ƒƒfƒBƒAID‚ÌŠl“¾
		ASSERT(param <= 0xFFFFFF);
		SetWord(param, pIoctrl->media);
		return;

	case 1:
		// Human68kŒİŠ·‚Ì‚½‚ß‚Ìƒ_ƒ~[
		ASSERT(param <= 0xFFFFFF);
		SetLong(param, pIoctrl->param);
		return;

	case -1:
		// í’“”»’è
		ASSERT(param <= 0xFFFFFF);
		{
			for (int i = 0; i < 8; i++) SetByte(param + i, pIoctrl->buffer[i]);
		}
		return;

	case -3:
		// ƒIƒvƒVƒ‡ƒ“Šl“¾
		ASSERT(param <= 0xFFFFFF);
		SetLong(param, pIoctrl->param);
		return;

#if 0
	case 2:
		// ƒƒfƒBƒAÄ”F¯
		return;

	case -2:
		// ƒIƒvƒVƒ‡ƒ“İ’è
		return;
#endif
	}
}

//---------------------------------------------------------------------------
//
//	ƒpƒ‰ƒ[ƒ^“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::GetParameter(DWORD addr, BYTE* pOption, DWORD nSize)
{
	ASSERT(this);
	ASSERT(addr <= 0xFFFFFF);
	ASSERT(pOption);

	DWORD nMode = 0;
	BYTE* p = pOption;
	for (DWORD i = 0; i < nSize - 2; i++) {
		BYTE c = (BYTE)GetByte(addr + i);
		*p++ = c;
		switch (nMode) {
		case 0:
			if (c == '\0') {
				return;
			}
			nMode = 1;
			break;
		case 1:
			if (c == '\0') nMode = 0;
			break;
		}
	}

	*p++ = '\0';
	*p++ = '\0';
}

#ifdef WINDRV_LOG
//---------------------------------------------------------------------------
//
//	ƒƒOo—Í
//
//---------------------------------------------------------------------------
void FASTCALL CWindrv::Log(DWORD level, char* format, ...) const
{
	ASSERT(this);
	ASSERT(format);
	ASSERT(windrv);

	va_list args;
	va_start(args, format);
	char message[512];
	vsprintf(message, format, args);
	va_end(args);

	windrv->Log(level, message);
}

//---------------------------------------------------------------------------
//
//	ƒƒOo—Í
//
//---------------------------------------------------------------------------
void FASTCALL Windrv::Log(DWORD level, char* message) const
{
	ASSERT(this);
	ASSERT(message);

	LOG((enum Log::loglevel)level, "%s", message);
}
#endif // WINDRV_LOG
