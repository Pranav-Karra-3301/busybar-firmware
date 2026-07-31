#include "js_fetch.h"
#include "js_readable_stream.h"
#include "js_fetch_body_methods.h"
#include "js_headers.h"
#include "js_request.h"
#include <fetch/fetch.h>

#define TAG                     "JsFetch"
#define FETCH_THREAD_STACK_SIZE (10 * 1024)

#define IS_RUNNING(child) (instance->child.status == ChildStatusRunning)

typedef enum RequestParseResultTag {
    RequestParseResultOk,
    RequestParseResultError,
} RequestParseResultTag;

typedef struct RequestParseResult {
    RequestParseResultTag tag;
    union {
        FetchRequest request;
        FuriString* error;
    };
} RequestParseResult;

static void fetch_request_free(FetchRequest* request) {
    if(request->url) {
        free((void*)request->url);
    }
    if(request->method) {
        free((void*)request->method);
    }
    for(size_t i = 0; i != request->headers.count; ++i) {
        free((void*)request->headers.data[i]);
    }
    if(request->body.data) {
        free((void*)request->body.data);
    }
}

static RequestParseResult parse_request(jerry_value_t obj) {
    furi_assert(js_is_instance_of(obj, "Request"));

    FetchRequest request = {0};
    RequestParseResult result;

    do {
        jerry_value_t url_val = jerry_object_get_sz(obj, "url");
        if(jerry_value_is_exception(url_val)) {
            result = (RequestParseResult){
                .tag = RequestParseResultError,
                .error = js_get_exception_string(url_val),
            };
            jerry_value_free(url_val);
            break;
        }
        request.url = js_string_to_c_string(url_val);
        jerry_value_free(url_val);
        if(!request.url) {
            result = (RequestParseResult){
                .tag = RequestParseResultError,
                .error = furi_string_alloc_set("URL is not a string"),
            };
            break;
        }
        FURI_LOG_D(TAG, "Fetch %s", request.url);

        if(js_object_has_property(obj, "method")) {
            jerry_value_t method_val = jerry_object_get_sz(obj, "method");
            request.method = js_string_to_c_string(method_val);
            jerry_value_free(method_val);
            if(!request.method) {
                result = (RequestParseResult){
                    .tag = RequestParseResultError,
                    .error = furi_string_alloc_set("Method is not a string"),
                };
                break;
            }
        }

        if(js_object_has_property(obj, "headers")) {
            jerry_value_t headers_val = jerry_object_get_sz(obj, "headers");
            // TODO instance of Headers
            if(jerry_value_is_object(headers_val)) {
                jerry_value_t keys = jerry_object_keys(headers_val);
                size_t num_keys = jerry_array_length(keys);
                size_t header_idx = 0;
                for(size_t i = 0; i != num_keys && header_idx != FETCH_HEADERS_COUNT_MAX; ++i) {
                    jerry_value_t key = jerry_object_get_index(keys, i);
                    jerry_value_t value = jerry_object_get(headers_val, key);
                    jerry_value_t value_conv = jerry_value_to_string(value);
                    if(jerry_value_is_string(key) && jerry_value_is_string(value_conv)) {
                        char* key_string = js_string_to_c_string(key);
                        char* value_string = js_string_to_c_string(value_conv);

                        char* header_string =
                            malloc(strlen(key_string) + 2 + strlen(value_string) + 1);
                        sprintf(header_string, "%s: %s", key_string, value_string);
                        free(key_string);
                        free(value_string);

                        request.headers.data[header_idx] = header_string;

                        header_idx += 1;
                    }
                    jerry_value_free(key);
                    jerry_value_free(value);
                    jerry_value_free(value_conv);
                }
                jerry_value_free(keys);
                request.headers.count = header_idx;
            } else {
                result = (RequestParseResult){
                    .tag = RequestParseResultError,
                    .error = furi_string_alloc_set("Headers is not an object"),
                };
            }
            jerry_value_free(headers_val);
        }

        if(js_object_has_property(obj, "body")) {
            jerry_value_t body_val = jerry_object_get_sz(obj, "body");
            // TODO handle many different cases
            jerry_value_t body_str = jerry_value_to_string(body_val);
            request.body.data = js_string_to_c_string(body_str);
            furi_check(request.body.data); // okay to crash - body handling TODO
            request.body.length = strlen(request.body.data);
            jerry_value_free(body_str);
            jerry_value_free(body_val);
            if(!request.body.data) {
                result = (RequestParseResult){
                    .tag = RequestParseResultError,
                    .error = furi_string_alloc_set("Body is not a string"),
                };
                break;
            }
        }
        result = (RequestParseResult){
            .tag = RequestParseResultOk,
            .request = request,
        };
    } while(false);

    if(result.tag == RequestParseResultError) {
        fetch_request_free(&request);
    }
    return result;
}

static void fetch_headers_callback(const void* data, size_t data_size, void* ctx) {
    JsFetch* context = ctx;
    char* buf = malloc(data_size);
    memcpy(buf, data, data_size);
    FetchEvent msg = {
        .type = FetchEventTypeHeaders,
        .instance = context,
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
    FetchEvent msg = {
        .type = FetchEventTypeError,
        .instance = context,
        .error =
            {
                .msg = furi_string_alloc_set(error),
            },
    };
    furi_message_queue_put(context->event_queue, &msg, FuriWaitForever);
}

static void fetch_rx_data_callback(const void* data, size_t data_size, void* ctx) {
    JsFetch* context = ctx;
    char* buf = malloc(data_size);
    memcpy(buf, data, data_size);
    FetchEvent msg = {
        .type = FetchEventTypeRxData,
        .instance = context,
        .data =
            {
                .buf = buf,
                .size = data_size,
            },
    };
    furi_message_queue_put(context->event_queue, &msg, FuriWaitForever);
}

static int32_t fetch_thread_callback(void* ctx) {
    JsFetch* context = ctx;
    Fetch* fetch = fetch_alloc();
    context->fetch.fetch = fetch;
    fetch_set_callback_context(fetch, context);
    fetch_set_header_callback(fetch, fetch_headers_callback);
    fetch_set_error_callback(fetch, fetch_error_callback);
    fetch_set_rx_data_callback(fetch, fetch_rx_data_callback);
    FetchStatus status = fetch_run(fetch, &context->request);
    fetch_request_free(&context->request);
    if(status == FetchStatusOk) {
        FetchEvent msg = {
            .type = FetchEventTypeDone,
            .instance = context,
        };
        furi_message_queue_put(context->event_queue, &msg, FuriWaitForever);
    } else {
        // aborted
    }
    fetch_free(fetch);

    {
        FetchEvent msg = {
            .type = FetchEventTypeThreadExit,
            .instance = context,
        };
        furi_message_queue_put(context->event_queue, &msg, FuriWaitForever);
    }
    return 0;
}

static void empty_event_queue(JsFetch* instance) {
    while(!DataEventQueue_empty_p(instance->chunk_queue)) {
        DataEvent event;
        DataEventQueue_pop_front(&event, instance->chunk_queue);
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
}

static bool free_if_not_running(JsFetch* instance) {
    if(!IS_RUNNING(sink) && !IS_RUNNING(fetch) && !IS_RUNNING(response) && !IS_RUNNING(promise) &&
       !instance->sink.feeding) {
        empty_event_queue(instance);
        DataEventQueue_clear(instance->chunk_queue);
        free(instance);

        return true;
    } else {
        return false;
    }
}
static void feed_data_sink(JsFetch* instance);

static void promise_free_cb(void* native_p, jerry_object_native_info_t* info_p) {
    UNUSED(info_p);
    JsFetch* instance = native_p;
    instance->promise.status = ChildStatusDone;
    free_if_not_running(instance);
}

static void response_free_cb(void* native_p, jerry_object_native_info_t* info_p) {
    UNUSED(info_p);
    JsFetch* instance = native_p;
    instance->response.status = ChildStatusDone;
    free_if_not_running(instance);
}

static const jerry_object_native_info_t promise_native_info = {.free_cb = promise_free_cb};
const jerry_object_native_info_t js_fetch_response_native_info = {.free_cb = response_free_cb};

static jerry_value_t fetch(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(call_info);
    UNUSED(args);

    if(args_count == 0) {
        return js_rejected_promise("At least 1 argument required, but only 0 passed");
    }
    jerry_value_t url = JS_ARG(0);
    jerry_value_t init = JS_ARG(1);

    jerry_value_t request = jerry_object();
    jerry_value_t request_init_result = js_request_init(request, url, init);
    if(jerry_value_is_exception(request_init_result)) {
        jerry_value_free(request);
        return js_rejected_promise_from_exception(request_init_result);
    }
    jerry_value_free(request_init_result);

    RequestParseResult request_res = parse_request(request);
    jerry_value_free(request);

    if(request_res.tag == RequestParseResultError) {
        jerry_value_t ret = js_rejected_promise(furi_string_get_cstr(request_res.error));
        furi_string_free(request_res.error);
        return ret;
    }

    JsFetch* instance = malloc(sizeof(JsFetch));
    instance->request = request_res.request;

    instance->promise.promise = jerry_promise();
    instance->promise.status = ChildStatusRunning;
    instance->response.status = ChildStatusNotYet;
    instance->fetch.status = ChildStatusRunning;
    instance->sink.status = ChildStatusNotYet;
    jerry_object_set_native_ptr(instance->promise.promise, &promise_native_info, instance);
    FuriThread* thread =
        furi_thread_alloc_ex("Fetch", FETCH_THREAD_STACK_SIZE, fetch_thread_callback, instance);
    instance->fetch.thread = thread;

    DataEventQueue_init(instance->chunk_queue);

    WITH_JS_RUNNER_APP(app, {
        app->num_fetch_threads += 1;
        instance->app = app;
        instance->event_queue = app->fetch_event_queue;
    });

    furi_thread_start(thread);

    return jerry_value_copy(instance->promise.promise);
}

static jerry_value_t
    create_response(JsFetch* instance, const char* headers_data, size_t headers_size) {
    jerry_value_t response = jerry_object();

    jerry_object_set_native_ptr(response, &js_fetch_response_native_info, instance);

    instance->response.response = response;
    jerry_value_t readable_stream = js_readable_stream_alloc(instance);
    jerry_value_t headers = js_headers_alloc(response, headers_data, headers_size);

    js_set_property(response, "headers", headers);
    js_set_property(response, "body", readable_stream);
    js_set_property(response, "bodyUsed", jerry_boolean(false));
    js_set_property(response, "type", jerry_string_sz("basic"));
    js_set_property(response, "url", jerry_string_sz("TODO"));

    js_set_method(response, "arrayBuffer", js_fetch_array_buffer);
    js_set_method(response, "blob", js_fetch_blob);
    js_set_method(response, "bytes", js_fetch_bytes);
    js_set_method(response, "formData", js_fetch_form_data);
    js_set_method(response, "json", js_fetch_json);
    js_set_method(response, "text", js_fetch_text);

    return response;
}

static void process_headers(JsFetch* instance, const void* data, size_t size) {
    if(instance->promise.status != ChildStatusDone) {
        jerry_value_t promise = instance->promise.promise;

        furi_check(jerry_object_delete_native_ptr(promise, &promise_native_info));
        instance->promise.status = ChildStatusDone;
        instance->response.status = ChildStatusRunning;

        jerry_value_t response = create_response(instance, data, size);
        furi_check(!jerry_value_is_exception(response));
        furi_check(jerry_value_is_promise(promise));
        js_runner_check_and_free(jerry_promise_resolve(promise, response));
        jerry_value_free(response);
        jerry_value_free(promise);
    } else {
        FURI_LOG_E(TAG, "Unexpected headers");
    }
    free((void*)data);
}

static void process_error(JsFetch* instance, FuriString* msg) {
    bool free_msg = true;
    if(instance->promise.status == ChildStatusRunning) {
        jerry_value_t promise = instance->promise.promise;
        furi_check(jerry_object_delete_native_ptr(promise, &promise_native_info));

        jerry_value_t error = jerry_string_sz(furi_string_get_cstr(msg));
        jerry_value_free(jerry_promise_reject(promise, error));
        jerry_value_free(error);
        jerry_value_free(promise);
        instance->promise.status = ChildStatusDone;
        instance->response.status = ChildStatusDone;
    }
    if(instance->response.status == ChildStatusRunning) {
        furi_check(false);
    }
    if(instance->sink.status == ChildStatusRunning) {
        DataEventQueue_push_back(
            instance->chunk_queue,
            (DataEvent){
                .type = DataEventTypeError,
                .error = msg,
            });
        free_msg = false;
    }
    if(free_msg) {
        furi_string_free(msg);
    }
    free_if_not_running(instance);
}

static void feed_data_sink(JsFetch* instance) {
    if(instance->sink.on_event) {
        instance->sink.feeding = true;
        while(!DataEventQueue_empty_p(instance->chunk_queue) &&
              instance->sink.status == ChildStatusRunning) {
            DataEvent event;
            DataEventQueue_pop_front(&event, instance->chunk_queue);
            // During this call sink can be deregistered, js objects can be destroyed, but no events can come from the queue
            bool consumed = instance->sink.on_event(instance, &event, instance->sink.context);
            if(!consumed) {
                DataEventQueue_push_front(instance->chunk_queue, event);
                break;
            }
        }
        instance->sink.feeding = false;
        free_if_not_running(instance);
    }
}

static void process_rx_data(JsFetch* instance, void* data, size_t size) {
    if(instance->promise.status != ChildStatusDone ||
       instance->response.status != ChildStatusDone || instance->sink.status != ChildStatusDone) {
        DataEventQueue_push_back(
            instance->chunk_queue,
            (DataEvent){
                .type = DataEventTypeData,
                .data = {
                    .buffer = data,
                    .size = size,
                }});
    } else {
        free(data);
    }
    if(instance->sink.status == ChildStatusRunning) {
        feed_data_sink(instance);
    }
}

static void process_done(JsFetch* instance) {
    if(instance->promise.status != ChildStatusDone ||
       instance->response.status != ChildStatusDone || instance->sink.status != ChildStatusDone) {
        DataEventQueue_push_back(
            instance->chunk_queue,
            (DataEvent){
                .type = DataEventTypeDone,
            });
    }
    if(instance->sink.status == ChildStatusRunning) {
        feed_data_sink(instance);
    }
}

static void process_thread_exit(JsFetch* instance) {
    furi_thread_join(instance->fetch.thread);
    furi_thread_free(instance->fetch.thread);
    instance->app->num_fetch_threads -= 1;
    instance->fetch.thread = NULL;
    instance->fetch.status = ChildStatusDone;
    JsRunnerApp* app = instance->app;
    free_if_not_running(instance);
    js_runner_check_event_loop(app);
}

void js_fetch_process_event(const FetchEvent* event) {
    JsFetch* instance = event->instance;
    switch(event->type) {
    case FetchEventTypeHeaders:
        process_headers(instance, event->data.buf, event->data.size);
        break;
    case FetchEventTypeRxData:
        process_rx_data(instance, event->data.buf, event->data.size);
        break;
    case FetchEventTypeError:
        process_error(instance, event->error.msg);
        break;
    case FetchEventTypeDone:
        process_done(instance);
        break;
    case FetchEventTypeThreadExit:
        process_thread_exit(instance);
        break;
    }
}

bool js_fetch_set_data_sink(
    JsFetch* instance,
    void* callback_context,
    JsFetchDataSinkCallback callback) {
    if(instance->sink.status == ChildStatusNotYet && callback) {
        instance->sink.on_event = callback;
        instance->sink.context = callback_context;
        instance->sink.feeding = false;
        instance->sink.status = ChildStatusRunning;
        js_set_property(instance->response.response, "bodyUsed", jerry_boolean(true));
        feed_data_sink(instance);
        return true;
    } else if(instance->sink.status == ChildStatusRunning && !callback) {
        // Data sink expired and won't accept any more packets
        instance->sink.on_event = NULL;
        instance->sink.context = NULL;
        instance->sink.status = ChildStatusDone;
        free_if_not_running(instance);
        return true;
    }
    return false;
}

void js_fetch_data_sink_ready(JsFetch* instance) {
    if(!instance->sink.feeding) {
        feed_data_sink(instance);
    }
}

bool js_fetch_cancel(JsFetch* instance) {
    if(instance->sink.status == ChildStatusRunning) {
        return false;
    }
    js_set_property(instance->response.response, "bodyUsed", jerry_boolean(true));
    instance->sink.status = ChildStatusDone;
    free_if_not_running(instance);
    return true;
}

void js_runner_setup_fetch(void) {
    js_runner_setup_request();

    jerry_value_t global_obj = jerry_current_realm();
    js_set_method(global_obj, "fetch", fetch);
    jerry_value_free(global_obj);
}
