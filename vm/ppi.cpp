//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ PPI(i8255A) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "adpcm.h"
#include "schedule.h"
#include "config.h"
#include "fileio.h"
#include "ppi.h"
#include "x68sound_bridge.h"

//===========================================================================
//
//	PPI
//
//===========================================================================
//#define PPI_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
PPI::PPI(VM *p) : MemDevice(p)
{
	int i;

	// Device ID initialization
	dev.id = MAKEID('P', 'P', 'I', ' ');
	dev.desc = "PPI (i8255A)";

	// Start address, End address
	memdev.first = 0xe9a000;
	memdev.last = 0xe9bfff;

	// Internal structure
	memset(&ppi, 0, sizeof(ppi));

	// Object
	adpcm = NULL;
	for (i=0; i<PortMax; i++) {
		joy[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL PPI::Init()
{
	int i;

 ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get ADPCM
 ASSERT(!adpcm);
	adpcm = (ADPCM*)vm->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
 ASSERT(adpcm);

	// Joystick device type
	for (i=0; i<PortMax; i++) {
		ppi.type[i] = 0;
 ASSERT(!joy[i]);
		joy[i] = new JoyDevice(this, i);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL PPI::Cleanup()
{
	int i;

 ASSERT(this);

	// Release joystick devices
	for (i=0; i<PortMax; i++) {
 ASSERT(joy[i]);
		delete joy[i];
		joy[i] = NULL;
	}
	 
	// To base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL PPI::Reset()
{
	int i;

 ASSERT(this);
 ASSERT_DIAG();

 LOG0(Log::Normal, "Reset");

	// PortC
	ppi.portc = 0;

	// Control
	for (i=0; i<PortMax; i++) {
		ppi.ctl[i] = 0;
	}

	// Send reset to joystick devices
	for (i=0; i<PortMax; i++) {
		joy[i]->Reset();
	}
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL PPI::Save(Fileio *fio, int ver)
{
	size_t sz;
	int i;
	DWORD type;

 ASSERT(this);
 ASSERT(fio);
 ASSERT(ver >= 0x0200);
 ASSERT_DIAG();

 LOG0(Log::Normal, "Save");

	// Save size
	sz = sizeof(ppi_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save actual data
	if (!fio->Write(&ppi, (int)sz)) {
		return FALSE;
	}

	// Save devices
	for (i=0; i<PortMax; i++) {
		// Device type
		type = joy[i]->GetType();
		if (!fio->Write(&type, sizeof(type))) {
			return FALSE;
		}

		// Device state
		if (!joy[i]->Save(fio, ver)) {
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL PPI::Load(Fileio *fio, int ver)
{
	size_t sz;
	int i;
	DWORD type;

 ASSERT(this);
 ASSERT(fio);
 ASSERT(ver >= 0x0200);
 ASSERT_DIAG();

 LOG0(Log::Normal, "Load");

	// Load size and verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(ppi_t)) {
		return FALSE;
	}

	// Load actual data
	if (!fio->Read(&ppi, (int)sz)) {
		return FALSE;
	}

	// version2.00 and later
	if (ver <= 0x200) {
		return TRUE;
	}

	// Load devices
	for (i=0; i<PortMax; i++) {
		// Get device type
		if (!fio->Read(&type, sizeof(type))) {
			return FALSE;
		}

		// If different from current device, recreate
		if (joy[i]->GetType() != type) {
			delete joy[i];
			joy[i] = NULL;

			// Also update type registered with PPI
			ppi.type[i] = (int)type;

			// Recreate
			joy[i] = CreateJoy(i, ppi.type[i]);
 ASSERT(joy[i]->GetType() == type);
		}

		// Device state
		if (!joy[i]->Load(fio, ver)) {
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL PPI::ApplyCfg(const Config *config)
{
	int i;

 ASSERT(this);
 ASSERT(config);
 ASSERT_DIAG();

 LOG0(Log::Normal, "Apply config");

	// Port loop
	for (i=0; i<PortMax; i++) {
		// If type matches, skip
		if (config->joy_type[i] == ppi.type[i]) {
			continue;
		}

		// Delete current device
 ASSERT(joy[i]);
		delete joy[i];
		joy[i] = NULL;

		// Save type, create joystick device
		ppi.type[i] = config->joy_type[i];
		joy[i] = CreateJoy(i, config->joy_type[i]);
	}
}

#if defined(_DEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void FASTCALL PPI::AssertDiag() const
{
 ASSERT(this);
 ASSERT(GetID() == MAKEID('P', 'P', 'I', ' '));
 ASSERT(adpcm);
 ASSERT(adpcm->GetID() == MAKEID('A', 'P', 'C', 'M'));
 ASSERT(joy[0]);
 ASSERT(joy[1]);
}
#endif	// _DEBUG

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL PPI::ReadByte(DWORD addr)
{
	DWORD data;

 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT(PortMax >= 2);
 ASSERT_DIAG();

	// Only even addresses are valid
	if ((addr & 1) == 0) {
		return 0xff;
	}

	// Mask to 8-bit
	addr &= 7;

	// Wait
	scheduler->Wait(1);

	// Read
	addr >>= 1;
	switch (addr) {
		// PortA
		case 0:
#if defined(PPI_LOG)
			data = joy[0]->ReadPort(ppi.ctl[0]);
			LOG2(Log::Normal, "Port1 read Control $%02X Data $%02X",
								ppi.ctl[0], data);
#else
			data = joy[0]->ReadPort(ppi.ctl[0]);
#endif	// PPI_LOG

			// Consider PC7,PC6
			if (ppi.ctl[0] & 0x80) {
				data &= ~0x40;
			}
			if (ppi.ctl[0] & 0x40) {
				data &= ~0x20;
			}
			return data;

		// PortB
		case 1:
#if defined(PPI_LOG)
			data = joy[1]->ReadPort(ppi.ctl[1]);
			LOG2(Log::Normal, "Port2 read Control $%02X Data $%02X",
								ppi.ctl[1], data);
			return data;
#else
			return joy[1]->ReadPort(ppi.ctl[1]);
#endif	// PPI_LOG

		// PortC
		case 2:
			return ppi.portc;
	}

	LOG1(Log::Warning, "Invalid I/O read R%02d", addr);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL PPI::ReadWord(DWORD addr)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT((addr & 1) == 0);
 ASSERT_DIAG();

	return (0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL PPI::WriteByte(DWORD addr, DWORD data)
{
	DWORD bit;
	int i;

 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT(data < 0x100);
 ASSERT_DIAG();

	// Only even addresses are valid
	if ((addr & 1) == 0) {
		return;
	}

	// Mask to 8-bit
	addr &= 7;

	// Wait
	scheduler->Wait(1);

	// Write to PortC
	if (addr == 5) {
		// Joystick device, ADPCM control
		SetPortC(data);
		return;
	}

	// Mode control
	if (addr == 7) {
		if (data < 0x80) {
			// Bit set/reset mode
			i = ((data >> 1) & 0x07);
			bit = (DWORD)(1 << i);
			if (data & 1) {
				SetPortC(DWORD(ppi.portc | bit));
			}
			else {
				SetPortC(DWORD(ppi.portc & ~bit));
			}
			return;
		}

		// Mode control
		if (data != 0x92) {
			LOG0(Log::Warning, "Unsupported mode select $%02X");
		}
		return;
	}

	LOG2(Log::Warning, "Invalid I/O write R%02d <- $%02X",
							addr, data);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL PPI::WriteWord(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT((addr & 1) == 0);
 ASSERT(data < 0x10000);
 ASSERT_DIAG();

	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//	Read only access
//
//---------------------------------------------------------------------------
DWORD FASTCALL PPI::ReadOnly(DWORD addr) const
{
	DWORD data;

 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT(PortMax >= 2);
 ASSERT_DIAG();

	// Only even addresses are valid
	if ((addr & 1) == 0) {
		return 0xff;
	}

	// Mask to 8-bit
	addr &= 7;

	// Read
	addr >>= 1;
	switch (addr) {
		// PortA
		case 0:
			// Get data
			data = joy[0]->ReadOnly(ppi.ctl[0]);

			// Consider PC7,PC6
			if (ppi.ctl[0] & 0x80) {
				data &= ~0x40;
			}
			if (ppi.ctl[0] & 0x40) {
				data &= ~0x20;
			}
			return data;

		// PortB
		case 1:
			return joy[1]->ReadOnly(ppi.ctl[1]);

		// PortC
		case 2:
			return ppi.portc;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	PortC set
//
//---------------------------------------------------------------------------
void FASTCALL PPI::SetPortC(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT(PortMax >= 2);
 ASSERT_DIAG();

	// Store data
	ppi.portc = data;
	static int portc_trace_count = 0;
	if (portc_trace_count < 24) {
		fprintf(stderr,
			"[vm][ppi-portc] n=%d data=$%02X ctl0=$%02X ctl1=$%02X pan=%u rate=%u\r\n",
			portc_trace_count,
			(unsigned)data,
			(unsigned)((ppi.portc & 0x03u)),
			(unsigned)((ppi.portc >> 2) & 0x03u),
			(unsigned)(ppi.portc & 3u),
			(unsigned)((ppi.portc >> 2) & 3u));
		++portc_trace_count;
	}

	// Control data merged(PortA)...bit0 is PC4, bit6,7 are PC6,7
	ppi.ctl[0] = ppi.portc & 0xc0;
	if (ppi.portc & 0x10) {
		ppi.ctl[0] |= 0x01;
	}
#if defined(PPI_LOG)
	LOG1(Log::Normal, "Port1 Control $%02X", ppi.ctl[0]);
#endif	// PPI_LOG
	joy[0]->Control(ppi.ctl[0]);

	// Control data merged(PortB)...bit0 is PC5
	if (ppi.portc & 0x20) {
		ppi.ctl[1] = 0x01;
	}
	else {
		ppi.ctl[1] = 0x00;
	}
#if defined(PPI_LOG)
	LOG1(Log::Normal, "Port2 Control $%02X", ppi.ctl[1]);
#endif	// PPI_LOG
	joy[1]->Control(ppi.ctl[1]);

	// ADPCM pan
	adpcm->SetPanpot(data & 3);

	// ADPCM speed ratio
	adpcm->SetRatio((data >> 2) & 3);
#if defined(XM6CORE_ENABLE_X68SOUND)
	Xm6X68Sound::WritePpi(static_cast<unsigned char>(data));
#endif
}

//---------------------------------------------------------------------------
//
//	Get internal data
//
//---------------------------------------------------------------------------
void FASTCALL PPI::GetPPI(ppi_t *buffer)
{
 ASSERT(this);
 ASSERT(buffer);
 ASSERT_DIAG();

	// Copy internal structure
	*buffer = ppi;
}

//---------------------------------------------------------------------------
//
//	Joystick device information setting
//
//---------------------------------------------------------------------------
void FASTCALL PPI::SetJoyInfo(int port, const joyinfo_t *info)
{
 ASSERT(this);
 ASSERT((port >= 0) && (port < PortMax));
 ASSERT(info);
 ASSERT(PortMax >= 2);
 ASSERT_DIAG();

	// If identical, do nothing
	if (memcmp(&ppi.info[port], info, sizeof(joyinfo_t)) == 0) {
		return;
	}

	// Save
	memcpy(&ppi.info[port], info, sizeof(joyinfo_t));

	// Notify joystick device for that port
 ASSERT(joy[port]);
	joy[port]->Notify();
}

//---------------------------------------------------------------------------
//
//	Joystick device information get
//
//---------------------------------------------------------------------------
const PPI::joyinfo_t* FASTCALL PPI::GetJoyInfo(int port) const
{
 ASSERT(this);
 ASSERT((port >= 0) && (port < PortMax));
 ASSERT(PortMax >= 2);
 ASSERT_DIAG();

	return &(ppi.info[port]);
}

//---------------------------------------------------------------------------
//
//	Joystick device type setting
//
//---------------------------------------------------------------------------
void FASTCALL PPI::SetJoyType(int port, int type)
{
 ASSERT(this);
 ASSERT((port >= 0) && (port < PortMax));
 ASSERT(type >= 0);
 ASSERT(PortMax >= 2);

	if (ppi.type[port] == type) {
		return;
	}

 ASSERT(joy[port]);
	delete joy[port];
	joy[port] = NULL;

	ppi.type[port] = type;
	joy[port] = CreateJoy(port, type);
}

//---------------------------------------------------------------------------
//
//	Set joystick type
//
//---------------------------------------------------------------------------
JoyDevice* FASTCALL PPI::CreateJoy(int port, int type)
{
 ASSERT(this);
 ASSERT(type >= 0);
 ASSERT((port >= 0) && (port < PortMax));
 ASSERT(PortMax >= 2);

	// Type match
	switch (type) {
		// None
		case 0:
			return new JoyDevice(this, port);

		// Atari standard
		case 1:
			return new JoyAtari(this, port);

		// Atari standard+START/SELCT
		case 2:
			return new JoyASS(this, port);

		// CyberAdaptor(analog)
		case 3:
			return new JoyCyberA(this, port);

		// CyberAdaptor(digital)
		case 4:
			return new JoyCyberD(this, port);

		// MD3 buttons
		case 5:
			return new JoyMd3(this, port);

		// MD6 buttons
		case 6:
			return new JoyMd6(this, port);

		// CPSF-SFC
		case 7:
			return new JoyCpsf(this, port);

		// CPSF-MD
		case 8:
			return new JoyCpsfMd(this, port);

		// Magical pad
		case 9:
			return new JoyMagical(this, port);

		// XPD-1LR
		case 10:
			return new JoyLR(this, port);

		// Pacl stick专用pad
		case 11:
			return new JoyPacl(this, port);

		// BM68 use controler
		case 12:
			return new JoyBM(this, port);

		// Others
		default:
		 ASSERT(FALSE);
			break;
	}

	// Normal, never reach here
	return new JoyDevice(this, port);
}

//===========================================================================
//
//	Joystick device
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyDevice::JoyDevice(PPI *parent, int no)
{
 ASSERT((no >= 0) || (no < PPI::PortMax));

	// Type NULL
	id = MAKEID('N', 'U', 'L', 'L');
	type = 0;

	// Parent device(PPI), port number setting
	ppi = parent;
	port = no;

	// No sticks, buttons, digital, data count 0
	axes = 0;
	buttons = 0;
	analog = FALSE;
	datas = 0;

	// Description
	axis_desc = NULL;
	button_desc = NULL;

	// Data buffer is NULL
	data = NULL;

	// Update check flag
	changed = TRUE;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
JoyDevice::~JoyDevice()
{
	// If data buffer exists, release
	if (data) {
		delete[] data;
		data = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL JoyDevice::Reset()
{
 ASSERT(this);
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyDevice::Save(Fileio *fio, int /*ver*/)
{
 ASSERT(this);
 ASSERT(fio);

	// If data count is 0, skip save
	if (datas <= 0) {
 ASSERT(datas == 0);
		return TRUE;
	}

	// Save only data count
	if (!fio->Write(data, sizeof(DWORD) * datas)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyDevice::Load(Fileio *fio, int /*ver*/)
{
 ASSERT(this);
 ASSERT(fio);

	// If data count is 0, skip load
	if (datas <= 0) {
 ASSERT(datas == 0);
		return TRUE;
	}

	// Update flag
	changed = TRUE;

	// Load actual data
	if (!fio->Read(data, sizeof(DWORD) * datas)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Port read
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyDevice::ReadPort(DWORD ctl)
{
 ASSERT(this);
 ASSERT((port >= 0) && (port < PPI::PortMax));
 ASSERT(ppi);
 ASSERT(ctl < 0x100);

	// Check update flag
	if (changed) {
		// Clear flag
		changed = FALSE;

		// Create data
		MakeData();
	}

	// Return same data as ReadOnly
	return ReadOnly(ctl);
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyDevice::ReadOnly(DWORD /*ctl*/) const
{
 ASSERT(this);

	// Not connected
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Control
//
//---------------------------------------------------------------------------
void FASTCALL JoyDevice::Control(DWORD /*ctl*/)
{
 ASSERT(this);
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyDevice::MakeData()
{
 ASSERT(this);
}

//---------------------------------------------------------------------------
//
//	Axis description
//
//---------------------------------------------------------------------------
const char* FASTCALL JoyDevice::GetAxisDesc(int axis) const
{
 ASSERT(this);
 ASSERT(axis >= 0);

	// If axis count exceeded, return NULL
	if (axis >= axes) {
		return NULL;
	}

	// If axis description table is NULL, return NULL
	if (!axis_desc) {
		return NULL;
	}

	// Return from axis description table
	return axis_desc[axis];
}

//---------------------------------------------------------------------------
//
//	Button description
//
//---------------------------------------------------------------------------
const char* FASTCALL JoyDevice::GetButtonDesc(int button) const
{
 ASSERT(this);
 ASSERT(button >= 0);

	// If button count exceeded, return NULL
	if (button >= buttons) {
		return NULL;
	}

	// If button description table is NULL, return NULL
	if (!button_desc) {
		return NULL;
	}

	// Return from button description table
	return button_desc[button];
}

//===========================================================================
//
//	Joystick device(ATARI standard)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyAtari::JoyAtari(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type ATAR
	id = MAKEID('A', 'T', 'A', 'R');
	type = 1;

	// 2 axes 2 buttons, data count 1
	axes = 2;
	buttons = 2;
	datas = 1;

	// Description table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyAtari::ReadOnly(DWORD ctl) const
{
 ASSERT(this);
 ASSERT(ctl < 0x100);

	// If PC4 is 1, return 0xff
	if (ctl & 1) {
		return 0xff;
	}

	// Return created data
	return data[0];
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyAtari::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// Button A
	if (info->button[0]) {
		data[0] &= ~0x40;
	}

	// Button B
	if (info->button[1]) {
		data[0] &= ~0x20;
	}
}

//---------------------------------------------------------------------------
//
//	Axis description table
//
//---------------------------------------------------------------------------
const char* JoyAtari::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyAtari::ButtonDescTable[] = {
	"A",
	"B"
};

//===========================================================================
//
//	Joystick device(ATARI standard+START/SELECT)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyASS::JoyASS(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type ATSS
	id = MAKEID('A', 'T', 'S', 'S');
	type = 2;

	// 2 axes 4 buttons, data count 1
	axes = 2;
	buttons = 4;
	datas = 1;

	// Description table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyASS::ReadOnly(DWORD ctl) const
{
 ASSERT(this);
 ASSERT(ctl < 0x100);

	// If PC4 is 1, return 0xff
	if (ctl & 1) {
		return 0xff;
	}

	// Return created data
	return data[0];
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyASS::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// Button A
	if (info->button[0]) {
		data[0] &= ~0x40;
	}

	// Button B
	if (info->button[1]) {
		data[0] &= ~0x20;
	}

	// START(Display as simultaneous left-right press)
	if (info->button[2]) {
		data[0] &= ~0x0c;
	}

	// SELECT(Display as simultaneous up-down press)
	if (info->button[3]) {
		data[0] &= ~0x03;
	}
}

//---------------------------------------------------------------------------
//
//	Axis description table
//
//---------------------------------------------------------------------------
const char* JoyASS::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyASS::ButtonDescTable[] = {
	"A",
	"B",
	"START",
	"SELECT"
};

//===========================================================================
//
//	Joystick device(CyberAdaptor, analog)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyCyberA::JoyCyberA(PPI *parent, int no) : JoyDevice(parent, no)
{
	int i;

	// Type CYBA
	id = MAKEID('C', 'Y', 'B', 'A');
	type = 3;

	// 3 axes 8 buttons, analog, data count 11
	axes = 3;
	buttons = 8;
	analog = TRUE;
	datas = 12;

	// Description table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	for (i=0; i<12; i++) {
		// ACK,L/H, buttons
		if (i & 1) {
			data[i] = 0xbf;
		}
		else {
			data[i] = 0x9f;
		}

		// Make trigger value 0x7f
		if ((i >= 2) && (i <= 5)) {
			// Convert analog data H to 7
			data[i] &= 0xf7;
		}
	}

	// Get scheduler
	scheduler = (Scheduler*)ppi->GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
 ASSERT(scheduler);

	// Auto reset(when controller is strangely swapped)
	Reset();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL JoyCyberA::Reset()
{
 ASSERT(this);
 ASSERT(scheduler);

	// Base class
	JoyDevice::Reset();

	// Sequence initialize
	seq = 0;

	// Control 0
	ctrl = 0;

	// Time retention
	hus = scheduler->GetTotalTime();
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyCyberA::Save(Fileio *fio, int ver)
{
 ASSERT(this);
 ASSERT(fio);

	// Base class
	if (!JoyDevice::Save(fio, ver)) {
		return FALSE;
	}

	// Save sequence
	if (!fio->Write(&seq, sizeof(seq))) {
		return FALSE;
	}

	// Save control
	if (!fio->Write(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// Save time
	if (!fio->Write(&hus, sizeof(hus))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyCyberA::Load(Fileio *fio, int ver)
{
 ASSERT(this);
 ASSERT(fio);

	// Base class
	if (!JoyDevice::Load(fio, ver)) {
		return FALSE;
	}

	// Load sequence
	if (!fio->Read(&seq, sizeof(seq))) {
		return FALSE;
	}

	// Load control
	if (!fio->Read(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// Load time
	if (!fio->Read(&hus, sizeof(hus))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Differential speed setting(Compared with other devices)
//	*Strictly, PC4 must first be held then PA4's b5->b6 transition must take 100us or more
//	  and there must be some input lag present, but this far no emulation
//
//---------------------------------------------------------------------------
#define JOYCYBERA_CYCLE		108

//---------------------------------------------------------------------------
//
//	Port read
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCyberA::ReadPort(DWORD ctl)
{
	DWORD diff;
	DWORD n;

	// Sequence 0 is invalid
	if (seq == 0) {
		return 0xff;
	}

	// Sequence 12 or higher is also invalid
	if (seq >= 13) {
		// Sequence 0
		seq = 0;
		return 0xff;
	}

	// Check update flag
	if (changed) {
		// Clear flag
		changed = FALSE;

		// Create data
		MakeData();
	}

 ASSERT((seq >= 1) && (seq <= 12));

	// Get difference
	diff = scheduler->GetTotalTime();
	diff -= hus;

	// Calculate from difference
	if (diff >= JOYCYBERA_CYCLE) {
		n = diff / JOYCYBERA_CYCLE;
		diff %= JOYCYBERA_CYCLE;

		// Reset sequence
		if ((seq & 1) == 0) {
			seq--;
		}
		// Advance by 2 units
		seq += (2 * n);

		// Adjust time
		hus += (JOYCYBERA_CYCLE * n);

		// +1
		if (diff >= (JOYCYBERA_CYCLE / 2)) {
			diff -= (JOYCYBERA_CYCLE / 2);
			seq++;
		}

		// Sequence overflow handling
		if (seq >= 13) {
			seq = 0;
			return 0xff;
		}
	}
	if (diff >= (JOYCYBERA_CYCLE / 2)) {
		// Later set
		if (seq & 1) {
			seq++;
		}
	}

	// Get data
	return ReadOnly(ctl);
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCyberA::ReadOnly(DWORD /*ctl*/) const
{
 ASSERT(this);

	// Sequence 0 is invalid
	if (seq == 0) {
		return 0xff;
	}

	// Sequence 12 or higher is also invalid
	if (seq >= 13) {
		return 0xff;
	}

	// Return data corresponding to sequence
 ASSERT((seq >= 1) && (seq <= 12));
	return data[seq - 1];
}

//---------------------------------------------------------------------------
//
//	Control
//
//---------------------------------------------------------------------------
void FASTCALL JoyCyberA::Control(DWORD ctl)
{
 ASSERT(this);
 ASSERT(ctl < 0x100);

	// Sequence 0(invalid) time and Sequence 11 or higher
	if ((seq == 0) || (seq >= 11)) {
		// 1->0, sequence start
		if (ctl) {
			// Became 1 this time
			ctrl = 1;
		}
		else {
			// Became 0 this time
			if (ctrl) {
				// 1->0 transition
				seq = 1;
				hus = scheduler->GetTotalTime();
			}
			ctrl = 0;
		}
		return;
	}

	// From sequence 1 or higher, only ACK valid
	ctrl = ctl;
	if (ctl) {
		return;
	}

	// Advance sequence by 2 units
	if ((seq & 1) == 0) {
		seq--;
	}
	seq += 2;

	// Retain time
	hus = scheduler->GetTotalTime();
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyCyberA::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

 ASSERT(this);
 ASSERT(ppi);

	// Get joystick device information
	info = ppi->GetJoyInfo(port);

	// data[0]:ButtonA,ButtonB,ButtonC,ButtonD
	data[0] |= 0x0f;
	if (info->button[0]) {
		data[0] &= ~0x08;
	}
	if (info->button[1]) {
		data[0] &= ~0x04;
	}
	if (info->button[2]) {
		data[0] &= ~0x02;
	}
	if (info->button[3]) {
		data[0] &= ~0x01;
	}

	// data[1]:ButtonE1,ButtonE2,ButtonF,ButtonG
	data[1] |= 0x0f;
	if (info->button[4]) {
		data[1] &= ~0x08;
	}
	if (info->button[5]) {
		data[1] &= ~0x04;
	}
	if (info->button[6]) {
		data[1] &= ~0x02;
	}
	if (info->button[7]) {
		data[1] &= ~0x01;
	}

	// data[2]:1H
	axis = info->axis[1];
	axis = (axis + 0x800) >> 4;
	data[2] &= 0xf0;
	data[2] |= (axis >> 4);

	// data[3]:2H
	axis = info->axis[0];
	axis = (axis + 0x800) >> 4;
	data[3] &= 0xf0;
	data[3] |= (axis >> 4);

	// data[4]:3H
	axis = info->axis[3];
	axis = (axis + 0x800) >> 4;
	data[4] &= 0xf0;
	data[4] |= (axis >> 4);

	// data[4]:4H(packed and zero by actual device)
	data[5] &= 0xf0;

	// data[6]:1L
	axis = info->axis[1];
	axis = (axis + 0x800) >> 4;
	data[6] &= 0xf0;
	data[6] |= (axis & 0x0f);

	// data[7]:2L
	axis = info->axis[0];
	axis = (axis + 0x800) >> 4;
	data[7] &= 0xf0;
	data[7] |= (axis & 0x0f);

	// data[8]:3L
	axis = info->axis[3];
	axis = (axis + 0x800) >> 4;
	data[8] &= 0xf0;
	data[8] |= (axis & 0x0f);

	// data[9]:4L(packed and zero by actual device)
	data[9] &= 0xf0;

	// data[10]:A,B,A',B'
	// A,B are digital buttons of player 1, A'B' are normal press buttons
	// Treat as digital buttons of player 1(Analog stick II)
	data[10] &= 0xf0;
	data[10] |= 0x0f;
	if (info->button[0]) {
		data[10] &= ~0x08;
	}
	if (info->button[1]) {
		data[10] &= ~0x04;
	}
}

//---------------------------------------------------------------------------
//
//	Axis description table
//
//---------------------------------------------------------------------------
const char* JoyCyberA::AxisDescTable[] = {
	"Stick X",
	"Stick Y",
	"Throttle"
};

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyCyberA::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"D",
	"E1",
	"E2",
	"START",
	"SELECT"
};

//===========================================================================
//
//	Joystick device(CyberAdaptor, digital)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyCyberD::JoyCyberD(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type CYBD
	id = MAKEID('C', 'Y', 'B', 'D');
	type = 4;

	// 3 axes 6 buttons, data count 2
	axes = 3;
	buttons = 6;
	datas = 2;

	// Description table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCyberD::ReadOnly(DWORD ctl) const
{
 ASSERT(this);
 ASSERT(ctl < 0x100);
 ASSERT(data[0] < 0x100);
 ASSERT(data[1] < 0x100);

	// Separated by PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyCyberD::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;
	data[1] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// Button A
	if (info->button[0]) {
		data[0] &= ~0x20;
	}

	// Button B
	if (info->button[1]) {
		data[0] &= ~0x40;
	}

	// Stick Up
	axis = info->axis[2];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x01;
	}
	// Stick Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x02;
	}

	// Button C
	if (info->button[2]) {
		data[1] &= ~0x04;
	}

	// Button D
	if (info->button[3]) {
		data[1] &= ~0x08;
	}

	// Button E1
	if (info->button[4]) {
		data[1] &= ~0x20;
	}

	// Button E2
	if (info->button[5]) {
		data[1] &= ~0x40;
	}
}

//---------------------------------------------------------------------------
//
//	Axis description table
//
//---------------------------------------------------------------------------
const char* JoyCyberD::AxisDescTable[] = {
	"X",
	"Y",
	"Throttle"
};

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyCyberD::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"D",
	"E1",
	"E2"
};

//===========================================================================
//
//	Joystick device(MD3 buttons)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyMd3::JoyMd3(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type MD3B
	id = MAKEID('M', 'D', '3', 'B');
	type = 5;

	// 2 axes 4 buttons, data count 2
	axes = 2;
	buttons = 4;
	datas = 2;

	// Description table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xf3;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyMd3::ReadOnly(DWORD ctl) const
{
 ASSERT(this);
 ASSERT(ctl < 0x100);
 ASSERT(data[0] < 0x100);
 ASSERT(data[1] < 0x100);

	// Separated by PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyMd3::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xf3;
	data[1] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x01;
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x02;
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x08;
	}

	// Button B
	if (info->button[1]) {
		data[1] &= ~0x20;
	}

	// Button C
	if (info->button[2]) {
		data[1] &= ~0x40;
	}

	// Button A
	if (info->button[0]) {
		data[0] &= ~0x20;
	}

	// Start button
	if (info->button[3]) {
		data[0] &= ~0x40;
	}
}

//---------------------------------------------------------------------------
//
//	Axis description table
//
//---------------------------------------------------------------------------
const char* JoyMd3::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyMd3::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"START"
};

//===========================================================================
//
//	Joystick device(MD6 buttons)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyMd6::JoyMd6(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type MD6B
	id = MAKEID('M', 'D', '6', 'B');
	type = 6;

	// 2 axes 8 buttons, data count 3
	axes = 2;
	buttons = 8;
	datas = 5;

	// Description table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xf3;
	data[1] = 0xff;
	data[2] = 0xf0;
	data[3] = 0xff;
	data[4] = 0xff;

	// Get scheduler
	scheduler = (Scheduler*)ppi->GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
 ASSERT(scheduler);

	// Auto reset(when controller is strangely swapped)
	Reset();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL JoyMd6::Reset()
{
 ASSERT(this);
 ASSERT(scheduler);

	// Base class
	JoyDevice::Reset();

	// Sequence, Control, Time initialize
	seq = 0;
	ctrl = 0;
	hus = scheduler->GetTotalTime();
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyMd6::Save(Fileio *fio, int ver)
{
 ASSERT(this);
 ASSERT(fio);

	// Base class
	if (!JoyDevice::Save(fio, ver)) {
		return FALSE;
	}

	// Save sequence
	if (!fio->Write(&seq, sizeof(seq))) {
		return FALSE;
	}

	// Save control
	if (!fio->Write(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// Save time
	if (!fio->Write(&hus, sizeof(hus))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyMd6::Load(Fileio *fio, int ver)
{
 ASSERT(this);
 ASSERT(fio);

	// Base class
	if (!JoyDevice::Load(fio, ver)) {
		return FALSE;
	}

	// Load sequence
	if (!fio->Read(&seq, sizeof(seq))) {
		return FALSE;
	}

	// Load control
	if (!fio->Read(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// Load time
	if (!fio->Read(&hus, sizeof(hus))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyMd6::ReadOnly(DWORD /*ctl*/) const
{
 ASSERT(this);
 ASSERT(data[0] < 0x100);
 ASSERT(data[1] < 0x100);
 ASSERT(data[2] < 0x100);
 ASSERT(data[3] < 0x100);
 ASSERT(data[4] < 0x100);

	// Sequence match
	switch (seq) {
		// Initial state CTL=0
		case 0:
			return data[0];

		// First round CTL=1
		case 1:
			return data[1];

		// First round CTL=0
		case 2:
			return data[0];

		// Second round CTL=1
		case 3:
			return data[1];

		// After 6B decided CTL=0
		case 4:
			return data[2];

		// After 6B decided CTL=1
		case 5:
			return data[3];

		// After 6B decided CTL=0
		case 6:
			return data[4];

		// After 6B decided CTL=1
		case 7:
			return data[1];

		// After 6B decided CTL=0
		case 8:
			return data[0];

		// Others(invalid)
		default:
		 ASSERT(FALSE);
			break;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Control
//
//---------------------------------------------------------------------------
void FASTCALL JoyMd6::Control(DWORD ctl)
{
	DWORD diff;

 ASSERT(this);
 ASSERT(ctl < 0x100);

	// Only bit0 needed
	ctl &= 0x01;

	// Always update
	ctrl = ctl;

	// If seq >= 3, check if 1.8ms(3600hus) has passed since last activation
	// If passed, reset to seq=0 or seq=1(4 button pad V4)
	if (seq >= 3) {
		diff = scheduler->GetTotalTime();
		diff -= hus;
		if (diff >= 3600) {
			// Reset
			if (ctl) {
				seq = 1;
				hus = scheduler->GetTotalTime();
			}
			else {
				seq = 0;
			}
			return;
		}
	}

	switch (seq) {
		// Sequence outside CTL=0
		case 0:
			// If 1, sequence 1 and time retention
			if (ctl) {
				seq = 1;
				hus = scheduler->GetTotalTime();
			}
			break;

		// After first 1 CTL=1
		case 1:
			// If 0, sequence 2
			if (!ctl) {
				seq = 2;
			}
			break;

		// After 1->0 CTL=0
		case 2:
			// If 1, time check
			if (ctl) {
				diff = scheduler->GetTotalTime();
				diff -= hus;
				if (diff <= 2200) {
					// If 1.1ms(2200hus) or less, go to next sequence(6B read)
					seq = 3;
				}
				else {
					// If timing is off, sequence 1 and time retention(3B read)
					seq = 1;
					hus = scheduler->GetTotalTime();
				}
			}
			break;

		// After 6B decided CTL=1
		case 3:
			if (!ctl) {
				seq = 4;
			}
			break;

		// After 6B decided CTL=0
		case 4:
			if (ctl) {
				seq = 5;
			}
			break;

		// After 6B decided CTL=1
		case 5:
			if (!ctl) {
				seq = 6;
			}
			break;

		// After 6B decided CTL=0
		case 6:
			if (ctl) {
				seq = 7;
			}
			break;

		// 1.8ms wait
		case 7:
			if (!ctl) {
				seq = 8;
			}
			break;

		// 1.8ms wait
		case 8:
			if (ctl) {
				seq = 7;
			}
			break;

		// Others(invalid)
		default:
		 ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyMd6::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xf3;
	data[1] = 0xff;
	data[2] = 0xf0;
	data[3] = 0xff;
	data[4] = 0xff;

	// Up(data[0], data[1], data[4])
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
		data[1] &= ~0x01;
		data[4] &= ~0x01;
	}
	// Down(data[0], data[1], data[4])
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
		data[1] &= ~0x02;
		data[4] &= ~0x02;
	}

	// Left(data[1], data[4])
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x04;
		data[4] &= ~0x04;
	}
	// Right(data[1], data[4])
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x08;
		data[4] &= ~0x08;
	}

	// Button B(data[1], data[3], data[4])
	if (info->button[1]) {
		// 3B punch
		data[1] &= ~0x20;

		// (4 button pad V4)
		data[3] &= ~0x20;

		// (SFII'patch)
		data[4] &= ~0x40;
	}

	// Button C(data[1], data[3])
	if (info->button[2]) {
		// 3B punch
		data[1] &= ~0x40;

		// (SFII'patch)
		data[3] &= ~0x20;

		// (4 button pad V4)
		data[3] &= ~0x40;
	}

	// Button A(data[0], data[2], data[4])
	if (info->button[0]) {
		// 3B punch
		data[0] &= ~0x20;

		// 6B punch
		data[2] &= ~0x20;

		// (SFII'patch)
		data[4] &= ~0x20;
	}

	// Start button(data[0], data[2])
	if (info->button[6]) {
		// 3B punch
		data[0] &= ~0x40;

		// 6B punch
		data[2] &= ~0x40;
	}

	// Button X(data[3])
	if (info->button[3]) {
		data[3] &= ~0x04;
	}

	// Button Y(data[3])
	if (info->button[4]) {
		data[3] &= ~0x02;
	}

	// Button Z(data[3])
	if (info->button[5]) {
		data[3] &= ~0x01;
	}

	// MODE button(data[3])
	if (info->button[7]) {
		data[3] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	Axis description table
//
//---------------------------------------------------------------------------
const char* JoyMd6::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyMd6::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"X",
	"Y",
	"Z",
	"START",
	"MODE"
};

//===========================================================================
//
//	Joystick device(CPSF-SFC)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyCpsf::JoyCpsf(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type CPSF
	id = MAKEID('C', 'P', 'S', 'F');
	type = 7;

	// 2 axes 8 buttons, data count 2
	axes = 2;
	buttons = 8;
	datas = 2;

	// Description table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCpsf::ReadOnly(DWORD ctl) const
{
 ASSERT(this);
 ASSERT(ctl < 0x100);
 ASSERT(data[0] < 0x100);
 ASSERT(data[1] < 0x100);

	// Separated by PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyCpsf::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;
	data[1] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}


	// Button Y
	if (info->button[0]) {
		data[1] &= ~0x02;
	}

	// Button X
	if (info->button[1]) {
		data[1] &= ~0x04;
	}

	// Button B
	if (info->button[2]) {
		data[0] &= ~0x40;
	}

	// Button A
	if (info->button[3]) {
		data[0] &= ~0x20;
	}

	// Button L
	if (info->button[4]) {
		data[1] &= ~0x20;
	}

	// Button R
	if (info->button[5]) {
		data[1] &= ~0x01;
	}


/* CPSFMD

// Button A
	if (info->button[0]) {
		data[0] &= ~0x20;
	}

// Button B
	if (info->button[1]) {
		data[0] &= ~0x40;
	}

// Button C
	if (info->button[2]) {
		data[1] &= ~0x20;
	}

// Button X
	if (info->button[3]) {
		data[1] &= ~0x04;
	}

// Button Y
	if (info->button[4]) {
		data[1] &= ~0x02;
	}

// Button Z
	if (info->button[5]) {
		data[1] &= ~0x01;
	}
	*/



	// Start button
	if (info->button[6]) {
		data[1] &= ~0x40;
	}

	// Select button
	if (info->button[7]) {
		data[1] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	Axis description table
//
//---------------------------------------------------------------------------
const char* JoyCpsf::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyCpsf::ButtonDescTable[] = {
	"Y",
	"X",
	"B",
	"A",
	"L",
	"R",
	"START",
	"SELECT",
	"ALT"
};

//===========================================================================
//
//	Joystick device(CPSF-MD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyCpsfMd::JoyCpsfMd(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type CPSM
	id = MAKEID('C', 'P', 'S', 'M');
	type = 8;

	// 2 axes 8 buttons, data count 2
	axes = 2;
	buttons = 8;
	datas = 2;

	// Description table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCpsfMd::ReadOnly(DWORD ctl) const
{
 ASSERT(this);
 ASSERT(ctl < 0x100);
 ASSERT(data[0] < 0x100);
 ASSERT(data[1] < 0x100);

	// Separated by PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyCpsfMd::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;
	data[1] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// Button A
	if (info->button[0]) {
		data[0] &= ~0x20;
	}

	// Button B
	if (info->button[1]) {
		data[0] &= ~0x40;
	}

	// Button C
	if (info->button[2]) {
		data[1] &= ~0x20;
	}

	// Button X
	if (info->button[3]) {
		data[1] &= ~0x04;
	}

	// Button Y
	if (info->button[4]) {
		data[1] &= ~0x02;
	}

	// Button Z
	if (info->button[5]) {
		data[1] &= ~0x01;
	}

	// Start button
	if (info->button[6]) {
		data[1] &= ~0x40;
	}

	// MODE button
	if (info->button[7]) {
		data[1] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	Axis description table
//
//---------------------------------------------------------------------------
const char* JoyCpsfMd::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyCpsfMd::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"X",
	"Y",
	"Z",
	"START",
	"MODE"
};

//===========================================================================
//
//	Joystick device(Magical pad)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyMagical::JoyMagical(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type MAGI
	id = MAKEID('M', 'A', 'G', 'I');
	type = 9;

	// 2 axes 6 buttons, data count 2
	axes = 2;
	buttons = 6;
	datas = 2;

	// Description table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xff;
	data[1] = 0xfc;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyMagical::ReadOnly(DWORD ctl) const
{
 ASSERT(this);
 ASSERT(ctl < 0x100);
 ASSERT(data[0] < 0x100);
 ASSERT(data[1] < 0x100);

	// Separated by PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyMagical::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;
	data[1] = 0xfc;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// Button A
	if (info->button[0]) {
		data[0] &= ~0x40;
	}

	// Button B
	if (info->button[1]) {
		data[1] &= ~0x40;
	}

	// Button C
	if (info->button[2]) {
		data[0] &= ~0x20;
	}

	// Button D
	if (info->button[3]) {
		data[1] &= ~0x40;
	}

	// Button R
	if (info->button[4]) {
		data[1] &= ~0x08;
	}

	// Button L
	if (info->button[5]) {
		data[1] &= ~0x04;
	}
}

//---------------------------------------------------------------------------
//
//	Axis description table
//
//---------------------------------------------------------------------------
const char* JoyMagical::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyMagical::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"D",
	"R",
	"L"
};

//===========================================================================
//
//	Joystick device(XPD-1LR)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyLR::JoyLR(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type XPLR
	id = MAKEID('X', 'P', 'L', 'R');
	type = 10;

	// 4 axes 2 buttons, data count 2
	axes = 4;
	buttons = 2;
	datas = 2;

	// Description table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyLR::ReadOnly(DWORD ctl) const
{
 ASSERT(this);
 ASSERT(ctl < 0x100);
 ASSERT(data[0] < 0x100);
 ASSERT(data[1] < 0x100);

	// Separated by PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyLR::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;
	data[1] = 0xff;

	// Right side Up
	axis = info->axis[3];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x01;
	}
	// Right side Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x02;
	}

	// Right side Left
	axis = info->axis[2];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x04;
	}
	// Right side Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x08;
	}

	// Button A
	if (info->button[0]) {
		data[1] &= ~0x40;
	}

	// Button B
	if (info->button[1]) {
		data[1] &= ~0x20;
	}

	// Left side Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Left side Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left side Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Left side Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// Button A
	if (info->button[0]) {
		data[0] &= ~0x40;
	}

	// Button B
	if (info->button[1]) {
		data[0] &= ~0x20;
	}
}

//---------------------------------------------------------------------------
//
//	Axis description table
//
//---------------------------------------------------------------------------
const char* JoyLR::AxisDescTable[] = {
	"Left-X",
	"Left-Y",
	"Right-X",
	"Right-Y"
};

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyLR::ButtonDescTable[] = {
	"A",
	"B"
};

//===========================================================================
//
//	Joystick device(Pacl stick dedicated pad)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyPacl::JoyPacl(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type PACL
	id = MAKEID('P', 'A', 'C', 'L');
	type = 11;

	// 0 axes 3 buttons, data count 1
	axes = 0;
	buttons = 3;
	datas = 1;

	// Description table
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyPacl::ReadOnly(DWORD ctl) const
{
 ASSERT(this);
 ASSERT(ctl < 0x100);
 ASSERT(data[0] < 0x100);

	// If PC4 is 1, return 0xff
	if (ctl & 1) {
		return 0xff;
	}

	// Return created data
	return data[0];
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyPacl::MakeData()
{
	const PPI::joyinfo_t *info;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;

	// Button A(Left)
	if (info->button[0]) {
		data[0] &= ~0x04;
	}

	// Button B(Jump)
	if (info->button[1]) {
		data[0] &= ~0x20;
	}

	// Button C(Right)
	if (info->button[2]) {
		data[0] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyPacl::ButtonDescTable[] = {
	"Left",
	"Jump",
	"Right",
};

//===========================================================================
//
//	Joystick device(BM68 dedicated controler)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
JoyBM::JoyBM(PPI *parent, int no) : JoyDevice(parent, no)
{
	// Type BM68
	id = MAKEID('B', 'M', '6', '8');
	type = 12;

	// 0 axes 6 buttons, data count 1
	axes = 0;
	buttons = 6;
	datas = 1;

	// Description table
	button_desc = ButtonDescTable;

	// Data buffer allocation
	data = new DWORD[datas];

	// Initial data set
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyBM::ReadOnly(DWORD ctl) const
{
 ASSERT(this);
 ASSERT(ctl < 0x100);
 ASSERT(data[0] < 0x100);

	// If PC4 is 1, return 0xff
	if (ctl & 1) {
		return 0xff;
	}

	// Return created data
	return data[0];
}

//---------------------------------------------------------------------------
//
//	Data create
//
//---------------------------------------------------------------------------
void FASTCALL JoyBM::MakeData()
{
	const PPI::joyinfo_t *info;

 ASSERT(this);
 ASSERT(ppi);

	// Initialize data
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;

	// Button 1(C)
	if (info->button[0]) {
		data[0] &= ~0x08;
	}

	// Button 2(C+,D-)
	if (info->button[1]) {
		data[0] &= ~0x04;
	}

	// Button 3(D)
	if (info->button[2]) {
		data[0] &= ~0x40;
	}

	// Button 4(D+,E-)
	if (info->button[3]) {
		data[0] &= ~0x20;
	}

	// Button 5(E)
	if (info->button[4]) {
		data[0] &= ~0x02;
	}

	// Button F(Hat)
	if (info->button[5]) {
		data[0] &= ~0x01;
	}
}

//---------------------------------------------------------------------------
//
//	Button description table
//
//---------------------------------------------------------------------------
const char* JoyBM::ButtonDescTable[] = {
	"C",
	"C#",
	"D",
	"D#",
	"E",
	"HiHat"
};