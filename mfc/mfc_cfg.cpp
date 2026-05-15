//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[MFC configuration]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "memory.h"
#include "opmif.h"
#include "adpcm.h"
#include "config.h"
#include "render.h"
#include "sasi.h"
#include "scsi.h"
#include "disk.h"
#include "filepath.h"
#include "ppi.h"
#include "mfc_frm.h"
#include "mfc_com.h"
#include "mfc_res.h"
#include "mfc_snd.h"
#include "mfc_inp.h"
#include "mfc_midi.h"
#include "mfc_tkey.h"
#include "mfc_w32.h"
#include "mfc_info.h"
#include "mfc_cfg.h"

namespace {
CFrmWnd *FASTCALL ResolveFrmWnd(CWnd *pHint)
{
	CWnd *pCandidates[3];

	pCandidates[0] = pHint;
	pCandidates[1] = AfxGetMainWnd();
	pCandidates[2] = (AfxGetApp()) ? AfxGetApp()->m_pMainWnd : NULL;

	for (int i = 0; i < (sizeof(pCandidates) / sizeof(pCandidates[0])); i++) {
		CWnd *pCur = pCandidates[i];
		for (int depth = 0; pCur && (depth < 16); depth++) {
			CFrmWnd *pFrame = DYNAMIC_DOWNCAST(CFrmWnd, pCur);
			if (pFrame) {
				return pFrame;
			}

			CWnd *pNext = pCur->GetParent();
			if (!pNext) {
				pNext = pCur->GetOwner();
			}
			if (pNext == pCur) {
				break;
			}
			pCur = pNext;
		}
	}

	return NULL;
}
}

//===========================================================================
//
//	Configuration
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CConfig::CConfig(CFrmWnd *pWnd) : CComponent(pWnd)
{
	// Component parameters
	m_dwID = MAKEID('C', 'F', 'G', ' ');
	m_strDesc = _T("Config Manager");
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::Init()
{
	int i;
	Filepath path;

	ASSERT(this);

	// Base class
	if (!CComponent::Init()) {
		return FALSE;
	}

	// Determine INI file path
	path.SetPath(_T("XM6.ini"));







	char szAppPath[MAX_PATH] = "";
	CString strAppDirectory;

	GetModuleFileName(NULL, szAppPath, MAX_PATH);

	// Extract directory path
	strAppDirectory = szAppPath;
	strAppDirectory = strAppDirectory.Left(strAppDirectory.ReverseFind('\\'));


	CString sz;
	sz = m_pFrmWnd->m_strXM6FilePath;
	m_pFrmWnd->m_strXM6FileName = sz.Mid(sz.ReverseFind('\\') + 1);

	int nLen = m_pFrmWnd->m_strXM6FileName.GetLength();
	TCHAR lpszBuf[MAX_PATH];
	_tcscpy(lpszBuf, m_pFrmWnd->m_strXM6FileName.GetBuffer(nLen));
	PathRemoveExtensionA(lpszBuf);

	CString fileNameNoExt = lpszBuf;
	CString fileToFind = strAppDirectory + "\\" + fileNameNoExt + ".ini";



	GetFileAttributes(fileToFind);
	if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(fileToFind) && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		// File not found, use default XM6.ini
		path.SetBaseFile("XM6");
	}
	else
	{
		// File found, use game-specific INI file
		path.SetBaseFile(fileNameNoExt);
	}

	_tcscpy(m_IniFile, path.GetPath());

	// Load configuration data from INI file
	LoadConfig();







	if(m_pFrmWnd->m_strXM6FilePath.GetLength()>0)
	{
		CString str = m_pFrmWnd->m_strXM6FilePath;
		CString extensionArchivo = "";

		int curPos = 0;
		CString resToken = str.Tokenize(_T("."), curPos);	// Get the extension from the full file path
		while (!resToken.IsEmpty())
		{
			// Get next token from file path
			extensionArchivo = resToken;
			resToken = str.Tokenize(_T("."), curPos);
		}

		// If file is HDF, analyze and load it
		if (extensionArchivo.MakeUpper() == "HDF")
		{

			// Load HDF file as SASI drive 0
			_tcscpy(m_Config.sasi_file[0], m_pFrmWnd->m_strXM6FilePath);
		}

	}










	// Maintain compatibility
	ResetSASI();
	ResetCDROM();

	// MRU
	for (i=0; i<MruTypes; i++) {
		ClearMRU(i);
		LoadMRU(i);
	}

	// Key
	LoadKey();

	// TrueKey
	LoadTKey();




	// Save and load
	m_bApply = FALSE;

	return TRUE;
}





BOOL FASTCALL CConfig::CustomInit(BOOL ArchivoDefault)
{
	Filepath path;

	ASSERT(this);

	// Base class
	if (!CComponent::Init()) {
		return FALSE;
	}

	// Determine INI file path
	path.SetPath(_T("XM6.ini"));

	int nLen = m_pFrmWnd->m_strXM6FileName.GetLength();
	TCHAR lpszBuf[MAX_PATH];
	_tcscpy(lpszBuf, m_pFrmWnd->m_strXM6FileName.GetBuffer(nLen));
	PathRemoveExtensionA(lpszBuf);

	// If default file is chosen, save as XM6 even if a game is loaded
	if (ArchivoDefault)
	{
		_tcscpy(lpszBuf, "XM6");
	}

	path.SetBaseFile(lpszBuf);
	_tcscpy(m_IniFile, path.GetPath());


   /* CString sz;
	sz.Format(_T("\n\nFilePath: %s\n\n"), m_pFrmWnd->m_strXM6FilePath);
	OutputDebugStringW(CT2W(sz)); */

	OutputDebugString("\n\nExecuted CustomInit to save configuration...\n\n");

	// Save and load
	m_bApply = FALSE;

	return TRUE;
}




//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::Cleanup()
{
	//int i;

	ASSERT(this);

	// Configuration data
	//SaveConfig();

	// MRU
	//for (i=0; i<MruTypes; i++) {
	//		SaveMRU(i);
	//}

	// Keys
	//SaveKey();

	// TrueKey
	//SaveTKey();

	// Base class
	CComponent::Cleanup();
}



// Same as cleanup but saving everything
void FASTCALL CConfig::Cleanup2()
{
	int i;

	// Save window and disk state
	m_pFrmWnd->SaveFrameWnd();
	m_pFrmWnd->SaveDiskState();

	ASSERT(this);


	// Save configuration
	SaveConfig();

	// Save MRU
	for (i = 0; i < MruTypes; i++) {
		SaveMRU(i);
	}

	// Save keys
	SaveKey();

	// TrueKey
	SaveTKey();


	// Base class
	//CComponent::Cleanup();
}


//---------------------------------------------------------------------------
//
//	Config class variable
//
//---------------------------------------------------------------------------
Config CConfig::m_Config;

//---------------------------------------------------------------------------
//
//	Config reference
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::GetConfig(Config *pConfigBuf) const
{
	ASSERT(this);
	ASSERT(pConfigBuf);

	// Copy work data
	*pConfigBuf = m_Config;
}

//---------------------------------------------------------------------------
//
//	Config setting
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SetConfig(Config *pConfigBuf)
{
	ASSERT(this);
	ASSERT(pConfigBuf);

	// Copy work data
	m_Config = *pConfigBuf;
}

//---------------------------------------------------------------------------
//
//	Window scale setting
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SetWindowScale(int nScale)
{
	ASSERT(this);

	if (nScale < 0) {
		nScale = 0;
	}
	if (nScale > 4) {
		nScale = 4;
	}
	m_Config.window_scale = nScale;
}

//---------------------------------------------------------------------------
//
//	MIDI device setting
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SetMIDIDevice(int nDevice, BOOL bIn)
{
	ASSERT(this);
	ASSERT(nDevice >= 0);

	// Set input or output device
	if (bIn) {
		m_Config.midiin_device = nDevice;
	}
	else {
		m_Config.midiout_device = nDevice;
	}
}

//---------------------------------------------------------------------------
//
//	INI file lookup table
//	Offset: Section, Key, Type, Default value, Min value, Max value
//
//---------------------------------------------------------------------------
const CConfig::INIKEY CConfig::IniTable[] = {
	{ &CConfig::m_Config.system_clock, _T("Basic"), _T("Clock"), 0, 0, 0, 5 },
	{ &CConfig::m_Config.mpu_fullspeed, NULL, _T("MPUFullSpeed"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.vm_fullspeed, NULL, _T("VMFullSpeed"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.ram_size, NULL, _T("Memory"), 0, 0, 0, 5 },
	{ &CConfig::m_Config.ram_sramsync, NULL, _T("AutoMemSw"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.mem_type, NULL, _T("Map"), 0, 1, 1, 6 },

	{ &CConfig::m_Config.sound_device, _T("Sound"), _T("Device"), 0, 0, 0, 15 },
	{ &CConfig::m_Config.sample_rate, NULL, _T("Rate"), 0, 5, 0, 5 },
	{ &CConfig::m_Config.primary_buffer, NULL, _T("Primary"), 0, 10, 2, 100 },
	{ &CConfig::m_Config.polling_buffer, NULL, _T("Polling"), 0, 5, 1, 100 },
	{ &CConfig::m_Config.adpcm_interp, NULL, _T("ADPCMInterP"), 1, TRUE, 0, 0 },

	{ &CConfig::m_Config.window_scale, _T("Display"), _T("Scale"), 0, 0, 0, 4 },
	{ &CConfig::m_Config.render_shader, NULL, _T("Shader"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.render_vsync, NULL, _T("VSync"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.render_mode, NULL, _T("Renderer"), 0, 1, 0, 1 },
	{ &CConfig::m_Config.alt_raster, NULL, _T("AltRaster"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.caption_info, NULL, _T("Info"), 1, TRUE, 0, 0 },

	{ &CConfig::m_Config.master_volume, _T("Volume"), _T("Master"), 0, 100, 0, 100 },
	{ &CConfig::m_Config.fm_enable, NULL, _T("FMEnable"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.fm_volume, NULL, _T("FM"), 0, 54, 0, 100 },
	{ &CConfig::m_Config.adpcm_enable, NULL, _T("ADPCMEnable"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.adpcm_volume, NULL, _T("ADPCM"), 0, 52, 0, 100 },

	{ &CConfig::m_Config.kbd_connect, _T("Keyboard"), _T("Connect"), 1, TRUE, 0, 0 },

	{ &CConfig::m_Config.mouse_speed, _T("Mouse"), _T("Speed"), 0, 205, 0, 512 },
	{ &CConfig::m_Config.mouse_port, NULL, _T("Port"), 0, 1, 0, 2 },
	{ &CConfig::m_Config.mouse_swap, NULL, _T("Swap"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.mouse_mid, NULL, _T("MidBtn"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.mouse_trackb, NULL, _T("TrackBall"), 1, FALSE, 0, 0 },

	{ &CConfig::m_Config.joy_type[0], _T("Joystick"), _T("Port1"), 0, 1, 0, 15 },
	{ &CConfig::m_Config.joy_type[1], NULL, _T("Port2"), 0, 1, 0, 15 },
	{ &CConfig::m_Config.joy_dev[0], NULL, _T("Device1"), 0, 1, 0, 15 },
	{ &CConfig::m_Config.joy_dev[1], NULL, _T("Device2"), 0, 2, 0, 15 },
	{ &CConfig::m_Config.joy_button0[0], NULL, _T("Button11"), 0, 1, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[1], NULL, _T("Button12"), 0, 2, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[2], NULL, _T("Button13"), 0, 3, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[3], NULL, _T("Button14"), 0, 4, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[4], NULL, _T("Button15"), 0, 5, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[5], NULL, _T("Button16"), 0, 6, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[6], NULL, _T("Button17"), 0, 7, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[7], NULL, _T("Button18"), 0, 8, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[8], NULL, _T("Button19"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[9], NULL, _T("Button1A"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[10], NULL, _T("Button1B"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[11], NULL, _T("Button1C"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[0], NULL, _T("Button21"), 0, 65537, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[1], NULL, _T("Button22"), 0, 65538, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[2], NULL, _T("Button23"), 0, 65539, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[3], NULL, _T("Button24"), 0, 65540, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[4], NULL, _T("Button25"), 0, 65541, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[5], NULL, _T("Button26"), 0, 65542, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[6], NULL, _T("Button27"), 0, 65543, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[7], NULL, _T("Button28"), 0, 65544, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[8], NULL, _T("Button29"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[9], NULL, _T("Button2A"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[10], NULL, _T("Button2B"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[11], NULL, _T("Button2C"), 0, 0, 0, 131071 },

	{ &CConfig::m_Config.sasi_drives, _T("SASI"), _T("Drives"), 0, -1, 0, 16 },
	{ &CConfig::m_Config.sasi_sramsync, NULL, _T("AutoMemSw"), 1, TRUE, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 0], NULL, _T("File0") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 1], NULL, _T("File1") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 2], NULL, _T("File2") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 3], NULL, _T("File3") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 4], NULL, _T("File4") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 5], NULL, _T("File5") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 6], NULL, _T("File6") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 7], NULL, _T("File7") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 8], NULL, _T("File8") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 9], NULL, _T("File9") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[10], NULL, _T("File10"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[11], NULL, _T("File11"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[12], NULL, _T("File12"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[13], NULL, _T("File13"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[14], NULL, _T("File14"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[15], NULL, _T("File15"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.sxsi_drives, _T("SxSI"), _T("Drives"), 0, 0, 0, 7 },
	{ &CConfig::m_Config.sxsi_mofirst, NULL, _T("FirstMO"), 1, FALSE, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 0], NULL, _T("File0"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 1], NULL, _T("File1"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 2], NULL, _T("File2"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 3], NULL, _T("File3"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 4], NULL, _T("File4"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 5], NULL, _T("File5"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.scsi_ilevel, _T("SCSI"), _T("IntLevel"), 0, 1, 0, 1 },
	{ &CConfig::m_Config.scsi_drives, NULL, _T("Drives"), 0, 0, 0, 7 },
	{ &CConfig::m_Config.scsi_sramsync, NULL, _T("AutoMemSw"), 1, TRUE, 0, 0 },
	{ &m_bCDROM,						NULL, _T("CDROM"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.scsi_mofirst, NULL, _T("FirstMO"), 1, FALSE, 0, 0 },
	{ CConfig::m_Config.scsi_file[ 0], NULL, _T("File0"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.scsi_file[ 1], NULL, _T("File1"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.scsi_file[ 2], NULL, _T("File2"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.scsi_file[ 3], NULL, _T("File3"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.scsi_file[ 4], NULL, _T("File4"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.port_com, _T("Port"), _T("COM"), 0, 0, 0, 9 },
	{ CConfig::m_Config.port_recvlog, NULL, _T("RecvLog"), 2, FILEPATH_MAX, 0, 0 },
	{ &CConfig::m_Config.port_384, NULL, _T("Force38400"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.port_lpt, NULL, _T("LPT"), 0, 0, 0, 9 },
	{ CConfig::m_Config.port_sendlog, NULL, _T("SendLog"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.midi_bid, _T("MIDI"), _T("ID"), 0, 0, 0, 2 },
	{ &CConfig::m_Config.midi_ilevel, NULL, _T("IntLevel"), 0, 0, 0, 1 },
	{ &CConfig::m_Config.midi_reset, NULL, _T("ResetCmd"), 0, 0, 0, 3 },
	{ &CConfig::m_Config.midiin_device, NULL, _T("InDevice"), 0, 0, 0, 15 },
	{ &CConfig::m_Config.midiin_delay, NULL, _T("InDelay"), 0, 0, 0, 200 },
	{ &CConfig::m_Config.midiout_device, NULL, _T("OutDevice"), 0, 0, 0, 15 },
	{ &CConfig::m_Config.midiout_delay, NULL, _T("OutDelay"), 0, 84, 20, 200 },

	{ &CConfig::m_Config.windrv_enable, _T("Windrv"), _T("Enable"), 0, 0, 0, 255 },
	{ &CConfig::m_Config.host_option, NULL, _T("Option"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_resume, NULL, _T("Resume"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.host_drives, NULL, _T("Drives"), 0, 0, 0, 10 },
	{ &CConfig::m_Config.host_flag[ 0], NULL, _T("Flag0"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 1], NULL, _T("Flag1"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 2], NULL, _T("Flag2"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 3], NULL, _T("Flag3"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 4], NULL, _T("Flag4"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 5], NULL, _T("Flag5"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 6], NULL, _T("Flag6"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 7], NULL, _T("Flag7"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 8], NULL, _T("Flag8"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 9], NULL, _T("Flag9"), 0, 0, 0, 0x7FFFFFFF },
	{ CConfig::m_Config.host_path[ 0], NULL, _T("Path0"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 1], NULL, _T("Path1"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 2], NULL, _T("Path2"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 3], NULL, _T("Path3"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 4], NULL, _T("Path4"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 5], NULL, _T("Path5"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 6], NULL, _T("Path6"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 7], NULL, _T("Path7"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 8], NULL, _T("Path8"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 9], NULL, _T("Path9"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.sram_64k, _T("Alter"), _T("SRAM64K"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.scc_clkup, NULL, _T("SCCClock"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.power_led, NULL, _T("BlueLED"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.dual_fdd, NULL, _T("DualFDD"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.sasi_parity, NULL, _T("SASIParity"), 1, TRUE, 0, 0 },

	{ &CConfig::m_Config.caption, _T("Window"), _T("Caption"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.menu_bar, NULL, _T("MenuBar"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.status_bar, NULL, _T("StatusBar"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.window_left, NULL, _T("Left"), 0, -0x8000, -0x8000, 0x7fff },
	{ &CConfig::m_Config.window_top, NULL, _T("Top"), 0, -0x8000, -0x8000, 0x7fff },
	{ &CConfig::m_Config.window_full, NULL, _T("Full"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.window_mode, NULL, _T("Mode"), 0, 0, 0, 0 },

	{ &CConfig::m_Config.resume_fd, _T("Resume"), _T("FD"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_fdi[0], NULL, _T("FDI0"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_fdi[1], NULL, _T("FDI1"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_fdw[0], NULL, _T("FDW0"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_fdw[1], NULL, _T("FDW1"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_fdm[0], NULL, _T("FDM0"), 0, 0, 0, 15 },
	{ &CConfig::m_Config.resume_fdm[1], NULL, _T("FDM1"), 0, 0, 0, 15 },
	{ &CConfig::m_Config.resume_mo, NULL, _T("MO"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_mos, NULL, _T("MOS"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_mow, NULL, _T("MOW"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_cd, NULL, _T("CD"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_iso, NULL, _T("ISO"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_state, NULL, _T("State"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_xm6, NULL, _T("XM6"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_screen, NULL, _T("Screen"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_dir, NULL, _T("Dir"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_path, NULL, _T("Path"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.tkey_mode, _T("TrueKey"), _T("Mode"), 0, 1, 0, 3 },
	{ &CConfig::m_Config.tkey_com, NULL, _T("COM"), 0, 0, 0, 9 },
	{ &CConfig::m_Config.tkey_rts, NULL, _T("RTS"), 1, FALSE, 0, 0 },

	{ &CConfig::m_Config.floppy_speed, _T("Misc"), _T("FloppySpeed"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.floppy_led, NULL, _T("FloppyLED"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.popup_swnd, NULL, _T("PopupWnd"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.auto_mouse, NULL, _T("AutoMouse"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.power_off, NULL, _T("PowerOff"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.ruta_savestate, NULL, _T("RutaSave"), 2, FILEPATH_MAX, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 }
};

//---------------------------------------------------------------------------
//
//	Load configuration data
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::LoadConfig()
{
	PINIKEY pIni;
	int nValue;
	BOOL bFlag;
	LPCTSTR pszSection;
	TCHAR szDef[1];
	TCHAR szBuf[FILEPATH_MAX];

	ASSERT(this);

	// �E�e�E�[�E�u�E��E��E�̐擪�E�ɍ��E�킹�E��E�
	pIni = (const PINIKEY)&IniTable[0];
	pszSection = NULL;
	szDef[0] = _T('\0');

	// �E�e�E�[�E�u�E��E�Bucle
	while (pIni->pBuf) {
		// �E�Z�E�N�E�V�E��E��E��E��E�ݒ�
		if (pIni->pszSection) {
			pszSection = pIni->pszSection;
		}
		ASSERT(pszSection);

		// �E�^�E�C�E�vCheck
		switch (pIni->nType) {
			// �E��E��E��E��E�^(�E�͈͂𒴂��E��E��E��E�f�E�t�E�H�E��E��E�g�E�l)
			case 0:
				nValue = ::GetPrivateProfileInt(pszSection, pIni->pszKey, pIni->nDef, m_IniFile);
				if ((nValue < pIni->nMin) || (pIni->nMax < nValue)) {
					nValue = pIni->nDef;
				}
				*((int*)pIni->pBuf) = nValue;
				break;

			// �E�_�E��E��E�^(0,1�E�̂ǂ��E��E�ł��E�Ȃ��E��E�΃f�E�t�E�H�E��E��E�g�E�l)
			case 1:
				nValue = ::GetPrivateProfileInt(pszSection, pIni->pszKey, -1, m_IniFile);
				switch (nValue) {
					case 0:
						bFlag = FALSE;
						break;
					case 1:
						bFlag = TRUE;
						break;
					default:
						bFlag = (BOOL)pIni->nDef;
						break;
				}
				*((BOOL*)pIni->pBuf) = bFlag;
				break;

			// �E��E��E��E��E��E�^(�E�o�E�b�E�t�E�@�E�T�E�C�E�Y�E�͈͓��E�ł̃^�E�[�E�~�E�l�E�[�E�g�E��E�ۏ�)
			case 2:
				ASSERT(pIni->nDef <= (sizeof(szBuf)/sizeof(TCHAR)));
				::GetPrivateProfileString(pszSection, pIni->pszKey, szDef, szBuf,
										sizeof(szBuf)/sizeof(TCHAR), m_IniFile);

				// �E�f�E�t�E�H�E��E��E�g�E�l�E�ɂ̓o�E�b�E�t�E�@�E�T�E�C�E�Y�E��E��E�L�E��E��E��E��E�邱�E��E�
				ASSERT(pIni->nDef > 0);
				szBuf[pIni->nDef - 1] = _T('\0');
				_tcscpy((LPTSTR)pIni->pBuf, szBuf);
				break;

			// Other
			default:
				ASSERT(FALSE);
				break;
		}

		// Siguiente
		pIni++;
	}
}

//---------------------------------------------------------------------------
//
//	Save configuration data
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SaveConfig() const
{
	PINIKEY pIni;
	CString string;
	LPCTSTR pszSection;

	ASSERT(this);

	// Align to start of table
	pIni = (const PINIKEY)&IniTable[0];
	pszSection = NULL;

	// Loop through table
	while (pIni->pBuf) {
		// Set section if specified
		if (pIni->pszSection) {
			pszSection = pIni->pszSection;
		}
		ASSERT(pszSection);

		// Type check
		switch (pIni->nType) {
			// Integer type
			case 0:
				string.Format(_T("%d"), *((int*)pIni->pBuf));
				::WritePrivateProfileString(pszSection, pIni->pszKey,
											string, m_IniFile);
				break;

			// Boolean type
			case 1:
				if (*(BOOL*)pIni->pBuf) {
					string = _T("1");
				}
				else {
					string = _T("0");
				}
				::WritePrivateProfileString(pszSection, pIni->pszKey,
											string, m_IniFile);
				break;

			// String type
			case 2:
				::WritePrivateProfileString(pszSection, pIni->pszKey,
											(LPCTSTR)pIni->pBuf, m_IniFile);
				break;

			// Other types
			default:
				ASSERT(FALSE);
				break;
		}

		// Next entry
		pIni++;
	}
}

//---------------------------------------------------------------------------
//
//	SASI reset
//	Up to version 1.44, only file search was performed, so search and configuration are done here
//	For smooth transition to version 1.45 and later
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::ResetSASI()
{
	int i;
	Filepath path;
	Fileio fio;
	TCHAR szPath[FILEPATH_MAX];

	ASSERT(this);

	// If number of drives >= 0, not needed (already configured)
	if (m_Config.sasi_drives >= 0) {
		return;
	}

	// Set number of drives to 0
	m_Config.sasi_drives = 0;

	// Create file names and loop
	for (i=0; i<16; i++) {
		_stprintf(szPath, _T("HD%d.HDF"), i);
		path.SetPath(szPath);
		path.SetBaseDir();
		_tcscpy(m_Config.sasi_file[i], path.GetPath());
	}

	// Check from the first one, and return the number of drives found
	for (i=0; i<16; i++) {
		path.SetPath(m_Config.sasi_file[i]);
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return;
		}

		// Size check (version 1.44 only supports 40MB drives)
		if (fio.GetFileSize() != 0x2793000) {
			fio.Close();
			return;
		}

		// Count up and close
		m_Config.sasi_drives++;
		fio.Close();
	}
}

//---------------------------------------------------------------------------
//
//	CD-ROM�E��E��E�Z�E�b�E�g
//	�E��E�version2.02�E�܂ł�CD-ROM�E��E��E�T�E�|�E�[�E�g�E�̂��E�߁ASCSINumber of drives�E��E�+1�E��E��E��E�
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::ResetCDROM()
{
	ASSERT(this);

	// CD-ROM�E�t�E��E��E�O�E��E��E�Z�E�b�E�g�E��E��E��E�Ă��E��E�ꍁE��͕s�E�v(�E�ݒ�ς�)
	if (m_bCDROM) {
		return;
	}

	// CD-ROM�E�t�E��E��E�O�E��E��E�Z�E�b�E�g
	m_bCDROM = TRUE;

	// SCSINumber of drives�E��E�3�E�ȏ�6�E�ȉ��E�̏ꍇ�E�Ɍ��E��E�A+1
	if ((m_Config.scsi_drives >= 3) && (m_Config.scsi_drives <= 6)) {
		m_Config.scsi_drives++;
	}
}

//---------------------------------------------------------------------------
//
//	CD-ROM�E�t�E��E��E�O
//
//---------------------------------------------------------------------------
BOOL CConfig::m_bCDROM;

//---------------------------------------------------------------------------
//
//	MRULimpiar
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::ClearMRU(int nType)
{
	int i;

	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));

	// �E��E��E��E�Limpiar
	for (i=0; i<9; i++) {
		memset(m_MRUFile[nType][i], 0, FILEPATH_MAX * sizeof(TCHAR));
	}

	// �E��Limpiar
	m_MRUNum[nType] = 0;
}

//---------------------------------------------------------------------------
//
//	MRU�E��E��E�[�E�h
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::LoadMRU(int nType)
{
	CString strSection;
	CString strKey;
	int i;

	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));

	// �E�Z�E�N�E�V�E��E��E��E�Create
	strSection.Format(_T("MRU%d"), nType);

	// Bucle
	for (i=0; i<9; i++) {
		strKey.Format(_T("File%d"), i);
		::GetPrivateProfileString(strSection, strKey, _T(""), m_MRUFile[nType][i],
								FILEPATH_MAX, m_IniFile);
		if (m_MRUFile[nType][i][0] == _T('\0')) {
			break;
		}
		m_MRUNum[nType]++;
	}
}

//---------------------------------------------------------------------------
//
//	MRU�E�Z�E�[�E�u
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SaveMRU(int nType) const
{
	CString strSection;
	CString strKey;
	int i;

	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));

	// �E�Z�E�N�E�V�E��E��E��E�Create
	strSection.Format(_T("MRU%d"), nType);

	// Bucle
	for (i=0; i<9; i++) {
		strKey.Format(_T("File%d"), i);
		::WritePrivateProfileString(strSection, strKey, m_MRUFile[nType][i],
								m_IniFile);
	}
}

//---------------------------------------------------------------------------
//
//	MRU�E�Z�E�b�E�g
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SetMRUFile(int nType, LPCTSTR lpszFile)
{
	int nMRU;
	int nCpy;
	int nNum;

	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));
	ASSERT(lpszFile);

	// �E��E��E�ɓ��E��E��E��E��E�̂��E�Ȃ��E��E�
	nNum = GetMRUNum(nType);
	for (nMRU=0; nMRU<nNum; nMRU++) {
		if (_tcscmp(m_MRUFile[nType][nMRU], lpszFile) == 0) {
			// �E�擪�E�ɂ��E��E��E�āA�E�܂��E��E��E��E��E��E��E�̂�Agregar�E��E��E�悤�E�Ƃ��E��E�
			if (nMRU == 0) {
				return;
			}

			// Copiar
			for (nCpy=nMRU; nCpy>=1; nCpy--) {
				memcpy(m_MRUFile[nType][nCpy], m_MRUFile[nType][nCpy - 1],
						FILEPATH_MAX * sizeof(TCHAR));
			}

			// �E�擪�E�ɃZ�E�b�E�g
			_tcscpy(m_MRUFile[nType][0], lpszFile);
			return;
		}
	}

	// �E�ړ�
	for (nMRU=7; nMRU>=0; nMRU--) {
		memcpy(m_MRUFile[nType][nMRU + 1], m_MRUFile[nType][nMRU],
				FILEPATH_MAX * sizeof(TCHAR));
	}

	// �E�擪�E�ɃZ�E�b�E�g
	ASSERT(_tcslen(lpszFile) < FILEPATH_MAX);
	_tcscpy(m_MRUFile[nType][0], lpszFile);

	// �E��Actualizacion
	if (m_MRUNum[nType] < 9) {
		m_MRUNum[nType]++;
	}
}

//---------------------------------------------------------------------------
//
//	MRU�E�擾
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::GetMRUFile(int nType, int nIndex, LPTSTR lpszFile) const
{
	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));
	ASSERT((nIndex >= 0) && (nIndex < 9));
	ASSERT(lpszFile);

	// �E���E�ȏ�Ȃ�\0
	if (nIndex >= m_MRUNum[nType]) {
		lpszFile[0] = _T('\0');
		return;
	}

	// Copiar
	ASSERT(_tcslen(m_MRUFile[nType][nIndex]) < FILEPATH_MAX);
	_tcscpy(lpszFile, m_MRUFile[nType][nIndex]);
}

//---------------------------------------------------------------------------
//
//	MRU�E���E�擾
//
//---------------------------------------------------------------------------
int FASTCALL CConfig::GetMRUNum(int nType) const
{
	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));

	return m_MRUNum[nType];
}

//---------------------------------------------------------------------------
//
//	Load keyboard mapping
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::LoadKey() const
{
	DWORD dwMap[0x100];
	CInput *pInput;
	CString strName;
	int i;
	int nValue;
	BOOL bFlag;

	ASSERT(this);

	// Get input component
	pInput = m_pFrmWnd->GetInput();
	ASSERT(pInput);

	// Flag OFF (no key data), clear map
	bFlag = FALSE;
	memset(dwMap, 0, sizeof(dwMap));

	// Loop through all keys
	for (i=0; i<0x100; i++) {
		strName.Format(_T("Key%d"), i);
		nValue = ::GetPrivateProfileInt(_T("Keyboard"), strName, 0, m_IniFile);

		// If value is out of range, abort (use default values)
		if ((nValue < 0) || (nValue > 0x73)) {
			return;
		}

		// If value exists, set it and turn flag on
		if (nValue != 0) {
			dwMap[i] = nValue;
			bFlag = TRUE;
		}
	}

	// If flag is on, set the map data
	if (bFlag) {
		pInput->SetKeyMap(dwMap);
	}
}

//---------------------------------------------------------------------------
//
//	Save keyboard mapping
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SaveKey() const
{
	DWORD dwMap[0x100];
	CInput *pInput;
	CString strName;
	CString strKey;
	int i;

	ASSERT(this);

	// Get input component
	pInput = m_pFrmWnd->GetInput();
	ASSERT(pInput);

	// Get map data
	pInput->GetKeyMap(dwMap);

	// Loop through all keys (256 entries)
	for (i=0; i<0x100; i++) {
		strName.Format(_T("Key%d"), i);
		strKey.Format(_T("%d"), dwMap[i]);
		::WritePrivateProfileString(_T("Keyboard"), strName,
									strKey, m_IniFile);
	}
}






//---------------------------------------------------------------------------
//
//	Load TrueKey mapping
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::LoadTKey() const
{
	CTKey *pTKey;
	int nMap[0x73];
	CString strName;
	CString strKey;
	int i;
	int nValue;
	BOOL bFlag;

	ASSERT(this);

	// Get TrueKey component
	pTKey = m_pFrmWnd->GetTKey();
	ASSERT(pTKey);

	// Flag OFF (no key data), clear map
	bFlag = FALSE;
	memset(nMap, 0, sizeof(nMap));

	// Loop through all keys
	for (i=0; i<0x73; i++) {
		strName.Format(_T("Key%d"), i);
		nValue = ::GetPrivateProfileInt(_T("TrueKey"), strName, 0, m_IniFile);

		// If value is out of range, abort (use default values)
		if ((nValue < 0) || (nValue > 0xfe)) {
			return;
		}

		// If value exists, set it and turn flag on
		if (nValue != 0) {
			nMap[i] = nValue;
			bFlag = TRUE;
		}
	}

	// If flag is on, set the map data
	if (bFlag) {
		pTKey->SetKeyMap(nMap);
	}
}

//---------------------------------------------------------------------------
//
//	Save TrueKey mapping
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SaveTKey() const
{
	CTKey *pTKey;
	int nMap[0x73];
	CString strName;
	CString strKey;
	int i;

	ASSERT(this);

	// Get TrueKey component
	pTKey = m_pFrmWnd->GetTKey();
	ASSERT(pTKey);

	// Get key map data
	pTKey->GetKeyMap(nMap);

	// Loop through all keys (0x73 entries)
	for (i=0; i<0x73; i++) {

		strName.Format(_T("Key%d"), i);
		strKey.Format(_T("%d"), nMap[i]);
		::WritePrivateProfileString(_T("TrueKey"), strName,
									strKey, m_IniFile);
	}
}

//---------------------------------------------------------------------------
//
//	Save configuration
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::Save(Fileio *pFio, int /*nVer*/)
{
	size_t sz;

	ASSERT(this);
	ASSERT(pFio);

	// Save size
	sz = sizeof(m_Config);
	if (!pFio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save main data
	if (!pFio->Write(&m_Config, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load configuration
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::Load(Fileio *pFio, int nVer)
{
	size_t sz;

	ASSERT(this);
	ASSERT(pFio);

	// Check for compatibility with older versions
	if (nVer <= 0x0201) {
		return Load200(pFio);
	}
	if (nVer <= 0x0203) {
		return Load202(pFio);
	}

	// Load size and compare
	if (!pFio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(m_Config)) {
		return FALSE;
	}

	// Load main data
	if (!pFio->Read(&m_Config, (int)sz)) {
		return FALSE;
	}

	// Set ApplyCfg required flag
	m_bApply = TRUE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�E��E��E�[�E�h(version2.00)
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::Load200(Fileio *pFio)
{
	int i;
	size_t sz;
	Config200 *pConfig200;

	ASSERT(this);
	ASSERT(pFio);

	// �E�L�E��E��E�X�E�g�E��E��E�āAversion2.00�E��E��E��E��E��E��E��E��E��E��E�[�E�h�E�ł��E��E�悤�E�ɂ��E��E�
	pConfig200 = (Config200*)&m_Config;

	// �E�T�E�C�E�Y�E��E��E��E��E�[�E�h�E�A�E�ƍ�
	if (!pFio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(Config200)) {
		return FALSE;
	}

	// �E�{�E�̂��E��E��E�[�E�h
	if (!pFio->Read(pConfig200, (int)sz)) {
		return FALSE;
	}

	// �E�V�E�K�E��E��E��E�(Config202)�E��E�Initialization
	m_Config.mem_type = 1;
	m_Config.scsi_ilevel = 1;
	m_Config.scsi_drives = 0;
	m_Config.scsi_sramsync = TRUE;
	m_Config.scsi_mofirst = FALSE;
	for (i=0; i<5; i++) {
		m_Config.scsi_file[i][0] = _T('\0');
	}

	// �E�V�E�K�E��E��E��E�(Config204)�E��E�Initialization
	m_Config.windrv_enable = 0;
	m_Config.resume_fd = FALSE;
	m_Config.resume_mo = FALSE;
	m_Config.resume_cd = FALSE;
	m_Config.resume_state = FALSE;
	m_Config.resume_screen = FALSE;
	m_Config.resume_dir = FALSE;

	// ApplyCfg�E�v�E��E��E�t�E��E��E�O�E��E��E�グ�E��E�
	m_bApply = TRUE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�E��E��E�[�E�h(version2.02)
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::Load202(Fileio *pFio)
{
	size_t sz;
	Config202 *pConfig202;

	ASSERT(this);
	ASSERT(pFio);

	// �E�L�E��E��E�X�E�g�E��E��E�āAversion2.02�E��E��E��E��E��E��E��E��E��E��E�[�E�h�E�ł��E��E�悤�E�ɂ��E��E�
	pConfig202 = (Config202*)&m_Config;

	// �E�T�E�C�E�Y�E��E��E��E��E�[�E�h�E�A�E�ƍ�
	if (!pFio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(Config202)) {
		return FALSE;
	}

	// �E�{�E�̂��E��E��E�[�E�h
	if (!pFio->Read(pConfig202, (int)sz)) {
		return FALSE;
	}

	// �E�V�E�K�E��E��E��E�(Config204)�E��E�Initialization
	m_Config.windrv_enable = 0;
	m_Config.resume_fd = FALSE;
	m_Config.resume_mo = FALSE;
	m_Config.resume_cd = FALSE;
	m_Config.resume_state = FALSE;
	m_Config.resume_screen = FALSE;
	m_Config.resume_dir = FALSE;

	// ApplyCfg�E�v�E��E��E�t�E��E��E�O�E��E��E�グ�E��E�
	m_bApply = TRUE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply�E�v�E��E�Check
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::IsApply()
{
	ASSERT(this);

	// �E�v�E��E��E�Ȃ�A�E��E��E��E��E�ŉ��E��E�
	if (m_bApply) {
		m_bApply = FALSE;
		return TRUE;
	}

	// �E�v�E��E��E��E��E�Ă��E�Ȃ�
	return FALSE;
}

//===========================================================================
//
//	Configuration property page
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CConfigPage::CConfigPage()
{
	// Clear member variables
	m_dwID = 0;
	m_nTemplate = 0;
	m_uHelpID = 0;
	m_uMsgID = 0;
	m_pConfig = NULL;
	m_pSheet = NULL;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CConfigPage, CPropertyPage)
	ON_WM_SETCURSOR()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
void FASTCALL CConfigPage::Init(CConfigSheet *pSheet)
{
	int nID;

	ASSERT(this);
	ASSERT(m_dwID != 0);

	// Remember parent sheet
	ASSERT(pSheet);
	m_pSheet = pSheet;

	// Accept ID
	nID = m_nTemplate;
	if (!::IsJapanese()) {
		nID += 50;
	}

	// Construction
	CommonConstruct(MAKEINTRESOURCE(nID), 0);

	// Add to sheet
	pSheet->AddPage(this);
}

//---------------------------------------------------------------------------
//
//	Dialog initialization
//
//---------------------------------------------------------------------------
BOOL CConfigPage::OnInitDialog()
{
	CConfigSheet *pSheet;

	ASSERT(this);

	// Get configuration data from parent window
	pSheet = (CConfigSheet*)GetParent();
	ASSERT(pSheet);
	m_pConfig = pSheet->m_pConfig;

	// Base class
	return CPropertyPage::OnInitDialog();
}

//---------------------------------------------------------------------------
//
//	Active page
//
//---------------------------------------------------------------------------
BOOL CConfigPage::OnSetActive()
{
	CStatic *pStatic;
	CString strEmpty;

	ASSERT(this);

	// Base class
	if (!CPropertyPage::OnSetActive()) {
		return FALSE;
	}

	// Help initialization
	ASSERT(m_uHelpID > 0);
	m_uMsgID = 0;
	pStatic = (CStatic*)GetDlgItem(m_uHelpID);
	ASSERT(pStatic);
	strEmpty.Empty();
	pStatic->SetWindowText(strEmpty);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Mouse cursor configuration
//
//---------------------------------------------------------------------------
BOOL CConfigPage::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT nMsg)
{
	CWnd *pChildWnd;
	CPoint pt;
	UINT nID;
	CRect rectParent;
	CRect rectChild;
	CString strText;
	CStatic *pStatic;

	// Help must be specified
	ASSERT(this);
	ASSERT(m_uHelpID > 0);

	// Get mouse position
	GetCursorPos(&pt);

	// Loop through child windows and check if position is within rectangle
	nID = 0;
	rectParent.top = 0;
	pChildWnd = GetTopWindow();

	// Loop
	while (pChildWnd) {
		// Skip if it's the help ID
		if (pChildWnd->GetDlgCtrlID() == (int)m_uHelpID) {
			pChildWnd = pChildWnd->GetNextWindow();
			continue;
		}

		// Get rectangle
		pChildWnd->GetWindowRect(&rectChild);

		// Check if point is in rectangle
		if (rectChild.PtInRect(pt)) {
			// If no rectangle acquired yet, or if this rectangle is smaller
			if (rectParent.top == 0) {
				// First detection
				rectParent = rectChild;
				nID = pChildWnd->GetDlgCtrlID();
			}
			else {
				if (rectChild.Width() < rectParent.Width()) {
					// Smaller detection
					rectParent = rectChild;
					nID = pChildWnd->GetDlgCtrlID();
				}
			}
		}

		// Next window
		pChildWnd = pChildWnd->GetNextWindow();
	}

	// Compare nID
	if (m_uMsgID == nID) {
		// Base class
		return CPropertyPage::OnSetCursor(pWnd, nHitTest, nMsg);
	}
	m_uMsgID = nID;

	// Load message and set
	::GetMsg(m_uMsgID, strText);
	pStatic = (CStatic*)GetDlgItem(m_uHelpID);
	ASSERT(pStatic);
	pStatic->SetWindowText(strText);

	// Base class
	return CPropertyPage::OnSetCursor(pWnd, nHitTest, nMsg);
}

//===========================================================================
//
//	Basic page
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CBasicPage::CBasicPage()
{
	// Always set ID and Help
	m_dwID = MAKEID('B', 'A', 'S', 'C');
	m_nTemplate = IDD_BASICPAGE;
	m_uHelpID = IDC_BASIC_HELP;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CBasicPage, CConfigPage)
	ON_BN_CLICKED(IDC_BASIC_CPUFULLB, OnMPUFull)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CBasicPage::OnInitDialog()
{
	CString string;
	CButton *pButton;
	CComboBox *pComboBox;
	int i;

	// Base class
	CConfigPage::OnInitDialog();

	// System clock
	pComboBox = (CComboBox*)GetDlgItem(IDC_BASIC_CLOCKC);
	ASSERT(pComboBox);
	for (i=0; i<6; i++) {
		::GetMsg((IDS_BASIC_CLOCK0 + i), string);
		pComboBox->AddString(string);
	}
	pComboBox->SetCurSel(m_pConfig->system_clock);

	// MPU full speed
	pButton = (CButton*)GetDlgItem(IDC_BASIC_CPUFULLB);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->mpu_fullspeed);

	// VM full speed
	pButton = (CButton*)GetDlgItem(IDC_BASIC_ALLFULLB);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->vm_fullspeed);

	// Main memory size
	pComboBox = (CComboBox*)GetDlgItem(IDC_BASIC_MEMORYC);
	ASSERT(pComboBox);
	for (i=0; i<6; i++) {
		::GetMsg((IDS_BASIC_MEMORY0 + i), string);
		pComboBox->AddString(string);
	}
	pComboBox->SetCurSel(m_pConfig->ram_size);

	// SRAM auto-sync
	pButton = (CButton*)GetDlgItem(IDC_BASIC_MEMSWB);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->ram_sramsync);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	OK button
//
//---------------------------------------------------------------------------
void CBasicPage::OnOK()
{
	CButton *pButton;
	CComboBox *pComboBox;

	// System clock
	pComboBox = (CComboBox*)GetDlgItem(IDC_BASIC_CLOCKC);
	ASSERT(pComboBox);
	m_pConfig->system_clock = pComboBox->GetCurSel();

	// MPU full speed
	pButton = (CButton*)GetDlgItem(IDC_BASIC_CPUFULLB);
	ASSERT(pButton);
	m_pConfig->mpu_fullspeed = pButton->GetCheck();

	// VM full speed
	pButton = (CButton*)GetDlgItem(IDC_BASIC_ALLFULLB);
	ASSERT(pButton);
	m_pConfig->vm_fullspeed = pButton->GetCheck();

	// Main memory size
	pComboBox = (CComboBox*)GetDlgItem(IDC_BASIC_MEMORYC);
	ASSERT(pComboBox);
	m_pConfig->ram_size = pComboBox->GetCurSel();

	// SRAM auto-sync
	pButton = (CButton*)GetDlgItem(IDC_BASIC_MEMSWB);
	ASSERT(pButton);
	m_pConfig->ram_sramsync = pButton->GetCheck();

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	MPU full speed
//
//---------------------------------------------------------------------------
void CBasicPage::OnMPUFull()
{
	CSxSIPage *pSxSIPage;
	CButton *pButton;
	CString strWarn;

	// Get button
	pButton = (CButton*)GetDlgItem(IDC_BASIC_CPUFULLB);
	ASSERT(pButton);

	// If off, do nothing
	if (pButton->GetCheck() == 0) {
		return;
	}

	// If no SxSI drives, do nothing
	pSxSIPage = (CSxSIPage*)m_pSheet->SearchPage(MAKEID('S', 'X', 'S', 'I'));
	ASSERT(pSxSIPage);
	if (pSxSIPage->GetDrives(m_pConfig) == 0) {
		return;
	}

	// Warning
	::GetMsg(IDS_MPUSXSI, strWarn);
	MessageBox(strWarn, NULL, MB_ICONINFORMATION | MB_OK);
}

//===========================================================================
//
//	Sound page
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSoundPage::CSoundPage()
{
	// Always set ID and Help
	m_dwID = MAKEID('S', 'N', 'D', ' ');
	m_nTemplate = IDD_SOUNDPAGE;
	m_uHelpID = IDC_SOUND_HELP;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSoundPage, CConfigPage)
	ON_WM_VSCROLL()
	ON_CBN_SELCHANGE(IDC_SOUND_DEVICEC, OnSelChange)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CSoundPage::OnInitDialog()
{
	CFrmWnd *pFrmWnd;
	CSound *pSound;
	CComboBox *pComboBox;
	CButton *pButton;
	CEdit *pEdit;
	CSpinButtonCtrl *pSpin;
	CString strName;
	CString strEdit;
	int i;

	// Base class
	CConfigPage::OnInitDialog();

	// Get sound component
	pFrmWnd = ResolveFrmWnd(this);
	if (!pFrmWnd) {
		return FALSE;
	}
	pSound = pFrmWnd->GetSound();
	ASSERT(pSound);

	// Device combo box initialization
	pComboBox = (CComboBox*)GetDlgItem(IDC_SOUND_DEVICEC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_SOUND_NOASSIGN, strName);
	pComboBox->AddString(strName);
	for (i=0; i<pSound->m_nDeviceNum; i++) {
		pComboBox->AddString(pSound->m_DeviceDescr[i]);
	}

	// Combo box cursor position
	if (m_pConfig->sample_rate == 0) {
		pComboBox->SetCurSel(0);
	}
	else {
		if (pSound->m_nDeviceNum <= m_pConfig->sound_device) {
			pComboBox->SetCurSel(0);
		}
		else {
			pComboBox->SetCurSel(m_pConfig->sound_device + 1);
		}
	}

	// Sample rate initialization
	for (i=0; i<5; i++) {
		pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE0 + i);
		ASSERT(pButton);
		pButton->SetCheck(0);
	}
	if (m_pConfig->sample_rate > 0) {
		pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE0 + m_pConfig->sample_rate - 1);
		ASSERT(pButton);
		pButton->SetCheck(1);
	}

	// Buffer size initialization
	pEdit = (CEdit*)GetDlgItem(IDC_SOUND_BUF1E);
	ASSERT(pEdit);
	strEdit.Format(_T("%d"), m_pConfig->primary_buffer * 10);
	pEdit->SetWindowText(strEdit);
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF1S);
	pSpin->SetBase(10);
	pSpin->SetRange(2, 100);
	pSpin->SetPos(m_pConfig->primary_buffer);

	// Polling interval initialization
	pEdit = (CEdit*)GetDlgItem(IDC_SOUND_BUF2E);
	ASSERT(pEdit);
	strEdit.Format(_T("%d"), m_pConfig->polling_buffer);
	pEdit->SetWindowText(strEdit);
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF2S);
	pSpin->SetBase(10);
	pSpin->SetRange(1, 100);
	pSpin->SetPos(m_pConfig->polling_buffer);

	// ADPCM interpolation initialization
	pButton = (CButton*)GetDlgItem(IDC_SOUND_INTERP);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->adpcm_interp);

	// Controls enabled/disabled
	m_bEnableCtrl = TRUE;
	if (m_pConfig->sample_rate == 0) {
		EnableControls(FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CSoundPage::OnOK()
{
	CComboBox *pComboBox;
	CButton *pButton;
	CSpinButtonCtrl *pSpin;
	int i;

	// Get device
	pComboBox = (CComboBox*)GetDlgItem(IDC_SOUND_DEVICEC);
	ASSERT(pComboBox);
	if (pComboBox->GetCurSel() == 0) {
		// No device selected
		m_pConfig->sample_rate = 0;
	}
	else {
		// Device selected
		m_pConfig->sound_device = pComboBox->GetCurSel() - 1;

		// Get sample rate
		for (i=0; i<5; i++) {
			pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE0 + i);
			ASSERT(pButton);
			if (pButton->GetCheck() == 1) {
				m_pConfig->sample_rate = i + 1;
				break;
			}
		}
	}

	// Get buffer
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF1S);
	ASSERT(pSpin);
	m_pConfig->primary_buffer = LOWORD(pSpin->GetPos());
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF2S);
	ASSERT(pSpin);
	m_pConfig->polling_buffer = LOWORD(pSpin->GetPos());

	// Get ADPCM linear interpolation
	pButton = (CButton*)GetDlgItem(IDC_SOUND_INTERP);
	ASSERT(pButton);
	m_pConfig->adpcm_interp = pButton->GetCheck();

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Desplazamiento vertical
//
//---------------------------------------------------------------------------
void CSoundPage::OnVScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar* pBar)
{
	CEdit *pEdit;
	CSpinButtonCtrl *pSpin;
	CString strEdit;

	// ?Coincide con el control de giro (spin control)?
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF1S);
	if ((CWnd*)pBar == (CWnd*)pSpin) {
		// Reflejar en el cuadro de edicion
		pEdit = (CEdit*)GetDlgItem(IDC_SOUND_BUF1E);
		strEdit.Format(_T("%d"), nPos * 10);
		pEdit->SetWindowText(strEdit);
	}

	// ?Coincide con el control de giro (spin control)?
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF2S);
	if ((CWnd*)pBar == (CWnd*)pSpin) {
		// Reflejar en el cuadro de edicion
		pEdit = (CEdit*)GetDlgItem(IDC_SOUND_BUF2E);
		strEdit.Format(_T("%d"), nPos);
		pEdit->SetWindowText(strEdit);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio en el cuadro combinado
//
//---------------------------------------------------------------------------
void CSoundPage::OnSelChange()
{
	int i;
	CComboBox *pComboBox;
	CButton *pButton;

	pComboBox = (CComboBox*)GetDlgItem(IDC_SOUND_DEVICEC);
	ASSERT(pComboBox);
	if (pComboBox->GetCurSel() == 0) {
		EnableControls(FALSE);
	}
	else {
		EnableControls(TRUE);
	}

	// Consider sample rate configuration
	if (m_bEnableCtrl) {
		// If enabled, at least one must be checked
		for (i=0; i<5; i++) {
			pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE0 + i);
			ASSERT(pButton);
			if (pButton->GetCheck() != 0) {
				return;
			}
		}

		// None checked, set to 62.5kHz
		pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE4);
		ASSERT(pButton);
		pButton->SetCheck(1);
		return;
	}

	// If disabled, uncheck all
	for (i=0; i<5; i++) {
		pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE0 + i);
		ASSERT(pButton);
		pButton->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CSoundPage::EnableControls(BOOL bEnable)
{
	int i;
	CWnd *pWnd;

	ASSERT(this);

	// Flag check
	if (m_bEnableCtrl == bEnable) {
		return;
	}
	m_bEnableCtrl = bEnable;

	// Configure all controls except Device and Help
	for(i=0; ; i++) {
		// End check
		if (ControlTable[i] == NULL) {
			break;
		}

		// Get control
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);
		pWnd->EnableWindow(bEnable);
	}
}

//---------------------------------------------------------------------------
//
//	Tabla de IDs de controles
//
//---------------------------------------------------------------------------
const UINT CSoundPage::ControlTable[] = {
	IDC_SOUND_RATEG,
	IDC_SOUND_RATE0,
	IDC_SOUND_RATE1,
	IDC_SOUND_RATE2,
	IDC_SOUND_RATE3,
	IDC_SOUND_RATE4,
	IDC_SOUND_BUFFERG,
	IDC_SOUND_BUF1L,
	IDC_SOUND_BUF1E,
	IDC_SOUND_BUF1S,
	IDC_SOUND_BUF1MS,
	IDC_SOUND_BUF2L,
	IDC_SOUND_BUF2E,
	IDC_SOUND_BUF2S,
	IDC_SOUND_BUF2MS,
	IDC_SOUND_OPTIONG,
	IDC_SOUND_INTERP,
	NULL
};

//===========================================================================
//
//	Page de volumen
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CVolPage::CVolPage()
{
	// Always set ID and Help
	m_dwID = MAKEID('V', 'O', 'L', ' ');
	m_nTemplate = IDD_VOLPAGE;
	m_uHelpID = IDC_VOL_HELP;

	// Objects
	m_pSound = NULL;
	m_pOPMIF = NULL;
	m_pADPCM = NULL;
	m_pMIDI = NULL;

	// Timer (Timer)
	m_nTimerID = NULL;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CVolPage, CConfigPage)
	ON_WM_HSCROLL()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_VOL_FMC, OnFMCheck)
	ON_BN_CLICKED(IDC_VOL_ADPCMC, OnADPCMCheck)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CVolPage::OnInitDialog()
{
	CFrmWnd *pFrmWnd;
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CString strLabel;
	CButton *pButton;
	int nPos;
	int nMax;

	// Base class
	CConfigPage::OnInitDialog();

	// Get sound component
	pFrmWnd = ResolveFrmWnd(this);
	if (!pFrmWnd) {
		return FALSE;
	}
	m_pSound = pFrmWnd->GetSound();
	ASSERT(m_pSound);

	// Get OPMIF
	m_pOPMIF = (OPMIF*)::GetVM()->SearchDevice(MAKEID('O', 'P', 'M', ' '));
	ASSERT(m_pOPMIF);

	// Get ADPCM
	m_pADPCM = (ADPCM*)::GetVM()->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
	ASSERT(m_pADPCM);

	// Get MIDI
	m_pMIDI = pFrmWnd->GetMIDI();
	ASSERT(m_pMIDI);

	// Master volume
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_VOLS);
	ASSERT(pSlider);
	pSlider->SetRange(0, 100);
	nPos = m_pSound->GetMasterVol(nMax);
	if (nPos >= 0) {
		// Volume can be adjusted
		pSlider->SetRange(0, nMax);
		pSlider->SetPos(nPos);
		pSlider->EnableWindow(TRUE);
		strLabel.Format(_T(" %d"), (nPos * 100) / nMax);
	}
	else {
		// Volume cannot be adjusted
		pSlider->SetPos(0);
		pSlider->EnableWindow(FALSE);
		strLabel.Empty();
	}
	pStatic = (CStatic*)GetDlgItem(IDC_VOL_VOLN);
	pStatic->SetWindowText(strLabel);
	m_nMasterVol = nPos;
	m_nMasterOrg = nPos;

	// WAVE level
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_MASTERS);
	ASSERT(pSlider);
	pSlider->SetRange(0, 100);
	::LockVM();
	nPos = m_pSound->GetVolume();
	::UnlockVM();
	pSlider->SetPos(nPos);
	strLabel.Format(_T(" %d"), nPos);
	pStatic = (CStatic*)GetDlgItem(IDC_VOL_MASTERN);
	pStatic->SetWindowText(strLabel);

	// MIDI level
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_SEPS);
	ASSERT(pSlider);
	pSlider->SetRange(0, 0xffff);
	nPos = m_pMIDI->GetOutVolume();
	if (nPos >= 0) {
		// MIDI�E�o�E�̓f�E�o�E�C�E�X�E�̓A�E�N�E�e�E�B�E�u�E��E��E��E�Se puede ajustar el volumen
		pSlider->SetPos(nPos);
		pSlider->EnableWindow(TRUE);
		strLabel.Format(_T(" %d"), ((nPos + 1) * 100) >> 16);
	}
	else {
		// MIDI�E�o�E�̓f�E�o�E�C�E�X�E�̓A�E�N�E�e�E�B�E�u�E�łȂ��E�A�E��E��E��E�No se puede ajustar el volumen
		pSlider->SetPos(0);
		pSlider->EnableWindow(FALSE);
		strLabel.Empty();
	}
	pStatic = (CStatic*)GetDlgItem(IDC_VOL_SEPN);
	pStatic->SetWindowText(strLabel);
	m_nMIDIVol = nPos;
	m_nMIDIOrg = nPos;

	// FM synthesizer
	pButton = (CButton*)GetDlgItem(IDC_VOL_FMC);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->fm_enable);
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_FMS);
	ASSERT(pSlider);
	pSlider->SetRange(0, 100);
	pSlider->SetPos(m_pConfig->fm_volume);
	strLabel.Format(_T(" %d"), m_pConfig->fm_volume);
	pStatic = (CStatic*)GetDlgItem(IDC_VOL_FMN);
	pStatic->SetWindowText(strLabel);

	// ADPCM synthesizer
	pButton = (CButton*)GetDlgItem(IDC_VOL_ADPCMC);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->adpcm_enable);
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_ADPCMS);
	ASSERT(pSlider);
	pSlider->SetRange(0, 100);
	pSlider->SetPos(m_pConfig->adpcm_volume);
	strLabel.Format(_T(" %d"), m_pConfig->adpcm_volume);
	pStatic = (CStatic*)GetDlgItem(IDC_VOL_ADPCMN);
	pStatic->SetWindowText(strLabel);

	// Iniciar temporizador (fuego cada 100ms)
	m_nTimerID = SetTimer(IDD_VOLPAGE, 100, NULL);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Desplazamiento horizontal
//
//---------------------------------------------------------------------------
void CVolPage::OnHScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar *pBar)
{
	UINT uID;
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CString strLabel;

	ASSERT(this);
	ASSERT(pBar);

	// Conversion
	pSlider = (CSliderCtrl*)pBar;
	ASSERT(pSlider);

	// Check
	switch (pSlider->GetDlgCtrlID()) {
		// Master volumeModificacion
		case IDC_VOL_VOLS:
			nPos = pSlider->GetPos();
			m_pSound->SetMasterVol(nPos);
			// Delegate update to OnTimer
			OnTimer(m_nTimerID);
			return;

		// WAVE level modification
		case IDC_VOL_MASTERS:
			// Modification
			nPos = pSlider->GetPos();
			::LockVM();
			m_pSound->SetVolume(nPos);
			::UnlockVM();

			// Update
			uID = IDC_VOL_MASTERN;
			strLabel.Format(_T(" %d"), nPos);
			break;

		// MIDI level modification
		case IDC_VOL_SEPS:
			nPos = pSlider->GetPos();
			m_pMIDI->SetOutVolume(nPos);
			// Delegate update to OnTimer
			OnTimer(m_nTimerID);
			return;

		// FM volume modification
		case IDC_VOL_FMS:
			// Modification
			nPos = pSlider->GetPos();
			::LockVM();
			m_pSound->SetFMVol(nPos);
			::UnlockVM();

			// Update
			uID = IDC_VOL_FMN;
			strLabel.Format(_T(" %d"), nPos);
			break;

		// ADPCM volume modification
		case IDC_VOL_ADPCMS:
			// Modification
			nPos = pSlider->GetPos();
			::LockVM();
			m_pSound->SetADPCMVol(nPos);
			::UnlockVM();

			// Update
			uID = IDC_VOL_ADPCMN;
			strLabel.Format(_T(" %d"), nPos);
			break;

		// Other
		default:
			ASSERT(FALSE);
			return;
	}

	// Modification
	pStatic = (CStatic*)GetDlgItem(uID);
	ASSERT(pStatic);
	pStatic->SetWindowText(strLabel);
}

//---------------------------------------------------------------------------
//
//	Timer
//
//---------------------------------------------------------------------------
void CVolPage::OnTimer(UINT /*nTimerID*/)
{
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CString strLabel;
	int nPos;
	int nMax;

	// Get master volume
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_VOLS);
	ASSERT(pSlider);
	nPos = m_pSound->GetMasterVol(nMax);

	// Volume comparison
	if (nPos != m_nMasterVol) {
		m_nMasterVol = nPos;

		// Processing
		if (nPos >= 0) {
			// Activation
			pSlider->SetPos(nPos);
			pSlider->EnableWindow(TRUE);
			strLabel.Format(_T(" %d"), (nPos * 100) / nMax);
		}
		else {
			// Deactivation
			pSlider->SetPos(0);
			pSlider->EnableWindow(FALSE);
			strLabel.Empty();
		}

		pStatic = (CStatic*)GetDlgItem(IDC_VOL_VOLN);
		pStatic->SetWindowText(strLabel);
	}

	// MIDI
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_SEPS);
	nPos = m_pMIDI->GetOutVolume();

	// MIDI comparison
	if (nPos != m_nMIDIVol) {
		m_nMIDIVol = nPos;

		// Processing
		if (nPos >= 0) {
			// Activation
			pSlider->SetPos(nPos);
			pSlider->EnableWindow(TRUE);
			strLabel.Format(_T(" %d"), ((nPos + 1) * 100) >> 16);
		}
		else {
			// Deactivation
			pSlider->SetPos(0);
			pSlider->EnableWindow(FALSE);
			strLabel.Empty();
		}

		pStatic = (CStatic*)GetDlgItem(IDC_VOL_SEPN);
		pStatic->SetWindowText(strLabel);
	}
}

//---------------------------------------------------------------------------
//
//	Sintetizador FMCheck
//
//---------------------------------------------------------------------------
void CVolPage::OnFMCheck()
{
	CButton *pButton;

	pButton = (CButton*)GetDlgItem(IDC_VOL_FMC);
	ASSERT(pButton);
	if (pButton->GetCheck()) {
		m_pOPMIF->EnableFM(TRUE);
	}
	else {
		m_pOPMIF->EnableFM(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Sintetizador ADPCMCheck
//
//---------------------------------------------------------------------------
void CVolPage::OnADPCMCheck()
{
	CButton *pButton;

	pButton = (CButton*)GetDlgItem(IDC_VOL_ADPCMC);
	ASSERT(pButton);
	if (pButton->GetCheck()) {
		m_pADPCM->EnableADPCM(TRUE);
	}
	else {
		m_pADPCM->EnableADPCM(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CVolPage::OnOK()
{
	CSliderCtrl *pSlider;
	CButton *pButton;

	// Timer�E��E�~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// WAVE level
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_MASTERS);
	ASSERT(pSlider);
	m_pConfig->master_volume = pSlider->GetPos();

	// FM enabled
	pButton = (CButton*)GetDlgItem(IDC_VOL_FMC);
	ASSERT(pButton);
	m_pConfig->fm_enable = pButton->GetCheck();

	// FM volume
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_FMS);
	ASSERT(pSlider);
	m_pConfig->fm_volume = pSlider->GetPos();

	// ADPCM enabled
	pButton = (CButton*)GetDlgItem(IDC_VOL_ADPCMC);
	ASSERT(pButton);
	m_pConfig->adpcm_enable = pButton->GetCheck();

	// ADPCM volume
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_ADPCMS);
	ASSERT(pSlider);
	m_pConfig->adpcm_volume = pSlider->GetPos();

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cancel
//
//---------------------------------------------------------------------------
void CVolPage::OnCancel()
{
	// Timer�E��E�~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Restore to original values (datos CONFIG)
	::LockVM();
	m_pSound->SetVolume(m_pConfig->master_volume);
	m_pOPMIF->EnableFM(m_pConfig->fm_enable);
	m_pSound->SetFMVol(m_pConfig->fm_volume);
	m_pADPCM->EnableADPCM(m_pConfig->adpcm_enable);
	m_pSound->SetADPCMVol(m_pConfig->adpcm_volume);
	::UnlockVM();

	// Restore to original values (mezclador)
	if (m_nMasterOrg >= 0) {
		m_pSound->SetMasterVol(m_nMasterOrg);
	}
	if (m_nMIDIOrg >= 0) {
		m_pMIDI->SetOutVolume(m_nMIDIOrg);
	}

	// Base class
	CConfigPage::OnCancel();
}

//===========================================================================
//
//	Page del teclado
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CKbdPage::CKbdPage()
{
	// Always set ID and Help
	m_dwID = MAKEID('K', 'E', 'Y', 'B');
	m_nTemplate = IDD_KBDPAGE;
	m_uHelpID = IDC_KBD_HELP;
	m_pInput = NULL;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CKbdPage, CConfigPage)
	ON_COMMAND(IDC_KBD_EDITB, OnEdit)
	ON_COMMAND(IDC_KBD_DEFB, OnDefault)
	ON_BN_CLICKED(IDC_KBD_NOCONB, OnConnect)
	ON_NOTIFY(NM_CLICK, IDC_KBD_MAPL, OnClick)
	ON_NOTIFY(NM_RCLICK, IDC_KBD_MAPL, OnRClick)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CKbdPage::OnInitDialog()
{
	CButton *pButton;
	CListCtrl *pListCtrl;
	CClientDC *pDC;
	TEXTMETRIC tm;
	CString strText;
	LPCTSTR lpszName;
	int nKey;
	LONG cx;

	// Base class
	CConfigPage::OnInitDialog();

	if (!m_pInput) {
		CFrmWnd *pWnd = ResolveFrmWnd(this);
		if (pWnd) {
			m_pInput = pWnd->GetInput();
		}
	}
	if (!m_pInput) {
		return FALSE;
	}

	// Backup key map
	m_pInput->GetKeyMap(m_dwBackup);
	memcpy(m_dwEdit, m_dwBackup, sizeof(m_dwBackup));

	// Get text metrics
	pDC = new CClientDC(this);
	::GetTextMetrics(pDC->m_hDC, &tm);
	delete pDC;
	cx = tm.tmAveCharWidth;

	// List control columns
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_KBD_MAPL);
	ASSERT(pListCtrl);
	if (::IsJapanese()) {
		// Japanese
		::GetMsg(IDS_KBD_NO, strText);
		pListCtrl->InsertColumn(0, strText, LVCFMT_LEFT, cx * 4, 0);
		::GetMsg(IDS_KBD_KEYTOP, strText);
		pListCtrl->InsertColumn(1, strText, LVCFMT_LEFT, cx * 10, 1);
		::GetMsg(IDS_KBD_DIRECTX, strText);
		pListCtrl->InsertColumn(2, strText, LVCFMT_LEFT, cx * 22, 2);
	}
	else {
		// English
		::GetMsg(IDS_KBD_NO, strText);
		pListCtrl->InsertColumn(0, strText, LVCFMT_LEFT, cx * 5, 0);
		::GetMsg(IDS_KBD_KEYTOP, strText);
		pListCtrl->InsertColumn(1, strText, LVCFMT_LEFT, cx * 12, 1);
		::GetMsg(IDS_KBD_DIRECTX, strText);
		pListCtrl->InsertColumn(2, strText, LVCFMT_LEFT, cx * 18, 2);
	}

	// Full row select option for list control (COMCTL32.DLL v4.71+)
	pListCtrl->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

	// Create items (la information del lado X68000 es fija independientemente del mapeo)
	pListCtrl->DeleteAllItems();
	cx = 0;
	for (nKey=0; nKey<=0x73; nKey++) {
		// Get el nombre de la tecla desde CKeyDispWnd
		lpszName = m_pInput->GetKeyName(nKey);
		if (lpszName) {
			// This key is valid
			strText.Format(_T("%02X"), nKey);
			pListCtrl->InsertItem(cx, strText);
			pListCtrl->SetItemText(cx, 1, lpszName);
			pListCtrl->SetItemData(cx, (DWORD)nKey);
			cx++;
		}
	}

	// �E��E��E�|�E�[�E�gActualizacion
	UpdateReport();

	// Connection
	pButton = (CButton*)GetDlgItem(IDC_KBD_NOCONB);
	ASSERT(pButton);
	pButton->SetCheck(!m_pConfig->kbd_connect);

	// �E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
	m_bEnableCtrl = TRUE;
	if (!m_pConfig->kbd_connect) {
		EnableControls(FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CKbdPage::OnOK()
{
	CButton *pButton;

	// Set key map
	m_pInput->SetKeyMap(m_dwEdit);

	// Connection
	pButton = (CButton*)GetDlgItem(IDC_KBD_NOCONB);
	ASSERT(pButton);
	m_pConfig->kbd_connect = !(pButton->GetCheck());

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CKbdPage::OnCancel()
{
	// Restore key map from backup
	m_pInput->SetKeyMap(m_dwBackup);

	// Base class
	CConfigPage::OnCancel();
}

//---------------------------------------------------------------------------
//
//	�E��E��E�|�E�[�E�gActualizacion
//
//---------------------------------------------------------------------------
void FASTCALL CKbdPage::UpdateReport()
{
	CListCtrl *pListCtrl;
	int nX68;
	int nWin;
	int nItem;
	CString strNext;
	CString strPrev;
	LPCTSTR lpszName;

	ASSERT(this);

	// Get control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_KBD_MAPL);
	ASSERT(pListCtrl);

	// List control row
	nItem = 0;
	for (nX68=0; nX68<=0x73; nX68++) {
		// Get el nombre de la tecla desde CKeyDispWnd
		lpszName = m_pInput->GetKeyName(nX68);
		if (lpszName) {
			// �E�L�E��E��E�ȃL�E�[�E��E��E��E��E��E�BInitialization
			strNext.Empty();

			// Set si hay una asignacion
			for (nWin=0; nWin<0x100; nWin++) {
				if (nX68 == (int)m_dwEdit[nWin]) {
					// Get nombre
					lpszName = m_pInput->GetKeyID(nWin);
					strNext = lpszName;
					break;
				}
			}

			// Overwrite if different
			strPrev = pListCtrl->GetItemText(nItem, 2);
			if (strPrev != strNext) {
				pListCtrl->SetItemText(nItem, 2, strNext);
			}

			// Next item
			nItem++;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Editar
//
//---------------------------------------------------------------------------
void CKbdPage::OnEdit()
{
	CKbdMapDlg dlg(this, m_dwEdit);

	ASSERT(this);

	// Execute dialog
	dlg.DoModal();

	// Mostrar�E��E�Actualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	Restablecer valores predeterminados
//
//---------------------------------------------------------------------------
void CKbdPage::OnDefault()
{
	ASSERT(this);

	// Put 106 map in buffer and set it
	m_pInput->SetDefaultKeyMap(m_dwEdit);
	m_pInput->SetKeyMap(m_dwEdit);

	// Mostrar�E��E�Actualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	Clic en item
//
//---------------------------------------------------------------------------
void CKbdPage::OnClick(NMHDR * /*pNMHDR*/, LRESULT * /*pResult*/)
{
	CListCtrl *pListCtrl;
	int nCount;
	int nItem;
	int nKey;
	int nPrev;
	int i;
	CKeyinDlg dlg(this);

	// �E��E��E�X�E�gGet control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_KBD_MAPL);
	ASSERT(pListCtrl);

	// Get count
	nCount = pListCtrl->GetItemCount();

	// Get selected index
	for (nItem=0; nItem<nCount; nItem++) {
		if (pListCtrl->GetItemState(nItem, LVIS_SELECTED)) {
			break;
		}
	}
	if (nItem >= nCount) {
		return;
	}

	// Get data pointed to by that index
	nKey = (int)pListCtrl->GetItemData(nItem);
	ASSERT((nKey >= 0) && (nKey <= 0x73));

	// Start configuration
	dlg.m_nTarget = nKey;
	dlg.m_nKey = 0;

	// Configure if corresponding Windows key exists
	nPrev = -1;
	for (i=0; i<0x100; i++) {
		if (m_dwEdit[i] == (DWORD)nKey) {
			dlg.m_nKey = i;
			nPrev = i;
			break;
		}
	}

	// Execute dialog
	m_pInput->EnableKey(FALSE);
	if (dlg.DoModal() != IDOK) {
		m_pInput->EnableKey(TRUE);
		return;
	}
	m_pInput->EnableKey(TRUE);

	// Configure key map
	if (nPrev >= 0) {
		m_dwEdit[nPrev] = 0;
	}
	m_dwEdit[dlg.m_nKey] = (DWORD)nKey;

	// SHIFT�E�L�E�[�E��E�OProcesamiento
	if (nPrev == DIK_LSHIFT) {
		m_dwEdit[DIK_RSHIFT] = 0;
	}
	if (dlg.m_nKey == DIK_LSHIFT) {
		m_dwEdit[DIK_RSHIFT] = (DWORD)nKey;
	}

	// Mostrar�E��E�Actualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	Clic derecho en item
//
//---------------------------------------------------------------------------
void CKbdPage::OnRClick(NMHDR * /*pNMHDR*/, LRESULT * /*pResult*/)
{
	CListCtrl *pListCtrl;
	int nCount;
	int nItem;
	int nKey;
	int nWin;
	int i;
	CString strText;
	CString strMsg;

	// �E��E��E�X�E�gGet control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_KBD_MAPL);
	ASSERT(pListCtrl);

	// Get count
	nCount = pListCtrl->GetItemCount();

	// Get selected index
	for (nItem=0; nItem<nCount; nItem++) {
		if (pListCtrl->GetItemState(nItem, LVIS_SELECTED)) {
			break;
		}
	}
	if (nItem >= nCount) {
		return;
	}

	// Get data pointed to by that index
	nKey = (int)pListCtrl->GetItemData(nItem);
	ASSERT((nKey >= 0) && (nKey <= 0x73));

	// Does corresponding Windows key exist?
	nWin = -1;
	for (i=0; i<0x100; i++) {
		if (m_dwEdit[i] == (DWORD)nKey) {
			nWin = i;
			break;
		}
	}
	if (nWin < 0) {
		// Not mapped
		return;
	}

	// �E��E��E�b�E�Z�E�[�E�W�E�{�E�b�E�N�E�X�E�ŁADelete�E�̗L�E��E��E��E�Check
	::GetMsg(IDS_KBD_DELMSG, strText);
	strMsg.Format(strText, nKey, m_pInput->GetKeyID(nWin));
	::GetMsg(IDS_KBD_DELTITLE, strText);
	if (MessageBox(strMsg, strText, MB_ICONQUESTION | MB_YESNO) != IDYES) {
		return;
	}

	// �E�Y�E��E��E��E��E��E�Windows�E�L�E�[�E��E�Delete
	m_dwEdit[nWin] = 0;

	// SHIFT�E�L�E�[�E��E�OProcesamiento
	if (nWin == DIK_LSHIFT) {
		m_dwEdit[DIK_RSHIFT] = 0;
	}

	// Mostrar�E��E�Actualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	�E��E�Conexion
//
//---------------------------------------------------------------------------
void CKbdPage::OnConnect()
{
	CButton *pButton;

	pButton = (CButton*)GetDlgItem(IDC_KBD_NOCONB);
	ASSERT(pButton);

	// Controls enabled/disabled
	if (pButton->GetCheck() == 1) {
		EnableControls(FALSE);
	}
	else {
		EnableControls(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CKbdPage::EnableControls(BOOL bEnable)
{
	int i;
	CWnd *pWnd;

	ASSERT(this);

	// Flag check
	if (m_bEnableCtrl == bEnable) {
		return;
	}
	m_bEnableCtrl = bEnable;

	// Configurar todos los controles excepto ID de placa y Ayuda
	for(i=0; ; i++) {
		// End check
		if (ControlTable[i] == NULL) {
			break;
		}

		// Get control
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);
		pWnd->EnableWindow(bEnable);
	}
}

//---------------------------------------------------------------------------
//
//	Tabla de IDs de controles
//
//---------------------------------------------------------------------------
const UINT CKbdPage::ControlTable[] = {
	IDC_KBD_MAPG,
	IDC_KBD_MAPL,
	IDC_KBD_EDITB,
	IDC_KBD_DEFB,
	NULL
};

//===========================================================================
//
//	Keyboard�E�}�E�b�E�vEditar�E�_�E�C�E�A�E��E��E�O
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CKbdMapDlg::CKbdMapDlg(CWnd *pParent, DWORD *pMap) : CDialog(IDD_KBDMAPDLG, pParent)
{
	CFrmWnd *pFrmWnd;

	// Edit�E�f�E�[�E�^�E��E��E�L�E��E�
	ASSERT(pMap);
	m_pEditMap = pMap;

	// English�E���E�ւ̑Ή�
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_KBDMAPDLG);
		m_nIDHelp = IDD_US_KBDMAPDLG;
	}

	// Get entrada
	pFrmWnd = ResolveFrmWnd(pParent);
	m_pInput = pFrmWnd ? pFrmWnd->GetInput() : NULL;

	// Sin mensaje de estado
	m_strStat.Empty();
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
#if !defined(WM_KICKIDLE)
#define WM_KICKIDLE		0x36a
#endif	// WM_KICKIDLE
BEGIN_MESSAGE_MAP(CKbdMapDlg, CDialog)
	ON_WM_PAINT()
	ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
	ON_MESSAGE(WM_APP, OnApp)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	�E�_�E�C�E�A�E��E��E�OInitialization
//
//---------------------------------------------------------------------------
BOOL CKbdMapDlg::OnInitDialog()
{
	LONG cx;
	LONG cy;
	CRect rectClient;
	CRect rectWnd;
	CStatic *pStatic;

	// Base class
	CDialog::OnInitDialog();

	if (!m_pInput) {
		CFrmWnd *pFrmWnd = ResolveFrmWnd(this);
		if (pFrmWnd) {
			m_pInput = pFrmWnd->GetInput();
		}
	}
	if (!m_pInput) {
		return FALSE;
	}

	// Get client size
	GetClientRect(&rectClient);

	// Get status bar height
	pStatic = (CStatic*)GetDlgItem(IDC_KBDMAP_STAT);
	ASSERT(pStatic);
	pStatic->GetWindowRect(&rectWnd);

	// Calculate difference (se asume > 0)
	cx = 616 - rectClient.Width();
	ASSERT(cx > 0);
	cy = (140 + rectWnd.Height()) - rectClient.Height();
	ASSERT(cy > 0);

	// Expand by cx, cy
	GetWindowRect(&rectWnd);
	SetWindowPos(&wndTop, 0, 0, rectWnd.Width() + cx, rectWnd.Height() + cy, SWP_NOMOVE);

	// �E�X�E�e�E�[�E�^�E�X�E�o�E�[�E��E��E�Ɉړ��E�ADelete
	pStatic->GetWindowRect(&rectClient);
	ScreenToClient(&rectClient);
	pStatic->SetWindowPos(&wndTop, 0, 140,
					rectClient.Width() + cx, rectClient.Height(), SWP_NOZORDER);
	pStatic->GetWindowRect(&m_rectStat);
	ScreenToClient(&m_rectStat);
	pStatic->DestroyWindow();

	// Move display window
	pStatic = (CStatic*)GetDlgItem(IDC_KBDMAP_DISP);
	ASSERT(pStatic);
	pStatic->GetWindowRect(&rectWnd);
	ScreenToClient(&rectWnd);
	pStatic->SetWindowPos(&wndTop, 0, 0, 616, 140, SWP_NOZORDER);

	// Place CKeyDispWnd at display window position
	pStatic->GetWindowRect(&rectWnd);
	ScreenToClient(&rectWnd);
	pStatic->DestroyWindow();
	m_pDispWnd = new CKeyDispWnd;
	m_pDispWnd->Create(NULL, NULL, WS_CHILD | WS_VISIBLE,
					rectWnd, this, 0, NULL);

	// Center window
	CenterWindow(GetParent());

	// Disable IME
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Prohibit keyboard input
	ASSERT(m_pInput);
	m_pInput->EnableKey(FALSE);

	// Load guide message
	::GetMsg(IDS_KBDMAP_GUIDE, m_strGuide);

	// Call OnKickIdle at the end (mostrar el estado actual desde el inicio)
	OnKickIdle(0, 0);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	OK
//
//---------------------------------------------------------------------------
void CKbdMapDlg::OnOK()
{
	// [CR]�E�ɂ��E�Fin�E��E�}�E��E�
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CKbdMapDlg::OnCancel()
{
	// Key enabled
	m_pInput->EnableKey(TRUE);

	// Base class
	CDialog::OnCancel();
}

//---------------------------------------------------------------------------
//
//	Dibujar dialogo
//
//---------------------------------------------------------------------------
void CKbdMapDlg::OnPaint()
{
	CPaintDC dc(this);

	// Delegar a OnDraw
	OnDraw(&dc);
}

//---------------------------------------------------------------------------
//
//	Sub-rutina de dibujo
//
//---------------------------------------------------------------------------
void FASTCALL CKbdMapDlg::OnDraw(CDC *pDC)
{
	CFont *pFont;

	ASSERT(this);
	ASSERT(pDC);

	// Color settings
	pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
	pDC->SetBkColor(::GetSysColor(COLOR_3DFACE));

	// Font settings
	pFont = (CFont*)pDC->SelectStockObject(DEFAULT_GUI_FONT);
	ASSERT(pFont);

	pDC->FillSolidRect(m_rectStat, ::GetSysColor(COLOR_3DFACE));
	pDC->DrawText(m_strStat, m_rectStat,
				DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

	// Restore font
	pDC->SelectObject(pFont);
}

//---------------------------------------------------------------------------
//
//	Ocioso (idle)Procesamiento
//
//---------------------------------------------------------------------------
LONG CKbdMapDlg::OnKickIdle(UINT /*uParam*/, LONG /*lParam*/)
{
	BOOL bBuf[0x100];
	BOOL bFlg[0x100];
	int nWin;
	DWORD dwCode;
	CKeyDispWnd *pWnd;

	// Get key state
	ASSERT(m_pInput);
	m_pInput->GetKeyBuf(bBuf);

	// Clear key flags temporarily
	memset(bFlg, 0, sizeof(bFlg));

	// �E��E��E�݂̃}�E�b�E�v�E�ɏ]�E��E��E�āAConversion�E�\�E��E��E��E��E�
	for (nWin=0; nWin<0x100; nWin++) {
		// ?Esta la tecla presionada?
		if (bBuf[nWin]) {
			// Get codigo
			dwCode = m_pEditMap[nWin];
			if (dwCode != 0) {
				// La tecla esta pulsada y asignada, configurar buffer de teclado
				bFlg[dwCode] = TRUE;
			}
		}
	}

	// SHIFT�E�L�E�[�E��E�OProcesamiento(L,R�E��E��E��E��E�킹�E��E�)
	bFlg[0x74] = bFlg[0x70];

	// Refrescar (dibujar)
	pWnd = (CKeyDispWnd*)m_pDispWnd;
	pWnd->Refresh(bFlg);

	return 0;
}

//---------------------------------------------------------------------------
//
//	Notificacion de ventanas subordinadas
//
//---------------------------------------------------------------------------
LONG CKbdMapDlg::OnApp(UINT uParam, LONG lParam)
{
	CKeyinDlg dlg(this);
	int nPrev;
	int nWin;
	CString strText;
	CString strName;
	CClientDC *pDC;

	ASSERT(this);
	ASSERT(uParam <= 0x73);

	// Distribution
	switch (lParam) {
		// Left button pressed
		case WM_LBUTTONDOWN:
			// �E�^�E�[�E�Q�E�b�E�g�E��E�Asignacion�E�L�E�[�E��E�Initialization
			dlg.m_nTarget = uParam;
			dlg.m_nKey = 0;

			// Configure if corresponding Windows key exists
			nPrev = -1;
			for (nWin=0; nWin<0x100; nWin++) {
				if (m_pEditMap[nWin] == uParam) {
					dlg.m_nKey = nWin;
					nPrev = nWin;
					break;
				}
			}

			// Execute dialog
			if (dlg.DoModal() != IDOK) {
				return 0;
			}

			// Configure key map
			m_pEditMap[dlg.m_nKey] = uParam;
			if (nPrev >= 0) {
				m_pEditMap[nPrev] = 0;
			}

			// SHIFT�E�L�E�[�E��E�OProcesamiento
			if (nPrev == DIK_LSHIFT) {
				m_pEditMap[DIK_RSHIFT] = 0;
			}
			if (dlg.m_nKey == DIK_LSHIFT) {
				m_pEditMap[DIK_RSHIFT] = uParam;
			}
			break;

		// Left button released
		case WM_LBUTTONUP:
			break;

		// Right button pressed
		case WM_RBUTTONDOWN:
			// Configure if corresponding Windows key exists
			nPrev = -1;
			for (nWin=0; nWin<0x100; nWin++) {
				if (m_pEditMap[nWin] == uParam) {
					dlg.m_nKey = nWin;
					nPrev = nWin;
					break;
				}
			}
			if (nPrev < 0) {
				break;
			}

			// �E��E��E�b�E�Z�E�[�E�W�E�{�E�b�E�N�E�X�E�ŁADelete�E�̗L�E��E��E��E�Check
			::GetMsg(IDS_KBD_DELMSG, strName);
			strText.Format(strName, uParam, m_pInput->GetKeyID(nWin));
			::GetMsg(IDS_KBD_DELTITLE, strName);
			if (MessageBox(strText, strName, MB_ICONQUESTION | MB_YESNO) != IDYES) {
				break;
			}

			// �E�Y�E��E��E��E��E��E�Windows�E�L�E�[�E��E�Delete
			m_pEditMap[nWin] = 0;

			// SHIFT�E�L�E�[�E��E�OProcesamiento
			if (nWin == DIK_LSHIFT) {
				m_pEditMap[DIK_RSHIFT] = 0;
			}
			break;

		// Right button released
		case WM_RBUTTONUP:
			break;

		// Mouse movement
		case WM_MOUSEMOVE:
			// Startup message settings
			strText = m_strGuide;

			// When key has focus
			if (uParam != 0) {
				// First show X68000 key
				strText.Format(_T("Key%02X  "), uParam);
				strText += m_pInput->GetKeyName(uParam);

				// �E�Y�E��E��E��E��E��E�Windows�E�L�E�[�E��E��E��E��E��E��E�Agregar
				for (nWin=0; nWin<0x100; nWin++) {
					if (m_pEditMap[nWin] == uParam) {
						// There was a Windows key
						strName = m_pInput->GetKeyID(nWin);
						strText += _T("    (");
						strText += strName;
						strText += _T(")");
						break;
					}
				}
			}

			// Message settings
			m_strStat = strText;
			pDC = new CClientDC(this);
			OnDraw(pDC);
			delete pDC;
			break;

		// Other(�E��E��E�肦�E�Ȃ�)
		default:
			ASSERT(FALSE);
			break;
	}

	return 0;
}

//===========================================================================
//
//	Dialogo de entrada de teclas
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CKeyinDlg::CKeyinDlg(CWnd *pParent) : CDialog(IDD_KEYINDLG, pParent)
{
	CFrmWnd *pFrmWnd;

	// English�E���E�ւ̑Ή�
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_KEYINDLG);
		m_nIDHelp = IDD_US_KEYINDLG;
	}

	// Get entrada
	pFrmWnd = ResolveFrmWnd(pParent);
	m_pInput = pFrmWnd ? pFrmWnd->GetInput() : NULL;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
#if !defined(WM_KICKIDLE)
#define WM_KICKIDLE		0x36a
#endif	// WM_KICKIDLE
BEGIN_MESSAGE_MAP(CKeyinDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_GETDLGCODE()
	ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	�E�_�E�C�E�A�E��E��E�OInitialization
//
//---------------------------------------------------------------------------
BOOL CKeyinDlg::OnInitDialog()
{
	CStatic *pStatic;
	CString string;
	CString targetText;

	// Base class
	CDialog::OnInitDialog();

	if (!m_pInput) {
		CFrmWnd *pFrmWnd = ResolveFrmWnd(this);
		if (pFrmWnd) {
			m_pInput = pFrmWnd->GetInput();
		}
	}
	if (!m_pInput) {
		return FALSE;
	}

	// Disable IME
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Pass current keys to buffer
	ASSERT(m_pInput);
	m_pInput->GetKeyBuf(m_bKey);

	// �E�K�E�C�E�h�E��E�`�E��E�Procesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_LABEL);
	if (!pStatic) {
		TRACE(_T("CKeyinDlg::OnInitDialog: missing IDC_KEYIN_LABEL\n"));
		return FALSE;
	}
	pStatic->GetWindowText(string);
	targetText.Format(_T("%02X"), m_nTarget);
	m_GuideString = string;
	m_GuideString.Replace(_T("%02X"), targetText);
	pStatic->GetWindowRect(&m_GuideRect);
	ScreenToClient(&m_GuideRect);
	pStatic->DestroyWindow();

	// Asignacion�E��E�`�E��E�Procesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_STATIC);
	if (!pStatic) {
		TRACE(_T("CKeyinDlg::OnInitDialog: missing IDC_KEYIN_STATIC\n"));
		return FALSE;
	}
	pStatic->GetWindowText(m_AssignString);
	pStatic->GetWindowRect(&m_AssignRect);
	ScreenToClient(&m_AssignRect);
	pStatic->DestroyWindow();

	// �E�L�E�[�E��E�`�E��E�Procesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_KEY);
	if (!pStatic) {
		TRACE(_T("CKeyinDlg::OnInitDialog: missing IDC_KEYIN_KEY\n"));
		return FALSE;
	}
	pStatic->GetWindowText(m_KeyString);
	if (m_nKey != 0) {
		m_KeyString = m_pInput->GetKeyID(m_nKey);
	}
	pStatic->GetWindowRect(&m_KeyRect);
	ScreenToClient(&m_KeyRect);
	pStatic->DestroyWindow();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	OK
//
//---------------------------------------------------------------------------
void CKeyinDlg::OnOK()
{
	// [CR]�E�ɂ��E�Fin�E��E�}�E��E�
}

//---------------------------------------------------------------------------
//
//	�E�_�E�C�E�A�E��E��E�OGet codigo
//
//---------------------------------------------------------------------------
UINT CKeyinDlg::OnGetDlgCode()
{
	// Habilitar la recepcion de mensajes de teclado
	return DLGC_WANTMESSAGE;
}

//---------------------------------------------------------------------------
//
//	Dibujar
//
//---------------------------------------------------------------------------
void CKeyinDlg::OnPaint()
{
	CPaintDC dc(this);
	CDC mDC;
	CRect rect;
	CBitmap Bitmap;
	CBitmap *pBitmap;

	// Create DC en memoria
	VERIFY(mDC.CreateCompatibleDC(&dc));

	// Create mapa de bits compatible
	GetClientRect(&rect);
	VERIFY(Bitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height()));
	pBitmap = mDC.SelectObject(&Bitmap);
	ASSERT(pBitmap);

	// Limpiar fondo
	mDC.FillSolidRect(&rect, ::GetSysColor(COLOR_3DFACE));

	// Dibujar
	OnDraw(&mDC);

	// BitBlt
	VERIFY(dc.BitBlt(0, 0, rect.Width(), rect.Height(), &mDC, 0, 0, SRCCOPY));

	// �E�r�E�b�E�g�E�}�E�b�E�vFin
	VERIFY(mDC.SelectObject(pBitmap));
	VERIFY(Bitmap.DeleteObject());

	// �E��E��E��E��E��E�DCFin
	VERIFY(mDC.DeleteDC());
}

//---------------------------------------------------------------------------
//
//	Sub-rutina de dibujo
//
//---------------------------------------------------------------------------
void FASTCALL CKeyinDlg::OnDraw(CDC *pDC)
{
	CFont *pFont;

	ASSERT(this);
	ASSERT(pDC);

	// Color settings
	pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
	pDC->SetBkColor(::GetSysColor(COLOR_3DFACE));

	// Font settings
	pFont = (CFont*)pDC->SelectStockObject(DEFAULT_GUI_FONT);
	ASSERT(pFont);

	// Mostrar
	pDC->DrawText(m_GuideString, m_GuideRect,
					DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
	pDC->DrawText(m_AssignString, m_AssignRect,
					DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
	pDC->DrawText(m_KeyString, m_KeyRect,
					DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

	// Restore font(Objetos�E��E�Delete�E��E��E�Ȃ��E�Ă悢)
	pDC->SelectObject(pFont);
}

//---------------------------------------------------------------------------
//
//	Ocioso (idle)
//
//---------------------------------------------------------------------------
LONG CKeyinDlg::OnKickIdle(UINT /*uParam*/, LONG /*lParam*/)
{
	BOOL bKey[0x100];
	int i;
	UINT nOld;

	// Memorizar tecla anterior
	nOld = m_nKey;

	// Recibir teclas via DirectInput
	m_pInput->GetKeyBuf(bKey);

	// �E�L�E�[Busqueda
	for (i=0; i<0x100; i++) {
		// Si hay una tecla nueva respecto a la anterior, configurarla
		if (!m_bKey[i] && bKey[i]) {
			m_nKey = (UINT)i;
		}

		// Copiar
		m_bKey[i] = bKey[i];
	}

	// SHIFT�E�L�E�[�E��E�OProcesamiento
	if (m_nKey == DIK_RSHIFT) {
		m_nKey = DIK_LSHIFT;
	}

	// Si coinciden, no es necesario cambiar
	if (m_nKey == nOld) {
		return 0;
	}

	// �E�L�E�[Nombre�E��E�Mostrar
	m_KeyString = m_pInput->GetKeyID(m_nKey);
	Invalidate(FALSE);

	return 0;
}

//---------------------------------------------------------------------------
//
//	Button derecho presionado
//
//---------------------------------------------------------------------------
void CKeyinDlg::OnRButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
	// �E�_�E�C�E�A�E��E��E�OFin
	EndDialog(IDOK);
}

//===========================================================================
//
//	Page del mouse
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CMousePage::CMousePage()
{
	// Always set ID and Help
	m_dwID = MAKEID('M', 'O', 'U', 'S');
	m_nTemplate = IDD_MOUSEPAGE;
	m_uHelpID = IDC_MOUSE_HELP;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CMousePage, CConfigPage)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_MOUSE_NPORT, OnPort)
	ON_BN_CLICKED(IDC_MOUSE_FPORT, OnPort)
	ON_BN_CLICKED(IDC_MOUSE_KPORT, OnPort)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CMousePage::OnInitDialog()
{
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CButton *pButton;
	CString strText;
	UINT nID;

	// Base class
	CConfigPage::OnInitDialog();

	// Velocidad
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_MOUSE_SLIDER);
	ASSERT(pSlider);
	pSlider->SetRange(0, 512);
	pSlider->SetPos(m_pConfig->mouse_speed);

	// Velocidad�E�e�E�L�E�X�E�g
	strText.Format(_T("%d%%"), (m_pConfig->mouse_speed * 100) >> 8);
	pStatic = (CStatic*)GetDlgItem(IDC_MOUSE_PARS);
	pStatic->SetWindowText(strText);

	// Connection�E��E�|�E�[�E�g
	nID = IDC_MOUSE_NPORT;
	switch (m_pConfig->mouse_port) {
		// Connection�E��E��E�Ȃ�
		case 0:
			break;
		// SCC
		case 1:
			nID = IDC_MOUSE_FPORT;
			break;
		// Keyboard
		case 2:
			nID = IDC_MOUSE_KPORT;
			break;
		// Other(�E��E��E�肦�E�Ȃ�)
		default:
			ASSERT(FALSE);
			break;
	}
	pButton = (CButton*)GetDlgItem(nID);
	ASSERT(pButton);
	pButton->SetCheck(1);

	// Opciones
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_SWAPB);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->mouse_swap);
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_MIDB);
	ASSERT(pButton);
	pButton->SetCheck(!m_pConfig->mouse_mid);
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_TBG);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->mouse_trackb);

	// Controls enabled/disabled
	m_bEnableCtrl = TRUE;
	if (m_pConfig->mouse_port == 0) {
		EnableControls(FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CMousePage::OnOK()
{
	CSliderCtrl *pSlider;
	CButton *pButton;

	// Velocidad
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_MOUSE_SLIDER);
	ASSERT(pSlider);
	m_pConfig->mouse_speed = pSlider->GetPos();

	// Connection�E�|�E�[�E�g
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_NPORT);
	ASSERT(pButton);
	if (pButton->GetCheck()) {
		m_pConfig->mouse_port = 0;
	}
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_FPORT);
	ASSERT(pButton);
	if (pButton->GetCheck()) {
		m_pConfig->mouse_port = 1;
	}
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_KPORT);
	ASSERT(pButton);
	if (pButton->GetCheck()) {
		m_pConfig->mouse_port = 2;
	}

	// Opciones
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_SWAPB);
	ASSERT(pButton);
	m_pConfig->mouse_swap = pButton->GetCheck();
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_MIDB);
	ASSERT(pButton);
	m_pConfig->mouse_mid = !(pButton->GetCheck());
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_TBG);
	ASSERT(pButton);
	m_pConfig->mouse_trackb = pButton->GetCheck();

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Desplazamiento horizontal
//
//---------------------------------------------------------------------------
void CMousePage::OnHScroll(UINT /*nSBCode*/, UINT /*nPos*/, CScrollBar *pBar)
{
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CString strText;

	// Conversion�E�ACheck
	pSlider = (CSliderCtrl*)pBar;
	if (pSlider->GetDlgCtrlID() != IDC_MOUSE_SLIDER) {
		return;
	}

	// Mostrar
	strText.Format(_T("%d%%"), (pSlider->GetPos() * 100) >> 8);
	pStatic = (CStatic*)GetDlgItem(IDC_MOUSE_PARS);
	pStatic->SetWindowText(strText);
}

//---------------------------------------------------------------------------
//
//	Seleccion de puerto
//
//---------------------------------------------------------------------------
void CMousePage::OnPort()
{
	CButton *pButton;

	// Get button
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_NPORT);
	ASSERT(pButton);

	// Connection�E��E��E�Ȃ� or �E��E��E�̃|�E�[�E�g�E�Ŕ��E��E�
	if (pButton->GetCheck()) {
		EnableControls(FALSE);
	}
	else {
		EnableControls(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CMousePage::EnableControls(BOOL bEnable)
{
	int i;
	CWnd *pWnd;

	ASSERT(this);

	// Flag check
	if (m_bEnableCtrl == bEnable) {
		return;
	}
	m_bEnableCtrl = bEnable;

	// Configurar todos los controles excepto ID de placa y Ayuda
	for(i=0; ; i++) {
		// End check
		if (ControlTable[i] == NULL) {
			break;
		}

		// Get control
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);
		pWnd->EnableWindow(bEnable);
	}
}

//---------------------------------------------------------------------------
//
//	Tabla de IDs de controles
//
//---------------------------------------------------------------------------
const UINT CMousePage::ControlTable[] = {
	IDC_MOUSE_SPEEDG,
	IDC_MOUSE_SLIDER,
	IDC_MOUSE_PARS,
	IDC_MOUSE_OPTG,
	IDC_MOUSE_SWAPB,
	IDC_MOUSE_MIDB,
	IDC_MOUSE_TBG,
	NULL
};

//===========================================================================
//
//	Page del joystick
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CJoyPage::CJoyPage()
{
	// Always set ID and Help
	m_dwID = MAKEID('J', 'O', 'Y', ' ');
	m_nTemplate = IDD_JOYPAGE;
	m_uHelpID = IDC_JOY_HELP;
	m_pInput = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CJoyPage::OnInitDialog()
{
	int i;
	int nDevice;
	CString strNoA;
	CString strDesc;
	DIDEVCAPS ddc;
	CComboBox *pComboBox;

	ASSERT(this);

	// Base class
	CConfigPage::OnInitDialog();

	if (!m_pInput) {
		CFrmWnd *pFrmWnd = ResolveFrmWnd(this);
		if (pFrmWnd) {
			m_pInput = pFrmWnd->GetInput();
		}
	}
	if (!m_pInput) {
		return FALSE;
	}

	// Get cadena "No Assign"
	::GetMsg(IDS_JOY_NOASSIGN, strNoA);

	// Cuadro combinado de puertos
	for (i=0; i<2; i++) {
		// Cuadro combinado�E�擾
		if (i == 0) {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC1);
		}
		else {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC2);
		}
		ASSERT(pComboBox);

		// Limpiar
		pComboBox->ResetContent();

		// Configurar cadenas secuencialmente
		pComboBox->AddString(strNoA);
		::GetMsg(IDS_JOY_ATARI, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_ATARISS, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_CYBERA, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_CYBERD, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_MD3, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_MD6, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_CPSF, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_CPSFMD, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_MAGICAL, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_LR, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_PACLAND, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_BM, strDesc);
		pComboBox->AddString(strDesc);

		// Cursor
		pComboBox->SetCurSel(m_pConfig->joy_type[i]);

		// �E�Ή�Buttons�E��E�Initialization
		OnSelChg(pComboBox);
	}

	// Cuadro combinado de dispositivos
	for (i=0; i<2; i++) {
		// Cuadro combinado�E�擾
		if (i == 0) {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_DEVCA);
		}
		else {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_DEVCB);
		}
		ASSERT(pComboBox);

		// Limpiar
		pComboBox->ResetContent();

		// Configuration "No Assign"
		pComboBox->AddString(strNoA);

		// �E�f�E�o�E�C�E�XBucle
		for (nDevice=0; ; nDevice++) {
			if (!m_pInput->GetJoyCaps(nDevice, strDesc, &ddc)) {
				// No hay mas dispositivos
				break;
			}

			// Agregar
			pComboBox->AddString(strDesc);
		}

		// �E�ݒ荀�E�ڂ�Cursor
		if (m_pConfig->joy_dev[i] < pComboBox->GetCount()) {
			pComboBox->SetCurSel(m_pConfig->joy_dev[i]);
		}
		else {
			// El valor excede el numero de dispositivos -> foco en "No Assign"
			pComboBox->SetCurSel(0);
		}

		// �E�Ή�Buttons�E��E�Initialization
		OnSelChg(pComboBox);
	}

	// Keyboard�E�f�E�o�E�C�E�X
	pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_DEVCC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	pComboBox->AddString(strNoA);
	pComboBox->SetCurSel(0);
	OnSelChg(pComboBox);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CJoyPage::OnOK()
{
	int i;
	int nButton;
	CComboBox *pComboBox;
	CInput::JOYCFG cfg;

	ASSERT(this);

	// Cuadro combinado de puertos
	for (i=0; i<2; i++) {
		// Cuadro combinado�E�擾
		if (i == 0) {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC1);
		}
		else {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC2);
		}

		// Get valor de configuracion
		m_pConfig->joy_type[i] = pComboBox->GetCurSel();
		m_pInput->joyType[i] = m_pConfig->joy_type[i];
	}

	// Cuadro combinado de dispositivos
	for (i=0; i<2; i++) {
		// Cuadro combinado�E�擾
		if (i == 0) {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_DEVCA);
		}
		else {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_DEVCB);
		}
		ASSERT(pComboBox);

		// Get valor de configuracion
		m_pConfig->joy_dev[i] = pComboBox->GetCurSel();
	}

	// Create m_pConfig para ejes y botones basado en la configuracion actual
	for (i=0; i<CInput::JoyDevices; i++) {
		// Leer la configuracion de operacion actual
		m_pInput->GetJoyCfg(i, &cfg);

		// Buttons
		for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
			// Asignacion�E��E�Rapid-fire�E��E��E��E��E��E�
			if (i == 0) {
				// Puerto 1
				m_pConfig->joy_button0[nButton] =
						cfg.dwButton[nButton] | (cfg.dwRapid[nButton] << 8);
			}
			else {
				// Puerto 2
				m_pConfig->joy_button1[nButton] =
						cfg.dwButton[nButton] | (cfg.dwRapid[nButton] << 8);
			}
		}
	}

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CJoyPage::OnCancel()
{
	// CInput�E�ɑ΂��E�ēƎ��E��E�ApplyCfg(�E�ݒ��E�Editar�E�O�E�ɖ߂�)
	m_pInput->ApplyCfg(m_pConfig);

	// Base class
	CConfigPage::OnCancel();
}

//---------------------------------------------------------------------------
//
//	Notificacion de comando
//
//---------------------------------------------------------------------------
BOOL CJoyPage::OnCommand(WPARAM wParam, LPARAM lParam)
{
	CComboBox *pComboBox;
	UINT nID;

	ASSERT(this);

	// �E��E��E�M�E��E�Get ID
	nID = (UINT)LOWORD(wParam);

	// CBN_SELCHANGE
	if (HIWORD(wParam) == CBN_SELCHANGE) {
		pComboBox = (CComboBox*)GetDlgItem(nID);
		ASSERT(pComboBox);

		// Rutina dedicada
		OnSelChg(pComboBox);
		return TRUE;
	}

	// BN_CLICKED
	if (HIWORD(wParam) == BN_CLICKED) {
		if ((nID == IDC_JOY_PORTD1) || (nID == IDC_JOY_PORTD2)) {
			// Detalles del lado del puerto
			OnDetail(nID);
		}
		else {
			// Device-side configuration
			OnSetting(nID);
		}
		return TRUE;
	}

	// Base class
	return CConfigPage::OnCommand(wParam, lParam);
}

//---------------------------------------------------------------------------
//
//	Cambio en el cuadro combinado
//
//---------------------------------------------------------------------------
void FASTCALL CJoyPage::OnSelChg(CComboBox *pComboBox)
{
	CButton *pButton;

	ASSERT(this);
	ASSERT(pComboBox);

	// �E�Ή�Buttons�E��E��E�擾
	pButton = GetCorButton(pComboBox->GetDlgCtrlID());
	if (!pButton) {
		return;
	}

	// Cuadro combinado�E�̑I�E��E��E�󋵂ɂ��E��E�Č��E�߂�
	if (pComboBox->GetCurSel() == 0) {
		// (Asignacion�E�Ȃ�)�E��E�Buttons�E��E��E��E�
		pButton->EnableWindow(FALSE);
	}
	else {
		// Buttons�E�L�E��E�
		pButton->EnableWindow(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	�E�|�E�[�E�g�E�ڍ�
//
//---------------------------------------------------------------------------
void FASTCALL CJoyPage::OnDetail(UINT nButton)
{
	CComboBox *pComboBox;
	int nPort;
	int nType;
	CString strDesc;
	CJoyDetDlg dlg(this);

	ASSERT(this);
	ASSERT(nButton != 0);

	// �E�|�E�[�E�g�E�擾
	nPort = 0;
	if (nButton == IDC_JOY_PORTD2) {
		nPort++;
	}

	// Get el cuadro combinado correspondiente
	pComboBox = GetCorCombo(nButton);
	if (!pComboBox) {
		return;
	}

	// �E�I�E��E�ԍ��E�𓾂�
	nType = pComboBox->GetCurSel();
	if (nType == 0) {
		return;
	}

	// �E�I�E��E�ԍ��E��E��E��E�AGet nombre
	pComboBox->GetLBText(nType, strDesc);

	// �E�p�E��E��E��E��E�[�E�^�E��E�n�E��E��E�AExecute dialogo
	dlg.m_strDesc = strDesc;
	dlg.m_nPort = nPort;
	dlg.m_nType = nType;
	dlg.DoModal();
}

//---------------------------------------------------------------------------
//
//	�E�f�E�o�E�C�E�X�E�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL CJoyPage::OnSetting(UINT nButton)
{
	CComboBox *pComboBox;
	CJoySheet sheet(this);
	CInput::JOYCFG cfg;
	int nJoy;
	int nCombo;
	int nType[PPI::PortMax];

	ASSERT(this);
	ASSERT(nButton != 0);

	// Get el cuadro combinado correspondiente
	pComboBox = GetCorCombo(nButton);
	if (!pComboBox) {
		return;
	}

	// �E�f�E�o�E�C�E�X�E�C�E��E��E�f�E�b�E�N�E�X�E�擾
	nJoy = -1;
	switch (pComboBox->GetDlgCtrlID()) {
		// �E�f�E�o�E�C�E�XA
		case IDC_JOY_DEVCA:
			nJoy = 0;
			break;

		// �E�f�E�o�E�C�E�XB
		case IDC_JOY_DEVCB:
			nJoy = 1;
			break;

		// Other(�E�Q�E�[�E��E��E�R�E��E��E�g�E��E��E�[�E��E��E�ł͂Ȃ��E�f�E�o�E�C�E�X)
		default:
			return;
	}
	ASSERT((nJoy == 0) || (nJoy == 1));
	ASSERT(nJoy < CInput::JoyDevices);

	// Cuadro combinado�E�̑I�E��E�ԍ��E�𓾂�
	nCombo = pComboBox->GetCurSel();
	if (nCombo == 0) {
		// Asignacion�E��E��E��E�
		return;
	}

	// Cuadro combinado�E�̑I�E��E�ԍ��E�𓾂�B0(Asignacion�E��E��E��E�)�E��E��E��E��E�e
	pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC1);
	ASSERT(pComboBox);
	nType[0] = pComboBox->GetCurSel();
	pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC2);
	ASSERT(pComboBox);
	nType[1] = pComboBox->GetCurSel();

	// �E��E��E�݂̃W�E��E��E�C�E�X�E�e�E�B�E�b�E�N�E�ݒ��E�ۑ�
	m_pInput->GetJoyCfg(nJoy, &cfg);

	// �E�p�E��E��E��E��E�[�E�^�E�ݒ�
	sheet.SetParam(nJoy, nCombo, nType);

	// Execute dialog(�E�W�E��E��E�C�E�X�E�e�E�B�E�b�E�N�E�؂�ւ��E��E��E��E��E�݁ACancelar�E�Ȃ�ݒ�߂�)
	m_pInput->EnableJoy(FALSE);
	if (sheet.DoModal() != IDOK) {
		m_pInput->SetJoyCfg(nJoy, &cfg);
	}
	m_pInput->EnableJoy(TRUE);
}

//---------------------------------------------------------------------------
//
//	�E�Ή��E��E��E��E�Buttons�E��E��E�擾
//
//---------------------------------------------------------------------------
CButton* CJoyPage::GetCorButton(UINT nComboBox)
{
	int i;
	CButton *pButton;

	ASSERT(this);
	ASSERT(nComboBox != 0);

	pButton = NULL;

	// Tabla de controles�E��E�Busqueda
	for (i=0; ; i+=2) {
		// �E�I�E�[Check
		if (ControlTable[i] == NULL) {
			return NULL;
		}

		// Si coincide, OK
		if (ControlTable[i] == nComboBox) {
			// �E�Ή��E��E��E��E�Buttons�E�𓾂�
			pButton = (CButton*)GetDlgItem(ControlTable[i + 1]);
			break;
		}
	}

	ASSERT(pButton);
	return pButton;
}

//---------------------------------------------------------------------------
//
//	Get el cuadro combinado correspondiente
//
//---------------------------------------------------------------------------
CComboBox* CJoyPage::GetCorCombo(UINT nButton)
{
	int i;
	CComboBox *pComboBox;

	ASSERT(this);
	ASSERT(nButton != 0);

	pComboBox = NULL;

	// Tabla de controles�E��E�Busqueda
	for (i=1; ; i+=2) {
		// �E�I�E�[Check
		if (ControlTable[i] == NULL) {
			return NULL;
		}

		// Si coincide, OK
		if (ControlTable[i] == nButton) {
			// Get el cuadro combinado correspondiente
			pComboBox = (CComboBox*)GetDlgItem(ControlTable[i - 1]);
			break;
		}
	}

	ASSERT(pComboBox);
	return pComboBox;
}

//---------------------------------------------------------------------------
//
//	Tabla de controles
//	�E��E�Cuadro combinado�E��E�Buttons�E�Ƃ̑��E�ݑΉ��E��E��E�Ƃ邽�E��E�
//
//---------------------------------------------------------------------------
UINT CJoyPage::ControlTable[] = {
	IDC_JOY_PORTC1, IDC_JOY_PORTD1,
	IDC_JOY_PORTC2, IDC_JOY_PORTD2,
	IDC_JOY_DEVCA, IDC_JOY_DEVAA,
	IDC_JOY_DEVCB, IDC_JOY_DEVAB,
	IDC_JOY_DEVCC, IDC_JOY_DEVAC,
	NULL, NULL
};

//===========================================================================
//
//	Dialogo de detalles del joystick
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CJoyDetDlg::CJoyDetDlg(CWnd *pParent) : CDialog(IDD_JOYDETDLG, pParent)
{
	// English�E���E�ւ̑Ή�
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_JOYDETDLG);
		m_nIDHelp = IDD_US_JOYDETDLG;
	}

	m_strDesc.Empty();
	m_nPort = -1;
	m_nType = 0;
}

//---------------------------------------------------------------------------
//
//	�E�_�E�C�E�A�E��E��E�OInitialization
//
//---------------------------------------------------------------------------
BOOL CJoyDetDlg::OnInitDialog()
{
	CString strBase;
	CString strText;
	CStatic *pStatic;
	PPI *pPPI;
	JoyDevice *pDevice;

	// Base class
	CDialog::OnInitDialog();

	ASSERT(m_strDesc.GetLength() > 0);
	ASSERT((m_nPort >= 0) && (m_nPort < PPI::PortMax));
	ASSERT(m_nType >= 1);

	// Nombre del puerto
	GetWindowText(strBase);
	strText.Format(strBase, m_nPort + 1);
	SetWindowText(strText);

	// Nombre
	pStatic = (CStatic*)GetDlgItem(IDC_JOYDET_NAMEL);
	ASSERT(pStatic);
	pStatic->SetWindowText(m_strDesc);

	// Create joystick
	pPPI = (PPI*)::GetVM()->SearchDevice(MAKEID('P', 'P', 'I', ' '));
	ASSERT(pPPI);
	pDevice = pPPI->CreateJoy(m_nPort, m_nType);
	ASSERT(pDevice);

	// Number de ejes
	pStatic = (CStatic*)GetDlgItem(IDC_JOYDET_AXISS);
	ASSERT(pStatic);
	pStatic->GetWindowText(strBase);
	strText.Format(strBase, pDevice->GetAxes());
	pStatic->SetWindowText(strText);

	// Buttons�E��E�
	pStatic = (CStatic*)GetDlgItem(IDC_JOYDET_BUTTONS);
	ASSERT(pStatic);
	pStatic->GetWindowText(strBase);
	strText.Format(strBase, pDevice->GetButtons());
	pStatic->SetWindowText(strText);

	// Tipo (Analogico/Digital)
	if (pDevice->IsAnalog()) {
		pStatic = (CStatic*)GetDlgItem(IDC_JOYDET_TYPES);
		::GetMsg(IDS_JOYDET_ANALOG, strText);
		pStatic->SetWindowText(strText);
	}

	// Number de datos
	pStatic = (CStatic*)GetDlgItem(IDC_JOYDET_DATASS);
	ASSERT(pStatic);
	pStatic->GetWindowText(strBase);
	strText.Format(strBase, pDevice->GetDatas());
	pStatic->SetWindowText(strText);

	// �E�W�E��E��E�C�E�X�E�e�E�B�E�b�E�NDelete
	delete pDevice;

	return TRUE;
}

//===========================================================================
//
//	Buttons�E�ݒ�y�E�[�E�W
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CBtnSetPage::CBtnSetPage()
{
	int i;

#if defined(_DEBUG)
	ASSERT(CInput::JoyButtons <= (sizeof(m_bButton)/sizeof(BOOL)));
	ASSERT(CInput::JoyButtons <= (sizeof(m_rectLabel)/sizeof(CRect)));
#endif	// _DEBUG

	m_pInput = NULL;

	// �E�W�E��E��E�C�E�X�E�e�E�B�E�b�E�N�E�ԍ��E��E�Limpiar
	m_nJoy = -1;

	// �E�^�E�C�E�v�E�ԍ��E��E�Limpiar
	for (i=0; i<PPI::PortMax; i++) {
		m_nType[i] = -1;
	}
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CBtnSetPage, CPropertyPage)
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_HSCROLL()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Create
//
//---------------------------------------------------------------------------
void FASTCALL CBtnSetPage::Init(CPropertySheet *pSheet)
{
	int nID;

	ASSERT(this);

	// Memorizar hoja padre
	ASSERT(pSheet);
	m_pSheet = pSheet;

	// IDAceptar
	nID = IDD_BTNSETPAGE;
	if (!::IsJapanese()) {
		nID += 50;
	}

	// Construccion
	CommonConstruct(MAKEINTRESOURCE(nID), 0);

	// �E�e�E�V�E�[�E�g�E��E�Agregar
	pSheet->AddPage(this);
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CBtnSetPage::OnInitDialog()
{
	CJoySheet *pJoySheet;
	int nButton;
	int nButtons;
	int nPort;
	int nCandidate;
	CStatic *pStatic;
	CComboBox *pComboBox;
	CSliderCtrl *pSlider;
	CString strText;
	CString strBase;
	CInput::JOYCFG cfg;
	DWORD dwData;
	PPI *pPPI;
	JoyDevice *pJoyDevice;
	CString strDesc;

	ASSERT(this);
	ASSERT(m_pSheet);

	// Base class
	CPropertyPage::OnInitDialog();

	if (!m_pInput) {
		CFrmWnd *pFrmWnd = ResolveFrmWnd(this);
		if (pFrmWnd) {
			m_pInput = pFrmWnd->GetInput();
		}
	}
	if (!m_pInput) {
		return FALSE;
	}

	// �E�e�E�N�E��E��E�X�E��E�Initialization(CPropertySheet�E��E�OnInitDialog�E��E��E��E��E��E��E�Ȃ�)
	pJoySheet = (CJoySheet*)m_pSheet;
	pJoySheet->InitSheet();
	ASSERT((m_nJoy >= 0) && (m_nJoy < CInput::JoyDevices));

	// Get la configuracion de joystick actual
	m_pInput->GetJoyCfg(m_nJoy, &cfg);

	// Get PPI
	pPPI = (PPI*)::GetVM()->SearchDevice(MAKEID('P', 'P', 'I', ' '));
	ASSERT(pPPI);

	// Buttons�E��E��E�擾
	nButtons = pJoySheet->GetButtons();

	// Get texto base
	::GetMsg(IDS_JOYSET_BTNPORT, strBase);

	// Control settings
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		// Etiqueta
		pStatic = (CStatic*)GetDlgItem(GetControl(nButton, BtnLabel));
		ASSERT(pStatic);
		if (nButton < nButtons) {
			// �E�L�E��E�(�E�E�E�B�E��E��E�h�E�EDelete)
			pStatic->GetWindowRect(&m_rectLabel[nButton]);
			ScreenToClient(&m_rectLabel[nButton]);
			pStatic->DestroyWindow();
		}
		else {
			// Inactivo (prohibir ventana)
			m_rectLabel[nButton].top = 0;
			m_rectLabel[nButton].left = 0;
			pStatic->EnableWindow(FALSE);
		}

		// Cuadro combinado
		pComboBox = (CComboBox*)GetDlgItem(GetControl(nButton, BtnCombo));
		ASSERT(pComboBox);
		if (nButton < nButtons) {
			// �E�L�E��E�(�E��E��E��E�Agregar)
			pComboBox->ResetContent();

			// Configurar "No Assign"
			::GetMsg(IDS_JOYSET_NOASSIGN, strText);
			pComboBox->AddString(strText);

			// �E�|�E�[�E�g�E�AButtons�E��E��E��E��E�
			for (nPort=0; nPort<PPI::PortMax; nPort++) {
				// Get device de joystick temporal
				pJoyDevice = pPPI->CreateJoy(0, m_nType[nPort]);

				for (nCandidate=0; nCandidate<PPI::ButtonMax; nCandidate++) {
					// �E�W�E��E��E�C�E�X�E�e�E�B�E�b�E�N�E�f�E�o�E�C�E�X�E��E��E��E�ButtonsGet nombre
					GetButtonDesc(pJoyDevice->GetButtonDesc(nCandidate), strDesc);

					// Formato
					strText.Format(strBase, nPort + 1, nCandidate + 1, (LPCTSTR)strDesc);
					pComboBox->AddString(strText);
				}

				// �E��E��E�W�E��E��E�C�E�X�E�e�E�B�E�b�E�N�E�f�E�o�E�C�E�X�E��E�Delete
				delete pJoyDevice;
			}

			// Cursor�E�ݒ�
			pComboBox->SetCurSel(0);
			if ((LOWORD(cfg.dwButton[nButton]) != 0) && (LOWORD(cfg.dwButton[nButton]) <= PPI::ButtonMax)) {
				if (cfg.dwButton[nButton] & 0x10000) {
					// Puerto 2
					pComboBox->SetCurSel(LOWORD(cfg.dwButton[nButton]) + PPI::ButtonMax);
				}
				else {
					// Puerto 1
					pComboBox->SetCurSel(LOWORD(cfg.dwButton[nButton]));
				}
			}
		}
		else {
			// Inactivo (prohibir ventana)
			pComboBox->EnableWindow(FALSE);
		}

		// Rapid-fire�E�X�E��E��E�C�E�_
		pSlider = (CSliderCtrl*)GetDlgItem(GetControl(nButton, BtnRapid));
		ASSERT(pSlider);
		if (nButton < nButtons) {
			// Activo (configurar rango y valor actual)
			pSlider->SetRange(0, CInput::JoyRapids);
			if (cfg.dwRapid[nButton] <= CInput::JoyRapids) {
				pSlider->SetPos(cfg.dwRapid[nButton]);
			}
		}
		else {
			// Inactivo (prohibir ventana)
			pSlider->EnableWindow(FALSE);
		}

		// Rapid-fire�E�l
		pStatic = (CStatic*)GetDlgItem(GetControl(nButton, BtnValue));
		ASSERT(pStatic);
		if (nButton < nButtons) {
			// �E�L�E��E�(�E��E��E��E��E�lMostrar)
			OnSlider(nButton);
			OnSelChg(nButton);
		}
		else {
			// �E��E��E��E�(Limpiar)
			strText.Empty();
			pStatic->SetWindowText(strText);
		}
	}

	// Buttons�E��E��E��E��E�l�E�ǂݎ��E�
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		m_bButton[nButton] = FALSE;
		dwData = m_pInput->GetJoyButton(m_nJoy, nButton);
		if ((dwData < 0x10000) && (dwData & 0x80)) {
			m_bButton[nButton] = TRUE;
		}
	}

	// Iniciar temporizador (fuego cada 100ms)
	m_nTimerID = SetTimer(IDD_BTNSETPAGE, 100, NULL);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Dibujar
//
//---------------------------------------------------------------------------
void CBtnSetPage::OnPaint()
{
	CPaintDC dc(this);

	// Dibujar�E��E��E�C�E��E�
	OnDraw(&dc, NULL, TRUE);
}

//---------------------------------------------------------------------------
//
//	Desplazamiento horizontal
//
//---------------------------------------------------------------------------
void CBtnSetPage::OnHScroll(UINT /*nSBCode*/, UINT /*nPos*/, CScrollBar *pBar)
{
	CSliderCtrl *pSlider;
	UINT nID;
	int nButton;

	// �E�R�E��E��E�g�E��E��E�[�E��E�ID�E��E��E�擾
	pSlider = (CSliderCtrl*)pBar;
	nID = pSlider->GetDlgCtrlID();

	// Buttons�E�C�E��E��E�f�E�b�E�N�E�X�E��E�Busqueda
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		if (GetControl(nButton, BtnRapid) == nID) {
			// Rutina dedicada�E��E��E�Ă�
			OnSlider(nButton);
			break;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Notificacion de comando
//
//---------------------------------------------------------------------------
BOOL CBtnSetPage::OnCommand(WPARAM wParam, LPARAM lParam)
{
	int nButton;
	UINT nID;

	ASSERT(this);

	// �E��E��E�M�E��E�Get ID
	nID = (UINT)LOWORD(wParam);

	// CBN_SELCHANGE
	if (HIWORD(wParam) == CBN_SELCHANGE) {
		// Buttons�E�C�E��E��E�f�E�b�E�N�E�X�E��E�Busqueda
		for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
			if (GetControl(nButton, BtnCombo) == nID) {
				OnSelChg(nButton);
				break;
			}
		}
	}

	// Base class
	return CPropertyPage::OnCommand(wParam, lParam);
}

//---------------------------------------------------------------------------
//
//	Dibujar�E��E��E�C�E��E�
//
//---------------------------------------------------------------------------
void FASTCALL CBtnSetPage::OnDraw(CDC *pDC, BOOL *pButton, BOOL bForce)
{
	int nButton;
	CFont *pFont;
	CString strBase;
	CString strText;

	ASSERT(this);
	ASSERT(pDC);

	// Color settings
	pDC->SetBkColor(::GetSysColor(COLOR_3DFACE));

	// Font settings
	pFont = (CFont*)pDC->SelectStockObject(DEFAULT_GUI_FONT);
	ASSERT(pFont);

	// Get la cadena base
	::GetMsg(IDS_JOYSET_BTNLABEL, strBase);

	// ButtonsBucle
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		// �E�L�E��E�(Mostrar�E��E��E�ׂ�)Buttons�E��E��E�ۂ�
		if ((m_rectLabel[nButton].left == 0) && (m_rectLabel[nButton].top == 0)) {
			// Buttons�E��E��E�Ȃ��E�̂ŁA�E��E��E��E��E�ɂ��E�ꂽ�E�X�E�^�E�e�E�B�E�b�E�N�E�e�E�L�E�X�E�g
			continue;
		}

		// !bForce�E�Ȃ�A�E��E�r�E��E��E��E�Aceptar
		if (!bForce) {
			ASSERT(pButton);
			if (m_bButton[nButton] == pButton[nButton]) {
				// �E��E�v�E��E��E�Ă��E��E�̂�Dibujar�E��E��E�Ȃ�
				continue;
			}
			// Difieren, guardar
			m_bButton[nButton] = pButton[nButton];
		}

		// �E�F�E��E�Aceptar
		if (m_bButton[nButton]) {
			// Presionado (Rojo)
			pDC->SetTextColor(RGB(255, 0, 0));
		}
		else {
			// No presionado (Negro)
			pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
		}

		// Mostrar
		strText.Format(strBase, nButton + 1);
		pDC->DrawText(strText, m_rectLabel[nButton],
						DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
	}

	// Restore font(Objetos�E��E�Delete�E��E��E�Ȃ��E�Ă悢)
	pDC->SelectObject(pFont);
}

//---------------------------------------------------------------------------
//
//	Timer
//
//---------------------------------------------------------------------------
void CBtnSetPage::OnTimer(UINT /*nTimerID*/)
{
	int nButton;
	BOOL bButton[CInput::JoyButtons];
	BOOL bFlag;
	DWORD dwData;
	CClientDC *pDC;

	ASSERT(this);

	// �E�t�E��E��E�OInitialization
	bFlag = FALSE;

	// �E��E��E�݂̃W�E��E��E�C�E�X�E�e�E�B�E�b�E�NButtons�E��E��E��E�ǂݎ��E�A�E��E�r
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		bButton[nButton] = FALSE;
		dwData = m_pInput->GetJoyButton(m_nJoy, nButton);
		if ((dwData < 0x10000) && (dwData & 0x80)) {
			bButton[nButton] = TRUE;
		}

		// Si es diferente, subir flag
		if (m_bButton[nButton] != bButton[nButton]) {
			bFlag = TRUE;
		}
	}

	// �E�t�E��E��E�O�E��E��E�オ�E��E��E�Ă��E��E�΁A�E��E�Dibujar
	if (bFlag) {
		pDC = new CClientDC(this);
		OnDraw(pDC, bButton, FALSE);
		delete pDC;
	}
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CBtnSetPage::OnOK()
{
	CJoySheet *pJoySheet;
	int nButtons;
	int nButton;
	CInput::JOYCFG cfg;
	CComboBox *pComboBox;
	CSliderCtrl *pSlider;
	int nSelect;

	// Timer�E��E�~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Get hoja padre
	pJoySheet = (CJoySheet*)m_pSheet;
	nButtons = pJoySheet->GetButtons();

	// Get los datos de configuracion actuales
	m_pInput->GetJoyCfg(m_nJoy, &cfg);

	// Leer controles y reflejar en la configuracion actual
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		// �E�L�E��E��E��E�Buttons�E��E�
		if (nButton >= nButtons) {
			// �E��E��E��E��E��E�Buttons�E�Ȃ̂ŁAAsignacion�E�ERapid-fire�E�Ƃ��E��E�0
			cfg.dwButton[nButton] = 0;
			cfg.dwRapid[nButton] = 0;
			continue;
		}

		// Leer asignacion
		pComboBox = (CComboBox*)GetDlgItem(GetControl(nButton, BtnCombo));
		ASSERT(pComboBox);
		nSelect = pComboBox->GetCurSel();

		// (Asignacion�E�Ȃ�)Check
		if (nSelect == 0) {
			// �E��E��E��E�Asignacion�E�Ȃ�AAsignacion�E�ERapid-fire�E�Ƃ��E��E�0
			cfg.dwButton[nButton] = 0;
			cfg.dwRapid[nButton] = 0;
			continue;
		}

		// Asignacion normal
		nSelect--;
		if (nSelect >= PPI::ButtonMax) {
			// Puerto 2
			cfg.dwButton[nButton] = (DWORD)(0x10000 | (nSelect - PPI::ButtonMax + 1));
		}
		else {
			// Puerto 1
			cfg.dwButton[nButton] = (DWORD)(nSelect + 1);
		}

		// Rapid-fire
		pSlider = (CSliderCtrl*)GetDlgItem(GetControl(nButton, BtnRapid));
		ASSERT(pSlider);
		cfg.dwRapid[nButton] = pSlider->GetPos();
	}

	// Reflejar datos de configuracion
	m_pInput->SetJoyCfg(m_nJoy, &cfg);

	// Base class
	CPropertyPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CBtnSetPage::OnCancel()
{
	// Timer�E��E�~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Base class
	CPropertyPage::OnCancel();
}

//---------------------------------------------------------------------------
//
//	�E�X�E��E��E�C�E�_Modificacion
//
//---------------------------------------------------------------------------
void FASTCALL CBtnSetPage::OnSlider(int nButton)
{
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CString strText;
	int nPos;

	ASSERT(this);
	ASSERT((nButton >= 0) && (nButton < CInput::JoyButtons));

	// �E�|�E�W�E�V�E��E��E��E��E��E��E�擾
	pSlider = (CSliderCtrl*)GetDlgItem(GetControl(nButton, BtnRapid));
	ASSERT(pSlider);
	nPos = pSlider->GetPos();

	// �E�Ή��E��E��E��E�Etiqueta�E��E��E�擾
	pStatic = (CStatic*)GetDlgItem(GetControl(nButton, BtnValue));
	ASSERT(pStatic);

	// Set valuees desde la tabla
	if ((nPos >= 0) && (nPos <= CInput::JoyRapids)) {
		// �E�Œ菬�E��E��E�_Procesamiento
		if (RapidTable[nPos] & 1) {
			strText.Format(_T("%d.5"), RapidTable[nPos] >> 1);
		}
		else {
			strText.Format(_T("%d"), RapidTable[nPos] >> 1);
		}
	}
	else {
		strText.Empty();
	}
	pStatic->SetWindowText(strText);
}

//---------------------------------------------------------------------------
//
//	Cambio en el cuadro combinado
//
//---------------------------------------------------------------------------
void FASTCALL CBtnSetPage::OnSelChg(int nButton)
{
	CComboBox *pComboBox;
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	int nPos;

	ASSERT(this);
	ASSERT((nButton >= 0) && (nButton < CInput::JoyButtons));

	// �E�|�E�W�E�V�E��E��E��E��E��E��E�擾
	pComboBox = (CComboBox*)GetDlgItem(GetControl(nButton, BtnCombo));
	ASSERT(pComboBox);
	nPos = pComboBox->GetCurSel();

	// �E�Ή��E��E��E��E�X�E��E��E�C�E�_�E�AEtiqueta�E��E��E�擾
	pSlider = (CSliderCtrl*)GetDlgItem(GetControl(nButton, BtnRapid));
	ASSERT(pSlider);
	pStatic = (CStatic*)GetDlgItem(GetControl(nButton, BtnValue));
	ASSERT(pStatic);

	// Configurar activacion/desactivacion de la ventana
	if (nPos == 0) {
		pSlider->EnableWindow(FALSE);
		pStatic->EnableWindow(FALSE);
	}
	else {
		pSlider->EnableWindow(TRUE);
		pStatic->EnableWindow(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Retrieve the button description
//
//---------------------------------------------------------------------------
void FASTCALL CBtnSetPage::GetButtonDesc(const char *pszDesc, CString& strDesc)
{
	LPCTSTR lpszT;

	ASSERT(this);

	// Initialization
	strDesc.Empty();

	// Return immediately if the source string is NULL
	if (!pszDesc) {
		return;
	}

	// Convert to TCHAR
	lpszT = A2CT(pszDesc);

	// Build a parenthesized string
	strDesc = _T("(");
	strDesc += lpszT;
	strDesc += _T(")");
}

//---------------------------------------------------------------------------
//
//	Get control
//
//---------------------------------------------------------------------------
UINT FASTCALL CBtnSetPage::GetControl(int nButton, CtrlType ctlType) const
{
	int nType;

	ASSERT(this);
	ASSERT((nButton >= 0) && (nButton < CInput::JoyButtons));

	// Get type
	nType = (int)ctlType;
	ASSERT((nType >= 0) && (nType < 4));

	// Get ID
	return ControlTable[(nButton << 2) + nType];
}

//---------------------------------------------------------------------------
//
//	Control table
//
//---------------------------------------------------------------------------
const UINT CBtnSetPage::ControlTable[] = {
	IDC_BTNSET_BTNL1, IDC_BTNSET_ASSIGNC1, IDC_BTNSET_RAPIDC1, IDC_BTNSET_RAPIDL1,
	IDC_BTNSET_BTNL2, IDC_BTNSET_ASSIGNC2, IDC_BTNSET_RAPIDC2, IDC_BTNSET_RAPIDL2,
	IDC_BTNSET_BTNL3, IDC_BTNSET_ASSIGNC3, IDC_BTNSET_RAPIDC3, IDC_BTNSET_RAPIDL3,
	IDC_BTNSET_BTNL4, IDC_BTNSET_ASSIGNC4, IDC_BTNSET_RAPIDC4, IDC_BTNSET_RAPIDL4,
	IDC_BTNSET_BTNL5, IDC_BTNSET_ASSIGNC5, IDC_BTNSET_RAPIDC5, IDC_BTNSET_RAPIDL5,
	IDC_BTNSET_BTNL6, IDC_BTNSET_ASSIGNC6, IDC_BTNSET_RAPIDC6, IDC_BTNSET_RAPIDL6,
	IDC_BTNSET_BTNL7, IDC_BTNSET_ASSIGNC7, IDC_BTNSET_RAPIDC7, IDC_BTNSET_RAPIDL7,
	IDC_BTNSET_BTNL8, IDC_BTNSET_ASSIGNC8, IDC_BTNSET_RAPIDC8, IDC_BTNSET_RAPIDL8,
	IDC_BTNSET_BTNL9, IDC_BTNSET_ASSIGNC9, IDC_BTNSET_RAPIDC9, IDC_BTNSET_RAPIDL9,
	IDC_BTNSET_BTNL10,IDC_BTNSET_ASSIGNC10,IDC_BTNSET_RAPIDC10,IDC_BTNSET_RAPIDL10,
	IDC_BTNSET_BTNL11,IDC_BTNSET_ASSIGNC11,IDC_BTNSET_RAPIDC11,IDC_BTNSET_RAPIDL11,
	IDC_BTNSET_BTNL12,IDC_BTNSET_ASSIGNC12,IDC_BTNSET_RAPIDC12,IDC_BTNSET_RAPIDL12
};

//---------------------------------------------------------------------------
//
//	Rapid-fire timing table
//	Precomputed timing values keep the update path lightweight
//
//---------------------------------------------------------------------------
const int CBtnSetPage::RapidTable[CInput::JoyRapids + 1] = {
	0,
	4,
	6,
	8,
	10,
	15,
	20,
	24,
	30,
	40,
	60
};

//===========================================================================
//
//	Joystick property sheet
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CJoySheet::CJoySheet(CWnd *pParent) : CPropertySheet(IDS_JOYSET, pParent)
{
	CFrmWnd *pFrmWnd;
	int i;

	// Use the English title when the UI is not in Japanese
	if (!::IsJapanese()) {
		::GetMsg(IDS_JOYSET, m_strCaption);
	}

	// Remove the Apply button
	m_psh.dwFlags |= PSH_NOAPPLYNOW;

	// Get the CInput instance
	pFrmWnd = ResolveFrmWnd(pParent);
	m_pInput = pFrmWnd ? pFrmWnd->GetInput() : NULL;

	// Parameter initialization
	m_nJoy = -1;
	m_nCombo = -1;
	for (i=0; i<PPI::PortMax; i++) {
		m_nType[i] = -1;
	}

	// Page initialization
	m_BtnSet.Init(this);
}

//---------------------------------------------------------------------------
//
//	�E�p�E��E��E��E��E�[�E�^�E�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL CJoySheet::SetParam(int nJoy, int nCombo, int nType[])
{
	int i;

	ASSERT(this);
	ASSERT((nJoy == 0) || (nJoy == 1));
	ASSERT(nJoy < CInput::JoyDevices);
	ASSERT(nCombo >= 1);
	ASSERT(nType);

	// �E�L�E��E�(Cuadro combinado�E��E�-1)
	m_nJoy = nJoy;
	m_nCombo = nCombo - 1;
	for (i=0; i<PPI::PortMax; i++) {
		m_nType[i] = nType[i];
	}

	// CapsLimpiar
	memset(&m_DevCaps, 0, sizeof(m_DevCaps));
}

//---------------------------------------------------------------------------
//
//	�E�V�E�[�E�gInitialization
//
//---------------------------------------------------------------------------
void FASTCALL CJoySheet::InitSheet()
{
	int i;
	CString strDesc;
	CString strFmt;
	CString strText;

	ASSERT(this);
	ASSERT((m_nJoy == 0) || (m_nJoy == 1));
	ASSERT(m_nJoy < CInput::JoyDevices);
	ASSERT(m_nCombo >= 0);
	if (!m_pInput) {
		return;
	}

	// Get Caps del dispositivo
	m_pInput->GetJoyCaps(m_nCombo, strDesc, &m_DevCaps);

	// �E�E�E�B�E��E��E�h�E�E�E�e�E�L�E�X�E�gEditar
	GetWindowText(strFmt);
	strText.Format(strFmt, _T('A' + m_nJoy), (LPCTSTR)strDesc);
	SetWindowText(strText);

	// Distribuir parametros a cada pagina
	m_BtnSet.m_nJoy = m_nJoy;
	for (i=0; i<PPI::PortMax; i++) {
		m_BtnSet.m_nType[i] = m_nType[i];
	}
}

//---------------------------------------------------------------------------
//
//	Number de ejes�E�擾
//
//---------------------------------------------------------------------------
int FASTCALL CJoySheet::GetAxes() const
{
	ASSERT(this);

	return (int)m_DevCaps.dwAxes;
}

//---------------------------------------------------------------------------
//
//	Buttons�E��E��E�擾
//
//---------------------------------------------------------------------------
int FASTCALL CJoySheet::GetButtons() const
{
	ASSERT(this);

	return (int)m_DevCaps.dwButtons;
}

//===========================================================================
//
//	Page SASI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSASIPage::CSASIPage()
{
	int i;

	// Always set ID and Help
	m_dwID = MAKEID('S', 'A', 'S', 'I');
	m_nTemplate = IDD_SASIPAGE;
	m_uHelpID = IDC_SASI_HELP;

	// SASIGet dispositivo
	m_pSASI = (SASI*)::GetVM()->SearchDevice(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(m_pSASI);

	// �E��E�Initialization
	m_bInit = FALSE;
	m_nDrives = -1;

	ASSERT(SASI::SASIMax <= 16);
	for (i=0; i<SASI::SASIMax; i++) {
		m_szFile[i][0] = _T('\0');
	}
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSASIPage, CConfigPage)
	ON_WM_VSCROLL()
	ON_NOTIFY(NM_CLICK, IDC_SASI_LIST, OnClick)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CSASIPage::OnInitDialog()
{
	CSpinButtonCtrl *pSpin;
	CButton *pButton;
	CListCtrl *pListCtrl;
	CClientDC *pDC;
	CString strCaption;
	CString strFile;
	TEXTMETRIC tm;
	LONG cx;
	int i;

	// Base class
	CConfigPage::OnInitDialog();

	// Initialization�E�t�E��E��E�OUp�E�ANumber of drives�E�擾
	m_bInit = TRUE;
	m_nDrives = m_pConfig->sasi_drives;
	ASSERT((m_nDrives >= 0) && (m_nDrives <= SASI::SASIMax));

	// Load cadenas de texto
	::GetMsg(IDS_SASI_DEVERROR, m_strError);

	// Number of drives
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SASI_DRIVES);
	ASSERT(pSpin);
	pSpin->SetBase(10);
	pSpin->SetRange(0, SASI::SASIMax);
	pSpin->SetPos(m_nDrives);

	// �E��E��E��E��E��E��E�X�E�C�E�b�E�`�E��E��E��E�Actualizacion
	pButton = (CButton*)GetDlgItem(IDC_SASI_MEMSWB);
	ASSERT(pButton);
	if (m_pConfig->sasi_sramsync) {
		pButton->SetCheck(1);
	}
	else {
		pButton->SetCheck(0);
	}

	// Get file name
	for (i=0; i<SASI::SASIMax; i++) {
		_tcscpy(m_szFile[i], m_pConfig->sasi_file[i]);
	}

	// Get text metrics
	pDC = new CClientDC(this);
	::GetTextMetrics(pDC->m_hDC, &tm);
	delete pDC;
	cx = tm.tmAveCharWidth;

	// �E��E��E�X�E�gControl settings
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SASI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->DeleteAllItems();
	::GetMsg(IDS_SASI_CAPACITY, strCaption);
	::GetMsg(IDS_SASI_FILENAME, strFile);
	if (::IsJapanese()) {
		pListCtrl->InsertColumn(0, _T("No."), LVCFMT_LEFT, cx * 4, 0);
	}
	else {
		pListCtrl->InsertColumn(0, _T("No."), LVCFMT_LEFT, cx * 5, 0);
	}
	pListCtrl->InsertColumn(1, strCaption, LVCFMT_CENTER,  cx * 6, 0);
	pListCtrl->InsertColumn(2, strFile, LVCFMT_LEFT, cx * 28, 0);

	// Full row select option for list control (COMCTL32.DLL v4.71+)
	pListCtrl->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
	UpdateList();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Page activa
//
//---------------------------------------------------------------------------
BOOL CSASIPage::OnSetActive()
{
	CSpinButtonCtrl *pSpin;
	CSCSIPage *pSCSIPage;
	BOOL bEnable;

	// Base class
	if (!CConfigPage::OnSetActive()) {
		return FALSE;
	}

	// Get la interfaz SCSI dinamicamente
	ASSERT(m_pSheet);
	pSCSIPage = (CSCSIPage*)m_pSheet->SearchPage(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(pSCSIPage);
	if (pSCSIPage->GetInterface(m_pConfig) == 2) {
		// Interfaz SCSI interna (SASI no disponible)
		bEnable = FALSE;
	}
	else {
		// SASI o interfaz SCSI externa
		bEnable = TRUE;
	}

	// Controls enabled/disabled
	if (bEnable) {
		// �E�L�E��E��E�̏ꍇ�E�A�E�X�E�s�E��E�Buttons�E��E��E�猻�E�݂�Number of drives�E��E��E�擾
		pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SASI_DRIVES);
		ASSERT(pSpin);
		if (pSpin->GetPos() > 0 ) {
			// Lista activa / Unit activa
			EnableControls(TRUE, TRUE);
		}
		else {
			// Lista inactiva / Unit activa
			EnableControls(FALSE, TRUE);
		}
	}
	else {
		// Lista inactiva / Unit inactiva
		EnableControls(FALSE, FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CSASIPage::OnOK()
{
	int i;
	TCHAR szPath[FILEPATH_MAX];
	CButton *pButton;
	CListCtrl *pListCtrl;

	// Number of drives
	ASSERT((m_nDrives >= 0) && (m_nDrives <= SASI::SASIMax));
	m_pConfig->sasi_drives = m_nDrives;

	// Nombre de archivo
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SASI_LIST);
	ASSERT(pListCtrl);
	for (i=0; i<m_nDrives; i++) {
		pListCtrl->GetItemText(i, 2, szPath, FILEPATH_MAX);
		_tcscpy(m_pConfig->sasi_file[i], szPath);
	}

	// Check�E�{�E�b�E�N�E�X(SASI�E�ESCSI�E�Ƃ��E��E��E�ʐݒ�)
	pButton = (CButton*)GetDlgItem(IDC_SASI_MEMSWB);
	ASSERT(pButton);
	if (pButton->GetCheck() == 1) {
		m_pConfig->sasi_sramsync = TRUE;
		m_pConfig->scsi_sramsync = TRUE;
	}
	else {
		m_pConfig->sasi_sramsync = FALSE;
		m_pConfig->scsi_sramsync = FALSE;
	}

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Desplazamiento vertical
//
//---------------------------------------------------------------------------
void CSASIPage::OnVScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar* /*pBar*/)
{
	ASSERT(this);
	ASSERT(nPos <= SASI::SASIMax);

	// Number of drivesActualizacion
	m_nDrives = nPos;

	// Controls enabled/disabled
	if (m_nDrives > 0) {
		EnableControls(TRUE);
	}
	else {
		EnableControls(FALSE);
	}

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	Clic en control de lista
//
//---------------------------------------------------------------------------
void CSASIPage::OnClick(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	CListCtrl *pListCtrl;
	int i;
	int nID;
	int nCount;
	TCHAR szPath[FILEPATH_MAX];

	// �E��E��E�X�E�gGet control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SASI_LIST);
	ASSERT(pListCtrl);

	// Get count
	nCount = pListCtrl->GetItemCount();

	// Get ID seleccionado
	nID = -1;
	for (i=0; i<nCount; i++) {
		if (pListCtrl->GetItemState(i, LVIS_SELECTED)) {
			nID = i;
			break;
		}
	}
	if (nID < 0) {
		return;
	}

	// Intentar abrir
	_tcscpy(szPath, m_szFile[nID]);
	if (!::FileOpenDlg(this, szPath, IDS_SASIOPEN)) {
		return;
	}

	// �E�p�E�X�E��E�Actualizacion
	_tcscpy(m_szFile[nID], szPath);

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	�E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
//
//---------------------------------------------------------------------------
void FASTCALL CSASIPage::UpdateList()
{
	CListCtrl *pListCtrl;
	int nCount;
	int i;
	CString strID;
	CString strDisk;
	CString strCtrl;
	DWORD dwDisk[SASI::SASIMax];

	// Get el numero actual del control de lista
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SASI_LIST);
	ASSERT(pListCtrl);
	nCount = pListCtrl->GetItemCount();

	// Si hay mas elementos en la lista, recortar la parte final
	while (nCount > m_nDrives) {
		pListCtrl->DeleteItem(nCount - 1);
		nCount--;
	}

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E��E��E��E��E��E��E�Ȃ��E��E��E��E��E�́AAgregar�E��E��E��E�
	while (m_nDrives > nCount) {
		strID.Format(_T("%d"), nCount + 1);
		pListCtrl->InsertItem(nCount, strID);
		nCount++;
	}

	// �E��E��E�f�E�BCheck(m_nDrive�E��E��E��E��E�܂Ƃ߂čs�E�Ȃ�)
	CheckSASI(dwDisk);

	// �E��E�rBucle
	for (i=0; i<nCount; i++) {
		// �E��E��E�f�E�BCheck�E�̌��E�ʂɂ��E�A�E��E��E��E��E��E�Create
		if (dwDisk[i] == 0) {
			// Desconocido
			strDisk = m_strError;
		}
		else {
			// MBMostrar
			strDisk.Format(_T("%uMB"), dwDisk[i]);
		}

		// Comparar y establecer
		strCtrl = pListCtrl->GetItemText(i, 1);
		if (strDisk != strCtrl) {
			pListCtrl->SetItemText(i, 1, strDisk);
		}

		// Nombre de archivo
		strDisk = m_szFile[i];
		strCtrl = pListCtrl->GetItemText(i, 2);
		if (strDisk != strCtrl) {
			pListCtrl->SetItemText(i, 2, strDisk);
		}
	}
}

//---------------------------------------------------------------------------
//
//	SASI�E�h�E��E��E�C�E�uCheck
//
//---------------------------------------------------------------------------
void FASTCALL CSASIPage::CheckSASI(DWORD *pDisk)
{
	int i;
	DWORD dwSize;
	Fileio fio;

	ASSERT(this);
	ASSERT(pDisk);

	// Bloqueo de VM
	::LockVM();

	// �E�h�E��E��E�C�E�uBucle
	for (i=0; i<m_nDrives; i++) {
		// Tama?o 0
		pDisk[i] = 0;

		// Intentar abrir
		if (!fio.Open(m_szFile[i], Fileio::ReadOnly)) {
			continue;
		}

		// Get size, cerrar
		dwSize = fio.GetFileSize();
		fio.Close();

		// �E�T�E�C�E�YCheck
		switch (dwSize) {
			case 0x9f5400:
				pDisk[i] = 10;
				break;

			// 20MB
			case 0x13c9800:
				pDisk[i] = 20;
				break;

			// 40MB
			case 0x2793000:
				pDisk[i] = 40;
				break;

			default:
				break;
		}
	}

	// Desbloquear
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	SASINumber of drives�E�擾
//
//---------------------------------------------------------------------------
int FASTCALL CSASIPage::GetDrives(const Config *pConfig) const
{
	ASSERT(this);
	ASSERT(pConfig);

	// Initialization�E��E��E��E�Ă��E�Ȃ��E��E�΁A�E�^�E��E��E��E�ꂽConfig�E��E��E��E�
	if (!m_bInit) {
		return pConfig->sasi_drives;
	}

	// Initialization�E�ς݂Ȃ�A�E��E��E�݂̒l�E��E�
	return m_nDrives;
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CSASIPage::EnableControls(BOOL bEnable, BOOL bDrive)
{
	CListCtrl *pListCtrl;
	CWnd *pWnd;

	ASSERT(this);

	// Control de lista (bEnable)
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SASI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->EnableWindow(bEnable);

	// Number of drives(bDrive)
	pWnd = GetDlgItem(IDC_SASI_DRIVEL);
	ASSERT(pWnd);
	pWnd->EnableWindow(bDrive);
	pWnd = GetDlgItem(IDC_SASI_DRIVEE);
	ASSERT(pWnd);
	pWnd->EnableWindow(bDrive);
	pWnd = GetDlgItem(IDC_SASI_DRIVES);
	ASSERT(pWnd);
	pWnd->EnableWindow(bDrive);
}

//===========================================================================
//
//	Page SxSI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSxSIPage::CSxSIPage()
{
	int i;

	// Always set ID and Help
	m_dwID = MAKEID('S', 'X', 'S', 'I');
	m_nTemplate = IDD_SXSIPAGE;
	m_uHelpID = IDC_SXSI_HELP;

	// Initialization(Otros�E�f�E�[�E�^)
	m_nSASIDrives = 0;
	for (i=0; i<8; i++) {
		m_DevMap[i] = DevNone;
	}
	ASSERT(SASI::SCSIMax == 6);
	for (i=0; i<SASI::SCSIMax; i++) {
		m_szFile[i][0] = _T('\0');
	}

	// �E��E�Initialization
	m_bInit = FALSE;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSxSIPage, CConfigPage)
	ON_WM_VSCROLL()
	ON_NOTIFY(NM_CLICK, IDC_SXSI_LIST, OnClick)
	ON_BN_CLICKED(IDC_SXSI_MOCHECK, OnCheck)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	�E�y�E�[�E�WInitialization
//
//---------------------------------------------------------------------------
BOOL CSxSIPage::OnInitDialog()
{
	int i;
	int nMax;
	int nDrives;
	CSASIPage *pSASIPage;
	CSpinButtonCtrl *pSpin;
	CButton *pButton;
	CListCtrl *pListCtrl;
	CDC *pDC;
	TEXTMETRIC tm;
	LONG cx;
	CString strCap;
	CString strFile;

	// Base class
	CConfigPage::OnInitDialog();

	// Initialization�E�t�E��E��E�OUp
	m_bInit = TRUE;

	// Page SASI�E�擾
	ASSERT(m_pSheet);
	pSASIPage = (CSASIPage*)m_pSheet->SearchPage(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(pSASIPage);

	// SASI�E�̐ݒ�Number of drives�E��E��E��E�ASCSI�E�ɐݒ�ł��E��E�ő�Number of drives�E�𓾂�
	m_nSASIDrives = pSASIPage->GetDrives(m_pConfig);
	nMax = m_nSASIDrives;
	nMax = (nMax + 1) >> 1;
	ASSERT((nMax >= 0) && (nMax <= 8));
	if (nMax >= 7) {
		nMax = 0;
	}
	else {
		nMax = 7 - nMax;
	}

	// SCSI�E�̍ő�Number of drives�E�𐧌�
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SXSI_DRIVES);
	pSpin->SetBase(10);
	nDrives = m_pConfig->sxsi_drives;
	if (nDrives > nMax) {
		nDrives = nMax;
	}
	pSpin->SetRange(0, (short)nMax);
	pSpin->SetPos(nDrives);

	// SCSI�E��E�Nombre de archivo�E��E��E�擾
	for (i=0; i<6; i++) {
		_tcscpy(m_szFile[i], m_pConfig->sxsi_file[i]);
	}

	// Configurar flag de prioridad MO
	pButton = (CButton*)GetDlgItem(IDC_SXSI_MOCHECK);
	if (m_pConfig->sxsi_mofirst) {
		pButton->SetCheck(1);
	}
	else {
		pButton->SetCheck(0);
	}

	// Get text metrics
	pDC = new CClientDC(this);
	::GetTextMetrics(pDC->m_hDC, &tm);
	delete pDC;
	cx = tm.tmAveCharWidth;

	// �E��E��E�X�E�gControl settings
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SXSI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->DeleteAllItems();
	if (::IsJapanese()) {
		pListCtrl->InsertColumn(0, _T("ID"), LVCFMT_LEFT, cx * 3, 0);
	}
	else {
		pListCtrl->InsertColumn(0, _T("ID"), LVCFMT_LEFT, cx * 4, 0);
	}
	::GetMsg(IDS_SXSI_CAPACITY, strCap);
	pListCtrl->InsertColumn(1, strCap, LVCFMT_CENTER,  cx * 7, 0);
	::GetMsg(IDS_SXSI_FILENAME, strFile);
	pListCtrl->InsertColumn(2, strFile, LVCFMT_LEFT, cx * 26, 0);

	// Full row select option for list control (COMCTL32.DLL v4.71+)
	pListCtrl->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

	// Get strings for the list control
	::GetMsg(IDS_SXSI_SASI, m_strSASI);
	::GetMsg(IDS_SXSI_MO, m_strMO);
	::GetMsg(IDS_SXSI_INIT, m_strInit);
	::GetMsg(IDS_SXSI_NONE, m_strNone);
	::GetMsg(IDS_SXSI_DEVERROR, m_strError);

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
	UpdateList();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Page activa
//
//---------------------------------------------------------------------------
BOOL CSxSIPage::OnSetActive()
{
	int nMax;
	int nPos;
	CSpinButtonCtrl *pSpin;
	BOOL bEnable;
	CSASIPage *pSASIPage;
	CSCSIPage *pSCSIPage;
	CAlterPage *pAlterPage;

	// Base class
	if (!CConfigPage::OnSetActive()) {
		return FALSE;
	}

	// Get page
	ASSERT(m_pSheet);
	pSASIPage = (CSASIPage*)m_pSheet->SearchPage(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(pSASIPage);
	pSCSIPage = (CSCSIPage*)m_pSheet->SearchPage(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(pSCSIPage);
	pAlterPage = (CAlterPage*)m_pSheet->SearchPage(MAKEID('A', 'L', 'T', ' '));
	ASSERT(pAlterPage);

	// Get dynamicamente el flag de habilitacion SxSI
	bEnable = TRUE;
	if (!pAlterPage->HasParity(m_pConfig)) {
		// Sin paridad configurada. SxSI no disponible
		bEnable = FALSE;
	}
	if (pSCSIPage->GetInterface(m_pConfig) != 0) {
		// Interfaz SCSI interna o externa. SxSI no disponible
		bEnable = FALSE;
	}

	// SASI�E��E�Number of drives�E��E��E�擾�E��E��E�ASCSI�E�̍ő�Number of drives�E�𓾂�
	m_nSASIDrives = pSASIPage->GetDrives(m_pConfig);
	nMax = m_nSASIDrives;
	nMax = (nMax + 1) >> 1;
	ASSERT((nMax >= 0) && (nMax <= 8));
	if (nMax >= 7) {
		nMax = 0;
	}
	else {
		nMax = 7 - nMax;
	}

	// SCSI�E�̍ő�Number of drives�E�𐧌�
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SXSI_DRIVES);
	ASSERT(pSpin);
	nPos = LOWORD(pSpin->GetPos());
	if (nPos > nMax) {
		nPos = nMax;
		pSpin->SetPos(nPos);
	}
	pSpin->SetRange(0, (short)nMax);

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
	UpdateList();

	// Controls enabled/disabled
	if (bEnable) {
		if (nPos > 0) {
			// Lista activa / Unit activa
			EnableControls(TRUE, TRUE);
		}
		else {
			// �E��E��E�X�E�g�E�L�E��E��E�E�E�h�E��E��E�C�E�u�E��E��E��E�
			EnableControls(FALSE, TRUE);
		}
	}
	else {
		// Lista inactiva / Unit inactiva
		EnableControls(FALSE, FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Desplazamiento vertical
//
//---------------------------------------------------------------------------
void CSxSIPage::OnVScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar* /*pBar*/)
{
	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion(�E��E��E��E��E��E�BuildMap�E��E��E�s�E��E�)
	UpdateList();

	// Controls enabled/disabled
	if (nPos > 0) {
		EnableControls(TRUE);
	}
	else {
		EnableControls(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Clic en control de lista
//
//---------------------------------------------------------------------------
void CSxSIPage::OnClick(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	CListCtrl *pListCtrl;
	int i;
	int nID;
	int nCount;
	int nDrive;
	TCHAR szPath[FILEPATH_MAX];

	// �E��E��E�X�E�gGet control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SXSI_LIST);
	ASSERT(pListCtrl);

	// Get count
	nCount = pListCtrl->GetItemCount();

	// Get ID seleccionado
	nID = -1;
	for (i=0; i<nCount; i++) {
		if (pListCtrl->GetItemState(i, LVIS_SELECTED)) {
			nID = i;
			break;
		}
	}
	if (nID < 0) {
		return;
	}

	// Identificar tipo segun el mapa
	if (m_DevMap[nID] != DevSCSI) {
		return;
	}

	// Get drive index desde ID (sin considerar MO)
	nDrive = 0;
	for (i=0; i<8; i++) {
		if (i == nID) {
			break;
		}
		if (m_DevMap[i] == DevSCSI) {
			nDrive++;
		}
	}
	ASSERT((nDrive >= 0) && (nDrive < SASI::SCSIMax));

	// Intentar abrir
	_tcscpy(szPath, m_szFile[nDrive]);
	if (!::FileOpenDlg(this, szPath, IDS_SCSIOPEN)) {
		return;
	}

	// �E�p�E�X�E��E�Actualizacion
	_tcscpy(m_szFile[nDrive], szPath);

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	Check�E�{�E�b�E�N�E�XModificacion
//
//---------------------------------------------------------------------------
void CSxSIPage::OnCheck()
{
	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion(�E��E��E��E��E��E�BuildMap�E��E��E�s�E��E�)
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CSxSIPage::OnOK()
{
	CSpinButtonCtrl *pSpin;
	CButton *pButton;
	int i;

	// Number of drives
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SXSI_DRIVES);
	ASSERT(pSpin);
	m_pConfig->sxsi_drives = LOWORD(pSpin->GetPos());

	// Flag de prioridad MO
	pButton = (CButton*)GetDlgItem(IDC_SXSI_MOCHECK);
	ASSERT(pButton);
	if (pButton->GetCheck() == 1) {
		m_pConfig->sxsi_mofirst = TRUE;
	}
	else {
		m_pConfig->sxsi_mofirst = FALSE;
	}

	// Nombre de archivo
	for (i=0; i<SASI::SCSIMax; i++) {
		_tcscpy(m_pConfig->sxsi_file[i], m_szFile[i]);
	}

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	�E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
//
//---------------------------------------------------------------------------
void FASTCALL CSxSIPage::UpdateList()
{
	int i;
	int nDrive;
	int nDev;
	int nCount;
	int nCap;
	CListCtrl *pListCtrl;
	CString strCtrl;
	CString strID;
	CString strSize;
	CString strFile;

	ASSERT(this);

	// Construir mapa
	BuildMap();

	// �E��E��E�X�E�gGet control�E�A�E�J�E�E�E��E��E�g�E�擾
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SXSI_LIST);
	ASSERT(pListCtrl);
	nCount = pListCtrl->GetItemCount();

	// Contar elementos que no son "None" en el mapa
	nDev = 0;
	for (i=0; i<8; i++) {
		if (m_DevMap[i] != DevNone) {
			nDev++;
		}
	}

	// Create items para nDev
	while (nCount > nDev) {
		pListCtrl->DeleteItem(nCount - 1);
		nCount--;
	}
	while (nDev > nCount) {
		strID.Format(_T("%d"), nCount + 1);
		pListCtrl->InsertItem(nCount, strID);
		nCount++;
	}

	// �E��E�rBucle
	nDrive = 0;
	nDev = 0;
	for (i=0; i<8; i++) {
		// Create cadena segun el tipo
		switch (m_DevMap[i]) {
			// Disco duro SASI
			case DevSASI:
				strSize = m_strNone;
				strFile = m_strSASI;
				break;

			// Disco duro SCSI
			case DevSCSI:
				nCap = CheckSCSI(nDrive);
				if (nCap > 0) {
					strSize.Format("%dMB", nCap);
				}
				else {
					strSize = m_strError;
				}
				strFile = m_szFile[nDrive];
				nDrive++;
				break;

			// Disco MO SCSI
			case DevMO:
				strSize = m_strNone;
				strFile = m_strMO;
				break;

			// Iniciador (Host)
			case DevInit:
				strSize = m_strNone;
				strFile = m_strInit;
				break;

			// Sin dispositivo
			case DevNone:
				// �E��E��E�ɐi�E��E�
				continue;

			// Other(�E��E��E�蓾�E�Ȃ�)
			default:
				ASSERT(FALSE);
				return;
		}

		// ID
		strID.Format(_T("%d"), i);
		strCtrl = pListCtrl->GetItemText(nDev, 0);
		if (strID != strCtrl) {
			pListCtrl->SetItemText(nDev, 0, strID);
		}

		// Capacity
		strCtrl = pListCtrl->GetItemText(nDev, 1);
		if (strSize != strCtrl) {
			pListCtrl->SetItemText(nDev, 1, strSize);
		}

		// Nombre de archivo
		strCtrl = pListCtrl->GetItemText(nDev, 2);
		if (strFile != strCtrl) {
			pListCtrl->SetItemText(nDev, 2, strFile);
		}

		// Siguiente
		nDev++;
	}
}

//---------------------------------------------------------------------------
//
//	�E�}�E�b�E�vCreate
//
//---------------------------------------------------------------------------
void FASTCALL CSxSIPage::BuildMap()
{
	int nSASI;
	int nMO;
	int nSCSI;
	int nInit;
	int nMax;
	int nID;
	int i;
	BOOL bMOFirst;
	CButton *pButton;
	CSpinButtonCtrl *pSpin;

	ASSERT(this);

	// Initialization
	nSASI = 0;
	nMO = 0;
	nSCSI = 0;
	nInit = 0;

	// Flag de prioridad MO�E��E��E�擾
	pButton = (CButton*)GetDlgItem(IDC_SXSI_MOCHECK);
	ASSERT(pButton);
	bMOFirst = FALSE;
	if (pButton->GetCheck() != 0) {
		bMOFirst = TRUE;
	}

	// SASINumber of drives�E��E��E��E�ASASI�E�̐�LID�E��E��E�𓾂�
	ASSERT((m_nSASIDrives >= 0) && (m_nSASIDrives <= 0x10));
	nSASI = m_nSASIDrives;
	nSASI = (nSASI + 1) >> 1;

	// Get maximo de MO, SCSI, INIT desde SASI
	if (nSASI <= 6) {
		nMO = 1;
		nSCSI = 6 - nSASI;
	}
	if (nSASI <= 7) {
		nInit = 1;
	}

	// SxSINumber of drives�E�̐ݒ��E��E��E��E�āA�E�l�E�𒲐�
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SXSI_DRIVES);
	ASSERT(pSpin);
	nMax = LOWORD(pSpin->GetPos());
	ASSERT((nMax >= 0) && (nMax <= (nSCSI + nMO)));
	if (nMax == 0) {
		// SxSINumber of drives�E��E�0
		nMO = 0;
		nSCSI = 0;
	}
	else {
		// Agrupar temporalmente HD+MO en nSCSI
		nSCSI = nMax;

		// Si es 1, solo MO
		if (nMax == 1) {
			nMO = 1;
			nSCSI = 0;
		}
		else {
			// Si es 2 o mas, asignar uno a MO
			nSCSI--;
			nMO = 1;
		}
	}

	// Resetr ID
	nID = 0;

	// �E�I�E�[�E��E�Limpiar
	for (i=0; i<8; i++) {
		m_DevMap[i] = DevNone;
	}

	// Set SASI
	for (i=0; i<nSASI; i++) {
		m_DevMap[nID] = DevSASI;
		nID++;
	}

	// Set SCSI, MO
	if (bMOFirst) {
		// Prioridad MO
		for (i=0; i<nMO; i++) {
			m_DevMap[nID] = DevMO;
			nID++;
		}
		for (i=0; i<nSCSI; i++) {
			m_DevMap[nID] = DevSCSI;
			nID++;
		}
	}
	else {
		// Prioridad HD
		for (i=0; i<nSCSI; i++) {
			m_DevMap[nID] = DevSCSI;
			nID++;
		}
		for (i=0; i<nMO; i++) {
			m_DevMap[nID] = DevMO;
			nID++;
		}
	}

	// Set iniciador
	for (i=0; i<nInit; i++) {
		ASSERT(nID <= 7);
		m_DevMap[7] = DevInit;
	}
}

//---------------------------------------------------------------------------
//
//	SCSI�E�n�E�[�E�h�E�f�E�B�E�X�E�NCapacityCheck
//	* Devuelve 0 en caso de error de dispositivo
//
//---------------------------------------------------------------------------
int FASTCALL CSxSIPage::CheckSCSI(int nDrive)
{
	Fileio fio;
	DWORD dwSize;

	ASSERT(this);
	ASSERT((nDrive >= 0) && (nDrive <= 5));

	// Bloquear
	::LockVM();

	// Abrir archivo
	if (!fio.Open(m_szFile[nDrive], Fileio::ReadOnly)) {
		// Error, devuelve 0
		fio.Close();
		::UnlockVM();
		return 0;
	}

	// Capacity�E�擾
	dwSize = fio.GetFileSize();

	// Desbloquear
	fio.Close();
	::UnlockVM();

	// �E�t�E�@�E�C�E��E��E�T�E�C�E�Y�E��E�Check(512�E�o�E�C�E�g�E�P�E��E�)
	if ((dwSize & 0x1ff) != 0) {
		return 0;
	}

	// �E�t�E�@�E�C�E��E��E�T�E�C�E�Y�E��E�Check(10MB�E�ȏ�)
	if (dwSize < 10 * 0x400 * 0x400) {
		return 0;
	}

	// �E�t�E�@�E�C�E��E��E�T�E�C�E�Y�E��E�Check(1016MB�E�ȉ�)
	if (dwSize > 1016 * 0x400 * 0x400) {
		return 0;
	}

	// Devolver size
	dwSize >>= 20;
	return dwSize;
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void CSxSIPage::EnableControls(BOOL bEnable, BOOL bDrive)
{
	int i;
	CWnd *pWnd;
	CListCtrl *pListCtrl;

	ASSERT(this);

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E��E�EMOCheck�E�ȊO�E�̑S�E�R�E��E��E�g�E��E��E�[�E��E��E��E�ݒ�
	for (i=0; ; i++) {
		// Get control
		if (!ControlTable[i]) {
			break;
		}
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);

		// �E�ݒ�
		pWnd->EnableWindow(bDrive);
	}

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E��E��E�ݒ�
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SXSI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->EnableWindow(bEnable);

	// MOCheck�E��E�ݒ�
	pWnd = GetDlgItem(IDC_SXSI_MOCHECK);
	ASSERT(pWnd);
	pWnd->EnableWindow(bEnable);
}

//---------------------------------------------------------------------------
//
//	Number of drives�E�擾
//
//---------------------------------------------------------------------------
int FASTCALL CSxSIPage::GetDrives(const Config *pConfig) const
{
	BOOL bEnable;
	CSASIPage *pSASIPage;
	CSCSIPage *pSCSIPage;
	CAlterPage *pAlterPage;
	CSpinButtonCtrl *pSpin;
	int nPos;

	ASSERT(this);
	ASSERT(pConfig);

	// Get page
	ASSERT(m_pSheet);
	pSASIPage = (CSASIPage*)m_pSheet->SearchPage(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(pSASIPage);
	pSCSIPage = (CSCSIPage*)m_pSheet->SearchPage(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(pSCSIPage);
	pAlterPage = (CAlterPage*)m_pSheet->SearchPage(MAKEID('A', 'L', 'T', ' '));
	ASSERT(pAlterPage);

	// Get dynamicamente el flag de habilitacion SxSI
	bEnable = TRUE;
	if (!pAlterPage->HasParity(pConfig)) {
		// Sin paridad configurada. SxSI no disponible
		bEnable = FALSE;
	}
	if (pSCSIPage->GetInterface(pConfig) != 0) {
		// Interfaz SCSI interna o externa. SxSI no disponible
		bEnable = FALSE;
	}
	if (pSASIPage->GetDrives(pConfig) >= 12) {
		// SASINumber of drives�E��E��E��E��E��E��E��E��E��E�BSxSI�E�͎g�E�p�E�ł��E�Ȃ�
		bEnable = FALSE;
	}

	// Si no esta disponible, es 0
	if (!bEnable) {
		return 0;
	}

	// �E��E�Initialization�E�̏ꍇ�E�A�E�ݒ�l�E��E�Ԃ�
	if (!m_bInit) {
		return pConfig->sxsi_drives;
	}

	// �E��E��E��E�Editar�E��E��E�̒l�E��E�Ԃ�
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SXSI_DRIVES);
	ASSERT(pSpin);
	nPos = LOWORD(pSpin->GetPos());
	return nPos;
}

//---------------------------------------------------------------------------
//
//	Tabla de controles
//
//---------------------------------------------------------------------------
const UINT CSxSIPage::ControlTable[] = {
	IDC_SXSI_GROUP,
	IDC_SXSI_DRIVEL,
	IDC_SXSI_DRIVEE,
	IDC_SXSI_DRIVES,
	NULL
};

//===========================================================================
//
//	Page SCSI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSCSIPage::CSCSIPage()
{
	int i;

	// Always set ID and Help
	m_dwID = MAKEID('S', 'C', 'S', 'I');
	m_nTemplate = IDD_SCSIPAGE;
	m_uHelpID = IDC_SCSI_HELP;

	// Get SCSI
	m_pSCSI = (SCSI*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(m_pSCSI);

	// Initialization(Otros�E�f�E�[�E�^)
	m_bInit = FALSE;
	m_nDrives = 0;
	m_bMOFirst = FALSE;

	// Mapa de dispositivos
	ASSERT(SCSI::DeviceMax == 8);
	for (i=0; i<SCSI::DeviceMax; i++) {
		m_DevMap[i] = DevNone;
	}

	// Rutas de archivos
	ASSERT(SCSI::HDMax == 5);
	for (i=0; i<SCSI::HDMax; i++) {
		m_szFile[i][0] = _T('\0');
	}
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSCSIPage, CConfigPage)
	ON_WM_VSCROLL()
	ON_NOTIFY(NM_CLICK, IDC_SCSI_LIST, OnClick)
	ON_BN_CLICKED(IDC_SCSI_NONEB, OnButton)
	ON_BN_CLICKED(IDC_SCSI_INTB, OnButton)
	ON_BN_CLICKED(IDC_SCSI_EXTB, OnButton)
	ON_BN_CLICKED(IDC_SCSI_MOCHECK, OnCheck)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	�E�y�E�[�E�WInitialization
//
//---------------------------------------------------------------------------
BOOL CSCSIPage::OnInitDialog()
{
	int i;
	BOOL bAvail;
	BOOL bEnable[2];
	CButton *pButton;
	CSpinButtonCtrl *pSpin;
	CDC *pDC;
	TEXTMETRIC tm;
	LONG cx;
	CListCtrl *pListCtrl;
	CString strCap;
	CString strFile;

	// Base class
	CConfigPage::OnInitDialog();

	// Initialization�E�t�E��E��E�OUp
	m_bInit = TRUE;

	// ROM�E�̗L�E��E��E�ɉ��E��E��E�āA�E�C�E��E��E�^�E�t�E�F�E�[�E�X�E��E��E�W�E�IButtons�E��E��E�֎~
	pButton = (CButton*)GetDlgItem(IDC_SCSI_EXTB);
	ASSERT(pButton);
	bEnable[0] = CheckROM(1);
	pButton->EnableWindow(bEnable[0]);
	pButton = (CButton*)GetDlgItem(IDC_SCSI_INTB);
	ASSERT(pButton);
	bEnable[1] = CheckROM(2);
	pButton->EnableWindow(bEnable[1]);

	// Tipo de interfaz
	pButton = (CButton*)GetDlgItem(IDC_SCSI_NONEB);
	bAvail = FALSE;
	switch (m_pConfig->mem_type) {
		// No instalar
		case Memory::None:
		case Memory::SASI:
			break;

		// Externo
		case Memory::SCSIExt:
			// ExternoROM�E��E��E��E��E�݂��E��E�ꍁE��̂�
			if (bEnable[0]) {
				pButton = (CButton*)GetDlgItem(IDC_SCSI_EXTB);
				bAvail = TRUE;
			}
			break;

		// Other(Interno)
		default:
			// InternoROM�E��E��E��E��E�݂��E��E�ꍁE��̂�
			if (bEnable[1]) {
				pButton = (CButton*)GetDlgItem(IDC_SCSI_INTB);
				bAvail = TRUE;
			}
			break;
	}
	ASSERT(pButton);
	pButton->SetCheck(1);

	// Number of drives
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SCSI_DRIVES);
	pSpin->SetBase(10);
	pSpin->SetRange(0, 7);
	m_nDrives = m_pConfig->scsi_drives;
	ASSERT((m_nDrives >= 0) && (m_nDrives <= 7));
	pSpin->SetPos(m_nDrives);

	// Flag de prioridad MO
	pButton = (CButton*)GetDlgItem(IDC_SCSI_MOCHECK);
	ASSERT(pButton);
	if (m_pConfig->scsi_mofirst) {
		pButton->SetCheck(1);
		m_bMOFirst = TRUE;
	}
	else {
		pButton->SetCheck(0);
		m_bMOFirst = FALSE;
	}

	// SCSI-HDRutas de archivos
	for (i=0; i<SCSI::HDMax; i++) {
		_tcscpy(m_szFile[i], m_pConfig->scsi_file[i]);
	}

	// Get text metrics
	pDC = new CClientDC(this);
	::GetTextMetrics(pDC->m_hDC, &tm);
	delete pDC;
	cx = tm.tmAveCharWidth;

	// �E��E��E�X�E�gControl settings
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SCSI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->DeleteAllItems();
	if (::IsJapanese()) {
		pListCtrl->InsertColumn(0, _T("ID"), LVCFMT_LEFT, cx * 3, 0);
	}
	else {
		pListCtrl->InsertColumn(0, _T("ID"), LVCFMT_LEFT, cx * 4, 0);
	}
	::GetMsg(IDS_SCSI_CAPACITY, strCap);
	pListCtrl->InsertColumn(1, strCap, LVCFMT_CENTER,  cx * 7, 0);
	::GetMsg(IDS_SCSI_FILENAME, strFile);
	pListCtrl->InsertColumn(2, strFile, LVCFMT_LEFT, cx * 26, 0);

	// Full row select option for list control (COMCTL32.DLL v4.71+)
	pListCtrl->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

	// Get strings for the list control
	::GetMsg(IDS_SCSI_MO, m_strMO);
	::GetMsg(IDS_SCSI_CD, m_strCD);
	::GetMsg(IDS_SCSI_INIT, m_strInit);
	::GetMsg(IDS_SCSI_NONE, m_strNone);
	::GetMsg(IDS_SCSI_DEVERROR, m_strError);

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion(�E��E��E��E��E��E�BuildMap�E��E��E�s�E��E�)
	UpdateList();

	// Controls enabled/disabled
	if (bAvail) {
		if (m_nDrives > 0) {
			// Lista activa / Unit activa
			EnableControls(TRUE, TRUE);
		}
		else {
			// Lista inactiva / Unit activa
			EnableControls(FALSE, TRUE);
		}
	}
	else {
		// Lista inactiva / Unit inactiva
		EnableControls(FALSE, FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CSCSIPage::OnOK()
{
	int i;

	// Tipo de interfaz�E��E��E�烁E���E��E��E��E��E�ʐݒ�
	switch (GetIfCtrl()) {
		// No instalar
		case 0:
			m_pConfig->mem_type = Memory::SASI;
			break;

		// Externo
		case 1:
			m_pConfig->mem_type = Memory::SCSIExt;
			break;

		// Interno
		case 2:
			// �E�^�E�C�E�v�E��E��E�Ⴄ�E�ꍁE��̂݁ASCSIInt�E��E�Modificacion
			if ((m_pConfig->mem_type == Memory::SASI) || (m_pConfig->mem_type == Memory::SCSIExt)) {
				m_pConfig->mem_type = Memory::SCSIInt;
			}
			break;

		// Other(�E��E��E�肦�E�Ȃ�)
		default:
			ASSERT(FALSE);
	}

	// Number of drives
	m_pConfig->scsi_drives = m_nDrives;

	// Flag de prioridad MO
	m_pConfig->scsi_mofirst = m_bMOFirst;

	// SCSI-HDRutas de archivos
	for (i=0; i<SCSI::HDMax; i++) {
		_tcscpy(m_pConfig->scsi_file[i], m_szFile[i]);
	}

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Desplazamiento vertical
//
//---------------------------------------------------------------------------
void CSCSIPage::OnVScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar* /*pBar*/)
{
	// Number of drives�E�擾
	m_nDrives = nPos;

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion(�E��E��E��E��E��E�BuildMap�E��E��E�s�E��E�)
	UpdateList();

	// Controls enabled/disabled
	if (nPos > 0) {
		EnableControls(TRUE);
	}
	else {
		EnableControls(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Clic en control de lista
//
//---------------------------------------------------------------------------
void CSCSIPage::OnClick(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	CListCtrl *pListCtrl;
	int i;
	int nID;
	int nCount;
	int nDrive;
	TCHAR szPath[FILEPATH_MAX];

	// �E��E��E�X�E�gGet control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SCSI_LIST);
	ASSERT(pListCtrl);

	// Get count
	nCount = pListCtrl->GetItemCount();

	// Get selected items
	nID = -1;
	for (i=0; i<nCount; i++) {
		if (pListCtrl->GetItemState(i, LVIS_SELECTED)) {
			nID = i;
			break;
		}
	}
	if (nID < 0) {
		return;
	}

	// Get ID de los datos del item
	nID = (int)pListCtrl->GetItemData(nID);

	// Identificar tipo segun el mapa
	if (m_DevMap[nID] != DevSCSI) {
		return;
	}

	// Get drive index desde ID (sin considerar MO)
	nDrive = 0;
	for (i=0; i<SCSI::DeviceMax; i++) {
		if (i == nID) {
			break;
		}
		if (m_DevMap[i] == DevSCSI) {
			nDrive++;
		}
	}
	ASSERT((nDrive >= 0) && (nDrive < SCSI::HDMax));

	// Intentar abrir
	_tcscpy(szPath, m_szFile[nDrive]);
	if (!::FileOpenDlg(this, szPath, IDS_SCSIOPEN)) {
		return;
	}

	// �E�p�E�X�E��E�Actualizacion
	_tcscpy(m_szFile[nDrive], szPath);

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	�E��E��E�W�E�IButtonsModificacion
//
//---------------------------------------------------------------------------
void CSCSIPage::OnButton()
{
	CButton *pButton;

	// �E�C�E��E��E�^�E�t�E�F�E�[�E�X�E��E��E��E��E��E�Check�E��E��E��E�Ă��E�邩
	pButton = (CButton*)GetDlgItem(IDC_SCSI_NONEB);
	ASSERT(pButton);
	if (pButton->GetCheck() != 0) {
		// Lista inactiva / Unit inactiva
		EnableControls(FALSE, FALSE);
		return;
	}

	if (m_nDrives > 0) {
		// Lista activa / Unit activa
		EnableControls(TRUE, TRUE);
	}
	else {
		// Lista inactiva / Unit activa
		EnableControls(FALSE, TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Check�E�{�E�b�E�N�E�XModificacion
//
//---------------------------------------------------------------------------
void CSCSIPage::OnCheck()
{
	CButton *pButton;

	// Get estado actual
	pButton = (CButton*)GetDlgItem(IDC_SCSI_MOCHECK);
	ASSERT(pButton);
	if (pButton->GetCheck() != 0) {
		m_bMOFirst = TRUE;
	}
	else {
		m_bMOFirst = FALSE;
	}

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion(�E��E��E��E��E��E�BuildMap�E��E��E�s�E��E�)
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	Tipo de interfaz�E�擾
//
//---------------------------------------------------------------------------
int FASTCALL CSCSIPage::GetInterface(const Config *pConfig) const
{
	ASSERT(this);
	ASSERT(pConfig);

	// Initialization�E�t�E��E��E�O
	if (!m_bInit) {
		// Initialization�E��E��E��E�Ă��E�Ȃ��E�̂ŁAConfig�E��E��E��E�擾
		switch (pConfig->mem_type) {
			// No instalar
			case Memory::None:
			case Memory::SASI:
				return 0;

			// Externo
			case Memory::SCSIExt:
				return 1;

			// Other(Interno)
			default:
				return 2;
		}
	}

	// Initialization�E��E��E��E�Ă��E��E�̂ŁA�E�R�E��E��E�g�E��E��E�[�E��E��E��E��E��E�擾
	return GetIfCtrl();
}

//---------------------------------------------------------------------------
//
//	Tipo de interfaz�E�擾(�E�R�E��E��E�g�E��E��E�[�E��E��E��E��E�)
//
//---------------------------------------------------------------------------
int FASTCALL CSCSIPage::GetIfCtrl() const
{
	CButton *pButton;

	ASSERT(this);

	// No instalar
	pButton = (CButton*)GetDlgItem(IDC_SCSI_NONEB);
	ASSERT(pButton);
	if (pButton->GetCheck() != 0) {
		return 0;
	}

	// Externo
	pButton = (CButton*)GetDlgItem(IDC_SCSI_EXTB);
	ASSERT(pButton);
	if (pButton->GetCheck() != 0) {
		return 1;
	}

	// Interno
	pButton = (CButton*)GetDlgItem(IDC_SCSI_INTB);
	ASSERT(pButton);
	ASSERT(pButton->GetCheck() != 0);
	return 2;
}

//---------------------------------------------------------------------------
//
//	ROMCheck
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSCSIPage::CheckROM(int nType) const
{
	Filepath path;
	Fileio fio;
	DWORD dwSize;

	ASSERT(this);
	ASSERT((nType >= 0) && (nType <= 2));

	// 0:Interno�E�̏ꍇ�E�͖��E��E��E��E��E��E�OK
	if (nType == 0) {
		return TRUE;
	}

	// Rutas de archivosCreate
	if (nType == 1) {
		// Externo
		path.SysFile(Filepath::SCSIExt);
	}
	else {
		// Interno
		path.SysFile(Filepath::SCSIInt);
	}

	// Bloquear
	::LockVM();

	// Intentar abrir
	if (!fio.Open(path, Fileio::ReadOnly)) {
		::UnlockVM();
		return FALSE;
	}

	// �E�t�E�@�E�C�E��E��E�T�E�C�E�Y�E�擾
	dwSize = fio.GetFileSize();
	fio.Close();
	::UnlockVM();

	if (nType == 1) {
		// Externo�E�́A0x2000�E�o�E�C�E�g�E�܂��E��E�0x1fe0�E�o�E�C�E�g(WinX68k�E��E��E��E��E�łƌ݊��E��E��E�Ƃ�)
		if ((dwSize == 0x2000) || (dwSize == 0x1fe0)) {
			return TRUE;
		}
	}
	else {
		// Interno�E�́A0x2000�E�o�E�C�E�g�E�̂�
		if (dwSize == 0x2000) {
			return TRUE;
		}
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	�E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
//
//---------------------------------------------------------------------------
void FASTCALL CSCSIPage::UpdateList()
{
	int i;
	int nDrive;
	int nDev;
	int nCount;
	int nCap;
	CListCtrl *pListCtrl;
	CString strCtrl;
	CString strID;
	CString strSize;
	CString strFile;

	ASSERT(this);

	// Construir mapa
	BuildMap();

	// �E��E��E�X�E�gGet control�E�A�E�J�E�E�E��E��E�g�E�擾
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SCSI_LIST);
	ASSERT(pListCtrl);
	nCount = pListCtrl->GetItemCount();

	// Contar elementos que no son "None" en el mapa
	nDev = 0;
	for (i=0; i<8; i++) {
		if (m_DevMap[i] != DevNone) {
			nDev++;
		}
	}

	// Create items para nDev
	while (nCount > nDev) {
		pListCtrl->DeleteItem(nCount - 1);
		nCount--;
	}
	while (nDev > nCount) {
		strID.Format(_T("%d"), nCount + 1);
		pListCtrl->InsertItem(nCount, strID);
		nCount++;
	}

	// �E��E�rBucle
	nDrive = 0;
	nDev = 0;
	for (i=0; i<SCSI::DeviceMax; i++) {
		// Create cadena segun el tipo
		switch (m_DevMap[i]) {
			// Disco duro SCSI
			case DevSCSI:
				nCap = CheckSCSI(nDrive);
				if (nCap > 0) {
					strSize.Format("%dMB", nCap);
				}
				else {
					strSize = m_strError;
				}
				strFile = m_szFile[nDrive];
				nDrive++;
				break;

			// Disco MO SCSI
			case DevMO:
				strSize = m_strNone;
				strFile = m_strMO;
				break;

			// CD-ROM SCSI
			case DevCD:
				strSize = m_strNone;
				strFile = m_strCD;
				break;

			// Iniciador (Host)
			case DevInit:
				strSize = m_strNone;
				strFile = m_strInit;
				break;

			// Sin dispositivo
			case DevNone:
				// �E��E��E�ɐi�E��E�
				continue;

			// Other(�E��E��E�蓾�E�Ȃ�)
			default:
				ASSERT(FALSE);
				return;
		}

		// �E�A�E�C�E�e�E��E��E�f�E�[�E�^
		if ((int)pListCtrl->GetItemData(nDev) != i) {
			pListCtrl->SetItemData(nDev, (DWORD)i);
		}

		// ID
		strID.Format(_T("%d"), i);
		strCtrl = pListCtrl->GetItemText(nDev, 0);
		if (strID != strCtrl) {
			pListCtrl->SetItemText(nDev, 0, strID);
		}

		// Capacity
		strCtrl = pListCtrl->GetItemText(nDev, 1);
		if (strSize != strCtrl) {
			pListCtrl->SetItemText(nDev, 1, strSize);
		}

		// Nombre de archivo
		strCtrl = pListCtrl->GetItemText(nDev, 2);
		if (strFile != strCtrl) {
			pListCtrl->SetItemText(nDev, 2, strFile);
		}

		// Siguiente
		nDev++;
	}
}

//---------------------------------------------------------------------------
//
//	�E�}�E�b�E�vCreate
//
//---------------------------------------------------------------------------
void FASTCALL CSCSIPage::BuildMap()
{
	int i;
	int nID;
	int nInit;
	int nHD;
	BOOL bMO;
	BOOL bCD;

	ASSERT(this);

	// Initialization
	nHD = 0;
	bMO = FALSE;
	bCD = FALSE;

	// �E�f�E�B�E�X�E�N�E��E��E��E�Aceptar
	switch (m_nDrives) {
		// 0�E��E�
		case 0:
			break;

		// 1�E��E�
		case 1:
			// Prioridad MO�E��E��E�APrioridad HD�E��E��E�ŕ��E��E��E��E�
			if (m_bMOFirst) {
				bMO = TRUE;
			}
			else {
				nHD = 1;
			}
			break;

		// 2�E��E�
		case 2:
			// Una unidad HD y una MO
			nHD = 1;
			bMO = TRUE;
			break;

		// 3�E��E�
		case 3:
			// Una unidad HD, una MO y una CD
			nHD = 1;
			bMO = TRUE;
			bCD = TRUE;
			break;

		// 4 o mas unidades
		default:
			ASSERT(m_nDrives <= 7);
			nHD= m_nDrives - 2;
			bMO = TRUE;
			bCD = TRUE;
			break;
	}

	// �E�I�E�[�E��E�Limpiar
	for (i=0; i<8; i++) {
		m_DevMap[i] = DevNone;
	}

	// Configurar iniciador primero
	ASSERT(m_pSCSI);
	nInit = m_pSCSI->GetSCSIID();
	ASSERT((nInit >= 0) && (nInit <= 7));
	m_DevMap[nInit] = DevInit;

	// MO configuration (solo si flag de prioridad activo)
	if (bMO && m_bMOFirst) {
		for (nID=0; nID<SCSI::DeviceMax; nID++) {
			if (m_DevMap[nID] == DevNone) {
				m_DevMap[nID] = DevMO;
				bMO = FALSE;
				break;
			}
		}
	}

	// HD configuration
	for (i=0; i<nHD; i++) {
		for (nID=0; nID<SCSI::DeviceMax; nID++) {
			if (m_DevMap[nID] == DevNone) {
				m_DevMap[nID] = DevSCSI;
				break;
			}
		}
	}

	// MO configuration
	if (bMO) {
		for (nID=0; nID<SCSI::DeviceMax; nID++) {
			if (m_DevMap[nID] == DevNone) {
				m_DevMap[nID] = DevMO;
				break;
			}
		}
	}

	// CD configuration (fijo ID=6, o 7 si esta en uso)
	if (bCD) {
		if (m_DevMap[6] == DevNone) {
			m_DevMap[6] = DevCD;
		}
		else {
			ASSERT(m_DevMap[7] == DevNone);
			m_DevMap[7] = DevCD;
		}
	}
}

//---------------------------------------------------------------------------
//
//	SCSI�E�n�E�[�E�h�E�f�E�B�E�X�E�NCapacityCheck
//	* Devuelve 0 en caso de error de dispositivo
//
//---------------------------------------------------------------------------
int FASTCALL CSCSIPage::CheckSCSI(int nDrive)
{
	Fileio fio;
	DWORD dwSize;

	ASSERT(this);
	ASSERT((nDrive >= 0) && (nDrive <= SCSI::HDMax));

	// Bloquear
	::LockVM();

	// Abrir archivo
	if (!fio.Open(m_szFile[nDrive], Fileio::ReadOnly)) {
		// Error, devuelve 0
		fio.Close();
		::UnlockVM();
		return 0;
	}

	// Capacity�E�擾
	dwSize = fio.GetFileSize();

	// Desbloquear
	fio.Close();
	::UnlockVM();

	// �E�t�E�@�E�C�E��E��E�T�E�C�E�Y�E��E�Check(512�E�o�E�C�E�g�E�P�E��E�)
	if ((dwSize & 0x1ff) != 0) {
		return 0;
	}

	// �E�t�E�@�E�C�E��E��E�T�E�C�E�Y�E��E�Check(10MB�E�ȏ�)
	if (dwSize < 10 * 0x400 * 0x400) {
		return 0;
	}

	// �E�t�E�@�E�C�E��E��E�T�E�C�E�Y�E��E�Check(4095MB�E�ȉ�)
	if (dwSize > 0xfff00000) {
		return 0;
	}

	// Devolver size
	dwSize >>= 20;
	return dwSize;
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CSCSIPage::EnableControls(BOOL bEnable, BOOL bDrive)
{
	int i;
	CWnd *pWnd;
	CListCtrl *pListCtrl;

	ASSERT(this);

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E��E�EMOCheck�E�ȊO�E�̑S�E�R�E��E��E�g�E��E��E�[�E��E��E��E�ݒ�
	for (i=0; ; i++) {
		// Get control
		if (!ControlTable[i]) {
			break;
		}
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);

		// �E�ݒ�
		pWnd->EnableWindow(bDrive);
	}

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E��E��E�ݒ�
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SCSI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->EnableWindow(bEnable);

	// MOCheck�E��E�ݒ�
	pWnd = GetDlgItem(IDC_SCSI_MOCHECK);
	ASSERT(pWnd);
	pWnd->EnableWindow(bEnable);
}

//---------------------------------------------------------------------------
//
//	Tabla de controles
//
//---------------------------------------------------------------------------
const UINT CSCSIPage::ControlTable[] = {
	IDC_SCSI_GROUP,
	IDC_SCSI_DRIVEL,
	IDC_SCSI_DRIVEE,
	IDC_SCSI_DRIVES,
	NULL
};

//===========================================================================
//
//	Page de puertos
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CPortPage::CPortPage()
{
	// Always set ID and Help
	m_dwID = MAKEID('P', 'O', 'R', 'T');
	m_nTemplate = IDD_PORTPAGE;
	m_uHelpID = IDC_PORT_HELP;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CPortPage::OnInitDialog()
{
	int i;
	CComboBox *pComboBox;
	CString strText;
	CButton *pButton;
	CEdit *pEdit;

	// Base class
	CConfigPage::OnInitDialog();

	// COMCuadro combinado
	pComboBox = (CComboBox*)GetDlgItem(IDC_PORT_COMC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_PORT_NOASSIGN, strText);
	pComboBox->AddString(strText);
	for (i=1; i<=9; i++) {
		strText.Format(_T("COM%d"), i);
		pComboBox->AddString(strText);
	}
	pComboBox->SetCurSel(m_pConfig->port_com);

	// Log de recepcion
	pEdit = (CEdit*)GetDlgItem(IDC_PORT_RECVE);
	ASSERT(pEdit);
	pEdit->SetWindowText(m_pConfig->port_recvlog);

	// Forzar 38400bps
	pButton = (CButton*)GetDlgItem(IDC_PORT_BAUDRATE);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->port_384);

	// LPTCuadro combinado
	pComboBox = (CComboBox*)GetDlgItem(IDC_PORT_LPTC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_PORT_NOASSIGN, strText);
	pComboBox->AddString(strText);
	for (i=1; i<=9; i++) {
		strText.Format(_T("LPT%d"), i);
		pComboBox->AddString(strText);
	}
	pComboBox->SetCurSel(m_pConfig->port_lpt);

	// Log de envio
	pEdit = (CEdit*)GetDlgItem(IDC_PORT_SENDE);
	ASSERT(pEdit);
	pEdit->SetWindowText(m_pConfig->port_sendlog);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CPortPage::OnOK()
{
	CComboBox *pComboBox;
	CEdit *pEdit;
	CButton *pButton;

	// COMCuadro combinado
	pComboBox = (CComboBox*)GetDlgItem(IDC_PORT_COMC);
	ASSERT(pComboBox);
	m_pConfig->port_com = pComboBox->GetCurSel();

	// Log de recepcion
	pEdit = (CEdit*)GetDlgItem(IDC_PORT_RECVE);
	ASSERT(pEdit);
	pEdit->GetWindowText(m_pConfig->port_recvlog, sizeof(m_pConfig->port_recvlog));

	// Forzar 38400bps
	pButton = (CButton*)GetDlgItem(IDC_PORT_BAUDRATE);
	ASSERT(pButton);
	m_pConfig->port_384 = pButton->GetCheck();

	// LPTCuadro combinado
	pComboBox = (CComboBox*)GetDlgItem(IDC_PORT_LPTC);
	ASSERT(pComboBox);
	m_pConfig->port_lpt = pComboBox->GetCurSel();

	// Log de envio
	pEdit = (CEdit*)GetDlgItem(IDC_PORT_SENDE);
	ASSERT(pEdit);
	pEdit->GetWindowText(m_pConfig->port_sendlog, sizeof(m_pConfig->port_sendlog));

	// Base class
	CConfigPage::OnOK();
}

//===========================================================================
//
//	Page MIDI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CMIDIPage::CMIDIPage()
{
	// Always set ID and Help
	m_dwID = MAKEID('M', 'I', 'D', 'I');
	m_nTemplate = IDD_MIDIPAGE;
	m_uHelpID = IDC_MIDI_HELP;

	// Objects
	m_pMIDI = NULL;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CMIDIPage, CConfigPage)
	ON_WM_VSCROLL()
	ON_BN_CLICKED(IDC_MIDI_BID0, OnBIDClick)
	ON_BN_CLICKED(IDC_MIDI_BID1, OnBIDClick)
	ON_BN_CLICKED(IDC_MIDI_BID2, OnBIDClick)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CMIDIPage::OnInitDialog()
{
	CButton *pButton;
	CComboBox *pComboBox;
	CSpinButtonCtrl *pSpin;
	CFrmWnd *pFrmWnd;
	CString strDesc;
	int nNum;
	int i;

	// Base class
	CConfigPage::OnInitDialog();

	// Get componente MIDI
	pFrmWnd = ResolveFrmWnd(this);
	if (!pFrmWnd) {
		return FALSE;
	}
	m_pMIDI = pFrmWnd->GetMIDI();
	ASSERT(m_pMIDI);

	// Controls enabled/disabled
	m_bEnableCtrl = TRUE;
	EnableControls(FALSE);
	if (m_pConfig->midi_bid != 0) {
		EnableControls(TRUE);
	}

	// �E�{�E�[�E�hID
	pButton = (CButton*)GetDlgItem(IDC_MIDI_BID0 + m_pConfig->midi_bid);
	ASSERT(pButton);
	pButton->SetCheck(1);

	// Nivel de interrupcion
	pButton = (CButton*)GetDlgItem(IDC_MIDI_ILEVEL4 + m_pConfig->midi_ilevel);
	ASSERT(pButton);
	pButton->SetCheck(1);

	// Reinicio de sintetizador
	pButton = (CButton*)GetDlgItem(IDC_MIDI_RSTGM + m_pConfig->midi_reset);
	ASSERT(pButton);
	pButton->SetCheck(1);

	// Dispositivo (ENTRADA)
	pComboBox = (CComboBox*)GetDlgItem(IDC_MIDI_INC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_MIDI_NOASSIGN, strDesc);
	pComboBox->AddString(strDesc);
	nNum = (int)m_pMIDI->GetInDevs();
	for (i=0; i<nNum; i++) {
		m_pMIDI->GetInDevDesc(i, strDesc);
		pComboBox->AddString(strDesc);
	}

	// Cuadro combinado�E��E�Cursor�E��E�ݒ�
	if (m_pConfig->midiin_device <= nNum) {
		pComboBox->SetCurSel(m_pConfig->midiin_device);
	}
	else {
		pComboBox->SetCurSel(0);
	}

	// Dispositivo (SALIDA)
	pComboBox = (CComboBox*)GetDlgItem(IDC_MIDI_OUTC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_MIDI_NOASSIGN, strDesc);
	pComboBox->AddString(strDesc);
	nNum = (int)m_pMIDI->GetOutDevs();
	if (nNum >= 1) {
		::GetMsg(IDS_MIDI_MAPPER, strDesc);
		pComboBox->AddString(strDesc);
		for (i=0; i<nNum; i++) {
			m_pMIDI->GetOutDevDesc(i, strDesc);
			pComboBox->AddString(strDesc);
		}
	}

	// Cuadro combinado�E��E�Cursor�E��E�ݒ�
	if (m_pConfig->midiout_device < (nNum + 2)) {
		pComboBox->SetCurSel(m_pConfig->midiout_device);
	}
	else {
		pComboBox->SetCurSel(0);
	}

	// Retraso (ENTRADA)
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYIS);
	ASSERT(pSpin);
	pSpin->SetBase(10);
	pSpin->SetRange(0, 200);
	pSpin->SetPos(m_pConfig->midiin_delay);

	// Retraso (SALIDA)
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYOS);
	ASSERT(pSpin);
	pSpin->SetBase(10);
	pSpin->SetRange(20, 200);
	pSpin->SetPos(m_pConfig->midiout_delay);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CMIDIPage::OnOK()
{
	int i;
	CButton *pButton;
	CComboBox *pComboBox;
	CSpinButtonCtrl *pSpin;

	// �E�{�E�[�E�hID
	for (i=0; i<3; i++) {
		pButton = (CButton*)GetDlgItem(IDC_MIDI_BID0 + i);
		ASSERT(pButton);
		if (pButton->GetCheck() == 1) {
			m_pConfig->midi_bid = i;
			break;
		}
	}

	// Nivel de interrupcion
	for (i=0; i<2; i++) {
		pButton = (CButton*)GetDlgItem(IDC_MIDI_ILEVEL4 + i);
		ASSERT(pButton);
		if (pButton->GetCheck() == 1) {
			m_pConfig->midi_ilevel = i;
			break;
		}
	}

	// Reinicio de sintetizador
	for (i=0; i<4; i++) {
		pButton = (CButton*)GetDlgItem(IDC_MIDI_RSTGM + i);
		ASSERT(pButton);
		if (pButton->GetCheck() == 1) {
			m_pConfig->midi_reset = i;
			break;
		}
	}

	// Dispositivo (ENTRADA)
	pComboBox = (CComboBox*)GetDlgItem(IDC_MIDI_INC);
	ASSERT(pComboBox);
	m_pConfig->midiin_device = pComboBox->GetCurSel();

	// Dispositivo (SALIDA)
	pComboBox = (CComboBox*)GetDlgItem(IDC_MIDI_OUTC);
	ASSERT(pComboBox);
	m_pConfig->midiout_device = pComboBox->GetCurSel();

	// Retraso (ENTRADA)
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYIS);
	ASSERT(pSpin);
	m_pConfig->midiin_delay = LOWORD(pSpin->GetPos());

	// Retraso (SALIDA)
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYOS);
	ASSERT(pSpin);
	m_pConfig->midiout_delay = LOWORD(pSpin->GetPos());

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CMIDIPage::OnCancel()
{
	// Restaurar retraso MIDI (IN)
	m_pMIDI->SetInDelay(m_pConfig->midiin_delay);

	// Restaurar retraso MIDI (OUT)
	m_pMIDI->SetOutDelay(m_pConfig->midiout_delay);

	// Base class
	CConfigPage::OnCancel();
}

//---------------------------------------------------------------------------
//
//	Desplazamiento vertical
//
//---------------------------------------------------------------------------
void CMIDIPage::OnVScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar *pBar)
{
	CSpinButtonCtrl *pSpin;

	// IN
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYIS);
	ASSERT(pSpin);
	if ((CWnd*)pSpin == (CWnd*)pBar) {
		m_pMIDI->SetInDelay(nPos);
	}

	// OUT
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYOS);
	ASSERT(pSpin);
	if ((CWnd*)pSpin == (CWnd*)pBar) {
		m_pMIDI->SetOutDelay(nPos);
	}
}

//---------------------------------------------------------------------------
//
//	Clic en ID de placa
//
//---------------------------------------------------------------------------
void CMIDIPage::OnBIDClick()
{
	CButton *pButton;

	// Get the board control without an ID
	pButton = (CButton*)GetDlgItem(IDC_MIDI_BID0);
	ASSERT(pButton);

	// Check�E��E��E���E�Ă��E�邩�E�ǂ��E��E��E�Œ��E�ׂ�
	if (pButton->GetCheck() == 1) {
		EnableControls(FALSE);
	}
	else {
		EnableControls(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CMIDIPage::EnableControls(BOOL bEnable)
{
	int i;
	CWnd *pWnd;

	ASSERT(this);

	// Flag check
	if (m_bEnableCtrl == bEnable) {
		return;
	}
	m_bEnableCtrl = bEnable;

	// Configurar todos los controles excepto ID de placa y Ayuda
	for(i=0; ; i++) {
		// End check
		if (ControlTable[i] == NULL) {
			break;
		}

		// Get control
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);
		pWnd->EnableWindow(bEnable);
	}
}

//---------------------------------------------------------------------------
//
//	Tabla de IDs de controles
//
//---------------------------------------------------------------------------
const UINT CMIDIPage::ControlTable[] = {
	IDC_MIDI_ILEVELG,
	IDC_MIDI_ILEVEL4,
	IDC_MIDI_ILEVEL2,
	IDC_MIDI_RSTG,
	IDC_MIDI_RSTGM,
	IDC_MIDI_RSTGS,
	IDC_MIDI_RSTXG,
	IDC_MIDI_RSTLA,
	IDC_MIDI_DEVG,
	IDC_MIDI_INS,
	IDC_MIDI_INC,
	IDC_MIDI_DLYIL,
	IDC_MIDI_DLYIE,
	IDC_MIDI_DLYIS,
	IDC_MIDI_DLYIG,
	IDC_MIDI_OUTS,
	IDC_MIDI_OUTC,
	IDC_MIDI_DLYOL,
	IDC_MIDI_DLYOE,
	IDC_MIDI_DLYOS,
	IDC_MIDI_DLYOG,
	NULL
};

//===========================================================================
//
//	Page de modificaciones
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CAlterPage::CAlterPage()
{
	// Always set ID and Help
	m_dwID = MAKEID('A', 'L', 'T', ' ');
	m_nTemplate = IDD_ALTERPAGE;
	m_uHelpID = IDC_ALTER_HELP;

	// Initialization
	m_bInit = FALSE;
	m_bParity = FALSE;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CAlterPage::OnInitDialog()
{
	// Base class
	CConfigPage::OnInitDialog();

	// Initialization�E�ς݁A�E�p�E��E��E�e�E�B�E�t�E��E��E�O�E��E��E�擾�E��E��E�Ă��E��E�
	m_bInit = TRUE;
	m_bParity = m_pConfig->sasi_parity;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Moverse de pagina
//
//---------------------------------------------------------------------------
BOOL CAlterPage::OnKillActive()
{
	CButton *pButton;

	ASSERT(this);

	// Check�E�{�E�b�E�N�E�X�E��E��E�p�E��E��E�e�E�B�E�t�E��E��E�O�E�ɔ��E�f�E��E��E��E��E��E�
	pButton = (CButton*)GetDlgItem(IDC_ALTER_PARITY);
	ASSERT(pButton);
	if (pButton->GetCheck() == 1) {
		m_bParity = TRUE;
	}
	else {
		m_bParity = FALSE;
	}

	// Base class
	return CConfigPage::OnKillActive();
}

//---------------------------------------------------------------------------
//
//	Intercambio de datos
//
//---------------------------------------------------------------------------
void CAlterPage::DoDataExchange(CDataExchange *pDX)
{
	ASSERT(this);
	ASSERT(pDX);

	// Base class
	CConfigPage::DoDataExchange(pDX);

	// Intercambio de datos
	DDX_Check(pDX, IDC_ALTER_SRAM, m_pConfig->sram_64k);
	DDX_Check(pDX, IDC_ALTER_SCC, m_pConfig->scc_clkup);
	DDX_Check(pDX, IDC_ALTER_POWERLED, m_pConfig->power_led);
	DDX_Check(pDX, IDC_ALTER_2DD, m_pConfig->dual_fdd);
	DDX_Check(pDX, IDC_ALTER_PARITY, m_pConfig->sasi_parity);
}

//---------------------------------------------------------------------------
//
//	SASI�E�p�E��E��E�e�E�B�E�@�E�\Check
//
//---------------------------------------------------------------------------
BOOL FASTCALL CAlterPage::HasParity(const Config *pConfig) const
{
	ASSERT(this);
	ASSERT(pConfig);

	// Initialization�E��E��E��E�Ă��E�Ȃ��E��E�΁A�E�^�E��E��E�ꂽConfig�E�f�E�[�E�^�E��E��E��E�
	if (!m_bInit) {
		return pConfig->sasi_parity;
	}

	// Initialization�E�ς݂Ȃ�A�E�ŐV�E��E�Editar�E��E��E�ʂ�m�E�点�E��E�
	return m_bParity;
}

//===========================================================================
//
//	Page de reanudacion (Resume)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CResumePage::CResumePage()
{
	// Always set ID and Help
	m_dwID = MAKEID('R', 'E', 'S', 'M');
	m_nTemplate = IDD_RESUMEPAGE;
	m_uHelpID = IDC_RESUME_HELP;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CResumePage::OnInitDialog()
{
	// Base class
	CConfigPage::OnInitDialog();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Intercambio de datos
//
//---------------------------------------------------------------------------
void CResumePage::DoDataExchange(CDataExchange *pDX)
{
	ASSERT(this);
	ASSERT(pDX);

	// Base class
	CConfigPage::DoDataExchange(pDX);

	// Intercambio de datos
	DDX_Check(pDX, IDC_RESUME_FDC, m_pConfig->resume_fd);
	DDX_Check(pDX, IDC_RESUME_MOC, m_pConfig->resume_mo);
	DDX_Check(pDX, IDC_RESUME_CDC, m_pConfig->resume_cd);
	DDX_Check(pDX, IDC_RESUME_XM6C, m_pConfig->resume_state);
	DDX_Check(pDX, IDC_RESUME_SCREENC, m_pConfig->resume_screen);
	DDX_Check(pDX, IDC_RESUME_DIRC, m_pConfig->resume_dir);
}

//===========================================================================
//
//	Dialogo TrueKey
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CTKeyDlg::CTKeyDlg(CWnd *pParent) : CDialog(IDD_KEYINDLG, pParent)
{
	CFrmWnd *pFrmWnd;

	// English�E���E�ւ̑Ή�
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_KEYINDLG);
		m_nIDHelp = IDD_US_KEYINDLG;
	}

	// Get TrueKey
	pFrmWnd = ResolveFrmWnd(pParent);
	m_pTKey = pFrmWnd ? pFrmWnd->GetTKey() : NULL;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CTKeyDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_GETDLGCODE()
	ON_WM_TIMER()
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Dialog Init
//
//---------------------------------------------------------------------------
BOOL CTKeyDlg::OnInitDialog()
{
	CString strText;
	CString targetText;
	CStatic *pStatic;
	LPCSTR lpszKey;

	// Base class
	CDialog::OnInitDialog();

	if (!m_pTKey) {
		CFrmWnd *pFrmWnd = ResolveFrmWnd(this);
		if (pFrmWnd) {
			m_pTKey = pFrmWnd->GetTKey();
		}
	}
	if (!m_pTKey) {
		return FALSE;
	}

	// Disable IME
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// �E�K�E�C�E�h�E��E�`�E��E�Procesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_LABEL);
	if (!pStatic) {
		TRACE(_T("CTKeyDlg::OnInitDialog: missing IDC_KEYIN_LABEL\n"));
		return FALSE;
	}
	pStatic->GetWindowText(strText);
	targetText.Format(_T("%02X"), m_nTarget);
	m_strGuide = strText;
	m_strGuide.Replace(_T("%02X"), targetText);
	pStatic->GetWindowRect(&m_rectGuide);
	ScreenToClient(&m_rectGuide);
	pStatic->DestroyWindow();

	// Asignacion�E��E�`�E��E�Procesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_STATIC);
	if (!pStatic) {
		TRACE(_T("CTKeyDlg::OnInitDialog: missing IDC_KEYIN_STATIC\n"));
		return FALSE;
	}
	pStatic->GetWindowText(m_strAssign);
	pStatic->GetWindowRect(&m_rectAssign);
	ScreenToClient(&m_rectAssign);
	pStatic->DestroyWindow();

	// �E�L�E�[�E��E�`�E��E�Procesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_KEY);
	if (!pStatic) {
		TRACE(_T("CTKeyDlg::OnInitDialog: missing IDC_KEYIN_KEY\n"));
		return FALSE;
	}
	pStatic->GetWindowText(m_strKey);
	if (m_nKey != 0) {
		lpszKey = m_pTKey->GetKeyID(m_nKey);
		if (lpszKey) {
			m_strKey = lpszKey;
		}
	}
	pStatic->GetWindowRect(&m_rectKey);
	ScreenToClient(&m_rectKey);
	pStatic->DestroyWindow();

	// Keyboard�E��E�Ԃ��E�擾
	::GetKeyboardState(m_KeyState);

	// Timer�E��E��E�͂�
	m_nTimerID = SetTimer(IDD_KEYINDLG, 100, NULL);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	OK
//
//---------------------------------------------------------------------------
void CTKeyDlg::OnOK()
{
	// [CR]�E�ɂ��E�Fin�E��E�}�E��E�
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CTKeyDlg::OnCancel()
{
	// Timer�E��E�~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Base class
	CDialog::OnCancel();
}

//---------------------------------------------------------------------------
//
//	Dibujar
//
//---------------------------------------------------------------------------
void CTKeyDlg::OnPaint()
{
	CPaintDC dc(this);
	CDC mDC;
	CRect rect;
	CBitmap Bitmap;
	CBitmap *pBitmap;

	// Create DC en memoria
	VERIFY(mDC.CreateCompatibleDC(&dc));

	// Create mapa de bits compatible
	GetClientRect(&rect);
	VERIFY(Bitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height()));
	pBitmap = mDC.SelectObject(&Bitmap);
	ASSERT(pBitmap);

	// Limpiar fondo
	mDC.FillSolidRect(&rect, ::GetSysColor(COLOR_3DFACE));

	// Dibujar
	OnDraw(&mDC);

	// BitBlt
	VERIFY(dc.BitBlt(0, 0, rect.Width(), rect.Height(), &mDC, 0, 0, SRCCOPY));

	// �E�r�E�b�E�g�E�}�E�b�E�vFin
	VERIFY(mDC.SelectObject(pBitmap));
	VERIFY(Bitmap.DeleteObject());

	// �E��E��E��E��E��E�DCFin
	VERIFY(mDC.DeleteDC());
}

//---------------------------------------------------------------------------
//
//	Dibujar�E��E��E�C�E��E�
//
//---------------------------------------------------------------------------
void FASTCALL CTKeyDlg::OnDraw(CDC *pDC)
{
	HFONT hFont;
	CFont *pFont;
	CFont *pDefFont;

	ASSERT(this);
	ASSERT(pDC);

	// Color settings
	pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
	pDC->SetBkColor(::GetSysColor(COLOR_3DFACE));

	// Font settings
	hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	ASSERT(hFont);
	pFont = CFont::FromHandle(hFont);
	pDefFont = pDC->SelectObject(pFont);
	ASSERT(pDefFont);

	// Mostrar
	pDC->DrawText(m_strGuide, m_rectGuide,
					DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
	pDC->DrawText(m_strAssign, m_rectAssign,
					DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
	pDC->DrawText(m_strKey, m_rectKey,
					DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

	// Restore font(Objetos�E��E�Delete�E��E��E�Ȃ��E�Ă悢)
	pDC->SelectObject(pDefFont);
}

//---------------------------------------------------------------------------
//
//	�E�_�E�C�E�A�E��E��E�OGet codigo
//
//---------------------------------------------------------------------------
UINT CTKeyDlg::OnGetDlgCode()
{
	// Habilitar la recepcion de mensajes de teclado
	return DLGC_WANTMESSAGE;
}

//---------------------------------------------------------------------------
//
//	Timer
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
void CTKeyDlg::OnTimer(UINT_PTR nID)
#else
void CTKeyDlg::OnTimer(UINT nID)
#endif
{
	BYTE state[0x100];
	int nKey;
	int nTarget;
	LPCTSTR lpszKey;

	// IDCheck
	if (m_nTimerID != nID) {
		return;
	}

	// Get tecla
	GetKeyboardState(state);

	// Si coincide, no hay cambio
	if (memcmp(state, m_KeyState, sizeof(state)) == 0) {
		return;
	}

	// Hacer copia de respaldo del objetivo
	nTarget = m_nKey;

	// Buscar cambios, excluyendo LBUTTON y RBUTTON
	for (nKey=3; nKey<0x100; nKey++) {
		// No pulsado anteriormente
		if ((m_KeyState[nKey] & 0x80) == 0) {
			// Pulsado esta vez
			if (state[nKey] & 0x80) {
				// �E�^�E�[�E�Q�E�b�E�g�E�ݒ�
				nTarget = nKey;
				break;
			}
		}
	}

	// �E�X�E�e�E�[�E�gActualizacion
	memcpy(m_KeyState, state, sizeof(state));

	// Si el objetivo no ha cambiado, no hacer nada
	if (m_nKey == nTarget) {
		return;
	}

	// �E��E��E��E��E��E�擾
	lpszKey = m_pTKey->GetKeyID(nTarget);
	if (lpszKey) {
		// Hay cadena de tecla, nueva configuracion
		m_nKey = nTarget;

		// �E�R�E��E��E�g�E��E��E�[�E��E��E�ɐݒ�A�E��E�Dibujar
		m_strKey = lpszKey;
		Invalidate(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Clic derecho
//
//---------------------------------------------------------------------------
void CTKeyDlg::OnRButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
	// Timer�E��E�~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// �E�_�E�C�E�A�E��E��E�OFin
	EndDialog(IDOK);
}

//===========================================================================
//
//	Page TrueKey
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CTKeyPage::CTKeyPage()
{
	// Always set ID and Help
	m_dwID = MAKEID('T', 'K', 'E', 'Y');
	m_nTemplate = IDD_TKEYPAGE;
	m_uHelpID = IDC_TKEY_HELP;
	m_pInput = NULL;
	m_pTKey = NULL;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CTKeyPage, CConfigPage)
	ON_BN_CLICKED(IDC_TKEY_WINC, OnSelChange)
	ON_CBN_SELCHANGE(IDC_TKEY_COMC, OnSelChange)
	ON_NOTIFY(NM_CLICK, IDC_TKEY_LIST, OnClick)
	ON_NOTIFY(NM_RCLICK, IDC_TKEY_LIST, OnRClick)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CTKeyPage::OnInitDialog()
{
	CComboBox *pComboBox;
	CButton *pButton;
	CString strText;
	int i;
	CDC *pDC;
	TEXTMETRIC tm;
	LONG cx;
	CListCtrl *pListCtrl;
	int nItem;
	LPCTSTR lpszKey;

	// Base class
	CConfigPage::OnInitDialog();

	if (!m_pInput || !m_pTKey) {
		CFrmWnd *pFrmWnd = ResolveFrmWnd(this);
		if (pFrmWnd) {
			if (!m_pInput) {
				m_pInput = pFrmWnd->GetInput();
			}
			if (!m_pTKey) {
				m_pTKey = pFrmWnd->GetTKey();
			}
		}
	}
	if (!m_pInput || !m_pTKey) {
		return FALSE;
	}

	// Cuadro combinado de puertos
	pComboBox = (CComboBox*)GetDlgItem(IDC_TKEY_COMC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_TKEY_NOASSIGN, strText);
	pComboBox->AddString(strText);
	for (i=1; i<=9; i++) {
		strText.Format(_T("COM%d"), i);
		pComboBox->AddString(strText);
	}
	pComboBox->SetCurSel(m_pConfig->tkey_com);

	// Inversion RTS
	pButton = (CButton*)GetDlgItem(IDC_TKEY_RTS);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->tkey_rts);

	// Modo
	pButton = (CButton*)GetDlgItem(IDC_TKEY_VMC);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->tkey_mode & 1);
	pButton = (CButton*)GetDlgItem(IDC_TKEY_WINC);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->tkey_mode >> 1);

	// Get text metrics
	pDC = new CClientDC(this);
	::GetTextMetrics(pDC->m_hDC, &tm);
	delete pDC;
	cx = tm.tmAveCharWidth;

	// List control columns
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_TKEY_LIST);
	ASSERT(pListCtrl);
	if (::IsJapanese()) {
		// Japanese
		::GetMsg(IDS_TKEY_NO, strText);
		pListCtrl->InsertColumn(0, strText, LVCFMT_LEFT, cx * 4, 0);
		::GetMsg(IDS_TKEY_KEYTOP, strText);
		pListCtrl->InsertColumn(1, strText, LVCFMT_LEFT, cx * 10, 1);
		::GetMsg(IDS_TKEY_VK, strText);
		pListCtrl->InsertColumn(2, strText, LVCFMT_LEFT, cx * 22, 2);
	}
	else {
		// English
		::GetMsg(IDS_TKEY_NO, strText);
		pListCtrl->InsertColumn(0, strText, LVCFMT_LEFT, cx * 5, 0);
		::GetMsg(IDS_TKEY_KEYTOP, strText);
		pListCtrl->InsertColumn(1, strText, LVCFMT_LEFT, cx * 12, 1);
		::GetMsg(IDS_TKEY_VK, strText);
		pListCtrl->InsertColumn(2, strText, LVCFMT_LEFT, cx * 18, 2);
	}

	// Create items (la information del lado X68000 es fija independientemente del mapeo)
	pListCtrl->DeleteAllItems();
	nItem = 0;
	for (i=0; i<=0x73; i++) {
		// Get el nombre de la tecla desde CKeyDispWnd
		lpszKey = m_pInput->GetKeyName(i);
		if (lpszKey) {
			// This key is valid
			strText.Format(_T("%02X"), i);
			pListCtrl->InsertItem(nItem, strText);
			pListCtrl->SetItemText(nItem, 1, lpszKey);
			pListCtrl->SetItemData(nItem, (DWORD)i);
			nItem++;
		}
	}

	// Full row select option for list control (COMCTL32.DLL v4.71+)
	pListCtrl->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

	// Get mapeo VK
	m_pTKey->GetKeyMap(m_nKey);

	// �E��E��E�X�E�g�E�R�E��E��E�g�E��E��E�[�E��E�Actualizacion
	UpdateReport();

	// Control activation settings
	if (m_pConfig->tkey_com == 0) {
		m_bEnableCtrl = TRUE;
		EnableControls(FALSE);
	}
	else {
		m_bEnableCtrl = FALSE;
		EnableControls(TRUE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CTKeyPage::OnOK()
{
	CComboBox *pComboBox;
	CButton *pButton;

	// Cuadro combinado de puertos
	pComboBox = (CComboBox*)GetDlgItem(IDC_TKEY_COMC);
	ASSERT(pComboBox);
	m_pConfig->tkey_com = pComboBox->GetCurSel();

	// Inversion RTS
	pButton = (CButton*)GetDlgItem(IDC_TKEY_RTS);
	ASSERT(pButton);
	m_pConfig->tkey_rts = pButton->GetCheck();

	// Asignacion
	m_pConfig->tkey_mode = 0;
	pButton = (CButton*)GetDlgItem(IDC_TKEY_VMC);
	ASSERT(pButton);
	if (pButton->GetCheck() == 1) {
		m_pConfig->tkey_mode |= 1;
	}
	pButton = (CButton*)GetDlgItem(IDC_TKEY_WINC);
	ASSERT(pButton);
	if (pButton->GetCheck() == 1) {
		m_pConfig->tkey_mode |= 2;
	}

	// Set key map
	m_pTKey->SetKeyMap(m_nKey);

	// Base class
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cambio en el cuadro combinado
//
//---------------------------------------------------------------------------
void CTKeyPage::OnSelChange()
{
	CComboBox *pComboBox;

	pComboBox = (CComboBox*)GetDlgItem(IDC_TKEY_COMC);
	ASSERT(pComboBox);

	// Controls enabled/disabled
	if (pComboBox->GetCurSel() > 0) {
		EnableControls(TRUE);
	}
	else {
		EnableControls(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Clic en item
//
//---------------------------------------------------------------------------
void CTKeyPage::OnClick(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	CListCtrl *pListCtrl;
	int nItem;
	int nCount;
	int nKey;
	CTKeyDlg dlg(this);

	// �E��E��E�X�E�gGet control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_TKEY_LIST);
	ASSERT(pListCtrl);

	// Get count
	nCount = pListCtrl->GetItemCount();

	// Get selected index
	for (nItem=0; nItem<nCount; nItem++) {
		if (pListCtrl->GetItemState(nItem, LVIS_SELECTED)) {
			break;
		}
	}
	if (nItem >= nCount) {
		return;
	}

	// Get data pointed to by that index(1�E�`0x73)
	nKey = (int)pListCtrl->GetItemData(nItem);
	ASSERT((nKey >= 1) && (nKey <= 0x73));

	// Start configuration
	dlg.m_nTarget = nKey;
	dlg.m_nKey = m_nKey[nKey - 1];

	// Execute dialog
	if (dlg.DoModal() != IDOK) {
		return;
	}

	// Configure key map
	m_nKey[nKey - 1] = dlg.m_nKey;

	// Mostrar�E��E�Actualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	Clic derecho en item
//
//---------------------------------------------------------------------------
void CTKeyPage::OnRClick(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	CString strText;
	CString strMsg;
	CListCtrl *pListCtrl;
	int nItem;
	int nCount;
	int nKey;

	// �E��E��E�X�E�gGet control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_TKEY_LIST);
	ASSERT(pListCtrl);

	// Get count
	nCount = pListCtrl->GetItemCount();

	// Get selected index
	for (nItem=0; nItem<nCount; nItem++) {
		if (pListCtrl->GetItemState(nItem, LVIS_SELECTED)) {
			break;
		}
	}
	if (nItem >= nCount) {
		return;
	}

	// Get data pointed to by that index(1�E�`0x73)
	nKey = (int)pListCtrl->GetItemData(nItem);
	ASSERT((nKey >= 1) && (nKey <= 0x73));

	// �E��E��E�ł�Delete�E��E��E��E�Ă��E��E��E�No hacer nada
	if (m_nKey[nKey - 1] == 0) {
		return;
	}

	// �E��E��E�b�E�Z�E�[�E�W�E�{�E�b�E�N�E�X�E�ŁADelete�E�̗L�E��E��E��E�Check
	::GetMsg(IDS_KBD_DELMSG, strText);
	strMsg.Format(strText, nKey, m_pTKey->GetKeyID(m_nKey[nKey - 1]));
	::GetMsg(IDS_KBD_DELTITLE, strText);
	if (MessageBox(strMsg, strText, MB_ICONQUESTION | MB_YESNO) != IDYES) {
		return;
	}

	// �E��E��E��E�
	m_nKey[nKey - 1] = 0;

	// Mostrar�E��E�Actualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	�E��E��E�|�E�[�E�gActualizacion
//
//---------------------------------------------------------------------------
void FASTCALL CTKeyPage::UpdateReport()
{
	CListCtrl *pListCtrl;
	LPCTSTR lpszKey;
	int nItem;
	int nKey;
	int nVK;
	CString strNext;
	CString strPrev;

	ASSERT(this);

	// Get control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_TKEY_LIST);
	ASSERT(pListCtrl);

	// List control row
	nItem = 0;
	for (nKey=1; nKey<=0x73; nKey++) {
		// Get el nombre de la tecla desde CKeyDispWnd
		lpszKey = m_pInput->GetKeyName(nKey);
		if (lpszKey) {
			// �E�L�E��E��E�ȃL�E�[�E��E��E��E��E��E�BInitialization
			strNext.Empty();

			// VKAsignacion�E��E��E��E��E��E�΁AGet nombre
			nVK = m_nKey[nKey - 1];
			if (nVK != 0) {
				lpszKey = m_pTKey->GetKeyID(nVK);
				strNext = lpszKey;
			}

			// Overwrite if different
			strPrev = pListCtrl->GetItemText(nItem, 2);
			if (strPrev != strNext) {
				pListCtrl->SetItemText(nItem, 2, strNext);
			}

			// Next item
			nItem++;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CTKeyPage::EnableControls(BOOL bEnable)
{
	int i;
	CWnd *pWnd;
	CButton *pButton;
	BOOL bCheck;

	ASSERT(this);

	// WindowsCheck�E�{�E�b�E�N�E�X�E�擾
	pButton = (CButton*)GetDlgItem(IDC_TKEY_WINC);
	ASSERT(pButton);
	bCheck = FALSE;
	if (pButton->GetCheck() != 0) {
		bCheck = TRUE;
	}

	// Flag check
	if (m_bEnableCtrl == bEnable) {
		// Retornar solo en caso de FALSE -> FALSE
		if (!m_bEnableCtrl) {
			return;
		}
	}
	m_bEnableCtrl = bEnable;

	// Configure all controls except Device and Help
	for(i=0; ; i++) {
		// End check
		if (ControlTable[i] == NULL) {
			break;
		}

		// Get control
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);

		// ControlTable[i]�E��E�IDC_TKEY_MAPG, IDC_TKEY_LIST�E�͓��E��E�
		switch (ControlTable[i]) {
			// En cuanto a los controles de mapeo de teclas de Windows
			case IDC_TKEY_MAPG:
			case IDC_TKEY_LIST:
				// Solo si bEnable y Windows activo
				if (bEnable && bCheck) {
					pWnd->EnableWindow(TRUE);
				}
				else {
					pWnd->EnableWindow(FALSE);
				}
				break;

			// Other�E��E�bEnable�E�ɏ]�E��E�
			default:
				pWnd->EnableWindow(bEnable);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Tabla de IDs de controles
//
//---------------------------------------------------------------------------
const UINT CTKeyPage::ControlTable[] = {
	IDC_TKEY_RTS,
	IDC_TKEY_ASSIGNG,
	IDC_TKEY_VMC,
	IDC_TKEY_WINC,
	IDC_TKEY_MAPG,
	IDC_TKEY_LIST,
	NULL
};

//===========================================================================
//
//	Otros�E�y�E�[�E�W
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CMiscPage::CMiscPage()
{
	// Always set ID and Help
	m_dwID = MAKEID('M', 'I', 'S', 'C');
	m_nTemplate = IDD_MISCPAGE;
	m_uHelpID = IDC_MISC_HELP;
}



BEGIN_MESSAGE_MAP(CMiscPage, CConfigPage)
	ON_BN_CLICKED(IDC_BUSCAR, OnBuscarFolder)
END_MESSAGE_MAP()


/* AQU? SE INICIALIZA EN EL CAMPO DE TEXTO LA RUTA DE GUARDADOS R?PIDOS */
BOOL CMiscPage::OnInitDialog()
{
	CComboBox *pCombo;
	CEdit *pEdit;
	int nRendererID;
	int nRendererLblID;
	CConfigPage::OnInitDialog();
	CFrmWnd *pFrmWnd;
	pFrmWnd = ResolveFrmWnd(this);

	// Opciones
	pEdit = (CEdit*)GetDlgItem(IDC_EDIT1);
	if (pFrmWnd) {
		pEdit->SetWindowTextA(pFrmWnd->m_strSaveStatePath);
	}
	else {
		pEdit->SetWindowTextA(m_pConfig->ruta_savestate);
	}

	// Determinar IDs segun idioma
	if (::IsJapanese()) {
		nRendererID = IDC_MISC_RENDERER;
		nRendererLblID = IDC_MISC_RENDERERL;
	} else {
		nRendererID = IDC_US_MISC_RENDERER;
		nRendererLblID = IDC_US_MISC_RENDERERL;
	}

	// Inicializar combobox de renderizador
	pCombo = (CComboBox*)GetDlgItem(nRendererID);
	if (pCombo) {
		pCombo->AddString(_T("GDI"));
		pCombo->AddString(_T("DirectX 9"));
		pCombo->SetCurSel(m_pConfig->render_mode);
	}

	/*int msgboxID = MessageBox(
       m_pConfig->ruta_savestate,"saves",
         2 );	*/

	return TRUE;
}


/* AQU? SE ABRE EL DI?LOGO PARA SELECCIONAR UNA CARPETA DEL SISTEMA  */
void CMiscPage::OnBuscarFolder()
{
	CEdit *pEdit;

/*	CFolderPickerDialog folderPickerDialog("c:\\", OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_ENABLESIZING, this,
        sizeof(OPENFILENAME));
    CString folderPath;


    if (folderPickerDialog.DoModal() == IDOK)
    {
        POSITION pos = folderPickerDialog.GetStartPosition();
        while (pos)
        {
            folderPath = folderPickerDialog.GetNextPathName(pos);
        }
    }*/


   CFolderPickerDialog m_dlg;
   CString folderPath;

	m_dlg.m_ofn.lpstrTitle = _T("Buscar folder");
	m_dlg.m_ofn.lpstrInitialDir = _T("C:\\");
	if (m_dlg.DoModal() == IDOK) {
folderPath=m_dlg.GetPathName();// Use this to get the selected folder name
// after the dialog has closed

// May need to add a '\' for usage in GUI and for later file saving,
// as there is no '\' on the returned name
       folderPath += _T("\\");
UpdateData(FALSE);// To show updated folder in GUI

// Debug
}


	pEdit = (CEdit*)GetDlgItem(IDC_EDIT1);
	pEdit->SetWindowTextA(folderPath);
}


/* AQU? SE GUARDA LA RUTA DE GUARDADO R?PIDO DE ESTADOS */
void CMiscPage::OnOK()
{
	CComboBox *pCombo;
	CEdit *pEdit;
	CString  folderDestino;
	int nRendererID;
	pEdit = (CEdit*)GetDlgItem(IDC_EDIT1);
	pEdit->GetWindowTextA(folderDestino);
	_tcscpy(m_pConfig->ruta_savestate, folderDestino);

	// Determinar ID segun idioma
	if (::IsJapanese()) {
		nRendererID = IDC_MISC_RENDERER;
	} else {
		nRendererID = IDC_US_MISC_RENDERER;
	}

	// Save valor del renderizador
	pCombo = (CComboBox*)GetDlgItem(nRendererID);
	if (pCombo) {
		m_pConfig->render_mode = pCombo->GetCurSel();
	}

	CConfigPage::OnOK();
}


//---------------------------------------------------------------------------
//
//	Intercambio de datos
//
//---------------------------------------------------------------------------
void CMiscPage::DoDataExchange(CDataExchange *pDX)
 {
 	int nVSyncID;

	// Determinar ID de VSync segun idioma (los demas controles usan los mismos IDs en ambas versiones)
 	if (::IsJapanese()) {
 		nVSyncID = IDC_MISC_VSYNC;
 	} else {
 		nVSyncID = IDC_US_MISC_VSYNC;
 	}

 	CConfigPage::DoDataExchange(pDX);

	// Intercambio de datos
 	DDX_Check(pDX, IDC_MISC_FDSPEED, m_pConfig->floppy_speed);
 	DDX_Check(pDX, IDC_MISC_FDLED, m_pConfig->floppy_led);
 	DDX_Check(pDX, IDC_MISC_POPUP, m_pConfig->popup_swnd);
 	DDX_Check(pDX, IDC_MISC_AUTOMOUSE, m_pConfig->auto_mouse);
 	DDX_Check(pDX, IDC_MISC_POWEROFF, m_pConfig->power_off);
 	DDX_Check(pDX, nVSyncID, m_pConfig->render_vsync);
 }

//===========================================================================
//
//	Configuration�E�v�E��E��E�p�E�e�E�B�E�V�E�[�E�g
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CConfigSheet::CConfigSheet(CWnd *pParent) : CPropertySheet(IDS_OPTIONS, pParent)
{
	// En este punto los datos de configuracion son NULL
	m_pConfig = NULL;

	// English�E���E�ւ̑Ή�
	if (!::IsJapanese()) {
		::GetMsg(IDS_OPTIONS, m_strCaption);
	}

	// ApplyButtons�E��E�Delete
	m_psh.dwFlags |= PSH_NOAPPLYNOW;

	// Memorizar ventana padre
	m_pFrmWnd = (CFrmWnd*)pParent;

	// Timer�E�Ȃ�
	m_nTimerID = NULL;

	// �E�y�E�[�E�WInitialization
	m_Basic.Init(this);
	m_Sound.Init(this);
	m_Vol.Init(this);
	m_Kbd.Init(this);
	m_Mouse.Init(this);
	m_Joy.Init(this);
	m_SASI.Init(this);
	m_SxSI.Init(this);
	m_SCSI.Init(this);
	m_Port.Init(this);
	m_MIDI.Init(this);
	m_Alter.Init(this);
	m_Resume.Init(this);
	m_TKey.Init(this);
	m_Misc.Init(this);
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CConfigSheet, CPropertySheet)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_TIMER()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Search Page
//
//---------------------------------------------------------------------------
CConfigPage* FASTCALL CConfigSheet::SearchPage(DWORD dwID) const
{
	int nPage;
	int nCount;
	CConfigPage *pPage;

	ASSERT(this);
	ASSERT(dwID != 0);

	// Get numero de paginas
	nCount = GetPageCount();
	ASSERT(nCount >= 0);

	// �E�y�E�[�E�WBucle
	for (nPage=0; nPage<nCount; nPage++) {
		// Get page
		pPage = (CConfigPage*)GetPage(nPage);
		ASSERT(pPage);

		// IDCheck
		if (pPage->GetID() == dwID) {
			return pPage;
		}
	}

	// No se encontro
	return NULL;
}

//---------------------------------------------------------------------------
//
//	On Create
//
//---------------------------------------------------------------------------
int CConfigSheet::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	// Base class
	if (CPropertySheet::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Timer�E��E��E�C�E��E��E�X�E�g�E�[�E��E�
	m_nTimerID = SetTimer(IDM_OPTIONS, 100, NULL);

	return 0;
}

//---------------------------------------------------------------------------
//
//	On Destroy
//
//---------------------------------------------------------------------------
void CConfigSheet::OnDestroy()
{
	// Timer�E��E�~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Base class
	CPropertySheet::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	Timer
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
void CConfigSheet::OnTimer(UINT_PTR nID)
#else
void CConfigSheet::OnTimer(UINT nID)
#endif
{
	CInfo *pInfo;

	ASSERT(m_pFrmWnd);

	// IDCheck
	if (m_nTimerID != nID) {
		return;
	}

	// Timer�E��E�~
	KillTimer(m_nTimerID);
	m_nTimerID = NULL;

	// Info�E��E��E��E��E�݂��E��E�΁AActualizacion
	pInfo = m_pFrmWnd->GetInfo();
	if (pInfo) {
		pInfo->UpdateStatus();
		pInfo->UpdateCaption();
		pInfo->UpdateInfo();
	}

	// Timer�E�ĊJ(Mostrar�E��E��E��E��E��E��E��E�100ms�E��E��E��E��E��E�)
	m_nTimerID = SetTimer(IDM_OPTIONS, 100, NULL);
}

#endif	// _WIN32
