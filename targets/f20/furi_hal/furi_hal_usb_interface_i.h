#pragma once

#include <stdint.h>
#include "furi_hal_usb.h"
#include "furi_hal_usb_interface.h"

void* usbd_cdc_init(void* settings);

void usbd_cdc_deinit(void);

void usbd_cdc_reset(uint8_t rhport);

uint16_t usbd_cdc_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len);

bool usbd_cdc_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request);

bool usbd_cdc_xfer_cb(
    uint8_t rhport,
    uint8_t ep_addr,
    xfer_result_t result,
    uint32_t xferred_bytes);

uint8_t const* usbd_get_report_desc(void);

void* usbd_hid_init(void* settings);

void usbd_hid_deinit(void);

void usbd_hid_reset(uint8_t rhport);

uint16_t usbd_hid_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len);

bool usbd_hid_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request);

bool usbd_hid_xfer_cb(
    uint8_t rhport,
    uint8_t ep_addr,
    xfer_result_t result,
    uint32_t xferred_bytes);

void* usbd_eth_init(void* settings);

void usbd_eth_deinit(void);

void usbd_eth_reset(uint8_t rhport);

uint16_t usbd_eth_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len);

bool usbd_eth_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request);

bool usbd_eth_xfer_cb(
    uint8_t rhport,
    uint8_t ep_addr,
    xfer_result_t result,
    uint32_t xferred_bytes);

char* usbd_eth_get_mac_str(void);
