#include "js_runner_i.h"

void js_set_property(jerry_value_t object, const char* name, jerry_value_t property) {
    js_runner_check_and_free(jerry_object_set_sz(object, name, property));
    jerry_value_free(property);
}

void js_set_method(jerry_value_t object, const char* name, jerry_external_handler_t handler) {
    jerry_value_t fn = jerry_function_external(handler);
    js_runner_check_and_free(jerry_object_set_sz(object, name, fn));
    jerry_value_free(fn);
}
