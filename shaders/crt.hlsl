// CRT Shader para XM6 - Efecto scanlines simples
// Shader de píxeles (ps_2_0) que aplica efecto CRT básico con scanlines

sampler2D textureSampler : register(s0);

float4 main(float2 texCoord : TEXCOORD0) : COLOR
{
    // Coordenadas UV normalizadas
    float2 uv = texCoord;
    
    // Altura aproximada de una línea de escaneo en UV
    float scanlineFreq = 2.0;  // Frecuencia de scanlines
    float scanlineAmplitude = 0.3;  // Intensidad del efecto
    
    // Calcular posición en píxeles virtuales (escala pantalla típica)
    float pixelY = uv.y * 480.0;  // Asumiendo altura 480
    
    // Patrón de scanlines
    float scanline = abs(sin(pixelY * 3.14159 * scanlineFreq)) * scanlineAmplitude;
    
    // Samplear color original
    float4 color = tex2D(textureSampler, uv);
    
    // Aplicar oscurecimiento por scanlines
    color.rgb *= (1.0 - scanline);
    
    // Ligera reducción de luminancia para efecto CRT (menos brillante)
    color.rgb *= 0.95;
    
    return color;
}
