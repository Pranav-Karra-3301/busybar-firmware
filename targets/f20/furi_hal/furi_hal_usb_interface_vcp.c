#include <furi_hal.h>
#include <tusb.h>
#include "furi_hal_usb_i.h"
#include "furi_hal_usb_interface_i.h"
#include "class/cdc/cdc.h"

#define TAG "USB CDC"

#define CDC_EP_BUF_SIZE 1024
#define CDC_TX_BUF_SIZE 1024
#define CDC_RX_BUF_SIZE 2048

typedef struct {
    uint8_t itf_num;
    uint8_t ep_notif;
    uint8_t ep_in;
    uint8_t ep_out;

    uint8_t line_state;

    TU_ATTR_ALIGNED(4) CdcLineCoding line_coding;

    tu_fifo_t rx_ff;
    tu_fifo_t tx_ff;

    uint8_t rx_ff_buf[CDC_RX_BUF_SIZE];
    uint8_t tx_ff_buf[CDC_TX_BUF_SIZE];

    SemaphoreHandle_t rx_ff_mutex;
    SemaphoreHandle_t tx_ff_mutex;

    // Endpoint Transfer buffer
    CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CDC_EP_BUF_SIZE];
    CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CDC_EP_BUF_SIZE];

    CdcCallbacks callbacks;
    void* context;
} CdcdInterface;

CFG_TUD_MEM_SECTION tu_static CdcdInterface _cdcd_itf;

static bool cdc_prep_out_transaction(CdcdInterface* p_cdc) {
    uint8_t const rhport = 0;
    uint16_t available = tu_fifo_remaining(&p_cdc->rx_ff);

    // Prepare for incoming data but only allow what we can store in the ring buffer.
    // TODO Actually we can still carry out the transfer, keeping count of received bytes
    // and slowly move it to the FIFO when read().
    // This pre-check reduces endpoint claiming
    TU_VERIFY(available >= sizeof(p_cdc->epout_buf));

    // claim endpoint
    TU_VERIFY(usbd_edpt_claim(rhport, p_cdc->ep_out));

    // fifo can be changed before endpoint is claimed
    available = tu_fifo_remaining(&p_cdc->rx_ff);

    if(available >= sizeof(p_cdc->epout_buf)) {
        return usbd_edpt_xfer(rhport, p_cdc->ep_out, p_cdc->epout_buf, sizeof(p_cdc->epout_buf));
    } else {
        // Release endpoint since we don't make any transfer
        usbd_edpt_release(rhport, p_cdc->ep_out);

        return false;
    }
}

void* usbd_cdc_init(void* settings) {
    tu_memclr(&_cdcd_itf, sizeof(_cdcd_itf));

    CdcContext* cfg = settings;

    CdcdInterface* p_cdc = &_cdcd_itf;

    // default line coding is : stop bit = 1, parity = none, data bits = 8
    p_cdc->line_coding.bit_rate = 115200;
    p_cdc->line_coding.stop_bits = 0;
    p_cdc->line_coding.parity = 0;
    p_cdc->line_coding.data_bits = 8;

    // Config RX fifo
    tu_fifo_config(&p_cdc->rx_ff, p_cdc->rx_ff_buf, TU_ARRAY_SIZE(p_cdc->rx_ff_buf), 1, false);

    // Config TX fifo as overwritable at initialization and will be changed to non-overwritable
    // if terminal supports DTR bit. Without DTR we do not know if data is actually polled by terminal.
    // In this way, the most current data is prioritized.
    tu_fifo_config(&p_cdc->tx_ff, p_cdc->tx_ff_buf, TU_ARRAY_SIZE(p_cdc->tx_ff_buf), 1, true);

    // TODO: furi osal for tinyusb
    p_cdc->rx_ff_mutex = xSemaphoreCreateMutex();
    p_cdc->tx_ff_mutex = xSemaphoreCreateMutex();

    tu_fifo_config_mutex(&p_cdc->rx_ff, NULL, p_cdc->rx_ff_mutex);
    tu_fifo_config_mutex(&p_cdc->tx_ff, p_cdc->tx_ff_mutex, NULL);

    if(cfg) {
        memcpy(&(_cdcd_itf.callbacks), cfg->callbacks, sizeof(CdcCallbacks));
        _cdcd_itf.context = cfg->context;
    } else {
        memset(&(_cdcd_itf.callbacks), 0, sizeof(CdcCallbacks));
        _cdcd_itf.context = NULL;
    }

    return NULL;
}

void usbd_cdc_deinit(void) {
    CdcdInterface* p_cdc = &_cdcd_itf;
    memset(&(p_cdc->callbacks), 0, sizeof(CdcCallbacks));
    _cdcd_itf.context = NULL;

    tu_fifo_config_mutex(&p_cdc->rx_ff, NULL, NULL);
    tu_fifo_config_mutex(&p_cdc->tx_ff, NULL, NULL);

    p_cdc->ep_in = 0;
    p_cdc->ep_out = 0;
    p_cdc->ep_notif = 0;

    vSemaphoreDelete(p_cdc->rx_ff_mutex);
    vSemaphoreDelete(p_cdc->tx_ff_mutex);

    tu_memclr(&_cdcd_itf, sizeof(_cdcd_itf));
}

void usbd_cdc_reset(uint8_t rhport) {
    (void)rhport;

    CdcdInterface* p_cdc = &_cdcd_itf;

    p_cdc->itf_num = 0;
    p_cdc->ep_notif = 0;
    p_cdc->ep_in = 0;
    p_cdc->ep_out = 0;
    p_cdc->line_state = 0;

    tu_fifo_clear(&p_cdc->rx_ff);
    tu_fifo_clear(&p_cdc->tx_ff);
    tu_fifo_set_overwritable(&p_cdc->tx_ff, true);
}

uint16_t usbd_cdc_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len) {
    // Only support ACM subclass
    TU_VERIFY(
        TUSB_CLASS_CDC == itf_desc->bInterfaceClass &&
            CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL == itf_desc->bInterfaceSubClass,
        0);

    CdcdInterface* p_cdc = &_cdcd_itf;

    //------------- Control Interface -------------//
    p_cdc->itf_num = itf_desc->bInterfaceNumber;

    uint16_t drv_len = sizeof(tusb_desc_interface_t);
    uint8_t const* p_desc = tu_desc_next(itf_desc);

    // Communication Functional Descriptors
    while(TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc) && drv_len <= max_len) {
        drv_len += tu_desc_len(p_desc);
        p_desc = tu_desc_next(p_desc);
    }

    if(TUSB_DESC_ENDPOINT == tu_desc_type(p_desc)) {
        // notification endpoint
        tusb_desc_endpoint_t const* desc_ep = (tusb_desc_endpoint_t const*)p_desc;

        TU_ASSERT(usbd_edpt_open(rhport, desc_ep), 0);
        p_cdc->ep_notif = desc_ep->bEndpointAddress;

        drv_len += tu_desc_len(p_desc);
        p_desc = tu_desc_next(p_desc);
    }

    //------------- Data Interface (if any) -------------//
    if((TUSB_DESC_INTERFACE == tu_desc_type(p_desc)) &&
       (TUSB_CLASS_CDC_DATA == ((tusb_desc_interface_t const*)p_desc)->bInterfaceClass)) {
        // next to endpoint descriptor
        drv_len += tu_desc_len(p_desc);
        p_desc = tu_desc_next(p_desc);

        // Open endpoint pair
        TU_ASSERT(
            usbd_open_edpt_pair(rhport, p_desc, 2, TUSB_XFER_BULK, &p_cdc->ep_out, &p_cdc->ep_in),
            0);

        drv_len += 2 * sizeof(tusb_desc_endpoint_t);
    }

    // Prepare for incoming data
    cdc_prep_out_transaction(p_cdc);

    return drv_len;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool usbd_cdc_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    // Handle class request only
    TU_VERIFY(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);

    CdcdInterface* p_cdc = &_cdcd_itf;

    switch(request->bRequest) {
    case CDC_REQUEST_SET_LINE_CODING:
        if(stage == CONTROL_STAGE_SETUP) {
            FURI_LOG_I(TAG, "Set Line Coding");
            tud_control_xfer(rhport, request, &p_cdc->line_coding, sizeof(CdcLineCoding));
        } else if(stage == CONTROL_STAGE_ACK) {
            if(p_cdc->callbacks.config_callback) {
                p_cdc->callbacks.config_callback(&p_cdc->line_coding, p_cdc->context);
            }
        }
        break;

    case CDC_REQUEST_GET_LINE_CODING:
        if(stage == CONTROL_STAGE_SETUP) {
            FURI_LOG_I(TAG, "Get Line Coding");
            tud_control_xfer(rhport, request, &p_cdc->line_coding, sizeof(CdcLineCoding));
        }
        break;

    case CDC_REQUEST_SET_CONTROL_LINE_STATE:
        if(stage == CONTROL_STAGE_SETUP) {
            tud_control_status(rhport, request);
        } else if(stage == CONTROL_STAGE_ACK) {
            // CDC PSTN v1.2 section 6.3.12
            // Bit 0: Indicates if DTE is present or not.
            //        This signal corresponds to V.24 signal 108/2 and RS-232 signal DTR (Data Terminal Ready)
            // Bit 1: Carrier control for half-duplex modems.
            //        This signal corresponds to V.24 signal 105 and RS-232 signal RTS (Request to Send)
            bool const dtr = tu_bit_test(request->wValue, 0);
            bool const rts = tu_bit_test(request->wValue, 1);

            p_cdc->line_state = (uint8_t)request->wValue;

            // Disable fifo overwriting if DTR bit is set
            tu_fifo_set_overwritable(&p_cdc->tx_ff, !dtr);

            FURI_LOG_I(TAG, "Set Control Line State: DTR = %d, RTS = %d", dtr, rts);

            // Invoke callback
            if(p_cdc->callbacks.ctrl_line_callback) {
                p_cdc->callbacks.ctrl_line_callback(dtr, rts, p_cdc->context);
            }
        }
        break;
    case CDC_REQUEST_SEND_BREAK:
        if(stage == CONTROL_STAGE_SETUP) {
            tud_control_status(rhport, request);
        } else if(stage == CONTROL_STAGE_ACK) {
            FURI_LOG_I(TAG, "Send Break");
            if(p_cdc->callbacks.send_break_callback) {
                p_cdc->callbacks.send_break_callback(request->wValue, p_cdc->context);
            }
        }
        break;

    default:
        return false; // stall unsupported request
    }

    return true;
}

bool usbd_cdc_xfer_cb(
    uint8_t rhport,
    uint8_t ep_addr,
    xfer_result_t result,
    uint32_t xferred_bytes) {
    (void)result;
    CdcdInterface* p_cdc = &_cdcd_itf;

    // Received new data
    if(ep_addr == p_cdc->ep_out) {
        tu_fifo_write_n(&p_cdc->rx_ff, p_cdc->epout_buf, (uint16_t)xferred_bytes);

        // invoke receive callback (if there is still data)
        if(p_cdc->callbacks.rx_callback && !tu_fifo_empty(&p_cdc->rx_ff)) {
            p_cdc->callbacks.rx_callback(p_cdc->context);
        }

        // prepare for OUT transaction
        cdc_prep_out_transaction(p_cdc);
    }

    // Data sent to host, we continue to fetch from tx fifo to send.
    // Note: This will cause incorrect baudrate set in line coding.
    //       Though maybe the baudrate is not really important !!!
    if(ep_addr == p_cdc->ep_in) {
        // invoke transmit callback to possibly refill tx fifo
        if(p_cdc->callbacks.tx_done_callback) {
            p_cdc->callbacks.tx_done_callback(p_cdc->context);
        }

        if(furi_hal_cdc_write_flush() == 0) {
            // If there is no data left, a ZLP should be sent if
            // xferred_bytes is multiple of EP Packet size and not zero
            if(!tu_fifo_count(&p_cdc->tx_ff) && xferred_bytes &&
               (0 == (xferred_bytes & (CDC_MAX_PACKET_LEN - 1)))) {
                if(usbd_edpt_claim(rhport, p_cdc->ep_in)) {
                    usbd_edpt_xfer(rhport, p_cdc->ep_in, NULL, 0);
                }
            }
        }
    }

    // nothing to do with notif endpoint for now

    return true;
}

bool furi_hal_cdc_is_connected(void) {
    // DTR (bit 0) active  is considered as connected
    return tud_ready() && tu_bit_test(_cdcd_itf.line_state, 0);
}

uint8_t furi_hal_cdc_get_line_state(void) {
    return _cdcd_itf.line_state;
}

void furi_hal_cdc_get_line_coding(CdcLineCoding* coding) {
    (*coding) = _cdcd_itf.line_coding;
}

uint32_t furi_hal_cdc_available(void) {
    return tu_fifo_count(&_cdcd_itf.rx_ff);
}

uint32_t furi_hal_cdc_read(void* buffer, uint32_t bufsize) {
    CdcdInterface* p_cdc = &_cdcd_itf;
    uint32_t num_read =
        tu_fifo_read_n(&p_cdc->rx_ff, buffer, (uint16_t)TU_MIN(bufsize, UINT16_MAX));
    cdc_prep_out_transaction(p_cdc);
    return num_read;
}

bool furi_hal_cdc_peek(uint8_t* chr) {
    return tu_fifo_peek(&_cdcd_itf.rx_ff, chr);
}

void furi_hal_cdc_read_flush(void) {
    CdcdInterface* p_cdc = &_cdcd_itf;
    tu_fifo_clear(&p_cdc->rx_ff);
    cdc_prep_out_transaction(p_cdc);
}

uint32_t furi_hal_cdc_write(void const* buffer, uint32_t bufsize) {
    CdcdInterface* p_cdc = &_cdcd_itf;
    uint16_t ret = tu_fifo_write_n(&p_cdc->tx_ff, buffer, (uint16_t)TU_MIN(bufsize, UINT16_MAX));

    // flush if queue more than packet size
    // may need to suppress -Wunreachable-code since most of the time CDC_INTF_NUMBER_TX_BUFSIZE < CDC_MAX_PACKET_LEN
    if((tu_fifo_count(&p_cdc->tx_ff) >= CDC_MAX_PACKET_LEN) ||
       ((CDC_TX_BUF_SIZE < CDC_MAX_PACKET_LEN) && tu_fifo_full(&p_cdc->tx_ff))) {
        furi_hal_cdc_write_flush();
    }

    return ret;
}

uint32_t furi_hal_cdc_write_flush(void) {
    CdcdInterface* p_cdc = &_cdcd_itf;

    // Skip if usb is not ready yet
    TU_VERIFY(tud_ready(), 0);

    // No data to send
    if(!tu_fifo_count(&p_cdc->tx_ff)) return 0;

    uint8_t const rhport = 0;

    // Claim the endpoint
    TU_VERIFY(usbd_edpt_claim(rhport, p_cdc->ep_in), 0);

    // Pull data from FIFO
    uint16_t const count = tu_fifo_read_n(&p_cdc->tx_ff, p_cdc->epin_buf, sizeof(p_cdc->epin_buf));

    if(count) {
        TU_ASSERT(usbd_edpt_xfer(rhport, p_cdc->ep_in, p_cdc->epin_buf, count), 0);
        return count;
    } else {
        // Release endpoint since we don't make any transfer
        // Note: data is dropped if terminal is not connected
        usbd_edpt_release(rhport, p_cdc->ep_in);
        return 0;
    }
}

uint32_t furi_hal_cdc_write_available(void) {
    return tu_fifo_remaining(&_cdcd_itf.tx_ff);
}

bool furi_hal_cdc_write_clear(void) {
    return tu_fifo_clear(&_cdcd_itf.tx_ff);
}
