#include "../unit_tests.h"
#include <js_runner/js_runner.h>
#include <storage/storage.h>
#include <string.h>

#define SCRIPT_FILE UNIT_TESTS_PATH("test.js")

#define MODULE1_FILE UNIT_TESTS_PATH("mod1.js")
#define MODULE2_FILE UNIT_TESTS_PATH("mod2.js")

static bool create_file(Storage* storage, const char* path, const char* data) {
    File* file = storage_file_alloc(storage);
    bool result = false;
    do {
        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            break;
        }

        if(storage_file_write(file, data, strlen(data)) != strlen(data)) {
            break;
        }

        if(!storage_file_close(file)) {
            break;
        }

        result = true;
    } while(0);

    storage_file_free(file);
    return result;
}

static void
    js_console_cb(JsRunnerConsoleSeverity severity, const char* buf, size_t size, void* context) {
    UNUSED(severity);

    char* out_buf = context;
    size_t out_buf_len = strlen(out_buf);
    memcpy(out_buf + out_buf_len, buf, size);
}

MU_TEST(js_tests_console) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    mu_check(create_file(storage, SCRIPT_FILE, "console.log(\"flipppper\");"));
    furi_record_close(RECORD_STORAGE);

    JsRunner* js_runner = furi_record_open(RECORD_JS_RUNNER);
    char buf[64] = {0};

    mu_assert_int_eq(
        JsRunnerErrorNone, js_runner_run(js_runner, SCRIPT_FILE, 1024, js_console_cb, buf));

    mu_assert_string_eq("flipppper\n", buf);

    furi_record_close(RECORD_JS_RUNNER);
}

MU_TEST(js_tests_modules) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    mu_check(create_file(
        storage,
        MODULE1_FILE,
        "import { hello } from './mod2.js'; console.log(\"module\"); hello(\"flipper\");"));
    mu_check(create_file(storage, MODULE2_FILE, "export function hello(arg) {console.log(arg);}"));
    furi_record_close(RECORD_STORAGE);

    JsRunner* js_runner = furi_record_open(RECORD_JS_RUNNER);
    char buf[64] = {0};

    mu_assert_int_eq(
        JsRunnerErrorNone, js_runner_run(js_runner, MODULE1_FILE, 4096, js_console_cb, buf));

    mu_assert_string_eq("module\nflipper\n", buf);

    furi_record_close(RECORD_JS_RUNNER);
}

MU_TEST_SUITE(js_test_suite) {
    MU_RUN_TEST(js_tests_console);
    MU_RUN_TEST(js_tests_modules);
}

int run_minunit_js_test(void) {
    MU_RUN_SUITE(js_test_suite);
    return MU_EXIT_CODE;
}
