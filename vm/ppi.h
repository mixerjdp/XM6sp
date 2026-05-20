//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 P.I. (ytanaka@ipc-tokai.or.jp)
//	[ PPI(i8255A) ]
//
//---------------------------------------------------------------------------

#if !defined(ppi_h)
#define ppi_h

#include "device.h"

//=========================================================================
//
//	PPI
//
//=========================================================================
class PPI : public MemDevice
{
public:
	// Constant definitions
	enum {
		PortMax = 2,					// Maximum number of ports
		AxisMax = 4,					// Maximum number of axes
		ButtonMax = 8					// Maximum number of buttons
	};

	// Joystick data definition
	typedef struct {
		DWORD axis[AxisMax];				// Axis info
		BOOL button[ButtonMax];				// Button info
	} joyinfo_t;

	// Internal data definition
	typedef struct {
		DWORD portc;					// Port C
		int type[PortMax];				// Joystick type
		DWORD ctl[PortMax];				// Joystickcontrol
		joyinfo_t info[PortMax];		// Joystickinfo
	} ppi_t;

public:
	// Core functions
	PPI(VM *p);
										// Constructor
	BOOL FASTCALL Init();
										// Initialize
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
	void FASTCALL ApplyCfg(const Config *config);
										// Apply settings
#if defined(_DEBUG)
	void FASTCALL AssertDiag() const;
										// Diagnostics
#endif	// _DEBUG

	// Memory device
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										// Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read-only

	// External API
	void FASTCALL GetPPI(ppi_t *buffer);
										// Get internal data
	void FASTCALL SetJoyInfo(int port, const joyinfo_t *info);
										// Set joystick info
	const joyinfo_t* FASTCALL GetJoyInfo(int port) const;
										// Get joystick info
	void FASTCALL SetJoyType(int port, int type);
								// set joystick type
	JoyDevice* FASTCALL CreateJoy(int port, int type);
										// Create joystick device

private:
	void FASTCALL SetPortC(DWORD data);
										// Set Port C
	ADPCM *adpcm;
										// ADPCM
	JoyDevice *joy[PortMax];
										// Joystick
	ppi_t ppi;
										// Internal data
};

//=========================================================================
//
//	Joystickdevice
//
//=========================================================================
class JoyDevice
{
public:
	// Core functions
	JoyDevice(PPI *parent, int no);
										// Constructor
	virtual ~JoyDevice();
										// Destructor
	DWORD FASTCALL GetID() const		{ return id; }
										// IDget
	DWORD FASTCALL GetType() const		{ return type; }
										// Get the type
	virtual void FASTCALL Reset();
										// Reset
	virtual BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	virtual BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load

	// Access
	virtual DWORD FASTCALL ReadPort(DWORD ctl);
										// Port read
	virtual DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	virtual void FASTCALL Control(DWORD ctl);
										// Control

	// Cache
	void FASTCALL Notify()				{ changed = TRUE; }
										// Notify a parent port change
	virtual void FASTCALL MakeData();
										// Build data

	// Properties
	int FASTCALL GetAxes() const		{ return axes; }
										// Get the axis count
	const char* FASTCALL GetAxisDesc(int axis) const;
										// Get the axis label
	int FASTCALL GetButtons() const		{ return buttons; }
										// Get the button count
	const char* FASTCALL GetButtonDesc(int button) const;
										// Get the button label
	BOOL FASTCALL IsAnalog() const		{ return analog; }
										// Get the analog/digital mode
	int FASTCALL GetDatas() const		{ return datas; }
										// Get the data count

protected:
	DWORD type;
										// Type
	DWORD id;
										// ID
	PPI *ppi;
										// PPI
	int port;
										// Port number
	int axes;
										// Axis count
	const char **axis_desc;
										// Axis label
	int buttons;
										// Button count
	const char **button_desc;
										// Button label
	BOOL analog;
										// Kind (analog/digital)
	DWORD *data;
										// Data payload
	int datas;
										// Data count
	BOOL changed;
										// Joystickchangenotify
};

//=========================================================================
//
//	Joystick (ATARI standard)
//
//=========================================================================
class JoyAtari : public JoyDevice
{
public:
	JoyAtari(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL MakeData();
										// Build data

private:
	static const char* AxisDescTable[];
										// Axis label table
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (ATARI standard + START/SELECT)
//
//=========================================================================
class JoyASS : public JoyDevice
{
public:
	JoyASS(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL MakeData();
										// Build data

private:
	static const char* AxisDescTable[];
										// Axis label table
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (Cyber Stick, analog)
//
//=========================================================================
class JoyCyberA : public JoyDevice
{
public:
	JoyCyberA(PPI *parent, int no);
										// Constructor

protected:
	void FASTCALL Reset();
										// Reset
	DWORD FASTCALL ReadPort(DWORD ctl);
										// Port read
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL Control(DWORD ctl);
										// Control
	void FASTCALL MakeData();
										// Build data
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load

private:
	DWORD seq;
										// Sequence
	DWORD ctrl;
										// Previous control value (0 or 1)
	DWORD hus;
										// Time used for evaluation
	Scheduler *scheduler;
										// Scheduler
	static const char* AxisDescTable[];
										// Axis label table
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (Cyber Stick, digital)
//
//=========================================================================
class JoyCyberD : public JoyDevice
{
public:
	JoyCyberD(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL MakeData();
										// Build data

private:
	static const char* AxisDescTable[];
										// Axis label table
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (MD3 buttons)
//
//=========================================================================
class JoyMd3 : public JoyDevice
{
public:
	JoyMd3(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL MakeData();
										// Build data

private:
	static const char* AxisDescTable[];
										// Axis label table
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (MD6 buttons)
//
//=========================================================================
class JoyMd6 : public JoyDevice
{
public:
	JoyMd6(PPI *parent, int no);
										// Constructor

protected:
	void FASTCALL Reset();
										// Reset
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL Control(DWORD ctl);
										// Control
	void FASTCALL MakeData();
										// Build data
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load

private:
	DWORD seq;
										// Sequence
	DWORD ctrl;
										// Previous control value (0 or 1)
	DWORD hus;
										// Time used for evaluation
	Scheduler *scheduler;
										// Scheduler
	static const char* AxisDescTable[];
										// Axis label table
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (CPSF-SFC)
//
//=========================================================================
class JoyCpsf : public JoyDevice
{
public:
	JoyCpsf(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL MakeData();
										// Build data

private:
	static const char* AxisDescTable[];
										// Axis label table
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (CPSF-MD)
//
//=========================================================================
class JoyCpsfMd : public JoyDevice
{
public:
	JoyCpsfMd(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL MakeData();
										// Build data

private:
	static const char* AxisDescTable[];
										// Axis label table
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (Magical Pad)
//
//=========================================================================
class JoyMagical : public JoyDevice
{
public:
	JoyMagical(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL MakeData();
										// Build data

private:
	static const char* AxisDescTable[];
										// Axis label table
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (XPD-1LR)
//
//=========================================================================
class JoyLR : public JoyDevice
{
public:
	JoyLR(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL MakeData();
										// Build data

private:
	static const char* AxisDescTable[];
										// Axis label table
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (Pac-Land dedicated pad)
//
//=========================================================================
class JoyPacl : public JoyDevice
{
public:
	JoyPacl(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL MakeData();
										// Build data

private:
	static const char* ButtonDescTable[];
										// Button label table
};

//=========================================================================
//
//	Joystick (BM68 dedicated controller)
//
//=========================================================================
class JoyBM : public JoyDevice
{
public:
	JoyBM(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read (read-only)
	void FASTCALL MakeData();
										// Build data

private:
	static const char* ButtonDescTable[];
										// Button label table
};

#endif	// ppi_h
