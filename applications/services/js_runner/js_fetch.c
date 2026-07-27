#include "js_fetch.h"
#include "js_readable_stream.h"
#include <fetch/fetch.h>

#define TAG                     "JsFetch"
#define FETCH_THREAD_STACK_SIZE (10 * 1024)

#define IS_RUNNING(child) (context->child.status == ChildStatusRunning)

typedef enum ResourceParseResultTag {
    ResourceParseResultOk,
    ResourceParseResultError,
} ResourceParseResultTag;

typedef struct ResourceParseResult {
    ResourceParseResultTag tag;
    union {
        FetchRequest request;
        FuriString* error;
    };
} ResourceParseResult;

static ResourceParseResult parse_resource(jerry_value_t resource) {
    jerry_value_t str = jerry_value_to_string(resource);
    jerry_size_t str_size = jerry_string_size(str, JERRY_ENCODING_UTF8);
    if(str_size == 0) {
        return (ResourceParseResult){
            .tag = ResourceParseResultError, .error = furi_string_alloc_set("Not a string")};
    }

    char* url = malloc(str_size + 1);
    jerry_string_to_buffer(str, JERRY_ENCODING_UTF8, (jerry_char_t*)url, str_size + 1);
    jerry_value_free(str);

    FURI_LOG_D(TAG, "Fetch %s", url);

    FetchRequest request = {
        .url = url,
        .method = "GET",
        .body.length = 0,
        .headers.count = 0,
    };

    return (ResourceParseResult){
        .tag = ResourceParseResultOk,
        .request = request,
    };
}

static jerry_value_t rejected_promise(const char* msg) {
    jerry_value_t ret = jerry_promise();
    jerry_value_t msg_val = jerry_string_sz(msg);
    jerry_value_t is_ok = jerry_promise_reject(ret, msg_val);
    jerry_value_free(is_ok);
    jerry_value_free(msg_val);
    return ret;
}

static void fetch_headers_callback(const void* data, size_t data_size, void* ctx) {
    JsFetch* context = ctx;
    FURI_LOG_D(TAG, "Headers");
    char* buf = malloc(data_size);
    memcpy(buf, data, data_size);
    FetchEvent msg = {
        .type = FetchEventTypeHeaders,
        .context = context,
        .data =
            {
                .buf = buf,
                .size = data_size,
            },
    };
    furi_message_queue_put(context->event_queue, &msg, FuriWaitForever);
}

static void fetch_error_callback(const char* error, void* ctx) {
    JsFetch* context = ctx;
    FURI_LOG_D(TAG, "Error: %s", error);
    FetchEvent msg = {
        .type = FetchEventTypeError,
        .context = context,
        .error =
            {
                .msg = furi_string_alloc_set(error),
            },
    };
    furi_message_queue_put(context->event_queue, &msg, FuriWaitForever);
}

static void fetch_rx_data_callback(const void* data, size_t data_size, void* ctx) {
    JsFetch* context = ctx;
    FURI_LOG_D(TAG, "Rx data: %zu", data_size);
    char* buf = malloc(data_size);
    memcpy(buf, data, data_size);
    FetchEvent msg = {
        .type = FetchEventTypeRxData,
        .context = context,
        .data =
            {
                .buf = buf,
                .size = data_size,
            },
    };
    furi_message_queue_put(context->event_queue, &msg, FuriWaitForever);
}

// static void fetch_progress_callback(const FetchProgress* progress, void* context) {
//     UNUSED(context);
//     FURI_LOG_D(TAG, "progress: %zu/%zu", progress->received_download_size, progress->total_download_size);
// }

static int32_t fetch_thread_callback(void* ctx) {
    JsFetch* context = ctx;
    FURI_LOG_D(TAG, "Fetch thread start");
    Fetch* fetch = fetch_alloc();
    context->fetch.fetch = fetch;
    fetch_set_callback_context(fetch, context);
    fetch_set_header_callback(fetch, fetch_headers_callback);
    fetch_set_error_callback(fetch, fetch_error_callback);
    fetch_set_rx_data_callback(fetch, fetch_rx_data_callback);
    FetchStatus status = fetch_run(fetch, &context->request);
    if(status == FetchStatusOk) {
        FURI_LOG_D(TAG, "fetch succeeded");
        FetchEvent msg = {
            .type = FetchEventTypeDone,
            .context = context,
        };
        furi_message_queue_put(context->event_queue, &msg, FuriWaitForever);
    } else {
        // aborted
        FURI_LOG_D(TAG, "fetch aborted: %d", status);
    }
    fetch_free(fetch);

    FURI_LOG_D(TAG, "fetch done");
    {
        FetchEvent msg = {
            .type = FetchEventTypeThreadExit,
            .context = context,
        };
        furi_message_queue_put(context->event_queue, &msg, FuriWaitForever);
    }
    return 0;
}

static bool free_if_not_running(JsFetch* context) {
    if(!IS_RUNNING(sink) && !IS_RUNNING(fetch) && !IS_RUNNING(response) && !IS_RUNNING(promise) &&
       !context->sink.feeding) {
        FURI_LOG_D(TAG, "Deleting");
        while(!DataEventQueue_empty_p(context->chunk_queue)) {
            DataEvent event;
            DataEventQueue_pop_front(&event, context->chunk_queue);
            switch(event.type) {
            case DataEventTypeData:
                free(event.data.buffer);
                break;
            case DataEventTypeError:
                furi_string_free(event.error);
                break;
            case DataEventTypeDone:
                break;
            }
        }
        DataEventQueue_clear(context->chunk_queue);
        free(context);

        return true;
    } else {
        return false;
    }
}
static void feed_data_sink(JsFetch* context);

static void promise_free_cb(void* native_p, jerry_object_native_info_t* info_p) {
    UNUSED(info_p);
    FURI_LOG_D(TAG, "promise free");
    JsFetch* context = native_p;
    context->promise.status = ChildStatusDone;
    free_if_not_running(context);
}

static void response_free_cb(void* native_p, jerry_object_native_info_t* info_p) {
    UNUSED(info_p);
    FURI_LOG_D(TAG, "response free");
    JsFetch* context = native_p;
    context->response.status = ChildStatusDone;
    free_if_not_running(context);
}

static const jerry_object_native_info_t promise_native_info = {.free_cb = promise_free_cb};
static const jerry_object_native_info_t response_native_info = {.free_cb = response_free_cb};

static jerry_value_t fetch(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(call_info);
    UNUSED(args);

    if(args_count == 0) {
        return rejected_promise("At least 1 argument required, but only 0 passed");
    }
    ResourceParseResult resource_res = parse_resource(args[0]);
    if(resource_res.tag == ResourceParseResultError) {
        jerry_value_t result = rejected_promise(furi_string_get_cstr(resource_res.error));
        furi_string_free(resource_res.error);
        return result;
    }

    JsFetch* context = malloc(sizeof(JsFetch));
    context->request = resource_res.request;

    context->promise.promise = jerry_promise();
    context->promise.status = ChildStatusRunning;
    context->response.status = ChildStatusNotYet;
    context->fetch.status = ChildStatusRunning;
    context->sink.status = ChildStatusNotYet;
    jerry_object_set_native_ptr(context->promise.promise, &promise_native_info, context);
    FuriThread* thread =
        furi_thread_alloc_ex("Fetch", FETCH_THREAD_STACK_SIZE, fetch_thread_callback, context);
    context->fetch.thread = thread;

    DataEventQueue_init(context->chunk_queue);

    WITH_APP(app, {
        app->num_fetch_threads += 1;
        context->app = app;
        context->event_queue = app->fetch_event_queue;
    });

    furi_thread_start(thread);

    return jerry_value_copy(context->promise.promise);
}

static jerry_value_t create_response(JsFetch* context) {
    jerry_value_t response = jerry_object();

    jerry_object_set_native_ptr(response, &response_native_info, context);

    context->response.response = response;
    jerry_value_t readable_stream = js_readable_stream_alloc(context);

    js_set_property(response, "body", readable_stream);
    js_set_property(response, "bodyUsed", jerry_boolean(false));
    js_set_property(response, "ok", jerry_boolean(true)); // TODO
    js_set_property(response, "status", jerry_number(200.0)); // TODO
    js_set_property(response, "statusText", jerry_string_sz("OK")); // TODO
    js_set_property(response, "type", jerry_string_sz("basic"));
    js_set_property(response, "url", jerry_string_sz("TODO"));

    return response;
}

static void process_headers(JsFetch* context, const void* data, size_t size) {
    UNUSED(data);
    UNUSED(size);

    if(context->promise.status != ChildStatusDone) {
        jerry_value_t promise = context->promise.promise;

        furi_check(jerry_object_delete_native_ptr(promise, &promise_native_info));
        context->promise.status = ChildStatusDone;
        context->response.status = ChildStatusRunning;

        jerry_value_t response = create_response(context);
        furi_check(!jerry_value_is_exception(response));
        furi_check(jerry_value_is_promise(promise));
        js_runner_check_and_free(jerry_promise_resolve(promise, response));
        jerry_value_free(response);
        jerry_value_free(promise);
    } else {
        FURI_LOG_E(TAG, "Unexpected headers");
    }
}

static void process_error(JsFetch* context, FuriString* msg) {
    bool free_msg = true;
    if(context->promise.status == ChildStatusRunning) {
        jerry_value_t promise = context->promise.promise;
        furi_check(jerry_object_delete_native_ptr(promise, &promise_native_info));

        jerry_value_t error = jerry_string_sz(furi_string_get_cstr(msg));
        jerry_value_free(jerry_promise_reject(promise, error));
        jerry_value_free(error);
        jerry_value_free(promise);
        context->promise.status = ChildStatusDone;
        context->response.status = ChildStatusDone;
    }
    if(context->response.status == ChildStatusRunning) {
        furi_check(false);
    }
    if(context->sink.status == ChildStatusRunning) {
        DataEventQueue_push_back(
            context->chunk_queue,
            (DataEvent){
                .type = DataEventTypeError,
                .error = msg,
            });
        free_msg = false;
    }
    if(free_msg) {
        furi_string_free(msg);
    }
    free_if_not_running(context);
}

static void feed_data_sink(JsFetch* context) {
    if(context->sink.on_event) {
        context->sink.feeding = true;
        while(!DataEventQueue_empty_p(context->chunk_queue) &&
              context->sink.status == ChildStatusRunning) {
            DataEvent event;
            DataEventQueue_pop_front(&event, context->chunk_queue);
            // During this call sink can be deregistered, js objects can be destroyed, but no events can come from the queue
            bool consumed = context->sink.on_event(context, &event, context->sink.context);
            if(!consumed) {
                FURI_LOG_D(TAG, "data is not consumed");
                DataEventQueue_push_front(context->chunk_queue, event);
                break;
            }
        }
        context->sink.feeding = false;
        free_if_not_running(context);
    }
}

static void process_rx_data(JsFetch* context, void* data, size_t size) {
    if(context->promise.status != ChildStatusDone || context->response.status != ChildStatusDone ||
       context->sink.status != ChildStatusDone) {
        FURI_LOG_D(TAG, "push data %zu", size);
        DataEventQueue_push_back(
            context->chunk_queue,
            (DataEvent){
                .type = DataEventTypeData,
                .data = {
                    .buffer = data,
                    .size = size,
                }});
    } else {
        free(data);
    }
    if(context->sink.status == ChildStatusRunning) {
        feed_data_sink(context);
    }
}

static void process_done(JsFetch* context) {
    if(context->promise.status != ChildStatusDone || context->response.status != ChildStatusDone ||
       context->sink.status != ChildStatusDone) {
        DataEventQueue_push_back(
            context->chunk_queue,
            (DataEvent){
                .type = DataEventTypeDone,
            });
    }
    if(context->sink.status == ChildStatusRunning) {
        feed_data_sink(context);
    }
}

static void process_thread_exit(JsFetch* context) {
    furi_thread_join(context->fetch.thread);
    furi_thread_free(context->fetch.thread);
    context->app->num_fetch_threads -= 1;
    context->fetch.thread = NULL;
    context->fetch.status = ChildStatusDone;
    JsRunnerApp* app = context->app;
    free_if_not_running(context);
    js_runner_check_event_loop(app);
}

void js_fetch_process_event(const FetchEvent* event) {
    JsFetch* context = event->context;
    FURI_LOG_D(
        TAG,
        "process event: event=%d fetch.status=%d, sink.status=%d, promise.status=%d, response.status=%d",
        event->type,
        context->fetch.status,
        context->sink.status,
        context->promise.status,
        context->response.status);
    switch(event->type) {
    case FetchEventTypeHeaders:
        process_headers(event->context, event->data.buf, event->data.size);
        break;
    case FetchEventTypeRxData:
        process_rx_data(event->context, event->data.buf, event->data.size);
        break;
    case FetchEventTypeError:
        process_error(event->context, event->error.msg);
        break;
    case FetchEventTypeDone:
        process_done(event->context);
        break;
    case FetchEventTypeThreadExit:
        process_thread_exit(event->context);
        break;
    }
}

bool js_fetch_set_data_sink(
    JsFetch* context,
    void* callback_context,
    JsFetchDataSinkCallback callback) {
    if(context->sink.status == ChildStatusNotYet && callback) {
        FURI_LOG_D(TAG, "data sink set");
        context->sink.on_event = callback;
        context->sink.context = callback_context;
        context->sink.feeding = false;
        context->sink.status = ChildStatusRunning;
        js_set_property(context->response.response, "bodyUsed", jerry_boolean(false));
        feed_data_sink(context);
        return true;
    } else if(context->sink.status == ChildStatusRunning && !callback) {
        // Data sink expired and won't accept any more packets
        FURI_LOG_D(TAG, "data sink expired");
        context->sink.on_event = NULL;
        context->sink.context = NULL;
        context->sink.status = ChildStatusDone;
        free_if_not_running(context);
        return true;
    }
    return false;
}

void js_fetch_data_sink_ready(JsFetch* context) {
    if(!context->sink.feeding) {
        feed_data_sink(context);
    }
}

void js_runner_setup_fetch(void) {
    jerry_value_t global_obj = jerry_current_realm();
    jerry_value_t fetch_fn = jerry_function_external(fetch);

    js_runner_check_and_free(jerry_object_set_sz(global_obj, "fetch", fetch_fn));

    jerry_value_free(fetch_fn);
    jerry_value_free(global_obj);
}
