#include "os.h"
#include "mfc_dx9.h"

// Cargamos dinámicamente o pragma explícito
#pragma comment(lib, "d3d9.lib")

// Definición para cargar D3D9Ex dinámicamente si está en Win7+
typedef HRESULT(WINAPI* LPFNDIRECT3DCREATE9EX)(UINT, IDirect3D9Ex**);

CDX9Renderer::CDX9Renderer()
    : m_hD3D9(NULL), m_pD3D(NULL), m_pD3DEx(NULL), m_pDevice(NULL), m_pDeviceEx(NULL), 
      m_pTexture(NULL), m_pOverlayTexture(NULL), m_pVertexBuffer(NULL), m_hWnd(NULL), m_bInitialized(FALSE), m_bDeviceLost(FALSE),
      m_bIsEx(FALSE), m_dwOwnerThreadId(0), m_TexWidth(0), m_TexHeight(0), m_OverlayWidth(384), m_OverlayHeight(48),
      m_bOverlayEnabled(FALSE), m_bOverlayDirty(FALSE), m_hOverlayDC(NULL), m_hOverlayBitmap(NULL),
      m_hOverlayOldBitmap(NULL), m_hOverlayOldFont(NULL), m_pOverlayBits(NULL)
{
    ZeroMemory(&m_d3dpp, sizeof(m_d3dpp));
	m_szOverlayLine1[0] = _T('\0');
	m_szOverlayLine2[0] = _T('\0');
}

CDX9Renderer::~CDX9Renderer() 
{
    Cleanup();
}

BOOL CDX9Renderer::Init(HWND hWnd, int width, int height, BOOL bWindowed, BOOL bVSync) 
{
    if (m_bInitialized) return TRUE;

    m_dwOwnerThreadId = GetCurrentThreadId();
    m_hWnd = hWnd;

    // LoadLibrary permite correr en versiones sin d3d9 si fuera el caso
    m_hD3D9 = LoadLibrary("d3d9.dll");
    if (!m_hD3D9) return FALSE;

    LPFNDIRECT3DCREATE9EX pfnDirect3DCreate9Ex = (LPFNDIRECT3DCREATE9EX)GetProcAddress(m_hD3D9, "Direct3DCreate9Ex");
    if (pfnDirect3DCreate9Ex && SUCCEEDED(pfnDirect3DCreate9Ex(D3D_SDK_VERSION, &m_pD3DEx))) {
        m_bIsEx = TRUE;
        m_pD3DEx->QueryInterface(__uuidof(IDirect3D9), (void**)&m_pD3D);
    } else {
        m_bIsEx = FALSE;
        m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
        if (!m_pD3D) {
            FreeLibrary(m_hD3D9);
            m_hD3D9 = NULL;
            return FALSE; // Fallback GDI directo
        }
    }

    ZeroMemory(&m_d3dpp, sizeof(m_d3dpp));
    m_d3dpp.Windowed = bWindowed;
    m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    m_d3dpp.BackBufferFormat = bWindowed ? D3DFMT_UNKNOWN : D3DFMT_X8R8G8B8;
    m_d3dpp.BackBufferCount = 1;
    m_d3dpp.BackBufferWidth = width;
    m_d3dpp.BackBufferHeight = height;
    m_d3dpp.PresentationInterval = bVSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT hr;
    DWORD dwBehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;

    if (m_bIsEx) {
        hr = m_pD3DEx->CreateDeviceEx(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            m_hWnd,
            dwBehaviorFlags,
            &m_d3dpp,
            NULL,
            &m_pDeviceEx
        );
        if (SUCCEEDED(hr)) {
            m_pDeviceEx->QueryInterface(__uuidof(IDirect3DDevice9), (void**)&m_pDevice);
        }
    } else {
        hr = m_pD3D->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            m_hWnd,
            dwBehaviorFlags,
            &m_d3dpp,
            &m_pDevice
        );
    }

    // Si fallamos usando HARDWARE_VERTEXPROCESSING, caemos a SOFTWARE (GPUs muy viejas)
    if (FAILED(hr)) {
        dwBehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        if (m_bIsEx) {
            hr = m_pD3DEx->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, dwBehaviorFlags, &m_d3dpp, NULL, &m_pDeviceEx);
            if (SUCCEEDED(hr)) m_pDeviceEx->QueryInterface(__uuidof(IDirect3DDevice9), (void**)&m_pDevice);
        } else {
            hr = m_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, dwBehaviorFlags, &m_d3dpp, &m_pDevice);
        }
    }

    if (FAILED(hr)) {
        Cleanup();
        return FALSE;
    }

    SetupDeviceStates();

    m_bInitialized = TRUE;
    return TRUE;
}

void CDX9Renderer::Cleanup() 
{
    if (m_dwOwnerThreadId != 0) {
        ASSERT(GetCurrentThreadId() == m_dwOwnerThreadId);
    }

	ReleaseTexture();
	ReleaseOverlayTexture();
	ReleaseOverlayGDIResources();
	ReleaseVertexBuffer();

    if (m_pDeviceEx) { m_pDeviceEx->Release(); m_pDeviceEx = NULL; }
    if (m_pDevice) { m_pDevice->Release(); m_pDevice = NULL; }
    if (m_pD3DEx) { m_pD3DEx->Release(); m_pD3DEx = NULL; }
    if (m_pD3D) { m_pD3D->Release(); m_pD3D = NULL; }

    if (m_hD3D9) { FreeLibrary(m_hD3D9); m_hD3D9 = NULL; }

	m_bInitialized = FALSE;
	m_bDeviceLost = FALSE;
	m_bOverlayEnabled = FALSE;
	m_bOverlayDirty = FALSE;
}

BOOL CDX9Renderer::CreateTexture(int width, int height) 
{
    if (!m_pDevice) return FALSE;
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    if (m_pTexture) {
        if (m_TexWidth >= width && m_TexHeight >= height) return TRUE;
        if (m_TexWidth > width) width = m_TexWidth;
        if (m_TexHeight > height) height = m_TexHeight;
    }

    ReleaseTexture();
    ReleaseVertexBuffer();

    HRESULT hr = m_pDevice->CreateTexture(
        width, height, 1,
        D3DUSAGE_DYNAMIC,
        D3DFMT_X8R8G8B8,
        D3DPOOL_DEFAULT,
        &m_pTexture,
        NULL
    );

    if (FAILED(hr)) return FALSE;

    m_TexWidth = width;
    m_TexHeight = height;
    return TRUE;
}

BOOL CDX9Renderer::CreateVertexBuffer()
{
    if (!m_pDevice) return FALSE;
    if (m_pVertexBuffer) return TRUE;

    HRESULT hr = m_pDevice->CreateVertexBuffer(
        4 * sizeof(Vertex),
        D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
        D3DFVF_XYZRHW | D3DFVF_TEX1,
        D3DPOOL_DEFAULT,
        &m_pVertexBuffer,
        NULL
    );
    return SUCCEEDED(hr);
}

void CDX9Renderer::SetupDeviceStates()
{
    if (!m_pDevice) return;

    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

void CDX9Renderer::ReleaseTexture() 
{
    if (m_pTexture) {
        m_pTexture->Release();
        m_pTexture = NULL;
    }
    m_TexWidth = 0;
    m_TexHeight = 0;
}

void CDX9Renderer::ReleaseVertexBuffer()
{
    if (m_pVertexBuffer) {
        m_pVertexBuffer->Release();
        m_pVertexBuffer = NULL;
    }
}

void CDX9Renderer::ReleaseOverlayTexture()
{
	if (m_pOverlayTexture) {
		m_pOverlayTexture->Release();
		m_pOverlayTexture = NULL;
	}

	// Si el device se resetea, forzar regeneracion del overlay.
	m_bOverlayDirty = TRUE;
}

BOOL CDX9Renderer::EnsureOverlayGDIResources()
{
	if (m_hOverlayDC && m_hOverlayBitmap && m_pOverlayBits) {
		return TRUE;
	}

	ReleaseOverlayGDIResources();

	BITMAPINFO bmi;
	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = m_OverlayWidth;
	bmi.bmiHeader.biHeight = -m_OverlayHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	m_hOverlayDC = CreateCompatibleDC(NULL);
	if (!m_hOverlayDC) {
		return FALSE;
	}

	m_hOverlayBitmap = CreateDIBSection(m_hOverlayDC, &bmi, DIB_RGB_COLORS, (void**)&m_pOverlayBits, NULL, 0);
	if (!m_hOverlayBitmap || !m_pOverlayBits) {
		ReleaseOverlayGDIResources();
		return FALSE;
	}

	m_hOverlayOldBitmap = (HBITMAP)SelectObject(m_hOverlayDC, m_hOverlayBitmap);
	if (!m_hOverlayOldBitmap) {
		ReleaseOverlayGDIResources();
		return FALSE;
	}

	m_hOverlayOldFont = (HFONT)SelectObject(m_hOverlayDC, GetStockObject(DEFAULT_GUI_FONT));
	SetBkMode(m_hOverlayDC, TRANSPARENT);

	return TRUE;
}

void CDX9Renderer::ReleaseOverlayGDIResources()
{
	if (!m_hOverlayDC) {
		m_pOverlayBits = NULL;
		return;
	}

	if (m_hOverlayOldFont) {
		SelectObject(m_hOverlayDC, m_hOverlayOldFont);
		m_hOverlayOldFont = NULL;
	}
	if (m_hOverlayOldBitmap) {
		SelectObject(m_hOverlayDC, m_hOverlayOldBitmap);
		m_hOverlayOldBitmap = NULL;
	}
	if (m_hOverlayBitmap) {
		DeleteObject(m_hOverlayBitmap);
		m_hOverlayBitmap = NULL;
	}
	DeleteDC(m_hOverlayDC);
	m_hOverlayDC = NULL;
	m_pOverlayBits = NULL;
}

void CDX9Renderer::SetOverlayText(LPCTSTR line1, LPCTSTR line2)
{
	TCHAR szLine1[96];
	TCHAR szLine2[64];

	if (line1 && line1[0]) {
		_tcsncpy(szLine1, line1, (sizeof(szLine1) / sizeof(szLine1[0])) - 1);
		szLine1[(sizeof(szLine1) / sizeof(szLine1[0])) - 1] = _T('\0');
	}
	else {
		szLine1[0] = _T('\0');
	}

	if (line2 && line2[0]) {
		_tcsncpy(szLine2, line2, (sizeof(szLine2) / sizeof(szLine2[0])) - 1);
		szLine2[(sizeof(szLine2) / sizeof(szLine2[0])) - 1] = _T('\0');
	}
	else {
		szLine2[0] = _T('\0');
	}

	if ((_tcscmp(m_szOverlayLine1, szLine1) == 0) && (_tcscmp(m_szOverlayLine2, szLine2) == 0)) {
		return;
	}

	_tcscpy(m_szOverlayLine1, szLine1);
	_tcscpy(m_szOverlayLine2, szLine2);
	m_bOverlayEnabled = (m_szOverlayLine1[0] != _T('\0'));
	m_bOverlayDirty = TRUE;
}

BOOL CDX9Renderer::UpdateOverlayTexture()
{
	if (!m_pDevice || !m_bOverlayEnabled) {
		return FALSE;
	}

	if (!m_pOverlayTexture) {
		HRESULT hr = m_pDevice->CreateTexture(
			m_OverlayWidth, m_OverlayHeight, 1,
			D3DUSAGE_DYNAMIC,
			D3DFMT_A8R8G8B8,
			D3DPOOL_DEFAULT,
			&m_pOverlayTexture,
			NULL
		);
		if (FAILED(hr)) {
			return FALSE;
		}
	}

	if (!EnsureOverlayGDIResources()) {
		return FALSE;
	}

	// Fondo transparente inicialmente
	memset(m_pOverlayBits, 0, m_OverlayWidth * m_OverlayHeight * sizeof(DWORD));

	RECT rc = { 0, 0, m_OverlayWidth, m_OverlayHeight };
	RECT rc1 = { 6, 3, m_OverlayWidth - 6, 20 };
	RECT rc2 = { 6, 23, m_OverlayWidth - 6, 40 };

	SetTextColor(m_hOverlayDC, RGB(255, 255, 255));
	DrawText(m_hOverlayDC, m_szOverlayLine1, -1, &rc1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	if (m_szOverlayLine2[0]) {
		SetTextColor(m_hOverlayDC, RGB(255, 220, 96));
		DrawText(m_hOverlayDC, m_szOverlayLine2, -1, &rc2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}

	// Aplicar alpha: fondo semitransparente + texto opaco
	for (int y = rc.top; y < rc.bottom; y++) {
		DWORD *pLine = m_pOverlayBits + (y * m_OverlayWidth);
		for (int x = rc.left; x < rc.right; x++) {
			DWORD c = pLine[x] & 0x00FFFFFF;
			if (c) {
				pLine[x] = 0xFF000000 | c;
			}
			else {
				pLine[x] = 0xA0000000;
			}
		}
	}

	D3DLOCKED_RECT lr;
	BOOL bOk = FALSE;
	if (SUCCEEDED(m_pOverlayTexture->LockRect(0, &lr, NULL, D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK))) {
		for (int y = 0; y < m_OverlayHeight; y++) {
			memcpy((BYTE*)lr.pBits + (y * lr.Pitch), m_pOverlayBits + (y * m_OverlayWidth), m_OverlayWidth * sizeof(DWORD));
		}
		m_pOverlayTexture->UnlockRect(0);
		bOk = TRUE;
	}

	if (bOk) {
		m_bOverlayDirty = FALSE;
	}

	return bOk;
}

BOOL CDX9Renderer::UpdateSurface(const DWORD* pSrcBuffer, int srcWidth, int srcHeight, int srcPitchPixels) 
{
    ASSERT(GetCurrentThreadId() == m_dwOwnerThreadId);
    if (!m_bInitialized || m_bDeviceLost || !m_pDevice || !pSrcBuffer) return FALSE;

    if (!CreateTexture(srcWidth, srcHeight)) return FALSE;

    D3DLOCKED_RECT lockedRect;
    if (SUCCEEDED(m_pTexture->LockRect(0, &lockedRect, NULL, D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK))) {
        DWORD* pDest = (DWORD*)lockedRect.pBits;
        int destPitch = lockedRect.Pitch / 4;

        for (int y = 0; y < srcHeight; ++y) {
            memcpy(pDest + (y * destPitch), pSrcBuffer + (y * srcPitchPixels), srcWidth * 4);
        }
        m_pTexture->UnlockRect(0);
        return TRUE;
    }
    return FALSE;
}

BOOL CDX9Renderer::ResetDevice(int width, int height, BOOL bWindowed, BOOL bVSync) 
{
    ASSERT(GetCurrentThreadId() == m_dwOwnerThreadId);
    if (!m_bInitialized || !m_pDevice) return FALSE;

    ReleaseTexture();
    ReleaseOverlayTexture();

    if (width < 1) width = 1;
    if (height < 1) height = 1;

    m_d3dpp.Windowed = bWindowed;
    m_d3dpp.BackBufferWidth = width;
    m_d3dpp.BackBufferHeight = height;
    m_d3dpp.PresentationInterval = bVSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT hr;
    if (m_bIsEx && m_pDeviceEx) {
        hr = m_pDeviceEx->ResetEx(&m_d3dpp, NULL);
    } else {
        hr = m_pDevice->Reset(&m_d3dpp);
    }

    if (SUCCEEDED(hr)) {
        SetupDeviceStates();
        m_bDeviceLost = FALSE;
        return TRUE;
    }
    
    if (hr == D3DERR_DEVICELOST) {
        m_bDeviceLost = TRUE;
    }

    return FALSE;
}

BOOL CDX9Renderer::PresentFrame(int srcWidth, int srcHeight, BOOL fillWindow, BOOL keepAspect) 
{
    ASSERT(GetCurrentThreadId() == m_dwOwnerThreadId);
    if (!m_bInitialized || !m_pDevice) return FALSE;

    if (m_bDeviceLost) {
        HRESULT hrTest = m_bIsEx ? m_pDeviceEx->CheckDeviceState(m_hWnd) : m_pDevice->TestCooperativeLevel();
        if (hrTest == D3DERR_DEVICENOTRESET || hrTest == S_PRESENT_MODE_CHANGED || hrTest == S_PRESENT_OCCLUDED) {
            if (!ResetDevice(m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, m_d3dpp.Windowed, m_d3dpp.PresentationInterval == D3DPRESENT_INTERVAL_ONE)) {
                return FALSE;
            }
        } else if (hrTest == D3DERR_DEVICELOST) {
            return FALSE;
        }
    }

    if (m_pTexture) {
        if (!CreateVertexBuffer()) {
            return FALSE;
        }

        m_pDevice->BeginScene();

        float bw = (float)m_d3dpp.BackBufferWidth;
        float bh = (float)m_d3dpp.BackBufferHeight;
        float offset = -0.5f; // DirectX 9 texel mapping offset
        
        float uMax = (float)srcWidth / (float)m_TexWidth;
        float vMax = (float)srcHeight / (float)m_TexHeight;

        float destW, destH, destX = 0, destY = 0;
        if (fillWindow) {
            if (keepAspect) {
                float srcAspect = (float)srcWidth / (float)srcHeight;
                float backAspect = bw / bh;
                if (srcAspect > backAspect) {
                    destW = bw;
                    destH = bw / srcAspect;
                } else {
                    destH = bh;
                    destW = bh * srcAspect;
                }
                destX = (bw - destW) / 2.0f;
                destY = (bh - destH) / 2.0f;
            } else {
                destW = bw;
                destH = bh;
            }
        } else {
            destW = (float)srcWidth;
            destH = (float)srcHeight;
        }

        Vertex *pVertices = NULL;
        if (FAILED(m_pVertexBuffer->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK))) {
            m_pDevice->EndScene();
            return FALSE;
        }

        pVertices[0].x = destX + offset;
        pVertices[0].y = destY + offset;
        pVertices[0].z = 0.0f;
        pVertices[0].rhw = 1.0f;
        pVertices[0].u = 0.0f;
        pVertices[0].v = 0.0f;

        pVertices[1].x = destX + destW + offset;
        pVertices[1].y = destY + offset;
        pVertices[1].z = 0.0f;
        pVertices[1].rhw = 1.0f;
        pVertices[1].u = uMax;
        pVertices[1].v = 0.0f;

        pVertices[2].x = destX + offset;
        pVertices[2].y = destY + destH + offset;
        pVertices[2].z = 0.0f;
        pVertices[2].rhw = 1.0f;
        pVertices[2].u = 0.0f;
        pVertices[2].v = vMax;

        pVertices[3].x = destX + destW + offset;
        pVertices[3].y = destY + destH + offset;
        pVertices[3].z = 0.0f;
        pVertices[3].rhw = 1.0f;
        pVertices[3].u = uMax;
        pVertices[3].v = vMax;

        m_pVertexBuffer->Unlock();

        m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        m_pDevice->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(Vertex));
        m_pDevice->SetTexture(0, m_pTexture);
        m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

		// OSD por hardware (DX9)
		if (m_bOverlayEnabled) {
			if (m_bOverlayDirty && !UpdateOverlayTexture()) {
				m_bOverlayEnabled = FALSE;
			}

			if (m_pOverlayTexture && m_bOverlayEnabled) {
				Vertex *pOverlay = NULL;
				if (SUCCEEDED(m_pVertexBuffer->Lock(0, 0, (void**)&pOverlay, D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK))) {
					float ox = 8.0f + offset;
					float oy = 8.0f + offset;
					float ow = (float)m_OverlayWidth;
					float oh = (float)m_OverlayHeight;

					pOverlay[0].x = ox;
					pOverlay[0].y = oy;
					pOverlay[0].z = 0.0f;
					pOverlay[0].rhw = 1.0f;
					pOverlay[0].u = 0.0f;
					pOverlay[0].v = 0.0f;

					pOverlay[1].x = ox + ow;
					pOverlay[1].y = oy;
					pOverlay[1].z = 0.0f;
					pOverlay[1].rhw = 1.0f;
					pOverlay[1].u = 1.0f;
					pOverlay[1].v = 0.0f;

					pOverlay[2].x = ox;
					pOverlay[2].y = oy + oh;
					pOverlay[2].z = 0.0f;
					pOverlay[2].rhw = 1.0f;
					pOverlay[2].u = 0.0f;
					pOverlay[2].v = 1.0f;

					pOverlay[3].x = ox + ow;
					pOverlay[3].y = oy + oh;
					pOverlay[3].z = 0.0f;
					pOverlay[3].rhw = 1.0f;
					pOverlay[3].u = 1.0f;
					pOverlay[3].v = 1.0f;

					m_pVertexBuffer->Unlock();

					m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
					m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
					m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
					m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
					m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

					m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
					m_pDevice->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(Vertex));
					m_pDevice->SetTexture(0, m_pOverlayTexture);
					m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

					m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
					m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
				}
			}
		}

        m_pDevice->EndScene();
    }

    HRESULT hr;
    if (m_bIsEx && m_pDeviceEx) {
        hr = m_pDeviceEx->PresentEx(NULL, NULL, NULL, NULL, 0);
    } else {
        hr = m_pDevice->Present(NULL, NULL, NULL, NULL);
    }

    if (hr == D3DERR_DEVICELOST) {
        m_bDeviceLost = TRUE;
        return FALSE;
    }

    return TRUE;
}
