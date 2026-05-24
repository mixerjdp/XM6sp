//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ PPI(i8255A) ]
//
//---------------------------------------------------------------------------

#if !defined(ppi_h)
#define ppi_h

#include "device.h"

//===========================================================================
//
//	PPI
//
//===========================================================================
class PPI : public MemDevice
{
public:
	// Constants
	enum {
		PortMax = 2,					// Number of ports
		AxisMax = 4,					// Max axes
		ButtonMax = 8					// Max buttons
	};

	// Joystick device data structure
	typedef struct {
		DWORD axis[AxisMax];				// Axis
		BOOL button[ButtonMax];				// Button state
	} joyinfo_t;

	// Internal data structure
	typedef struct {
		DWORD portc;					// PortC
		int type[PortMax];				// Joystick device type
		DWORD ctl[PortMax];				// Joystick device controller
		joyinfo_t info[PortMax];		// Joystick state
	} ppi_t;

public:
	// Basic member function
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
										// Apply config
#if defined(_DEBUG)
	void FASTCALL AssertDiag() const;
										// Assert
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
										// Read only access

	// External API
	void FASTCALL GetPPI(ppi_t *buffer);
										// Get internal data
	void FASTCALL SetJoyInfo(int port, const joyinfo_t *info);
										// Joystick info set
	const joyinfo_t* FASTCALL GetJoyInfo(int port) const;
										// Joystick info get
	void FASTCALL SetJoyType(int port, int type);
								// set joystick type
	JoyDevice* FASTCALL CreateJoy(int port, int type);
										// Create joystick device

private:
	void FASTCALL SetPortC(DWORD data);
										// PortC set
	ADPCM *adpcm;
										// ADPCM
	JoyDevice *joy[PortMax];
										// Joystick
	ppi_t ppi;
										// Internal data
};

//===========================================================================
//
//	Joystick device
//
//===========================================================================
class JoyDevice
{
public:
	// Basic member function
	JoyDevice(PPI *parent, int no);
										// Constructor
	virtual ~JoyDevice();
										// Destructor
	DWORD FASTCALL GetID() const		{ return id; }
										// ID get
	DWORD FASTCALL GetType() const		{ return type; }
										// Type get
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
										// Port read(Read Only)
	virtual void FASTCALL Control(DWORD ctl);
										// Control

	// Update
	void FASTCALL Notify()				{ changed = TRUE; }
										// Notify port change
	virtual void FASTCALL MakeData();
										// Data create

	// Properties
	int FASTCALL GetAxes() const		{ return axes; }
										// Axis count get
	const char* FASTCALL GetAxisDesc(int axis) const;
										// Axis desc get
	int FASTCALL GetButtons() const		{ return buttons; }
										// Button count get
	const char* FASTCALL GetButtonDesc(int button) const;
										// Button desc get
	BOOL FASTCALL IsAnalog() const		{ return analog; }
										// Analog/Digital get
	int FASTCALL GetDatas() const		{ return datas; }
										// Data count get

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
										// Axis description
	int buttons;
										// Button count
	const char **button_desc;
										// Button description
	BOOL analog;
										// Analog(Analog/Digital)
	DWORD *data;
										// Data buffer
	int datas;
										// Data count
	BOOL changed;
										// Joystick change notification
};

//===========================================================================
//
//	Joystick device(ATARI standard)
//
//===========================================================================
class JoyAtari : public JoyDevice
{
public:
	JoyAtari(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL MakeData();
										// Data create

private:
	static const char* AxisDescTable[];
										// Axis description table
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(ATARI standard+START/SELECT)
//
//===========================================================================
class JoyASS : public JoyDevice
{
public:
	JoyASS(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL MakeData();
										// Data create

private:
	static const char* AxisDescTable[];
										// Axis description table
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(CyberAdaptor, analog)
//
//===========================================================================
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
										// Port read(Read Only)
	void FASTCALL Control(DWORD ctl);
										// Control
	void FASTCALL MakeData();
										// Data create
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load

private:
	DWORD seq;
										// Sequence
	DWORD ctrl;
										// External controller(0 or 1)
	DWORD hus;
										// Time used
	Scheduler *scheduler;
										// Scheduler
	static const char* AxisDescTable[];
										// Axis description table
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(CyberAdaptor, digital)
//
//===========================================================================
class JoyCyberD : public JoyDevice
{
public:
	JoyCyberD(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL MakeData();
										// Data create

private:
	static const char* AxisDescTable[];
										// Axis description table
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(MD3 buttons)
//
//===========================================================================
class JoyMd3 : public JoyDevice
{
public:
	JoyMd3(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL MakeData();
										// Data create

private:
	static const char* AxisDescTable[];
										// Axis description table
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(MD6 buttons)
//
//===========================================================================
class JoyMd6 : public JoyDevice
{
public:
	JoyMd6(PPI *parent, int no);
										// Constructor

protected:
	void FASTCALL Reset();
										// Reset
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL Control(DWORD ctl);
										// Control
	void FASTCALL MakeData();
										// Data create
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load

private:
	DWORD seq;
										// Sequence
	DWORD ctrl;
										// External controller(0 or 1)
	DWORD hus;
										// Time used
	Scheduler *scheduler;
										// Scheduler
	static const char* AxisDescTable[];
										// Axis description table
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(CPSF-SFC)
//
//===========================================================================
class JoyCpsf : public JoyDevice
{
public:
	JoyCpsf(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL MakeData();
										// Data create

private:
	static const char* AxisDescTable[];
										// Axis description table
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(CPSF-MD)
//
//===========================================================================
class JoyCpsfMd : public JoyDevice
{
public:
	JoyCpsfMd(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL MakeData();
										// Data create

private:
	static const char* AxisDescTable[];
										// Axis description table
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(Magical pad)
//
//===========================================================================
class JoyMagical : public JoyDevice
{
public:
	JoyMagical(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL MakeData();
										// Data create

private:
	static const char* AxisDescTable[];
										// Axis description table
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(XPD-1LR)
//
//===========================================================================
class JoyLR : public JoyDevice
{
public:
	JoyLR(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL MakeData();
										// Data create

private:
	static const char* AxisDescTable[];
										// Axis description table
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(Pacl stick dedicated pad)
//
//===========================================================================
class JoyPacl : public JoyDevice
{
public:
	JoyPacl(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL MakeData();
										// Data create

private:
	static const char* ButtonDescTable[];
										// Button description table
};

//===========================================================================
//
//	Joystick device(BM68 dedicated controler)
//
//===========================================================================
class JoyBM : public JoyDevice
{
public:
	JoyBM(PPI *parent, int no);
										// Constructor

protected:
	DWORD FASTCALL ReadOnly(DWORD ctl) const;
										// Port read(Read Only)
	void FASTCALL MakeData();
										// Data create

private:
	static const char* ButtonDescTable[];
										// Button description table
};

#endif	// ppi_h