//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ MFC �X�P�W���[�� ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "cpu.h"
#include "mouse.h"
#include "render.h"
#include "config.h"
#include "mfc_com.h"
#include "mfc_frm.h"
#include "mfc_draw.h"
#include "mfc_snd.h"
#include "mfc_inp.h"
#include "mfc_cpu.h"
#include "mfc_sch.h"

//===========================================================================
//
//	�X�P�W���[��
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
CScheduler::CScheduler(CFrmWnd *pFrmWnd) : CComponent(pFrmWnd)
{
	// �R���|�[�l���g�p�����[�^
	m_dwID = MAKEID('S', 'C', 'H', 'E');
	m_strDesc = _T("Scheduler");

	// ���[�N������
	m_pCPU = NULL;
	m_pRender = NULL;
	m_pThread = NULL;
	m_pSound = NULL;
	m_pInput = NULL;
	m_bExitReq = FALSE;
	m_dwExecTime = 0;
	m_nSubWndNum = 0;
	m_nSubWndDisp = -1;
	m_bMPUFull = FALSE;
	m_bVMFull = FALSE;
	m_dwDrawCount = 0;
	m_dwDrawTime = 0;
	m_bMenu = FALSE;
	m_bActivate = TRUE;
	m_bBackup = TRUE;
	m_bSavedValid = FALSE;
	m_bSavedEnable = FALSE;
}

//---------------------------------------------------------------------------
//
//	������
//
//---------------------------------------------------------------------------
BOOL FASTCALL CScheduler::Init()
{
	ASSERT(this);

	// ��{�N���X
	if (!CComponent::Init()) {
		return FALSE;
	}

	// CPU�擾
	ASSERT(!m_pCPU);
	m_pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pCPU);

	// �����_���擾
	ASSERT(!m_pRender);
	m_pRender = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(m_pRender);

	// �T�E���h�R���|�[�l���g������
	ASSERT(!m_pSound);
	m_pSound = (CSound*)SearchComponent(MAKEID('S', 'N', 'D', ' '));
	ASSERT(m_pSound);

	// �C���v�b�g�R���|�[�l���g������
	ASSERT(!m_pInput);
	m_pInput = (CInput*)SearchComponent(MAKEID('I', 'N', 'P', ' '));
	ASSERT(m_pInput);

	// �}���`���f�B�A�^�C�}�[�̎��ԊԊu��1ms�ɐݒ�
	::timeBeginPeriod(1);

	// �X���b�h�𗧂Ă�
	m_pThread = AfxBeginThread(ThreadFunc, this);
	if (!m_pThread) {
		::timeEndPeriod(1);
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�N���[���A�b�v
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::Cleanup()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// ��~
	Stop();

	// �}���`���f�B�A�^�C�}�[�̎��ԊԊu��߂�
	::timeEndPeriod(1);

	// ��{�N���X
	CComponent::Cleanup();
}

//---------------------------------------------------------------------------
//
//	�ݒ�K�p
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::ApplyCfg(const Config *pConfig)
{
	BOOL bFlag;

	ASSERT(this);
	ASSERT_VALID(this);
	ASSERT(pConfig);

	// ��VM�t����ۑ�
	bFlag = m_bVMFull;

	m_bMPUFull = pConfig->mpu_fullspeed;
	m_bVMFull = pConfig->vm_fullspeed;

	// ������烊�Z�b�g
	if (bFlag != m_bVMFull) {
		Reset();
	}
}

#if defined(_DEBUG)
//---------------------------------------------------------------------------
//
//	�f�f
//
//---------------------------------------------------------------------------
void CScheduler::AssertValid() const
{
	ASSERT(this);
	ASSERT(GetID() == MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pCPU);
	ASSERT(m_pCPU->GetID() == MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pRender);
	ASSERT(m_pRender->GetID() == MAKEID('R', 'E', 'N', 'D'));
	ASSERT(m_pSound);
	ASSERT(m_pSound->GetID() == MAKEID('S', 'N', 'D', ' '));
	ASSERT(m_pInput);
	ASSERT(m_pInput->GetID() == MAKEID('I', 'N', 'P', ' '));
}
#endif	// _DEBUG

//---------------------------------------------------------------------------
//
//	���Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::Reset()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// ���Z�b�g
	m_dwExecTime = GetTime();
	m_dwDrawTime = m_dwExecTime;
	m_dwDrawBackup = 0;
	m_dwDrawCount = 0;
	m_dwDrawPrev = 0;
}

//---------------------------------------------------------------------------
//
//	��~
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::Stop()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// �X���b�h���オ���Ă���ꍇ�̂ݏI������
	if (m_pThread) {
		// �I�����N�G�X�g�𗧂Ă�
		m_bExitReq = TRUE;

		// ��~�܂ő҂�
		::WaitForSingleObject(m_pThread->m_hThread, INFINITE);

		// �X���b�h�͏I������
		m_pThread = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL CScheduler::Save(Fileio *pFio, int /*nVer*/)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// �Z�[�u����Enable��Ԃ�ۑ�(version2.04)
	if (!pFio->Write(&m_bSavedEnable, sizeof(m_bSavedEnable))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL CScheduler::Load(Fileio *pFio, int nVer)
{
	ASSERT(this);
	ASSERT(nVer >= 0x0200);
	ASSERT_VALID(this);

	// �Z�[�u����Enable��ԕۑ��͂���Ă��Ȃ����̂Ɖ���
	m_bSavedValid = FALSE;

	// version2.04�ȍ~�ł���΁A���[�h
	if (nVer >= 0x0204) {
		if (!pFio->Read(&m_bSavedEnable, sizeof(m_bSavedEnable))) {
			return FALSE;
		}
		m_bSavedValid = TRUE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�X���b�h�֐�
//
//---------------------------------------------------------------------------
UINT CScheduler::ThreadFunc(LPVOID pParam)
{
	CScheduler *pSch;

	// �p�����[�^���󂯎��
	pSch = (CScheduler*)pParam;
	ASSERT(pSch);
#if defined(_DEBUG)
	pSch->AssertValid();
#endif	// _DEBUG

	// ���s
	pSch->Run();

	// �I���R�[�h�������ăX���b�h���I��
	return 0;
}


//---------------------------------------------------------------------------
//
//	Ejecucion
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::Run()
{
	VM *pVM;
	Scheduler *pScheduler;
	Render *pRender;
	DWORD dwExecCount;
	DWORD dwCycle;
	DWORD dwTime;

	ASSERT(this);
	ASSERT_VALID(this);

	// Adquisicion de VM
	pVM = ::GetVM();
	ASSERT(pVM);
	pScheduler = (Scheduler*)pVM->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(pScheduler);
	pRender = (Render*)pVM->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(pRender);

	// Contador de tiempo
	m_dwExecTime = GetTime();
	dwExecCount = 0;
	m_dwDrawTime = m_dwExecTime;
	m_dwDrawBackup = 0;
	m_dwDrawCount = 0;
	m_dwDrawPrev = 0;
	m_bBackup = m_bEnable;

	//  Bucle hasta que la solicitud de salida se eleva
	while (!m_bExitReq) {
		// Diagnostico permanente
		ASSERT_VALID(this);

		// Sleep
		::Sleep(0);

		// Asegurar
		Lock();

		// Si no se levanta ninguna bandera valida, la maquina se detiene
		if (!m_bEnable) {
			// Volver a dibujar siempre cuando se activa -> se desactiva por puntos de interrupcion, fallo de alimentacion, etc.
			if (m_bBackup) {
				m_pFrmWnd->GetView()->Invalidate(FALSE);
				m_bBackup = FALSE;
			}

			// Dibujo
			Refresh();
			dwExecCount = 0;

			// Procesamiento y coincidencia temporal de otros componentes
			m_pSound->Process(FALSE);
			m_pInput->Process(FALSE);
			m_dwExecTime = GetTime();
			Unlock();

			// Sleep
			::Sleep(10);
			continue;
		}

		// Actualizacion de la bandera de respaldo (se estaba ejecutando justo antes)
		m_bBackup = TRUE;

		// Si la hora se invierte, hay espacio para mostrar
		dwTime = GetTime();
		if (m_dwExecTime > dwTime) {
			// Si hay un hueco importante, se puede ajustar (soporte de bucle de 49 dias).
			if ((m_dwExecTime - dwTime) > 1000) {
				m_dwExecTime = dwTime;
			}

			// Dibujo
			Refresh();
			dwExecCount = 0;

			// Procesamiento en modo rapido
			if (m_dwExecTime > GetTime()) 
			{
				// Hay mucho tiempo
				if (m_bVMFull) {
					// Modo VM r�pido
					m_dwExecTime = GetTime();
				}
				else {
					if (!m_bMPUFull) {
						// Modo normal
						Unlock();
						::Sleep(0);
						continue;
					}
				
					// Modo MPU en r�pido
					dwCycle = pScheduler->GetCPUCycle();
					while (m_dwExecTime > GetTime()) {
						m_pCPU->Exec(pScheduler->GetCPUSpeed());
					}					
					pScheduler->SetCPUCycle(dwCycle);
					Unlock();
					continue;

			       //m_dwExecTime = GetTime(); // Prueba, agilizar ejecuci�n
				   

				}
			}
		}

		// Determinar si es posible el renderizado (1 o 36)
		if (m_dwExecTime >= dwTime) {
			pRender->EnableAct(TRUE);
		}
		else {
			if ((dwTime - m_dwExecTime) <= 1) {
				// ����
				pRender->EnableAct(TRUE);
			}
			else {
				pRender->EnableAct(FALSE);
			}
		}


		/* ACA SE MODIFICAN LOS FPS*/

		// Impulsar la VM
		if (!pVM->Exec(1081 * 2)) {
			// Puntos de interrupcion
			SyncDisasm();
			dwExecCount++;
			m_bEnable = FALSE;
			Unlock();
			continue;
		}

		// Comprobacion de la fuente de alimentacion
		if (!pVM->IsPower()) {
			// Apagado
			SyncDisasm();
			dwExecCount++;
			m_bEnable = FALSE;
			Unlock();
			continue;
		}
		dwExecCount++;
		m_dwExecTime++;

		// Procesamiento de otros componentes
		m_pSound->Process(TRUE);
		m_pInput->Process(TRUE);

		// Si dwExecCount supera el numero especificado, muestra una vez y fuerza el ajuste de tiempo
		if (dwExecCount > 400) 
		{
			Refresh();
			dwExecCount = 0;
			m_dwExecTime = GetTime();
		}

		// Dormir una vez cada 8ms si esta inactivo o el menu ON
		if (!m_bActivate || m_bMenu) {
			if ((m_dwExecTime & 0x07) == 0) {
				Unlock();
				::Sleep(1);
				continue;
			}
		}

		// Desbloquear
		Unlock();
	}
}


//---------------------------------------------------------------------------
//
//	refrescar (por ejemplo, la memoria)
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::Refresh()
{
	int num;
	CDrawView *pView;

	ASSERT(this);
	ASSERT_VALID(this);
	ASSERT(m_pFrmWnd);

	// Consigue la vista.
	pView = m_pFrmWnd->GetView();
	ASSERT(pView);

	//  Obtiene el numero de subventanas.
	num = pView->GetSubWndNum();

	// Reinicia si el numero de piezas es diferente al valor de la memoria.
	if (m_nSubWndNum != num) {
		m_nSubWndNum = num;
		m_nSubWndDisp = -1;
	}

	if (m_bEnable) {
		// Esta en marcha y es el turno de la pantalla principal.
		if (m_nSubWndDisp < 0) {
			// Si el renderizador no esta listo, no dibujara.
			if (!m_pRender->IsReady()) {
				return;
			}
			SyncDisasm();
		}
	}

	// Pantalla (parcial)
	pView->Draw(m_nSubWndDisp);

	// Si se muestra la pantalla principal, cuenta atras.
	if (m_nSubWndDisp < 0) {
		m_pRender->Complete();
		m_dwDrawCount++;
	}

	// Vista de la ciclica
	m_nSubWndDisp++;
	if (m_nSubWndDisp >= m_nSubWndNum) {
		m_nSubWndDisp = -1;
	}
}

//---------------------------------------------------------------------------
//
//	combinacion de PC con ensamblador inverso
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::SyncDisasm()
{
	CDisasmWnd *pWnd;
	CDrawView *pView;
	DWORD dwID;
	int i;

	ASSERT(this);
	ASSERT_VALID(this);

	// Consigue la vista.
	pView = m_pFrmWnd->GetView();
	ASSERT(pView);

	// Hasta un maximo de 8 piezas.
	for (i=0; i<8; i++) {
		dwID = MAKEID('D', 'I', 'S', ('A' + i));
		pWnd = (CDisasmWnd*)pView->SearchSWnd(dwID);
		if (pWnd) {
			// Resultados.
			pWnd->SetPC(m_pCPU->GetPC());
		}
	}
}

//---------------------------------------------------------------------------
//
//	Adquisicion de la velocidad de fotogramas
//
//---------------------------------------------------------------------------
int FASTCALL CScheduler::GetFrameRate()
{
	DWORD dwCount;
	DWORD dwTime;
    DWORD dwDiff;

	ASSERT(this);
	ASSERT_VALID(this);

	// control invalido
	if (!m_bEnable) {
		return 0;
	}

	// Adquisicion de tiempo (soporte de bucle, inicializacion si es largo)
	dwTime = GetTime();
	if (dwTime <= m_dwDrawTime) {
		m_dwDrawTime = dwTime;
		m_dwDrawBackup = 0;
		m_dwDrawCount = 0;
		m_dwDrawPrev = 0;
		return 0;
	}
	dwDiff = dwTime - m_dwDrawTime;
	if (dwDiff > 3500) {
		m_dwDrawTime = dwTime;
		m_dwDrawBackup = 0;
		m_dwDrawCount = 0;
		m_dwDrawPrev = 0;
		return 0;
	}

	// Si la diferencia es inferior a 500 ms, el proceso es normal.
	if (dwDiff < 500) {
		m_dwDrawBackup = 0;
		dwDiff /= 10;
		dwCount = m_dwDrawCount * 1000;
		if (dwDiff == 0) {
			return 0;
		}
		return (dwCount / dwDiff);
	}

	// Memoria si la diferencia es inferior a 1000 ms.
	if (dwDiff < 1000) {
		if (m_dwDrawBackup == 0) {
			m_dwDrawBackup = dwTime;
			m_dwDrawPrev = m_dwDrawCount;
		}
		dwDiff /= 10;
		dwCount = m_dwDrawCount * 1000;
		return (dwCount / dwDiff);
	}

	// Mas que eso, asi que cambia a Prev
	dwDiff /= 10;
	dwCount = m_dwDrawCount * 1000;
	m_dwDrawTime = m_dwDrawBackup;
	m_dwDrawBackup = 0;
	m_dwDrawCount -= m_dwDrawPrev;
	return (dwCount / dwDiff);
}

#endif	// _WIN32
