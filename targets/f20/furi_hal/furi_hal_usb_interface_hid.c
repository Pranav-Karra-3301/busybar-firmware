#include <furi_hal.h>
#include <tusb.h>
#include "furi_hal_usb_i.h"
#include "furi_hal_usb_interface_i.h"
#include "class/hid/hid_device.h"

#define TAG "USB HID"

#define HID_EP_BUF_SIZE 64

typedef struct {
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t ep_out; // optional Out endpoint
    uint8_t itf_protocol; // Boot mouse or keyboard

    uint8_t protocol_mode; // Boot (0) or Report protocol (1)
    uint8_t idle_rate; // up to application to handle idle rate
    uint16_t report_desc_len;

    CFG_TUSB_MEM_ALIGN uint8_t epin_buf[HID_EP_BUF_SIZE];
    CFG_TUSB_MEM_ALIGN uint8_t epout_buf[HID_EP_BUF_SIZE];

    // TODO save hid descriptor since host can specifically request this after enumeration
    // Note: HID descriptor may be not available from application after enumeration
    tusb_hid_descriptor_hid_t const* hid_descriptor;
} hidd_interface_t;

CFG_TUD_MEM_SECTION tu_static hidd_interface_t _hidd_itf;

void* usbd_hid_init(void* cfg) {
    UNUSED(cfg);
    tu_memclr(&_hidd_itf, sizeof(_hidd_itf));

    return NULL;
}

void usbd_hid_deinit(void) {
}

void usbd_hid_reset(uint8_t rhport) {
    (void)rhport;
    tu_memclr(&_hidd_itf, sizeof(_hidd_itf));
}

uint16_t usbd_hid_open(uint8_t rhport, tusb_desc_interface_t const* desc_itf, uint16_t max_len) {
    TU_VERIFY(TUSB_CLASS_HID == desc_itf->bInterfaceClass, 0);

    // len = interface + hid + n*endpoints
    uint16_t const drv_len =
        (uint16_t)(sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) +
                   desc_itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t));
    TU_ASSERT(max_len >= drv_len, 0);

    // Find available interface
    hidd_interface_t* p_hid = &_hidd_itf;

    uint8_t const* p_desc = (uint8_t const*)desc_itf;

    //------------- HID descriptor -------------//
    p_desc = tu_desc_next(p_desc);
    TU_ASSERT(HID_DESC_TYPE_HID == tu_desc_type(p_desc), 0);
    p_hid->hid_descriptor = (tusb_hid_descriptor_hid_t const*)p_desc;

    //------------- Endpoint Descriptor -------------//
    p_desc = tu_desc_next(p_desc);
    TU_ASSERT(
        usbd_open_edpt_pair(
            rhport,
            p_desc,
            desc_itf->bNumEndpoints,
            TUSB_XFER_INTERRUPT,
            &p_hid->ep_out,
            &p_hid->ep_in),
        0);

    if(desc_itf->bInterfaceSubClass == HID_SUBCLASS_BOOT)
        p_hid->itf_protocol = desc_itf->bInterfaceProtocol;

    p_hid->protocol_mode = HID_PROTOCOL_REPORT; // Per Specs: default is report mode
    p_hid->itf_num = desc_itf->bInterfaceNumber;

    // Use offsetof to avoid pointer to the odd/misaligned address
    p_hid->report_desc_len = tu_unaligned_read16(
        (uint8_t const*)p_hid->hid_descriptor +
        offsetof(tusb_hid_descriptor_hid_t, wReportLength));

    // Prepare for output endpoint
    if(p_hid->ep_out) {
        if(!usbd_edpt_xfer(rhport, p_hid->ep_out, p_hid->epout_buf, sizeof(p_hid->epout_buf))) {
            TU_LOG_FAILED();
            TU_BREAKPOINT();
        }
    }

    return drv_len;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool usbd_hid_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);

    hidd_interface_t* p_hid = &_hidd_itf;

    if(request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
        //------------- STD Request -------------//
        if(stage == CONTROL_STAGE_SETUP) {
            uint8_t const desc_type = tu_u16_high(request->wValue);
            //uint8_t const desc_index = tu_u16_low (request->wValue);

            if(request->bRequest == TUSB_REQ_GET_DESCRIPTOR && desc_type == HID_DESC_TYPE_HID) {
                TU_VERIFY(p_hid->hid_descriptor);
                TU_VERIFY(tud_control_xfer(
                    rhport,
                    request,
                    (void*)(uintptr_t)p_hid->hid_descriptor,
                    p_hid->hid_descriptor->bLength));
            } else if(
                request->bRequest == TUSB_REQ_GET_DESCRIPTOR &&
                desc_type == HID_DESC_TYPE_REPORT) {
                uint8_t const* desc_report = usbd_get_report_desc();
                tud_control_xfer(
                    rhport, request, (void*)(uintptr_t)desc_report, p_hid->report_desc_len);
            } else {
                return false; // stall unsupported request
            }
        }
    } else if(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS) {
        //------------- Class Specific Request -------------//
        switch(request->bRequest) {
        case HID_REQ_CONTROL_GET_REPORT:
            if(stage == CONTROL_STAGE_SETUP) {
                // uint8_t const report_type = tu_u16_high(request->wValue);
                uint8_t const report_id = tu_u16_low(request->wValue);

                uint8_t* report_buf = p_hid->epin_buf;
                uint16_t req_len = tu_min16(request->wLength, CFG_TUD_HID_EP_BUFSIZE);

                uint16_t xferlen = 0;

                // If host request a specific Report ID, add ID to as 1 byte of response
                if((report_id != HID_REPORT_TYPE_INVALID) && (req_len > 1)) {
                    *report_buf++ = report_id;
                    req_len--;

                    xferlen++;
                }

                xferlen += 0;
                FURI_LOG_W(TAG, "TODO: get_report");
                TU_ASSERT(xferlen > 0);

                tud_control_xfer(rhport, request, p_hid->epin_buf, xferlen);
            }
            break;

        case HID_REQ_CONTROL_SET_REPORT:
            if(stage == CONTROL_STAGE_SETUP) {
                TU_VERIFY(request->wLength <= sizeof(p_hid->epout_buf));
                tud_control_xfer(rhport, request, p_hid->epout_buf, request->wLength);
            } else if(stage == CONTROL_STAGE_ACK) {
                // uint8_t const report_type = tu_u16_high(request->wValue);
                uint8_t const report_id = tu_u16_low(request->wValue);

                uint8_t const* report_buf = p_hid->epout_buf;
                uint16_t report_len = tu_min16(request->wLength, CFG_TUD_HID_EP_BUFSIZE);

                // If host request a specific Report ID, extract report ID in buffer before invoking callback
                if((report_id != HID_REPORT_TYPE_INVALID) && (report_len > 1) &&
                   (report_id == report_buf[0])) {
                    report_buf++;
                    report_len--;
                }

                FURI_LOG_W(TAG, "TODO: tud_hid_set_report_cb");

                // tud_hid_set_report_cb(
                //     report_id, (hid_report_type_t)report_type, report_buf, report_len);
            }
            break;

        case HID_REQ_CONTROL_SET_IDLE:
            if(stage == CONTROL_STAGE_SETUP) {
                p_hid->idle_rate = tu_u16_high(request->wValue);

                tud_control_status(rhport, request);
            }
            break;

        case HID_REQ_CONTROL_GET_IDLE:
            if(stage == CONTROL_STAGE_SETUP) {
                // TODO idle rate of report
                tud_control_xfer(rhport, request, &p_hid->idle_rate, 1);
            }
            break;

        case HID_REQ_CONTROL_GET_PROTOCOL:
            if(stage == CONTROL_STAGE_SETUP) {
                tud_control_xfer(rhport, request, &p_hid->protocol_mode, 1);
            }
            break;

        case HID_REQ_CONTROL_SET_PROTOCOL:
            if(stage == CONTROL_STAGE_SETUP) {
                tud_control_status(rhport, request);
            } else if(stage == CONTROL_STAGE_ACK) {
                p_hid->protocol_mode = (uint8_t)request->wValue;
            }
            break;

        default:
            return false; // stall unsupported request
        }
    } else {
        return false; // stall unsupported request
    }

    return true;
}

bool usbd_hid_xfer_cb(
    uint8_t rhport,
    uint8_t ep_addr,
    xfer_result_t result,
    uint32_t xferred_bytes) {
    UNUSED(result);
    UNUSED(xferred_bytes);

    hidd_interface_t* p_hid = &_hidd_itf;

    // Sent report successfully
    if(ep_addr == p_hid->ep_in) {
        // TODO: report semaphore
    }
    // Received report
    else if(ep_addr == p_hid->ep_out) {
        FURI_LOG_W(TAG, "TODO: tud_hid_set_report_cb");
        // tud_hid_set_report_cb(
        //     0, HID_REPORT_TYPE_INVALID, p_hid->epout_buf, (uint16_t)xferred_bytes);
        TU_ASSERT(
            usbd_edpt_xfer(rhport, p_hid->ep_out, p_hid->epout_buf, sizeof(p_hid->epout_buf)));
    }

    return true;
}

bool furi_hal_hid_is_ready(void) {
    uint8_t const rhport = 0;
    uint8_t const ep_in = _hidd_itf.ep_in;
    return tud_ready() && (ep_in != 0) && !usbd_edpt_busy(rhport, ep_in);
}

bool furi_hal_hid_report(uint8_t report_id, void const* report, uint16_t len) {
    uint8_t const rhport = 0;
    hidd_interface_t* p_hid = &_hidd_itf;

    // claim endpoint
    TU_VERIFY(usbd_edpt_claim(rhport, p_hid->ep_in));

    // prepare data
    if(report_id) {
        p_hid->epin_buf[0] = report_id;
        TU_VERIFY(0 == tu_memcpy_s(p_hid->epin_buf + 1, CFG_TUD_HID_EP_BUFSIZE - 1, report, len));
        len++;
    } else {
        TU_VERIFY(0 == tu_memcpy_s(p_hid->epin_buf, CFG_TUD_HID_EP_BUFSIZE, report, len));
    }

    return usbd_edpt_xfer(rhport, p_hid->ep_in, p_hid->epin_buf, len);
}

bool furi_hal_hid_keyboard_report(uint8_t report_id, uint8_t modifier, uint8_t keycode[6]) {
    hid_keyboard_report_t report;

    report.modifier = modifier;
    report.reserved = 0;

    if(keycode) {
        memcpy(report.keycode, keycode, sizeof(report.keycode));
    } else {
        tu_memclr(report.keycode, 6);
    }

    return tud_hid_report(report_id, &report, sizeof(report));
}

bool furi_hal_hid_mouse_report(
    uint8_t report_id,
    uint8_t buttons,
    int8_t x,
    int8_t y,
    int8_t vertical,
    int8_t horizontal) {
    hid_mouse_report_t report = {
        .buttons = buttons, .x = x, .y = y, .wheel = vertical, .pan = horizontal};

    return tud_hid_report(report_id, &report, sizeof(report));
}
