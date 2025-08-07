#include "ble_service_battery_i.h"

#include "../../../power/power_service/power.h"

#define TAG "BleBatteryU5"

typedef struct {
    PowerEventType event_type;
    FuriThread* thr;
    BatteryStatusInfo battery_status;
    uint8_t battery_level;
} BleBatteryServiceContext;

void ble_service_prepare_char_frame(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    uint8_t command_event,
    uint8_t ch_index,
    size_t data_size,
    void* data) {
    BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->output;
    frame->header.frame_type = frame_type;
    frame->header.command = command_event;
    frame->header.service_index = instance->desc->index;
    frame->header.data_size = sizeof(BleCharacteristicInitHeader) + data_size;

    BleCharacteristicData* ch_data = (BleCharacteristicData*)frame->data;
    ch_data->header.index = ch_index;
    ch_data->header.data_size = data_size;
    memcpy(ch_data->data, data, data_size);
}

static void ble_power_pubsub_message_callback(const void* message, void* context) {
    BLE_LOG_W("Battery event!");

    BleServiceObject* instance = context;
    const PowerEvent* event = message;

    if(ble_service_lock(instance)) {
        BleBatteryServiceContext* data_ctx = instance->data_context;
        data_ctx->event_type = event->type;
        furi_thread_flags_set(furi_thread_get_id(data_ctx->thr), 1);
        ble_service_unlock(instance);
    }
    BLE_LOG_W("Battery event exit!");
}

static int32_t bat_test_thread(void* context) {
    BleServiceObject* instance = context;

    while(1) {
        furi_thread_flags_wait(1, FuriFlagWaitAny, FuriWaitForever);
        BLE_LOG_D("Bat_thr");
        if(ble_service_lock(instance)) {
            BleBatteryServiceContext* data_ctx = instance->data_context;
            BatteryStatusInfo* battery_status = &data_ctx->battery_status;

            if(data_ctx->event_type == PowerEventBatteryPresent) {
                battery_status->state.fields.battery_present = 1;
            } else if(data_ctx->event_type == PowerEventBatteryPresent) {
                battery_status->state.fields.battery_present = 0;
            } else if(data_ctx->event_type == PowerEventChargeAmountUpdate) {
                PowerInfo info = {0};
                Power* power = furi_record_open(RECORD_POWER);
                power_get_info(power, &info);
                furi_record_close(RECORD_POWER);

                data_ctx->battery_level = info.charge;

                battery_status->state.fields.wired_source_present = info.charge_enabled;
                battery_status->state.fields.wireless_source_present = 0;
                battery_status->state.fields.battery_charge_state = info.is_charging ? 1 : 2;
                battery_status->state.fields.battery_charge_level = 1;
                battery_status->state.fields.charging_type = 2;
                battery_status->state.fields.charging_fault_reason = 0;
            }

            BLE_LOG_D("update_char");
            BleCharacteristicObject* ch = instance->chars[BleSrvBatteryCharacterIndexBatteryLevel];

            ble_service_prepare_char_frame(
                instance,
                BleIntercomFrameTypeNotification,
                BleCommandServiceNotify,
                BleSrvBatteryCharacterIndexBatteryLevel,
                ch->data_size,
                ch->data);
            ble_service_send_intercom_frame(instance);

            ch = instance->chars[BleSrvBatteryCharacterIndexBatteryStatus];
            ble_service_prepare_char_frame(
                instance,
                BleIntercomFrameTypeNotification,
                BleCommandServiceNotify,
                BleSrvBatteryCharacterIndexBatteryStatus,
                ch->data_size,
                ch->data);
            ble_service_send_intercom_frame(instance);

            ble_service_unlock(instance);
        }
    }
    return 0;
}

bool ble_service_battery_init(void* object) {
    furi_assert(object);

    BLE_LOG_W("battery_init");

    BleServiceObject* instance = object;

    BleBatteryServiceContext* context = malloc(sizeof(BleBatteryServiceContext));
    instance->data_context = context;

    Power* power = furi_record_open(RECORD_POWER);
    PowerInfo info = {0};
    power_get_info(power, &info);

    context->battery_level = info.charge;
    instance->chars[BleSrvBatteryCharacterIndexBatteryLevel]->data = &context->battery_level;
    instance->chars[BleSrvBatteryCharacterIndexBatteryLevel]->data_size = sizeof(uint8_t);

    BatteryStatusInfo* battery_status = &context->battery_status;

    battery_status->flags = 0;
    battery_status->state.fields.battery_present = 1;
    battery_status->state.fields.wired_source_present = info.is_charging;
    battery_status->state.fields.wireless_source_present = 0;
    battery_status->state.fields.battery_charge_state = info.is_charging ? 1 : 2;
    battery_status->state.fields.battery_charge_level = 1;
    battery_status->state.fields.charging_type = 2;
    battery_status->state.fields.charging_fault_reason = 0;

    instance->chars[BleSrvBatteryCharacterIndexBatteryStatus]->data = battery_status;
    instance->chars[BleSrvBatteryCharacterIndexBatteryStatus]->data_size =
        sizeof(BatteryStatusInfo);

    furi_pubsub_subscribe(power_get_pubsub(power), ble_power_pubsub_message_callback, instance);

    furi_record_close(RECORD_POWER);

    context->thr = furi_thread_alloc_ex("BatTest", 1024, bat_test_thread, instance);
    furi_thread_start(context->thr);

    return true;
}
