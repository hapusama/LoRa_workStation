/*!
 * \file      board.c
 *
 * \brief     STM32H743 board support for the LoRaMac Class A application
 */
#include <stdio.h>
#include "stm32h7xx.h"
#include "utilities.h"
#include "gpio.h"
#include "spi.h"
#include "uart.h"
#include "timer.h"
#include "board-config.h"
#include "led-board.h"
#include "lpm-board.h"
#include "rtc-board.h"
#include "sx1276-board.h"
#include "board.h"

#define UID_WORD0_ADDRESS                         ( UID_BASE + 0U )
#define UID_WORD1_ADDRESS                         ( UID_BASE + 4U )
#define UID_WORD2_ADDRESS                         ( UID_BASE + 8U )

#define UART1_FIFO_TX_SIZE                        1024
#define UART1_FIFO_RX_SIZE                        1024

Uart_t Uart1;
uint8_t Uart1TxBuffer[UART1_FIFO_TX_SIZE];
uint8_t Uart1RxBuffer[UART1_FIFO_RX_SIZE];

static bool McuInitialized = false;
static volatile bool SystemWakeupTimeCalibrated = false;
static TimerEvent_t CalibrateSystemWakeupTimeTimer;

static void BoardUnusedIoInit( void );
static void SystemClockConfig( void );
static void SystemClockReConfig( void );
static void CalibrateSystemWakeupTime( void );
static void BoardErrorHandler( void );

static void OnCalibrateSystemWakeupTimeTimerEvent( void *context )
{
    ( void )context;
    RtcSetMcuWakeUpTime( );
    SystemWakeupTimeCalibrated = true;
}

void BoardCriticalSectionBegin( uint32_t *mask )
{
    *mask = __get_PRIMASK( );
    __disable_irq( );
}

void BoardCriticalSectionEnd( uint32_t *mask )
{
    __set_PRIMASK( *mask );
}

void BoardInitPeriph( void )
{
}

void BoardInitMcu( void )
{
    if( McuInitialized == false )
    {
        HAL_Init( );
        SystemClockConfig( );

        SCB_EnableICache( );
        SCB_EnableDCache( );

        led_init( );

        FifoInit( &Uart1.FifoTx, Uart1TxBuffer, UART1_FIFO_TX_SIZE );
        FifoInit( &Uart1.FifoRx, Uart1RxBuffer, UART1_FIFO_RX_SIZE );
        UartInit( &Uart1, UART_1, UART1_TX, UART1_RX );
        UartConfig( &Uart1, RX_TX, 115200, UART_8_BIT, UART_1_STOP_BIT,
                    NO_PARITY, NO_FLOW_CTRL );

        RtcInit( );
        BoardUnusedIoInit( );

        /* This radio board is powered externally rather than through USB. */
        LpmSetOffMode( LPM_APPLI_ID, LPM_DISABLE );
    }
    else
    {
        SystemClockReConfig( );
    }

    SpiInit( &SX1276.Spi, SPI_1, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, NC );
    SX1276IoInit( );

    if( McuInitialized == false )
    {
        McuInitialized = true;
        CalibrateSystemWakeupTime( );
    }
}

void BoardResetMcu( void )
{
    NVIC_SystemReset( );
}

void BoardDeInitMcu( void )
{
    SpiDeInit( &SX1276.Spi );
    SX1276IoDeInit( );
}

uint32_t BoardGetRandomSeed( void )
{
    return *( ( uint32_t * )UID_WORD0_ADDRESS ) ^
           *( ( uint32_t * )UID_WORD1_ADDRESS ) ^
           *( ( uint32_t * )UID_WORD2_ADDRESS );
}

void BoardGetUniqueId( uint8_t *id )
{
    uint32_t id0 = *( ( uint32_t * )UID_WORD0_ADDRESS );
    uint32_t id1 = *( ( uint32_t * )UID_WORD1_ADDRESS );
    uint32_t id2 = *( ( uint32_t * )UID_WORD2_ADDRESS );
    uint32_t mixed = id0 + id2;

    id[7] = ( uint8_t )( mixed >> 24 );
    id[6] = ( uint8_t )( mixed >> 16 );
    id[5] = ( uint8_t )( mixed >> 8 );
    id[4] = ( uint8_t )mixed;
    id[3] = ( uint8_t )( id1 >> 24 );
    id[2] = ( uint8_t )( id1 >> 16 );
    id[1] = ( uint8_t )( id1 >> 8 );
    id[0] = ( uint8_t )id1;
}

uint16_t BoardBatteryMeasureVolage( void )
{
    return 0;
}

uint32_t BoardGetBatteryVoltage( void )
{
    return 0;
}

uint8_t BoardGetBatteryLevel( void )
{
    return 0;
}

static void BoardUnusedIoInit( void )
{
    HAL_DBGMCU_EnableDBGSleepMode( );
    HAL_DBGMCU_EnableDBGStopMode( );
    HAL_DBGMCU_EnableDBGStandbyMode( );
}

static void SystemClockConfig( void )
{
    RCC_OscInitTypeDef osc = { 0 };
    RCC_ClkInitTypeDef clk = { 0 };
    RCC_PeriphCLKInitTypeDef periph = { 0 };

    HAL_PWREx_ConfigSupply( PWR_LDO_SUPPLY );
    __HAL_PWR_VOLTAGESCALING_CONFIG( PWR_REGULATOR_VOLTAGE_SCALE1 );
    while( __HAL_PWR_GET_FLAG( PWR_FLAG_VOSRDY ) == RESET )
    {
    }

    HAL_PWR_EnableBkUpAccess( );
    __HAL_RCC_LSEDRIVE_CONFIG( RCC_LSEDRIVE_LOW );

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.LSEState = RCC_LSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 4;
    osc.PLL.PLLN = 50;
    osc.PLL.PLLP = 2;
    osc.PLL.PLLQ = 4;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN = 0;
    if( HAL_RCC_OscConfig( &osc ) != HAL_OK )
    {
        BoardErrorHandler( );
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider = RCC_HCLK_DIV2;
    clk.APB3CLKDivider = RCC_APB3_DIV2;
    clk.APB1CLKDivider = RCC_APB1_DIV2;
    clk.APB2CLKDivider = RCC_APB2_DIV2;
    clk.APB4CLKDivider = RCC_APB4_DIV2;
    if( HAL_RCC_ClockConfig( &clk, FLASH_LATENCY_4 ) != HAL_OK )
    {
        BoardErrorHandler( );
    }

    periph.PeriphClockSelection = RCC_PERIPHCLK_RTC | RCC_PERIPHCLK_SPI123;
    periph.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    periph.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;
    if( HAL_RCCEx_PeriphCLKConfig( &periph ) != HAL_OK )
    {
        BoardErrorHandler( );
    }

    HAL_NVIC_SetPriority( SysTick_IRQn, 0, 0 );
}

static void CalibrateSystemWakeupTime( void )
{
    if( SystemWakeupTimeCalibrated == false )
    {
        TimerInit( &CalibrateSystemWakeupTimeTimer,
                   OnCalibrateSystemWakeupTimeTimerEvent );
        TimerSetValue( &CalibrateSystemWakeupTimeTimer, 1000 );
        TimerStart( &CalibrateSystemWakeupTimeTimer );
        while( SystemWakeupTimeCalibrated == false )
        {
        }
    }
}

static void SystemClockReConfig( void )
{
    /* Sleep mode keeps PLL and bus clocks running on this first H7 port. */
    SystemCoreClockUpdate( );
}

static void BoardErrorHandler( void )
{
    __disable_irq( );
    while( 1 )
    {
    }
}

void SysTick_Handler( void )
{
    HAL_IncTick( );
    HAL_SYSTICK_IRQHandler( );
}

uint8_t GetBoardPowerSource( void )
{
    return BATTERY_POWER;
}

void LpmEnterStopMode( void )
{
    BoardDeInitMcu( );
    HAL_SuspendTick( );
    HAL_PWR_EnterSLEEPMode( PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI );
    HAL_ResumeTick( );
}

void LpmExitStopMode( void )
{
    BoardInitMcu( );
}

void LpmEnterSleepMode( void )
{
    HAL_SuspendTick( );
    HAL_PWR_EnterSLEEPMode( PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI );
    HAL_ResumeTick( );
}

void BoardLowPowerHandler( void )
{
    __disable_irq( );
    LpmEnterLowPower( );
    __enable_irq( );
}

#if !defined( __CC_ARM )
int _write( int fd, const void *buf, size_t count )
{
    ( void )fd;
    while( UartPutBuffer( &Uart1, ( uint8_t * )buf, ( uint16_t )count ) != 0 )
    {
    }
    return ( int )count;
}

int _read( int fd, const void *buf, size_t count )
{
    size_t bytesRead = 0;
    ( void )fd;
    while( UartGetBuffer( &Uart1, ( uint8_t * )buf, ( uint16_t )count,
                          ( uint16_t * )&bytesRead ) != 0 )
    {
    }
    return ( int )bytesRead;
}
#else
int fputc( int c, FILE *stream )
{
    ( void )stream;
    while( UartPutChar( &Uart1, ( uint8_t )c ) != 0 )
    {
    }
    return c;
}

int fgetc( FILE *stream )
{
    uint8_t c = 0;
    ( void )stream;
    while( UartGetChar( &Uart1, &c ) != 0 )
    {
    }
    return ( int )c;
}
#endif
