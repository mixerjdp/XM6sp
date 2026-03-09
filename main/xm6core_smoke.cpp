//----------------------------------------------------------------------------
//
//  XM6Core smoke test
//
//  Checks a minimal integration path:
//  - create/destroy
//  - callback registration
//  - exec / exec_events_only
//  - save/load state in memory
//
//----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../vm/xm6core.h"

static int g_message_count = 0;

static void XM6CORE_CALL smoke_message_callback(const char* message, void* user)
{
	(void)user;
	++g_message_count;
	if (message) {
		printf("[xm6 message] %s\n", message);
	}
}

static int check_result(const char* step, int rc)
{
	if (rc != XM6CORE_OK) {
		printf("[FAIL] %s (rc=%d)\n", step, rc);
		return 0;
	}
	printf("[ OK ] %s\n", step);
	return 1;
}

int main(int argc, char** argv)
{
	const char* system_dir = NULL;
	XM6Handle handle = NULL;
	unsigned int state_size = 0;
	unsigned char* state_buffer = NULL;
	int rc;
	int ok = 1;

	if (argc >= 2) {
		system_dir = argv[1];
	}

	setvbuf(stdout, NULL, _IONBF, 0);

	printf("[step] xm6_get_version\n");
	printf("xm6_get_version: %s\n", xm6_get_version());

	if (system_dir && system_dir[0] != '\0') {
		printf("[step] xm6_set_system_dir\n");
		rc = xm6_set_system_dir(system_dir);
		if (!check_result("xm6_set_system_dir", rc)) {
			return 1;
		}
	}

	printf("[step] xm6_create\n");
	handle = xm6_create();
	if (!handle) {
		printf("[FAIL] xm6_create returned NULL.\n");
		printf("Hint: set a valid ROM directory (IPLROM.DAT, CGROM.DAT, etc.).\n");
		return 2;
	}
	printf("[ OK ] xm6_create\n");

	if (!check_result("xm6_set_message_callback",
	                  xm6_set_message_callback(handle, smoke_message_callback, NULL))) {
		ok = 0;
		goto cleanup;
	}

	if (!check_result("xm6_reset", xm6_reset(handle))) {
		ok = 0;
		goto cleanup;
	}

	if (!check_result("xm6_exec", xm6_exec(handle, 2000))) {
		ok = 0;
		goto cleanup;
	}

	rc = xm6_exec_events_only(handle, 2000);
	if (!check_result("xm6_exec_events_only", rc)) {
		ok = 0;
		goto cleanup;
	}

	if (!check_result("xm6_state_size", xm6_state_size(handle, &state_size))) {
		ok = 0;
		goto cleanup;
	}
	if (state_size == 0) {
		printf("[FAIL] xm6_state_size returned 0.\n");
		ok = 0;
		goto cleanup;
	}
	printf("[ OK ] state_size=%u bytes\n", state_size);

	state_buffer = (unsigned char*)malloc(state_size);
	if (!state_buffer) {
		printf("[FAIL] malloc(%u) failed.\n", state_size);
		ok = 0;
		goto cleanup;
	}

	if (!check_result("xm6_save_state_mem",
	                  xm6_save_state_mem(handle, state_buffer, state_size))) {
		ok = 0;
		goto cleanup;
	}

	if (!check_result("xm6_load_state_mem",
	                  xm6_load_state_mem(handle, state_buffer, state_size))) {
		ok = 0;
		goto cleanup;
	}

cleanup:
	if (state_buffer) {
		free(state_buffer);
		state_buffer = NULL;
	}
	if (handle) {
		xm6_destroy(handle);
		handle = NULL;
		printf("[ OK ] xm6_destroy\n");
	}

	printf("Messages observed: %d\n", g_message_count);
	return ok ? 0 : 1;
}
