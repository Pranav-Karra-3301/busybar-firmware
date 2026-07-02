#include "../unit_tests.h"
#include <js_runner/js_runner.h>
#include <storage/storage.h>
#include <string.h>

#define SCRIPT_FILE UNIT_TESTS_PATH("test.js")

static bool create_file(Storage* storage, const char* path, const char* data) {
    File* file = storage_file_alloc(storage);
    bool result = false;
    do {
        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_NEW)) {
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

MU_TEST(js_tests_expr) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    mu_check(create_file(storage, SCRIPT_FILE, "console.log(\"flipppper\");"));
    furi_record_close(RECORD_STORAGE);

    JsRunner* js_runner = furi_record_open(RECORD_JS_RUNNER);
    char buf[64] = {0};

    js_runner_run(js_runner, SCRIPT_FILE, 1024, js_console_cb, buf);

    mu_assert_string_eq(buf, "flipppper\n");

    furi_record_close(RECORD_JS_RUNNER);
}

MU_TEST_SUITE(js_test_suite) {
    MU_RUN_TEST(js_tests_expr);
}

int run_minunit_js_test(void) {
    MU_RUN_SUITE(js_test_suite);
    return MU_EXIT_CODE;
}
