/*!
 * \file      main.c
 *
 * \brief     LoRaMac classA device implementation
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

/*! \file classA/NucleoL152/main.c */

#include <stdio.h>
#include "utilities.h"
#include "board.h"
#include "gpio.h"
#include "timer.h"
#include "radio.h"
#include "LoRaMac.h"
#include "sx1276Regs-LoRa.h"

/*!
 * Defines the application data transmission duty cycle. 3s, value in [ms].
 */
#define APP_TX_DUTYCYCLE                            3000

#define TX_OUTPUT_POWER                             14        // dBm

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]

#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]

#define LORA_SPREADING_FACTOR                       10         // [SF7..SF12]

#define LORA_PREAMBLE_LENGTH                        32         // Same for TX/RX, in symbols

#define RX_WND_1_FREQ																510000000

#define RX_WND_2_FREQ                         			505300000

#define RX_WND_2_SF																	10

#define LORAWAN_APP_DATA_MAX_SIZE                   242  //User application data buffer size

#define LORAWAN_TX_BUFFER_MAX_SIZE									255

#define MAJOR_VERSION_LORAWAN												0

#define MTYPE_JOIN_REQUEST_MASK											0x00
#define MTYPE_JOIN_ACCEPT_MASK											0x20
#define MTYPE_UNCONFIRMED_DATA_UP_MASK							0x40
#define MTYPE_UNCONFIRMED_DATA_DOWN_MASK						0x60
#define MTYPE_CONFIRMED_DATA_UP_MASK								0x80
#define MTYPE_CONFIRMED_DATA_DOWN_MASK							0xA0
#define MTYPE_RFU_MASK															0xC0
#define MTYPE_PROPRIETARY_MASK											0xE0

/*!
 * frequency channel
 */
static const uint32_t channel[] = { 486300000, 486500000, 486700000, 486900000, 487100000, 487300000, 487500000, 487700000 };

/*!
 * Device address
 */
static uint32_t DevAddr = ( uint32_t )0x11223344;

/*!
 * Application port
 */
static uint8_t AppPort = 88;

/*!
 * User application tx data size
 */
static uint8_t AppTxDataSize = 20;

/*!
 * User application rx data size
 */
static uint8_t AppRxDataSize = 0;

/*!
 * User application tx data
 */
static uint8_t AppTxDataBuffer[LORAWAN_APP_DATA_MAX_SIZE];

/*!
 * User application rx data
 */
static uint8_t AppRxDataBuffer[LORAWAN_APP_DATA_MAX_SIZE];

/*!
 * Send frame
 */
static uint8_t LoRaMacBuffer[LORAWAN_TX_BUFFER_MAX_SIZE];

/*!
 * LoRaMAC frame counter. Each time a packet is sent the counter is incremented.
 * Only the 16 LSB bits are sent
 */
static uint32_t UpLinkCounter = 0;

/*!
 * LoRaMAC frame counter. Each time a packet is received the counter is incremented.
 * Only the 16 LSB bits are received
 */
static uint32_t DownLinkCounter = 0;

/*!
 * Indicates if the node is connected to a private or public network
 */
static bool PublicNetwork = true;

/*!
 * LoRaMac reception windows delay
 * \remark normal frame: RxWindowXDelay = ReceiveDelayX - RADIO_WAKEUP_TIME
 *         join frame  : RxWindowXDelay = JoinAcceptDelayX - RADIO_WAKEUP_TIME
 */
static uint32_t RxWindow1Delay = 1000;
static uint32_t RxWindow2Delay = 2000;

/*!
 * Maximum RX window duration
 */
uint32_t MaxRxWindow = 3000;

/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;

/*!
 * LoRaMac reception windows timers
 */
static TimerEvent_t RxWindowTimer1;
static TimerEvent_t RxWindowTimer2;

/*!
 * Timer to handle the application data transmission duty cycle
 */
static TimerEvent_t TxNextPacketTimer;

/*!
 * Holds the current rx window slot
 */
static LoRaMacRxSlot_t RxSlot;

/*!
 * Defines the application data transmission duty cycle
 */
static uint32_t TxDutyCycleTime;

static TimerTime_t AggregatedLastTxDoneTime;

static bool TxRadioParamsPrinted = false;

/*!
 * LED GPIO pins objects
 */
extern Gpio_t Led1;
extern Gpio_t Led2;
extern Gpio_t Led3;

/*!
 * Device states
 */
static enum eDeviceState
{
    DEVICE_STATE_INIT,
    DEVICE_STATE_SEND,
    DEVICE_STATE_CYCLE,
    DEVICE_STATE_SLEEP
}DeviceState;

/*!
 * Prints the provided buffer in HEX
 * 
 * \param buffer Buffer to be printed
 * \param size   Buffer size to be printed
 */
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

/*!
 * \brief Function to be executed on Radio Tx Done event
 */
void OnTxDone( void )
{
		TimerTime_t curTime = TimerGetCurrentTime( );
	
		Radio.Sleep( );
	
		// Setup timers
		TimerSetValue( &RxWindowTimer1, RxWindow1Delay );
		TimerStart( &RxWindowTimer1 );
	
		TimerSetValue( &RxWindowTimer2, RxWindow2Delay );
    TimerStart( &RxWindowTimer2 );/**/
	
		// Update Aggregated last tx done time
    AggregatedLastTxDoneTime = curTime;
	
		GpioWrite(&Led3, 1);// led off
}

/*!
 * \brief Function to be executed on Radio Rx Done event
 */
void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
		uint8_t i;
		uint8_t pktHeaderLen = 0;
		uint8_t mtype = 0;
		uint32_t address = 0;
		uint8_t fCtrl = 0;
		uint8_t fPort = 0;
		uint32_t mic = 0x12345678;
		uint32_t micRx = 0;
	
		uint16_t sequenceCounter = 0;
    uint16_t sequenceCounterPrev = 0;
    uint16_t sequenceCounterDiff = 0;
    uint32_t downLinkCounter = 0;
	
		uint8_t appPayloadStartIndex = 0;
	
		Radio.Sleep( );
    TimerStop( &RxWindowTimer2 );
	
		GpioWrite(&Led2, 0);// led on
	
		// MHDR: | (7..5) MType | (4..2) RFU | (1..0) Major |
		mtype = payload[pktHeaderLen++] & 0xE0;
	
		switch( mtype )
		{
		case MTYPE_UNCONFIRMED_DATA_DOWN_MASK:
		case MTYPE_CONFIRMED_DATA_DOWN_MASK:
				// MHDR |                FHDR                | FPort |   FRMPayload   | MIC
				//			|   DevAddr   | FCtrl | FCnt | FOpts |
				address = payload[pktHeaderLen++];
				address |= ( (uint32_t)payload[pktHeaderLen++] << 8 );
				address |= ( (uint32_t)payload[pktHeaderLen++] << 16 );
				address |= ( (uint32_t)payload[pktHeaderLen++] << 24 );
				if (address != DevAddr)
				{
						printf("[Info] Received a pkt with address error\r\n");
						return ;
				}
		
				fCtrl = payload[pktHeaderLen++];
		
				sequenceCounter = ( uint16_t )payload[pktHeaderLen++];
        sequenceCounter |= ( uint16_t )payload[pktHeaderLen++] << 8;
		
				appPayloadStartIndex = 9 + fCtrl & 0x0F;
		
				micRx |= ( uint32_t )payload[size - 4];
				micRx |= ( ( uint32_t )payload[size - 3] << 8 );
				micRx |= ( ( uint32_t )payload[size - 2] << 16 );
				micRx |= ( ( uint32_t )payload[size - 1] << 24 );
				if (mic != micRx)
				{
						printf("[Info] Received a pkt with mic error\r\n");
						return ;
				}
		
				sequenceCounterPrev = ( uint16_t )DownLinkCounter;
        sequenceCounterDiff = ( sequenceCounter - sequenceCounterPrev );
				if( sequenceCounterDiff < ( 1 << 15 ) )
						downLinkCounter += sequenceCounterDiff;
				else
						downLinkCounter = downLinkCounter + 0x10000 + ( int16_t )sequenceCounterDiff;
				DownLinkCounter = downLinkCounter;
				
				fPort = payload[pktHeaderLen++];
				if (fPort!=AppPort)
				{
						printf("[Info] Received a pkt with fPort error\r\n");
						return ;
				}
				
				AppRxDataSize = size - 4 - appPayloadStartIndex;
				for(i=0; i<AppRxDataSize; i++)
					AppRxDataBuffer[i] = payload[appPayloadStartIndex++];
		
				break;
		default:
				printf("[Info] Received a pkt with mtype error\r\n");
				break;
		}
	
		printf("rx done\r\n");
		PrintHexBuffer( AppRxDataBuffer, AppRxDataSize );
		
		//GpioToggle(&Led2);
		GpioWrite(&Led2, 1);// led off
}

/*!
 * \brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout( void )
{
		Radio.Sleep( );
}

/*!
 * \brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout( void )
{
		Radio.Sleep( );
		
		if( RxSlot == RX_SLOT_WIN_1 )
		{
			if( TimerGetElapsedTime( AggregatedLastTxDoneTime ) >= RxWindow2Delay )
			{
					TimerStop( &RxWindowTimer2 );
			}
		}
		
		if(RxSlot == RX_SLOT_WIN_1)
			printf("rx 1 timeout\r\n");
		else
			printf("rx 2 timeout\r\n");
}

/*!
 * \brief Function executed on Radio Rx Error event
 */
void OnRxError( void )
{
		Radio.Sleep( );
		
		if( RxSlot == RX_SLOT_WIN_1 )
		{
			if( TimerGetElapsedTime( AggregatedLastTxDoneTime ) >= RxWindow2Delay )
			{
					TimerStop( &RxWindowTimer2 );
			}
		}
}

/*!
 * \brief Function executed on first Rx window timer event
 */
static void OnRxWnd1TimerEvent( void* context )
{
		uint16_t symbTimeout;
	
		TimerStop( &RxWindowTimer1 );
	
    RxSlot = RX_SLOT_WIN_1;	
		symbTimeout = 125000 / (1<<(LORA_SPREADING_FACTOR - LORA_BANDWIDTH + 1));//125000*(1<<BW) / (1<<SF) / 2; symbol count in 1/2 second
	
		Radio.SetChannel( RX_WND_1_FREQ );
		Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                                   LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                                   symbTimeout, false,
                                   0, true, 0, 0, false, false );
		Radio.Rx( MaxRxWindow );
}

/*!
 * \brief Function executed on second Rx window timer event
 */
static void OnRxWnd2TimerEvent( void* context )
{
		uint16_t symbTimeout;
	
		TimerStop( &RxWindowTimer2 );
	
		if( Radio.GetStatus( ) != RF_IDLE )
        return;
		
		RxSlot = RX_SLOT_WIN_2;
		symbTimeout = 125000 / (1<<(RX_WND_2_SF - LORA_BANDWIDTH + 1));//125000*(1<<BW) / (1<<SF) / 2; symbol count in 1/2 second
	
		Radio.SetChannel( RX_WND_2_FREQ );
		Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, RX_WND_2_SF,
                                   LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                                   symbTimeout, false,
                                   0, true, 0, 0, false, false );
		Radio.Rx( MaxRxWindow );
}

/*!
 * \brief Function executed on TxNextPacket Timeout event
 */
static void OnTxNextPacketTimerEvent( void* context )
{
		TimerStop( &TxNextPacketTimer );
	
		DeviceState = DEVICE_STATE_SEND;
}

static uint32_t DecodeLoRaBandwidthHz( uint8_t bandwidthReg )
{
    switch( bandwidthReg )
    {
    case 7:
        return 125000;
    case 8:
        return 250000;
    case 9:
        return 500000;
    default:
        return 0;
    }
}

static int8_t DecodeLoRaTxPowerDbm( uint8_t paConfig, uint8_t paDac )
{
    uint8_t outputPower = paConfig & 0x0F;

    if( ( paConfig & RFLR_PACONFIG_PASELECT_PABOOST ) == RFLR_PACONFIG_PASELECT_PABOOST )
    {
        if( ( paDac & RFLR_PADAC_20DBM_ON ) == RFLR_PADAC_20DBM_ON )
        {
            return ( int8_t )( outputPower + 5 );
        }
        return ( int8_t )( outputPower + 2 );
    }

    if( ( ( paConfig >> 4 ) & 0x07 ) == 0 )
    {
        return ( int8_t )( outputPower - 4 );
    }
    return ( int8_t )outputPower;
}

static void PrintLoRaTxRadioParams( void )
{
    uint8_t modemConfig1 = Radio.Read( REG_LR_MODEMCONFIG1 );
    uint8_t modemConfig2 = Radio.Read( REG_LR_MODEMCONFIG2 );
    uint8_t preambleMsb = Radio.Read( REG_LR_PREAMBLEMSB );
    uint8_t preambleLsb = Radio.Read( REG_LR_PREAMBLELSB );
    uint8_t paConfig = Radio.Read( REG_LR_PACONFIG );
    uint8_t paDac = Radio.Read( REG_LR_PADAC );

    uint8_t bandwidthReg = ( modemConfig1 >> 4 ) & 0x0F;
    uint8_t codingRate = ( modemConfig1 >> 1 ) & 0x07;
    uint8_t spreadingFactor = ( modemConfig2 >> 4 ) & 0x0F;
    uint16_t preambleLen = ( ( uint16_t )preambleMsb << 8 ) | preambleLsb;
    uint32_t bandwidthHz = DecodeLoRaBandwidthHz( bandwidthReg );
    int8_t txPowerDbm = DecodeLoRaTxPowerDbm( paConfig, paDac );

    printf( "\r\n[Radio TX Reg] RegModemConfig1=0x%02X RegModemConfig2=0x%02X RegPreamble=0x%02X%02X RegPaConfig=0x%02X RegPaDac=0x%02X\r\n",
            modemConfig1, modemConfig2, preambleMsb, preambleLsb, paConfig, paDac );

    printf( "[Radio TX Param] SF%u, BW=%lu Hz, CR=4/%u, Preamble=%u symbols, TP=%d dBm\r\n\r\n",
            ( unsigned int )spreadingFactor, ( unsigned long )bandwidthHz,
            ( unsigned int )( codingRate + 4 ), ( unsigned int )preambleLen,
            ( int )txPowerDbm );
}

/*!
 * \brief   Prepares the payload of the frame
 */
static void PrepareTxFrame( uint8_t port )
{
		int i;
		switch( port )
    {
    case 88:
        {
						for(i=0; i<AppTxDataSize; i++)
							AppTxDataBuffer[i] = '0' + i;
        }
        break;
    default:
        break;
    }
}

/*!
 * \brief   Construct the frame
 */
static void SendFrame( void )
{
		uint8_t i;
		uint8_t pktHeaderLen = 0;
		uint8_t mhdr = 0;
		uint8_t fctrl = 0;
		uint32_t mic = 0x12345678;
	
		//prepare mac frame
		// | MHDR | MACPayload | MIC
		mhdr = MTYPE_UNCONFIRMED_DATA_UP_MASK | MAJOR_VERSION_LORAWAN;
	
		LoRaMacBuffer[pktHeaderLen++] = mhdr;
	
		// Pack the MACPayload
    // MACPayload: | FHDR | FPort | FRMPayload
    // FHDR: | DevAddr | FCtrl | FCnt | FOpts
	
		LoRaMacBuffer[pktHeaderLen++] = ( DevAddr ) & 0xFF;
		LoRaMacBuffer[pktHeaderLen++] = ( DevAddr >> 8 ) & 0xFF;
		LoRaMacBuffer[pktHeaderLen++] = ( DevAddr >> 16 ) & 0xFF;
		LoRaMacBuffer[pktHeaderLen++] = ( DevAddr >> 24 ) & 0xFF;
	
		LoRaMacBuffer[pktHeaderLen++] = fctrl;
		
		UpLinkCounter++;
		LoRaMacBuffer[pktHeaderLen++] = UpLinkCounter & 0xFF;
		LoRaMacBuffer[pktHeaderLen++] = ( UpLinkCounter >> 8 ) & 0xFF;
		
		LoRaMacBuffer[pktHeaderLen++] = AppPort;
		
		for(i=0; i<AppTxDataSize; i++)
			LoRaMacBuffer[pktHeaderLen++] = AppTxDataBuffer[i];
		
		LoRaMacBuffer[pktHeaderLen++] = mic & 0xFF;
		LoRaMacBuffer[pktHeaderLen++] = ( mic >> 8 ) & 0xFF;
		LoRaMacBuffer[pktHeaderLen++] = ( mic >> 16 ) & 0xFF;
		LoRaMacBuffer[pktHeaderLen++] = ( mic >> 24 ) & 0xFF;
		
		// set tx config
		Radio.SetChannel( channel[7] );
		Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
											 LORA_SPREADING_FACTOR, LORA_CODINGRATE,
											 LORA_PREAMBLE_LENGTH, false,
											 true, 0, 0, false, 3000 );

		if( TxRadioParamsPrinted == false )
		{
				PrintLoRaTxRadioParams( );
				TxRadioParamsPrinted = true;
		}
		
		Radio.Send( LoRaMacBuffer, pktHeaderLen );
		
		GpioWrite(&Led3, 0);// led on
}


/**
 * Main application entry point.
 */
int main( void )
{
		// Target board initialization
		BoardInitMcu( );
    BoardInitPeriph( );
	
    DeviceState = DEVICE_STATE_INIT;

    printf( "###### ===== ClassA demo application v1.0.RC1 ==== ######\r\n\r\n" );
		
    while( 1 )
    {
        switch( DeviceState )
        {
						case DEVICE_STATE_INIT:
						{
								// Initialize timers
								TimerInit( &RxWindowTimer1, OnRxWnd1TimerEvent );
								TimerInit( &RxWindowTimer2, OnRxWnd2TimerEvent );
							
								TimerInit( &TxNextPacketTimer, OnTxNextPacketTimerEvent );
							
								// Initialize Radio driver
								RadioEvents.TxDone = OnTxDone;
								RadioEvents.RxDone = OnRxDone;
								RadioEvents.TxTimeout = OnTxTimeout;
								RadioEvents.RxTimeout = OnRxTimeout;
								RadioEvents.RxError = OnRxError;
								Radio.Init( &RadioEvents );
							
								// Random seed initialization
								srand1( BoardGetRandomSeed( ) );
							
								Radio.SetPublicNetwork( PublicNetwork );
							
								Radio.Sleep( );
							
								DeviceState = DEVICE_STATE_SEND;
							
								break;
						}
						case DEVICE_STATE_SEND:
						{
								// Send the packet
								PrepareTxFrame(AppPort);
								SendFrame( );
								
								TxDutyCycleTime = APP_TX_DUTYCYCLE;
								
								DeviceState = DEVICE_STATE_CYCLE;
								
								break;
						}
						case DEVICE_STATE_CYCLE:
						{
								// Schedule next packet transmission
                TimerSetValue( &TxNextPacketTimer, TxDutyCycleTime );
                TimerStart( &TxNextPacketTimer );
							
								DeviceState = DEVICE_STATE_SLEEP;
							
								break;
						}
						case DEVICE_STATE_SLEEP:
						{
								break;
						}
						default:
						{
                DeviceState = DEVICE_STATE_SEND;
                break;
            }
						
				}
				
    }
}
