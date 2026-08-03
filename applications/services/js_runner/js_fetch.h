/**
 * @file js_fetch.h
 *
 * @brief fetch() JS API implementation
 */
#pragma once
#include "js_runner_i.h"
#include <m-deque.h>

typedef struct JsFetch JsFetch;

typedef enum JsFetchEventType {
    JsFetchEventTypeHeaders,
    JsFetchEventTypeRxData,
    JsFetchEventTypeError,
    JsFetchEventTypeDone,
    JsFetchEventTypeThreadExit,
} JsFetchEventType;

typedef struct JsFetchEvent {
    JsFetchEventType type;
    JsFetch* instance;
    union {
        struct {
            void* buf;
            size_t size;
        } data;
        struct {
            FuriString* msg;
        } error;
    };
} JsFetchEvent;

typedef enum JsFetchDataEventType {
    JsFetchDataEventTypeData,
    JsFetchDataEventTypeDone,
    JsFetchDataEventTypeError,
} JsFetchDataEventType;

typedef struct JsFetchDataEvent {
    JsFetchDataEventType type;
    union {
        struct {
            void* buffer;
            size_t size;
        } data;
        FuriString* error;
    };
} JsFetchDataEvent;

M_DEQUE_DEF(DataEventQueue, JsFetchDataEvent, M_POD_OPLIST);

typedef bool (
    *JsFetchDataSinkCallback)(JsFetch* instance, JsFetchDataEvent* event, void* callback_context);

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

extern const jerry_object_native_info_t js_fetch_response_native_info;

void js_setup_fetch(void);

void js_fetch_process_event(const JsFetchEvent* event);

/**
 * @brief Set a data sink which receives incoming data chunks.
 *
 * Data sink, when set, has an exclusive access to data chunks through its callback.
 * It is not possible to set another data sink.
 *
 * @param instance JsFetch instance
 * @param callback_context user pointer to be passed to the callback
 * @param callback if NULL, data sink is no more
 * @return true if data sink has been set, false if another data sink is or has already been active.
 */
bool js_fetch_set_data_sink(
    JsFetch* instance,
    JsFetchDataSinkCallback callback,
    void* callback_context);

/**
 * @brief Indicate that a data sink is ready to receive new data.
 * @param instance JsFetch instance
 */
void js_fetch_data_sink_ready(JsFetch* instance);

/**
 * Cancel ongoing fetch activity
 *
 * @param instance JsFetch instance
 */
bool js_fetch_cancel(JsFetch* instance);

/**
 * @param buffer if NULL, no more chunks
 * @return true if data is consumed
 */
