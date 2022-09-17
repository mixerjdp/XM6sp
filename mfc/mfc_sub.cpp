//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
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

	// 動作停止
	Enable(FALSE);

	// 親ウィンドウへ通知
	m_pDrawView->DelSWnd(this);

	// 基本クラスへ
	CWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	ウィンドウ削除完了
//
//---------------------------------------------------------------------------
void CSubWnd::PostNcDestroy()
{
	// インタフェース要素を削除
	ASSERT(this);
	delete this;
}

//---------------------------------------------------------------------------
//
//	背景描画
//
//---------------------------------------------------------------------------
BOOL CSubWnd::OnEraseBkgnd(CDC *pDC)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// 有効なら、全領域責任を持つ
	if (m_bEnable) {
		return TRUE;
	}

	// 基本クラスへ
	return CWnd::OnEraseBkgnd(pDC);
}

//---------------------------------------------------------------------------
//
//	アクティベート
//
//---------------------------------------------------------------------------
void CSubWnd::OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// ポップアップウィンドウのみ
	if (m_bPopup) {
		if (nState == WA_INACTIVE) {
			// ポップアップウィンドウがインアクティブなら、低速実行
			m_pSch->Activate(FALSE);
		}
		else {
			// ポップアップウィンドウがアクティブなら、通常実行
			m_pSch->Activate(TRUE);
		}
	}

	// 基本クラス
	CWnd::OnActivate(nState, pWnd, bMinimized);
}

//---------------------------------------------------------------------------
//
//	動作制御
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::Enable(BOOL bEnable)
{
	ASSERT(this);

	m_bEnable = bEnable;
}

//---------------------------------------------------------------------------
//
//	ウィンドウIDを取得
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
//	テキストフォントセットアップ
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::SetupTextFont()
{
	CClientDC dc(this);
	CFont *pFont;
	TEXTMETRIC tm;

	ASSERT(this);

	// フォントセレクト
	ASSERT(m_pTextFont);
	pFont = dc.SelectObject(m_pTextFont);
	ASSERT(pFont);

	// テキストメトリックを取得
	VERIFY(dc.GetTextMetrics(&tm));

	// フォントを戻す
	VERIFY(dc.SelectObject(pFont));

	// テキスト横幅、縦幅を格納
	m_tmWidth = tm.tmAveCharWidth;
	m_tmHeight = tm.tmHeight + tm.tmExternalLeading;
}

//---------------------------------------------------------------------------
//
//	メッセージスレッドから更新
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::Update()
{
	ASSERT(this);
	ASSERT_VALID(this);
}

//---------------------------------------------------------------------------
//
//	セーブ
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
//	ロード
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
//	設定適用
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
//	診断
//
//---------------------------------------------------------------------------
void CSubWnd::AssertValid() const
{
	ASSERT(this);

	// 他のオブジェクト
	ASSERT(m_pSch);
	ASSERT(m_pSch->GetID() == MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pDrawView);
	ASSERT(m_pDrawView->m_hWnd);
	ASSERT(::IsWindow(m_pDrawView->m_hWnd));

	// プロパティ
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
//	サブテキストウィンドウ
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CSubTextWnd::CSubTextWnd()
{
	// メンバ変数初期化
	m_bReverse = FALSE;
	m_pTextBuf = NULL;
	m_pDrawBuf = NULL;
}

//---------------------------------------------------------------------------
//
//	メッセージ マップ
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
//	ウィンドウ作成
//
//---------------------------------------------------------------------------
int CSubTextWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	ASSERT(this);
	ASSERT(!m_pTextBuf);
	ASSERT(m_nWidth > 0);
	ASSERT(m_nHeight > 0);

	// テキストバッファ確保
	m_pTextBuf = new BYTE[m_nWidth * m_nHeight];
	ASSERT(m_pTextBuf);

	// 描画バッファ確保、初期化
	m_pDrawBuf = new BYTE[m_nWidth * m_nHeight];
	ASSERT(m_pDrawBuf);
	memset(m_pDrawBuf, 0xff, m_nWidth * m_nHeight);

	// 基本クラス(ここでDrawViewヘ通知)
	if (CSubWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	ウィンドウ削除
//
//---------------------------------------------------------------------------
void CSubTextWnd::OnDestroy()
{
	ASSERT(this);

	// 基本クラスを先に実行する(DrawViewへの通知を急ぐため)
	CSubWnd::OnDestroy();

	// テキストバッファ解放
	ASSERT(m_pTextBuf);
	if (m_pTextBuf) {
		delete[] m_pTextBuf;
		m_pTextBuf = NULL;
	}

	// 描画バッファ解放
	ASSERT(m_pDrawBuf);
	if (m_pDrawBuf) {
		delete[] m_pDrawBuf;
		m_pDrawBuf = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	描画
//
//---------------------------------------------------------------------------
void CSubTextWnd::OnPaint()
{
	PAINTSTRUCT ps;

	// VMをロック
	::LockVM();

    BeginPaint(&ps);

	// テキストバッファ、有効フラグチェック
	if (m_pTextBuf && m_bEnable) {
		// 描画バッファをFFで埋める
		memset(m_pDrawBuf, 0xff, m_nWidth * m_nHeight);

		// 描画は行わない(VMスレッドからのRefreshに任せる)
	}

	EndPaint(&ps);

	// VMアンロック
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	サイズ変更
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

	// キャラクタ当たりのクライアントサイズを計算
	GetClientRect(&rectClient);
	nWidth = rectClient.Width() / m_tmWidth;
	nHeight = rectClient.Height() / m_tmHeight;

	// 一致していなくて
	if ((nWidth != m_nWidth) || (nHeight != m_nHeight)) {
		// 最小化でなくて
		if (nType != SIZE_MINIMIZED) {
			// テキストバッファがあれば
			if (m_pTextBuf && m_pDrawBuf) {
				// 余裕があれば、+1
				if ((nWidth * m_tmWidth) < rectClient.Width()) {
					nWidth++;
				}
				if ((nHeight * m_tmWidth) < rectClient.Height()) {
					nHeight++;
				}

				// バッファ変更(VMロックを挟む)
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

	// 再描画を図る
	Invalidate(FALSE);

	// 基本クラスへ
	CSubWnd::OnSize(nType, cx, cy);
}

//---------------------------------------------------------------------------
//
//	リフレッシュ描画
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::Refresh()
{
	CClientDC dc(this);

	ASSERT(this);

	// テキストバッファ、有効フラグチェック
	if (m_pTextBuf && m_bEnable) {
		// セットアップ
		Setup();

		// 描画
		OnDraw(&dc);
	}
}

//---------------------------------------------------------------------------
//
//	描画メイン
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

	// フォント指定
	pFont = pDC->SelectObject(m_pTextFont);
	ASSERT(pFont);

	// 色指定
	pDC->SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
	pDC->SetBkColor(::GetSysColor(COLOR_WINDOW));
	bReverse = FALSE;

	// ポインタ、座標初期化
	pText = m_pTextBuf;
	pDraw = m_pDrawBuf;
	point.y = 0;

	// yループ
	for (y=0; y<m_nHeight; y++) {
		point.x = 0;
		for (x=0; x<m_nWidth; x++) {
			// 一致チェック
			if (*pText == *pDraw) {
				pText++;
				pDraw++;
				point.x += m_tmWidth;
				continue;
			}

			// コピー
			ch = *pText++;
			*pDraw++ = ch;

			// 反転チェック
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

			// テキストアウト
			pDC->TextOut(point.x, point.y, (LPCTSTR)&ch ,1);
			point.x += m_tmWidth;
		}
		point.y += m_tmHeight;
	}

	// フォント指定解除
	VERIFY(pDC->SelectObject(pFont));
}

//---------------------------------------------------------------------------
//
//	クリア
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::Clear()
{
	ASSERT(this);
	ASSERT(m_pTextBuf);
	ASSERT(m_nWidth >= 0);
	ASSERT(m_nHeight >= 0);

	// クリア
	if (m_pTextBuf) {
		memset(m_pTextBuf, 0x20, m_nWidth * m_nHeight);
	}

	// 反転オフ
	Reverse(FALSE);
}

//---------------------------------------------------------------------------
//
//	文字セット
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::SetChr(int x, int y, TCHAR ch)
{
	ASSERT(this);
	ASSERT(m_pTextBuf);
	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(_istprint(ch));

	// 有効範囲かチェック
	if ((x < m_nWidth) && (y < m_nHeight)) {
		// 書き込み
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
//	文字列セット
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::SetString(int x, int y, LPCTSTR lpszText)
{
	ASSERT(this);
	ASSERT(m_pTextBuf);
	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(lpszText);

	// '\0'までループ
	while (_istprint(*lpszText)) {
		SetChr(x, y, *lpszText);

		lpszText++;
		x++;
	}
}

//---------------------------------------------------------------------------
//
//	反転セット
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
//	リサイズ
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::ReSize(int nWidth, int nHeight)
{
	CRect rectWnd;
	CRect rectNow;

	ASSERT(this);
	ASSERT(nWidth > 0);
	ASSERT(nHeight > 0);

	// 一致チェック
	if ((nWidth == m_nWidth) && (nHeight == m_nHeight)) {
		return;
	}

	// バッファがなければ何もしない
	if (!m_pTextBuf || !m_pDrawBuf) {
		return;
	}

	// サイズを算出
	rectWnd.left = 0;
	rectWnd.top = 0;
	rectWnd.right = nWidth * m_tmWidth;
	rectWnd.bottom = nHeight * m_tmHeight;
	CalcWindowRect(&rectWnd);

	// 現在と同じなら何もしない
	GetWindowRect(&rectNow);
	if ((rectNow.Width() == rectWnd.Width()) && (rectNow.Height() == rectWnd.Height())) {
		return;
	}

	// サイズ変更
	SetWindowPos(&wndTop, 0, 0, rectWnd.Width(), rectWnd.Height(), SWP_NOMOVE);
}

//===========================================================================
//
//	サブテキスト可変ウィンドウ
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CSubTextSizeWnd::CSubTextSizeWnd()
{
}

//---------------------------------------------------------------------------
//
//	ウィンドウ作成準備
//
//---------------------------------------------------------------------------
BOOL CSubTextSizeWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// 基本クラス
	if (!CSubTextWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// サイズ可変、最大化可・
	cs.style |= WS_THICKFRAME;
	cs.style |= WS_MAXIMIZEBOX;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	描画メイン
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextSizeWnd::OnDraw(CDC *pDC)
{
	CRect rect;

	ASSERT(this);
	ASSERT(pDC);

	// クライアント矩形を取得
	GetClientRect(&rect);

	// 右側をクリア
	rect.left = m_nWidth * m_tmWidth;
	pDC->FillSolidRect(&rect, GetSysColor(COLOR_WINDOW));

	// 下側をクリア
	rect.left = 0;
	rect.top = m_nHeight * m_tmHeight;
	pDC->FillSolidRect(&rect, GetSysColor(COLOR_WINDOW));

	// 基本クラスへ
	CSubTextWnd::OnDraw(pDC);
}

//===========================================================================
//
//	サブテキストスクロールウィンドウ
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CSubTextScrlWnd::CSubTextScrlWnd()
{
	// メンバ変数初期化
	m_ScrlWidth = -1;
	m_ScrlHeight = -1;
	m_bScrlH = FALSE;
	m_bScrlV = FALSE;
	m_ScrlX = 0;
	m_ScrlY = 0;
}

//---------------------------------------------------------------------------
//
//	メッセージ マップ
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
//	ウィンドウ作成準備
//
//---------------------------------------------------------------------------
BOOL CSubTextScrlWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// 基本クラス
	if (!CSubTextSizeWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// スクロールバーを追加
	cs.style |= WS_HSCROLL;
	cs.style |= WS_VSCROLL;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ウィンドウ作成
//
//---------------------------------------------------------------------------
int CSubTextScrlWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	CRect wrect;
	CRect crect;

	ASSERT(this);

	// 基本クラス
	if (CSubTextSizeWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

	// スクロール準備
	ShowScrollBar(SB_BOTH, FALSE);
	SetupScrl();

	// サイズ調整
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
//	サイズ変更
//
//---------------------------------------------------------------------------
void CSubTextScrlWnd::OnSize(UINT nType, int cx, int cy)
{
	ASSERT(this);

	// 基本クラスを先に実行
	CSubTextSizeWnd::OnSize(nType, cx, cy);

	// スクロール準備
	SetupScrl();
}

//---------------------------------------------------------------------------
//
//	スクロール準備
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

	// クライアントサイズを取得
	GetClientRect(&rect);

	// 最小化なら何もしない
	if ((rect.right == 0) || (rect.bottom == 0)) {
		return;
	}

	// Hスクロールを判定
	width = rect.right / m_tmWidth;
	if (width < m_ScrlWidth) {
		if (!m_bScrlH) {
			m_bScrlH = TRUE;
			m_ScrlX = 0;
			ShowScrollBar(SB_HORZ, TRUE);
		}
		// スクロールバーが必要なので、詳細設定
		SetupScrlH();
	}
	else {
		if (m_bScrlH) {
			m_bScrlH = FALSE;
			m_ScrlX = 0;
			ShowScrollBar(SB_HORZ, FALSE);
		}
	}

	// Vスクロールを判定
	height = rect.bottom / m_tmHeight;
	if (height < m_ScrlHeight) {
		if (!m_bScrlV) {
			m_bScrlV = TRUE;
			m_ScrlY = 0;
			ShowScrollBar(SB_VERT, TRUE);
		}
		// スクロールバーが必要なので、詳細設定
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
//	スクロール準備(水平)
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextScrlWnd::SetupScrlH()
{
	SCROLLINFO si;
	CRect rect;
	int width;

	ASSERT(this);

	// 水平表示可能キャラクタを取得
	GetClientRect(&rect);
	width = rect.right / m_tmWidth;

	// スクロール情報をセット
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_ScrlWidth - 1;
	si.nPage = width;

	// 位置は、必要なら補正する
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
//	スクロール(水平)
//
//---------------------------------------------------------------------------
void CSubTextScrlWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar * /*pBar*/)
{
	SCROLLINFO si;

	ASSERT(this);

	// スクロール情報を取得
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_HORZ, &si, SIF_ALL);

	// スクロールバーコード別
	switch (nSBCode) {
		// 左へ
		case SB_LEFT:
			m_ScrlX = si.nMin;
			break;

		// 右へ
		case SB_RIGHT:
			m_ScrlX = si.nMax;
			break;

		// 1ライン左へ
		case SB_LINELEFT:
			if (m_ScrlX > 0) {
				m_ScrlX--;
			}
			break;

		// 1ライン右へ
		case SB_LINERIGHT:
			if (m_ScrlX < si.nMax) {
				m_ScrlX++;
			}
			break;

		// 1ページ左へ
		case SB_PAGELEFT:
			if (m_ScrlX >= (int)si.nPage) {
				m_ScrlX -= (int)si.nPage;
			}
			else {
				m_ScrlX = 0;
			}
			break;

		// 1ページ右へ
		case SB_PAGERIGHT:
			if ((m_ScrlX + (int)si.nPage) <= (int)si.nMax) {
				m_ScrlX += si.nPage;
			}
			else {
				m_ScrlX = si.nMax;
			}
			break;

		// サム移動
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			m_ScrlX = nPos;
			break;
	}

	// 正に補正
	if (m_ScrlX < 0) {
		m_ScrlX = 0;
	}

	// セット
	SetupScrlH();
}

//---------------------------------------------------------------------------
//
//	スクロール準備(垂直)
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextScrlWnd::SetupScrlV()
{
	SCROLLINFO si;
	CRect rect;
	int height;

	ASSERT(this);

	// 垂直表示可能キャラクタを取得
	GetClientRect(&rect);
	height = rect.bottom / m_tmHeight;

	// スクロール情報をセット
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_ScrlHeight - 1;
	si.nPage = height;

	// 位置は、必要なら補正する
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
//	スクロール(垂直)
//
//---------------------------------------------------------------------------
void CSubTextScrlWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar * /*pBar*/)
{
	SCROLLINFO si;

	ASSERT(this);

	// スクロール情報を取得
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_VERT, &si, SIF_ALL);

	// スクロールバーコード別
	switch (nSBCode) {
		// 上へ
		case SB_TOP:
			m_ScrlY = si.nMin;
			break;

		// 下へ
		case SB_BOTTOM:
			m_ScrlY = si.nMax;
			break;

		// 1ライン上へ
		case SB_LINEUP:
			if (m_ScrlY > 0) {
				m_ScrlY--;
			}
			break;

		// 1ライン下へ
		case SB_LINEDOWN:
			if (m_ScrlY < si.nMax) {
				m_ScrlY++;
			}
			break;

		// 1ページ上へ
		case SB_PAGEUP:
			if (m_ScrlY >= (int)si.nPage) {
				m_ScrlY -= si.nPage;
			}
			else {
				m_ScrlY = 0;
			}
			break;

		// 1ページ下へ
		case SB_PAGEDOWN:
			if ((m_ScrlY + (int)si.nPage) <= (int)si.nMax) {
				m_ScrlY += si.nPage;
			}
			else {
				m_ScrlY = si.nMax;
			}
			break;

		// サム移動
		case SB_THUMBPOSITION:
			m_ScrlY = nPos;
			break;
		case SB_THUMBTRACK:
			m_ScrlY = nPos;
			break;
	}

	// 正に補正
	if (m_ScrlY < 0) {
		m_ScrlY = 0;
	}

	// セット、描画
	SetupScrlV();
}

//===========================================================================
//
//	サブリストウィンドウ
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CSubListWnd::CSubListWnd()
{
	// リストコントロールの準備が終わるまでDisableしておく
	m_bEnable = FALSE;
}

//---------------------------------------------------------------------------
//
//	メッセージ マップ
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubListWnd, CSubWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_DRAWITEM()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	ウィンドウ作成準備
//
//---------------------------------------------------------------------------
BOOL CSubListWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// 基本クラス
	if (!CSubWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// ウィンドウサイズを可変とする
	cs.style |= WS_THICKFRAME;
	cs.style |= WS_MAXIMIZEBOX;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ウィンドウ作成
//
//---------------------------------------------------------------------------
int CSubListWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	CRect rect;
	CDC *pDC;
	TEXTMETRIC tm;

	// 基本クラス
	if (CSubWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

	// リストコントロールを作成
	VERIFY(m_ListCtrl.Create(WS_CHILD | WS_VISIBLE |
							LVS_REPORT | LVS_NOSORTHEADER| LVS_SINGLESEL |
						 	LVS_OWNERDRAWFIXED | LVS_OWNERDATA,
							CRect(0, 0, 0, 0), this, 0));

	// フォントサイズを取り直す
	pDC = m_ListCtrl.GetDC();
	pDC->GetTextMetrics(&tm);
	m_ListCtrl.ReleaseDC(pDC);
	m_tmWidth = tm.tmAveCharWidth;
	m_tmHeight = tm.tmHeight + tm.tmExternalLeading;

	// 初期化
	InitList();

	// 有効にして、リフレッシュ
	m_bEnable = TRUE;
	Refresh();

	return 0;
}

//---------------------------------------------------------------------------
//
//	サイズ変更
//
//---------------------------------------------------------------------------
void CSubListWnd::OnSize(UINT nType, int cx, int cy)
{
	CRect rect;

	ASSERT(this);

	// 基本クラス
	CSubWnd::OnSize(nType, cx, cy);

	// クライアントエリア一杯になるように、リストコントロールを調整
	if (m_ListCtrl.GetSafeHwnd()) {
		GetClientRect(&rect);
		m_ListCtrl.SetWindowPos(&wndTop, 0, 0, rect.right, rect.bottom, SWP_NOZORDER);
	}
}

//---------------------------------------------------------------------------
//
//	オーナードロー
//
//---------------------------------------------------------------------------
void CSubListWnd::OnDrawItem(int /*nID*/, LPDRAWITEMSTRUCT /*lpDIS*/)
{
	// 必ず派生クラスで定義すること
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
							
				
				// Stretchblt: nHeight y nWidth son par疥etros del tama exacto del bitmap origen en la resolucion origen

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

	// ビットマップがある場合に限り
	if (m_hBitmap) {
		// メモリDC作成
		mDC.CreateCompatibleDC(&dc);

		// オブジェクト選択
		hBitmap = (HBITMAP)::SelectObject(mDC.m_hDC, m_hBitmap);

		// BitBlt or StretchBlt
		if (hBitmap) {
			if (m_nMul == 2) {
				// 等倍
				dc.BitBlt(0, 0, m_bmi.biWidth, -m_bmi.biHeight,
					&mDC, 0, 0, SRCCOPY);
			}
			else {
				// n倍
				dc.StretchBlt(0, 0,
					(m_bmi.biWidth * m_nMul) >> 1,
					-((m_bmi.biHeight * m_nMul) >> 1),
					&mDC,
					0, 0,
					m_bmi.biWidth, -m_bmi.biHeight,
					SRCCOPY);
			}

			// オブジェクト選択終了
			::SelectObject(mDC.m_hDC, hBitmap);
		}

		// メモリDC終了
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
//	サブビットマップウィンドウ
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CSubBitmapWnd::CSubBitmapWnd()
{
	// メンバ変数初期化
	m_nWidth = 48;
	m_nHeight = 16;
	m_pBMPWnd = NULL;

	// 仮想画面サイズ(派生クラスで必ず再定義すること)
	m_nScrlWidth = -1;
	m_nScrlHeight = -1;
}

//---------------------------------------------------------------------------
//
//	メッセージ マップ
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubBitmapWnd, CSubWnd)
	ON_WM_CREATE()
	ON_WM_SIZING()
	ON_WM_SIZE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	ウィンドウ作成準備
//
//---------------------------------------------------------------------------
BOOL CSubBitmapWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// 基本クラス
	if (!CSubWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// サイズ可変
	cs.style |= WS_THICKFRAME;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ウィンドウ作成
//
//---------------------------------------------------------------------------
int CSubBitmapWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	UINT id;
	CSize size;
	CRect rect;

	ASSERT(this);

	// 基本クラス
	if (CSubWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

	// ステータスバー
	id = 0;
	m_StatusBar.Create(this);
	size = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
	GetClientRect(&rect);
	m_StatusBar.MoveWindow(0, rect.bottom - size.cy, rect.Width(), size.cy);
	m_StatusBar.SetIndicators(&id, 1);
	m_StatusBar.SetPaneInfo(0, 0, SBPS_STRETCH, 0);

	// BMPウィンドウ
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
//	サイズ変更中
//
//---------------------------------------------------------------------------
void CSubBitmapWnd::OnSizing(UINT nSide, LPRECT lpRect)
{
	CRect rect;
	CSize sizeBar;
	CRect rectSizing;

	// 基本クラス
	CSubWnd::OnSizing(nSide, lpRect);

	// ステータスバーがなければ、リターン
	if (!::IsWindow(m_StatusBar.m_hWnd)) {
		return;
	}

	// BMPウィンドウの最大サイズを得る(スクロールバー込み)
	m_pBMPWnd->GetMaximumRect(&rect, TRUE);

	// ステータスバーのサイズを得て、合計
	sizeBar = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
	rect.bottom += sizeBar.cy;

	// このウィンドウの最大時のサイズを得る
	::AdjustWindowRectEx(&rect, GetStyle(), FALSE, GetExStyle());

	// オーバーチェック
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
//	サイズ変更
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

	// 基本クラス
	CSubWnd::OnSize(nType, cx, cy);

	// ステータスバー再配置(ウィンドウ有効の場合に限定)
	if (::IsWindow(m_StatusBar.m_hWnd)) {
		// ステータスバーの高さ、クライアント領域の広さを得る
		sizeBar = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
		GetClientRect(&rectClient);

		// クライアント領域が、ステータスバーを収めるために十分であれば位置変更
		if (rectClient.Height() > sizeBar.cy) {
			m_StatusBar.MoveWindow(0,
								rectClient.Height() - sizeBar.cy,
								rectClient.Width(),
								sizeBar.cy);

			// BMPウィンドウをあわせて再配置
			rectClient.bottom -= sizeBar.cy;
			m_pBMPWnd->MoveWindow(0, 0, rectClient.Width(), rectClient.Height());
		}

		// BMPウィンドウのサイズを得て、オーバー分取得
		m_pBMPWnd->GetWindowRect(&rectWnd);
		m_pBMPWnd->GetMaximumRect(&rectMax, FALSE);
		cx = rectWnd.Width() - rectMax.Width();
		cy = rectWnd.Height() - rectMax.Height();

		// もしあれば、このウィンドウをそれだけ縮小(スクロールバーの自動ON/OFFに対処)
		if ((cx > 0) || (cy > 0)) {
			GetWindowRect(&rectWnd);
			SetWindowPos(&wndTop, 0, 0, rectWnd.Width() - cx, rectWnd.Height() - cy,
						SWP_NOMOVE);
			return;
		}

#if 0
		// BMPウィンドウのフィットサイズを得て、オーバー分取得
		m_pBMPWnd->GetFitRect(&rectFit);
		cx = rectWnd.Width() - rectFit.Width();
		cy = rectWnd.Height() - rectFit.Height();

		// もしあれば、このウィンドウをそれだけ縮小(200%以上の場合の余分に対処)
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
//	更新
//
//---------------------------------------------------------------------------
void FASTCALL CSubBitmapWnd::Refresh()
{
	CRect rect;

	// 有効フラグチェック
	if (!m_bEnable || !m_pBMPWnd) {
		return;
	}

	// 描画矩形取得
	m_pBMPWnd->GetDrawRect(&rect);
	if ((rect.Width() == 0) && (rect.Height() == 0)) {
		return;
	}
	
	// セットアップ
	Setup(rect.left, rect.top, rect.Width(), rect.Height(), m_pBMPWnd->GetBits());
	//m_pBMPWnd->m_nScrlWidth;


	CDrawView::DRAWINFO info;
	m_pDrawView->GetDrawInfo(&info);
	/*CString sz;
	sz.Format(_T("info.nWidth:%d   info.nHeight:%d    \r\n"), info.nWidth, info.nHeight);
	OutputDebugStringW(CT2W(sz));*/
	// 表示
	m_pBMPWnd->Refresh();
}

//---------------------------------------------------------------------------
//
//	パレット変換
//
//---------------------------------------------------------------------------
DWORD FASTCALL CSubBitmapWnd::ConvPalette(WORD value)
{
	DWORD r;
	DWORD g;
	DWORD b;

	// 全てコピー
	r = (DWORD)value;
	g = (DWORD)value;
	b = (DWORD)value;

	// MSBからG:5、R:5、B:5、I:1の順になっている
	// これを R:8 G:8 B:8のDWORDに変換。b31-b24は使わない
	r <<= 13;
	r &= 0xf80000;
	g &= 0x00f800;
	b <<= 2;
	b &= 0x0000f8;

	// 輝度ビットは一律Up
	if (value & 1) {
		r |= 0x070000;
		g |= 0x000700;
		b |= 0x000007;
	}

	return (DWORD)(r | g | b);
}

#endif	// _WIN32
