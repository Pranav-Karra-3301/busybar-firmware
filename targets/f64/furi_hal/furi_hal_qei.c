
#include "furi_hal_qei.h"
#include <furi.h>
#include "rsi_qei.h"
#include "furi_hal_resources.h"
#include "furi_hal_bus.h"


#define TAG "QEI"

STATIC INLINE void furi_hal_qei_set_pos_cnt(volatile QEI_Type *pstcQei, uint32_t Position)
{
  // configure least significant word (LSW)
  pstcQei->QEI_POSITION_CNT_REG = (0xFFFF & Position);
  pstcQei->QEI_POSITION_CNT_REG |= (0xFFFF & (Position >> 16)) << 16;
}

STATIC INLINE void furi_hal_qei_set_velocity(volatile QEI_Type *pstcQei, uint32_t data)
{
  pstcQei->QEI_VELOCITY_REG = data;
}

void furi_hal_qei_init(void) {
    
    furi_hal_gpio_init(&gpio_encoder_a, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_encoder_b, GpioModeInput, GpioPullUp, GpioSpeedLow);
    
    // Enable QEI clock
    furi_hal_bus_enable(FuriHalBusQEI_PCLK);

    RSI_QEI_SetMode(QEI, QEI_ENCODING_MODE_2X);
    RSI_QEI_ConfigureDeltaTimeAndFreq(QEI, 512000000, 10000);
    //Set comparison with 1 at the beginning of the smart
    RSI_QEI_SetPosMatch(QEI, 1);
	// Set the minimum rotation speed
    furi_hal_qei_set_velocity(QEI, 1);
    // Enable an interrupt to compare transitions 0 and 1
    RSI_QEI_IntrUnMask(QEI, QEI_POS_CNT_RST_INTR_LVL | QEI_POS_CNT_MAT_INTR_LVL);
	//Mask the interruption
    RSI_QEI_IntrMask(QEI, VELOCITY_COMPUTATION_OVER_INTR_LVL);


	NVIC_ClearPendingIRQ(QEI_IRQn);
	//NVIC_SetPriority(QEI_IRQn, GPIO_INTERRUPT_PRIOPRITY7);
	//NVIC_SetPriority(QEI_IRQn, PRIORITY_50);
	NVIC_EnableIRQ(QEI_IRQn);
}

void furi_hal_qei_deinit(void) {
    // Disable QEI clock
    furi_hal_bus_disable(FuriHalBusQEI_PCLK);
    //Todo deinit GPIO
}

