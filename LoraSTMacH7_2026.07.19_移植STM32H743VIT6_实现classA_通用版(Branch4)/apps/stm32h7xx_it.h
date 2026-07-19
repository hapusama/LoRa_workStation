#ifndef STM32H7XX_IT_H
#define STM32H7XX_IT_H

#include "stm32h7xx.h"

void NMI_Handler( void );
void HardFault_Handler( void );
void MemManage_Handler( void );
void BusFault_Handler( void );
void UsageFault_Handler( void );
void SVC_Handler( void );
void DebugMon_Handler( void );
void PendSV_Handler( void );

#endif
