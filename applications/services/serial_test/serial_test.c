#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <furi.h>

#ifdef STM32U595xx
#define SERIAL_ID FuriHalSerialIdUsart1
#define SERIAL_TEST_INITIATOR
#else
#define SERIAL_ID FuriHalSerialIdUsart0
#endif

#define BAUD_RATE   (11250000UL)
#define BUFFER_SIZE (1024UL)

#define TAG "SerialTest"

typedef enum {
    SerialTestFlagRxAvailable = (1UL << 0),
} SerialTestFlag;

#define SERIAL_TEST_FLAG_ALL (SerialTestFlagRxAvailable)

typedef struct {
    FuriThreadId thread_id;
    uint32_t cycle_counter;
    uint8_t tx_buffer[BUFFER_SIZE];
    uint8_t rx_buffer[BUFFER_SIZE];
} SerialTest;

static void serial_test_fill_buffer(uint8_t* buffer) {
    for(uint32_t i = 0; i < BUFFER_SIZE; ++i) {
        buffer[i] = i & 0xFF;
    }
}

static void serial_test_dump_buffer(uint8_t* buffer) {
    // 32* 32 = 1024
    for(uint32_t col = 0; col < 32; ++col) {
        for(uint32_t row = 0; row < 32; ++row) {
            const uint8_t ch = buffer[col * 32 + row];

            if(ch < 0x10) {
                furi_log_puts("0");
            }

            furi_log_puthex32(ch);
            furi_log_puts(" ");
        }
        furi_log_puts("\r\n");
    }
}

SerialTest* serial_test_alloc(void) {
    SerialTest* instance = malloc(sizeof(SerialTest));
    instance->thread_id = furi_thread_get_current_id();
    serial_test_fill_buffer(instance->tx_buffer);
    return instance;
}

static void serial_test_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    UNUSED(handle);

    SerialTest* instance = context;

    if(event & FuriHalSerialRxEventData) {
        furi_thread_flags_set(instance->thread_id, SerialTestFlagRxAvailable);
    }
}

int serial_test_srv(void* arg) {
    UNUSED(arg);

    SerialTest* instance = serial_test_alloc();

    FuriHalSerialHandle* handle = furi_hal_serial_control_acquire(SERIAL_ID);
    furi_check(handle);

    furi_hal_serial_init(handle, BAUD_RATE);
    furi_hal_serial_set_callback(handle, NULL, serial_test_rx_callback, instance);
    furi_hal_serial_dma_rx_start(handle, instance->rx_buffer, BUFFER_SIZE);
#ifdef SERIAL_TEST_INITIATOR
    furi_hal_serial_dma_tx(handle, instance->tx_buffer, BUFFER_SIZE);
#endif

    for(;;) {
        const uint32_t flags = furi_thread_flags_wait(SERIAL_TEST_FLAG_ALL, FuriFlagWaitAny, 500);

        if(flags & FuriFlagError) {
            if(flags == FuriFlagErrorTimeout) {
#ifdef SERIAL_TEST_INITIATOR
                furi_hal_serial_dma_rx_stop(handle);
                FURI_LOG_E(TAG, "Receive timeout");
#else
                continue;
#endif
            } else {
                furi_crash("Unexpected error");
            }

        } else if(flags & SerialTestFlagRxAvailable) {
            if(memcmp(instance->tx_buffer, instance->rx_buffer, BUFFER_SIZE) != 0) {
                FURI_LOG_E(TAG, "Buffer %lu mismatch!", instance->cycle_counter);
                serial_test_dump_buffer(instance->rx_buffer);

            } else {
                FURI_LOG_I(TAG, "Buffer match!");
            }
        }

        furi_hal_serial_dma_rx_start(handle, instance->rx_buffer, BUFFER_SIZE);
        furi_hal_serial_dma_tx(handle, instance->tx_buffer, BUFFER_SIZE);

        ++instance->cycle_counter;
    }
}
