#include "js_readable_stream.h"
#include "js_runner_i.h"
#include <m-deque.h>

#define TAG "JsReadableStream"

M_DEQUE_DEF(PromiseQueue, jerry_value_t);

typedef struct JsReadableStream {
    JsFetch* parent;

    PromiseQueue_t promise_queue;
    bool data_expected;

    bool has_closed_promise;
    jerry_value_t closed_promise;

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

static jerry_value_t readable_stream_cancel(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count);
static jerry_value_t get_reader(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count);

static void detach_sink(JsReadableStream* instance);

jerry_value_t js_readable_stream_alloc(JsFetch* parent) {
    JsReadableStream* instance = malloc(sizeof(JsReadableStream));
    instance->parent = parent;
    instance->data_expected = true;
    instance->readable_stream_status = ChildStatusRunning;
    instance->async_iterator_status = ChildStatusNotYet;
    instance->has_closed_promise = false;
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

    {
        jerry_value_t cancel_fn = jerry_function_external(readable_stream_cancel);
        js_runner_check_and_free(jerry_object_set_sz(rs, "cancel", cancel_fn));
        jerry_value_free(cancel_fn);
    }

    {
        jerry_value_t get_reader_fn = jerry_function_external(get_reader);
        js_runner_check_and_free(jerry_object_set_sz(rs, "getReader", get_reader_fn));
        jerry_value_free(get_reader_fn);
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

static void resolve_everything_with_done(JsReadableStream* instance, jerry_value_t value) {
    while(!PromiseQueue_empty_p(instance->promise_queue)) {
        jerry_value_t promise;
        PromiseQueue_pop_front(&promise, instance->promise_queue);

        jerry_value_t result = iterator_result(true, jerry_value_copy(value));
        js_runner_check_and_free(jerry_promise_resolve(promise, result));
        jerry_value_free(promise);
        jerry_value_free(result);
    }
    jerry_value_free(value);
}

static void resolve_closed_promise(JsReadableStream* instance, const char* error_msg) {
    if(instance->has_closed_promise) {
        FURI_LOG_D(TAG, "resolve_closed_promise");
        if(error_msg) {
            jerry_value_t error = jerry_throw_sz(JERRY_ERROR_TYPE, error_msg);
            jerry_value_free(jerry_promise_reject(instance->closed_promise, error));
            jerry_value_free(error);
        } else {
            jerry_value_t result = jerry_undefined();
            jerry_value_free(jerry_promise_resolve(instance->closed_promise, result));
            jerry_value_free(result);
        }
        jerry_value_free(instance->closed_promise);
        instance->has_closed_promise = false;
        js_runner_run_jobs();
    }
}

static void free_if_can(JsReadableStream* instance) {
    if(instance->async_iterator_status != ChildStatusRunning &&
       instance->readable_stream_status != ChildStatusRunning) {
        FURI_LOG_D(TAG, "freeing readable_stream");
        resolve_closed_promise(instance, NULL);
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

static jerry_value_t iterator_next(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);

    FURI_LOG_D(TAG, "Next");

    JsReadableStream* instance =
        jerry_object_get_native_ptr(call_info->this_value, &async_iterator_native_info);

    jerry_value_t promise = jerry_promise();

    if(instance->data_expected && instance->parent) {
        PromiseQueue_push_back(instance->promise_queue, jerry_value_copy(promise));

        js_fetch_data_sink_ready(instance->parent);
    } else {
        jerry_value_t result = iterator_result(true, jerry_undefined());
        js_runner_check_and_free(jerry_promise_resolve(promise, result));
        jerry_value_free(result);
        js_runner_run_jobs();
    }

    return promise;
}

static jerry_value_t iterator_return(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);

    FURI_LOG_D(TAG, "Return");

    JsReadableStream* instance =
        jerry_object_get_native_ptr(call_info->this_value, &async_iterator_native_info);

    jerry_value_t promise = jerry_promise();

    jerry_value_t value = JS_ARG(0);

    detach_sink(instance);

    jerry_value_t result = iterator_result(true, value);
    js_runner_check_and_free(jerry_promise_resolve(promise, result));
    jerry_value_free(result);
    js_runner_run_jobs();

    return promise;
}

static jerry_value_t readable_stream_cancel(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);

    JsReadableStream* instance =
        jerry_object_get_native_ptr(call_info->this_value, &readable_stream_native_info);

    if(js_fetch_cancel(instance->parent)) {
        jerry_value_t promise = jerry_promise();
        jerry_value_t result = jerry_undefined();
        js_runner_check_and_free(jerry_promise_resolve(promise, result));
        jerry_value_free(result);

        return promise;
    } else {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid state: body is locked");
    }
}

static bool data_sink_callback(JsFetch* fetch, DataEvent* event, void* callback_context) {
    UNUSED(fetch);
    JsReadableStream* instance = callback_context;
    FURI_LOG_D(TAG, "data_sink_callback");
    if(!PromiseQueue_empty_p(instance->promise_queue)) {
        switch(event->type) {
        case DataEventTypeData: {
            FURI_LOG_D(TAG, "\tdata chunk consumed");
            jerry_value_t promise;
            PromiseQueue_pop_front(&promise, instance->promise_queue);

            jerry_value_t result =
                iterator_result(false, chunk_to_uint8array(event->data.buffer, event->data.size));
            js_runner_check_and_free(jerry_promise_resolve(promise, result));
            jerry_value_free(promise);
            jerry_value_free(result);
            break;
        }
        case DataEventTypeDone: {
            FURI_LOG_D(TAG, "\tdata stream end");
            instance->data_expected = false;
            resolve_closed_promise(instance, NULL);
            detach_sink(instance);

            resolve_everything_with_done(instance, jerry_undefined());
            js_runner_run_jobs();

            break;
        }
        case DataEventTypeError: {
            FURI_LOG_D(TAG, "\tdata error: %s", furi_string_get_cstr(event->error));
            instance->data_expected = false;

            resolve_closed_promise(instance, "cancelled");
            detach_sink(instance);

            resolve_everything_with_done(
                instance, jerry_throw_sz(JERRY_ERROR_TYPE, furi_string_get_cstr(event->error)));

            furi_string_free(event->error);
            js_runner_run_jobs();
            break;
        }
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

    detach_sink(instance);
    resolve_everything_with_done(instance, jerry_undefined());
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
    JsReadableStream* instance =
        jerry_object_get_native_ptr(call_info->this_value, &readable_stream_native_info);

    bool set_data_sink_ok = instance->parent &&
                            js_fetch_set_data_sink(instance->parent, instance, data_sink_callback);

    if(!set_data_sink_ok) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "Data is already in use");
    }

    jerry_value_t iter = jerry_object();
    jerry_object_set_native_ptr(iter, &async_iterator_native_info, instance);

    jerry_value_t next_fn = jerry_function_external(iterator_next);
    js_runner_check_and_free(jerry_object_set_sz(iter, "next", next_fn));
    jerry_value_free(next_fn);

    jerry_value_t return_fn = jerry_function_external(iterator_return);
    js_runner_check_and_free(jerry_object_set_sz(iter, "return", return_fn));
    js_runner_check_and_free(jerry_object_set_sz(iter, "throw", return_fn));
    jerry_value_free(return_fn);

    instance->async_iterator_status = ChildStatusRunning;

    return iter;
}


static jerry_value_t get_reader(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(args);
    UNUSED(args_count);

    FURI_LOG_D(TAG, "Get reader");
    JsReadableStream* instance =
        jerry_object_get_native_ptr(call_info->this_value, &readable_stream_native_info);

    bool set_data_sink_ok = instance->parent &&
                            js_fetch_set_data_sink(instance->parent, instance, data_sink_callback);

    if(!set_data_sink_ok) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "Data is already in use");
    }

    jerry_value_t reader = jerry_object();
    jerry_object_set_native_ptr(reader, &async_iterator_native_info, instance);

    {
        // .read() is the same as iterator's .next()
        jerry_value_t next_fn = jerry_function_external(iterator_next);
        js_runner_check_and_free(jerry_object_set_sz(reader, "read", next_fn));
        jerry_value_free(next_fn);
    }


    {
        // .cancel() is the same as iterator's .return()
        jerry_value_t return_fn = jerry_function_external(iterator_return);
        js_runner_check_and_free(jerry_object_set_sz(reader, "cancel", return_fn));
        jerry_value_free(return_fn);
    }

    {
        // .closed promise
        instance->has_closed_promise = true;
        instance->closed_promise = jerry_promise();
        js_runner_check_and_free(jerry_object_set_sz(reader, "closed", instance->closed_promise));
    }

    instance->async_iterator_status = ChildStatusRunning;

    return reader;

}

static void detach_sink(JsReadableStream* instance) {
    if(instance->parent) {
        js_fetch_set_data_sink(instance->parent, NULL, NULL);
    }
    instance->parent = NULL;
}
