#include <furi_hal.h>
#include <tusb.h>
#include "furi_hal_usb_i.h"
#include "furi_hal_usb_interface_i.h"
#include "class/net/net_device.h"
#include "device/usbd.h"
#include "device/usbd_pvt.h"

#define TAG "USB ETH"

typedef struct {
    uint8_t itf_num; // Index number of Management Interface, +1 for Data Interface

    uint8_t ep_notif;
    uint8_t ep_in;
    uint8_t ep_out;

} EthdInterface;

CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN tu_static uint8_t received[USB_ETH_MTU];
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN tu_static uint8_t transmitted[USB_ETH_MTU];

struct ecm_notify_struct {
    tusb_control_request_t header;
    uint32_t downlink, uplink;
};

tu_static const struct ecm_notify_struct ecm_notify_nc = {
    .header =
        {
            .bmRequestType = 0xA1,
            .bRequest = 0 /* NETWORK_CONNECTION aka NetworkConnection */,
            .wValue = 1 /* Connected */,
            .wLength = 0,
        },
};

tu_static const struct ecm_notify_struct ecm_notify_csc = {
    .header =
        {
            .bmRequestType = 0xA1,
            .bRequest = 0x2A /* CONNECTION_SPEED_CHANGE aka ConnectionSpeedChange */,
            .wLength = 8,
        },
    .downlink = 9728000,
    .uplink = 9728000,
};

CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN tu_static union {
    struct ecm_notify_struct ecm_buf;
} notify;

CFG_TUD_MEM_SECTION tu_static EthdInterface _ethd_itf;

tu_static bool can_xmit;

static void usbd_eth_report(uint8_t rhport, bool nc) {
    notify.ecm_buf = (nc) ? ecm_notify_nc : ecm_notify_csc;
    notify.ecm_buf.header.wIndex = _ethd_itf.itf_num;

    if(usbd_edpt_busy(rhport, _ethd_itf.ep_notif)) {
        return;
    }

    usbd_edpt_xfer(
        rhport,
        _ethd_itf.ep_notif,
        (uint8_t*)&notify.ecm_buf,
        (nc) ? sizeof(notify.ecm_buf.header) : sizeof(notify.ecm_buf));
}

static void usbd_eth_handle_incoming_packet(uint32_t len) {
    uint8_t* pnt = received;
    uint32_t size = 0;

    size = len;

    if(!tud_network_recv_cb(pnt, (uint16_t)size)) {
        /* if a buffer was never handled by user code, we must renew on the user's behalf */
        furi_hal_usb_eth_recv_renew();
    }
}

static void usbd_eth_do_in_xfer(uint8_t* buf, uint16_t len) {
    can_xmit = false;
    usbd_edpt_xfer(0, _ethd_itf.ep_in, buf, len);
}

void* usbd_eth_init(void* settings) {
    UNUSED(settings);
    tu_memclr(&_ethd_itf, sizeof(_ethd_itf));
    return NULL;
}

void usbd_eth_deinit(void) {
}

void usbd_eth_reset(uint8_t rhport) {
    (void)rhport;
    usbd_eth_init(NULL);
}

uint16_t usbd_eth_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len) {
    // confirm interface hasn't already been allocated
    TU_ASSERT(0 == _ethd_itf.ep_notif, 0);

    _ethd_itf.itf_num = itf_desc->bInterfaceNumber;

    uint16_t drv_len = sizeof(tusb_desc_interface_t);
    uint8_t const* p_desc = tu_desc_next(itf_desc);

    // Communication Functional Descriptors
    while(TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc) && drv_len <= max_len) {
        drv_len += tu_desc_len(p_desc);
        p_desc = tu_desc_next(p_desc);
    }

    // notification endpoint (if any)
    if(TUSB_DESC_ENDPOINT == tu_desc_type(p_desc)) {
        TU_ASSERT(usbd_edpt_open(rhport, (tusb_desc_endpoint_t const*)p_desc), 0);

        _ethd_itf.ep_notif = ((tusb_desc_endpoint_t const*)p_desc)->bEndpointAddress;

        drv_len += tu_desc_len(p_desc);
        p_desc = tu_desc_next(p_desc);
    }

    TU_ASSERT(TUSB_DESC_INTERFACE == tu_desc_type(p_desc), 0);

    do {
        tusb_desc_interface_t const* data_itf_desc = (tusb_desc_interface_t const*)p_desc;
        TU_ASSERT(TUSB_CLASS_CDC_DATA == data_itf_desc->bInterfaceClass, 0);

        drv_len += tu_desc_len(p_desc);
        p_desc = tu_desc_next(p_desc);
    } while((TUSB_DESC_INTERFACE == tu_desc_type(p_desc)) && (drv_len <= max_len));

    // Pair of endpoints
    TU_ASSERT(TUSB_DESC_ENDPOINT == tu_desc_type(p_desc), 0);

    if(_ethd_itf.ep_in == 0 && _ethd_itf.ep_out == 0) {
        TU_ASSERT(usbd_open_edpt_pair(
            rhport, p_desc, 2, TUSB_XFER_BULK, &_ethd_itf.ep_out, &_ethd_itf.ep_in));

        tud_network_init_cb();
        can_xmit = true; // we are ready to transmit a packet
        furi_hal_usb_eth_recv_renew(); // prepare for incoming packets
    }

    drv_len += 2 * sizeof(tusb_desc_endpoint_t);

    return drv_len;
}

bool usbd_eth_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    if(stage == CONTROL_STAGE_SETUP) {
        switch(request->bmRequestType_bit.type) {
        case TUSB_REQ_TYPE_CLASS:
            /* the only required CDC-ECM Management Element Request is SetEthernetPacketFilter */
            if(0x43 /* SET_ETHERNET_PACKET_FILTER */ == request->bRequest) {
                tud_control_xfer(rhport, request, NULL, 0);
                usbd_eth_report(rhport, true);
            }

            break;

        // unsupported request
        default:
            return false;
        }
    }

    return true;
}

bool usbd_eth_xfer_cb(
    uint8_t rhport,
    uint8_t ep_addr,
    xfer_result_t result,
    uint32_t xferred_bytes) {
    (void)rhport;
    (void)result;

    /* new packet received */
    if(ep_addr == _ethd_itf.ep_out) {
        usbd_eth_handle_incoming_packet(xferred_bytes);
    }

    /* data transmission finished */
    if(ep_addr == _ethd_itf.ep_in) {
        /* TinyUSB requires the class driver to implement ZLP (since ZLP usage is class-specific) */

        if(xferred_bytes && (0 == (xferred_bytes % CFG_TUD_NET_ENDPOINT_SIZE))) {
            usbd_eth_do_in_xfer(NULL, 0); /* a ZLP is needed */
        } else {
            /* we're finally finished */
            can_xmit = true;
        }
    }

    if(ep_addr == _ethd_itf.ep_notif) {
        if(sizeof(notify.ecm_buf.header) == xferred_bytes) {
            usbd_eth_report(rhport, false);
        }
    }

    return true;
}

char* usbd_eth_get_mac_str(void) {
    return "0CFA22012345";
}

void furi_hal_usb_eth_recv_renew(void) {
    usbd_edpt_xfer(0, _ethd_itf.ep_out, received, sizeof(received));
}

bool furi_hal_usb_eth_can_xmit(uint16_t size) {
    (void)size;

    return can_xmit;
}

void furi_hal_usb_eth_xmit(void* ref, uint16_t arg) {
    uint8_t* data;
    uint16_t len;

    if(!can_xmit) return;

    len = 0;
    data = transmitted + len;

    len += tud_network_xmit_cb(data, ref, arg);

    usbd_eth_do_in_xfer(transmitted, len);
}
