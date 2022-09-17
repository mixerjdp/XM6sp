//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[Subventana MFC]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "xm6.h"
#include "render.h"
#include "mfc_frm.h"
#include "mfc_draw.h"
#include "mfc_res.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_sub.h"

//===========================================================================
//
//	subventana
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	constructor
//
//---------------------------------------------------------------------------
CSubWnd::CSubWnd()
{
	// objeto
	m_pSch = NULL;
	m_pDrawView = NULL;
	m_pNextWnd = NULL;

	// propiedad
	m_strCaption.Empty();
	m_bEnable = TRUE;
	m_dwID = 0;
	m_bPopup = FALSE;

	// tamano de la ventana
	m_nWidth = -1;
	m_nHeight = -1;

	// fuente de texto
	m_pTextFont = NULL;
	m_tmWidth = -1;
	m_tmHeight = -1;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubWnd, CWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
	ON_WM_ACTIVATE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	inicializacion
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSubWnd::Init(CDrawView *pDrawView)
{
	BOOL bRet;
	CFrmWnd *pFrmWnd;

	ASSERT(this);
	ASSERT(pDrawView);
	ASSERT(m_dwID != 0);

	// Memoria de la vista de dibujo
	ASSERT(!m_pDrawView);
	m_pDrawView = pDrawView;
	ASSERT(m_pDrawView);

	// Adquisicion de la ventana del marco
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);

	// Adquisicion del programador
	ASSERT(!m_pSch);
	m_pSch = pFrmWnd->GetScheduler();
	ASSERT(m_pSch);

	// creacion de ventanas
	if (pFrmWnd->IsPopupSWnd()) {
		// ventana emergente
		m_bPopup = TRUE;
		bRet = CreateEx(0,
					pDrawView->GetWndClassName(),
					m_strCaption,
					WS_POPUP | WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION |
					WS_VISIBLE | WS_MINIMIZEBOX | WS_BORDER,
					0, 0,
					400, 400,
					pDrawView->m_hWnd,
					(HMENU)0,
					0);
	}
	else {
		// ventana hija
		m_bPopup = FALSE;
		bRet = Create(NULL, 
					m_strCaption,
					WS_CHILD | WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION |
					WS_VISIBLE | WS_MINIMIZEBOX | WS_CLIPSIBLINGS,
					CRect(0, 0, 400, 400),
					pDrawView,
					(UINT)m_dwID);
	}

	// Si tiene exito.
	if (bRet) {
		// Registro en la ventana principal
		m_pDrawView->AddSWnd(this);
	}

	return bRet;
}

//---------------------------------------------------------------------------
//
//	creacion de ventanas
//
//---------------------------------------------------------------------------
int CSubWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int nSWnd;
	CRect rectWnd;
	CRect rectParent;
	CPoint point;

	ASSERT(this);
	ASSERT(lpCreateStruct);
	ASSERT(m_nWidth > 0);
	ASSERT(m_nHeight > 0);

	// clase basica
	if (CWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	//  Configuracion de los iconos
	SetIcon(AfxGetApp()->LoadIcon(IDI_XICON), TRUE);

	// IME Off
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Configuracion de la fuente del texto
	m_pTextFont = m_pDrawView->GetTextFont();
	SetupTextFont();

	// Calcular el tamano de ajuste de la ventana
	rectWnd.left = 0;
	rectWnd.top = 0;
	rectWnd.right = m_nWidth * m_tmWidth;
	rectWnd.bottom = m_nHeight* m_tmHeight;
	CalcWindowRect(&rectWnd);

	// Obtener indice (previsto)
	nSWnd = m_pDrawView->GetNewSWnd();

	// Posicion de la ventana determinada a partir del indice.
	m_pDrawView->GetWindowRect(&rectParent);
	point.x = (nSWnd * 24) % (rectParent.Width() - 24);
	point.y = (nSWnd * 24) % (rectParent.Height() - 24);

	if (m_bPopup) {
		// Para los tipos de ventanas emergentes, las coordenadas de la pantalla son la referencia
		point.x += rectParent.left;
		point.y += rectParent.top;
	}

	// Fijar la posicion y el tamano de la ventana 
	SetWindowPos(&wndTop, point.x, point.y,
		rectWnd.right, rectWnd.bottom, 0);

		

	return 0;
}

//---------------------------------------------------------------------------
//
//	eliminacion de ventanas
//
//---------------------------------------------------------------------------
void CSubWnd::OnDestroy()
{
	ASSERT(this);

	// �����~
	Enable(FALSE);

	// �e�E�B���h�E�֒ʒm
	m_pDrawView->DelSWnd(this);

	// ��{�N���X��
	CWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	�E�B���h�E�폜����
//
//---------------------------------------------------------------------------
void CSubWnd::PostNcDestroy()
{
	// �C���^�t�F�[�X�v�f���폜
	ASSERT(this);
	delete this;
}

//---------------------------------------------------------------------------
//
//	�w�i�`��
//
//---------------------------------------------------------------------------
BOOL CSubWnd::OnEraseBkgnd(CDC *pDC)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// �L���Ȃ�A�S�̈�ӔC������
	if (m_bEnable) {
		return TRUE;
	}

	// ��{�N���X��
	return CWnd::OnEraseBkgnd(pDC);
}

//---------------------------------------------------------------------------
//
//	�A�N�e�B�x�[�g
//
//---------------------------------------------------------------------------
void CSubWnd::OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// �|�b�v�A�b�v�E�B���h�E�̂�
	if (m_bPopup) {
		if (nState == WA_INACTIVE) {
			// �|�b�v�A�b�v�E�B���h�E���C���A�N�e�B�u�Ȃ�A�ᑬ���s
			m_pSch->Activate(FALSE);
		}
		else {
			// �|�b�v�A�b�v�E�B���h�E���A�N�e�B�u�Ȃ�A�ʏ���s
			m_pSch->Activate(TRUE);
		}
	}

	// ��{�N���X
	CWnd::OnActivate(nState, pWnd, bMinimized);
}

//---------------------------------------------------------------------------
//
//	���쐧��
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::Enable(BOOL bEnable)
{
	ASSERT(this);

	m_bEnable = bEnable;
}

//---------------------------------------------------------------------------
//
//	�E�B���h�EID���擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL CSubWnd::GetID() const
{
	ASSERT(this);
	ASSERT_VALID(this);

	return m_dwID;
}

//---------------------------------------------------------------------------
//
//	�e�L�X�g�t�H���g�Z�b�g�A�b�v
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::SetupTextFont()
{
	CClientDC dc(this);
	CFont *pFont;
	TEXTMETRIC tm;

	ASSERT(this);

	// �t�H���g�Z���N�g
	ASSERT(m_pTextFont);
	pFont = dc.SelectObject(m_pTextFont);
	ASSERT(pFont);

	// �e�L�X�g���g���b�N���擾
	VERIFY(dc.GetTextMetrics(&tm));

	// �t�H���g��߂�
	VERIFY(dc.SelectObject(pFont));

	// �e�L�X�g�����A�c�����i�[
	m_tmWidth = tm.tmAveCharWidth;
	m_tmHeight = tm.tmHeight + tm.tmExternalLeading;
}

//---------------------------------------------------------------------------
//
//	���b�Z�[�W�X���b�h����X�V
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::Update()
{
	ASSERT(this);
	ASSERT_VALID(this);
}

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSubWnd::Save(Fileio* /*pFio*/, int /*nVer*/)
{
	ASSERT(this);
	ASSERT_VALID(this);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSubWnd::Load(Fileio* /*pFio*/, int /*nVer*/)
{
	ASSERT(this);
	ASSERT_VALID(this);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�ݒ�K�p
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::ApplyCfg(const Config* /*pConfig*/)
{
	ASSERT(this);
	ASSERT_VALID(this);
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	�f�f
//
//---------------------------------------------------------------------------
void CSubWnd::AssertValid() const
{
	ASSERT(this);

	// ���̃I�u�W�F�N�g
	ASSERT(m_pSch);
	ASSERT(m_pSch->GetID() == MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pDrawView);
	ASSERT(m_pDrawView->m_hWnd);
	ASSERT(::IsWindow(m_pDrawView->m_hWnd));

	// �v���p�e�B
	ASSERT(m_strCaption.GetLength() > 0);
	ASSERT(m_dwID != 0);
	ASSERT(m_nWidth > 0);
	ASSERT(m_nHeight > 0);
	ASSERT(m_pTextFont);
	ASSERT(m_pTextFont->m_hObject);
	ASSERT(m_tmWidth > 0);
	ASSERT(m_tmHeight > 0);
}
#endif	// NDEBUG

//===========================================================================
//
//	�T�u�e�L�X�g�E�B���h�E
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
CSubTextWnd::CSubTextWnd()
{
	// �����o�ϐ�������
	m_bReverse = FALSE;
	m_pTextBuf = NULL;
	m_pDrawBuf = NULL;
}

//---------------------------------------------------------------------------
//
//	���b�Z�[�W �}�b�v
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubTextWnd, CSubWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_PAINT()
	ON_WM_SIZE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	�E�B���h�E�쐬
//
//---------------------------------------------------------------------------
int CSubTextWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	ASSERT(this);
	ASSERT(!m_pTextBuf);
	ASSERT(m_nWidth > 0);
	ASSERT(m_nHeight > 0);

	// �e�L�X�g�o�b�t�@�m��
	m_pTextBuf = new BYTE[m_nWidth * m_nHeight];
	ASSERT(m_pTextBuf);

	// �`��o�b�t�@�m�ہA������
	m_pDrawBuf = new BYTE[m_nWidth * m_nHeight];
	ASSERT(m_pDrawBuf);
	memset(m_pDrawBuf, 0xff, m_nWidth * m_nHeight);

	// ��{�N���X(������DrawView�w�ʒm)
	if (CSubWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	�E�B���h�E�폜
//
//---------------------------------------------------------------------------
void CSubTextWnd::OnDestroy()
{
	ASSERT(this);

	// ��{�N���X���Ɏ��s����(DrawView�ւ̒ʒm���}������)
	CSubWnd::OnDestroy();

	// �e�L�X�g�o�b�t�@���
	ASSERT(m_pTextBuf);
	if (m_pTextBuf) {
		delete[] m_pTextBuf;
		m_pTextBuf = NULL;
	}

	// �`��o�b�t�@���
	ASSERT(m_pDrawBuf);
	if (m_pDrawBuf) {
		delete[] m_pDrawBuf;
		m_pDrawBuf = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	�`��
//
//---------------------------------------------------------------------------
void CSubTextWnd::OnPaint()
{
	PAINTSTRUCT ps;

	// VM�����b�N
	::LockVM();

    BeginPaint(&ps);

	// �e�L�X�g�o�b�t�@�A�L���t���O�`�F�b�N
	if (m_pTextBuf && m_bEnable) {
		// �`��o�b�t�@��FF�Ŗ��߂�
		memset(m_pDrawBuf, 0xff, m_nWidth * m_nHeight);

		// �`��͍s��Ȃ�(VM�X���b�h�����Refresh�ɔC����)
	}

	EndPaint(&ps);

	// VM�A�����b�N
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	�T�C�Y�ύX
//
//---------------------------------------------------------------------------
void CSubTextWnd::OnSize(UINT nType, int cx, int cy)
{
	int nWidth;
	int nHeight;
	CRect rectClient;

	ASSERT(this);
	ASSERT(m_tmWidth > 0);
	ASSERT(m_tmHeight > 0);

	// �L�����N�^������̃N���C�A���g�T�C�Y���v�Z
	GetClientRect(&rectClient);
	nWidth = rectClient.Width() / m_tmWidth;
	nHeight = rectClient.Height() / m_tmHeight;

	// ��v���Ă��Ȃ���
	if ((nWidth != m_nWidth) || (nHeight != m_nHeight)) {
		// �ŏ����łȂ���
		if (nType != SIZE_MINIMIZED) {
			// �e�L�X�g�o�b�t�@�������
			if (m_pTextBuf && m_pDrawBuf) {
				// �]�T������΁A+1
				if ((nWidth * m_tmWidth) < rectClient.Width()) {
					nWidth++;
				}
				if ((nHeight * m_tmWidth) < rectClient.Height()) {
					nHeight++;
				}

				// �o�b�t�@�ύX(VM���b�N������)
				::LockVM();
				ASSERT(m_pTextBuf);
				if (m_pTextBuf) {
					delete[] m_pTextBuf;
					m_pTextBuf = NULL;
				}
				ASSERT(m_pDrawBuf);
				if (m_pDrawBuf) {
					delete[] m_pDrawBuf;
					m_pDrawBuf = NULL;
				}
				m_pTextBuf = new BYTE[nWidth * nHeight];
				m_pDrawBuf = new BYTE[nWidth * nHeight];
				m_nWidth = nWidth;
				m_nHeight = nHeight;
				::UnlockVM();
			}
		}
	}

	// �ĕ`���}��
	Invalidate(FALSE);

	// ��{�N���X��
	CSubWnd::OnSize(nType, cx, cy);
}

//---------------------------------------------------------------------------
//
//	���t���b�V���`��
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::Refresh()
{
	CClientDC dc(this);

	ASSERT(this);

	// �e�L�X�g�o�b�t�@�A�L���t���O�`�F�b�N
	if (m_pTextBuf && m_bEnable) {
		// �Z�b�g�A�b�v
		Setup();

		// �`��
		OnDraw(&dc);
	}
}

//---------------------------------------------------------------------------
//
//	�`�惁�C��
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::OnDraw(CDC *pDC)
{
	CFont *pFont;
	BOOL bReverse;
	BYTE *pText;
	BYTE *pDraw;
	CPoint point;
	int x;
	int y;
	BYTE ch;

	ASSERT(this);
	ASSERT(pDC);
	ASSERT(m_pTextBuf);
	ASSERT(m_pDrawBuf);
	ASSERT(m_pTextFont);
	ASSERT(m_nWidth >= 0);
	ASSERT(m_nHeight >= 0);

	// �t�H���g�w��
	pFont = pDC->SelectObject(m_pTextFont);
	ASSERT(pFont);

	// �F�w��
	pDC->SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
	pDC->SetBkColor(::GetSysColor(COLOR_WINDOW));
	bReverse = FALSE;

	// �|�C���^�A���W������
	pText = m_pTextBuf;
	pDraw = m_pDrawBuf;
	point.y = 0;

	// y���[�v
	for (y=0; y<m_nHeight; y++) {
		point.x = 0;
		for (x=0; x<m_nWidth; x++) {
			// ��v�`�F�b�N
			if (*pText == *pDraw) {
				pText++;
				pDraw++;
				point.x += m_tmWidth;
				continue;
			}

			// �R�s�[
			ch = *pText++;
			*pDraw++ = ch;

			// ���]�`�F�b�N
			if (ch >= 0x80) {
				ch &= 0x7f;
				if (!bReverse) {
					pDC->SetTextColor(::GetSysColor(COLOR_WINDOW));
					pDC->SetBkColor(::GetSysColor(COLOR_WINDOWTEXT));
					bReverse = TRUE;
				}
			}
			else {
				if (bReverse) {
					pDC->SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
					pDC->SetBkColor(::GetSysColor(COLOR_WINDOW));
					bReverse = FALSE;
				}
			}

			// �e�L�X�g�A�E�g
			pDC->TextOut(point.x, point.y, (LPCTSTR)&ch ,1);
			point.x += m_tmWidth;
		}
		point.y += m_tmHeight;
	}

	// �t�H���g�w�����
	VERIFY(pDC->SelectObject(pFont));
}

//---------------------------------------------------------------------------
//
//	�N���A
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::Clear()
{
	ASSERT(this);
	ASSERT(m_pTextBuf);
	ASSERT(m_nWidth >= 0);
	ASSERT(m_nHeight >= 0);

	// �N���A
	if (m_pTextBuf) {
		memset(m_pTextBuf, 0x20, m_nWidth * m_nHeight);
	}

	// ���]�I�t
	Reverse(FALSE);
}

//---------------------------------------------------------------------------
//
//	�����Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::SetChr(int x, int y, TCHAR ch)
{
	ASSERT(this);
	ASSERT(m_pTextBuf);
	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(_istprint(ch));

	// �L���͈͂��`�F�b�N
	if ((x < m_nWidth) && (y < m_nHeight)) {
		// ��������
		if (m_bReverse) {
			m_pTextBuf[(y * m_nWidth) + x] = (BYTE)(ch | 0x80);
		}
		else {
			m_pTextBuf[(y * m_nWidth) + x] = ch;
		}
	}
}

//---------------------------------------------------------------------------
//
//	������Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::SetString(int x, int y, LPCTSTR lpszText)
{
	ASSERT(this);
	ASSERT(m_pTextBuf);
	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(lpszText);

	// '\0'�܂Ń��[�v
	while (_istprint(*lpszText)) {
		SetChr(x, y, *lpszText);

		lpszText++;
		x++;
	}
}

//---------------------------------------------------------------------------
//
//	���]�Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::Reverse(BOOL bReverse)
{
	ASSERT(this);
	ASSERT(m_pTextBuf);

	m_bReverse = bReverse;
}

//---------------------------------------------------------------------------
//
//	���T�C�Y
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::ReSize(int nWidth, int nHeight)
{
	CRect rectWnd;
	CRect rectNow;

	ASSERT(this);
	ASSERT(nWidth > 0);
	ASSERT(nHeight > 0);

	// ��v�`�F�b�N
	if ((nWidth == m_nWidth) && (nHeight == m_nHeight)) {
		return;
	}

	// �o�b�t�@���Ȃ���Ή������Ȃ�
	if (!m_pTextBuf || !m_pDrawBuf) {
		return;
	}

	// �T�C�Y���Z�o
	rectWnd.left = 0;
	rectWnd.top = 0;
	rectWnd.right = nWidth * m_tmWidth;
	rectWnd.bottom = nHeight * m_tmHeight;
	CalcWindowRect(&rectWnd);

	// ���݂Ɠ����Ȃ牽�����Ȃ�
	GetWindowRect(&rectNow);
	if ((rectNow.Width() == rectWnd.Width()) && (rectNow.Height() == rectWnd.Height())) {
		return;
	}

	// �T�C�Y�ύX
	SetWindowPos(&wndTop, 0, 0, rectWnd.Width(), rectWnd.Height(), SWP_NOMOVE);
}

//===========================================================================
//
//	�T�u�e�L�X�g�σE�B���h�E
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
CSubTextSizeWnd::CSubTextSizeWnd()
{
}

//---------------------------------------------------------------------------
//
//	�E�B���h�E�쐬����
//
//---------------------------------------------------------------------------
BOOL CSubTextSizeWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// ��{�N���X
	if (!CSubTextWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// �T�C�Y�ρA�ő剻�E
	cs.style |= WS_THICKFRAME;
	cs.style |= WS_MAXIMIZEBOX;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�`�惁�C��
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextSizeWnd::OnDraw(CDC *pDC)
{
	CRect rect;

	ASSERT(this);
	ASSERT(pDC);

	// �N���C�A���g��`���擾
	GetClientRect(&rect);

	// �E�����N���A
	rect.left = m_nWidth * m_tmWidth;
	pDC->FillSolidRect(&rect, GetSysColor(COLOR_WINDOW));

	// �������N���A
	rect.left = 0;
	rect.top = m_nHeight * m_tmHeight;
	pDC->FillSolidRect(&rect, GetSysColor(COLOR_WINDOW));

	// ��{�N���X��
	CSubTextWnd::OnDraw(pDC);
}

//===========================================================================
//
//	�T�u�e�L�X�g�X�N���[���E�B���h�E
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
CSubTextScrlWnd::CSubTextScrlWnd()
{
	// �����o�ϐ�������
	m_ScrlWidth = -1;
	m_ScrlHeight = -1;
	m_bScrlH = FALSE;
	m_bScrlV = FALSE;
	m_ScrlX = 0;
	m_ScrlY = 0;
}

//---------------------------------------------------------------------------
//
//	���b�Z�[�W �}�b�v
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubTextScrlWnd, CSubTextSizeWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	�E�B���h�E�쐬����
//
//---------------------------------------------------------------------------
BOOL CSubTextScrlWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// ��{�N���X
	if (!CSubTextSizeWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// �X�N���[���o�[��ǉ�
	cs.style |= WS_HSCROLL;
	cs.style |= WS_VSCROLL;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�E�B���h�E�쐬
//
//---------------------------------------------------------------------------
int CSubTextScrlWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	CRect wrect;
	CRect crect;

	ASSERT(this);

	// ��{�N���X
	if (CSubTextSizeWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

	// �X�N���[������
	ShowScrollBar(SB_BOTH, FALSE);
	SetupScrl();

	// �T�C�Y����
	GetWindowRect(&wrect);
	GetClientRect(&crect);
	wrect.right -= wrect.left;
	wrect.right -= crect.right;
	wrect.right += m_ScrlWidth * m_tmWidth;
	SetWindowPos(&wndTop, 0, 0, wrect.right, wrect.Height(),
						SWP_NOMOVE | SWP_NOZORDER);

	return 0;
}

//---------------------------------------------------------------------------
//
//	�T�C�Y�ύX
//
//---------------------------------------------------------------------------
void CSubTextScrlWnd::OnSize(UINT nType, int cx, int cy)
{
	ASSERT(this);

	// ��{�N���X���Ɏ��s
	CSubTextSizeWnd::OnSize(nType, cx, cy);

	// �X�N���[������
	SetupScrl();
}

//---------------------------------------------------------------------------
//
//	�X�N���[������
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextScrlWnd::SetupScrl()
{
	CRect rect;
	int width;
	int height;

	ASSERT(this);
	ASSERT(m_ScrlWidth >= 0);
	ASSERT(m_ScrlHeight >= 0);
	ASSERT(m_tmWidth > 0);
	ASSERT(m_tmHeight > 0);

	// �N���C�A���g�T�C�Y���擾
	GetClientRect(&rect);

	// �ŏ����Ȃ牽�����Ȃ�
	if ((rect.right == 0) || (rect.bottom == 0)) {
		return;
	}

	// H�X�N���[���𔻒�
	width = rect.right / m_tmWidth;
	if (width < m_ScrlWidth) {
		if (!m_bScrlH) {
			m_bScrlH = TRUE;
			m_ScrlX = 0;
			ShowScrollBar(SB_HORZ, TRUE);
		}
		// �X�N���[���o�[���K�v�Ȃ̂ŁA�ڍאݒ�
		SetupScrlH();
	}
	else {
		if (m_bScrlH) {
			m_bScrlH = FALSE;
			m_ScrlX = 0;
			ShowScrollBar(SB_HORZ, FALSE);
		}
	}

	// V�X�N���[���𔻒�
	height = rect.bottom / m_tmHeight;
	if (height < m_ScrlHeight) {
		if (!m_bScrlV) {
			m_bScrlV = TRUE;
			m_ScrlY = 0;
			ShowScrollBar(SB_VERT, TRUE);
		}
		// �X�N���[���o�[���K�v�Ȃ̂ŁA�ڍאݒ�
		SetupScrlV();
	}
	else {
		if (m_bScrlV) {
			m_bScrlV = FALSE;
			m_ScrlY = 0;
			ShowScrollBar(SB_VERT, FALSE);
		}
	}
}

//---------------------------------------------------------------------------
//
//	�X�N���[������(����)
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextScrlWnd::SetupScrlH()
{
	SCROLLINFO si;
	CRect rect;
	int width;

	ASSERT(this);

	// �����\���\�L�����N�^���擾
	GetClientRect(&rect);
	width = rect.right / m_tmWidth;

	// �X�N���[�������Z�b�g
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_ScrlWidth - 1;
	si.nPage = width;

	// �ʒu�́A�K�v�Ȃ�␳����
	si.nPos = m_ScrlX;
	if (si.nPos + width > m_ScrlWidth) {
		si.nPos = m_ScrlWidth - width;
		if (si.nPos < 0) {
			si.nPos = 0;
		}
		m_ScrlX = si.nPos;
	}

	SetScrollInfo(SB_HORZ, &si, TRUE);
}

//---------------------------------------------------------------------------
//
//	�X�N���[��(����)
//
//---------------------------------------------------------------------------
void CSubTextScrlWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar * /*pBar*/)
{
	SCROLLINFO si;

	ASSERT(this);

	// �X�N���[�������擾
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_HORZ, &si, SIF_ALL);

	// �X�N���[���o�[�R�[�h��
	switch (nSBCode) {
		// ����
		case SB_LEFT:
			m_ScrlX = si.nMin;
			break;

		// �E��
		case SB_RIGHT:
			m_ScrlX = si.nMax;
			break;

		// 1���C������
		case SB_LINELEFT:
			if (m_ScrlX > 0) {
				m_ScrlX--;
			}
			break;

		// 1���C���E��
		case SB_LINERIGHT:
			if (m_ScrlX < si.nMax) {
				m_ScrlX++;
			}
			break;

		// 1�y�[�W����
		case SB_PAGELEFT:
			if (m_ScrlX >= (int)si.nPage) {
				m_ScrlX -= (int)si.nPage;
			}
			else {
				m_ScrlX = 0;
			}
			break;

		// 1�y�[�W�E��
		case SB_PAGERIGHT:
			if ((m_ScrlX + (int)si.nPage) <= (int)si.nMax) {
				m_ScrlX += si.nPage;
			}
			else {
				m_ScrlX = si.nMax;
			}
			break;

		// �T���ړ�
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			m_ScrlX = nPos;
			break;
	}

	// ���ɕ␳
	if (m_ScrlX < 0) {
		m_ScrlX = 0;
	}

	// �Z�b�g
	SetupScrlH();
}

//---------------------------------------------------------------------------
//
//	�X�N���[������(����)
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextScrlWnd::SetupScrlV()
{
	SCROLLINFO si;
	CRect rect;
	int height;

	ASSERT(this);

	// �����\���\�L�����N�^���擾
	GetClientRect(&rect);
	height = rect.bottom / m_tmHeight;

	// �X�N���[�������Z�b�g
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_ScrlHeight - 1;
	si.nPage = height;

	// �ʒu�́A�K�v�Ȃ�␳����
	si.nPos = m_ScrlY;
	if (si.nPos + height > m_ScrlHeight) {
		si.nPos = m_ScrlHeight - height;
		if (si.nPos < 0) {
			si.nPos = 0;
		}
		m_ScrlY = si.nPos;
	}

	SetScrollInfo(SB_VERT, &si, TRUE);
}

//---------------------------------------------------------------------------
//
//	�X�N���[��(����)
//
//---------------------------------------------------------------------------
void CSubTextScrlWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar * /*pBar*/)
{
	SCROLLINFO si;

	ASSERT(this);

	// �X�N���[�������擾
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_VERT, &si, SIF_ALL);

	// �X�N���[���o�[�R�[�h��
	switch (nSBCode) {
		// ���
		case SB_TOP:
			m_ScrlY = si.nMin;
			break;

		// ����
		case SB_BOTTOM:
			m_ScrlY = si.nMax;
			break;

		// 1���C�����
		case SB_LINEUP:
			if (m_ScrlY > 0) {
				m_ScrlY--;
			}
			break;

		// 1���C������
		case SB_LINEDOWN:
			if (m_ScrlY < si.nMax) {
				m_ScrlY++;
			}
			break;

		// 1�y�[�W���
		case SB_PAGEUP:
			if (m_ScrlY >= (int)si.nPage) {
				m_ScrlY -= si.nPage;
			}
			else {
				m_ScrlY = 0;
			}
			break;

		// 1�y�[�W����
		case SB_PAGEDOWN:
			if ((m_ScrlY + (int)si.nPage) <= (int)si.nMax) {
				m_ScrlY += si.nPage;
			}
			else {
				m_ScrlY = si.nMax;
			}
			break;

		// �T���ړ�
		case SB_THUMBPOSITION:
			m_ScrlY = nPos;
			break;
		case SB_THUMBTRACK:
			m_ScrlY = nPos;
			break;
	}

	// ���ɕ␳
	if (m_ScrlY < 0) {
		m_ScrlY = 0;
	}

	// �Z�b�g�A�`��
	SetupScrlV();
}

//===========================================================================
//
//	�T�u���X�g�E�B���h�E
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
CSubListWnd::CSubListWnd()
{
	// ���X�g�R���g���[���̏������I���܂�Disable���Ă���
	m_bEnable = FALSE;
}

//---------------------------------------------------------------------------
//
//	���b�Z�[�W �}�b�v
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubListWnd, CSubWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_DRAWITEM()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	�E�B���h�E�쐬����
//
//---------------------------------------------------------------------------
BOOL CSubListWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// ��{�N���X
	if (!CSubWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// �E�B���h�E�T�C�Y���ςƂ���
	cs.style |= WS_THICKFRAME;
	cs.style |= WS_MAXIMIZEBOX;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�E�B���h�E�쐬
//
//---------------------------------------------------------------------------
int CSubListWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	CRect rect;
	CDC *pDC;
	TEXTMETRIC tm;

	// ��{�N���X
	if (CSubWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

	// ���X�g�R���g���[�����쐬
	VERIFY(m_ListCtrl.Create(WS_CHILD | WS_VISIBLE |
							LVS_REPORT | LVS_NOSORTHEADER| LVS_SINGLESEL |
						 	LVS_OWNERDRAWFIXED | LVS_OWNERDATA,
							CRect(0, 0, 0, 0), this, 0));

	// �t�H���g�T�C�Y����蒼��
	pDC = m_ListCtrl.GetDC();
	pDC->GetTextMetrics(&tm);
	m_ListCtrl.ReleaseDC(pDC);
	m_tmWidth = tm.tmAveCharWidth;
	m_tmHeight = tm.tmHeight + tm.tmExternalLeading;

	// ������
	InitList();

	// �L���ɂ��āA���t���b�V��
	m_bEnable = TRUE;
	Refresh();

	return 0;
}

//---------------------------------------------------------------------------
//
//	�T�C�Y�ύX
//
//---------------------------------------------------------------------------
void CSubListWnd::OnSize(UINT nType, int cx, int cy)
{
	CRect rect;

	ASSERT(this);

	// ��{�N���X
	CSubWnd::OnSize(nType, cx, cy);

	// �N���C�A���g�G���A��t�ɂȂ�悤�ɁA���X�g�R���g���[���𒲐�
	if (m_ListCtrl.GetSafeHwnd()) {
		GetClientRect(&rect);
		m_ListCtrl.SetWindowPos(&wndTop, 0, 0, rect.right, rect.bottom, SWP_NOZORDER);
	}
}

//---------------------------------------------------------------------------
//
//	�I�[�i�[�h���[
//
//---------------------------------------------------------------------------
void CSubListWnd::OnDrawItem(int /*nID*/, LPDRAWITEMSTRUCT /*lpDIS*/)
{
	// �K���h���N���X�Œ�`���邱��
	ASSERT(FALSE);
}

//===========================================================================
//
//	Ventana Sub BMP
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	constructor
//
//---------------------------------------------------------------------------
CSubBMPWnd::CSubBMPWnd()
{
	// sin mapa de bits
	memset(&m_bmi, 0, sizeof(m_bmi));
	m_bmi.biSize = sizeof(BITMAPINFOHEADER);
	m_pBits = NULL;
	m_hBitmap = NULL;

	//100% de ampliacion
	m_nMul = 2;

	// Tamano de la pantalla virtual
	m_nScrlWidth = -1;
	m_nScrlHeight = -1;

	//Desplazamiento
	m_nScrlX = 0;
	m_nScrlY = 0;

	// cursor del raton
	m_nCursorX = -1;
	m_nCursorY = -1;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubBMPWnd, CWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	creacion de ventanas
//
//---------------------------------------------------------------------------
int CSubBMPWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	ASSERT(this);

	// clase basica
	if (CWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// IME Off
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Barras de desplazamiento disponibles
	ShowScrollBar(SB_HORZ, TRUE);
	ShowScrollBar(SB_VERT, TRUE);

	return 0;
}

//---------------------------------------------------------------------------
//
//  eliminacion de la ventana
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnDestroy()
{
	ASSERT(this);
	ASSERT_VALID(this);

	//  Eliminacion de mapas de bits
	if (m_hBitmap) {
		::DeleteObject(m_hBitmap);
		m_hBitmap = NULL;
		ASSERT(m_pBits);
		m_pBits = NULL;
	}

	// clase basica
	CWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	Eliminacion de la ventana completada.
//
//---------------------------------------------------------------------------
void CSubBMPWnd::PostNcDestroy()
{
	ASSERT(this);
	ASSERT(!m_hBitmap);
	ASSERT(!m_pBits);

	// Eliminar elementos de la interfaz.
	delete this;
}

//---------------------------------------------------------------------------
//
//	Cambio de tamano de las ventanas
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnSize(UINT nType, int cx, int cy)
{
	CClientDC dc(this);

	ASSERT(this);

	// clase basica
	CWnd::OnSize(nType, cx, cy);

	// inicializacion del raton
	m_nCursorX = -1;
	m_nCursorY = -1;

	// bloquear
	::LockVM();

	// Si tienes un mapa de bits, liberalo una vez
	if (m_hBitmap) {
		::DeleteObject(m_hBitmap);
		m_hBitmap = NULL;
		ASSERT(m_pBits);
		m_pBits = NULL;
	}

	// Regresar si se minimiza
	if (nType == SIZE_MINIMIZED) {
		::UnlockVM();
		return;
	}

	// Pulse cx,cy hasta el area de desplazamiento.
	cx = (cx * 2) / m_nMul;
	if (cx >= m_nScrlWidth) {
		cx = m_nScrlWidth;
	}
	cy = (cy * 2) / m_nMul;
	if (cy >= m_nScrlHeight) {
		cy = m_nScrlHeight;
	}

	// Creacion de mapas de bits (32 bits, igual tamano)
	m_bmi.biWidth = cx;
	m_bmi.biHeight = -cy;
	m_bmi.biPlanes = 1;
	m_bmi.biBitCount = 32;
	m_bmi.biSizeImage = cx * cy * 4;
	m_hBitmap = ::CreateDIBSection(dc.m_hDC, (BITMAPINFO*)&m_bmi,
						DIB_RGB_COLORS, (void**)&m_pBits, NULL, 0);

	// Inicializar (rellenar en negro)
	if (m_hBitmap) {
		memset(m_pBits, 0, m_bmi.biSizeImage);
	}

	// Ajustes de la barra de desplazamiento
	SetupScrlH();
	SetupScrlV();

	// desbloquear
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	dibujo de fondo
//
//---------------------------------------------------------------------------
BOOL CSubBMPWnd::OnEraseBkgnd(CDC* /*pDC*/)
{
	// Nada.
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	redibujar
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnPaint()
{
	PAINTSTRUCT ps;

	// Solo se detiene para Windows.
	BeginPaint(&ps);
	EndPaint(&ps);
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	diagnostico
//
//---------------------------------------------------------------------------
void CSubBMPWnd::AssertValid() const
{
	ASSERT(this);
	ASSERT(m_hWnd);
	ASSERT(::IsWindow(m_hWnd));
	ASSERT(m_nMul >= 1);
	ASSERT(m_nScrlWidth > 0);
	ASSERT(m_nScrlHeight > 0);
	ASSERT(m_nScrlX >= 0);
	ASSERT(m_nScrlX < m_nScrlWidth);
	ASSERT(m_nScrlY >= 0);
	ASSERT(m_nScrlY < m_nScrlHeight);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Desplazamiento listo (horizontal)
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::SetupScrlH()
{
	SCROLLINFO si;
	CRect rect;

	ASSERT(this);
	ASSERT_VALID(this);

	// Si no hay mapa de bits, no lo haremos.
	if (!m_hBitmap) {
		return;
	}

	// Establecer la informacion de desplazamiento.
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_nScrlWidth - 1;
	si.nPage = m_bmi.biWidth;

	// La posicion se corrige si es necesario.
	si.nPos = m_nScrlX;
	if (si.nPos + (int)si.nPage >= m_nScrlWidth) {
		si.nPos = m_nScrlWidth - (int)si.nPage;
	}
	if (si.nPos < 0) {
		si.nPos = 0;
	}
	m_nScrlX = si.nPos;
	ASSERT((m_nScrlX >= 0) && (m_nScrlX < m_nScrlWidth));

	// configuracion (de un ordenador o archivo, etc.)
	SetScrollInfo(SB_HORZ, &si, TRUE);
}

//---------------------------------------------------------------------------
//
//	Desplazamiento (horizontal)
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pBar*/)
{
	SCROLLINFO si;

	ASSERT(this);
	ASSERT_VALID(this);

	// Obtenga informacion sobre el desplazamiento.
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_HORZ, &si, SIF_ALL);

	// Por codigo de barras de desplazamiento
	switch (nSBCode) {
		// A la izquierda.
		case SB_LEFT:
			m_nScrlX = si.nMin;
			break;

		// A la derecha.
		case SB_RIGHT:
			m_nScrlX = si.nMax;
			break;

		// Una linea a la izquierda.
		case SB_LINELEFT:
			if (m_nScrlX > 0) {
				m_nScrlX--;
			}
			break;

		// Una linea a la derecha.
		case SB_LINERIGHT:
			if (m_nScrlX < si.nMax) {
				m_nScrlX++;
			}
			break;

		// Pagina 1, izquierda.
		case SB_PAGELEFT:
			if (m_nScrlX >= (int)si.nPage) {
				m_nScrlX -= (int)si.nPage;
			}
			else {
				m_nScrlX = 0;
			}
			break;

		// Pagina 1 derecha.
		case SB_PAGERIGHT:
			if ((m_nScrlX + (int)si.nPage) <= si.nMax) {
				m_nScrlX += (int)si.nPage;
			}
			else {
				m_nScrlX = si.nMax;
			}
			break;

		// cambio de pulgar
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			m_nScrlX = nPos;
			break;
	}
	ASSERT((m_nScrlX >= 0) && (m_nScrlX < m_nScrlWidth));

	// set
	SetupScrlH();
}

//---------------------------------------------------------------------------
//
//	Preparado para el desplazamiento (vertical)
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::SetupScrlV()
{
	SCROLLINFO si;

	ASSERT(this);
	ASSERT_VALID(this);

	// Si no hay mapa de bits, no lo haremos.
	if (!m_hBitmap) {
		return;
	}

	// Establecer la informacion de desplazamiento.
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_nScrlHeight - 1;
	si.nPage = -m_bmi.biHeight;

	// La posicion se corrige si es necesario.
	si.nPos = m_nScrlY;
	if (si.nPos + (int)si.nPage >= m_nScrlHeight) {
		si.nPos = m_nScrlHeight - (int)si.nPage;
	}
	if (si.nPos < 0) {
		si.nPos = 0;
	}
	m_nScrlY = si.nPos;
	ASSERT((m_nScrlY >= 0) && (m_nScrlY <= m_nScrlHeight));

	SetScrollInfo(SB_VERT, &si, TRUE);
}

//---------------------------------------------------------------------------
//
//	Desplazamiento (vertical)
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pBar*/)
{
	SCROLLINFO si;

	ASSERT(this);
	ASSERT_VALID(this);

	// Obtenga informacion sobre el desplazamiento.
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_VERT, &si, SIF_ALL);

	// Por codigo de barras de desplazamiento
	switch (nSBCode) {
		// Hacia arriba.
		case SB_TOP:
			m_nScrlY = si.nMin;
			break;

		// abajo
		case SB_BOTTOM:
			m_nScrlY = si.nMax;
			break;

		// Una linea.
		case SB_LINEUP:
			if (m_nScrlY > 0) {
				m_nScrlY--;
			}
			break;

		// Una linea menos.
		case SB_LINEDOWN:
			if (m_nScrlY < si.nMax) {
				m_nScrlY++;
			}
			break;

		// Pagina 1 DE 1
		case SB_PAGEUP:
			if (m_nScrlY >= (int)si.nPage) {
				m_nScrlY -= (int)si.nPage;
			}
			else {
				m_nScrlY = 0;
			}
			break;

		// Pagina 1 abajo.
		case SB_PAGEDOWN:
			if ((m_nScrlY + (int)si.nPage) <= si.nMax) {
				m_nScrlY += (int)si.nPage;
			}
			else {
				m_nScrlY = si.nMax;
			}
			break;

		// cambio de pulgar
		case SB_THUMBPOSITION:
			m_nScrlY = nPos;
			break;
		case SB_THUMBTRACK:
			m_nScrlY = nPos;
			break;
	}

	ASSERT((m_nScrlY >= 0) && (m_nScrlY <= m_nScrlHeight));

	// set
	SetupScrlV();
}

//---------------------------------------------------------------------------
//
//	moviendo el raton
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Memoria de la posicion del movimiento del raton
	m_nCursorX = point.x;
	m_nCursorY = point.y;

	// Consideracion del aumento
	m_nCursorX = (m_nCursorX * 2) / m_nMul;
	m_nCursorY = (m_nCursorY * 2) / m_nMul;

	// clase basica
	CWnd::OnMouseMove(nFlags, point);
}

//---------------------------------------------------------------------------
//
//	dibujo
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::Refresh(int nWidth, int nHeight)
{
	CClientDC dc(this);
	CDC mDC;
	HBITMAP hBitmap;	
	CRect Rect;


	ASSERT(this);
	ASSERT_VALID(this);

	// Solo si hay un mapa de bits disponible.
	if (m_hBitmap) {
		// Creacion de la memoria DC
		mDC.CreateCompatibleDC(&dc);
		
		// seleccion de objetos
		hBitmap = (HBITMAP)::SelectObject(mDC.m_hDC, m_hBitmap);
		
		// BitBlt or StretchBlt
		if (hBitmap) {
			if (m_nMul == 2) {
				// igual tamano
			    //	dc.BitBlt(0, 0, m_bmi.biWidth, -m_bmi.biHeight,
				//					&mDC, 0, 0, SRCCOPY);
				
				int bmibiwidth =  m_bmi.biWidth;
				int bmibiheight = -m_bmi.biHeight;
							
				
				// Stretchblt: nHeight y nWidth son par�metros del tama�o exacto del bitmap origen en la resolucion origen

				dc.StretchBlt(0, 0,
					bmibiwidth,
					bmibiheight,
					&mDC,
					0, 0,
					nHeight, nWidth,
					SRCCOPY);

							

				/*CString sz;
				sz.Format(_T("info.nWidth:%d   info.nHeight:%d\r\n"), nWidth, nHeight);
				OutputDebugStringW(CT2W(sz));*/

			}
			else {
				// n veces
				dc.StretchBlt(  0, 0,
								(m_bmi.biWidth * m_nMul) >> 1,
								-((m_bmi.biHeight * m_nMul) >> 1),
								&mDC,
								0, 0,
								m_bmi.biWidth, -m_bmi.biHeight,
								SRCCOPY);
			}

			// Fin de la seleccion de objetos
			::SelectObject(mDC.m_hDC, hBitmap);
		}

		// Fin de la memoria DC
		mDC.DeleteDC();
	}
}



void FASTCALL CSubBMPWnd::Refresh()
{
	CClientDC dc(this);
	CDC mDC;
	HBITMAP hBitmap;

	ASSERT(this);
	ASSERT_VALID(this);

	// �r�b�g�}�b�v������ꍇ�Ɍ���
	if (m_hBitmap) {
		// ������DC�쐬
		mDC.CreateCompatibleDC(&dc);

		// �I�u�W�F�N�g�I��
		hBitmap = (HBITMAP)::SelectObject(mDC.m_hDC, m_hBitmap);

		// BitBlt or StretchBlt
		if (hBitmap) {
			if (m_nMul == 2) {
				// ���{
				dc.BitBlt(0, 0, m_bmi.biWidth, -m_bmi.biHeight,
					&mDC, 0, 0, SRCCOPY);
			}
			else {
				// n�{
				dc.StretchBlt(0, 0,
					(m_bmi.biWidth * m_nMul) >> 1,
					-((m_bmi.biHeight * m_nMul) >> 1),
					&mDC,
					0, 0,
					m_bmi.biWidth, -m_bmi.biHeight,
					SRCCOPY);
			}

			// �I�u�W�F�N�g�I���I��
			::SelectObject(mDC.m_hDC, hBitmap);
		}

		// ������DC�I��
		mDC.DeleteDC();
	}
}


//---------------------------------------------------------------------------
//
//	Adquisicion del maximo rectangulo de la ventana
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::GetMaximumRect(LPRECT lpRect, BOOL bScroll)
{
	ASSERT(this);
	ASSERT(lpRect);
	ASSERT_VALID(this);

	// Obtiene el tamano de la ventana BMP al maximo.
	lpRect->left = 0;
	lpRect->top = 0;
	lpRect->right = (m_nScrlWidth * m_nMul) >> 1;
	lpRect->bottom = (m_nScrlHeight * m_nMul) >> 1;

	//  Anadir barras de desplazamiento
	if (bScroll) {
		lpRect->right += ::GetSystemMetrics(SM_CXVSCROLL);
		lpRect->bottom += ::GetSystemMetrics(SM_CYHSCROLL);
	}

	// Convertido en rectangulo de ventana
	CalcWindowRect(lpRect);
}


BOOL FASTCALL CSubBMPWnd::Init(CDrawView* pDrawView)
{	
	pDrawView = NULL;
	return 0;
}

//---------------------------------------------------------------------------
//
//	Adquisicion de rectangulos de ajuste
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::GetFitRect(LPRECT lpRect)
{
	ASSERT(this);
	ASSERT(lpRect);
	ASSERT_VALID(this);

	// Obtener el rectangulo actual del cliente
	GetClientRect(lpRect);

	// Convertido en rectangulo de ventana
	CalcWindowRect(lpRect);
}

//---------------------------------------------------------------------------
//
//	Adquisicion de rectangulos de dibujo
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::GetDrawRect(LPRECT lpRect)
{
	ASSERT(this);
	ASSERT(lpRect);
	ASSERT_VALID(this);

	// Si no hay mapa de bits, error
	if (!m_hBitmap) {
		ASSERT(!m_pBits);
		lpRect->top = 0;
		lpRect->left = 0;
		lpRect->right = 0;
		lpRect->bottom = 0;
		return;
	}

	// Mapa de bits disponible
	ASSERT(m_pBits);

	// Ajustes de desplazamiento
	lpRect->left = m_nScrlX;
	lpRect->top = m_nScrlY;

	// Ajuste del rango (m_bmi.biHeight es siempre negativo).
	lpRect->right = lpRect->left + m_bmi.biWidth;
	lpRect->bottom = lpRect->top - m_bmi.biHeight;

	// examen
	ASSERT(lpRect->left <= lpRect->right);
	ASSERT(lpRect->top <= lpRect->bottom);
	ASSERT(lpRect->right <= m_nScrlWidth);
	ASSERT(lpRect->bottom <= m_nScrlHeight);
}

//---------------------------------------------------------------------------
//
//	adquisicion de bits
//
//---------------------------------------------------------------------------
BYTE* FASTCALL CSubBMPWnd::GetBits() const
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Mapa de bits disponible
	if (m_pBits) {
		ASSERT(m_hBitmap);
		return m_pBits;
	}

	// sin mapa de bits
	ASSERT(!m_hBitmap);
	return NULL;
}

//===========================================================================
//
//	�T�u�r�b�g�}�b�v�E�B���h�E
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
CSubBitmapWnd::CSubBitmapWnd()
{
	// �����o�ϐ�������
	m_nWidth = 48;
	m_nHeight = 16;
	m_pBMPWnd = NULL;

	// ���z��ʃT�C�Y(�h���N���X�ŕK���Ē�`���邱��)
	m_nScrlWidth = -1;
	m_nScrlHeight = -1;
}

//---------------------------------------------------------------------------
//
//	���b�Z�[�W �}�b�v
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubBitmapWnd, CSubWnd)
	ON_WM_CREATE()
	ON_WM_SIZING()
	ON_WM_SIZE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	�E�B���h�E�쐬����
//
//---------------------------------------------------------------------------
BOOL CSubBitmapWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// ��{�N���X
	if (!CSubWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// �T�C�Y��
	cs.style |= WS_THICKFRAME;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�E�B���h�E�쐬
//
//---------------------------------------------------------------------------
int CSubBitmapWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	UINT id;
	CSize size;
	CRect rect;

	ASSERT(this);

	// ��{�N���X
	if (CSubWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

	// �X�e�[�^�X�o�[
	id = 0;
	m_StatusBar.Create(this);
	size = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
	GetClientRect(&rect);
	m_StatusBar.MoveWindow(0, rect.bottom - size.cy, rect.Width(), size.cy);
	m_StatusBar.SetIndicators(&id, 1);
	m_StatusBar.SetPaneInfo(0, 0, SBPS_STRETCH, 0);

	// BMP�E�B���h�E
	rect.bottom -= size.cy;
	m_pBMPWnd = new CSubBMPWnd;
	ASSERT(m_nScrlWidth > 0);
	m_pBMPWnd->m_nScrlWidth = m_nScrlWidth;
	ASSERT(m_nScrlHeight > 0);
	m_pBMPWnd->m_nScrlHeight = m_nScrlHeight;
	m_pBMPWnd->Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
					rect, this, 0, NULL);

	return 0;
}

//---------------------------------------------------------------------------
//
//	�T�C�Y�ύX��
//
//---------------------------------------------------------------------------
void CSubBitmapWnd::OnSizing(UINT nSide, LPRECT lpRect)
{
	CRect rect;
	CSize sizeBar;
	CRect rectSizing;

	// ��{�N���X
	CSubWnd::OnSizing(nSide, lpRect);

	// �X�e�[�^�X�o�[���Ȃ���΁A���^�[��
	if (!::IsWindow(m_StatusBar.m_hWnd)) {
		return;
	}

	// BMP�E�B���h�E�̍ő�T�C�Y�𓾂�(�X�N���[���o�[����)
	m_pBMPWnd->GetMaximumRect(&rect, TRUE);

	// �X�e�[�^�X�o�[�̃T�C�Y�𓾂āA���v
	sizeBar = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
	rect.bottom += sizeBar.cy;

	// ���̃E�B���h�E�̍ő厞�̃T�C�Y�𓾂�
	::AdjustWindowRectEx(&rect, GetStyle(), FALSE, GetExStyle());

	// �I�[�o�[�`�F�b�N
	rectSizing = *lpRect;
	if (rectSizing.Width() >= rect.Width()) {
		lpRect->right = lpRect->left + rect.Width();
	}
	if (rectSizing.Height() >= rect.Height()) {
		lpRect->bottom = lpRect->top + rect.Height();
	}
}

//---------------------------------------------------------------------------
//
//	�T�C�Y�ύX
//
//---------------------------------------------------------------------------
void CSubBitmapWnd::OnSize(UINT nType, int cx, int cy)
{
	CSize sizeBar;
	CRect rectClient;
	CRect rectWnd;
	CRect rectMax;
	CRect rectFit;

	ASSERT(this);
	ASSERT(cx >= 0);
	ASSERT(cy >= 0);

	// ��{�N���X
	CSubWnd::OnSize(nType, cx, cy);

	// �X�e�[�^�X�o�[�Ĕz�u(�E�B���h�E�L���̏ꍇ�Ɍ���)
	if (::IsWindow(m_StatusBar.m_hWnd)) {
		// �X�e�[�^�X�o�[�̍����A�N���C�A���g�̈�̍L���𓾂�
		sizeBar = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
		GetClientRect(&rectClient);

		// �N���C�A���g�̈悪�A�X�e�[�^�X�o�[�����߂邽�߂ɏ\���ł���Έʒu�ύX
		if (rectClient.Height() > sizeBar.cy) {
			m_StatusBar.MoveWindow(0,
								rectClient.Height() - sizeBar.cy,
								rectClient.Width(),
								sizeBar.cy);

			// BMP�E�B���h�E�����킹�čĔz�u
			rectClient.bottom -= sizeBar.cy;
			m_pBMPWnd->MoveWindow(0, 0, rectClient.Width(), rectClient.Height());
		}

		// BMP�E�B���h�E�̃T�C�Y�𓾂āA�I�[�o�[���擾
		m_pBMPWnd->GetWindowRect(&rectWnd);
		m_pBMPWnd->GetMaximumRect(&rectMax, FALSE);
		cx = rectWnd.Width() - rectMax.Width();
		cy = rectWnd.Height() - rectMax.Height();

		// ��������΁A���̃E�B���h�E�����ꂾ���k��(�X�N���[���o�[�̎���ON/OFF�ɑΏ�)
		if ((cx > 0) || (cy > 0)) {
			GetWindowRect(&rectWnd);
			SetWindowPos(&wndTop, 0, 0, rectWnd.Width() - cx, rectWnd.Height() - cy,
						SWP_NOMOVE);
			return;
		}

#if 0
		// BMP�E�B���h�E�̃t�B�b�g�T�C�Y�𓾂āA�I�[�o�[���擾
		m_pBMPWnd->GetFitRect(&rectFit);
		cx = rectWnd.Width() - rectFit.Width();
		cy = rectWnd.Height() - rectFit.Height();

		// ��������΁A���̃E�B���h�E�����ꂾ���k��(200%�ȏ�̏ꍇ�̗]���ɑΏ�)
		if ((cx > 0) || (cy > 0)) {
			GetWindowRect(&rectWnd);
			SetWindowPos(&wndTop, 0, 0, rectWnd.Width() - cx, rectWnd.Height() - cy,
						SWP_NOMOVE);
		}
#endif
	}
}

//---------------------------------------------------------------------------
//
//	�X�V
//
//---------------------------------------------------------------------------
void FASTCALL CSubBitmapWnd::Refresh()
{
	CRect rect;

	// �L���t���O�`�F�b�N
	if (!m_bEnable || !m_pBMPWnd) {
		return;
	}

	// �`���`�擾
	m_pBMPWnd->GetDrawRect(&rect);
	if ((rect.Width() == 0) && (rect.Height() == 0)) {
		return;
	}
	
	// �Z�b�g�A�b�v
	Setup(rect.left, rect.top, rect.Width(), rect.Height(), m_pBMPWnd->GetBits());
	//m_pBMPWnd->m_nScrlWidth;


	CDrawView::DRAWINFO info;
	m_pDrawView->GetDrawInfo(&info);
	/*CString sz;
	sz.Format(_T("info.nWidth:%d   info.nHeight:%d    \r\n"), info.nWidth, info.nHeight);
	OutputDebugStringW(CT2W(sz));*/
	// �\��
	m_pBMPWnd->Refresh();
}

//---------------------------------------------------------------------------
//
//	�p���b�g�ϊ�
//
//---------------------------------------------------------------------------
DWORD FASTCALL CSubBitmapWnd::ConvPalette(WORD value)
{
	DWORD r;
	DWORD g;
	DWORD b;

	// �S�ăR�s�[
	r = (DWORD)value;
	g = (DWORD)value;
	b = (DWORD)value;

	// MSB����G:5�AR:5�AB:5�AI:1�̏��ɂȂ��Ă���
	// ����� R:8 G:8 B:8��DWORD�ɕϊ��Bb31-b24�͎g��Ȃ�
	r <<= 13;
	r &= 0xf80000;
	g &= 0x00f800;
	b <<= 2;
	b &= 0x0000f8;

	// �P�x�r�b�g�͈ꗥUp
	if (value & 1) {
		r |= 0x070000;
		g |= 0x000700;
		b |= 0x000007;
	}

	return (DWORD)(r | g | b);
}

#endif	// _WIN32
