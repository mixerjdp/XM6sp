//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
//	[MFC subwindow (CPU)]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "cpu.h"
#include "memory.h"
#include "schedule.h"
#include "mfp.h"
#include "scc.h"
#include "dmac.h"
#include "iosc.h"
#include "render.h"
#include "mfc_sub.h"
#include "mfc_draw.h"
#include "mfc_res.h"
#include "mfc_cpu.h"

//---------------------------------------------------------------------------
//
//	Communication with cpudebug.c
//
//---------------------------------------------------------------------------
#if defined(__cplusplus)
extern "C" {
#endif	// __cplusplus

void cpudebug_disassemble(int n);
										// Disassemble one line
extern void (*cpudebug_put)(const char*);
										// Single-line output
extern DWORD debugpc;
										// Disassembly PC

#if defined(__cplusplus)
}
#endif	// __cplusplus

static char debugbuf[0x200];
										// Output buffer

//===========================================================================
//
//	History dialog
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CHistoryDlg::CHistoryDlg(UINT nID, CWnd *pParentWnd) : CDialog(nID, pParentWnd)
{
	// Initialization
	m_dwValue = 0;
	m_nBit = 32;
}

//---------------------------------------------------------------------------
//
//	Dialog initialization
//
//---------------------------------------------------------------------------
BOOL CHistoryDlg::OnInitDialog()
{
	int i;
	int nNum;
	DWORD *pData;
	CString strText;
	CComboBox *pComboBox;

	// Base class
	if (!CDialog::OnInitDialog()) {
		return FALSE;
	}

	// Generate the mask
	m_dwMask = 0;
	for (i=0; i<(int)m_nBit; i++) {
		m_dwMask <<= 1;
		m_dwMask |= 0x01;
	}

	// Clear the combo box
	pComboBox = (CComboBox*)GetDlgItem(IDC_ADDR_ADDRE);
	ASSERT(pComboBox);
	pComboBox->ResetContent();

	// Add entries to the combo box
	nNum = *(int *)GetNumPtr();
	pData = GetDataPtr();
	for (i=0; i<nNum; i++) {
		if (pData[i] > m_dwMask) {
			// If the value exceeds the mask, default to 32 bits
			strText.Format(_T("%08X"), pData[i]);
		}
		else {
			// Less than or equal to the mask
			switch (m_nBit) {
				case 8:
					strText.Format(_T("%02X"), pData[i]);
					break;
				case 16:
					strText.Format(_T("%04X"), pData[i]);
					break;
				case 24:
					strText.Format(_T("%06X"), pData[i]);
					break;
				default:
					strText.Format(_T("%08X"), pData[i]);
					break;
			}
		}
		// Add
		pComboBox->AddString(strText);
	}

	// m_dwValue is always masked
	m_dwValue &= m_dwMask;
	switch (m_nBit) {
		case 8:
			strText.Format(_T("%02X"), m_dwValue);
			break;
		case 16:
			strText.Format(_T("%04X"), m_dwValue);
			break;
		case 24:
			strText.Format(_T("%06X"), m_dwValue);
			break;
		default:
			strText.Format(_T("%08X"), m_dwValue);
			break;
	}
	pComboBox->SetWindowText(strText);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Dialog OK
//
//---------------------------------------------------------------------------
void CHistoryDlg::OnOK()
{
	CComboBox *pComboBox;
	CString strText;
	int i;
	int nHit;
	int nNum;
	DWORD *pData;

	// Get the numeric input value
	pComboBox = (CComboBox*)GetDlgItem(IDC_ADDR_ADDRE);
	ASSERT(pComboBox);
	pComboBox->GetWindowText(strText);
	m_dwValue = _tcstoul((LPCTSTR)strText, NULL, 16);

	// Check whether it matches an existing entry
	nNum = *(int *)GetNumPtr();
	pData = GetDataPtr();
	nHit = -1;
	for (i=0; i<nNum; i++) {
		if (pData[i] == m_dwValue) {
			nHit = i;
			break;
		}
	}

	// New or reused
	if (nHit >= 0) {
		// Same as the existing entry. Swap positions
		for (i=(nHit - 1); i>=0; i--) {
			pData[i + 1] = pData[i];
		}
		pData[0] = m_dwValue;
	}
	else {
		// New entry. Shift the existing entries down
		for (i=9; i>=1; i--) {
			pData[i] = pData[i - 1];
		}

		// Put the most recent entry at [0]
		pData[0] = m_dwValue;

		// Up to 10 entries can be added
		if (nNum < 10) {
			*(int *)GetNumPtr() = (nNum + 1);
		}
	}

	// Mask m_dwValue and accept
	m_dwValue &= m_dwMask;

	// Base class
	CDialog::OnOK();
}

//===========================================================================
//
//	Address input dialog
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CAddrDlg::CAddrDlg(CWnd *pParent) : CHistoryDlg(IDD_ADDRDLG, pParent)
{
	// Use the English dialog template
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_ADDRDLG);
		m_nIDHelp = IDD_US_ADDRDLG;
	}

	m_nBit = 24;
}

//---------------------------------------------------------------------------
//
//	Menu configuration
//
//---------------------------------------------------------------------------
void CAddrDlg::SetupHisMenu(CMenu *pMenu)
{
	int i;
	CString string;

	ASSERT(pMenu);

	// Menu string
	for (i=0; i<(int)m_Num; i++) {
		string.Format("$%06X", m_Data[i]);
		pMenu->ModifyMenu(IDM_HISTORY_0 + i, MF_BYCOMMAND | MF_STRING,
							IDM_HISTORY_0 + i, (LPCTSTR)string);
	}

	// Delete menu item
	for (i=m_Num; i<10; i++) {
		pMenu->DeleteMenu(IDM_HISTORY_0 + i, MF_BYCOMMAND);
	}
}

//---------------------------------------------------------------------------
//
//	Get menu result
//
//---------------------------------------------------------------------------
DWORD CAddrDlg::GetAddr(UINT nID)
{
	DWORD dwAddr;
    int i;

	ASSERT((nID >= IDM_HISTORY_0) && (nID <= IDM_HISTORY_9));

	// Get address
	nID -= IDM_HISTORY_0;
	ASSERT(nID < 10);
	dwAddr = m_Data[nID];

	// Intercambiar lugar
	for (i=(int)(nID - 1); i>=0; i--) {
		m_Data[i + 1] = m_Data[i];
	}
	m_Data[0] = dwAddr;

	return dwAddr;
}

//---------------------------------------------------------------------------
//
//	Get history count pointer
//
//---------------------------------------------------------------------------
UINT* CAddrDlg::GetNumPtr()
{
	return &m_Num;
}

//---------------------------------------------------------------------------
//
//	Get history data pointer
//
//---------------------------------------------------------------------------
DWORD* CAddrDlg::GetDataPtr()
{
	return m_Data;
}

//---------------------------------------------------------------------------
//
//	History count
//
//---------------------------------------------------------------------------
UINT CAddrDlg::m_Num = 0;

//---------------------------------------------------------------------------
//
//	History data
//
//---------------------------------------------------------------------------
DWORD CAddrDlg::m_Data[10] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//===========================================================================
//
//	Register input dialog
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CRegDlg::CRegDlg(CWnd *pParent) : CHistoryDlg(IDD_REGDLG, pParent)
{
	// Use the English dialog template
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_REGDLG);
		m_nIDHelp = IDD_US_REGDLG;
	}

	m_nIndex = 0;
	m_nBit = 32;
}

//---------------------------------------------------------------------------
//
//	Dialog initialization
//
//---------------------------------------------------------------------------
BOOL CRegDlg::OnInitDialog()
{
	CWnd *pWnd;
	CString strRegister;
	CPU *pCPU;
	CPU::cpu_t reg;

	ASSERT(this);
	ASSERT(m_nIndex < 20);

	// Get CPU registers
	::LockVM();
	pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(pCPU);
	pCPU->GetCPU(&reg);
	::UnlockVM();

	// Build the string
	if (m_nIndex <= 7) {
		strRegister.Format("D%d", m_nIndex);
		m_dwValue = reg.dreg[m_nIndex];
	}
	if ((m_nIndex >= 8) && (m_nIndex <= 15)) {
		strRegister.Format("A%d", m_nIndex & 7);
		m_dwValue = reg.areg[m_nIndex & 7];
	}
	switch (m_nIndex) {
		// USP
		case 16:
			strRegister = "USP";
			if (reg.sr & 0x2000) {
				m_dwValue = reg.sp;
			}
			else {
				m_dwValue = reg.areg[7];
			}
			break;
		// SSP
		case 17:
			strRegister = "SSP";
			if (reg.sr & 0x2000) {
				m_dwValue = reg.areg[7];
			}
			else {
				m_dwValue = reg.sp;
			}
			break;
		// PC
		case 18:
			strRegister = "PC";
			m_dwValue = reg.pc;
			break;
		// SR
		case 19:
			strRegister = "SR";
			m_dwValue = reg.sr;
			break;
	}

	// Call the base class here
	if (!CHistoryDlg::OnInitDialog()) {
		return FALSE;
	}

	// Set value
	pWnd = GetDlgItem(IDC_ADDR_ADDRL);
	ASSERT(pWnd);
	pWnd->SetWindowText(strRegister);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	OK
//
//---------------------------------------------------------------------------
void CRegDlg::OnOK()
{
	CComboBox *pComboBox;
	CString string;
	DWORD dwValue;
	CPU *pCPU;
	CPU::cpu_t reg;

	ASSERT(this);

	// Get value
	pComboBox = (CComboBox*)GetDlgItem(IDC_ADDR_ADDRE);
	ASSERT(pComboBox);
	pComboBox->GetWindowText(string);
	dwValue = ::strtoul((LPCTSTR)string, NULL, 16);

	// Lock the VM and read the registers
	::LockVM();
	pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(pCPU);
	pCPU->GetCPU(&reg);

	// by indice
	if (m_nIndex <= 7) {
		reg.dreg[m_nIndex] = dwValue;
	}
	if ((m_nIndex >= 8) && (m_nIndex <= 15)) {
		reg.areg[m_nIndex & 7] = dwValue;
	}
	switch (m_nIndex) {
		// USP
		case 16:
			if (reg.sr & 0x2000) {
				reg.sp = dwValue;
			}
			else {
				reg.areg[7] = dwValue;
			}
			break;
		// SSP
		case 17:
			if (reg.sr & 0x2000) {
				reg.areg[7] = dwValue;
			}
			else {
				reg.sp = dwValue;
			}
			break;
		// PC
		case 18:
			reg.pc = dwValue & 0xfffffe;
			break;
		// SR
		case 19:
			reg.sr = (WORD)dwValue;
			break;
	}

	// Set registers, unlock the VM
	pCPU->SetCPU(&reg);
	::UnlockVM();

	// Base class
	CHistoryDlg::OnOK();
}

//---------------------------------------------------------------------------
//
//	Get history count pointer
//
//---------------------------------------------------------------------------
UINT* CRegDlg::GetNumPtr()
{
	return &m_Num;
}

//---------------------------------------------------------------------------
//
//	Get history data pointer
//
//---------------------------------------------------------------------------
DWORD* CRegDlg::GetDataPtr()
{
	return m_Data;
}

//---------------------------------------------------------------------------
//
//	History count
//
//---------------------------------------------------------------------------
UINT CRegDlg::m_Num = 0;

//---------------------------------------------------------------------------
//
//	History data
//
//---------------------------------------------------------------------------
DWORD CRegDlg::m_Data[10] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//===========================================================================
//
//	Data input dialog
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CDataDlg::CDataDlg(CWnd *pParent) : CHistoryDlg(IDD_DATADLG, pParent)
{
	// Use the English dialog template
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_DATADLG);
		m_nIDHelp = IDD_US_DATADLG;
	}

	// Initialize just in case
	m_dwAddr = 0;
	m_nSize = 0;
}

//---------------------------------------------------------------------------
//
//	Dialog initialization
//
//---------------------------------------------------------------------------
BOOL CDataDlg::OnInitDialog()
{
	CWnd *pWnd;
	CString string;

	ASSERT(this);
	ASSERT(m_dwAddr < 0x1000000);

	// Determine the address and bit width
	switch (m_nSize) {
		// Byte
		case 0:
			string.Format("$%06X (B)", m_dwAddr);
			m_nBit = 8;
			break;
		// Word
		case 1:
			string.Format("$%06X (W)", m_dwAddr);
			m_nBit = 16;
			break;
		// Long
		case 2:
			string.Format("$%06X (L)", m_dwAddr);
			m_nBit = 32;
			break;
		// Other
		default:
			ASSERT(FALSE);
			break;
	}

	// Base class
	if (!CHistoryDlg::OnInitDialog()) {
		return FALSE;
	}

	// Configure after initialization
	pWnd = GetDlgItem(IDC_ADDR_ADDRL);
	ASSERT(pWnd);
	pWnd->SetWindowText(string);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get history count pointer
//
//---------------------------------------------------------------------------
UINT* CDataDlg::GetNumPtr()
{
	return &m_Num;
}

//---------------------------------------------------------------------------
//
//	Get history data pointer
//
//---------------------------------------------------------------------------
DWORD* CDataDlg::GetDataPtr()
{
	return m_Data;
}

//---------------------------------------------------------------------------
//
//	History count
//
//---------------------------------------------------------------------------
UINT CDataDlg::m_Num = 0;

//---------------------------------------------------------------------------
//
//	History data
//
//---------------------------------------------------------------------------
DWORD CDataDlg::m_Data[10] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//===========================================================================
//
//	CPU register window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CCPURegWnd::CCPURegWnd()
{
	// Window parameter definitions
	m_dwID = MAKEID('M', 'P', 'U', 'R');
	::GetMsg(IDS_SWND_CPUREG, m_strCaption);
	m_nWidth = 27;
	m_nHeight = 10;

	// Get CPU
	m_pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pCPU);
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CCPURegWnd, CSubTextWnd)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_CONTEXTMENU()
	ON_COMMAND_RANGE(IDM_REG_D0, IDM_REG_SR, OnReg)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Configuration
//
//---------------------------------------------------------------------------
void FASTCALL CCPURegWnd::Setup()
{
	CPU::cpu_t buf;
	int i;
	CString string;

	ASSERT(this);

	// Clear
	Clear();

	// Get CPU registers
	m_pCPU->GetCPU(&buf);

	// Set (D, A)
	for (i=0; i<8; i++) {
		string.Format("D%1d  %08X", i, buf.dreg[i]);
		SetString(0, i, string);

		string.Format("A%1d  %08X", i, buf.areg[i]);
		SetString(15, i, string);
	}

	// Set (Stack)
	if (buf.sr & 0x2000) {
		string.Format("USP %08X", buf.sp);
		SetString(0, 8, string);
		string.Format("SSP %08X", buf.areg[7]);
		SetString(15, 8, string);
	}
	else {
		string.Format("USP %08X", buf.areg[7]);
		SetString(0, 8, string);
		string.Format("SSP %08X", buf.sp);
		SetString(15, 8, string);
	}

	// Set (Other)
	string.Format("PC    %06X", buf.pc);
	SetString(0, 9, string);
	string.Format("SR      %04X", buf.sr);
	SetString(15, 9, string);
}

//---------------------------------------------------------------------------
//
//	Left double-click
//
//---------------------------------------------------------------------------
void CCPURegWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	int x;
	int y;
	int index;
	CRegDlg dlg(this);

	// Calcular x, y
	x = point.x / m_tmWidth;
	y = point.y / m_tmHeight;

	// Get index
	if (y < 8) {
		if (x < 15) {
			// D0-D7
			index = y;
		}
		else {
			// A0-A7
			index = y + 8;
		}
	}
	else {
		index = y - 8;
		index <<= 1;
		if (x >= 15) {
			index++;
		}
		index += 16;
	}

	printf("%d", nFlags);

	// Execute dialogo
	dlg.m_nIndex = index;
	dlg.DoModal();
}

//---------------------------------------------------------------------------
//
//	Context menu
//
//---------------------------------------------------------------------------
void CCPURegWnd::OnContextMenu(CWnd *pWnd, CPoint point)
{
	CRect rect;
	CMenu menu;
	CMenu *pMenu;

	// Determinar si se presiono dentro del area del cliente
	GetClientRect(&rect);
	ClientToScreen(&rect);
	if (!rect.PtInRect(point)) {
		CSubTextWnd::OnContextMenu(pWnd, point);
		return;
	}

	// Load menu
	menu.LoadMenu(IDR_REGMENU);
	pMenu = menu.GetSubMenu(0);

	// Menu configuration
	SetupRegMenu(pMenu, m_pCPU, TRUE);

	// Execute menu
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							point.x, point.y, this);
}

//---------------------------------------------------------------------------
//
//	Menu configuration
//
//---------------------------------------------------------------------------
void CCPURegWnd::SetupRegMenu(CMenu *pMenu, CPU *pCPU, BOOL bSR)
{
	int i;
	CString string;
	CPU::cpu_t reg;

	ASSERT(pMenu);
	ASSERT(pCPU);

	// Get CPU registers
	::LockVM();
	pCPU->GetCPU(&reg);
	::UnlockVM();

	// Configuration (D)
	for (i=0; i<8; i++) {
		string.Format("D%1d ($%08X)", i, reg.dreg[i]);
		pMenu->ModifyMenu(IDM_REG_D0 + i, MF_BYCOMMAND | MF_STRING,
							IDM_REG_D0 + i, (LPCTSTR)string);
	}
	// Configuration (A)
	for (i=0; i<8; i++) {
		string.Format("A%1d ($%08X)", i, reg.areg[i]);
		pMenu->ModifyMenu(IDM_REG_A0 + i, MF_BYCOMMAND | MF_STRING,
							IDM_REG_A0 + i, (LPCTSTR)string);
	}
	// Configuration (USP, SSP)
	if (reg.sr & 0x2000) {
		string.Format("USP ($%08X)", reg.sp);
		pMenu->ModifyMenu(IDM_REG_USP, MF_BYCOMMAND | MF_STRING, IDM_REG_USP, (LPCTSTR)string);
		string.Format("SSP ($%08X)", reg.areg[7]);
		pMenu->ModifyMenu(IDM_REG_SSP, MF_BYCOMMAND | MF_STRING, IDM_REG_SSP, (LPCTSTR)string);
	}
	else {
		string.Format("USP ($%08X)", reg.areg[7]);
		pMenu->ModifyMenu(IDM_REG_USP, MF_BYCOMMAND | MF_STRING, IDM_REG_USP, (LPCTSTR)string);
		string.Format("SSP ($%08X)", reg.sp);
		pMenu->ModifyMenu(IDM_REG_SSP, MF_BYCOMMAND | MF_STRING, IDM_REG_SSP, (LPCTSTR)string);
	}

	// Configuration (PC)
	string.Format("PC ($%06X)", reg.pc);
	pMenu->ModifyMenu(IDM_REG_PC, MF_BYCOMMAND | MF_STRING, IDM_REG_PC, (LPCTSTR)string);

	// Configuration (SR)
	if (bSR) {
		string.Format("SR ($%04X)", reg.sr);
		pMenu->ModifyMenu(IDM_REG_SR, MF_BYCOMMAND | MF_STRING, IDM_REG_SR, (LPCTSTR)string);
	}
}

//---------------------------------------------------------------------------
//
//	Get register value
//
//---------------------------------------------------------------------------
DWORD CCPURegWnd::GetRegValue(CPU *pCPU, UINT uID)
{
	CPU::cpu_t reg;

	ASSERT(pCPU);
	ASSERT((uID >= IDM_REG_D0) && (uID <= IDM_REG_SR));

	// Get CPU registers
	::LockVM();
	pCPU->GetCPU(&reg);
	::UnlockVM();

	// D0 a D7
	if (uID <= IDM_REG_D7) {
		return reg.dreg[uID - IDM_REG_D0];
	}

	// A0 a A7
	if (uID <= IDM_REG_A7) {
		return reg.areg[uID - IDM_REG_A0];
	}

	// USP
	if (uID == IDM_REG_USP) {
		if (reg.sr & 0x2000) {
			return reg.sp;
		}
		else {
			return reg.areg[7];
		}
	}

	// SSP
	if (uID == IDM_REG_SSP) {
		if (reg.sr & 0x2000) {
			return reg.areg[7];
		}
		else {
			return reg.sp;
		}
	}

	// PC
	if (uID == IDM_REG_PC) {
		return reg.pc;
	}

	// SR
	ASSERT(uID == IDM_REG_SR);
	return reg.sr;
}

//---------------------------------------------------------------------------
//
//	Register command
//
//---------------------------------------------------------------------------
void CCPURegWnd::OnReg(UINT nID)
{
	CRegDlg dlg(this);

	ASSERT((nID >= IDM_REG_D0) && (nID <= IDM_REG_SR));

	// Convert
	nID -= IDM_REG_D0;

	// Let the dialog handle it
	dlg.m_nIndex = nID;
	dlg.DoModal();
}

//===========================================================================
//
//	Interrupt window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CIntWnd::CIntWnd()
{
	// Window parameter definitions
	m_dwID = MAKEID('I', 'N', 'T', ' ');
	::GetMsg(IDS_SWND_INT, m_strCaption);
	m_nWidth = 47;
	m_nHeight = 9;

	// Get CPU
	m_pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pCPU);
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CIntWnd, CSubTextWnd)
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Configuration
//
//---------------------------------------------------------------------------
void FASTCALL CIntWnd::Setup()
{
	static const char *desc_table[] = {
		"INTSW",
		"MFP",
		"SCC",
		"(Ext.)",
		"DMAC",
		"(Ext.)",
		"IOSC"
	};
	int y;
	int i;
	int level;
	CString string;
	CPU::cpu_t cpu;

	// Get CPU data
	m_pCPU->GetCPU(&cpu);
	level = (cpu.sr >> 8);
	level &= 0x07;

	// Clear
	Clear();
	y = 0;

	// Guide
	SetString(0, y, "(High)  Device  Mask  Vector     Req        Ack");
	y++;

	// Seven-level processing
	for (i=7; i>=1; i--) {
		// Set interrupt name
		string.Format("Level%1d  ", i);
		string += desc_table[7 - i];
		SetString(0, y, string);

		// Mask
		if (i < 7) {
			if (i <= level) {
				SetString(16, y, "Mask");
			}
		}

		// Is it pending?
		if (cpu.intr[0] & 0x80) {
			// If pending, display the vector
			string.Format("$%02X", cpu.intr[i]);
			SetString(22, y, string);
		}

		// Request counter
		string.Format("%10d", cpu.intreq[i]);
		SetString(26, y, string);

		// Response counter
		string.Format("%10d", cpu.intack[i]);
		SetString(37, y, string);

		// Next
		y++;
		cpu.intr[0] <<= 1;
	}

	// Guide
	SetString(0, y, "(Low)");
}

//---------------------------------------------------------------------------
//
//	Left double-click
//
//---------------------------------------------------------------------------
void CIntWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	int y;
	int level;
	CPU::cpu_t cpu;

	// Calcular y
	y = point.y / m_tmHeight;

	// y=1,2,3,4,5,6,7 corresponden respectivamente a int7,6,5,4,3,2,1
	level = 8 - y;
	if ((level < 1) || (level > 7)) {
		return;
	}

	// Lock and fetch data
	::LockVM();
	m_pCPU->GetCPU(&cpu);

	// Clear
	ASSERT((level >= 1) && (level <= 7));
	cpu.intreq[level] = 0;
	cpu.intack[level] = 0;

	// Set data and unlock
	m_pCPU->SetCPU(&cpu);
	printf("%d", nFlags);
	::UnlockVM();
}

//===========================================================================
//
//	Disassembly window
//
//===========================================================================

#if defined(__cplusplus)
extern "C" {
#endif
//---------------------------------------------------------------------------
//
//	cpudebug.c string output
//
//---------------------------------------------------------------------------
void disasm_put(const char *s)
{
	strcpy(debugbuf, s);
}

//---------------------------------------------------------------------------
//
//	Memory device
//
//---------------------------------------------------------------------------
static Memory* cpudebug_memory;

//---------------------------------------------------------------------------
//
//	cpudebug.c word read
//
//---------------------------------------------------------------------------
WORD cpudebug_fetch(DWORD addr)
{
	WORD w;

	ASSERT(cpudebug_memory);

	addr &= 0xfffffe;
	w = (WORD)(cpudebug_memory->ReadOnly(addr));
	w <<= 8;
	w |= cpudebug_memory->ReadOnly(addr + 1);

	return w;
}
#if defined(__cplusplus)
};
#endif

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CDisasmWnd::CDisasmWnd(int index)
{
	// Up to 8 disassembly window types
	ASSERT((index >= 0) && (index <= 0x07));

	// Window parameter definitions
	m_dwID = MAKEID('D', 'I', 'S', (index + 'A'));
	::GetMsg(IDS_SWND_DISASM, m_strCaption);
	m_nWidth = 70;
	m_nHeight = 16;

	// Window parameter definitions (scroll)
	m_ScrlWidth = 70;
	m_ScrlHeight = 0x8000;

	// The first window is synchronized with the PC; the others are not
	if (index == 0) {
		m_bSync = TRUE;
	}
	else {
		m_bSync = FALSE;
	}

	// Other
	m_pAddrBuf = NULL;
	m_Caption = m_strCaption;
	m_CaptionSet = "";

	// Get the device
	m_pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pCPU);
	cpudebug_memory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(cpudebug_memory);
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);
	m_pMFP = (MFP*)::GetVM()->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(m_pMFP);
	m_pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(m_pMemory);
	m_pSCC = (SCC*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'C', ' '));
	ASSERT(m_pSCC);
	m_pDMAC = (DMAC*)::GetVM()->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	ASSERT(m_pDMAC);
	m_pIOSC = (IOSC*)::GetVM()->SearchDevice(MAKEID('I', 'O', 'S', 'C'));
	ASSERT(m_pIOSC);

	// Initialize the address to the PC
	m_dwSetAddr = m_pCPU->GetPC();
	m_dwAddr = m_dwSetAddr;
	m_dwAddr = m_dwAddr & 0xff0000;
	m_dwPC = 0xffffffff;

	// Disassembly buffer linkage
	::cpudebug_put = ::disasm_put;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CDisasmWnd, CSubTextScrlWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_VSCROLL()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_DIS_NEWWIN, OnNewWin)
	ON_COMMAND(IDM_DIS_PC, OnPC)
	ON_COMMAND(IDM_DIS_SYNC, OnSync)
	ON_COMMAND(IDM_DIS_ADDR, OnAddr)
	ON_COMMAND_RANGE(IDM_REG_D0, IDM_REG_PC, OnReg)
	ON_COMMAND_RANGE(IDM_STACK_0, IDM_STACK_F, OnStack)
	ON_COMMAND_RANGE(IDM_DIS_BREAKP0, IDM_DIS_BREAKP7, OnBreak)
	ON_COMMAND_RANGE(IDM_HISTORY_0, IDM_HISTORY_9, OnHistory)
	ON_COMMAND_RANGE(IDM_DIS_RESET, IDM_DIS_FLINE, OnCPUExcept)
	ON_COMMAND_RANGE(IDM_DIS_TRAP0, IDM_DIS_TRAPF, OnTrap)
	ON_COMMAND_RANGE(IDM_DIS_MFP0, IDM_DIS_MFPF, OnMFP)
	ON_COMMAND_RANGE(IDM_DIS_SCC0, IDM_DIS_SCC7, OnSCC)
	ON_COMMAND_RANGE(IDM_DIS_DMAC0, IDM_DIS_DMAC7, OnDMAC)
	ON_COMMAND_RANGE(IDM_DIS_IOSC0, IDM_DIS_IOSC3, OnIOSC)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CDisasmWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	int i;

	// Ensure the address buffer first
	m_pAddrBuf = new DWORD[ m_nHeight ];
	for (i=0; i<m_nHeight; i++) {
		m_pAddrBuf[i] = 0xffffffff;
	}

	// Base class
	if (CSubTextScrlWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

	// Address initialization
	SetAddr(m_dwSetAddr);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window destruction
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnDestroy()
{
	m_bEnable = FALSE;

	// Release the address buffer
	if (m_pAddrBuf) {
		delete[] m_pAddrBuf;
		m_pAddrBuf = NULL;
	}

	// Call the base class
	CSubTextScrlWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	Resize
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnSize(UINT nType, int cx, int cy)
{
	CRect rect;
	int i;

	ASSERT(this);
	ASSERT(cx >= 0);
	ASSERT(cy >= 0);

	// Release the address buffer if it exists
	::LockVM();
	if (m_pAddrBuf) {
		delete[] m_pAddrBuf;
		m_pAddrBuf = NULL;
	}
	::UnlockVM();

	// Base class (within this, CDisasmWnd::OnSize can be called again)
	CSubTextScrlWnd::OnSize(nType, cx, cy);

	// Reacquire the address buffer. Also verify release handling.
	::LockVM();
	if (m_pAddrBuf) {
		delete[] m_pAddrBuf;
		m_pAddrBuf = NULL;
	}
	m_pAddrBuf = new DWORD[ m_nHeight ];
	for (i=0; i<m_nHeight; i++) {
		m_pAddrBuf[i] = 0xffffffff;
	}
	::UnlockVM();

	// Restore the address
	SetAddr(m_dwSetAddr);
}

//---------------------------------------------------------------------------
//
//	Left click
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
	OnLButtonDblClk(nFlags, point);
}

//---------------------------------------------------------------------------
//
//	Left double-click
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	int y;
	DWORD dwAddr;

	// Abort if there is no address buffer
	if (!m_pAddrBuf) {
		return;
	}

	// Get the address and verify it
	y = point.y / m_tmHeight;
	dwAddr = m_pAddrBuf[y];
	if (dwAddr >= 0x01000000) {
		return;
	}

	// Lock the VM
	::LockVM();

	// If there is an address, clear it; otherwise set it
	if (m_pScheduler->IsBreak(dwAddr) >= 0) {
		m_pScheduler->DelBreak(dwAddr);
	}
	else {
		m_pScheduler->SetBreak(dwAddr);
	}
	printf("%d", nFlags);
	// Unlock the VM
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Vertical scroll
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar)
{
	SCROLLINFO si;
	DWORD dwDiff;
	DWORD dwAddr;
	int i;

	// Get scroll information
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_VERT, &si, SIF_ALL);

	// By scroll bar code
	switch (nSBCode) {
		// Up
		case SB_TOP:
			m_ScrlY = si.nMin;
			break;

		// Down
		case SB_BOTTOM:
			m_ScrlY = si.nMax;
			break;

		// 1 line up
		case SB_LINEUP:
			// Check the address buffer
			if (m_pAddrBuf) {
				// Get the previous address and compute the difference
				dwDiff = GetPrevAddr(m_pAddrBuf[0]);
				dwDiff = m_pAddrBuf[0] - dwDiff;

				// If there is a difference, decrement; otherwise use -1
				if (dwDiff > 0) {
					dwDiff >>= 1;
					m_ScrlY -= dwDiff;
				}
				else {
					m_ScrlY--;
				}
			}
			break;

		// 1 line down
		case SB_LINEDOWN:
			// Inspect the address buffer and advance one instruction
			if (m_nHeight >= 2) {
				if (m_pAddrBuf) {
					dwDiff = m_pAddrBuf[1] - m_pAddrBuf[0];
					dwDiff >>= 1;
					m_ScrlY += dwDiff;
				}
			}

			break;

		// 1 page up
		case SB_PAGEUP:
			// Check the address buffer
			if (m_pAddrBuf) {
				dwAddr = m_pAddrBuf[0];
				for (i=0; i<m_nHeight-1; i++) {
					// Get the previous address and compute the difference
					dwDiff = GetPrevAddr(dwAddr);
					dwDiff = dwAddr - dwDiff;
					dwAddr -= dwDiff;

					// If there is a difference, decrement; otherwise use -1
					if (dwDiff > 0) {
						dwDiff >>= 1;
						m_ScrlY -= dwDiff;
					}
					else {
						m_ScrlY--;
					}

					// Check desbordamiento
					if (m_ScrlY < 0) {
						m_ScrlY = 0;
					}
				}

				// Consider the case where we could not step back at all
				if (dwAddr == m_pAddrBuf[0]) {
					m_ScrlY -= si.nPage;
				}
			}
			break;

		// 1 page down
		case SB_PAGEDOWN:
			// Inspect the address buffer and advance m_nHeight instructions
			if (m_pAddrBuf) {
				dwDiff = m_pAddrBuf[m_nHeight - 1] - m_pAddrBuf[0];
				dwDiff >>= 1;
				m_ScrlY += dwDiff;
			}
			break;

		// Move thumb
		case SB_THUMBPOSITION:
			m_ScrlY = nPos;
			break;
		case SB_THUMBTRACK:
			m_ScrlY = nPos;
			break;
	}

	// Check desbordamiento
	if (m_ScrlY < 0) {
		m_ScrlY = 0;
	}
	if (m_ScrlY > si.nMax) {
		m_ScrlY = si.nMax;
	}

	pBar = NULL;
	// Set
	SetupScrlV();
}

//---------------------------------------------------------------------------
//
//	Context menu
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnContextMenu(CWnd *pWnd, CPoint point)
{
	CRect rect;
	CMenu menu;
	CMenu *pMenu;

	// Determinar si se presiono dentro del area del cliente
	GetClientRect(&rect);
	ClientToScreen(&rect);
	if (!rect.PtInRect(point)) {
		CSubTextScrlWnd::OnContextMenu(pWnd, point);
		return;
	}

	// Load menu
	if (::IsJapanese()) {
		menu.LoadMenu(IDR_DISMENU);
	}
	else {
		menu.LoadMenu(IDR_US_DISMENU);
	}
	pMenu = menu.GetSubMenu(0);

	// Configuration
	SetupContext(pMenu);

	// Execute menu
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							point.x, point.y, this);
}

//---------------------------------------------------------------------------
//
//	Context menu configuration
//
//---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetupContext(CMenu *pMenu)
{
	CMenu *pSubMenu;
	int i;

	ASSERT(pMenu);

	// Sync with PC
	if (m_bSync) {
		pMenu->CheckMenuItem(IDM_DIS_SYNC, MF_BYCOMMAND | MF_CHECKED);
	}

	// New window
	if (!m_pDrawView->IsNewWindow(TRUE)) {
		pMenu->EnableMenuItem(IDM_DIS_NEWWIN, MF_BYCOMMAND | MF_GRAYED);
	}

	// MPU registers, Stack, Address history
	CCPURegWnd::SetupRegMenu(pMenu, m_pCPU, FALSE);
	CMemoryWnd::SetupStackMenu(pMenu, m_pMemory, m_pCPU);
	CAddrDlg::SetupHisMenu(pMenu);

	// Breakpoint
	SetupBreakMenu(pMenu, m_pScheduler);

	// Bloquear VM
	::LockVM();

	// Prepare interrupt vectors
	pSubMenu = pMenu->GetSubMenu(9);

	// Standard MPU
	SetupVector(pSubMenu, 0, 1, 11);

	// trap #x
	SetupVector(pSubMenu, 12, 0x20, 16);

	// MFP
	SetupVector(pSubMenu, 29, (m_pMFP->GetVR() & 0xf0), 16);

	// SCC
	for (i=0; i<8; i++) {
		SetupVector(pSubMenu, 46 + i, m_pSCC->GetVector(i), 1);
	}

	// DMAC
	for (i=0; i<8; i++) {
		SetupVector(pSubMenu, 55 + i, m_pDMAC->GetVector(i), 1);
	}

	// IOSC
	SetupVector(pSubMenu, 64, m_pIOSC->GetVector(), 4);

	// Desbloquear VM
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Interrupt vector configuration
//
//---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetupVector(CMenu *pMenu, UINT index, DWORD vector, int num)
{
	int i;
	DWORD handler;

	ASSERT(pMenu);
	ASSERT(num > 0);

	// Initialize interrupt vector addresses
	vector <<= 2;

	// loop
	for (i=0; i<num; i++) {
		// Get interrupt controller address
		handler = (DWORD)m_pMemory->ReadOnly(vector + 1);
		handler <<= 8;
		handler |= (DWORD)m_pMemory->ReadOnly(vector + 2);
		handler <<= 8;
		handler |= (DWORD)m_pMemory->ReadOnly(vector + 3);
		vector += 4;

		// Set address
		SetupAddress(pMenu, index, handler);
		index++;
	}
}

//---------------------------------------------------------------------------
//
//  Address configuration
//
//---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetupAddress(CMenu *pMenu, UINT index, DWORD addr)
{
	CString string;
	CString menustr;
	int ext;
	UINT id;

	ASSERT(pMenu);
	ASSERT(addr <= 0xffffff);

	// Get the current string
	pMenu->GetMenuString(index, string, MF_BYPOSITION);

	// Find the opening parenthesis and keep only the text that follows it
	ext = string.Find(" : ");
	if (ext >= 0) {
		menustr = string.Mid(ext + 3);
	}
	else {
		menustr = string;
	}

	// Build the string ($)
	string.Format("$%06X : ", addr);
	string += menustr;

	// Set string
	id = pMenu->GetMenuItemID(index);
	pMenu->ModifyMenu(index, MF_BYPOSITION | MF_STRING, id, string);
}

//---------------------------------------------------------------------------
//
//	New window
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnNewWin()
{
	CDisasmWnd *pDisasmWnd;
	DWORD dwAddr;

	// Request the parent window to create a new window
	pDisasmWnd = (CDisasmWnd*)m_pDrawView->NewWindow(TRUE);

	// If it succeeds, pass my own address
	if (pDisasmWnd) {
		dwAddr = m_ScrlY * 2;
		dwAddr += m_dwAddr;
		pDisasmWnd->SetAddr(dwAddr);
	}
}

//---------------------------------------------------------------------------
//
//	Move to PC
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnPC()
{
	// Set the address to the current PC (Refresh runs internally)
	SetAddr(m_pCPU->GetPC());
}

//---------------------------------------------------------------------------
//
//	Sync with PC
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnSync()
{
	m_bSync = (!m_bSync);
}

//---------------------------------------------------------------------------
//
//	Address input
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnAddr()
{
	CAddrDlg dlg(this);

	// Execute dialogo
	dlg.m_dwValue = m_dwSetAddr;
	if (dlg.DoModal() != IDOK) {
		return;
	}

	// Set address
	SetAddr(dlg.m_dwValue);
}

//---------------------------------------------------------------------------
//
//	Registers MPU
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnReg(UINT nID)
{
	DWORD dwAddr;

	ASSERT((nID >= IDM_REG_D0) && (nID <= IDM_REG_PC));

	dwAddr = CCPURegWnd::GetRegValue(m_pCPU, nID);
	SetAddr(dwAddr);
}

//---------------------------------------------------------------------------
//
//	Stack
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnStack(UINT nID)
{
	DWORD dwAddr;

	ASSERT((nID >= IDM_STACK_0) && (nID <= IDM_STACK_F));

	dwAddr = CMemoryWnd::GetStackAddr(nID, m_pMemory, m_pCPU);
	SetAddr(dwAddr);
}

//---------------------------------------------------------------------------
//
//	Breakpoint
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnBreak(UINT nID)
{
	DWORD dwAddr;

	ASSERT((nID >= IDM_DIS_BREAKP0) && (nID <= IDM_DIS_BREAKP7));

	dwAddr = GetBreak(nID, m_pScheduler);
	SetAddr(dwAddr);
}

//---------------------------------------------------------------------------
//
//	Address history
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnHistory(UINT nID)
{
	DWORD dwAddr;

	ASSERT((nID >= IDM_HISTORY_0) && (nID <= IDM_HISTORY_9));

	dwAddr = CAddrDlg::GetAddr(nID);
	SetAddr(dwAddr);
}

//---------------------------------------------------------------------------
//
//	CPU exception vectors
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnCPUExcept(UINT nID)
{
	nID -= IDM_DIS_RESET;

	OnVector(nID + 1);
}

//---------------------------------------------------------------------------
//
//	trap vectors
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnTrap(UINT nID)
{
	nID -= IDM_DIS_TRAP0;

	OnVector(nID + 0x20);
}

//---------------------------------------------------------------------------
//
//	MFP vectors
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnMFP(UINT nID)
{
	nID -= IDM_DIS_MFP0;

	OnVector(nID + (m_pMFP->GetVR() & 0xf0));
}

//---------------------------------------------------------------------------
//
//	SCC vectors
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnSCC(UINT nID)
{
	DWORD vector;

	nID -= IDM_DIS_SCC0;
	ASSERT(nID <= 7);

	// Get vector number
	::LockVM();
	vector = m_pSCC->GetVector(nID);
	::UnlockVM();

	OnVector(vector);
}

//---------------------------------------------------------------------------
//
//	DMAC vectors
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnDMAC(UINT nID)
{
	DWORD vector;

	nID -= IDM_DIS_DMAC0;
	ASSERT(nID <= 7);

	// Get vector number
	::LockVM();
	vector = m_pDMAC->GetVector(nID);
	::UnlockVM();

	OnVector(vector);
}

//---------------------------------------------------------------------------
//
//	IOSC vectors
//
//---------------------------------------------------------------------------
void CDisasmWnd::OnIOSC(UINT nID)
{
	DWORD vector;

	nID -= IDM_DIS_IOSC0;
	ASSERT(nID <= 3);

	// Get vector number
	::LockVM();
	vector = m_pIOSC->GetVector() + nID;
	::UnlockVM();

	OnVector(vector);
}

//---------------------------------------------------------------------------
//
//	Vector specification
//
//---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::OnVector(UINT vector)
{
	DWORD addr;

	// Vector read
	::LockVM();
	vector <<= 2;
	addr = (DWORD)m_pMemory->ReadOnly(vector + 1);
	addr <<= 8;
	addr |= (DWORD)m_pMemory->ReadOnly(vector + 2);
	addr <<= 8;
	addr |= (DWORD)m_pMemory->ReadOnly(vector + 3);
	::UnlockVM();

	// Address specification
	SetAddr(addr);
}

//---------------------------------------------------------------------------
//
//	Address specification
//
//---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetAddr(DWORD dwAddr)
{
	int offset;
	CString string;

	::LockVM();

	// Store the address
	dwAddr &= 0xffffff;
	m_dwSetAddr = dwAddr;
	m_dwAddr = dwAddr & 0xff0000;

	// Keep only the low part
	offset = dwAddr & 0x00ffff;
	offset >>= 1;

	// scroll
	m_ScrlY = offset;
	::UnlockVM();
	SetScrollPos(SB_VERT, offset, TRUE);

	// Update the caption string
	string.Format(" [%d] ($%06X - $%06X)", (m_dwID & 0xff) - 'A' + 1, m_dwAddr, m_dwAddr + 0xffff);
	string = m_strCaption + string;
	if (m_Caption != string) {
		m_Caption = string;
		SetWindowText(string);
	}
}

//---------------------------------------------------------------------------
//
//	PC specification
//
//---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetPC(DWORD pc)
{
	ASSERT(pc <= 0xffffff);

	// If the sync flag is set, assign the address
	if (m_bSync) {
		m_dwPC = pc;
	}
	else {
		m_dwPC = 0xffffffff;
	}
}

//---------------------------------------------------------------------------
//
//	Update from the message thread
//
//---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::Update()
{
	// PC specification
	if (m_dwPC < 0x1000000) {
		SetAddr(m_dwPC);
		m_dwPC = 0xffffffff;
	}
}

//---------------------------------------------------------------------------
//
//	Configuration
//
//---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::Setup()
{
	DWORD dwAddr;
	DWORD dwPC;
	int i;
	int j;
	int k;

	// Address specification
	dwAddr = (m_dwAddr & 0xff0000);
	dwAddr |= (DWORD)(m_ScrlY * 2);
	::debugpc = dwAddr;

	// Get PC
	dwPC = m_pCPU->GetPC();

	// loop
	for (i=0; i<m_nHeight; i++) {
		dwAddr = ::debugpc;

		// Store the address
		if (m_pAddrBuf) {
			m_pAddrBuf[i] = dwAddr;
		}

		// Check for address wraparound (consider FFFFFFF wraparound)
		if (dwAddr > 0xffffff) {
			// Wraparound detected. Keep the address
			ASSERT(i > 0);
			if (m_pAddrBuf) {
				m_pAddrBuf[i] = m_pAddrBuf[i - 1];
			}

			// Delete
			Reverse(FALSE);
			for (j=0; j<m_nWidth; j++) {
				SetChr(j, i, ' ');
			}
			continue;
		}

		// Determine attributes
		k = m_pScheduler->IsBreak(dwAddr);
		if (k >= 0) {
			Reverse(TRUE);
		}
		else {
			Reverse(FALSE);
		}
		// Padding
		for (j=0; j<m_nWidth; j++) {
			SetChr(j, i, ' ');
		}

		// Disassemble
		::cpudebug_disassemble(1);

		// PC marker, break marker
		if (k >= 0) {
			::debugbuf[0] = (char)(k + '1');
		}
		else {
			::debugbuf[0] = ' ';
		}
		if (dwAddr == dwPC) {
			::debugbuf[1] = '>';
		}
		else {
			::debugbuf[1] = ' ';
		}

		// Display
		if (m_ScrlX < (int)strlen(::debugbuf)) {
			SetString(0, i, &debugbuf[m_ScrlX]);
		}

		// Handle the case where the address is invalid because of a breakpoint
		k = m_pScheduler->IsBreak(dwAddr, TRUE);
		if (k >= 0) {
			Reverse(TRUE);
			SetChr(0, i, (char)(k + '1'));
		}
	}
}

//---------------------------------------------------------------------------
//
//	Get previous address
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDisasmWnd::GetPrevAddr(DWORD dwAddr)
{
	int i;
	DWORD dwTest;

	ASSERT(dwAddr <= 0xffffff);

	// Address initialization
	dwTest = dwAddr;

	for (i=0; i<16; i++) {
		// Decrement dwTest and check for overflow
		dwTest -= 2;
		if (dwTest >= 0x01000000) {
			return dwAddr;
		}

		// Disassemble from there and check the address increment
		::debugpc = dwTest;
		::cpudebug_disassemble(1);

		// If it matches, check "UNRECOG"
		if (::debugpc == dwAddr) {
			if ((::debugbuf[35] == 'U') || (::debugbuf[36] == 'N') || (::debugbuf[37] == 'R')) {
				continue;
			}
			// OK, return
			return dwTest;
		}
	}

	// No match. Return dwAddr
	return dwAddr;
}

//---------------------------------------------------------------------------
//
//	Breakpoint menu configuration
//
//---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetupBreakMenu(CMenu *pMenu, Scheduler *pScheduler)
{
	int num;
	int i;
	Scheduler::breakpoint_t buf;
	CString string;

	ASSERT(pMenu);
	ASSERT(pScheduler);

	// Clear the count
	num = 0;

	// Configuration
	::LockVM();
	for (i=0; i<Scheduler::BreakMax; i++) {
		pScheduler->GetBreak(i, &buf);
		if (buf.use) {
			// It is in use, so set it
			string.Format("%1d : $%06X", num + 1, buf.addr);
			pMenu->ModifyMenu(IDM_DIS_BREAKP0 + num, MF_BYCOMMAND | MF_STRING,
				IDM_DIS_BREAKP0 + num, (LPCTSTR)string);

			// +1
			num++;
		}
	}
	::UnlockVM();

	// Clear the rest
	for (i=num; i<Scheduler::BreakMax; i++) {
		pMenu->DeleteMenu(IDM_DIS_BREAKP0 + i, MF_BYCOMMAND);
	}
}

//---------------------------------------------------------------------------
//
//	Get breakpoint menu
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDisasmWnd::GetBreak(UINT nID, Scheduler *pScheduler)
{
	int i;
	Scheduler::breakpoint_t buf;

	ASSERT((nID >= IDM_DIS_BREAKP0) && (nID <= IDM_DIS_BREAKP7));
	ASSERT(pScheduler);
	nID -= IDM_DIS_BREAKP0;

	// Search loop
	::LockVM();
	for (i=0; i<Scheduler::BreakMax; i++) {
		pScheduler->GetBreak(i, &buf);
		if (buf.use) {
			// It is in use; is this the one?
			if (nID == 0) {
				::UnlockVM();
				return buf.addr;
			}
			nID--;
		}
	}
	::UnlockVM();

	return 0;
}

//===========================================================================
//
//	Memory window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CMemoryWnd::CMemoryWnd(int nWnd)
{
	// Up to 8 memory window types
	ASSERT((nWnd >= 0) && (nWnd <= 7));

	// Window parameter definitions
	m_dwID = MAKEID('M', 'E', 'M', (nWnd + 'A'));
	::GetMsg(IDS_SWND_MEMORY, m_strCaption);
	m_nWidth = 73;
	m_nHeight = 16;

	// Window parameter definitions (scroll)
	m_ScrlWidth = 73;
	m_ScrlHeight = 0x8000;

	// Get CPU
	m_pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pCPU);

	// Get memory
	m_pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(m_pMemory);

	// Other
	m_dwAddr = 0;
	m_nUnit = 0;
	m_strCaptionReq.Empty();
	m_strCaptionSet.Empty();
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CMemoryWnd, CSubTextScrlWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_MEMORY_ADDR, OnAddr)
	ON_COMMAND(IDM_MEMORY_NEWWIN, OnNewWin)
	ON_COMMAND_RANGE(IDM_MEMORY_BYTE, IDM_MEMORY_LONG, OnUnit)
	ON_COMMAND_RANGE(IDM_MEMORY_0, IDM_MEMORY_F, OnRange)
	ON_COMMAND_RANGE(IDM_REG_D0, IDM_REG_PC, OnReg)
	ON_COMMAND_RANGE(IDM_AREA_MPU, IDM_AREA_IPLROM, OnArea)
	ON_COMMAND_RANGE(IDM_HISTORY_0, IDM_HISTORY_9, OnHistory)
	ON_COMMAND_RANGE(IDM_STACK_0, IDM_STACK_F, OnStack)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CMemoryWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	// Base class
	if (CSubTextScrlWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Address initialization
	SetAddr(0);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Drawing
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnPaint()
{
	// Base class
	CSubTextScrlWnd::OnPaint();

	// Caption configuration
	SetWindowText(m_strCaption);
}

//---------------------------------------------------------------------------
//
//	Left double-click
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnLButtonDblClk(UINT /*nFlags*/, CPoint point)
{
	int x;
	int y;
	DWORD dwAddr;
	DWORD dwData;
	CDataDlg dlg(this);

	// Calcular x, y
	x = point.x / m_tmWidth;
	y = point.y / m_tmHeight;

	// Check x
	if (x < 8) {
		return;
	}
	x -= 8;

	// Get y address
	dwAddr = m_dwAddr | (m_ScrlY << 5);
	dwAddr += (y << 4);
	if ((dwAddr - m_dwAddr) >= 0x100000) {
		return;
	}

	// Get x address
	switch (m_nUnit) {
		// Byte
		case 0:
			x /= 3;
			break;
		// Word
		case 1:
			x /= 5;
			x <<= 1;
			break;
		// Long
		case 2:
			x /= 9;
			x <<= 2;
			break;
		// Other
		default:
			ASSERT(FALSE);
			break;
	}
	if (x >= 16) {
		return;
	}
	dwAddr += x;

	// Data read
	::LockVM();
	switch (m_nUnit) {
		// Byte
		case 0:
			dwData = m_pMemory->ReadOnly(dwAddr);
			break;
		// Word
		case 1:
			dwData = m_pMemory->ReadOnly(dwAddr);
			dwData <<= 8;
			dwData |= m_pMemory->ReadOnly(dwAddr + 1);
			break;
		// Long
		case 2:
			dwData = m_pMemory->ReadOnly(dwAddr);
			dwData <<= 8;
			dwData |= m_pMemory->ReadOnly(dwAddr + 1);
			dwData <<= 8;
			dwData |= m_pMemory->ReadOnly(dwAddr + 2);
			dwData <<= 8;
			dwData |= m_pMemory->ReadOnly(dwAddr + 3);
			break;
		// Other
		default:
			dwData = 0;
			ASSERT(FALSE);
			break;
	}
	::UnlockVM();

	// Execute dialogo
	dlg.m_dwAddr = dwAddr;
	dlg.m_dwValue = dwData;
	dlg.m_nSize = m_nUnit;
	if (dlg.DoModal() != IDOK) {
		return;
	}

	// Write
	dwData = dlg.m_dwValue;
	::LockVM();
	switch (m_nUnit) {
		// Byte
		case 0:
			m_pMemory->WriteByte(dwAddr, dwData);
			break;
		// Word
		case 1:
			m_pMemory->WriteWord(dwAddr, dwData);
			break;
		// Long
		case 2:
			m_pMemory->WriteWord(dwAddr, (WORD)(dwData >> 16));
			m_pMemory->WriteWord(dwAddr + 2, (WORD)dwData);
			break;
		// Other
		default:
			ASSERT(FALSE);
			break;
	}
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Context menu
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnContextMenu(CWnd *pWnd, CPoint point)
{
	CRect rect;
	CMenu menu;
	CMenu *pMenu;

	// Determinar si se presiono dentro del area del cliente
	GetClientRect(&rect);
	ClientToScreen(&rect);
	if (!rect.PtInRect(point)) {
		CSubTextScrlWnd::OnContextMenu(pWnd, point);
		return;
	}

	// Execute menu
	if (::IsJapanese()) {
		menu.LoadMenu(IDR_MEMORYMENU);
	}
	else {
		menu.LoadMenu(IDR_US_MEMORYMENU);
	}
	pMenu = menu.GetSubMenu(0);
	SetupContext(pMenu);
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							point.x, point.y, this);
}

//---------------------------------------------------------------------------
//
//	Context menu configuration
//
//---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::SetupContext(CMenu *pMenu)
{
	ASSERT(pMenu);

	// New window
	if (!m_pDrawView->IsNewWindow(FALSE)) {
		pMenu->EnableMenuItem(IDM_MEMORY_NEWWIN, MF_BYCOMMAND | MF_GRAYED);
	}

	// Check size
	pMenu->CheckMenuRadioItem(IDM_MEMORY_BYTE, IDM_MEMORY_LONG,
			IDM_MEMORY_BYTE + m_nUnit, MF_BYCOMMAND);

	// Check address
	pMenu->CheckMenuRadioItem(IDM_MEMORY_0, IDM_MEMORY_F,
			IDM_MEMORY_0 + (m_dwAddr >> 20), MF_BYCOMMAND);

	// Registers MPU
	CCPURegWnd::SetupRegMenu(pMenu, m_pCPU, FALSE);

	// Address history
	CAddrDlg::SetupHisMenu(pMenu);

	// Stack
	SetupStackMenu(pMenu, m_pMemory, m_pCPU);
}

//---------------------------------------------------------------------------
//
//	Address input
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnAddr()
{
	CAddrDlg dlg(this);

	// Execute dialogo
	dlg.m_dwValue = m_dwAddr | (m_ScrlY * 0x20);
	if (dlg.DoModal() != IDOK) {
		return;
	}

	// Set address
	SetAddr(dlg.m_dwValue);
}

//---------------------------------------------------------------------------
//
//	New window
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnNewWin()
{
	CMemoryWnd *pWnd;

	// Request the parent window to create a new window
	pWnd = (CMemoryWnd*)m_pDrawView->NewWindow(FALSE);

	// If created, use the same address and size
	if (pWnd) {
		pWnd->SetAddr(m_dwAddr | (m_ScrlY * 0x20));
		pWnd->SetUnit(m_nUnit);
	}
}

//---------------------------------------------------------------------------
//
//	Display unit specification
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnUnit(UINT uID)
{
	int unit;

	unit = (int)(uID - IDM_MEMORY_BYTE);
	ASSERT((unit >= 0) && (unit <= 2));

	SetUnit(unit);
}

//---------------------------------------------------------------------------
//
//	Set the display unit
//
//---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::SetUnit(int nUnit)
{
	ASSERT(this);
	ASSERT((nUnit >= 0) && (nUnit <= 2));

	// Lock and switch
	::LockVM();
	m_nUnit = nUnit;
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Address range specification
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnRange(UINT uID)
{
	DWORD dwAddr;

	ASSERT((uID >= IDM_MEMORY_0) && (uID <= IDM_MEMORY_F));

	dwAddr = (DWORD)(uID - IDM_MEMORY_0);
	dwAddr *= 0x100000;
	dwAddr |= (DWORD)(m_ScrlY * 0x20);
	SetAddr(dwAddr);
}

//---------------------------------------------------------------------------
//
//	Register specification
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnReg(UINT uID)
{
	DWORD dwAddr;

	ASSERT((uID >= IDM_REG_D0) && (uID <= IDM_REG_PC));

	dwAddr = CCPURegWnd::GetRegValue(m_pCPU, uID);
	SetAddr(dwAddr);
}

//---------------------------------------------------------------------------
//
//	Area specification
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnArea(UINT uID)
{
	CMenu menu;
	TCHAR buf[0x100];
	DWORD dwAddr;

	ASSERT((uID >= IDM_AREA_MPU) && (uID <= IDM_AREA_IPLROM));

	// Load menu
	if (::IsJapanese()) {
		menu.LoadMenu(IDR_MEMORYMENU);
	}
	else {
		menu.LoadMenu(IDR_US_MEMORYMENU);
	}

	// Get the menu string for the specified ID
	menu.GetMenuString(uID, buf, 0x100, MF_BYCOMMAND);

	// Assume the format "$000000 : "
	buf[0] = _T('0');
	buf[7] = _T('\0');
	dwAddr = ::_tcstoul(buf, NULL, 16);
	SetAddr(dwAddr);
}

//---------------------------------------------------------------------------
//
//	History specification
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnHistory(UINT uID)
{
	DWORD dwAddr;

	ASSERT((uID >= IDM_HISTORY_0) && (uID <= IDM_HISTORY_9));
	dwAddr = CAddrDlg::GetAddr(uID);
	SetAddr(dwAddr);
}

//---------------------------------------------------------------------------
//
//	Stack specification
//
//---------------------------------------------------------------------------
void CMemoryWnd::OnStack(UINT uID)
{
	DWORD dwAddr;

	ASSERT((uID >= IDM_STACK_0) && (uID <= IDM_STACK_F));
	dwAddr = GetStackAddr(uID, m_pMemory, m_pCPU);
	SetAddr(dwAddr);
}

//---------------------------------------------------------------------------
//
//	Address specification
//
//---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::SetAddr(DWORD dwAddr)
{
	int offset;
	CString strCap;

	ASSERT(this);
	ASSERT(dwAddr <= 0x1000000);

	// Keep only the upper part
	m_dwAddr = dwAddr & 0xf00000;

	// Keep only the lower part
	offset = dwAddr & 0x0fffff;
	offset /= 0x20;

	// scroll
	m_ScrlY = offset;
	SetScrollPos(SB_VERT, offset, TRUE);

	// Build the caption string
	strCap.Format(_T(" [%d] ($%06X - $%06X)"), (m_dwID & 0xff) - 'A' + 1,
									m_dwAddr, m_dwAddr + 0x0fffff);
	m_CSection.Lock();
	m_strCaptionReq = m_strCaption + strCap;
	m_CSection.Unlock();
}

//---------------------------------------------------------------------------
//
//	Update from the message thread
//
//---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::Update()
{
	CString strCap;

	// Get the caption string
	m_CSection.Lock();
	strCap = m_strCaptionReq;
	m_CSection.Unlock();

	// Comparison
	if (m_strCaptionSet != strCap) {
		m_strCaptionSet = strCap;
		SetWindowText(m_strCaptionSet);
	}
}

//---------------------------------------------------------------------------
//
//	Configuration
//
//---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::Setup()
{
	int x;
	int y;
	CString strText;
	CString strHex;
	DWORD dwAddr;
	DWORD dwOffset;
	TCHAR szAscii[2];

	// Clear and initialize the address
	Clear();
	dwAddr = (m_dwAddr & 0xf00000);
	dwOffset = (DWORD)(m_ScrlY << 5);
	dwAddr |= dwOffset;

	// Initialize the string
	szAscii[1] = _T('\0');

	// Y loop
	for (y=0; y<m_nHeight; y++) {
		// Check desbordamiento
		if (dwOffset >= 0x100000) {
			break;
		}

		// Display the address
		strText.Format(_T("%06X:"), dwAddr);

		// X loop
		switch (m_nUnit) {
			// Byte
			case 0:
				for (x=0; x<16; x++) {
					strHex.Format(_T(" %02X"), m_pMemory->ReadOnly(dwAddr));
					strText += strHex;
					dwAddr++;
				}
				break;
			// Word
			case 1:
				for (x=0; x<8; x++) {
					strHex.Format(_T(" %02X%02X"),  m_pMemory->ReadOnly(dwAddr),
													m_pMemory->ReadOnly(dwAddr + 1));
					strText += strHex;
					dwAddr += 2;
				}
				break;
			// Long
			case 2:
				for (x=0; x<4; x++) {
					strHex.Format(" %02X%02X%02X%02X",  m_pMemory->ReadOnly(dwAddr),
														m_pMemory->ReadOnly(dwAddr + 1),
														m_pMemory->ReadOnly(dwAddr + 2),
														m_pMemory->ReadOnly(dwAddr + 3));
					strText += strHex;
					dwAddr += 4;
				}
				break;
			// Other (not possible)
			default:
				ASSERT(FALSE);
				break;
		}

		// Step back once
		dwAddr -= 0x10;
		dwAddr &= 0xffffff;

		// Append ASCII character
		strText += _T("  ");
		for (x=0; x<16; x++) {
			szAscii[0] = (TCHAR)m_pMemory->ReadOnly(dwAddr + x);
			if ((szAscii[0] >= 0) && (szAscii[0] < 0x20)) {
				szAscii[0] = TCHAR('.');
			}
			if ((szAscii[0] < 0) || (szAscii[0] >= 0x7f)) {
				szAscii[0] = TCHAR('.');
			}
			strText += szAscii;
		}
		dwAddr += 0x10;
		dwAddr &= 0xffffff;

		// Advance offset
		dwOffset += 0x10;

		// Display
		if (m_ScrlX < strText.GetLength()) {
			SetString(0, y, (LPCTSTR)(strText) + m_ScrlX * sizeof(TCHAR));
		}
	}
}

//---------------------------------------------------------------------------
//
//	Prepare vertical scrolling
//
//---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::SetupScrlV()
{
	SCROLLINFO si;
	CRect rect;
	int height;

	// Get the vertical character count
	GetClientRect(&rect);
	height = rect.bottom / m_tmHeight;

	// Adjustment (because there are 2 lines per scroll unit)
	height >>= 1;

	// Set offset information
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_ScrlHeight - 1;
	si.nPage = height;

	// Adjust the position if necessary
	si.nPos = m_ScrlY;
	if (si.nPos + height > m_ScrlHeight) {
		si.nPos = m_ScrlHeight - height;
		if (si.nPos < 0) {
			si.nPos = 0;
		}
		m_ScrlY = si.nPos;
	}

	SetScrollInfo(SB_VERT, &si, TRUE);
}

//---------------------------------------------------------------------------
//
//	Stack menu configuration
//
//---------------------------------------------------------------------------
void CMemoryWnd::SetupStackMenu(CMenu *pMenu, Memory *pMemory, CPU *pCPU)
{
	int i;
	CPU::cpu_t reg;
	DWORD dwAddr;
	DWORD dwValue;
	CString strMenu;

	ASSERT(pMenu);
	ASSERT(pMemory);
	ASSERT(pCPU);

	// Lock the VM and read the registers
	::LockVM();
	pCPU->GetCPU(&reg);

	// 16 levels
	for (i=0; i<16; i++) {
		// Compute the address
		dwAddr = reg.areg[7];
		dwAddr += (i << 1);
		dwAddr &= 0xfffffe;

		// Get data
		dwValue = pMemory->ReadOnly(dwAddr + 1);
		dwAddr = (dwAddr + 2) & 0xfffffe;
		dwValue <<= 8;
		dwValue |= pMemory->ReadOnly(dwAddr);
		dwValue <<= 8;
		dwValue |= pMemory->ReadOnly(dwAddr + 1);

		// Update menu
		strMenu.Format(_T("(A7+%1X) : $%06X"), (i << 1), dwValue);
		pMenu->ModifyMenu(IDM_STACK_0 + i, MF_BYCOMMAND | MF_STRING,
							IDM_STACK_0 + i, (LPCTSTR)strMenu);
	}

	// Unlock the VM
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Get the stack address
//
//---------------------------------------------------------------------------
DWORD CMemoryWnd::GetStackAddr(UINT nID, Memory *pMemory, CPU *pCPU)
{
	CPU::cpu_t reg;
	DWORD dwAddr;
	DWORD dwValue;

	ASSERT((nID >= IDM_STACK_0) && (nID <= IDM_STACK_F));
	ASSERT(pMemory);
	ASSERT(pCPU);

	// Compute the offset
	nID -= IDM_STACK_0;
	ASSERT(nID <= 15);
	nID <<= 1;

	// Compute the address and memory from the CPU registers
	::LockVM();
	pCPU->GetCPU(&reg);
	dwAddr = reg.areg[7];
	dwAddr += nID;
	dwAddr &= 0xfffffe;

	// Get data
	dwValue = pMemory->ReadOnly(dwAddr + 1);
	dwAddr = (dwAddr + 2) & 0xfffffe;
	dwValue <<= 8;
	dwValue |= pMemory->ReadOnly(dwAddr);
	dwValue <<= 8;
	dwValue |= pMemory->ReadOnly(dwAddr + 1);
	::UnlockVM();

	return dwValue;
}

//===========================================================================
//
//	Breakpoint window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CBreakPWnd::CBreakPWnd()
{
	// Window parameter definitions
	m_dwID = MAKEID('B', 'R', 'K', 'P');
	::GetMsg(IDS_SWND_BREAKP, m_strCaption);
	m_nWidth = 43;
	m_nHeight = Scheduler::BreakMax + 1;

	// Get the scheduler
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CBreakPWnd, CSubTextWnd)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_BREAKP_ENABLE, OnEnable)
	ON_COMMAND(IDM_BREAKP_CLEAR, OnClear)
	ON_COMMAND(IDM_BREAKP_DEL, OnDel)
	ON_COMMAND(IDM_BREAKP_ADDR, OnAddr)
	ON_COMMAND_RANGE(IDM_HISTORY_0, IDM_HISTORY_9, OnHistory)
	ON_COMMAND(IDM_BREAKP_ALL, OnAll)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Left double-click
//
//---------------------------------------------------------------------------
void CBreakPWnd::OnLButtonDblClk(UINT /*nFlags*/, CPoint point)
{
	int y;
	Scheduler::breakpoint_t buf;

	// Get y, adjust (-1), check
	y = point.y / m_tmHeight;
	y--;
	if ((y < 0) || (y >= Scheduler::BreakMax)) {
		return;
	}

	// Lock and get the breakpoint
	::LockVM();
	m_pScheduler->GetBreak(y, &buf);

	// Invert if in use
	if (buf.use) {
		m_pScheduler->EnableBreak(y, !(buf.enable));
	}

	// Unlock
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Context menu
//
//---------------------------------------------------------------------------
void CBreakPWnd::OnContextMenu(CWnd *pWnd, CPoint point)
{
	CRect rect;
	CMenu menu;
	CMenu *pMenu;

	// Determinar si se presiono dentro del area del cliente
	GetClientRect(&rect);
	ClientToScreen(&rect);
	if (!rect.PtInRect(point)) {
		CSubTextWnd::OnContextMenu(pWnd, point);
		return;
	}

	// Store the position
	m_Point = point;

	// Execute menu
	if (::IsJapanese()) {
		menu.LoadMenu(IDR_BREAKPMENU);
	}
	else {
		menu.LoadMenu(IDR_US_BREAKPMENU);
	}
	pMenu = menu.GetSubMenu(0);
	SetupContext(pMenu);
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							point.x, point.y, this);
}

//---------------------------------------------------------------------------
//
//	Context menu configuration
//
//---------------------------------------------------------------------------
void FASTCALL CBreakPWnd::SetupContext(CMenu *pMenu)
{
	int y;
	CPoint point;
	Scheduler::breakpoint_t buf;
	int nCount;
	int nBreak;

	ASSERT(pMenu);

	// Initialization
	buf.enable = FALSE;
	buf.use = FALSE;

	// Get y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;

	// Get the number of active breakpoints and fetch the current one
	nCount = 0;
	::LockVM();
	for (nBreak=0; nBreak<Scheduler::BreakMax; nBreak++) {
		m_pScheduler->GetBreak(nBreak, &buf);
		if (buf.use) {
			nCount++;
		}
	}
	buf.use = FALSE;
	if ((y >= 0) && (y < Scheduler::BreakMax)) {
		m_pScheduler->GetBreak(y, &buf);
	}
	::UnlockVM();

	// Delete all
	if (nCount > 0) {
		pMenu->EnableMenuItem(IDM_BREAKP_ALL, MF_BYCOMMAND | MF_ENABLED);
	}
	else {
		pMenu->EnableMenuItem(IDM_BREAKP_ALL, MF_BYCOMMAND | MF_GRAYED);
	}

	// Address history
	CAddrDlg::SetupHisMenu(pMenu);

	// If the current one is not in use, disable the ones affected by changes
	if (!buf.use) {
		pMenu->EnableMenuItem(IDM_BREAKP_ENABLE, MF_BYCOMMAND | MF_GRAYED);
		pMenu->EnableMenuItem(IDM_BREAKP_CLEAR, MF_BYCOMMAND | MF_GRAYED);
		pMenu->EnableMenuItem(IDM_BREAKP_DEL, MF_BYCOMMAND | MF_GRAYED);
		return;
	}

	// If it is in use, check Enable
	if (buf.enable) {
		pMenu->CheckMenuItem(IDM_BREAKP_ENABLE, MF_BYCOMMAND | MF_CHECKED);
	}
	else {
		pMenu->CheckMenuItem(IDM_BREAKP_ENABLE, MF_BYCOMMAND | MF_UNCHECKED);
	}
}

//---------------------------------------------------------------------------
//
//	Enable/disable
//
//---------------------------------------------------------------------------
void CBreakPWnd::OnEnable()
{
	int y;
	CPoint point;
	Scheduler::breakpoint_t buf;

	// Get y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;
	ASSERT((y >= 0) && (y < Scheduler::BreakMax));

	// Toggle breakpoint
	::LockVM();
	m_pScheduler->GetBreak(y, &buf);
	ASSERT(buf.use);
	m_pScheduler->EnableBreak(y, !(buf.enable));
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Clear count
//
//---------------------------------------------------------------------------
void CBreakPWnd::OnClear()
{
	int y;
	CPoint point;

	// Get y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;
	ASSERT((y >= 0) && (y < Scheduler::BreakMax));

	// Clear count
	::LockVM();
	m_pScheduler->ClearBreak(y);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Delete breakpoint
//
//---------------------------------------------------------------------------
void CBreakPWnd::OnDel()
{
	int y;
	CPoint point;
	Scheduler::breakpoint_t buf;

	// Get y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;
	ASSERT((y >= 0) && (y < Scheduler::BreakMax));

	// Clear count
	::LockVM();
	m_pScheduler->GetBreak(y, &buf);
	ASSERT(buf.use);
	m_pScheduler->DelBreak(buf.addr);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Address specification
//
//---------------------------------------------------------------------------
void CBreakPWnd::OnAddr()
{
	int y;
	CPoint point;
	Scheduler::breakpoint_t buf;
	CPU *pCPU;
	CPU::cpu_t reg;
	DWORD dwAddr;
	CAddrDlg dlg(this);

	// Get y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;

	::LockVM();
	// If it points to a valid breakpoint, use that address
	dwAddr = 0xffffffff;
	if ((y >= 0) && (y < Scheduler::BreakMax)) {
		m_pScheduler->GetBreak(y, &buf);
		if (buf.use) {
			dwAddr = buf.addr & 0xffffff;
		}
	}
	// Otherwise, use the PC
	if (dwAddr == 0xffffffff) {
		pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
		ASSERT(pCPU);
		pCPU->GetCPU(&reg);
		dwAddr = reg.pc & 0xffffff;
	}
	::UnlockVM();
	ASSERT(dwAddr <= 0xffffff);

	// Input dialog
	dlg.m_dwValue = dwAddr;
	if (dlg.DoModal() != IDOK) {
		return;
	}

	// Let the shared routine handle it
	SetAddr(dlg.m_dwValue);
}

//---------------------------------------------------------------------------
//
//	Address history
//
//---------------------------------------------------------------------------
void CBreakPWnd::OnHistory(UINT nID)
{
	DWORD dwAddr;

	ASSERT((nID >= IDM_HISTORY_0) && (nID <= IDM_HISTORY_9));

	// Let the shared routine handle it
	dwAddr = CAddrDlg::GetAddr(nID);
	SetAddr(dwAddr);
}

//---------------------------------------------------------------------------
//
//	Delete all
//
//---------------------------------------------------------------------------
void CBreakPWnd::OnAll()
{
	Scheduler::breakpoint_t buf;
	int i;

	// Clear all
	::LockVM();
	for (i=0; i<Scheduler::BreakMax; i++) {
		m_pScheduler->GetBreak(i, &buf);
		if (buf.use) {
			m_pScheduler->DelBreak(buf.addr);
		}
	}
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Set address
//
//---------------------------------------------------------------------------
void FASTCALL CBreakPWnd::SetAddr(DWORD dwAddr)
{
	int y;
	CPoint point;
	Scheduler::breakpoint_t buf;

	ASSERT(dwAddr <= 0xffffff);

	// If the address is already registered, invalidate it
	::LockVM();
	for (y=0; y<Scheduler::BreakMax; y++) {
		m_pScheduler->GetBreak(y, &buf);
		if (buf.use) {
			if (buf.addr == dwAddr) {
				::UnlockVM();
				return;
			}
		}
	}
	::UnlockVM();

	// Get y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;

	// If it points to an active breakpoint, replace it
	::LockVM();
	if ((y >= 0) && (y < Scheduler::BreakMax)) {
		m_pScheduler->GetBreak(y, &buf);
		if (buf.use) {
			m_pScheduler->AddrBreak(y, dwAddr);
			::UnlockVM();
			return;
		}
	}

	// Otherwise, configure it as new
	m_pScheduler->SetBreak(dwAddr);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Configuration
//
//---------------------------------------------------------------------------
void FASTCALL CBreakPWnd::Setup()
{
	int i;
	CString strText;
	CString strFmt;
	Scheduler::breakpoint_t buf;

	// Clear
	Clear();

	// Show guide
	SetString(0, 0, _T("No."));
	SetString(5, 0, _T("Address"));
	SetString(14, 0, _T("Flag"));
	SetString(28, 0, _T("Time"));
	SetString(38, 0, _T("Count"));

	// loop
	for (i=0; i<Scheduler::BreakMax; i++) {
		// Number
		strText.Format(_T("%2d "), i + 1);

		// Get and verify validity
		m_pScheduler->GetBreak(i, &buf);
		if (buf.use) {
			// Address
			strFmt.Format(_T("  $%06X "), buf.addr);
			strText += strFmt;

			// Flag
			if (buf.enable) {
				strText += _T(" Enable");
			}
			else {
				strText += _T("Disable");
			}

			// time
			if (buf.count > 0) {
				strFmt.Format(_T(" %7d.%05dms"), (buf.time / 2000), (buf.time % 2000) * 5);
				strText += strFmt;

				// count
				strFmt.Format(_T("   %4d"), buf.count);
				strText += strFmt;
			}
		}

		// Set the string
		SetString(0, i + 1, strText);
	}
}

#endif	// _WIN32
