#include <furi_hal_serial.h>
#include <furi_hal_serial_types_i.h>

#include <furi_hal_resources.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_bus.h>

#include <si91x_device.h>

#define FRAC_BITS (6UL)
#define FRAC_MULTIPLIER (1UL << FRAC_BITS)
#define FRAC_MASK (FRAC_MULTIPLIER - 1UL)

#define LCR_DLAB_POS (7)
#define LCR_DLAB_SET (1U << LCR_DLAB_POS)
#define LCR_DLS_POS (0)
#define LCR_DLS_8BIT (3UL << LCR_DLS_POS)

#define FCR_FIFOE_POS (0)
#define FCR_FIFOE_SET (1UL << FCR_FIFOE_POS)

#define IIR_IID_MODEM_STATUS (0x0)
#define IIR_IID_NO_INTERRUPT (0x1)
#define IIR_IID_THR_EMPTY (0x2)
#define IIR_IID_RX_AVAILABLE (0x4)
#define IIR_IID_LINE_STATUS (0x6)
#define IIR_IID_BUSY_DETECT (0x7)
#define IIR_IID_CHAR_TIMEOUT (0xC)

typedef struct {
    FuriHalSerialHandle* handle;
    FuriHalSerialAsyncRxCallback rx_callback;
    void* context;
} FuriHalSerial;

typedef struct {
    USART0_Type* periph;
    FuriHalInterruptId irq;
} FuriHalSerialConfig;

static const FuriHalSerialConfig furi_hal_serial_config[FuriHalSerialIdMax] = {
    [FuriHalSerialIdUsart0] = {
        .periph = UART0,
        .irq = FuriHalInterruptIdUSART0,
    },
    [FuriHalSerialIdUart1] = {
        .periph = UART1,
        .irq = FuriHalInterruptIdUART1,
    },
    [FuriHalSerialIdUlpuart] = {
        .periph = ULP_UART,
        .irq = FuriHalInterruptIdULPSS_UART,
    },
};

static FuriHalSerial furi_hal_serial[FuriHalSerialIdMax];

void furi_hal_serial_init(FuriHalSerialHandle* handle, uint32_t baud) {
    furi_check(handle);

    if(handle->id == FuriHalSerialIdUsart0) {
        furi_hal_bus_enable(FuriHalBusUSART1_PCLK);
        furi_hal_bus_enable(FuriHalBusUSART1_SCLK);

        // TODO: Prettify clock selection
        // Select SOC PLL clock
        M4CLK->CLK_CONFIG_REG2_b.USART1_SCLK_SEL = 0x01;
        // Wait for the switch to complete
        while ((M4CLK->PLL_STAT_REG_b.USART1_SCLK_SWITCHED) != 1)
          ;
        // No clock division
        M4CLK->CLK_CONFIG_REG2_b.USART1_SCLK_DIV_FAC = 0;

        // Init main pins
        furi_hal_gpio_init_ex(&gpio_usart0_clk, GpioModeInput, GpioPullNo, GpioSpeedHigh, GpioAltFn2USART0_RX);
        furi_hal_gpio_init_ex(&gpio_usart0_rx, GpioModeInput, GpioPullNo, GpioSpeedHigh, GpioAltFn2USART0_RX);
        furi_hal_gpio_init_ex(&gpio_usart0_tx, GpioModeOutputPushPull, GpioPullNo, GpioSpeedHigh, GpioAltFn2USART0_TX);

    } else if(handle->id == FuriHalSerialIdUart1) {
        furi_hal_bus_enable(FuriHalBusUSART2_PCLK);
        furi_hal_bus_enable(FuriHalBusUSART2_SCLK);

        // TODO: Prettify clock selection
        // Select SOC PLL clock
        M4CLK->CLK_CONFIG_REG2_b.USART2_SCLK_SEL = 0x01;
        // Wait for the switch to complete
        while ((M4CLK->PLL_STAT_REG_b.USART2_SCLK_SWITCHED) != 1)
          ;
        // No clock division
        M4CLK->CLK_CONFIG_REG2_b.USART2_SCLK_DIV_FAC = 0;

        // Init main pins
        furi_hal_gpio_init_ex(&gpio_uart1_rx, GpioModeInput, GpioPullNo, GpioSpeedHigh, GpioAltFn6SOCPERH_ON_ULP_GPIO_8);
        furi_hal_gpio_init_ex(&gpio_uart1_tx, GpioModeOutputPushPull, GpioPullNo, GpioSpeedHigh, GpioAltFn6SOCPERH_ON_ULP_GPIO_11);
        // Init virtual (multiplexed) pins
        furi_hal_gpio_init_ex(&gpio_i_uart1_rx, GpioModeInput, GpioPullNo, GpioSpeedHigh, GpioAltFn6UART1_RX);
        furi_hal_gpio_init_ex(&gpio_i_uart1_tx, GpioModeOutputPushPull, GpioPullNo, GpioSpeedHigh, GpioAltFn9UART1_TX);

    } else if(handle->id == FuriHalSerialIdUlpuart) {
        furi_hal_bus_enable(FuriHalBusUlpPCLK_UART);
        furi_hal_bus_enable(FuriHalBusUlpSCLK_UART);

        // TODO: This should be elsewhere (not here)
        // Enable ULP clock from HP domain
        furi_hal_bus_enable(FuriHalBusULPSS_CLK);

        // TODO: Prettify clock selection
        // Select HP to ULP clock
        ULPCLK->ULP_UART_CLK_GEN_REG_b.ULP_UART_CLK_SEL = 6;
        // No clock division
        M4CLK->CLK_CONFIG_REG4_b.ULPSS_CLK_DIV_FAC = 0;

        // Init main pins
        furi_hal_gpio_init_ex(&gpio_ulp_uart_rx, GpioModeInput, GpioPullNo, GpioSpeedHigh, GpioAltFn9ULPPERH_ON_SOC_GPIO_2);
        furi_hal_gpio_init_ex(&gpio_ulp_uart_tx, GpioModeOutputPushPull, GpioPullNo, GpioSpeedHigh, GpioAltFn9ULPPERH_ON_SOC_GPIO_3);
        // Init virtual (multiplexed) pins
        furi_hal_gpio_enable_ulp_on_hp(&gpio_ulp_2, GpioAltFn3ULP_UART_RX);
        furi_hal_gpio_enable_ulp_on_hp(&gpio_ulp_i_3, GpioAltFn3ULP_UART_TX);

    } else {
        furi_crash();
    }

    USART0_Type* periph = furi_hal_serial_config[handle->id].periph;
    // Enable FIFO
    periph->FCR = FCR_FIFOE_SET;

    furi_hal_serial_set_br(handle, baud);
}

void furi_hal_serial_deinit(FuriHalSerialHandle* handle) {
    furi_check(handle);
}

bool furi_hal_serial_is_baud_rate_supported(FuriHalSerialHandle* handle, uint32_t baud) {
    furi_check(handle);
    return baud >= 9600UL && baud <= 7372800UL;
}

void furi_hal_serial_set_br(FuriHalSerialHandle* handle, uint32_t baud) {
    furi_check(handle);

    USART0_Type* periph = furi_hal_serial_config[handle->id].periph;

    /*
     * Integer part:
     *   divisor = PCLK / (baud * 16)
     * Fractional part:
     *   6 bits (1/64...63/64)
     * Multiply both sides by 64:
     *   divisor_64 = (PCLK * 4) / baud
     */

    const uint32_t divisor_64 = (SystemCoreClock * (FRAC_MULTIPLIER / 16)) / baud;
    const uint32_t divisor = divisor_64 >> FRAC_BITS;

    // Enable divisor modification
    periph->LCR = LCR_DLAB_SET;
    // Divisor low 8 bits
    periph->DLL = divisor & 0xFF;
    // Divisor high 8 bits
    periph->DLH = divisor >> 8;
    // Fractional part 6 bits
    periph->DLF = divisor_64 & FRAC_MASK;
    // Disable divisor modification and use 8bit per character
    periph->LCR = LCR_DLS_8BIT;
}

void furi_hal_serial_suspend(FuriHalSerialHandle* handle) {
    furi_check(handle);
}

void furi_hal_serial_resume(FuriHalSerialHandle* handle) {
    furi_check(handle);
}

void furi_hal_serial_tx(FuriHalSerialHandle* handle, const uint8_t* buffer, size_t buffer_size) {
    furi_check(handle);

    USART0_Type* periph = furi_hal_serial_config[handle->id].periph;

    while(buffer_size > 0) {
        while(!periph->USR_b.TFNF)
            ;

        periph->THR = *buffer;

        ++buffer;
        --buffer_size;
    }
}

void furi_hal_serial_tx_wait_complete(FuriHalSerialHandle* handle) {
    furi_check(handle);

    USART0_Type* periph = furi_hal_serial_config[handle->id].periph;

    while(!periph->USR_b.TFE)
        ;
}

static void furi_hal_serial_event_init(FuriHalSerialHandle* handle, bool report_errors) {
    UNUSED(handle);
    UNUSED(report_errors);
}

static void furi_hal_serial_rx_irq_callback(void* context) {
    furi_assert(context);

    FuriHalSerialRxEvent event = 0;

    FuriHalSerialHandle* handle = context;
    const FuriHalSerial* serial = &furi_hal_serial[handle->id];
    const FuriHalSerialConfig* config = &furi_hal_serial_config[handle->id];

    if(config->periph->IIR_b.IID == IIR_IID_RX_AVAILABLE) {
        event |= FuriHalSerialRxEventData;
    }

    if(serial->rx_callback) {
        serial->rx_callback(handle, event, serial->context);
    }
}

static void furi_hal_serial_async_rx_configure(
    FuriHalSerialHandle* handle,
    FuriHalSerialAsyncRxCallback callback,
    void* context) {

    FuriHalSerial* serial = &furi_hal_serial[handle->id];
    const FuriHalSerialConfig* config = &furi_hal_serial_config[handle->id];

    serial->handle = handle;
    serial->rx_callback = callback;
    serial->context = context;

    if(callback) {
        furi_hal_interrupt_set_isr(config->irq, furi_hal_serial_rx_irq_callback, handle);
        config->periph->IER_b.ERBFI = 1;
    } else {
        furi_hal_interrupt_set_isr(config->irq, NULL, NULL);
        config->periph->IER_b.ERBFI = 0;
    }
}

void furi_hal_serial_async_rx_start(
    FuriHalSerialHandle* handle,
    FuriHalSerialAsyncRxCallback callback,
    void* context,
    bool report_errors) {
    furi_check(handle);
    furi_check(callback);

    furi_hal_serial_event_init(handle, report_errors);
    furi_hal_serial_async_rx_configure(handle, callback, context);
}

void furi_hal_serial_async_rx_stop(FuriHalSerialHandle* handle) {
    furi_check(handle);
}

bool furi_hal_serial_async_rx_available(FuriHalSerialHandle* handle) {
    furi_check(FURI_IS_IRQ_MODE());
    furi_check(handle->id < FuriHalSerialIdMax);

    return furi_hal_serial_config[handle->id].periph->USR_b.RFNE;
}

uint8_t furi_hal_serial_async_rx(FuriHalSerialHandle* handle) {
    furi_check(FURI_IS_IRQ_MODE());
    furi_check(handle->id < FuriHalSerialIdMax);

    return furi_hal_serial_config[handle->id].periph->RBR;
}
