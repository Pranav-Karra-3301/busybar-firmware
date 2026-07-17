#include "js_readable_stream.h"
#include "js_runner_i.h"
#include <m-deque.h>

#define TAG "JsReadableStream"

M_DEQUE_DEF(PromiseQueue, jerry_value_t);

typedef struct JsReadableStream {
    JsFetch* parent;

    PromiseQueue_t promise_queue;
    bool data_expected;

    ChildStatus readable_stream_status;
    ChildStatus async_iterator_status;
} JsReadableStream;

static jerry_value_t iterator_result(bool done, jerry_value_t value);

static void readable_stream_free_cb(void* native_p, jerry_object_native_info_t* info_p);
static void async_iterator_free_cb(void* native_p, jerry_object_native_info_t* info_p);
static const jerry_object_native_info_t readable_stream_native_info = {
    .free_cb = readable_stream_free_cb};
static const jerry_object_native_info_t async_iterator_native_info = {
    .free_cb = async_iterator_free_cb};

static jerry_value_t async_iterator(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count);

jerry_value_t js_readable_stream_alloc(JsFetch* parent) {
    JsReadableStream* instance = malloc(sizeof(JsReadableStream));
    instance->parent = parent;
    instance->data_expected = true;
    instance->readable_stream_status = ChildStatusRunning;
    instance->async_iterator_status = ChildStatusNotYet;
    PromiseQueue_init(instance->promise_queue);

    jerry_value_t rs = jerry_object();
    jerry_object_set_native_ptr(rs, &readable_stream_native_info, instance);

    {
        jerry_value_t async_iterator_fn = jerry_function_external(async_iterator);
        jerry_value_t async_iterator_sym = jerry_symbol(JERRY_SYMBOL_ASYNC_ITERATOR);
        js_runner_check_and_free(jerry_object_set(rs, async_iterator_sym, async_iterator_fn));
        jerry_value_free(async_iterator_fn);
        jerry_value_free(async_iterator_sym);
    }

    return rs;
}

static jerry_value_t chunk_to_uint8array(void* buffer, size_t size) {
    // buffer will be released by arraybuffer_free_callback
    jerry_value_t arraybuffer = jerry_arraybuffer_external(buffer, size, buffer);

    jerry_value_t uint8array = jerry_typedarray_with_buffer(JERRY_TYPEDARRAY_UINT8, arraybuffer);
    jerry_value_free(arraybuffer);
    return uint8array;
}

static jerry_value_t iterator_result(bool done, jerry_value_t value) {
    jerry_value_t obj = jerry_object();
    js_runner_check_and_free(jerry_object_set_sz(obj, "value", value));
    jerry_value_t done_val = jerry_boolean(done);
    js_runner_check_and_free(jerry_object_set_sz(obj, "done", done_val));

    jerry_value_free(value);
    jerry_value_free(done_val);
    return obj;
}

static void resolve_everything_with_done(JsReadableStream* instance) {
    while(!PromiseQueue_empty_p(instance->promise_queue)) {
        jerry_value_t promise;
        PromiseQueue_pop_front(&promise, instance->promise_queue);

        jerry_value_t result = iterator_result(true, jerry_undefined());
        js_runner_check_and_free(jerry_promise_resolve(promise, result));
        jerry_value_free(promise);
        jerry_value_free(result);
    }
}

static void free_if_can(JsReadableStream* instance) {
    if(instance->async_iterator_status != ChildStatusRunning &&
       instance->readable_stream_status != ChildStatusRunning) {
        FURI_LOG_D(TAG, "freeing readable_stream");
        furi_check(PromiseQueue_empty_p(instance->promise_queue));
        PromiseQueue_clear(instance->promise_queue);

        free(instance);
    }
}

static void readable_stream_free_cb(void* native_p, jerry_object_native_info_t* info_p) {
    UNUSED(info_p);
    JsReadableStream* instance = native_p;
    FURI_LOG_D(TAG, "readable_stream_free_cb");
    instance->readable_stream_status = ChildStatusDone;
    free_if_can(instance);
}

static jerry_value_t next(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);

    FURI_LOG_D(TAG, "Next");

    JsReadableStream* readable_stream =
        jerry_object_get_native_ptr(call_info->this_value, &async_iterator_native_info);

    jerry_value_t promise = jerry_promise();

    if(readable_stream->data_expected) {
        PromiseQueue_push_back(readable_stream->promise_queue, jerry_value_copy(promise));

        js_fetch_data_sink_ready(readable_stream->parent);
    } else {
        jerry_value_t result = iterator_result(true, jerry_undefined());
        js_runner_check_and_free(jerry_promise_resolve(promise, result));
        jerry_value_free(result);
        js_runner_run_jobs();
    }

    return promise;
}

static bool data_sink_callback(JsFetch* fetch, void* buffer, size_t size, void* callback_context) {
    JsReadableStream* readable_stream = callback_context;
    FURI_LOG_D(TAG, "data_sink_callback");
    if(!PromiseQueue_empty_p(readable_stream->promise_queue)) {
        if(buffer) {
            FURI_LOG_D(TAG, "\tdata chunk consumed");
            jerry_value_t promise;
            PromiseQueue_pop_front(&promise, readable_stream->promise_queue);

            jerry_value_t result = iterator_result(false, chunk_to_uint8array(buffer, size));
            js_runner_check_and_free(jerry_promise_resolve(promise, result));
            jerry_value_free(promise);
            jerry_value_free(result);
        } else {
            FURI_LOG_D(TAG, "\tdata stream end");
            resolve_everything_with_done(readable_stream);

            readable_stream->data_expected = false;
            js_fetch_set_data_sink(fetch, NULL, NULL);
        }
        js_runner_run_jobs();
        return true;
    } else {
        return false;
    }
}

static void async_iterator_free_cb(void* native_p, jerry_object_native_info_t* info_p) {
    UNUSED(native_p);
    UNUSED(info_p);
    JsReadableStream* instance = native_p;
    FURI_LOG_D(TAG, "async_iterator_free_cb");

    js_fetch_set_data_sink(instance->parent, NULL, NULL);
    resolve_everything_with_done(instance);
    js_runner_run_jobs();
    instance->async_iterator_status = ChildStatusDone;
    free_if_can(instance);
}

static jerry_value_t async_iterator(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);

    FURI_LOG_D(TAG, "Get iterator");
    JsReadableStream* readable_stream =
        jerry_object_get_native_ptr(call_info->this_value, &readable_stream_native_info);
    bool set_data_sink_ok = false;
    set_data_sink_ok =
        js_fetch_set_data_sink(readable_stream->parent, readable_stream, data_sink_callback);

    if(!set_data_sink_ok) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "Data is already in use");
    }

    jerry_value_t iter = jerry_object();
    jerry_object_set_native_ptr(iter, &async_iterator_native_info, readable_stream);

    jerry_value_t next_fn = jerry_function_external(next);
    js_runner_check_and_free(jerry_object_set_sz(iter, "next", next_fn));
    jerry_value_free(next_fn);

    readable_stream->async_iterator_status = ChildStatusRunning;

    return iter;
}
