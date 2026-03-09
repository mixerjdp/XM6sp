//---------------------------------------------------------------------------
//
//  X68000 EMULATOR "XM6"
//
//  Host service callback contracts for VM/frontend decoupling.
//
//---------------------------------------------------------------------------

#if !defined(host_services_h)
#define host_services_h

#include "os.h"
#include "xm6.h"

typedef void (FASTCALL *host_sync_callback_t)(void *user);
typedef void (FASTCALL *host_message_callback_t)(const TCHAR* message, void *user);

typedef struct {
	host_sync_callback_t lock_vm;
	host_sync_callback_t unlock_vm;
	host_message_callback_t message;
	void *user;
} host_services_t;

#endif	// host_services_h
