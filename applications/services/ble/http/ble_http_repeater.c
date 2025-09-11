#include "../ble.h"
#include <mongoose.h>
#include <network/network.h>

#define TAG "BleHttp"

#define BLE_HTTP_HOST "http://127.0.0.1:80"

typedef struct {
    struct mg_mgr mgr;
    struct mg_connection* conn;
    FuriThread* thread;
    FuriSemaphore* wait;
    Ble* ble;
    Network* network;
    bool exit;
} BleHttpRepeater;

static BleHttpRepeater* ble_http_repeater;

static void ble_uart_rx_callback(size_t data_size, void* data, void* context) {
    furi_assert(context);
    BleHttpRepeater* instance = context;
    if(data_size > 0) {
        mg_wakeup(&instance->mgr, instance->conn->id, data, data_size);
    }
}

static void ble_uart_tx_done_callback(void* context) {
    furi_assert(context);
    BleHttpRepeater* instance = context;
    furi_semaphore_release(instance->wait);
}

static void ble_event_handler(struct mg_connection* conn, int ev, void* ev_data) {
    BleHttpRepeater* ble_http = ble_http_repeater;

    if(ev == MG_EV_WAKEUP) {
        struct mg_str* data = (struct mg_str*)ev_data;
        mg_send(conn, data->buf, data->len);
    } else if(ev == MG_EV_READ) {
        size_t total_size = conn->recv.len;
        size_t index = 0;
        while(total_size) {
            size_t send_size = total_size > 180 ? 180 : total_size;
            ble_uart_tx_data(
                ble_http->ble, BleUartChannelNordic, &conn->recv.buf[index], send_size);

            if(furi_semaphore_acquire(ble_http->wait, 1000) != FuriStatusOk) {
                FURI_LOG_W(TAG, "Error during send process");
                break;
            }

            index += send_size;
            total_size -= send_size;
        }
        conn->recv.len = 0;
    } else if(ev == MG_EV_CLOSE) {
        ble_http->conn = mg_connect(&ble_http->mgr, BLE_HTTP_HOST, ble_event_handler, ble_http);
    }
}

static int32_t ble_http_repeater_thread_handler(void* p) {
    BleHttpRepeater* ble_http = p;
    network_init_current_thread(ble_http_repeater->network);

    mg_mgr_init(&ble_http->mgr);
    mg_wakeup_init(&ble_http->mgr);

    ble_http->conn = mg_connect(&ble_http->mgr, BLE_HTTP_HOST, ble_event_handler, ble_http);

    // Event loop
    while(!ble_http->exit) {
        mg_mgr_poll(&ble_http->mgr, 1000);
    }

    // Cleanup
    mg_mgr_free(&ble_http->mgr);
    network_deinit_current_thread(ble_http_repeater->network);

    return 0;
}

void ble_http_repeater_start(Ble* ble) {
    furi_assert(ble);
    FURI_LOG_D(TAG, "Start ble repeater");
    __FuriCriticalInfo info = __furi_critical_enter();
    if(ble_http_repeater == NULL) {
        ble_http_repeater = malloc(sizeof(BleHttpRepeater));
        ble_http_repeater->ble = ble;
        ble_http_repeater->wait = furi_semaphore_alloc(1, 0);

        ble_uart_set_rx_callback(
            ble, BleUartChannelNordic, ble_uart_rx_callback, ble_http_repeater);
        ble_uart_set_tx_done_callback(
            ble, BleUartChannelNordic, ble_uart_tx_done_callback, ble_http_repeater);

        ble_http_repeater->network = furi_record_open(RECORD_NETWORK);
        network_init_current_thread(ble_http_repeater->network);

        ble_http_repeater->thread = furi_thread_alloc_ex(
            TAG, 1024 * 8, ble_http_repeater_thread_handler, ble_http_repeater);
        furi_thread_start(ble_http_repeater->thread);
    }
    __furi_critical_exit(info);
}

void ble_http_repeater_stop() {
    do {
        __FuriCriticalInfo info = __furi_critical_enter();

        if(!ble_http_repeater) {
            __furi_critical_exit(info);
            break;
        }

        BleHttpRepeater* temp = ble_http_repeater;
        ble_http_repeater = NULL;
        temp->exit = true;

        __furi_critical_exit(info);
        furi_thread_join(temp->thread);
        furi_thread_free(temp->thread);
        furi_semaphore_free(temp->wait);
        network_deinit_current_thread(temp->network);
        furi_record_close(RECORD_NETWORK);

        free(temp);
    } while(false);
}
