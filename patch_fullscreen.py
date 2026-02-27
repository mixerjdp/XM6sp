import sys

file_path = 'mfc/mfc_cmd.cpp'
with open(file_path, 'r', encoding='latin-1') as f:
    lines = f.readlines()

start_line = -1
end_line = -1

for i, line in enumerate(lines):
    if 'void CFrmWnd::OnFullScreen()' in line:
        start_line = i
    if start_line != -1 and line.strip() == '}' and i > start_line:
        # Check if next few lines are OnFullScreenUI to be sure
        if i + 10 < len(lines):
            found_next = False
            for j in range(i+1, i+10):
                if 'void CFrmWnd::OnFullScreenUI' in lines[j]:
                    found_next = True
                    break
            if found_next:
                end_line = i
                break

if start_line != -1 and end_line != -1:
    new_func = [
        'void CFrmWnd::OnFullScreen()\n',
        '{\n',
        '\tBOOL bEnable;\n',
        '\tBOOL bSound;\n',
        '\tBOOL bMouse;\n',
        '\n',
        '\t// Detener emulacion y sonido brevemente para el cambio de layout\n',
        '\tbEnable = GetScheduler()->IsEnable();\n',
        '\tbMouse = GetInput()->GetMouseMode();\n',
        '\tif (bMouse) {\n',
        '\t\tGetInput()->SetMouseMode(FALSE);\n',
        '\t}\n',
        '\tGetScheduler()->Enable(FALSE);\n',
        '\t::LockVM();\n',
        '\t::UnlockVM();\n',
        '\t::LockVM();\n',
        '\tbSound = GetSound()->IsEnable();\n',
        '\tGetSound()->Enable(FALSE);\n',
        '\t::UnlockVM();\n',
        '\n',
        '\tif (m_bFullScreen) {\n',
        '\t\t// Salir de pantalla completa (Modo Ventana)\n',
        '\t\tExitBorderlessFullscreen();\n',
        '\t\tm_bFullScreen = FALSE;\n',
        '\n',
        '\t\tShowCaption();\n',
        '\t\tShowMenu();\n',
        '\t\tShowStatus();\n',
        '\n',
        '\t\tHideTaskBar(FALSE, TRUE);\n',
        '\t\tInitPos(FALSE);\n',
        '\t\tRecalcLayout();\n',
        '\t}\n',
        '\telse {\n',
        '\t\t// Entrar a pantalla completa (Borderless)\n',
        '\t\tEnterBorderlessFullscreen();\n',
        '\t\tm_bFullScreen = TRUE;\n',
        '\n',
        '\t\tHideTaskBar(TRUE, TRUE);\n',
        '\t\tInitPos(FALSE);\n',
        '\t\tRecalcStatusView();\n',
        '\t}\n',
        '\n',
        '\t// Restaurar estados\n',
        '\tGetScheduler()->Enable(bEnable);\n',
        '\tGetSound()->Enable(bSound);\n',
        '\tGetInput()->SetMouseMode(bMouse);\n',
        '\tResetCaption();\n',
        '\tResetStatus();\n',
        '\n',
        '\t// Auto-captura de mouse si es necesario\n',
        '\tif (m_bAutoMouse && bEnable && !bMouse && m_bFullScreen) {\n',
        '\t\tOnMouseMode();\n',
        '\t}\n',
        '}\n'
    ]
    
    lines[start_line:end_line+1] = new_func
    
    with open(file_path, 'w', encoding='latin-1') as f:
        f.writelines(lines)
    print("Successfully replaced OnFullScreen from line %d to %d" % (start_line+1, end_line+1))
else:
    print("Failed to find OnFullScreen range: start=%d, end=%d" % (start_line, end_line))
    sys.exit(1)
