#ifndef __LED_H__
#define __LED_H__

#include "gpio.h"
#include "board-config.h"

typedef enum eLedColor
{
	LED_GREEN,
	LED_BLUE,
	LED_WHITE
}LedColor_t;

extern Gpio_t Led1;
extern Gpio_t Led2;
extern Gpio_t Led3;

#define LED_GREEN_OFF GpioWrite(&Led1, 1)	//ÂÌµÆ<----->PA1
#define LED_GREEN_ON  GpioWrite(&Led1, 0)

#define LED_BLUE_OFF GpioWrite(&Led2, 1)		//À¶µÆ<----->PA2
#define LED_BLUE_ON  GpioWrite(&Led2, 0)

#define LED_WHITE_OFF GpioWrite(&Led3, 1)		//ºìµÆ<----->PA3
#define LED_WHITE_ON  GpioWrite(&Led3, 0)

void led_init(void);
int get_led_stats(LedColor_t color);


#endif //__LED_H__
