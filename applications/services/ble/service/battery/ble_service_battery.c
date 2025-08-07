#include "ble_service_battery_i.h"

#define TAG "BleBattery"

// void ble_service_prepare_char_frame(
//     BleServiceObject* instance,
//     BleIntercomFrameType frame_type,
//     uint8_t command_event,
//     uint8_t ch_index,
//     size_t data_size,
//     void* data) {
//     BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->output;
//     frame->header.frame_type = frame_type;
//     frame->header.command = command_event;
//     frame->header.service_index = instance->desc->index;
//     frame->header.data_size = sizeof(BleCharacteristicInitHeader) + data_size;

//     BleCharacteristicData* ch_data = (BleCharacteristicData*)frame->data;
//     ch_data->header.index = ch_index;
//     ch_data->header.data_size = data_size;
//     memcpy(ch_data->data, data, data_size);
//     // ///TODO: need more checks if there_is_enough memory in buffer
//     // if(data_size && data) memcpy(frame->data, data, data_size);
// }

// static void ble_power_pubsub_message_callback(const void* message, void* context) {
//     BleServiceObject* instance = context;
//     const PowerEvent* event = message;

//     if(ble_service_lock(instance)) {
//         BleBatteryServiceContext* data_ctx = instance->data_context;
//         BatteryStatusInfo* battery_status = &data_ctx->battery_status;

//         if(event->type == PowerEventBatteryPresent) {
//             battery_status->state.fields.battery_present = 1;
//         } else if(event->type == PowerEventBatteryPresent) {
//             battery_status->state.fields.battery_present = 0;
//         } else if(event->type == PowerEventChargeAmountUpdate) {
//             PowerInfo info = {0};
//             power_get_info(data_ctx->power, &info);

//             data_ctx->battery_level = info.charge;

//             battery_status->state.fields.wired_source_present = info.charge_enabled;
//             battery_status->state.fields.wireless_source_present = 0;
//             battery_status->state.fields.battery_charge_state = info.is_charging ? 1 : 2;
//             battery_status->state.fields.battery_charge_level = 1;
//             battery_status->state.fields.charging_type = 2;
//             battery_status->state.fields.charging_fault_reason = 0;
//         }

//         BLE_LOG_D("update_char");
//         BleCharacteristicObject* ch = instance->chars[BleSrvBatteryCharacterIndexBatteryLevel];

//         ble_service_prepare_char_frame(
//             instance,
//             BleIntercomFrameTypeNotification,
//             BleCommandServiceNotify,
//             BleSrvBatteryCharacterIndexBatteryLevel,
//             ch->data_size,
//             ch->data);
//         ble_service_send_intercom_frame(instance);

//         ch = instance->chars[BleSrvBatteryCharacterIndexBatteryStatus];
//         ble_service_prepare_char_frame(
//             instance,
//             BleIntercomFrameTypeNotification,
//             BleCommandServiceNotify,
//             BleSrvBatteryCharacterIndexBatteryStatus,
//             ch->data_size,
//             ch->data);
//         ble_service_send_intercom_frame(instance);

//         ble_service_unlock(instance);
//     }
// }

// static void power_events_callback(const void* message, void* context) {
//     furi_assert(message);
//     furi_assert(context);

//     PowerEvent* event = (PowerEvent*)message;
//     switch(event->type) {
//     case PowerEventChargingStateUpdate:
//         update_event = StatusBarUpdateEventPowerChargingState;
//         break;

//     case PowerEventChargeAmountUpdate:
//         update_event = StatusBarUpdateEventPowerChargeAmount;
//         break;

//     case PowerEventUsbConnectionStateUpdate:
//         update_event = StatusBarUpdateEventUsbConnectionState;
//         break;

//     default:
//         return;
//     }

//     furi_event_loop_set_custom_event(instance->event_loop, update_event);
// }

// static bool ble_service_battery_init(void* object) {
//     furi_assert(object);

//     BLE_LOG_W("battery_init");

//     BleServiceObject* instance = object;

//     BleBatteryServiceContext* context = malloc(sizeof(BleBatteryServiceContext));
//     instance->data_context = context;

//     context->power = furi_record_open(RECORD_POWER);
//     PowerInfo info = {0};
//     power_get_info(context->power, &info);

//     context->battery_level = info.charge;
//     instance->chars[BleSrvBatteryCharacterIndexBatteryLevel]->data = &context->battery_level;
//     instance->chars[BleSrvBatteryCharacterIndexBatteryLevel]->data_size = sizeof(uint8_t);

//     BatteryStatusInfo* battery_status = &context->battery_status;

//     battery_status->flags = 0;
//     battery_status->state.fields.battery_present = 1;
//     battery_status->state.fields.wired_source_present = info.is_charging;
//     battery_status->state.fields.wireless_source_present = 0;
//     battery_status->state.fields.battery_charge_state = info.is_charging ? 1 : 2;
//     battery_status->state.fields.battery_charge_level = 1;
//     battery_status->state.fields.charging_type = 2;
//     battery_status->state.fields.charging_fault_reason = 0;

//     instance->chars[BleSrvBatteryCharacterIndexBatteryStatus]->data = battery_status;
//     instance->chars[BleSrvBatteryCharacterIndexBatteryStatus]->data_size =
//         sizeof(BatteryStatusInfo);

//     furi_pubsub_subscribe(
//         power_get_pubsub(context->power), ble_power_pubsub_message_callback, instance);

//     // furi_record_close(RECORD_POWER);
//     // context->
//     // context->timer = furi_timer_alloc(ble_bat_test, FuriTimerTypePeriodic, instance);
//     // furi_timer_start(context->timer, 30000);
//     return true;
// }

static const uint8_t* battery_character_get_data(void* obj) {
    furi_assert(obj);
    BleCharacteristicObject* instance = obj;
    return instance->data;
}

//==========================================================
static const BleCharacteristicDescriptor battery_service_characteristics[] = {
    {
        .intercom_index = BleSrvBatteryCharacterIndexBatteryLevel,
        .name = "Battery Level",
        .data_size = sizeof(uint8_t),
        .get_data = battery_character_get_data,
#if defined(SI917)
        .uuid = {.Char_UUID_16 = 0x2A19},
        .uuid_size = 2,
        .char_properties = BLE_ATT_PROPERTY_READ | BLE_ATT_PROPERTY_NOTIFY,
#endif
    },
    {
        .intercom_index = BleSrvBatteryCharacterIndexBatteryStatus,
        .name = "Battery Status",
        .data_size = sizeof(BatteryStatusInfo),
        .get_data = battery_character_get_data,
#if defined(SI917)
        .uuid = {.Char_UUID_16 = 0x2BED},
        .uuid_size = 2,
        .char_properties = BLE_ATT_PROPERTY_READ | BLE_ATT_PROPERTY_NOTIFY,
#endif
    },
};

const BleServiceDescriptor ble_service_config_battery = {
    .name = "Battery Service",
#if defined(SI917)
    .uuid = {.Char_UUID_16 = 0x180F},
    .uuid_size = 2,
#endif
    .init = ble_service_battery_init,
    .index = BleIntercomServiceIndexBattery,
    .init_method = BleServiceInitMethodLocal,
    .char_count = COUNT_OF(battery_service_characteristics),
    .char_descriptors = battery_service_characteristics,
};
