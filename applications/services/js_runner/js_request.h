#pragma once
#include "js_runner_i.h"

void js_setup_request(void);
jerry_value_t js_request_init(jerry_value_t this_value, jerry_value_t url, jerry_value_t init);
