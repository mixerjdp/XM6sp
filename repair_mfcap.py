import os

file_path = 'mfc/mfc_app.cpp'

if os.path.exists(file_path):
    with open(file_path, 'rb') as f:
        content = f.read()

    # Patrones rotos especificos detectados del C2001
    bad1 = b'TRACE(_T("Ge\r\n\t\t\t\t\t\t  ID:%d\r\n\'al cargar cadena "), uID);'
    bad2 = b'ID:%d\r\n\'al cargar cadena '
    bad3 = b'\r\n\'dena ID:%d\\n'
    bad4 = b'ID:%d\r\n\'al cargar cadena'

    # Un brute force replace bajando a bytes.
    # El problema es que el inyector anterior partio la linea a la mitad:
    # TRACE(_T("Error al cargar cadena ID:%d\n"), uID); -> 
    # se convirtio a b"\\t\\t\\tTRACE(_T(\\"Ge\\r\\n\\t\\t\\t\\t\\t\\t  ID:%d\\r\\n\\'al cargar cadena \\"), uID);\\r\\n"
    
    # Voy a buscar por indice todas las llaves '{' y '}' despues de IsJapanese()
    
    lines = content.split(b'\r\n')
    
    # Reparando iterativamente cualquier linea que tenga comillas sin terminar
    # pero como es solo TRACE puedo re-hardcodear el bloque exacto.

    for i in range(120, 150):
        if i < len(lines):
             # Si encuentro la comilla abierta desbalanceada:
             if b'TRACE(_T("Ge' in lines[i]:
                  lines[i] = b'\t\t\tTRACE(_T("Error al cargar cadena ID:%d\\n"), uID);'
             elif b'ID:%d' in lines[i] and b'al cargar cadena' in lines[i]:
                  lines[i] = b'' # borrar basura residual
             elif b'TRACE(_T("Error al cargar ca' in lines[i]:
                  pass
             elif b'dena ID:' in lines[i]:
                  lines[i] = b'' # basura de una division
             elif b'uID);' in lines[i] and not b'TRACE' in lines[i]:
                  lines[i] = b'' # basura
             
    with open(file_path, 'wb') as f:
        f.write(b'\r\n'.join(lines))
    print("Corregido el archivo mfc_app.cpp directamente por bytes.")

