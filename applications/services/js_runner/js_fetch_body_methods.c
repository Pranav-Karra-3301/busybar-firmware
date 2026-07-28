#include "js_fetch_body_methods.h"

typedef struct BodyMethod BodyMethod;
typedef bool (*BodyCollectedCallback)(BodyMethod* instance);

typedef struct BodyMethod {
    JsFetch* parent;
    jerry_value_t promise;
    ByteArray_t* body;

    BodyCollectedCallback on_body_collected;
} BodyMethod;

static bool data_sink_callback(JsFetch* fetch, DataEvent* event, void* callback_context);
static jerry_value_t run_js_method(JsFetch* parent, BodyCollectedCallback on_body_collected);

static bool array_buffer_body_collected(BodyMethod* instance);
static bool blob_body_collected(BodyMethod* instance);
static bool bytes_body_collected(BodyMethod* instance);
static bool json_body_collected(BodyMethod* instance);
static bool form_data_body_collected(BodyMethod* instance);
static bool text_body_collected(BodyMethod* instance);

jerry_value_t js_fetch_array_buffer(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);
    JsFetch* parent =
        jerry_object_get_native_ptr(call_info->this_value, &js_fetch_response_native_info);
    return run_js_method(parent, array_buffer_body_collected);
}

jerry_value_t js_fetch_blob(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);
    JsFetch* parent =
        jerry_object_get_native_ptr(call_info->this_value, &js_fetch_response_native_info);
    return run_js_method(parent, blob_body_collected);
}

jerry_value_t js_fetch_bytes(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);
    JsFetch* parent =
        jerry_object_get_native_ptr(call_info->this_value, &js_fetch_response_native_info);
    return run_js_method(parent, bytes_body_collected);
}

jerry_value_t js_fetch_form_data(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);
    JsFetch* parent =
        jerry_object_get_native_ptr(call_info->this_value, &js_fetch_response_native_info);
    return run_js_method(parent, form_data_body_collected);
}

jerry_value_t js_fetch_json(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);
    JsFetch* parent =
        jerry_object_get_native_ptr(call_info->this_value, &js_fetch_response_native_info);
    return run_js_method(parent, json_body_collected);
}

jerry_value_t js_fetch_text(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);
    JsFetch* parent =
        jerry_object_get_native_ptr(call_info->this_value, &js_fetch_response_native_info);
    return run_js_method(parent, text_body_collected);
}

jerry_object_native_info_t promise_native_info = {0};

static jerry_value_t run_js_method(JsFetch* parent, BodyCollectedCallback on_body_collected) {
    BodyMethod* instance = malloc(sizeof(BodyMethod));
    instance->parent = parent;
    instance->promise = jerry_promise();
    instance->body = malloc(sizeof(*instance->body));
    ByteArray_init(*instance->body);
    instance->on_body_collected = on_body_collected;
    if(!js_fetch_set_data_sink(parent, instance, data_sink_callback)) {
        jerry_value_t promise = instance->promise;
        ByteArray_clear(*instance->body);
        free(instance->body);
        free(instance);
        jerry_value_t error = jerry_throw_sz(JERRY_ERROR_TYPE, "Body is already in use");
        jerry_value_free(jerry_promise_reject(promise, error));
        jerry_value_free(error);
        return promise;
    } else {
        jerry_object_set_native_ptr(instance->promise, &promise_native_info, instance);
        js_fetch_data_sink_ready(parent);
        return jerry_value_copy(instance->promise);
    }
}

static void process_data(BodyMethod* instance, void* buffer, size_t size) {
    size_t old_size = ByteArray_size(*instance->body);
    ByteArray_resize(*instance->body, old_size + size);
    memcpy(ByteArray_get(*instance->body, old_size), buffer, size);
}

static void process_done(BodyMethod* instance) {
    js_fetch_set_data_sink(instance->parent, NULL, NULL);
    bool success = instance->on_body_collected(instance);
    if(!success) {
        // otherwise buffer ownership is transferred
        ByteArray_clear(*instance->body);
        free(instance->body);
    }
    free(instance);
}

static void process_error(BodyMethod* instance, FuriString* msg) {
    js_fetch_set_data_sink(instance->parent, NULL, NULL);
    ByteArray_clear(*instance->body);
    jerry_value_t exception = jerry_throw_sz(JERRY_ERROR_TYPE, furi_string_get_cstr(msg));
    jerry_value_free(jerry_promise_reject(instance->promise, exception));
    jerry_value_free(instance->promise);
    jerry_value_free(exception);
    furi_string_free(msg);
    free(instance);
    js_runner_run_jobs();
}

static bool data_sink_callback(JsFetch* fetch, DataEvent* event, void* callback_context) {
    UNUSED(fetch);
    BodyMethod* instance = callback_context;
    switch(event->type) {
    case DataEventTypeData:
        process_data(instance, event->data.buffer, event->data.size);
        break;
    case DataEventTypeDone:
        process_done(instance);
        break;
    case DataEventTypeError:
        process_error(instance, event->error);
        break;
    }
    return true;
}

static bool array_buffer_body_collected(BodyMethod* instance) {
    jerry_value_t array_buffer = jerry_arraybuffer_external(
        ByteArray_get(*instance->body, 0), ByteArray_size(*instance->body), instance->body);
    js_runner_check_and_free(jerry_promise_resolve(instance->promise, array_buffer));
    jerry_value_free(instance->promise);
    jerry_value_free(array_buffer);
    js_runner_run_jobs();
    return true;
}

static bool blob_body_collected(BodyMethod* instance) {
    jerry_value_t exception = jerry_throw_sz(JERRY_ERROR_TYPE, "unimplemented");
    jerry_value_free(jerry_promise_reject(instance->promise, exception));
    jerry_value_free(instance->promise);
    js_runner_run_jobs();
    return false;
}

static bool bytes_body_collected(BodyMethod* instance) {
    jerry_value_t array_buffer = jerry_arraybuffer_external(
        ByteArray_get(*instance->body, 0), ByteArray_size(*instance->body), instance->body);
    jerry_value_t bytes = jerry_typedarray_with_buffer(JERRY_TYPEDARRAY_UINT8, array_buffer);

    js_runner_check_and_free(jerry_promise_resolve(instance->promise, array_buffer));
    jerry_value_free(instance->promise);
    jerry_value_free(array_buffer);
    jerry_value_free(bytes);
    js_runner_run_jobs();
    return true;
}

static bool json_body_collected(BodyMethod* instance) {
    jerry_value_t exception = jerry_throw_sz(JERRY_ERROR_TYPE, "unimplemented");
    jerry_value_free(jerry_promise_reject(instance->promise, exception));
    jerry_value_free(instance->promise);
    js_runner_run_jobs();
    return false;
}

static bool form_data_body_collected(BodyMethod* instance) {
    jerry_value_t exception = jerry_throw_sz(JERRY_ERROR_TYPE, "unimplemented");
    jerry_value_free(jerry_promise_reject(instance->promise, exception));
    jerry_value_free(instance->promise);
    js_runner_run_jobs();
    return false;
}

static bool text_body_collected(BodyMethod* instance) {
    jerry_value_t string = jerry_string_external(
        ByteArray_get(*instance->body, 0), ByteArray_size(*instance->body), instance->body);
    js_runner_check_and_free(jerry_promise_resolve(instance->promise, string));
    jerry_value_free(instance->promise);
    jerry_value_free(string);
    js_runner_run_jobs();
    return true;
}
