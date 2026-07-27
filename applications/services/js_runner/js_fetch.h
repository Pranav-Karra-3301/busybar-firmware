#pragma once
#include "js_runner_i.h"
#include <m-deque.h>

typedef enum DataEventType {
    DataEventTypeData,
    DataEventTypeDone,
    DataEventTypeError,
} DataEventType;

typedef struct DataEvent {
    DataEventType type;
    union {
        struct {
            void* buffer;
            size_t size;
        } data;
        FuriString* error;
    };
} DataEvent;

M_DEQUE_DEF(DataEventQueue, DataEvent, M_POD_OPLIST);

typedef bool (
    *JsFetchDataSinkCallback)(JsFetch* instance, DataEvent* event, void* callback_context);

typedef struct JsFetch {
    JsRunnerApp* app;
    FuriMessageQueue* event_queue;
    FetchRequest request;

    DataEventQueue_t chunk_queue;

    struct {
        ChildStatus status;
        FuriThread* thread;
        Fetch* fetch;
    } fetch;
    struct {
        ChildStatus status;
        JsFetchDataSinkCallback on_event;
        void* context;
        bool feeding;
    } sink;
    struct {
        ChildStatus status;
        jerry_value_t promise;
    } promise;
    struct {
        ChildStatus status;
        jerry_value_t response;
    } response;
} JsFetch;

void js_fetch_process_event(const FetchEvent* event);

/**
 * @param callback if NULL, data sink is no more
 */
bool js_fetch_set_data_sink(
    JsFetch* instance,
    void* callback_context,
    JsFetchDataSinkCallback callback);
void js_fetch_data_sink_ready(JsFetch* instance);

/**
 * @param buffer if NULL, no more chunks
 * @return true if data is consumed
 */
