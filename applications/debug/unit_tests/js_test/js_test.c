#include "../unit_tests.h"
#include <jerryscript.h>
#include <string.h>

static bool square_with_js(double input, double* output) {
    jerry_init(JERRY_INIT_EMPTY);

    jerry_value_t global = jerry_current_realm();

    jerry_value_t input_name = jerry_string_sz("input");
    jerry_value_t input_value = jerry_number(input);

    jerry_object_set(global, input_name, input_value);

    jerry_value_free(input_name);
    jerry_value_free(input_value);

    static const char* script = "input * input;";

    jerry_value_t result =
        jerry_eval((const jerry_char_t*)script, strlen(script), JERRY_PARSE_NO_OPTS);

    bool ret = true;
    if(jerry_value_is_exception(result)) {
        ret = false;
    } else {
        *output = jerry_value_as_number(result);
        ret = true;
    }

    jerry_value_free(result);
    jerry_value_free(global);
    jerry_cleanup();
    return ret;
}

MU_TEST(js_tests_expr) {
    double input = 2.5;
    double output = 0.0;
    mu_check(square_with_js(input, &output));
    mu_assert_double_eq(output, 6.25);
}

MU_TEST_SUITE(js_test_suite) {
    MU_RUN_TEST(js_tests_expr);
}

int run_minunit_js_test(void) {
    MU_RUN_SUITE(js_test_suite);
    return MU_EXIT_CODE;
}
