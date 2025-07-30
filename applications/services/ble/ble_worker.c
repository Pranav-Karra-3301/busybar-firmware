#include <furi.h>

#include <sl_status.h>
#include <sl_wifi.h>
#include <sl_wifi_callback_framework.h>

#include "ble_config.h"
#include "wifi_config.h"

#include "rsi_ble_apis.h"
#include "rsi_ble_common_config.h"
#include "rsi_bt_common_apis.h"

#include "ble_common.h"
#include "ble_backend_util.h"

#define TAG "BleWorker"

#define BLE_WORKER_LOCAL_NAME         "Busybar"
#define BLE_WORKER_LOCAL_DEV_ADDR_LEN 18 // Length of the local device address
#define BLE_WORKER_MAX_MTU_SIZE       200

#define UUID_SIZE 16

#define BLE_WORKER_BT_HCI_COMMAND_DISALLOWED 0x4E0C
//! application events list
typedef enum {
    BLEWorkerEvtExit = (1 << 0),
    BLEWorkerEvtAdvReport = (1 << 1),
    BLEWorkerEvtConnected = (1 << 2),
    BLEWorkerEvtDisconnected = (1 << 3),
    BLEWorkerEvtPhyUpdateComplete = (1 << 4),
    BLEWorkerEvtConnUpdate = (1 << 5),
    BLEWorkerEvtDataLengthChange = (1 << 6),

    BLEWorkerEvtReceveRemoteFeatures = (1 << 7),
    BLEWorkerEvtMoreDataReq = (1 << 8),

    BLEWorkerEvtWrite = (1 << 9),
    BLEWorkerEvtDataTransmit = (1 << 10),
    BLEWorkerEvtMtu = (1 << 11),
} BLEWorkerEvt;

#define BLE_USART_ECHO_ALL_EVENTS                                                                \
    (BLEWorkerEvtExit | BLEWorkerEvtAdvReport | BLEWorkerEvtConnected |                          \
     BLEWorkerEvtDisconnected | BLEWorkerEvtPhyUpdateComplete | BLEWorkerEvtConnUpdate |         \
     BLEWorkerEvtDataLengthChange | BLEWorkerEvtReceveRemoteFeatures | BLEWorkerEvtMoreDataReq | \
     BLEWorkerEvtWrite | BLEWorkerEvtDataTransmit | BLEWorkerEvtMtu)

typedef struct {
    FuriThread* thread;

    uint8_t device_found;
    uint8_t conn_params_updated;
    uint8_t remote_name[31];
    uint8_t remote_addr_type;
    uint8_t remote_dev_str_addr[18];
    uint8_t remote_dev_bd_addr[6];

    uint8_t str_remote_address[18];
    uint8_t remote_dev_address[6];

    rsi_ble_event_phy_update_t app_phy_update_complete;
    rsi_ble_event_data_length_update_t data_length_update;
    rsi_ble_event_conn_update_t event_conn_update_complete;

    rsi_ble_event_remote_features_t remote_dev_feature;

    rsi_ble_event_write_t app_ble_write_event;
    rsi_ble_event_mtu_t app_ble_mtu_event;

    bool exit;

    ///TODO: replace this with LIST
    // BleCharacteristicObject* characteristics[10];
    BleServiceObject* services[3];
} BleWorker;

//==========================================================
// Advertise packet config
#define BLE_ADVERTISE_PACKET_MAX_SIZE (32)

typedef struct FURI_PACKED {
    uint8_t length;
    uint8_t type;
} BleAdvertiseHeader;

typedef struct FURI_PACKED {
    BleAdvertiseHeader header;
    uint8_t data;
} BleAdvertiseByteData;

typedef struct FURI_PACKED {
    BleAdvertiseHeader header;
    uint16_t data;
} BleAdvertiseWordData;

typedef struct FURI_PACKED {
    BleAdvertiseHeader header;
    char data[sizeof(BLE_WORKER_LOCAL_NAME)];
} BleAdvertiseLocalName;

typedef struct FURI_PACKED {
    BleAdvertiseByteData flags;
    BleAdvertiseWordData appearance;
    BleAdvertiseLocalName local_name;
    BleAdvertiseWordData manufacturer;
} BleAdvertiseData;

static const BleAdvertiseData advertise_data = {
    .flags =
        {
            .header = {.length = 2, .type = 1},
            .data = 6,
        },
    .appearance =
        {
            .header = {.type = 0x19, .length = 3},
            .data = 0x0880, //0x00C0,
        },
    .local_name =
        {
            .header = {.type = 0x9, .length = sizeof(BLE_WORKER_LOCAL_NAME) + 1},
            .data = BLE_WORKER_LOCAL_NAME,
        },
    .manufacturer =
        {
            .header = {.type = 0xFF, .length = 3},
            .data = 0x0E29,
        },
};

static_assert(sizeof(advertise_data) <= BLE_ADVERTISE_PACKET_MAX_SIZE);
//==========================================================

static const BleCharacteristicDescriptor battery_service_characteristics[] = {
    {
        .intercom_index = 0,
        .name = "Battery Level",
        .uuid = {.Char_UUID_16 = 0x2A19},
        .uuid_size = 2,
        .data_size = sizeof(uint8_t),
        .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
    },
    {
        .intercom_index = 1,
        .name = "Battery Status",
        .uuid = {.Char_UUID_16 = 0x2BED},
        .uuid_size = 2,
        .data_size = sizeof(BatteryStatusInfo),
        .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
    },
};

//==========================================================
#define UART_SERVICE_UUID \
    {0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}
#define UART_RX_CHAR_UUID \
    {0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}
#define UART_TX_CHAR_UUID \
    {0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}

static const BleCharacteristicDescriptor nordic_uart_service_characteristics[] = {
    {
        .intercom_index = 0,
        .name = "Uart Rx",
        .uuid = {.Char_UUID_128 = UART_RX_CHAR_UUID},
        .uuid_size = 16,
        .data_size = sizeof(2),
        .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_WRITE,
    },
    {
        .intercom_index = 1,
        .name = "Uart Tx",
        .uuid = {.Char_UUID_128 = UART_TX_CHAR_UUID},
        .uuid_size = 16,
        .data_size = sizeof(2),
        .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
    },
};
//==========================================================
const BleCharacteristicDescriptor device_info_service_characteristics[] = {
    {
        .intercom_index = BleSrvDeviceInfoCharacterIndexSerialNumber,
        .name = "Serial Number",
        .uuid = {.Char_UUID_16 = 0x2A25},
        .uuid_size = 2,
        .char_properties = RSI_BLE_ATT_PROPERTY_READ,
    },
    {
        .intercom_index = BleSrvDeviceInfoCharacterIndexHardwareRevision,
        .name = "Hardware Revision",
        .uuid = {.Char_UUID_16 = 0x2A27},
        .uuid_size = 2,
        .char_properties = RSI_BLE_ATT_PROPERTY_READ,
    },
    {
        .intercom_index = BleSrvDeviceInfoCharacterIndexSoftwareRevision,
        .name = "Software Revision",
        .uuid = {.Char_UUID_16 = 0x2A26},
        .uuid_size = 2,
        .char_properties = RSI_BLE_ATT_PROPERTY_READ,
    },
};
//==========================================================
static const BleServiceDescriptor service_config[] = {
    [BleIntercomServiceIndexDeviceInfo] =
        {
            .name = "Device Information",
            .uuid = {.Char_UUID_16 = 0x180A},
            .uuid_size = 2,
            .index = BleIntercomServiceIndexDeviceInfo,
            .init_method = BleServiceInitMethodRemote,
            .char_count = COUNT_OF(device_info_service_characteristics),
            .char_descriptors = device_info_service_characteristics,
        },
    [BleIntercomServiceIndexBattery] =
        {
            .name = "Battery Service",
            .uuid = {.Char_UUID_16 = 0x180F},
            .uuid_size = 2,
            .index = BleIntercomServiceIndexBattery,
            .init_method = BleServiceInitMethodLocal,
            .char_count = COUNT_OF(battery_service_characteristics),
            .char_descriptors = battery_service_characteristics,
        },
    [BleIntercomServiceIndexUart] =
        {
            .name = "Nordic UART",
            .uuid = {.Char_UUID_128 = UART_SERVICE_UUID},
            .uuid_size = 16,
            .index = BleIntercomServiceIndexUart,
            .init_method = BleServiceInitMethodLocal,
            .char_count = COUNT_OF(nordic_uart_service_characteristics),
            .char_descriptors = nordic_uart_service_characteristics,
        },
};

//==========================================================
static BleWorker* ble_worker_instance;
/*==============================================*/
/**
 * @fn         ble_usart_echo_app_on_adv_report_event
 * @brief      invoked when advertise report event is received
 * @param[in]  adv_report, pointer to the received advertising report
 * @return     none.
 * @section description
 * This callback function updates the scanned remote devices list
 */
void ble_worker_on_adv_report_event(rsi_ble_event_adv_report_t* adv_report) {
    if(ble_worker_instance->device_found == 1) {
        return;
    }

    memset(&ble_worker_instance->remote_name, 0, sizeof(ble_worker_instance->remote_name));
    BT_LE_ADPacketExtract(
        ble_worker_instance->remote_name, adv_report->adv_data, adv_report->adv_data_len);

    ble_worker_instance->remote_addr_type = adv_report->dev_addr_type;
    rsi_6byte_dev_address_to_ascii(
        ble_worker_instance->remote_dev_str_addr, (uint8_t*)adv_report->dev_addr);
    memcpy((int8_t*)ble_worker_instance->remote_dev_bd_addr, (uint8_t*)adv_report->dev_addr, 6);

#if (CONNECT_OPTION == CONN_BY_NAME)
    if((ble_worker_instance->device_found == 0) &&
       ((strcmp((const char*)ble_worker_instance->remote_name, RSI_REMOTE_DEVICE_NAME)) == 0)) {
        ble_worker_instance->device_found = 1;

        furi_thread_flags_set(
            furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtAdvReport);
        return;
    }
#elif (CONNECT_OPTION == CONN_BY_ADDR)
    if((!strcmp(RSI_BLE_REMOTE_DEV_ADDR, (char*)ble_worker_instance->remote_dev_str_addr))) {
        ble_worker_instance->device_found = 1;
        furi_thread_flags_set(
            furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtAdvReport);
    }
#endif

    return;
}

/*==============================================*/
/**
 * @fn         ble_worker_on_connect_event
 * @brief      invoked when connection complete event is received
 * @param[out] resp_conn, connected remote device information
 * @return     none.
 * @section description
 * This callback function indicates the status of the connection
 */
static void ble_worker_on_connect_event(rsi_ble_event_conn_status_t* resp_conn) {
    memcpy(ble_worker_instance->remote_dev_address, resp_conn->dev_addr, 6);
    rsi_6byte_dev_address_to_ascii(ble_worker_instance->str_remote_address, resp_conn->dev_addr);
    furi_thread_flags_set(furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtConnected);
}

/**
 * @fn         ble_worker_on_disconnect_event
 * @brief      invoked when disconnection event is received
 * @param[in]  resp_disconnect, disconnected remote device information
 * @param[in]  reason, reason for disconnection.
 * @return     none.
 * @section description
 * This callback function indicates disconnected device information and status
 */
static void
    ble_worker_on_disconnect_event(rsi_ble_event_disconnect_t* resp_disconnect, uint16_t reason) {
    UNUSED(
        reason); //This statement is added only to resolve compilation warning, value is unchanged
    memcpy(ble_worker_instance->remote_dev_address, resp_disconnect->dev_addr, 6);
    rsi_6byte_dev_address_to_ascii(
        ble_worker_instance->str_remote_address, resp_disconnect->dev_addr);

    furi_thread_flags_set(
        furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtDisconnected);
}

/**
 * @fn         ble_worker_phy_update_complete_event
 * @brief      invoked when disconnection event is received
 * @param[in]  resp_disconnect, disconnected remote device information
 * @param[in]  reason, reason for disconnection.
 * @return     none.
 * @section description
 * This Callback function indicates disconnected device information and status
 */
void ble_worker_phy_update_complete_event(
    rsi_ble_event_phy_update_t* rsi_ble_event_phy_update_complete) {
    memcpy(
        &ble_worker_instance->app_phy_update_complete,
        rsi_ble_event_phy_update_complete,
        sizeof(rsi_ble_event_phy_update_t));
    furi_thread_flags_set(
        furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtPhyUpdateComplete);
}

/**
 * @fn         ble_worker_data_length_change_event
 * @brief      invoked when data length is set
 * @section description
 * This Callback function indicates data length is set
 */
void ble_worker_data_length_change_event(
    rsi_ble_event_data_length_update_t* rsi_ble_data_length_update) {
    memcpy(
        &ble_worker_instance->data_length_update,
        rsi_ble_data_length_update,
        sizeof(rsi_ble_event_data_length_update_t));

    furi_thread_flags_set(
        furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtDataLengthChange);
}

/**
 * @fn         ble_worker_on_enhance_conn_status_event
 * @brief      invoked when enhanced connection complete event is received
 * @param[out] resp_conn, connected remote device information
 * @return     none.
 * @section description
 * This callback function indicates the status of the connection
 */
void ble_worker_on_enhance_conn_status_event(rsi_ble_event_enhance_conn_status_t* resp_enh_conn) {
    memcpy(ble_worker_instance->remote_dev_address, resp_enh_conn->dev_addr, 6);
    rsi_6byte_dev_address_to_ascii(
        ble_worker_instance->str_remote_address, resp_enh_conn->dev_addr);
    furi_thread_flags_set(furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtConnected);
}

void ble_worker_on_conn_update_complete_event(
    rsi_ble_event_conn_update_t* rsi_ble_event_conn_update_complete,
    uint16_t resp_status) {
    UNUSED(resp_status);
    memcpy(
        &ble_worker_instance->event_conn_update_complete,
        rsi_ble_event_conn_update_complete,
        sizeof(rsi_ble_event_conn_update_t));
    memcpy(
        ble_worker_instance->remote_dev_address, rsi_ble_event_conn_update_complete->dev_addr, 6);

    furi_thread_flags_set(furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtConnUpdate);
}
/*==============================================*/
/**
 * @fn         ble_worker_simple_peripheral_on_remote_features_event
 * @brief      invoked when LE remote features event is received.
 * @param[in] resp_conn, connected remote device information
 * @return     none.
 * @section description
 * This callback function indicates the status of the connection
 */
void ble_worker_simple_peripheral_on_remote_features_event(
    rsi_ble_event_remote_features_t* rsi_ble_event_remote_features) {
    memcpy(
        &ble_worker_instance->remote_dev_feature,
        rsi_ble_event_remote_features,
        sizeof(rsi_ble_event_remote_features_t));
    furi_thread_flags_set(
        furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtReceveRemoteFeatures);
}

static void ble_worker_more_data_req_event(rsi_ble_event_le_dev_buf_ind_t* rsi_ble_more_data_evt) {
    UNUSED(rsi_ble_more_data_evt);

    //! set conn specific event
    furi_thread_flags_set(
        furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtMoreDataReq);

    return;
}

/*==============================================*/
/**
 * @fn         ble_worker_on_gatt_write_event
 * @brief      its invoked when write/notify/indication events are received.
 * @param[in]  event_id, it indicates write/notification event id.
 * @param[in]  rsi_ble_write, write event parameters.
 * @return     none.
 * @section description
 * This callback function is invoked when write/notify/indication events are received
 */
static void
    ble_worker_on_gatt_write_event(uint16_t event_id, rsi_ble_event_write_t* rsi_ble_write) {
    UNUSED(event_id);

    memcpy(
        &ble_worker_instance->app_ble_write_event, rsi_ble_write, sizeof(rsi_ble_event_write_t));
    furi_thread_flags_set(furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtWrite);
}

/**
 * @fn         ble_worker_on_mtu_event
 * @brief      its invoked when write/notify/indication events are received.
 * @param[in]  event_id, it indicates write/notification event id.
 * @param[in]  rsi_ble_write, write event parameters.
 * @return     none.
 * @section description
 * This callback function is invoked when write/notify/indication events are received
 */
static void ble_worker_on_mtu_event(rsi_ble_event_mtu_t* rsi_ble_mtu) {
    memcpy(&ble_worker_instance->app_ble_mtu_event, rsi_ble_mtu, sizeof(rsi_ble_event_mtu_t));
    rsi_6byte_dev_address_to_ascii(
        ble_worker_instance->str_remote_address, ble_worker_instance->app_ble_mtu_event.dev_addr);

    furi_thread_flags_set(furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtMtu);
}

static void ble_hw_config() {
    sl_status_t status = 0;
    static uint8_t rsi_app_resp_get_dev_addr[RSI_DEV_ADDR_LEN] = {0};
    uint8_t local_dev_addr[BLE_WORKER_LOCAL_DEV_ADDR_LEN] = {0};

    status = sl_wifi_init(&wifi_config_client, NULL, sl_wifi_default_event_handler);
    if(status == SL_STATUS_ALREADY_INITIALIZED) {
        BLE_LOG_I("Already initialized");
    } else if(status != SL_STATUS_OK) {
        BLE_LOG_W("Wi-Fi Initialization Failed, Error Code : 0x0x%08lx", status);
        ///TODO: don't crash, return false instead
        furi_crash();
    }

    //! get the local device MAC address.
    status = rsi_bt_get_local_device_address(rsi_app_resp_get_dev_addr);
    if(status != RSI_SUCCESS) {
        BLE_LOG_W("Get local device address failed = 0x%08lx", status);
        ///TODO: don't crash, return false instead
        furi_crash();
    } else {
        rsi_6byte_dev_address_to_ascii(local_dev_addr, rsi_app_resp_get_dev_addr);
        BLE_LOG_I("Local device address %s", local_dev_addr);
    }

    // //! registering the GAP callback functions
    rsi_ble_gap_register_callbacks(
        ble_worker_on_adv_report_event,
        ble_worker_on_connect_event,
        ble_worker_on_disconnect_event,
        NULL,
        ble_worker_phy_update_complete_event,
        ble_worker_data_length_change_event,
        ble_worker_on_enhance_conn_status_event,
        NULL,
        ble_worker_on_conn_update_complete_event,
        NULL);

    rsi_ble_gap_extended_register_callbacks(
        ble_worker_simple_peripheral_on_remote_features_event, ble_worker_more_data_req_event);

    rsi_ble_gatt_register_callbacks(
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        ble_worker_on_gatt_write_event,
        NULL,
        NULL,
        NULL,
        ble_worker_on_mtu_event,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL);

    // ble_worker_add_simple_chat_serv(instance);
    // ble_add_battery_service(instance);

    // //! Set local name
    status = rsi_bt_set_local_name((uint8_t*)BLE_WORKER_LOCAL_NAME);
    if(status != RSI_SUCCESS) {
        BLE_LOG_W("Failed to set local name, error code : 0x%08lx", status);
        furi_crash();
    }

    BLE_LOG_I("Flags: %d", advertise_data.flags.data);
    BLE_LOG_I("Appearance: %04X", advertise_data.appearance.data);
    BLE_LOG_I("Manufacturer: %04X", advertise_data.manufacturer.data);
    BLE_LOG_I("Local Name: %s", advertise_data.local_name.data);

    //! set advertise data
    rsi_ble_set_advertise_data((uint8_t*)&advertise_data, sizeof(advertise_data));

    // ble_adjust_gap_service_data();
    BLE_LOG_I("Wireless Initialization Success");
}

static int32_t ble_worker_thread_callback(void* context) {
    BleWorker* instance = context;

    sl_status_t status = 0;

    FURI_LOG_D(TAG, "Worker Start");
    while(true) {
        uint32_t events =
            furi_thread_flags_wait(BLE_USART_ECHO_ALL_EVENTS, FuriFlagWaitAny, FuriWaitForever);

        if(events & BLEWorkerEvtConnected) {
            //! event invokes when connection was completed
            BLE_LOG_I("Connected, str_remote_address : %s", instance->str_remote_address);

            //! Setting MTU Exchange event
            status =
                rsi_ble_mtu_exchange_event(instance->remote_dev_address, BLE_WORKER_MAX_MTU_SIZE);
            if(status != RSI_SUCCESS) {
                BLE_LOG_W("MTU request cmd failed with error code = 0x%08lx", status);
            }

            if(!instance->conn_params_updated) {
                status = rsi_ble_conn_params_update(
                    instance->remote_dev_address,
                    CONN_INTERVAL_MIN,
                    CONN_INTERVAL_MAX,
                    CONN_LATENCY,
                    SUPERVISION_TIMEOUT);
                if(status != RSI_SUCCESS) {
                    BLE_LOG_W(
                        "Failed to update connection parameters, error code : 0x%08lx", status);
                    furi_crash();
                }
            }
        }

        if(events & BLEWorkerEvtDisconnected) {
            //! event invokes when disconnection was completed
            BLE_LOG_I("Disconnected, str_remote_address : %s", instance->str_remote_address);

            instance->device_found = 0;
            instance->conn_params_updated = 0;

            //! start advertising
            status = rsi_ble_start_advertising();
            if(status != RSI_SUCCESS) {
                BLE_LOG_W("Failed to start advertising, error code : 0x%08lx", status);
            } else {
                BLE_LOG_I("Start advertising...");
            }
        }

        if(events & BLEWorkerEvtReceveRemoteFeatures) {
            //! event invokes when remote features were received
            BLE_LOG_I(
                "Feature received is 0x%x",
                *(uint8_t*)instance->remote_dev_feature.remote_features);

            if(instance->remote_dev_feature.remote_features[0] & 0x20) {
                status = rsi_ble_set_data_len(instance->remote_dev_address, TX_LEN, TX_TIME);
                if(status != RSI_SUCCESS) {
                    BLE_LOG_W("Failed to set data length, error code : 0x%08lx", status);

                    furi_thread_flags_set(
                        furi_thread_get_id(ble_worker_instance->thread),
                        BLEWorkerEvtReceveRemoteFeatures);
                }

            } else if(instance->remote_dev_feature.remote_features[1] & 0x01) {
                status = rsi_ble_setphy(
                    (int8_t*)instance->remote_dev_address,
                    TX_PHY_RATE,
                    RX_PHY_RATE,
                    CODDED_PHY_RATE);
                if(status != RSI_SUCCESS) {
                    if(status != BLE_WORKER_BT_HCI_COMMAND_DISALLOWED) {
                        //retry the same command
                        furi_thread_flags_set(
                            furi_thread_get_id(ble_worker_instance->thread),
                            BLEWorkerEvtDataLengthChange);
                    } else {
                        BLE_LOG_W("Failed to set phy, error code : 0x%08lx", status);
                    }
                }
            }
        }

        if(events & BLEWorkerEvtDataLengthChange) {
            BLE_LOG_I(
                "Max_tx_octets: %d\r\nMax_tx_time: %d\r\nMax_rx_octets: %d\r\nMax_rx_time: %d",
                instance->data_length_update.MaxTxOctets,
                instance->data_length_update.MaxTxTime,
                instance->data_length_update.MaxRxOctets,
                instance->data_length_update.MaxRxTime);

            if(instance->remote_dev_feature.remote_features[1] & 0x01) {
                osDelay(500);
                status = rsi_ble_setphy(
                    (int8_t*)instance->remote_dev_address,
                    TX_PHY_RATE,
                    RX_PHY_RATE,
                    CODDED_PHY_RATE);
                if(status != RSI_SUCCESS) {
                    if(status != BLE_WORKER_BT_HCI_COMMAND_DISALLOWED) {
                        //retry the same command
                        furi_thread_flags_set(
                            furi_thread_get_id(ble_worker_instance->thread),
                            BLEWorkerEvtDataLengthChange);
                    } else {
                        BLE_LOG_W("Failed to set phy, error code : 0x%08lx", status);
                    }
                }
            }
        }

        if(events & BLEWorkerEvtPhyUpdateComplete) {
            //! phy update complete event
            BLE_LOG_I(
                "Tx Phy rate = 0x%x  and Rx Phy rate = 0x%x",
                instance->app_phy_update_complete.TxPhy,
                instance->app_phy_update_complete.RxPhy);
        }

        if(events & BLEWorkerEvtConnUpdate) {
            BLE_LOG_I(
                "Connection parameters update completed \r\n Connection interval = %d, Latency = %d, Supervision Timeout = %d",
                instance->event_conn_update_complete.conn_interval,
                instance->event_conn_update_complete.conn_latency,
                instance->event_conn_update_complete.timeout);
        }

        if(events & BLEWorkerEvtMtu) {
            //! event invokes when write/notification events received
            BLE_LOG_I(
                "MTU size received from remote device(%s) is %u",
                instance->str_remote_address,
                instance->app_ble_mtu_event.mtu_size);

            status = rsi_ble_set_wo_resp_notify_buf_info(
                instance->remote_dev_address, DLE_BUFFER_MODE, DLE_BUFFER_COUNT);
            if(status != RSI_SUCCESS) {
                BLE_LOG_W("Failed to set the buffer configuration mode, error: 0x%08lx", status);
            } else {
                BLE_LOG_I(
                    "Buffer configuration done for notify and set_att cmds buf mode = %d , max buff count =%d",
                    DLE_BUFFER_MODE,
                    DLE_BUFFER_COUNT);
            }
        }

        if(events & BLEWorkerEvtWrite) {
            BLE_LOG_I("Received packet type = %u", instance->app_ble_write_event.pkt_type);

            //TO DO: send ERR or write response
            // if((*(uint16_t*)instance->app_ble_write_event.handle) == instance->ble_att1_val_hndl) {
            //     // furi_string_printf(
            //     //     instance->msg, "Received data: %s", instance->app_ble_write_event.att_value);
            //     // cli_shell_notification_print(instance->shell, instance->msg);

            //     // rsi_ble_gatt_write_response(instance->remote_dev_address, 0);

            //     //Todo: if need send Error response
            //     //rsi_ble_att_error_response(instance->remote_dev_address,
            //     //    *(uint16_t *)instance->app_ble_write_event.handle,
            //     //    opcode,
            //     //    err);

            //     // Send notification to remote device
            //     // rsi_ble_notify_value(
            //     //     instance->remote_dev_address,
            //     //     instance->ble_att2_val_hndl,
            //     //     instance->app_ble_write_event.length,
            //     //     instance->app_ble_write_event.att_value);
            // } else {
            //     rsi_ble_gatt_write_response(instance->remote_dev_address, 0);
            // }
        }

        if(events & BLEWorkerEvtExit) {
            rsi_ble_stop_advertising();
            break;
        }
    }

    return 0;
}

/**
 * @fn         ble_usart_echo_app_prepare_128bit_uuid
 * @brief      this function is used to prepare the 128bit UUID
 * @param[in]  temp_service,received 128-bit service.
 * @param[out] temp_uuid,formed 128-bit service structure.
 * @return     none.
 * @section description
 * This function prepares the 128bit UUID
 */
static void
    ble_worker_prepare_128bit_uuid(const uint8_t temp_service[UUID_SIZE], uuid_t* temp_uuid) {
    temp_uuid->val.val128.data1 =
        ((temp_service[0] << 24) | (temp_service[1] << 16) | (temp_service[2] << 8) |
         (temp_service[3]));
    temp_uuid->val.val128.data2 = ((temp_service[5]) | (temp_service[4] << 8));
    temp_uuid->val.val128.data3 = ((temp_service[7]) | (temp_service[6] << 8));
    temp_uuid->val.val128.data4[0] = temp_service[9];
    temp_uuid->val.val128.data4[1] = temp_service[8];
    temp_uuid->val.val128.data4[2] = temp_service[11];
    temp_uuid->val.val128.data4[3] = temp_service[10];
    temp_uuid->val.val128.data4[4] = temp_service[15];
    temp_uuid->val.val128.data4[5] = temp_service[14];
    temp_uuid->val.val128.data4[6] = temp_service[13];
    temp_uuid->val.val128.data4[7] = temp_service[12];
}

/**
 * @fn         ble_usart_echo_app_add_char_serv_att
 * @brief      this function is used to add characteristic service attribute..
 * @param[in]  serv_handler, service handler.
 * @param[in]  handle, characteristic service attribute handle.
 * @param[in]  val_prop, characteristic value property.
 * @param[in]  att_val_handle, characteristic value handle
 * @param[in]  att_val_uuid, characteristic value uuid
 * @return     none.
 * @section description
 * This function is used at application to add characteristic attribute
 */
static uint16_t ble_worker_add_char_serv_att(
    void* serv_handler,
    uint16_t handle,
    uint8_t val_prop,
    uint16_t att_val_handle,
    uuid_t att_val_uuid) {
    rsi_ble_req_add_att_t new_att = {0};

    //! preparing the attribute service structure
    new_att.serv_handler = serv_handler;
    new_att.handle = handle;
    new_att.att_uuid.size = 2;
    new_att.att_uuid.val.val16 = RSI_BLE_CHAR_SERV_UUID;
    new_att.property = RSI_BLE_ATT_PROPERTY_READ;

    //! preparing the characteristic attribute value
    if(att_val_uuid.size == UUID_SIZE) {
        new_att.data_len = 4 + att_val_uuid.size;
        new_att.data[0] = val_prop;
        rsi_uint16_to_2bytes(&new_att.data[2], att_val_handle);
        memcpy(&new_att.data[4], &att_val_uuid.val.val128, sizeof(att_val_uuid.val.val128));
    } else {
        new_att.data_len = 6;
        rsi_uint16_to_2bytes(&new_att.data[2], att_val_handle);
        new_att.data[0] = val_prop;
        rsi_uint16_to_2bytes(&new_att.data[4], att_val_uuid.val.val16);
    }

    //! Add attribute to the service
    sl_status_t status = rsi_ble_add_attribute(&new_att);
    if(status != SL_STATUS_OK) {
        BLE_LOG_W("Status: %04lX", status);
    }

    return handle;
}

static uint16_t ble_worker_add_char_val_att(
    void* serv_handler,
    uint16_t handle,
    uuid_t att_type_uuid,
    uint8_t val_prop,
    uint8_t* data,
    uint8_t data_len,
    uint8_t auth_read) {
    rsi_ble_req_add_att_t new_att = {0};

    //! preparing the attributes
    new_att.serv_handler = serv_handler;
    new_att.handle = handle;
    new_att.config_bitmap = auth_read;
    memcpy(&new_att.att_uuid, &att_type_uuid, sizeof(uuid_t));
    new_att.property = val_prop;

    //! preparing the attribute value
    new_att.data_len = RSI_MIN(sizeof(new_att.data), data_len);

    if(data != NULL) memcpy(new_att.data, data, new_att.data_len);

    //! add attribute to the service
    sl_status_t status = rsi_ble_add_attribute(&new_att);

    if(status != SL_STATUS_OK) {
        BLE_LOG_W("Status: %04lX", status);
    }

    //! check the attribute property with notification/Indication
    if((val_prop & RSI_BLE_ATT_PROPERTY_NOTIFY) || (val_prop & RSI_BLE_ATT_PROPERTY_INDICATE)) {
        //! if notification/indication property supports then we need to add client characteristic service.
        handle += 1;
        //! preparing the client characteristic attribute & values
        memset(&new_att, 0, sizeof(rsi_ble_req_add_att_t));
        new_att.serv_handler = serv_handler;
        new_att.handle = handle;
        new_att.att_uuid.size = 2;
        new_att.att_uuid.val.val16 = RSI_BLE_CLIENT_CHAR_UUID;
        new_att.property = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_WRITE;
        new_att.data_len = 2;

        //! add attribute to the service
        int32_t ret = rsi_ble_add_attribute(&new_att);
        BLE_LOG_I("Add CCCD handle: %04X, Ret: %lX", handle, ret);
    }
    return handle;
}

static void ble_prepare_uuid(const Char_UUID_t* temp, const uint8_t size, uuid_t* uuid) {
    uuid->size = size;
    if(size == 2)
        uuid->val.val16 = temp->Char_UUID_16;
    else if(size == 16)
        ble_worker_prepare_128bit_uuid(temp->Char_UUID_128, uuid);
}

BleCharacteristicObject* ble_characteristic_alloc(
    const BleCharacteristicDescriptor* desc,
    void* service_handler,
    uint16_t handle,
    uint16_t* out_handle) {
    furi_assert(desc);
    furi_assert(service_handler);
    furi_assert(desc->data_size);
    furi_assert(out_handle);

    uuid_t uuid = {0};
    ble_prepare_uuid(&desc->uuid, desc->uuid_size, &uuid);

    BleCharacteristicObject* instance = malloc(sizeof(BleCharacteristicObject));
    instance->data = malloc(desc->data_size);
    instance->desc = desc;

    BLE_LOG_I("Add char %s att handle: %04X", desc->name, handle);
    handle = ble_worker_add_char_serv_att(
        service_handler, handle, desc->char_properties, handle + 1, uuid);

    BLE_LOG_I("Add char %s val att handle: %04X", desc->name, handle + 1);
    *out_handle = ble_worker_add_char_val_att(
        service_handler,
        handle + 1,
        uuid,
        desc->char_properties,
        instance->data,
        desc->data_size,
        0);

    instance->handle = *out_handle;

    return instance;
}

///TODO: Possibly return handle here for future use
static BleServiceObject* ble_worker_create_service(const BleServiceDescriptor* service_config) {
    rsi_ble_resp_add_serv_t new_serv_resp = {0};
    uuid_t uuid = {0};

    BleServiceObject* instance = malloc(sizeof(BleServiceObject));
    BLE_LOG_I("Create %s service", service_config->name);
    ble_prepare_uuid(&service_config->uuid, service_config->uuid_size, &uuid);
    rsi_ble_add_service(uuid, &new_serv_resp);

    instance->service_handler = new_serv_resp.serv_handler;
    instance->desc = service_config;
    instance->chars = malloc(sizeof(BleCharacteristicObject*) * service_config->char_count);

    uint16_t handle = new_serv_resp.start_handle;
    BLE_LOG_I("Start handle: %04X", handle);
    for(size_t i = 0; i < service_config->char_count; i++) {
        const BleCharacteristicDescriptor* config = &service_config->char_descriptors[i];

        BleCharacteristicObject* ble_char =
            ble_characteristic_alloc(config, instance->service_handler, handle + 1, &handle);

        instance->chars[config->intercom_index] = ble_char;
    }
    return instance;
}

void ble_worker_init() {
    ble_worker_instance = malloc(sizeof(BleWorker));

    ble_worker_instance->thread =
        furi_thread_alloc_ex("BleWorker", 2048, ble_worker_thread_callback, ble_worker_instance);

    //TODO: build services and add characteristics here

    ble_hw_config();

    for(size_t i = 0; i < COUNT_OF(service_config); i++) {
        if(service_config[i].init_method == BleServiceInitMethodRemote) {
            BLE_LOG_I("Skip creation of %s", service_config[i].name);
            continue;
        }

        BleServiceObject* service = ble_worker_create_service(&service_config[i]);
        ble_worker_instance->services[service->desc->index] = service;
    }

    uuid_t uuid = {0};
    uuid.size = 2;
    uuid.val.val16 = 0x2A01;
    uint16_t value_handle = 0;
    sl_status_t status;
    if(ble_find_characteristic_value_handle_by_uiid(&uuid, 0x001E, &value_handle)) {
        uint16_t data = 0x00C0;
        BLE_LOG_I("Handle found: %04X", value_handle);
        status = rsi_ble_set_local_att_value(value_handle, 2, (uint8_t*)&data);
        BLE_LOG_I("Status: %lX", status);
    }

    // value_handle =
    //     ble_worker_instance->characteristics[BleIntercomCharIndexDeviceInfoSerialNumber]->handle;
    // BLE_LOG_I("Handle serial: %04X", value_handle);
    // const char* test = "Att value adjusted after creation";
    // status = rsi_ble_set_local_att_value(value_handle, strlen(test), (uint8_t*)test);
    // BLE_LOG_I("Adjs Status: %lX", status);

    ble_print_service_hierarchy(0x001E);
}

///TODO: Optional, maybe not needed but will add for now
void ble_worker_deinit() {
    free(ble_worker_instance);
}

void ble_worker_init_service(BleIntercomFrameServiceConfig* config) {
    furi_assert(config);

    //const BleServiceDescriptor* service_descriptor = &service_config[config->service_index];
    BleServiceDescriptor* service_descriptor = malloc(sizeof(BleServiceDescriptor));

    memcpy(
        service_descriptor,
        &service_config[config->header.service_index],
        sizeof(BleServiceDescriptor));

    BLE_LOG_I("Init service: %s", service_descriptor->name);

    BleCharacteristicDescriptor* descriptors =
        malloc(config->char_count * sizeof(BleCharacteristicDescriptor));

    //check counts are equal

    for(size_t i = 0; i < config->char_count; i++) {
        memcpy(
            &descriptors[i],
            &service_descriptor->char_descriptors[i],
            sizeof(BleCharacteristicDescriptor));

        BLE_LOG_I(
            "data_size old: %d, new: %d",
            descriptors[i].data_size,
            config->chars_config[i].data_size);

        descriptors[i].data_size = config->chars_config[i].data_size;
    }
    service_descriptor->char_descriptors = descriptors;

    BLE_LOG_I("Service descriptor copied");

    BleServiceObject* service = ble_worker_create_service(service_descriptor);
    ble_worker_instance->services[service->desc->index] = service;

    free(descriptors);
    free(service_descriptor);

    // ble_print_service_hierarchy(0x001E);
}

void ble_worker_start() {
    ///TODO: this can be moved to thread
    //! start advertising
    sl_status_t status = rsi_ble_start_advertising();
    if(status != RSI_SUCCESS) {
        BLE_LOG_W("Failed to start advertising, error code : 0x%08lx", status);
    } else {
        BLE_LOG_I("Start advertising...");
    }

    furi_thread_start(ble_worker_instance->thread);
}

void ble_worker_stop() {
    BLE_LOG_I("Stopping BLE...");
    furi_thread_flags_set(furi_thread_get_id(ble_worker_instance->thread), BLEWorkerEvtExit);
    furi_thread_join(ble_worker_instance->thread);
    BLE_LOG_I("Stopped");
}

void ble_worker_set_value(
    uint16_t service_index,
    uint16_t char_index,
    uint16_t data_size,
    const uint8_t* data) {
    furi_assert(data);
    BLE_LOG_I("Set Value");

    BleServiceObject* service = ble_worker_instance->services[service_index];
    BleCharacteristicObject* char_obj = service->chars[char_index];

    rsi_ble_set_local_att_value(char_obj->handle, data_size, data);
}
