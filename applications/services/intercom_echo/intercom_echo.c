#include <furi.h>

#include <intercom/intercom.h>

#define TAG "IntercomEchoSrv"

#define RX_BUF_SIZE (256U)
#define MESSAGE     "Hello there!"

typedef struct {
    Intercom* intercom;
    FuriEventLoop* event_loop;
    FuriSemaphore* rx_semaphore;
    FuriEventLoopTimer* tx_timer;
    uint8_t rx_data[RX_BUF_SIZE];
    size_t rx_data_size;
} IntercomEchoService;

static void intercom_echo_service_timer_callback(void* context) {
    IntercomEchoService* instance = context;
    furi_check(intercom_tx(instance->intercom, MESSAGE, strlen(MESSAGE), 0) == strlen(MESSAGE));

    FURI_LOG_I(TAG, "Payload sent");
}

static void intercom_echo_service_semaphore_callback(FuriEventLoopObject* object, void* context) {
    IntercomEchoService* instance = context;
    furi_check(object == instance->rx_semaphore);
    furi_event_loop_timer_start(instance->tx_timer, 500);
    furi_check(furi_semaphore_acquire(instance->rx_semaphore, 0) == FuriStatusOk);
}

static void
    intercom_echo_service_intercom_rx_callback(const void* data, size_t data_size, void* context) {
    furi_check(data_size <= RX_BUF_SIZE);
    IntercomEchoService* instance = context;

    if(data_size != strlen(MESSAGE)) {
        FURI_LOG_E(TAG, "Length mismatch: expected %u, got %zu", strlen(MESSAGE), data_size);
    } else {
        memcpy(instance->rx_data, data, data_size);
        instance->rx_data_size = data_size;

        if(memcmp(instance->rx_data, MESSAGE, data_size) == 0) {
            FURI_LOG_I(TAG, "Payload received");
        } else {
            FURI_LOG_E(
                TAG,
                "Payload mismatch: expected: '%s', got '%s'",
                MESSAGE,
                (const char*)instance->rx_data);
        }
    }

    furi_check(furi_semaphore_release(instance->rx_semaphore) == FuriStatusOk);
}

static IntercomEchoService* intercom_echo_service_alloc(void) {
    IntercomEchoService* instance = malloc(sizeof(IntercomEchoService));

    instance->event_loop = furi_event_loop_alloc();
    instance->rx_semaphore = furi_semaphore_alloc(1, 1);
    instance->tx_timer = furi_event_loop_timer_alloc(
        instance->event_loop,
        intercom_echo_service_timer_callback,
        FuriEventLoopTimerTypeOnce,
        instance);

    furi_event_loop_subscribe_semaphore(
        instance->event_loop,
        instance->rx_semaphore,
        FuriEventLoopEventIn,
        intercom_echo_service_semaphore_callback,
        instance);

    instance->intercom = furi_record_open(RECORD_INTERCOM);
    intercom_set_rx_callback(
        instance->intercom, intercom_echo_service_intercom_rx_callback, instance);

    return instance;
}

int32_t intercom_echo_srv(void* arg) {
    UNUSED(arg);

    IntercomEchoService* instance = intercom_echo_service_alloc();
    furi_event_loop_run(instance->event_loop);

    furi_crash();
}
