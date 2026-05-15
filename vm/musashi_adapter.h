//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Musashi Adapter - Compatibility layer replacing Starscream
//	[ musashi_adapter.h ]
//
//---------------------------------------------------------------------------

#ifndef musashi_adapter_h
#define musashi_adapter_h

#pragma pack(push, 1)

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------
//
//	Starscream-compatible region structs (stubs for Memory class layout)
//
//---------------------------------------------------------------------------
struct STARSCREAM_PROGRAMREGION {
	unsigned lowaddr;
	unsigned highaddr;
	unsigned offset;
};

struct STARSCREAM_DATAREGION {
	unsigned lowaddr;
	unsigned highaddr;
	void    *memorycall;
	void    *userdata;
};

//---------------------------------------------------------------------------
//
//	Starscream-compatible context struct
//	Backed by Musashi internally; synced before/after execution
//
//---------------------------------------------------------------------------
struct S68000CONTEXT {
	/* Memory region pointers (kept for memory.h compilation, not used by Musashi) */
	struct STARSCREAM_PROGRAMREGION *fetch;
	struct STARSCREAM_DATAREGION    *readbyte;
	struct STARSCREAM_DATAREGION    *readword;
	struct STARSCREAM_DATAREGION    *writebyte;
	struct STARSCREAM_DATAREGION    *writeword;
	struct STARSCREAM_PROGRAMREGION *s_fetch;
	struct STARSCREAM_DATAREGION    *s_readbyte;
	struct STARSCREAM_DATAREGION    *s_readword;
	struct STARSCREAM_DATAREGION    *s_writebyte;
	struct STARSCREAM_DATAREGION    *s_writeword;
	struct STARSCREAM_PROGRAMREGION *u_fetch;
	struct STARSCREAM_DATAREGION    *u_readbyte;
	struct STARSCREAM_DATAREGION    *u_readword;
	struct STARSCREAM_DATAREGION    *u_writebyte;
	struct STARSCREAM_DATAREGION    *u_writeword;

	/* CPU state - synced with Musashi */
	void         (*resethandler)(void);
	unsigned       dreg[8];
	unsigned       areg[8];
	unsigned       asp;
	unsigned       pc;
	unsigned       odometer;
	unsigned char  interrupts[8];
	unsigned short sr;
	unsigned short contextfiller00;
};

//---------------------------------------------------------------------------
//
//	Global context and I/O cycle counter
//
//---------------------------------------------------------------------------
extern struct S68000CONTEXT s68000context;
extern unsigned int s68000iocycle;

//---------------------------------------------------------------------------
//
//	Starscream-compatible API (implemented by musashi_adapter.cpp)
//
//---------------------------------------------------------------------------
int      s68000init            (void);
unsigned s68000reset           (void);
unsigned s68000exec            (int n);
int      s68000interrupt       (int level, int vector);
void     s68000flushInterrupts (void);
int      s68000GetContextSize  (void);
void     s68000GetContext      (void *context);
void     s68000SetContext      (void *context);
int      s68000fetch           (unsigned address);
unsigned s68000readOdometer    (void);
unsigned s68000tripOdometer    (void);
unsigned s68000controlOdometer (int n);
void     s68000releaseTimeslice(void);
unsigned s68000readPC          (void);
unsigned s68000wait            (unsigned cycle);
void     musashi_adjust_timeslice(int cycles);
void     musashi_flush_timeslice(void);
void     musashi_set_render_mode(int mode);

//---------------------------------------------------------------------------
//
//	XM6-specific functions
//
//---------------------------------------------------------------------------
unsigned int s68000getcounter(void);
void         s68000setcounter(unsigned int c);

//---------------------------------------------------------------------------
//
//	Bus error injection
//
//---------------------------------------------------------------------------
void s68000buserr(unsigned int addr, unsigned int param);

//---------------------------------------------------------------------------
//
//	Callbacks from CPU core to XM6 (implemented in cpu.cpp)
//
//---------------------------------------------------------------------------
void s68000intack(int level);

//---------------------------------------------------------------------------
//
//	Musashi callback prototypes (implemented in musashi_adapter.cpp)
//
//---------------------------------------------------------------------------
int  musashi_int_ack_callback(int int_level);
void musashi_reset_callback(void);
void musashi_fc_callback(unsigned int new_fc);

//---------------------------------------------------------------------------
//
//	Memory access state (for function code tracking)
//
//---------------------------------------------------------------------------
extern unsigned int musashi_current_fc;
extern bool musashi_is_resetting;

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif /* musashi_adapter_h */
