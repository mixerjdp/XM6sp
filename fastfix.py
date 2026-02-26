import os
file_path = 'mfc/mfc_app.cpp'

with open(file_path, 'rb') as f:
    content = f.read()

# Buscamos la funcion y la borramos hasta terminar
s_str = b"CString FASTCALL GetString(UINT uID)"
e_str = b"//---------------------------------------------------------------------------"

start_idx = content.find(s_str)
end_idx = content.find(e_str, start_idx + 10)

if start_idx != -1 and end_idx != -1:
    good = b'CString FASTCALL GetString(UINT uID)\r\n{\r\n\tCString string;\r\n\r\n\t// ?Es japones?\r\n\tif (IsJapanese()) {\r\n\t\tif (string.LoadString(uID)) {\r\n\t\t\treturn string;\r\n\t\t}\r\n#if defined(_DEBUG)\r\n\t\telse {\r\n\t\t\tTRACE(_T("GetString() failed (ID:%d)\\n"), uID);\r\n\t\t}\r\n#endif\r\n\t\tstring.Empty();\r\n\t}\r\n\r\n\t// Ingles\r\n\tif (string.LoadString(uID + 5000)) {\r\n\t\treturn string;\r\n\t}\r\n#if defined(_DEBUG)\r\n\telse {\r\n\t\tTRACE(_T("GetString() failed (ID:%d)\\n"), uID + 5000);\r\n\t}\r\n#endif\r\n\r\n\t// Por defecto\r\n\tif (string.LoadString(uID)) {\r\n\t\treturn string;\r\n\t}\r\n#if defined(_DEBUG)\r\n\telse {\r\n\t\tTRACE(_T("GetString() failed final (ID:%d)\\n"), uID);\r\n\t}\r\n#endif\r\n\r\n\tstring.Empty();\r\n\tstring.Format(_T("(Res ID:%d)"), uID);\r\n\treturn string;\r\n}\r\n\r\n'
    
    new_c = content[:start_idx] + good + content[end_idx:]
    with open('mfc/mfc_app.cpp', 'wb') as f:
        f.write(new_c)
    print("mfc_app.cpp reparado")
else:
    print("Indices no encontrados.")
