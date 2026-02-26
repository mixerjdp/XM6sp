import sys

with open('mfc/mfc_cmd.cpp', 'rb') as f:
    data = f.read()

start_sig = b'void CFrmWnd::OnFullScreen()\r\n{\r\n'
start_idx = data.find(start_sig)
if start_idx == -1:
    print('Start not found')
    sys.exit(1)

body_start = start_idx + len(start_sig)

end_sig = b'//---------------------------------------------------------------------------\r\n//\r\n//\tUI de pantalla completa'
end_idx = data.find(end_sig, body_start)
if end_idx == -1:
    print('End not found')
    sys.exit(1)

# Backtrack to find the last closing brace '}' of OnFullScreen before the end_sig
last_brace = data.rfind(b'}', body_start, end_idx)
if last_brace == -1:
    print('Brace not found')
    sys.exit(1)

new_body = b'''\tBOOL bEnable;
\tBOOL bSound;
\tBOOL bMouse;
\tCRect rectWnd;

\tbEnable = GetScheduler()->IsEnable();
\tbMouse = GetInput()->GetMouseMode();
\tif (bMouse) {
\t\tGetInput()->SetMouseMode(FALSE);
\t}
\tGetScheduler()->Enable(FALSE);
\t::LockVM();
\t::UnlockVM();
\t::LockVM();
\tbSound = GetSound()->IsEnable();
\tGetSound()->Enable(FALSE);
\t::UnlockVM();

\tif (m_bFullScreen) {
\t\tExitBorderlessFullscreen();

\t\tif (IsZoomed()) {
\t\t\tShowWindow(SW_RESTORE);
\t\t}

\t\tm_bFullScreen = FALSE;
\t\tShowCaption();
\t\tShowMenu();
\t\tShowStatus();
\t\tHideTaskBar(FALSE, TRUE);

\t\tInitPos(FALSE);
\t\tRecalcLayout();
\t\t
\t\tif (m_pDrawView) {
\t\t\tm_pDrawView->Invalidate(FALSE);
\t\t\tm_pDrawView->RequestPresent();
\t\t}

\t\tGetScheduler()->Enable(bEnable);
\t\tGetSound()->Enable(bSound);
\t\tGetInput()->SetMouseMode(bMouse);
\t\tResetCaption();
\t\tResetStatus();

\t\tif (m_bAutoMouse && bMouse) {
\t\t\tOnMouseMode();
\t\t}
\t\treturn;
\t}

\tGetWindowRect(&rectWnd);
\tm_nWndLeft = rectWnd.left;
\tm_nWndTop = rectWnd.top;

\tm_bFullScreen = TRUE;
\tShowCaption();
\tShowMenu();

\tHideTaskBar(TRUE, TRUE);

\tEnterBorderlessFullscreen();

\tRecalcStatusView();

\tif (m_pDrawView) {
\t\tm_pDrawView->Invalidate(FALSE);
\t\tm_pDrawView->RequestPresent();
\t}

\tGetScheduler()->Enable(bEnable);
\tGetSound()->Enable(bSound);
\tGetInput()->SetMouseMode(bMouse);
\tResetCaption();
\tResetStatus();

\tif (m_bAutoMouse && bEnable && !bMouse) {
\t\tOnMouseMode();
\t}
'''

new_data = data[:body_start] + new_body + data[last_brace:]

with open('mfc/mfc_cmd.cpp', 'wb') as f:
    f.write(new_data)

print('Success')
