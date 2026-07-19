/*!
 * \file      uart-board.c
 *
 * \brief     STM32H743 USART1 board driver
 */
#include "stm32h7xx.h"
#include "utilities.h"
#include "uart-board.h"

#define TX_BUFFER_RETRY_COUNT                       10

static UART_HandleTypeDef UartHandle;
static uint8_t RxData;

extern Uart_t Uart1;

void UartMcuInit( Uart_t *obj, UartId_t uartId, PinNames tx, PinNames rx )
{
    obj->UartId = uartId;

    __HAL_RCC_USART1_FORCE_RESET( );
    __HAL_RCC_USART1_RELEASE_RESET( );
    __HAL_RCC_USART1_CLK_ENABLE( );

    GpioInit( &obj->Tx, tx, PIN_ALTERNATE_FCT, PIN_PUSH_PULL,
              PIN_PULL_UP, GPIO_AF7_USART1 );
    GpioInit( &obj->Rx, rx, PIN_ALTERNATE_FCT, PIN_PUSH_PULL,
              PIN_PULL_UP, GPIO_AF7_USART1 );
}

void UartMcuConfig( Uart_t *obj, UartMode_t mode, uint32_t baudrate,
                    WordLength_t wordLength, StopBits_t stopBits,
                    Parity_t parity, FlowCtrl_t flowCtrl )
{
    UartHandle.Instance = USART1;
    UartHandle.Init.BaudRate = baudrate;

    if( mode == TX_ONLY )
    {
        assert_param( obj->FifoTx.Data != NULL );
        UartHandle.Init.Mode = UART_MODE_TX;
    }
    else if( mode == RX_ONLY )
    {
        assert_param( obj->FifoRx.Data != NULL );
        UartHandle.Init.Mode = UART_MODE_RX;
    }
    else
    {
        assert_param( ( obj->FifoTx.Data != NULL ) &&
                      ( obj->FifoRx.Data != NULL ) );
        UartHandle.Init.Mode = UART_MODE_TX_RX;
    }

    UartHandle.Init.WordLength = ( wordLength == UART_9_BIT ) ?
                                 UART_WORDLENGTH_9B : UART_WORDLENGTH_8B;
    UartHandle.Init.StopBits = ( stopBits == UART_2_STOP_BIT ) ?
                               UART_STOPBITS_2 : UART_STOPBITS_1;

    if( parity == EVEN_PARITY )
    {
        UartHandle.Init.Parity = UART_PARITY_EVEN;
    }
    else if( parity == ODD_PARITY )
    {
        UartHandle.Init.Parity = UART_PARITY_ODD;
    }
    else
    {
        UartHandle.Init.Parity = UART_PARITY_NONE;
    }

    if( flowCtrl == RTS_FLOW_CTRL )
    {
        UartHandle.Init.HwFlowCtl = UART_HWCONTROL_RTS;
    }
    else if( flowCtrl == CTS_FLOW_CTRL )
    {
        UartHandle.Init.HwFlowCtl = UART_HWCONTROL_CTS;
    }
    else if( flowCtrl == RTS_CTS_FLOW_CTRL )
    {
        UartHandle.Init.HwFlowCtl = UART_HWCONTROL_RTS_CTS;
    }
    else
    {
        UartHandle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    }

    UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;
    UartHandle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    UartHandle.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    UartHandle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if( HAL_UART_Init( &UartHandle ) != HAL_OK )
    {
        assert_param( FAIL );
    }
    HAL_UARTEx_DisableFifoMode( &UartHandle );

    HAL_NVIC_SetPriority( USART1_IRQn, 1, 0 );
    HAL_NVIC_EnableIRQ( USART1_IRQn );
    __HAL_UART_ENABLE_IT( &UartHandle, UART_IT_RXNE );
}

void UartMcuDeInit( Uart_t *obj )
{
    HAL_NVIC_DisableIRQ( USART1_IRQn );
    HAL_UART_DeInit( &UartHandle );
    __HAL_RCC_USART1_FORCE_RESET( );
    __HAL_RCC_USART1_RELEASE_RESET( );
    __HAL_RCC_USART1_CLK_DISABLE( );
    GpioInit( &obj->Tx, obj->Tx.pin, PIN_ANALOGIC, PIN_PUSH_PULL,
              PIN_NO_PULL, 0 );
    GpioInit( &obj->Rx, obj->Rx.pin, PIN_ANALOGIC, PIN_PUSH_PULL,
              PIN_NO_PULL, 0 );
}

uint8_t UartMcuPutChar( Uart_t *obj, uint8_t data )
{
    CRITICAL_SECTION_BEGIN( );
    if( IsFifoFull( &obj->FifoTx ) == true )
    {
        CRITICAL_SECTION_END( );
        return 1;
    }

    FifoPush( &obj->FifoTx, data );
    __HAL_UART_ENABLE_IT( &UartHandle, UART_IT_TXE );
    CRITICAL_SECTION_END( );
    return 0;
}

uint8_t UartMcuGetChar( Uart_t *obj, uint8_t *data )
{
    CRITICAL_SECTION_BEGIN( );
    if( IsFifoEmpty( &obj->FifoRx ) == false )
    {
        *data = FifoPop( &obj->FifoRx );
        CRITICAL_SECTION_END( );
        return 0;
    }
    CRITICAL_SECTION_END( );
    return 1;
}

uint8_t UartMcuPutBuffer( Uart_t *obj, uint8_t *buffer, uint16_t size )
{
    uint16_t i;

    for( i = 0; i < size; i++ )
    {
        uint8_t retryCount = 0;
        while( UartMcuPutChar( obj, buffer[i] ) != 0 )
        {
            if( ++retryCount > TX_BUFFER_RETRY_COUNT )
            {
                return 1;
            }
        }
    }
    return 0;
}

uint8_t UartMcuGetBuffer( Uart_t *obj, uint8_t *buffer, uint16_t size,
                          uint16_t *nbReadBytes )
{
    uint16_t localSize = 0;

    while( ( localSize < size ) &&
           ( UartMcuGetChar( obj, buffer + localSize ) == 0 ) )
    {
        localSize++;
    }
    *nbReadBytes = localSize;
    return ( localSize == 0 ) ? 1 : 0;
}

void USART1_IRQHandler( void )
{
    uint32_t status = UartHandle.Instance->ISR;

    if( ( status & UART_FLAG_RXNE ) != 0U )
    {
        RxData = ( uint8_t )UartHandle.Instance->RDR;
        if( IsFifoFull( &Uart1.FifoRx ) == false )
        {
            FifoPush( &Uart1.FifoRx, RxData );
        }
        if( Uart1.IrqNotify != NULL )
        {
            Uart1.IrqNotify( UART_NOTIFY_RX );
        }
    }

    if( ( ( status & UART_FLAG_TXE ) != 0U ) &&
        ( __HAL_UART_GET_IT_SOURCE( &UartHandle, UART_IT_TXE ) != RESET ) )
    {
        if( IsFifoEmpty( &Uart1.FifoTx ) == false )
        {
            UartHandle.Instance->TDR = FifoPop( &Uart1.FifoTx );
        }
        else
        {
            __HAL_UART_DISABLE_IT( &UartHandle, UART_IT_TXE );
            if( Uart1.IrqNotify != NULL )
            {
                Uart1.IrqNotify( UART_NOTIFY_TX );
            }
        }
    }

    if( ( status & ( UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE ) ) != 0U )
    {
        __HAL_UART_CLEAR_FLAG( &UartHandle,
                               UART_CLEAR_OREF | UART_CLEAR_NEF |
                               UART_CLEAR_FEF );
    }
}
