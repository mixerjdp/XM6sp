#pragma once
#ifndef MFC_DX9_H
#define MFC_DX9_H

#include <d3d9.h>

class CDX9Renderer {
public:
    CDX9Renderer();
    ~CDX9Renderer();

    // Init: Inicializa Direct3D9.
    // Intenta usar Direct3DCreate9Ex primero (Win7+), si falla, cae a Direct3DCreate9 (XP).
    BOOL Init(HWND hWnd, int width, int height, BOOL bWindowed, BOOL bVSync);
    void Cleanup();

    // Actualiza la textura dinámica de D3D con el buffer de pixeles del emulador
    BOOL UpdateSurface(const DWORD* pSrcBuffer, int srcWidth, int srcHeight, int srcPitchPixels);
    
    // Muestra la imagen en pantalla
    BOOL PresentFrame(int srcWidth, int srcHeight, BOOL fillWindow, BOOL keepAspect);

    // OSD (overlay) renderizado por hardware
    void SetOverlayText(LPCTSTR line1, LPCTSTR line2);

    // Resetear el dispositivo en caso de pérdida o cambio de tamaño
    BOOL ResetDevice(int width, int height, BOOL bWindowed, BOOL bVSync);
    
    // Status
    BOOL IsInitialized() const { return m_bInitialized; }
    BOOL IsDeviceLost() const { return m_bDeviceLost; }

private:
    struct Vertex {
        float x, y, z, rhw;
        float u, v;
    };

    BOOL CreateTexture(int width, int height);
    BOOL CreateVertexBuffer();
    void SetupDeviceStates();
    BOOL UpdateOverlayTexture();
    void ReleaseTexture();
    void ReleaseVertexBuffer();
    void ReleaseOverlayTexture();

    HMODULE m_hD3D9;
    IDirect3D9* m_pD3D;
    IDirect3D9Ex* m_pD3DEx;
    IDirect3DDevice9* m_pDevice;
    IDirect3DDevice9Ex* m_pDeviceEx;
    IDirect3DTexture9* m_pTexture;
    IDirect3DTexture9* m_pOverlayTexture;
    IDirect3DVertexBuffer9* m_pVertexBuffer;

    HWND m_hWnd;
    D3DPRESENT_PARAMETERS m_d3dpp;

    BOOL m_bInitialized;
    BOOL m_bDeviceLost;
    BOOL m_bIsEx; // Indica si estamos usando D3D9Ex
    DWORD m_dwOwnerThreadId; // Guardar ID del hilo

    int m_TexWidth;
    int m_TexHeight;
    int m_OverlayWidth;
    int m_OverlayHeight;
    BOOL m_bOverlayEnabled;
    BOOL m_bOverlayDirty;
    TCHAR m_szOverlayLine1[96];
    TCHAR m_szOverlayLine2[64];
};

#endif // MFC_DX9_H
