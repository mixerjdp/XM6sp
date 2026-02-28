XM6 CRT Shader Resources
========================

Este directorio contiene recursos para el efecto shader CRT en XM6.

Requisitos:
-----------
1. DirectX 9 SDK (d3dx9.h, d3dx9.lib) - Necesario para compilar shaders HLSL
2. Archivo crt.hlsl - Shader de píxeles para efecto CRT

Configuración:
--------------
- El emulador busca "shaders/crt.hlsl" relativo al directorio del ejecutable (XM6.exe)
- Es decir, si XM6.exe está en "00proj.vc7/Debug/", entonces espera:
  "00proj.vc7/Debug/shaders/crt.hlsl"

Instalación:
------------
1. Copiar esta carpeta "shaders/" al mismo nivel que XM6.exe:
   - Para Debug:   00proj.vc7/Debug/shaders/
   - Para Release: 00proj.vc7/Release/shaders/

2. (Opcional) Activar compilación dinámica de shaders:
   - En mfc/mfc_dx9.cpp, descomentar:
     #include <d3dx9.h>
     #pragma comment(lib, "d3dx9.lib")
     #define USE_D3DX9 1
   - Requiere instalar DirectX 9 SDK

Uso:
----
- Presionar F9 para alternar efecto CRT Shader
- El estado se muestra en pantalla (OSD): "CRT Shader: ON/OFF"
- Si el shader falla en cargar, se desactiva automáticamente

Nota: 
-----
Actualmente USE_D3DX9 = 0, por lo que la compilación dinámica está deshabilitada.
El shader funciona como fallback (sin efecto visual real).
Para activar el efecto, instalar D3DX9 SDK y activar USE_D3DX9 = 1 en mfc_dx9.cpp.
