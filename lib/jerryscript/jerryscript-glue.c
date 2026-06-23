#include "jerryscript.h"
#include <furi.h>

int32_t jerry_port_local_tza(double unix_ms) {
    return 0;
}

void jerry_port_init(void) {
}

void jerry_port_fatal(jerry_fatal_code_t code) {
    furi_crash(code);
}

double jerry_port_current_time(void) {
    return 0.0;
}
