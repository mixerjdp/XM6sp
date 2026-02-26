#include "os.h"
#include "mfc_dx9.h"

// Cargamos dinámicamente o pragma explícito
#pragma comment(lib, "d3d9.lib")

// Definición para cargar D3D9Ex dinámicamente si está en Win7+
typedef HRESULT(WINAPI* LPFNDIRECT3DCREATE9EX)(UINT, IDirect3D9Ex**);

CDX9Renderer::CDX9Renderer()
    : m_hD3D9(NULL), m_pD3D(NULL), m_pD3DEx(NULL), m_pDevice(NULL), m_pDeviceEx(NULL), 
      m_pTexture(NULL), m_hWnd(NULL), m_bInitialized(FALSE), m_bDeviceLost(FALSE),
      m_bIsEx(FALSE), m_dwOwnerThreadId(0), m_TexWidth(0), m_TexHeight(0) 
{
    ZeroMemory(&m_d3dpp, sizeof(m_d3dpp));
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

    m_bInitialized = TRUE;
    return TRUE;
}

void CDX9Renderer::Cleanup() 
{
    if (m_dwOwnerThreadId != 0) {
        ASSERT(GetCurrentThreadId() == m_dwOwnerThreadId);
    }

    ReleaseTexture();

    if (m_pDeviceEx) { m_pDeviceEx->Release(); m_pDeviceEx = NULL; }
    if (m_pDevice) { m_pDevice->Release(); m_pDevice = NULL; }
    if (m_pD3DEx) { m_pD3DEx->Release(); m_pD3DEx = NULL; }
    if (m_pD3D) { m_pD3D->Release(); m_pD3D = NULL; }

    if (m_hD3D9) { FreeLibrary(m_hD3D9); m_hD3D9 = NULL; }

    m_bInitialized = FALSE;
    m_bDeviceLost = FALSE;
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

void CDX9Renderer::ReleaseTexture() 
{
    if (m_pTexture) {
        m_pTexture->Release();
        m_pTexture = NULL;
    }
    m_TexWidth = 0;
    m_TexHeight = 0;
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
        m_pDevice->BeginScene();
        
        m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);

        struct Vertex { 
            float x, y, z, rhw; 
            float u, v; 
        };
        #define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW | D3DFVF_TEX1)

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

        Vertex vertices[] = {
            { destX + offset,         destY + offset,         0.0f, 1.0f, 0.0f, 0.0f },
            { destX + destW + offset, destY + offset,         0.0f, 1.0f, uMax, 0.0f },
            { destX + offset,         destY + destH + offset, 0.0f, 1.0f, 0.0f, vMax },
            { destX + destW + offset, destY + destH + offset, 0.0f, 1.0f, uMax, vMax }
        };

        m_pDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
        m_pDevice->SetTexture(0, m_pTexture);
        m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));

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
