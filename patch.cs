using System;
using System.IO;
using System.Text;
class Program { 
    static void Main() { 
        string path = @"c:\sw\XM62026\vm\crtc.cpp";
        string content = File.ReadAllText(path, Encoding.Default);
        content = content.Replace("void FASTCALL CRTC::CheckRaster()\r\n{\r\n#if 1\r\n\tif (crtc.raster_count == crtc.raster_int) {", "extern class Config config;\r\n\r\nvoid FASTCALL CRTC::CheckRaster()\r\n{\r\n\tif (config.alt_raster) {\r\n\t\tif (crtc.raster_count >= crtc.raster_int - 1 && crtc.raster_count <= crtc.raster_int + 1) {\r\n\t\t\tmfp->SetGPIP(6, 0);\r\n\t\t}\r\n\t\telse {\r\n\t\t\tmfp->SetGPIP(6, 1);\r\n\t\t}\r\n\t}\r\n\telse if (crtc.raster_count == crtc.raster_int) {");
        File.WriteAllText(path, content, Encoding.Default);
        Console.WriteLine("Parche aplicado.");
    } 
}
