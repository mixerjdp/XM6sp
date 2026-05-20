//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 P.I. (ytanaka@ipc-tokai.or.jp)
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

//=========================================================================
//
//	PPI
//
//=========================================================================
//#define PPI_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
PPI::PPI(VM *p) : MemDevice(p)
{
	int i;

	// Initialize the device ID
	dev.id = MAKEID('P', 'P', 'I', ' ');
	dev.desc = "PPI (i8255A)";

	// Start and end addresses
	memdev.first = 0xe9a000;
	memdev.last = 0xe9bfff;

	// Internal state
	memset(&ppi, 0, sizeof(ppi));

	// Objects
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

	// Acquire ADPCM
	ASSERT(!adpcm);
	adpcm = (ADPCM*)vm->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
	ASSERT(adpcm);

	// Joystick type
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
	 
	// Return to the base class
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

	LOG0(Log::Normal, "ƒŠƒZƒbƒg");

	// Port C
	ppi.portc = 0;

	// Control
	for (i=0; i<PortMax; i++) {
		ppi.ctl[i] = 0;
	}

	// Notify the joystick devices of a reset
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

	LOG0(Log::Normal, "ƒZ[ƒu");

	// Save the size
	sz = sizeof(ppi_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save the payload
	if (!fio->Write(&ppi, (int)sz)) {
		return FALSE;
	}

	// Save the device
	for (i=0; i<PortMax; i++) {
		// Device type
		type = joy[i]->GetType();
		if (!fio->Write(&type, sizeof(type))) {
			return FALSE;
		}

		// Device payload
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

	LOG0(Log::Normal, "ƒ[ƒh");

	// Load and validate the size
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(ppi_t)) {
		return FALSE;
	}

	// Load the payload
	if (!fio->Read(&ppi, (int)sz)) {
		return FALSE;
	}

	// Up to here for version 2.00
	if (ver <= 0x200) {
		return TRUE;
	}

	// Load the device
	for (i=0; i<PortMax; i++) {
		// Get the device type
		if (!fio->Read(&type, sizeof(type))) {
			return FALSE;
		}

		// Recreate it if it does not match the current device
		if (joy[i]->GetType() != type) {
			delete joy[i];
			joy[i] = NULL;

			// Also update the type remembered by PPI
			ppi.type[i] = (int)type;

			// Recreate
			joy[i] = CreateJoy(i, ppi.type[i]);
			ASSERT(joy[i]->GetType() == type);
		}

		// Device-specific state
		if (!joy[i]->Load(fio, ver)) {
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL PPI::ApplyCfg(const Config *config)
{
	int i;

	ASSERT(this);
	ASSERT(config);
	ASSERT_DIAG();

	LOG0(Log::Normal, "Ý’è“K—p");

	// Port loop
	for (i=0; i<PortMax; i++) {
		// If the type matches, continue
		if (config->joy_type[i] == ppi.type[i]) {
			continue;
		}

		// Delete the current device
		ASSERT(joy[i]);
		delete joy[i];
		joy[i] = NULL;

		// Store the type and build the joystick
		ppi.type[i] = config->joy_type[i];
		joy[i] = CreateJoy(i, config->joy_type[i]);
	}
}

#if defined(_DEBUG)
//---------------------------------------------------------------------------
//
//	Diagnostics
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

	// Only odd addresses are decoded
	if ((addr & 1) == 0) {
		return 0xff;
	}

	// Mirror every 8 bytes
	addr &= 7;

	// Wait
	scheduler->Wait(1);

	// Decode
	addr >>= 1;
	switch (addr) {
		// Port A
		case 0:
#if defined(PPI_LOG)
			data = joy[0]->ReadPort(ppi.ctl[0]);
			LOG2(Log::Normal, "ƒ|[ƒg1“Ç‚Ýo‚µ ƒRƒ“ƒgƒ[ƒ‹$%02X ƒf[ƒ^$%02X",
								ppi.ctl[0], data);
#else
			data = joy[0]->ReadPort(ppi.ctl[0]);
#endif	// PPI_LOG

			// Account for PC7 and PC6
			if (ppi.ctl[0] & 0x80) {
				data &= ~0x40;
			}
			if (ppi.ctl[0] & 0x40) {
				data &= ~0x20;
			}
			return data;

		// Port B
		case 1:
#if defined(PPI_LOG)
			data = joy[1]->ReadPort(ppi.ctl[1]);
			LOG2(Log::Normal, "ƒ|[ƒg2“Ç‚Ýo‚µ ƒRƒ“ƒgƒ[ƒ‹$%02X ƒf[ƒ^$%02X",
								ppi.ctl[1], data);
			return data;
#else
			return joy[1]->ReadPort(ppi.ctl[1]);
#endif	// PPI_LOG

		// Port C
		case 2:
			return ppi.portc;
	}

	LOG1(Log::Warning, "–¢ŽÀ‘•ƒŒƒWƒXƒ^“Ç‚Ýž‚Ý R%02d", addr);
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

	// Only odd addresses are decoded
	if ((addr & 1) == 0) {
		return;
	}

	// Mirror every 8 bytes
	addr &= 7;

	// Wait
	scheduler->Wait(1);

	// Write to Port C
	if (addr == 5) {
		// Joystick/ADPCM control
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
			LOG0(Log::Warning, "ƒTƒ|[ƒgŠOƒ‚[ƒhŽw’è $%02X");
		}
		return;
	}

	LOG2(Log::Warning, "–¢ŽÀ‘•ƒŒƒWƒXƒ^‘‚«ž‚Ý R%02d <- $%02X",
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
//	Read-only
//
//---------------------------------------------------------------------------
DWORD FASTCALL PPI::ReadOnly(DWORD addr) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(PortMax >= 2);
	ASSERT_DIAG();

	// Only odd addresses are decoded
	if ((addr & 1) == 0) {
		return 0xff;
	}

	// Mirror every 8 bytes
	addr &= 7;

	// Decode
	addr >>= 1;
	switch (addr) {
		// Port A
		case 0:
			// Get data
			data = joy[0]->ReadOnly(ppi.ctl[0]);

			// Account for PC7 and PC6
			if (ppi.ctl[0] & 0x80) {
				data &= ~0x40;
			}
			if (ppi.ctl[0] & 0x40) {
				data &= ~0x20;
			}
			return data;

		// Port B
		case 1:
			return joy[1]->ReadOnly(ppi.ctl[1]);

		// Port C
		case 2:
			return ppi.portc;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Set Port C
//
//---------------------------------------------------------------------------
void FASTCALL PPI::SetPortC(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);
	ASSERT(PortMax >= 2);
	ASSERT_DIAG();

	// Store the data
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

	// Assemble control data (Port A): bit0 selects PC4, bits6-7 select PC6-7
	ppi.ctl[0] = ppi.portc & 0xc0;
	if (ppi.portc & 0x10) {
		ppi.ctl[0] |= 0x01;
	}
#if defined(PPI_LOG)
	LOG1(Log::Normal, "ƒ|[ƒg1 ƒRƒ“ƒgƒ[ƒ‹ $%02X", ppi.ctl[0]);
#endif	// PPI_LOG
	joy[0]->Control(ppi.ctl[0]);

	// Assemble control data (Port B): bit0 selects PC5
	if (ppi.portc & 0x20) {
		ppi.ctl[1] = 0x01;
	}
	else {
		ppi.ctl[1] = 0x00;
	}
#if defined(PPI_LOG)
	LOG1(Log::Normal, "ƒ|[ƒg2 ƒRƒ“ƒgƒ[ƒ‹ $%02X", ppi.ctl[1]);
#endif	// PPI_LOG
	joy[1]->Control(ppi.ctl[1]);

	// ADPCM pan pot
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

	// Copy the internal state
	*buffer = ppi;
}

//---------------------------------------------------------------------------
//
//	Set joystick info
//
//---------------------------------------------------------------------------
void FASTCALL PPI::SetJoyInfo(int port, const joyinfo_t *info)
{
	ASSERT(this);
	ASSERT((port >= 0) && (port < PortMax));
	ASSERT(info);
	ASSERT(PortMax >= 2);
	ASSERT_DIAG();

	// Compare and do nothing if unchanged
	if (memcmp(&ppi.info[port], info, sizeof(joyinfo_t)) == 0) {
		return;
	}

	// Save
	memcpy(&ppi.info[port], info, sizeof(joyinfo_t));

	// Notify the joystick device assigned to that port
	ASSERT(joy[port]);
	joy[port]->Notify();
}

//---------------------------------------------------------------------------
//
//	Get joystick info
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
//	Create joystick device
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

	// By type
	switch (type) {
		// No connection
		case 0:
			return new JoyDevice(this, port);

		// ATARI standard
		case 1:
			return new JoyAtari(this, port);

		// ATARI standard + START/SELECT
		case 2:
			return new JoyASS(this, port);

		// Cyber Stick (analog)
		case 3:
			return new JoyCyberA(this, port);

		// Cyber Stick (digital)
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

		// Magical Pad
		case 9:
			return new JoyMagical(this, port);

		// XPD-1LR
		case 10:
			return new JoyLR(this, port);

		// Pac-Land dedicated pad
		case 11:
			return new JoyPacl(this, port);

		// BM68 dedicated controller
		case 12:
			return new JoyBM(this, port);

		// other
		default:
			ASSERT(FALSE);
			break;
	}

	// Normally, execution should never reach here
	return new JoyDevice(this, port);
}

//=========================================================================
//
//	Joystickdevice
//
//=========================================================================

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

	// Store the parent device (PPI) and set the port number
	ppi = parent;
	port = no;

	// No axes/buttons, digital, data count 0
	axes = 0;
	buttons = 0;
	analog = FALSE;
	datas = 0;

	// Display
	axis_desc = NULL;
	button_desc = NULL;

	// The data buffer is NULL
	data = NULL;

	// Update check required
	changed = TRUE;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
JoyDevice::~JoyDevice()
{
	// Free the data buffer if allocated
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

	// Do not save when the data count is 0
	if (datas <= 0) {
		ASSERT(datas == 0);
		return TRUE;
	}

	// Save only the data count
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

	// Do not load when the data count is 0
	if (datas <= 0) {
		ASSERT(datas == 0);
		return TRUE;
	}

	// Marked for update
	changed = TRUE;

	// Load the data payload
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

	// Check the change flag
	if (changed) {
		// Clear the flag
		changed = FALSE;

		// Build data
		MakeData();
	}

	// Return the same data as ReadOnly
	return ReadOnly(ctl);
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyDevice::ReadOnly(DWORD /*ctl*/) const
{
	ASSERT(this);

	// Disconnected
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
//	Build data
//
//---------------------------------------------------------------------------
void FASTCALL JoyDevice::MakeData()
{
	ASSERT(this);
}

//---------------------------------------------------------------------------
//
//	Axis label
//
//---------------------------------------------------------------------------
const char* FASTCALL JoyDevice::GetAxisDesc(int axis) const
{
	ASSERT(this);
	ASSERT(axis >= 0);

	// Return NULL if the axis index is out of range
	if (axis >= axes) {
		return NULL;
	}

	// Return NULL if there is no axis label table
	if (!axis_desc) {
		return NULL;
	}

	// Return the value from the axis label table
	return axis_desc[axis];
}

//---------------------------------------------------------------------------
//
//	Button label
//
//---------------------------------------------------------------------------
const char* FASTCALL JoyDevice::GetButtonDesc(int button) const
{
	ASSERT(this);
	ASSERT(button >= 0);

	// Return NULL if the button index is out of range
	if (button >= buttons) {
		return NULL;
	}

	// Return NULL if there is no button label table
	if (!button_desc) {
		return NULL;
	}

	// Return the value from the button label table
	return button_desc[button];
}

//=========================================================================
//
//	Joystick (ATARI standard)
//
//=========================================================================

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

	// 2 axes, 2 buttons, data count 1
	axes = 2;
	buttons = 2;
	datas = 1;

	// Display table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
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

	// Return the prebuilt data
	return data[0];
}

//---------------------------------------------------------------------------
//
//	Build data
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
//	Axis label table
//
//---------------------------------------------------------------------------
const char* JoyAtari::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button label table
//
//---------------------------------------------------------------------------
const char* JoyAtari::ButtonDescTable[] = {
	"A",
	"B"
};

//=========================================================================
//
//	Joystick (ATARI standard + START/SELECT)
//
//=========================================================================

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

	// 2 axes, 4 buttons, data count 1
	axes = 2;
	buttons = 4;
	datas = 1;

	// Display table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
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

	// Return the prebuilt data
	return data[0];
}

//---------------------------------------------------------------------------
//
//	Build data
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

	// START (represented as Left+Right pressed together)
	if (info->button[2]) {
		data[0] &= ~0x0c;
	}

	// SELECT (represented as Up+Down pressed together)
	if (info->button[3]) {
		data[0] &= ~0x03;
	}
}

//---------------------------------------------------------------------------
//
//	Axis label table
//
//---------------------------------------------------------------------------
const char* JoyASS::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button label table
//
//---------------------------------------------------------------------------
const char* JoyASS::ButtonDescTable[] = {
	"A",
	"B",
	"START",
	"SELECT"
};

//=========================================================================
//
//	Joystick (Cyber Stick, analog)
//
//=========================================================================

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

	// 3 axes, 8 buttons, analog, data count 11
	axes = 3;
	buttons = 8;
	analog = TRUE;
	datas = 12;

	// Display table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	for (i=0; i<12; i++) {
		// ACK, L/H, buttons
		if (i & 1) {
			data[i] = 0xbf;
		}
		else {
			data[i] = 0x9f;
		}

		// Use 0x7f as the center value
		if ((i >= 2) && (i <= 5)) {
			// Set analog data H to 7
			data[i] &= 0xf7;
		}
	}

	// Get the scheduler
	scheduler = (Scheduler*)ppi->GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(scheduler);

	// Auto-reset (to handle controller hot-swapping)
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

	// Initialize the sequence
	seq = 0;

	// Control = 0
	ctrl = 0;

	// Store the time
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

	// Save the sequence
	if (!fio->Write(&seq, sizeof(seq))) {
		return FALSE;
	}

	// Save the control state
	if (!fio->Write(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// Save the time
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

	// Load the sequence
	if (!fio->Read(&seq, sizeof(seq))) {
		return FALSE;
	}

	// Load the control state
	if (!fio->Read(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// Load the time
	if (!fio->Read(&hus, sizeof(hus))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Set the response speed (based on real hardware comparison)
//	More precisely, after PC4 is raised first, there appears to be at least 100 us before PA4 b5->b6 falls
//	but this level of timing nuance is not emulated
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

	// Sequence 12 or later is invalid
	if (seq >= 13) {
		// Sequence 0
		seq = 0;
		return 0xff;
	}

	// Check the change flag
	if (changed) {
		// Clear the flag
		changed = FALSE;

		// Build data
		MakeData();
	}

	ASSERT((seq >= 1) && (seq <= 12));

	// Get the delta
	diff = scheduler->GetTotalTime();
	diff -= hus;

	// Calculate from the delta
	if (diff >= JOYCYBERA_CYCLE) {
		n = diff / JOYCYBERA_CYCLE;
		diff %= JOYCYBERA_CYCLE;

		// Reset the sequence
		if ((seq & 1) == 0) {
			seq--;
		}
		// Advance in steps of 2
		seq += (2 * n);

		// Adjust the time
		hus += (JOYCYBERA_CYCLE * n);

		// +1
		if (diff >= (JOYCYBERA_CYCLE / 2)) {
			diff -= (JOYCYBERA_CYCLE / 2);
			seq++;
		}

		// Sequence overflow guard
		if (seq >= 13) {
			seq = 0;
			return 0xff;
		}
	}
	if (diff >= (JOYCYBERA_CYCLE / 2)) {
		// Set the latter half
		if (seq & 1) {
			seq++;
		}
	}

	// Get data
	return ReadOnly(ctl);
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCyberA::ReadOnly(DWORD /*ctl*/) const
{
	ASSERT(this);

	// Sequence 0 is invalid
	if (seq == 0) {
		return 0xff;
	}

	// Sequence 12 or later is invalid
	if (seq >= 13) {
		return 0xff;
	}

	// Return data for the current sequence
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

	// Sequence 0 (invalid) and sequence 11 or later
	if ((seq == 0) || (seq >= 11)) {
		// Start the sequence on a 1->0 transition
		if (ctl) {
			// Changed it to 1 this time
			ctrl = 1;
		}
		else {
			// Changed it to 0 this time
			if (ctrl) {
				// Falling edge from 1 to 0
				seq = 1;
				hus = scheduler->GetTotalTime();
			}
			ctrl = 0;
		}
		return;
	}

	// From sequence 1 onward, only ACK is valid
	ctrl = ctl;
	if (ctl) {
		return;
	}

	// Has the effect of advancing the sequence by 2 steps
	if ((seq & 1) == 0) {
		seq--;
	}
	seq += 2;

	// Remember the time
	hus = scheduler->GetTotalTime();
}

//---------------------------------------------------------------------------
//
//	Build data
//
//---------------------------------------------------------------------------
void FASTCALL JoyCyberA::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// Get joystick info
	info = ppi->GetJoyInfo(port);

	// data[0]: Button A, Button B, Button C, Button D
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

	// data[1]: Button E1, Button E2, Button F, Button G
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

	// data[2]: 1H
	axis = info->axis[1];
	axis = (axis + 0x800) >> 4;
	data[2] &= 0xf0;
	data[2] |= (axis >> 4);

	// data[3]: 2H
	axis = info->axis[0];
	axis = (axis + 0x800) >> 4;
	data[3] &= 0xf0;
	data[3] |= (axis >> 4);

	// data[4]: 3H
	axis = info->axis[3];
	axis = (axis + 0x800) >> 4;
	data[4] &= 0xf0;
	data[4] |= (axis >> 4);

	// data[5]: 4H (reserved; 0 on real hardware)
	data[5] &= 0xf0;

	// data[6]: 1L
	axis = info->axis[1];
	axis = (axis + 0x800) >> 4;
	data[6] &= 0xf0;
	data[6] |= (axis & 0x0f);

	// data[7]: 2L
	axis = info->axis[0];
	axis = (axis + 0x800) >> 4;
	data[7] &= 0xf0;
	data[7] |= (axis & 0x0f);

	// data[8]: 3L
	axis = info->axis[3];
	axis = (axis + 0x800) >> 4;
	data[8] &= 0xf0;
	data[8] |= (axis & 0x0f);

	// data[9]: 4L (reserved; 0 on real hardware)
	data[9] &= 0xf0;

	// data[10]: A,B,A',B'
	// A and B are lever-integrated mini-buttons; A' and B' are normal push buttons
	// Treat them as lever-integrated mini-buttons (After Burner II)
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
//	Axis label table
//
//---------------------------------------------------------------------------
const char* JoyCyberA::AxisDescTable[] = {
	"Stick X",
	"Stick Y",
	"Throttle"
};

//---------------------------------------------------------------------------
//
//	Button label table
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

//=========================================================================
//
//	Joystick (Cyber Stick, digital)
//
//=========================================================================

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

	// 3 axes, 6 buttons, data count 2
	axes = 3;
	buttons = 6;
	datas = 2;

	// Display table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCyberD::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// Branch according to PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Build data
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

	// Throttle Up
	axis = info->axis[2];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x01;
	}
	// Throttle Down
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
//	Axis label table
//
//---------------------------------------------------------------------------
const char* JoyCyberD::AxisDescTable[] = {
	"X",
	"Y",
	"Throttle"
};

//---------------------------------------------------------------------------
//
//	Button label table
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

//=========================================================================
//
//	Joystick (MD3 buttons)
//
//=========================================================================

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

	// 2 axes, 4 buttons, data count 2
	axes = 2;
	buttons = 4;
	datas = 2;

	// Display table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xf3;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyMd3::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// Branch according to PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Build data
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
//	Axis label table
//
//---------------------------------------------------------------------------
const char* JoyMd3::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button label table
//
//---------------------------------------------------------------------------
const char* JoyMd3::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"START"
};

//=========================================================================
//
//	Joystick (MD6 buttons)
//
//=========================================================================

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

	// 2 axes, 8 buttons, data count 3
	axes = 2;
	buttons = 8;
	datas = 5;

	// Display table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xf3;
	data[1] = 0xff;
	data[2] = 0xf0;
	data[3] = 0xff;
	data[4] = 0xff;

	// Get the scheduler
	scheduler = (Scheduler*)ppi->GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(scheduler);

	// Auto-reset (to handle controller hot-swapping)
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

	// Initialize the sequence, control, and time
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

	// Save the sequence
	if (!fio->Write(&seq, sizeof(seq))) {
		return FALSE;
	}

	// Save the control state
	if (!fio->Write(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// Save the time
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

	// Load the sequence
	if (!fio->Read(&seq, sizeof(seq))) {
		return FALSE;
	}

	// Load the control state
	if (!fio->Read(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// Load the time
	if (!fio->Read(&hus, sizeof(hus))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
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

	// By sequence
	switch (seq) {
		// Initial state, CTL=0
		case 0:
			return data[0];

		// First cycle, CTL=1
		case 1:
			return data[1];

		// First cycle, CTL=0
		case 2:
			return data[0];

		// Second cycle, CTL=1
		case 3:
			return data[1];

		// After 6-button detection, CTL=0
		case 4:
			return data[2];

		// After 6-button detection, CTL=1
		case 5:
			return data[3];

		// After 6-button detection, CTL=0
		case 6:
			return data[4];

		// After 6-button detection, CTL=1
		case 7:
			return data[1];

		// After 6-button detection, CTL=0
		case 8:
			return data[0];

		// Other (should never happen)
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

	// Only bit0 is needed
	ctl &= 0x01;

	// Always update
	ctrl = ctl;

	// If seq >= 3, check whether 1.8 ms (3600 hus) has elapsed since the previous activation
	// If so, reset to seq=0 or seq=1 (Jotei Senki V4)
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
		// Out-of-sequence, CTL=0
		case 0:
			// If 1, move to sequence 1 and store the time
			if (ctl) {
				seq = 1;
				hus = scheduler->GetTotalTime();
			}
			break;

		// After the first 1, CTL=1
		case 1:
			// If 0, move to sequence 2
			if (!ctl) {
				seq = 2;
			}
			break;

		// After 1->0, CTL=0
		case 2:
			// If 1, check the elapsed time
			if (ctl) {
				diff = scheduler->GetTotalTime();
				diff -= hus;
				if (diff <= 2200) {
					// If 1.1 ms (2200 hus) or less, advance to the next sequence (6-button read)
					seq = 3;
				}
				else {
					// If enough time has passed, treat it the same as sequence 1 (3-button read)
					seq = 1;
					hus = scheduler->GetTotalTime();
				}
			}
			break;

		// After 6-button detection, CTL=1
		case 3:
			if (!ctl) {
				seq = 4;
			}
			break;

		// After 6-button detection, CTL=0
		case 4:
			if (ctl) {
				seq = 5;
			}
			break;

		// After 6-button detection, CTL=1
		case 5:
			if (!ctl) {
				seq = 6;
			}
			break;

		// After 6-button detection, CTL=0
		case 6:
			if (ctl) {
				seq = 7;
			}
			break;

		// Wait 1.8 ms
		case 7:
			if (!ctl) {
				seq = 8;
			}
			break;

		// Wait 1.8 ms
		case 8:
			if (ctl) {
				seq = 7;
			}
			break;

		// Other (should never happen)
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Build data
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

	// Button B (data[1], data[3], data[4])
	if (info->button[1]) {
		// 3-button compatibility
		data[1] &= ~0x20;

		// (Jotei Senki V4)
		data[3] &= ~0x20;

		// (SFII' patch)
		data[4] &= ~0x40;
	}

	// Button C (data[1], data[3])
	if (info->button[2]) {
		// 3-button compatibility
		data[1] &= ~0x40;

		// (SFII' patch)
		data[3] &= ~0x20;

		// (Jotei Senki V4)
		data[3] &= ~0x40;
	}

	// Button A (data[0], data[2], data[4])
	if (info->button[0]) {
		// 3-button compatibility
		data[0] &= ~0x20;

		// 6-button marker
		data[2] &= ~0x20;

		// (SFII' patch)
		data[4] &= ~0x20;
	}

	// Start button (data[0], data[2])
	if (info->button[6]) {
		// 3-button compatibility
		data[0] &= ~0x40;

		// 6-button marker
		data[2] &= ~0x40;
	}

	// Button X (data[3])
	if (info->button[3]) {
		data[3] &= ~0x04;
	}

	// Button Y (data[3])
	if (info->button[4]) {
		data[3] &= ~0x02;
	}

	// Button Z (data[3])
	if (info->button[5]) {
		data[3] &= ~0x01;
	}

	// MODE button (data[3])
	if (info->button[7]) {
		data[3] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	Axis label table
//
//---------------------------------------------------------------------------
const char* JoyMd6::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button label table
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

//=========================================================================
//
//	Joystick (CPSF-SFC)
//
//=========================================================================

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

	// 2 axes, 8 buttons, data count 2
	axes = 2;
	buttons = 8;
	datas = 2;

	// Display table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCpsf::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// Branch according to PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Build data
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

	// SELECT button
	if (info->button[7]) {
		data[1] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	Axis label table
//
//---------------------------------------------------------------------------
const char* JoyCpsf::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button label table
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

//=========================================================================
//
//	Joystick (CPSF-MD)
//
//=========================================================================

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

	// 2 axes, 8 buttons, data count 2
	axes = 2;
	buttons = 8;
	datas = 2;

	// Display table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCpsfMd::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// Branch according to PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Build data
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
//	Axis label table
//
//---------------------------------------------------------------------------
const char* JoyCpsfMd::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button label table
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

//=========================================================================
//
//	Joystick (Magical Pad)
//
//=========================================================================

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

	// 2 axes, 6 buttons, data count 2
	axes = 2;
	buttons = 6;
	datas = 2;

	// Display table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xff;
	data[1] = 0xfc;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyMagical::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// Branch according to PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Build data
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
//	Axis label table
//
//---------------------------------------------------------------------------
const char* JoyMagical::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	Button label table
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

//=========================================================================
//
//	Joystick (XPD-1LR)
//
//=========================================================================

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

	// 4 axes, 2 buttons, data count 2
	axes = 4;
	buttons = 2;
	datas = 2;

	// Display table
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyLR::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// Branch according to PC4
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	Build data
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

	// Right side Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right side Right
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
//	Axis label table
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
//	Button label table
//
//---------------------------------------------------------------------------
const char* JoyLR::ButtonDescTable[] = {
	"A",
	"B"
};

//=========================================================================
//
//	Joystick (Pac-Land dedicated pad)
//
//=========================================================================

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

	// 0 axes, 3 buttons, data count 1
	axes = 0;
	buttons = 3;
	datas = 1;

	// Display table
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
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

	// Return the prebuilt data
	return data[0];
}

//---------------------------------------------------------------------------
//
//	Build data
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

	// Button A (Left)
	if (info->button[0]) {
		data[0] &= ~0x04;
	}

	// Button B (Jump)
	if (info->button[1]) {
		data[0] &= ~0x20;
	}

	// Button C (Right)
	if (info->button[2]) {
		data[0] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	Button label table
//
//---------------------------------------------------------------------------
const char* JoyPacl::ButtonDescTable[] = {
	"Left",
	"Jump",
	"Right",
};

//=========================================================================
//
//	Joystick (BM68 dedicated controller)
//
//=========================================================================

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

	// 0 axes, 6 buttons, data count 1
	axes = 0;
	buttons = 6;
	datas = 1;

	// Display table
	button_desc = ButtonDescTable;

	// Allocate data buffer
	data = new DWORD[datas];

	// Set the initial data
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	Port read (read-only)
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

	// Return the prebuilt data
	return data[0];
}

//---------------------------------------------------------------------------
//
//	Build data
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

	// Button 1 (C)
	if (info->button[0]) {
		data[0] &= ~0x08;
	}

	// Button 2 (C+,D-)
	if (info->button[1]) {
		data[0] &= ~0x04;
	}

	// Button 3 (D)
	if (info->button[2]) {
		data[0] &= ~0x40;
	}

	// Button 4 (D+,E-)
	if (info->button[3]) {
		data[0] &= ~0x20;
	}

	// Button 5 (E)
	if (info->button[4]) {
		data[0] &= ~0x02;
	}

	// Button F (Hat)
	if (info->button[5]) {
		data[0] &= ~0x01;
	}
}

//---------------------------------------------------------------------------
//
//	Button label table
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

