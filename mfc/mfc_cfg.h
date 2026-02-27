//---------------------------------------------------------------------------
//
//	EMULADOR X68000 "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ MFC Configuracion ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_cfg_h)
#define mfc_cfg_h

#include "config.h"
#include "ppi.h"

//===========================================================================
//
//	Configuracion
//
//===========================================================================
class CConfig : public CComponent
{
public:
	// Funciones basicas
	CConfig(CFrmWnd *pWnd);
										// Constructor
	BOOL FASTCALL Init();
	BOOL FASTCALL CustomInit(BOOL ArchivoDefault);   // mi personalizacion
										// Inicializacion
	void FASTCALL Cleanup();
	void FASTCALL Cleanup2();
										// Limpieza (Cleanup)

	// Datos de configuracion (global)
	void FASTCALL GetConfig(Config *pConfigBuf) const;
										// Obtener datos de configuracion
	void FASTCALL SetConfig(Config *pConfigBuf);
										// Establecer datos de configuracion

	// Datos de configuracion (individuales)
	void FASTCALL SetStretch(BOOL bStretch);
										// Configuracion de expansion de pantalla
	void FASTCALL SetMIDIDevice(int nDevice, BOOL bIn);
										// Configuracion de dispositivo MIDI

	// MRU
	void FASTCALL SetMRUFile(int nType, LPCTSTR pszFile);
										// Configuracion de archivo MRU (mas reciente)
	void FASTCALL GetMRUFile(int nType, int nIndex, LPTSTR pszFile) const;
										// Obtener archivo MRU
	int FASTCALL GetMRUNum(int nType) const;
										// Obtener numero de archivos MRU

	// Guardar / Cargar
	BOOL FASTCALL Save(Fileio *pFio, int nVer);
										// Guardar
	BOOL FASTCALL Load(Fileio *pFio, int nVer);
										// Cargar
	BOOL FASTCALL IsApply();
										// ?Aplicar?

private:
	// Datos de configuracion
	typedef struct _INIKEY {
		void *pBuf;						// �|�C���^
		LPCTSTR pszSection;				// Nombre de seccion
		LPCTSTR pszKey;					// Nombre de clave
		int nType;						// �^
		int nDef;						// Valor por defecto
		int nMin;						// Valor minimo (solo algunos tipos)
		int nMax;						// Valor maximo (solo algunos)
	} INIKEY, *PINIKEY;

	// Archivo INI
	TCHAR m_IniFile[FILEPATH_MAX];
										// Nombre del archivo INI

	// Datos de configuracion
	void FASTCALL LoadConfig();
										// Cargar datos de configuracion
	void FASTCALL SaveConfig() const;
										// Guardar datos de configuracion
	static const INIKEY IniTable[];
										// Tabla INI de datos de configuracion
	static Config m_Config;
										// Datos de configuracion

	// �o�[�W�����݊�
	void FASTCALL ResetSASI();
										// Reconfiguracion SASI
	void FASTCALL ResetCDROM();
										// Reconfiguracion CD-ROM
	static BOOL m_bCDROM;
										// CD-ROM activado

	// MRU
	enum {
		MruTypes = 5					// Numero de tipos MRU
	};
	void FASTCALL ClearMRU(int nType);
										// Limpiar MRU
	void FASTCALL LoadMRU(int nType);
										// Cargar MRU
	void FASTCALL SaveMRU(int nType) const;
										// Guardar MRU
	TCHAR m_MRUFile[MruTypes][9][FILEPATH_MAX];
										// Archivos MRU
	int m_MRUNum[MruTypes];
										// Numero de MRU

	// �L�[
	void FASTCALL LoadKey() const;
										// Cargar clave
	void FASTCALL SaveKey() const;
										// Guardar clave

	// TrueKey
	void FASTCALL LoadTKey() const;
										// Cargar TrueKey
	void FASTCALL SaveTKey() const;
										// Guardar TrueKey

	// �o�[�W�����݊�
	BOOL FASTCALL Load200(Fileio *pFio);
										// version 2.00 o version 2.01
	BOOL FASTCALL Load202(Fileio *pFio);
										// version 2.02 o version 2.03

	// Cargar�EGuardar
	BOOL m_bApply;
										// Cargar��ApplyCfg��v��
};

//---------------------------------------------------------------------------
//
//	Pre-definicion de clase
//
//---------------------------------------------------------------------------
class CConfigSheet;

//===========================================================================
//
//	ConfiguracionPagina de propiedades
//
//===========================================================================
class CConfigPage : public CPropertyPage
{
public:
	CConfigPage();
										// Constructor
	void FASTCALL Init(CConfigSheet *pSheet);
										// Inicializacion
	virtual BOOL OnInitDialog();
										// Inicializacion del dialogo
	virtual BOOL OnSetActive();
										// Pagina activa
	DWORD FASTCALL GetID() const		{ return m_dwID; }
										// Obtener ID

protected:
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT nMsg);
										// Configuracion del cursor del raton
	Config *m_pConfig;
										// Datos de configuracion
	DWORD m_dwID;
										// ID de la pagina
	int m_nTemplate;
										// ID de plantilla
	UINT m_uHelpID;
										// ID de ayuda
	UINT m_uMsgID;
										// ID de mensaje de ayuda
	CConfigSheet *m_pSheet;
										// Hoja de propiedades

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina basica
//
//===========================================================================
class CBasicPage : public CConfigPage
{
public:
	CBasicPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar

protected:
	afx_msg void OnMPUFull();
										// Velocidad completa MPU

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina de sonido
//
//===========================================================================
class CSoundPage : public CConfigPage
{
public:
	CSoundPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Desplazamiento vertical
	afx_msg void OnSelChange();
										// Cambio en el cuadro combinado

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Cambio de estado de los controles
	BOOL m_bEnableCtrl;
										// Estado de los controles
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina de volumen
//
//===========================================================================
class CVolPage : public CConfigPage
{
public:
	CVolPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar
	void OnCancel();
										// Cancelar

protected:
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Desplazamiento horizontal
	afx_msg void OnFMCheck();
										// Sintetizador FMVerificacion
	afx_msg void OnADPCMCheck();
										// Sintetizador ADPCMVerificacion
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
#else
	afx_msg void OnTimer(UINT nTimerID);
#endif
										// Temporizador

private:
	CSound *m_pSound;
										// Sonido
	OPMIF *m_pOPMIF;
										// Interfaz OPM
	ADPCM *m_pADPCM;
										// ADPCM
	CMIDI *m_pMIDI;
										// MIDI
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
#else
	UINT m_nTimerID;
#endif
										// ID del temporizador
	int m_nMasterVol;
										// Volumen maestro
	int m_nMasterOrg;
										// Volumen maestro original
	int m_nMIDIVol;
										// Volumen MIDI
	int m_nMIDIOrg;
										// Volumen MIDI original

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina del teclado
//
//===========================================================================
class CKbdPage : public CConfigPage
{
public:
	CKbdPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar
	void OnCancel();
										// Cancelar

protected:
	afx_msg void OnEdit();
										// Editar
	afx_msg void OnDefault();
										// Por defecto
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic en columna
	afx_msg void OnRClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic derecho en columna
	afx_msg void OnConnect();
										// Conexion

private:
	void FASTCALL UpdateReport();
										// ���|�[�gActualizacion
	void FASTCALL EnableControls(BOOL bEnable);
										// Cambio de estado de los controles
	DWORD m_dwEdit[0x100];
										// Edicion
	DWORD m_dwBackup[0x100];
										// Respaldo / Backup
	CInput *m_pInput;
										// CInput
	BOOL m_bEnableCtrl;
										// Estado de los controles
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Dialogo de edicion de mapa de teclado
//
//===========================================================================
class CKbdMapDlg : public CDialog
{
public:
	CKbdMapDlg(CWnd *pParent, DWORD *pMap);
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// OK
	void OnCancel();
										// Cancelar

protected:
	afx_msg void OnPaint();
										// �_�C�A���ODibujar
	afx_msg LONG OnKickIdle(UINT uParam, LONG lParam);
										// Proceso idle
	afx_msg LONG OnApp(UINT uParam, LONG lParam);
										// ���[�U(Notificacion de ventanas subordinadas)

private:
	void FASTCALL OnDraw(CDC *pDC);
										// Sub-rutina de dibujo
	CRect m_rectStat;
										// Rectangulo de estado
	CString m_strStat;
										// Mensaje de estado
	CString m_strGuide;
										// Mensaje guia
	CWnd *m_pDispWnd;
										// CKeyDispWnd
	CInput *m_pInput;
										// CInput
	DWORD *m_pEditMap;
										// Editar���̃}�b�v

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Dialogo de entrada de teclas
//
//===========================================================================
class CKeyinDlg : public CDialog
{
public:
	CKeyinDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// OK
	UINT m_nTarget;
										// Tecla objetivo
	UINT m_nKey;
										// Asignacion�L�[

protected:
	afx_msg void OnPaint();
										// Dibujar
	afx_msg UINT OnGetDlgCode();
										// �_�C�A���O�R�[�hObtener
	afx_msg LONG OnKickIdle(UINT uParam, LONG lParam);
										// Proceso idle
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
										// Clic derecho

private:
	void FASTCALL OnDraw(CDC *pDC);
										// Sub-rutina de dibujo
	CInput *m_pInput;
										// CInput
	BOOL m_bKey[0x100];
										// Para memorizar teclas
	CRect m_GuideRect;
										// Rectangulo guia
	CString m_GuideString;
										// Cadena guia
	CRect m_AssignRect;
										// �L�[Asignacion��`
	CString m_AssignString;
										// �L�[Asignacion������
	CRect m_KeyRect;
										// Rectangulo de tecla
	CString m_KeyString;
										// Cadena de tecla

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina del raton
//
//===========================================================================
class CMousePage : public CConfigPage
{
public:
	CMousePage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar

protected:
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Desplazamiento horizontal
	afx_msg void OnPort();
										// Seleccion de puerto

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Cambio de estado de los controles
	BOOL m_bEnableCtrl;
										// Estado de los controles
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina del joystick
//
//===========================================================================
class CJoyPage : public CConfigPage
{
public:
	CJoyPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar
	void OnCancel();
										// Cancelar

protected:
	BOOL OnCommand(WPARAM wParam, LPARAM lParam);
										// Notificacion de comando

private:
	void FASTCALL OnSelChg(CComboBox* pComboBox);
										// Cambio en el cuadro combinado
	void FASTCALL OnDetail(UINT nButton);
										// Detalles
	void FASTCALL OnSetting(UINT nButton);
										// Configuracion
	CButton* GetCorButton(UINT nComboBox);
										// �Ή�BotonesObtener
	CComboBox* GetCorCombo(UINT nButton);
										// �Ή�Cuadro combinadoObtener
	CInput *m_pInput;
										// CInput
	static UINT ControlTable[];
										// Tabla de controles
};

//===========================================================================
//
//	�W���C�X�e�B�b�NDialogo de detalles
//
//===========================================================================
class CJoyDetDlg : public CDialog
{
public:
	CJoyDetDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion

	CString m_strDesc;
										// �f�o�C�XNombre
	int m_nPort;
										// Numero de puerto (0 o 1)
	int m_nType;
										// Tipo (0-12)
};

//===========================================================================
//
//	BotonesConfiguracion�y�[�W
//
//===========================================================================
class CBtnSetPage : public CPropertyPage
{
public:
	CBtnSetPage();
										// Constructor
	void FASTCALL Init(CPropertySheet *pSheet);
										// Crear
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar
	void OnCancel();
										// Cancelar
	int m_nJoy;
										// Numero de joystick (0 o 1)
	int m_nType[PPI::PortMax];
										// �W���C�X�e�B�b�NTipo (0-12)

protected:
	afx_msg void OnPaint();
										// �_�C�A���ODibujar
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
#else
	afx_msg void OnTimer(UINT nTimerID);
#endif
										// Temporizador
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Desplazamiento horizontal
	BOOL OnCommand(WPARAM wParam, LPARAM lParam);
										// Notificacion de comando

private:
	enum CtrlType {
		BtnLabel,						// Etiqueta(Botonesn)
		BtnCombo,						// Cuadro combinado
		BtnRapid,						// Disparo rapido�X���C�_
		BtnValue						// Disparo rapidoEtiqueta
	};
	void FASTCALL OnDraw(CDC *pDC, BOOL *pButton, BOOL bForce);
										// Dibujo principal
	void FASTCALL OnSlider(int nButton);
										// �X���C�_Modificacion
	void FASTCALL OnSelChg(int nButton);
										// Cambio en el cuadro combinado
	void FASTCALL GetButtonDesc(const char *pszDesc, CString &strDesc);
										// BotonesMostrarObtener
	UINT FASTCALL GetControl(int nButton, CtrlType ctlType) const;
										// �R���g���[��Obtener ID
	CPropertySheet *m_pSheet;
										// Hoja padre
	CInput *m_pInput;
										// CInput
	CRect m_rectLabel[12];
										// Etiqueta�ʒu
	BOOL m_bButton[12];
										// Botones�����L��
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
#else
	UINT m_nTimerID;
#endif
										// ID del temporizador
	static const UINT ControlTable[];
										// Tabla de controles
	static const int RapidTable[];
										// Disparo rapido�e�[�u��

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	�W���C�X�e�B�b�NHoja de propiedades
//
//===========================================================================
class CJoySheet : public CPropertySheet
{
public:
	CJoySheet(CWnd *pParent);
										// Constructor
	void FASTCALL SetParam(int nJoy, int nCombo, int nType[]);
										// �p�����[�^Configuracion
	void FASTCALL InitSheet();
										// �V�[�gInicializacion
	int FASTCALL GetAxes() const;
										// Numero de ejesObtener
	int FASTCALL GetButtons() const;
										// Botones��Obtener

private:
	CBtnSetPage m_BtnSet;
										// BotonesConfiguracion�y�[�W
	CInput *m_pInput;
										// CInput
	int m_nJoy;
										// Numero de joystick (0 o 1)
	int m_nCombo;
										// Cuadro combinado�I��
	int m_nType[PPI::PortMax];
										// Seleccion de tipo del lado VM
	DIDEVCAPS m_DevCaps;
										// Caps del dispositivo
};

//===========================================================================
//
//	Pagina SASI
//
//===========================================================================
class CSASIPage : public CConfigPage
{
public:
	CSASIPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar
	BOOL OnSetActive();
										// Pagina activa
	int FASTCALL GetDrives(const Config *pConfig) const;
										// SASINumero de unidadesObtener

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Desplazamiento vertical
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic en columna

private:
	void FASTCALL UpdateList();
										// Actualizar control de lista
	void FASTCALL CheckSASI(DWORD *pDisk);
										// SASI�t�@�C��Verificacion
	void FASTCALL EnableControls(BOOL bEnable, BOOL bDrive = TRUE);
										// Cambio de estado de los controles
	SASI *m_pSASI;
										// SASI
	BOOL m_bInit;
										// Flag de inicializacion
	int m_nDrives;
										// Numero de unidades
	TCHAR m_szFile[16][FILEPATH_MAX];
										// Nombre del archivo de disco duro SASI
	CString m_strError;
										// Cadena de error

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina SxSI
//
//===========================================================================
class CSxSIPage : public CConfigPage
{
public:
	CSxSIPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	BOOL OnSetActive();
										// Pagina activa
	void OnOK();
										// Aceptar
	int FASTCALL GetDrives(const Config *pConfig) const;
										// SxSINumero de unidadesObtener

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Desplazamiento vertical
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic en columna
	afx_msg void OnCheck();
										// Verificacion�{�b�N�X�N���b�N

private:
	enum DevType {
		DevSASI,						// Disco duro SASI�h���C�u
		DevSCSI,						// Disco duro SCSI�h���C�u
		DevMO,							// Unidad MO SCSI
		DevInit,						// SCSI Iniciador (Host)
		DevNone							// Sin dispositivo
	};
	void FASTCALL UpdateList();
										// Actualizar control de lista
	void FASTCALL BuildMap();
										// Mapa de dispositivosCrear
	int FASTCALL CheckSCSI(int nDrive);
										// Dispositivo SCSIVerificacion
	void FASTCALL EnableControls(BOOL bEnable, BOOL bDrive = TRUE);
										// Cambio de estado de los controles
	BOOL m_bInit;
										// Flag de inicializacion
	int m_nSASIDrives;
										// SASINumero de unidades
	DevType m_DevMap[8];
										// Mapa de dispositivos
	TCHAR m_szFile[6][FILEPATH_MAX];
										// Nombre del archivo de disco duro SCSI
	CString m_strSASI;
										// Cadena HD SASI
	CString m_strMO;
										// Cadena MO SCSI
	CString m_strInit;
										// Cadena del iniciador
	CString m_strNone;
										// Cadena que indica n/a
	CString m_strError;
										// �f�o�C�XCadena de error
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina SCSI
//
//===========================================================================
class CSCSIPage : public CConfigPage
{
public:
	CSCSIPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar
	int FASTCALL GetInterface(const Config *pConfig) const;
										// Tipo de interfazObtener

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Desplazamiento vertical
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic en columna
	afx_msg void OnButton();
										// ���W�IBotones�I��
	afx_msg void OnCheck();
										// Verificacion�{�b�N�X�N���b�N

private:
	enum DevType {
		DevSCSI,						// Disco duro SCSI�h���C�u
		DevMO,							// Unidad MO SCSI
		DevCD,							// CD-ROM SCSI�h���C�u
		DevInit,						// SCSI Iniciador (Host)
		DevNone							// Sin dispositivo
	};
	int FASTCALL GetIfCtrl() const;
										// Tipo de interfazObtener(�R���g���[�����)
	BOOL FASTCALL CheckROM(int nType) const;
										// ROMVerificacion
	void FASTCALL UpdateList();
										// Actualizar control de lista
	void FASTCALL BuildMap();
										// Mapa de dispositivosCrear
	int FASTCALL CheckSCSI(int nDrive);
										// Dispositivo SCSIVerificacion
	void FASTCALL EnableControls(BOOL bEnable, BOOL bDrive = TRUE);
										// Cambio de estado de los controles
	SCSI *m_pSCSI;
										// Dispositivo SCSI
	BOOL m_bInit;
										// Flag de inicializacion
	int m_nDrives;
										// Numero de unidades
	BOOL m_bMOFirst;
										// Flag de primero MO
	DevType m_DevMap[8];
										// Mapa de dispositivos
	TCHAR m_szFile[5][FILEPATH_MAX];
										// Nombre del archivo de disco duro SCSI
	CString m_strMO;
										// Cadena MO SCSI
	CString m_strCD;
										// Cadena CD SCSI
	CString m_strInit;
										// Cadena del iniciador
	CString m_strNone;
										// Cadena que indica n/a
	CString m_strError;
										// �f�o�C�XCadena de error
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina de puertos
//
//===========================================================================
class CPortPage : public CConfigPage
{
public:
	CPortPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar
};

//===========================================================================
//
//	Pagina MIDI
//
//===========================================================================
class CMIDIPage : public CConfigPage
{
public:
	CMIDIPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar
	void OnCancel();
										// Cancelar

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Desplazamiento vertical
	afx_msg void OnBIDClick();
										// Clic en ID de placa

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Cambio de estado de los controles
	CMIDI *m_pMIDI;
										// Componente MIDI
	BOOL m_bEnableCtrl;
										// Estado de los controles
	int m_nInDelay;
										// Retraso de entrada (ms)
	int m_nOutDelay;
										// Retraso de salida (ms)
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina de modificaciones
//
//===========================================================================
class CAlterPage : public CConfigPage
{
public:
	CAlterPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	BOOL OnKillActive();
										// Moverse de pagina
	BOOL FASTCALL HasParity(const Config *pConfig) const;
										// SASI�p���e�B�@�\Verificacion

protected:
	void DoDataExchange(CDataExchange *pDX);
										// Intercambio de datos

private:
	BOOL m_bInit;
										// Inicializacion
	BOOL m_bParity;
										// Con paridad
};

//===========================================================================
//
//	Pagina de reanudacion (Resume)
//
//===========================================================================
class CResumePage : public CConfigPage
{
public:
	CResumePage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion

protected:
	void DoDataExchange(CDataExchange *pDX);
										// Intercambio de datos
};

//===========================================================================
//
//	TrueKeyDialogo de entrada
//
//===========================================================================
class CTKeyDlg : public CDialog
{
public:
	CTKeyDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// OK
	void OnCancel();
										// Cancelar
	int m_nTarget;
										// Tecla objetivo
	int m_nKey;
										// Asignacion�L�[

protected:
	afx_msg void OnPaint();
										// Dibujar
	afx_msg UINT OnGetDlgCode();
										// �_�C�A���O�R�[�hObtener
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
										// Temporizador
#else
	afx_msg void OnTimer(UINT nTimerID);
										// Temporizador
#endif
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
										// Clic derecho

private:
	void FASTCALL OnDraw(CDC *pDC);
										// Dibujo principal
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
										// ID del temporizador
#else
	UINT m_nTimerID;
										// ID del temporizador
#endif
	BYTE m_KeyState[0x100];
										// Estado de teclas VK
	CTKey *m_pTKey;
										// TrueKey
	CRect m_rectGuide;
										// Rectangulo guia
	CString m_strGuide;
										// Cadena guia
	CRect m_rectAssign;
										// �L�[Asignacion��`
	CString m_strAssign;
										// �L�[Asignacion������
	CRect m_rectKey;
										// Rectangulo de tecla
	CString m_strKey;
										// Cadena de tecla

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina TrueKey
//
//===========================================================================
class CTKeyPage : public CConfigPage
{
public:
	CTKeyPage();
										// Constructor
	BOOL OnInitDialog();
										// Inicializacion
	void OnOK();
										// Aceptar

protected:
	afx_msg void OnSelChange();
										// Cambio en el cuadro combinado
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic en columna
	afx_msg void OnRClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic derecho en columna

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Cambio de estado de los controles
	void FASTCALL UpdateReport();
										// ���|�[�gActualizacion
	BOOL m_bEnableCtrl;
										// Flag de activacion de controles
	CInput *m_pInput;
										// CInput
	CTKey *m_pTKey;
										// TrueKey
	int m_nKey[0x73];
										// Editar����Conversion�e�[�u��
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Pagina de miscelanea
//
//===========================================================================
class CMiscPage : public CConfigPage
{
public:
	CMiscPage();
	BOOL OnInitDialog();
	void OnBuscarFolder();
	void OnOK();	
	void DoDataExchange(CDataExchange *pDX);
										// Intercambio de datos

	int m_nRendererDefault;
										
DECLARE_MESSAGE_MAP()
};

//===========================================================================
//
//	ConfiguracionHoja de propiedades
//
//===========================================================================
class CConfigSheet : public CPropertySheet
{
public:
	CConfigSheet(CWnd *pParent);
										// Constructor
	Config *m_pConfig;
										// Datos de configuracion
	CConfigPage* FASTCALL SearchPage(DWORD dwID) const;
										// �y�[�WBusqueda

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// �E�B���h�ECrear
	afx_msg void OnDestroy();
										// �E�B���h�EEliminar
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
#else
	afx_msg void OnTimer(UINT nTimerID);
#endif
										// Temporizador

private:
	CFrmWnd *m_pFrmWnd;
										// Ventana de marco (Frame window)
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
#else
	UINT m_nTimerID;
#endif
										// ID del temporizador

	CBasicPage m_Basic;
										// Basico
	CSoundPage m_Sound;
										// Sonido
	CVolPage m_Vol;
										// Volumen
	CKbdPage m_Kbd;
										// Teclado
	CMousePage m_Mouse;
										// Mouse
	CJoyPage m_Joy;
										// Joystick
	CSASIPage m_SASI;
										// SASI
	CSxSIPage m_SxSI;
										// SxSI
	CSCSIPage m_SCSI;
										// SCSI
	CPortPage m_Port;
										// Puertos
	CMIDIPage m_MIDI;
										// MIDI
	CAlterPage m_Alter;
										// ����
	CResumePage m_Resume;
										// ���W���[��
	CTKeyPage m_TKey;
										// TrueKey
	CMiscPage m_Misc;
										// Otros

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

#endif	// mfc_cfg_h
#endif	// _WIN32
