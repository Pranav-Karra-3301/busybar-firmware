#include <furi.h>
#include "input.h"

#include <furi_hal_serial.h>
#include <furi_hal_resources.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_qei.h>

#define TAG "Input"

#define GPIO_Read(input_pin) (furi_hal_gpio_read(input_pin.pin->gpio) ^ (input_pin.pin->inverted))

#define INPUT_DEBOUNCE_TIMER_TICKS 1 //ms
#define INPUT_QUEUE_SIZE           15

#define INPUT_DEBUG
#ifdef INPUT_DEBUG
#define INPUT_LOG(...) FURI_LOG_D(TAG, __VA_ARGS__)
#else
#define INPUT_LOG(...)
#endif

typedef struct {
    const InputPin* pin;
    uint16_t debounce_count;
    bool state;
} InputSrvKeyState;

typedef struct {
    FuriEventLoop* event_loop;
    FuriSemaphore* input_key_semaphore;
    FuriMessageQueue* input_queue;
    FuriEventLoopTimer* debounce_timer;
    InputSrvKeyState* key_state;
    // InputSrvKeySequence* key_sequence;
} InputSrv;

static void input_isr_key(void* context) {
    InputSrv* instance = context;
    furi_semaphore_release(instance->input_key_semaphore);
}

static bool input_key_semaphore_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    InputSrv* instance = context;
    furi_assert(object == instance->input_key_semaphore);

    furi_check(furi_semaphore_acquire(instance->input_key_semaphore, 0) == FuriStatusOk);

    if(!furi_event_loop_timer_is_running(instance->debounce_timer)) {
        furi_event_loop_timer_start(instance->debounce_timer, INPUT_DEBOUNCE_TIMER_TICKS);
    }
    return true;
}

static void input_send(InputSrv* instance, uint32_t num_pin, InputType input_type) {
    InputEvent event;

    event.key = instance->key_state[num_pin].pin->key;

    if((instance->key_state[num_pin].pin->key == InputSwitch)) {
        if(input_type == InputTypePress) {
            event.type = InputTypeSwitch;
            event.switch_position = instance->key_state[num_pin].pin->switch_position;
            // furi_pubsub_publish(instance->event_pubsub, RECORD_INPUT_EVENTS, &event);
            INPUT_LOG(
                "Switch %s %d, event %s",
                instance->key_state[num_pin].pin->name,
                instance->key_state[num_pin].pin->switch_position,
                input_type == InputTypePress ? "press" : "release");
        }
    } else {
        event.type = input_type;
        INPUT_LOG(
            "Key %s, event %s",
            instance->key_state[num_pin].pin->name,
            input_type == InputTypePress ? "press" : "release");
    }
    UNUSED(event);
    furi_check(furi_message_queue_put(instance->input_queue, &event, 0) == FuriStatusOk);
}

static void input_debounce_timer_callback(void* context) {
    furi_assert(context);
    InputSrv* instance = context;
    bool is_changing = false;
    for(size_t i = 0; i < input_pins_count; i++) {
        bool state = GPIO_Read(instance->key_state[i]);

        if(state) {
            if(instance->key_state[i].debounce_count < INPUT_DEBOUNCE_TICKS) {
                instance->key_state[i].debounce_count++;
                is_changing = true;
            }
        } else if(instance->key_state[i].debounce_count > 0) {
            instance->key_state[i].debounce_count--;
            is_changing = true;
        }

        if(!is_changing && instance->key_state[i].state != state) {
            instance->key_state[i].state = state;

            if(state) {
                input_send(instance, i, InputTypePress);
            } else {
                input_send(instance, i, InputTypeRelease);
            }
        }
    }

    if(!is_changing) {
        furi_event_loop_timer_stop(instance->debounce_timer);
    }
}

static void input_qei_callback(int16_t delta_pos, void* context) {
    InputSrv* instance = context;
    InputEvent event = {
        .key = InputEncoder,
        .type = InputTypeEncoderTurn,
        .click_count = delta_pos,
    };
    furi_check(furi_message_queue_put(instance->input_queue, &event, 0) == FuriStatusOk);
}

static bool input_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    InputSrv* instance = context;
    furi_assert(object == instance->input_queue);

    InputEvent event;
    furi_check(furi_message_queue_get(instance->input_queue, &event, 0) == FuriStatusOk);

    // if(event.type == InputTypeEncoderTurn) {
    //     input_key_sequence_run(NULL, event.type, event.click_count);
    // } else {
    //     input_key_sequence_run(event.key, event.type, 0);
    // }

    INPUT_LOG("Key %d, event %s", event.key, event.type == InputTypePress ? "press" : "release");

    return true;
}

int32_t input_srv(void* p) {
    UNUSED(p);
    //__BKPT();
    INPUT_LOG("Starting");
    InputSrv* instance = malloc(sizeof(InputSrv));
    instance->input_key_semaphore = furi_semaphore_alloc(1, 0);
    instance->input_queue = furi_message_queue_alloc(sizeof(InputEvent), INPUT_QUEUE_SIZE);
    instance->event_loop = furi_event_loop_alloc();
    instance->debounce_timer = furi_event_loop_timer_alloc(
        instance->event_loop,
        input_debounce_timer_callback,
        FuriEventLoopTimerTypePeriodic,
        instance);

    instance->key_state = malloc(sizeof(InputSrvKeyState) * input_pins_count);
    for(size_t i = 0; i < input_pins_count; i++) {
        furi_hal_gpio_add_int_callback(
            input_pins[i].gpio, input_pins[i].condition, input_isr_key, instance);
        instance->key_state[i].pin = &input_pins[i];
        instance->key_state[i].state = GPIO_Read(instance->key_state[i]);
    }

    furi_event_loop_subscribe_semaphore(
        instance->event_loop,
        instance->input_key_semaphore,
        FuriEventLoopEventIn,
        input_key_semaphore_callback,
        instance);
    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->input_queue,
        FuriEventLoopEventIn,
        input_queue_callback,
        instance);

    furi_hal_qei_init();
    furi_hal_qei_set_delta_pos_callback(input_qei_callback, instance);

    // Start Input Service
    furi_event_loop_run(instance->event_loop);

    return 0;
}
