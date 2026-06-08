/**
 * @file jerry_port.c
 * @brief JerryScript Port for ElenixOS
 */

#include <stdio.h>
#include <stdlib.h>
#include "jerryscript-port.h"
#include "script_engine_core.h"

void JERRY_ATTR_NORETURN jerry_port_fatal(jerry_fatal_code_t code)
{
    printf("[JerryScript] Fatal error: code=%d\r\n", (int)code);

    if (!script_engine_is_fatal_scope_active())
    {
        /*
         * longjmp to an invalid setjmp frame is undefined behavior.
         * After script_engine_run() returns, its stack frame is gone.
         * This should never happen, but if it does, abort is safer
         * than corrupting memory via longjmp to a dead frame.
         */
        abort();
    }

    if (script_engine_is_fatal_recovering())
    {
        /* Prevent infinite longjmp loop if setjmp recovery itself faults */
        abort();
    }
    script_engine_set_fatal_recovering(true);

    script_engine_fatal_longjmp((int)(code != 0 ? code : -1));
    /* unreachable */
    while (1) { }
}
