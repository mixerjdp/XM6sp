//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Core Soft (replaces core_asm) - Event Dispatcher
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "device.h"
#include "event.h"
#include "core_soft.h" 

//
// メモリデコード用データ (Memory decode data) - 8KB slots
//
static const BYTE MemDecodeData[] = {
	// $C00000 - $DFFFFF (GVRAM): 256 slots
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,

	// $E00000 - $E7FFFF (TVRAM): 64 slots
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,

	// $E80000 - $E8FFFF (Peripherals): 15 slots
	3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,

	// $E9E000 (WINDRV): 1 slot
	18,

	// $EA0000 (SCSI): 1 slot
	19,

	// $EA2000 (Reserve): 6 slots
	0,0,0,0,0,0,

	// $EAE000 (MIDI): 1 slot
	20,

	// $EB0000 - $EBFFFF (SPRITE): 8 slots
	21,21,21,21,21,21,21,21,

	// $EC0000 - $ECBFFF (User): 6 slots
	0,0,0,0,0,0,

	// $ECC000 (MERCURY): 1 slot
	22,

	// $ECE000 (NEPTUNE): 1 slot
	23,

	// $ED0000 - $EDFFFF (SRAM): 8 slots
	24,24,24,24,24,24,24,24,

	// $EE0000 - $EFFFFF (Reserve): 16 slots
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

// 実体 (Table instance)
extern "C" uintptr_t MemDecodeTable[0x180] = { 0 };

//
// メモリデコードの初期化 (Memory decode initialization)
//
extern "C" void MemInitDecode(Device* /*mem*/, MemDevice **devarray)
{
	uintptr_t *table = MemDecodeTable;
	
	for (int i = 0; i < 0x180; i++) {
		BYTE index = MemDecodeData[i];
		table[i] = reinterpret_cast<uintptr_t>(devarray[index]);
	}
}

//
// イベントテーブルと数 (Event table and count)
//
static Event* EventTable[32];
static DWORD EventNum = 0;

//
// イベント変更通知 (Notify Event)
//
void NotifyEvent(Event *first)
{
	EventNum = 0;
	Event *curr = first;
	
	while (curr != NULL && EventNum < 32) {
		EventTable[EventNum++] = curr;
		curr = curr->ev.next;
	}
}

//
// 最小のイベントを探す (Find minimum event)
//
DWORD GetMinEvent(DWORD hus)
{
	DWORD min_hus = hus;
	
	for (DWORD i = 0; i < EventNum; i++) {
		Event *ev = EventTable[i];
		DWORD remain = ev->ev.remain;
		
		if (remain != 0 && remain < min_hus) {
			min_hus = remain;
		}
	}
	
	return min_hus;
}

//
// イベント時間減少と実行 (Execute sub events)
//
BOOL SubExecEvent(DWORD hus)
{
	for (DWORD i = 0; i < EventNum; i++) {
		Event *e = EventTable[i];
		DWORD time = e->ev.time;
		
		if (time == 0) {
			continue;
		}
		
		if (e->ev.remain <= hus) {
			// 時間をリセット (Reset time)
			e->ev.remain = time;
			
			// イベント実行(Device::Callback呼び出し) (Execute event)
			Device *dev = e->ev.device;
			BOOL res = dev->Callback(e);
			
			// 結果が0なら、FALSEのため無効化 (If false, disable)
			if (!res) {
				e->ev.time = 0;
				e->ev.remain = 0;
			}
		} else {
			// 時間を引く (Subtract time)
			e->ev.remain -= hus;
		}
	}
	
	return TRUE;
}
