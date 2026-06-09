/*!
 * \file      main.c
 *
 * \brief     Ping-Pong implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "gpio.h"
#include "delay.h"
#include "timer.h"
#include "radio.h"

#define RF_FREQUENCY                                487700000	//486300000 // Hz

#define TX_OUTPUT_POWER                             14        // dBm

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       12         // [SF7..SF12]

#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx

#define LORA_SYMBOL_TIMEOUT                         0x3FF         // Symbols

#define LORA_IQ_INVERSION_ON                        false

#define BUFFER_SIZE                                 255

/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;

/*!
 * indicate whether in rx mode
 */
static bool isRxing = false;

/*!
 * indicate whether rssi data can be printed
 */
static bool isRssiCanBePrinted = false;

/*!
 * indicate rssi data buffer index
 */
static uint32_t rssiIdx = 0;

/*!
 * indicate real-time rssi value buffer
 */
#define RSSI_BUFFER_SIZE														5000 // 5s restore length when delay is 10ms
static int16_t rssiBuffer[RSSI_BUFFER_SIZE];

/*!
 * LED GPIO pins objects
 */
extern Gpio_t Led1;
extern Gpio_t Led2;
extern Gpio_t Led3;

/*!
 * \brief Function to be executed on event that header is valid
 */
void OnValidHeader( void );

/*!
 * \brief Function to be executed on event that header is valid
 */
void OnPayloadCRCError( bool isCRCError );

/*!
 * \brief Function to be executed on Radio Tx Done event
 */
void OnTxDone( void );

/*!
 * \brief Function to be executed on Radio Rx Done event
 */
void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );

/*!
 * \brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout( void );

/*!
 * \brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout( void );

/*!
 * \brief Function executed on Radio Rx Error event
 */
void OnRxError( void );

/*!
 * Prints the provided buffer in HEX
 * 
 * \param buffer Buffer to be printed
 * \param size   Buffer size to be printed
 */
void PrintHexBuffer( uint8_t *buffer, uint8_t size );

/**
 * Main application entry point.
 */
int main( void )
{
		uint32_t j;
	
    // Target board initialization
    BoardInitMcu( );
    BoardInitPeriph( );

    // Radio initialization
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;
		RadioEvents.ValidHeader = OnValidHeader;
		RadioEvents.PayloadCRCError = OnPayloadCRCError;

    Radio.Init( &RadioEvents );

    Radio.SetChannel( RF_FREQUENCY );
	
		Radio.SetPublicNetwork(true);

    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                                   LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                                   LORA_SYMBOL_TIMEOUT, false,
                                   0, true, 0, 0, LORA_IQ_INVERSION_ON, true );

    Radio.Rx( 0 );

		printf("==========Continuous Reception Mode Start==========\r\n");
		while ( 1 )
		{
				if(isRxing && (rssiIdx < RSSI_BUFFER_SIZE))
				{
					rssiBuffer[rssiIdx++] = Radio.Rssi(MODEM_LORA);
					DelayMs( 5 );
				}
				else if(isRssiCanBePrinted)
				{
					printf("%d\t", rssiIdx);
					for(j = 0; j < rssiIdx; j++)
						printf("%d ", rssiBuffer[j]);
					printf("\r\n");
					
					isRssiCanBePrinted = false;
				}
		}
    
}

void PrintHexBuffer( uint8_t *buffer, uint8_t size )
{
    uint8_t newline = 0;

    for( uint8_t i = 0; i < size; i++ )
    {
        if( newline != 0 )
        {
            printf( "\r\n" );
            newline = 0;
        }

        printf( "%02X ", buffer[i] );

        if( ( ( i + 1 ) % 16 ) == 0 )
        {
            newline = 1;
        }
    }
    printf( "\r\n" );
}

void OnValidHeader( void )
{
		rssiIdx = 0;
		isRxing = true;
	
		printf("HDOK\t"); // vaild header
}

void OnPayloadCRCError( bool isCRCError )
{
		if(isCRCError)
			printf("PLFL\t");
		else
			printf("PLOK\t");
}

void OnTxDone( void )
{
}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
		uint8_t i;
		//uint32_t j;
	
		GpioWrite(&Led3, 0);// led on
	
		isRxing = false;
	
		printf( "%d\t", (payload[0] + (payload[1] << 8)) ); // print the packet number
	
		for(i=2; i < size; i++) // don't print the first two bytes, otherwise it will cause problem of copying to excel
			printf("%c", payload[i]);
		printf("\t");
	
		//PrintHexBuffer(payload, size);
		for(i = 0; i < size; i++)
        printf("%02X ", payload[i]);
		printf("\t");
	
		printf("%d\t%d\t", rssi, snr);
	
		printf("%d\t%d\t", Radio.GetValidHeaderCnt(), Radio.GetValidPacketCnt()+1); // the pkt cnt may has not been flushed, when readed it at this time
	
		// print real-time rssi
		isRssiCanBePrinted = true;
		/*printf("%d\t", rssiIdx); // executed in main function, otherwise it will block in this function when length large than 250
		for(j = 0; j < rssiIdx; j++)
			printf("%d ", rssiBuffer[j]);
		printf("\r\n");*/
		
		GpioWrite(&Led3, 1);// led off
}

void OnTxTimeout( void )
{
}

void OnRxTimeout( void ) // no use. 2022.06.02
{
}

void OnRxError( void ) // no use. 2022.06.02
{
}
