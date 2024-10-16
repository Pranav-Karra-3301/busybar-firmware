#include <furi_hal_resources.h>
#include <furi_hal_bus.h>

#include <sl_si91x_gpio_common.h>

#define PADSELECTION_ALL_M4  (0x3FFDFEUL) // GPIO 6...15, 46...57
#define PADSELECTION1_ALL_M4 (0x000FFFUL) // ULP GPIO 0...11

#define TAG "FuriHalResources"

const GpioPin gpio_6 = {.type = GpioTypeHp, .pin = 6};
const GpioPin gpio_7 = {.type = GpioTypeHp, .pin = 7};
const GpioPin gpio_8 = {.type = GpioTypeHp, .pin = 8};
const GpioPin gpio_9 = {.type = GpioTypeHp, .pin = 9};
const GpioPin gpio_10 = {.type = GpioTypeHp, .pin = 10};
const GpioPin gpio_11 = {.type = GpioTypeHp, .pin = 11};
const GpioPin gpio_12 = {.type = GpioTypeHp, .pin = 12};
const GpioPin gpio_15 = {.type = GpioTypeHp, .pin = 15};
const GpioPin gpio_25 = {.type = GpioTypeHp, .pin = 25};
const GpioPin gpio_26 = {.type = GpioTypeHp, .pin = 26};
const GpioPin gpio_27 = {.type = GpioTypeHp, .pin = 27};
const GpioPin gpio_28 = {.type = GpioTypeHp, .pin = 28};
const GpioPin gpio_29 = {.type = GpioTypeHp, .pin = 29};
const GpioPin gpio_30 = {.type = GpioTypeHp, .pin = 30};
const GpioPin gpio_46 = {.type = GpioTypeHp, .pin = 40};
const GpioPin gpio_47 = {.type = GpioTypeHp, .pin = 40};
const GpioPin gpio_48 = {.type = GpioTypeHp, .pin = 40};
const GpioPin gpio_49 = {.type = GpioTypeHp, .pin = 40};
const GpioPin gpio_50 = {.type = GpioTypeHp, .pin = 50};
const GpioPin gpio_51 = {.type = GpioTypeHp, .pin = 51};
const GpioPin gpio_52 = {.type = GpioTypeHp, .pin = 52};
const GpioPin gpio_53 = {.type = GpioTypeHp, .pin = 53};
const GpioPin gpio_54 = {.type = GpioTypeHp, .pin = 54};
const GpioPin gpio_55 = {.type = GpioTypeHp, .pin = 55};
const GpioPin gpio_56 = {.type = GpioTypeHp, .pin = 56};
const GpioPin gpio_57 = {.type = GpioTypeHp, .pin = 57};

const GpioPin gpio_ulp_0 = {.type = GpioTypeUlp, .pin = 0};
const GpioPin gpio_ulp_1 = {.type = GpioTypeUlp, .pin = 1};
const GpioPin gpio_ulp_2 = {.type = GpioTypeUlp, .pin = 2};
const GpioPin gpio_ulp_3 = {.type = GpioTypeUlp, .pin = 3};
const GpioPin gpio_ulp_4 = {.type = GpioTypeUlp, .pin = 4};
const GpioPin gpio_ulp_5 = {.type = GpioTypeUlp, .pin = 5};
const GpioPin gpio_ulp_6 = {.type = GpioTypeUlp, .pin = 6};
const GpioPin gpio_ulp_7 = {.type = GpioTypeUlp, .pin = 7};
const GpioPin gpio_ulp_8 = {.type = GpioTypeUlp, .pin = 8};
const GpioPin gpio_ulp_9 = {.type = GpioTypeUlp, .pin = 9};
const GpioPin gpio_ulp_10 = {.type = GpioTypeUlp, .pin = 10};
const GpioPin gpio_ulp_11 = {.type = GpioTypeUlp, .pin = 11};

const GpioPin gpio_uulp_0 = {.type = GpioTypeUulp, .pin = 0};
const GpioPin gpio_uulp_1 = {.type = GpioTypeUulp, .pin = 1};
const GpioPin gpio_uulp_2 = {.type = GpioTypeUulp, .pin = 2};
const GpioPin gpio_uulp_3 = {.type = GpioTypeUulp, .pin = 3};

void furi_hal_resources_init_early(void) {
    // Enable GPIO clock
    furi_hal_bus_enable(FuriHalBusEGPIO_CLK);
    // Enable ULP GPIO clock
    furi_hal_bus_enable(FuriHalBusUlpEGPIO_CLK_EN);
    // Control HP GPIO pads from M4
    PADSELECTION = PADSELECTION_ALL_M4;
    // Control ULP GPIO pads from M4
    PADSELECTION_1 = PADSELECTION1_ALL_M4;
}

void furi_hal_resources_deinit_early(void) {

}

void furi_hal_resources_init(void) {

}
