#pragma once
#ifndef MFC_DX9_H
#define MFC_DX9_H

#include <d3d9.h>

class CDX9Renderer {
public:
    CDX9Renderer();
    ~CDX9Renderer();

// Initialize Direct3D9.
// Try Direct3DCreate9Ex first (Win7+); if it fails, fall back to Direct3DCreate9 (XP).
    BOOL Init(HWND hWnd, int width, int height, BOOL bWindowed, BOOL bVSync);
    void Cleanup();

    // Update the D3D dynamic texture with the emulator pixel buffer
    BOOL UpdateSurface(const DWORD* pSrcBuffer, int srcWidth, int srcHeight, int srcPitchPixels);

// Present the image on screen
    BOOL PresentFrame(int srcWidth, int srcHeight, int scalePercent);

// Hardware-rendered OSD overlay
    void SetOverlayText(LPCTSTR line1, LPCTSTR line2);

    // Reset the device after a loss or a resize
    BOOL ResetDevice(int width, int height, BOOL bWindowed, BOOL bVSync);

// Status
    BOOL IsInitialized() const { return m_bInitialized; }
    BOOL IsDeviceLost() const { return m_bDeviceLost; }
    BOOL IsShaderEnabled() const { return m_bShaderEnabled; }
    void SetShaderEnabled(BOOL bEnable) { m_bShaderEnabled = bEnable; }
    BOOL CreateCRTShader(LPCTSTR szPath);

private:
    struct Vertex {
        float x, y, z, rhw;
        float u, v;
    };

    BOOL CreateTexture(int width, int height);
    BOOL CreateVertexBuffer();
    void SetupDeviceStates();
    BOOL EnsureOverlayGDIResources();
    BOOL UpdateOverlayTexture();
    void ReleaseOverlayGDIResources();
    void ReleaseTexture();
    void ReleaseVertexBuffer();
    void ReleaseOverlayTexture();
    void ReleaseCRTShader();

    HMODULE m_hD3D9;
    IDirect3D9* m_pD3D;
    IDirect3D9Ex* m_pD3DEx;
    IDirect3DDevice9* m_pDevice;
    IDirect3DDevice9Ex* m_pDeviceEx;
    IDirect3DTexture9* m_pTexture;
    IDirect3DTexture9* m_pOverlayTexture;
    IDirect3DVertexBuffer9* m_pVertexBuffer;
    IDirect3DPixelShader9 *m_pCRTShader;	// CRT post-processing shader

    HWND m_hWnd;
    D3DPRESENT_PARAMETERS m_d3dpp;
    TCHAR m_szCRTShaderPath[MAX_PATH];	// Path of the loaded CRT shader

    BOOL m_bInitialized;
    BOOL m_bDeviceLost;
    BOOL m_bIsEx;	// True if D3D9Ex is in use
    BOOL m_bShaderEnabled;	// CRT shader enabled
    DWORD m_dwOwnerThreadId;	// Owner thread ID

    int m_TexWidth;
    int m_TexHeight;
    int m_OverlayWidth;
    int m_OverlayHeight;
    BOOL m_bOverlayEnabled;
    BOOL m_bOverlayDirty;
    TCHAR m_szOverlayLine1[96];
    TCHAR m_szOverlayLine2[64];
    HDC m_hOverlayDC;
    HBITMAP m_hOverlayBitmap;
    HBITMAP m_hOverlayOldBitmap;
    HFONT m_hOverlayOldFont;
    DWORD* m_pOverlayBits;
};

#endif  // MFC_DX9_H
