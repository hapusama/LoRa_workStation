/*!
 * \file      spi-board.c
 *
 * \brief     STM32H743 SPI driver for the Ai-Thinker Ra-02
 */
#include "stm32h7xx.h"
#include "utilities.h"
#include "board.h"
#include "gpio.h"
#include "spi-board.h"

static SPI_HandleTypeDef SpiHandle[2];

void SpiInit( Spi_t *obj, SpiId_t spiId, PinNames mosi, PinNames miso,
              PinNames sclk, PinNames nss )
{
    SPI_HandleTypeDef *handle;

    CRITICAL_SECTION_BEGIN( );
    obj->SpiId = spiId;
    handle = &SpiHandle[spiId];

    if( spiId == SPI_1 )
    {
        __HAL_RCC_SPI1_FORCE_RESET( );
        __HAL_RCC_SPI1_RELEASE_RESET( );
        __HAL_RCC_SPI1_CLK_ENABLE( );
        handle->Instance = SPI1;
        GpioInit( &obj->Mosi, mosi, PIN_ALTERNATE_FCT, PIN_PUSH_PULL,
                  PIN_PULL_DOWN, GPIO_AF5_SPI1 );
        GpioInit( &obj->Miso, miso, PIN_ALTERNATE_FCT, PIN_PUSH_PULL,
                  PIN_PULL_DOWN, GPIO_AF5_SPI1 );
        GpioInit( &obj->Sclk, sclk, PIN_ALTERNATE_FCT, PIN_PUSH_PULL,
                  PIN_PULL_DOWN, GPIO_AF5_SPI1 );
        GpioInit( &obj->Nss, nss, PIN_ALTERNATE_FCT, PIN_PUSH_PULL,
                  PIN_PULL_UP, GPIO_AF5_SPI1 );
    }
    else
    {
        __HAL_RCC_SPI2_FORCE_RESET( );
        __HAL_RCC_SPI2_RELEASE_RESET( );
        __HAL_RCC_SPI2_CLK_ENABLE( );
        handle->Instance = SPI2;
        GpioInit( &obj->Mosi, mosi, PIN_ALTERNATE_FCT, PIN_PUSH_PULL,
                  PIN_PULL_DOWN, GPIO_AF5_SPI2 );
        GpioInit( &obj->Miso, miso, PIN_ALTERNATE_FCT, PIN_PUSH_PULL,
                  PIN_PULL_DOWN, GPIO_AF5_SPI2 );
        GpioInit( &obj->Sclk, sclk, PIN_ALTERNATE_FCT, PIN_PUSH_PULL,
                  PIN_PULL_DOWN, GPIO_AF5_SPI2 );
        GpioInit( &obj->Nss, nss, PIN_ALTERNATE_FCT, PIN_PUSH_PULL,
                  PIN_PULL_UP, GPIO_AF5_SPI2 );
    }

    handle->Init.NSS = ( nss == NC ) ? SPI_NSS_SOFT : SPI_NSS_HARD_INPUT;
    SpiFormat( obj, SPI_DATASIZE_8BIT, SPI_POLARITY_LOW,
               SPI_PHASE_1EDGE, ( nss == NC ) ? 0 : 1 );
    SpiFrequency( obj, 10000000U );

    handle->Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    handle->Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    handle->Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    handle->Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    handle->Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    handle->Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    handle->Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
    handle->Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    handle->Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    handle->Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
    handle->Init.IOSwap = SPI_IO_SWAP_DISABLE;

    if( HAL_SPI_Init( handle ) != HAL_OK )
    {
        assert_param( FAIL );
    }
    CRITICAL_SECTION_END( );
}

void SpiDeInit( Spi_t *obj )
{
    SPI_HandleTypeDef *handle = &SpiHandle[obj->SpiId];

    __HAL_SPI_DISABLE( handle );
    handle->State = HAL_SPI_STATE_RESET;
    if( obj->SpiId == SPI_1 )
    {
        __HAL_RCC_SPI1_CLK_DISABLE( );
    }
    else
    {
        __HAL_RCC_SPI2_CLK_DISABLE( );
    }
    GpioInit( &obj->Mosi, obj->Mosi.pin, PIN_OUTPUT, PIN_PUSH_PULL,
              PIN_NO_PULL, 0 );
    GpioInit( &obj->Miso, obj->Miso.pin, PIN_OUTPUT, PIN_PUSH_PULL,
              PIN_PULL_DOWN, 0 );
    GpioInit( &obj->Sclk, obj->Sclk.pin, PIN_OUTPUT, PIN_PUSH_PULL,
              PIN_NO_PULL, 0 );
    GpioInit( &obj->Nss, obj->Nss.pin, PIN_OUTPUT, PIN_PUSH_PULL,
              PIN_PULL_UP, 1 );
}

void SpiFormat( Spi_t *obj, int8_t bits, int8_t cpol, int8_t cpha,
                int8_t slave )
{
    SPI_HandleTypeDef *handle = &SpiHandle[obj->SpiId];

    handle->Init.Direction = SPI_DIRECTION_2LINES;
    handle->Init.DataSize = ( bits == SPI_DATASIZE_8BIT ) ?
                            SPI_DATASIZE_8BIT : SPI_DATASIZE_16BIT;
    handle->Init.CLKPolarity = cpol;
    handle->Init.CLKPhase = cpha;
    handle->Init.FirstBit = SPI_FIRSTBIT_MSB;
    handle->Init.TIMode = SPI_TIMODE_DISABLE;
    handle->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    handle->Init.CRCPolynomial = 7;
    handle->Init.Mode = ( slave == 0 ) ? SPI_MODE_MASTER : SPI_MODE_SLAVE;
}

void SpiFrequency( Spi_t *obj, uint32_t hz )
{
    ( void )hz;

    /* PLL1Q is 200 MHz. DIV32 gives 6.25 MHz, below Ra-02's 10 MHz limit. */
    SpiHandle[obj->SpiId].Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
}

uint16_t SpiInOut( Spi_t *obj, uint16_t outData )
{
    uint8_t txData = ( uint8_t )outData;
    uint8_t rxData = 0;

    if( ( obj == NULL ) || ( SpiHandle[obj->SpiId].Instance == NULL ) )
    {
        assert_param( FAIL );
        return 0;
    }

    CRITICAL_SECTION_BEGIN( );
    if( HAL_SPI_TransmitReceive( &SpiHandle[obj->SpiId], &txData, &rxData,
                                 1, HAL_MAX_DELAY ) != HAL_OK )
    {
        rxData = 0;
    }
    CRITICAL_SECTION_END( );

    return rxData;
}
