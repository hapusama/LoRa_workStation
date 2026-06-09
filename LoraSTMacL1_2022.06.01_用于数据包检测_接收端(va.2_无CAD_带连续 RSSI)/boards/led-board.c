#include "led-board.h"

Gpio_t Led1;
Gpio_t Led2;
Gpio_t Led3;

void led_init(void)
{
	GpioInit(&Led1, LED_1, PIN_OUTPUT, PIN_PUSH_PULL, PIN_PULL_UP, 1);
	GpioInit(&Led2, LED_2, PIN_OUTPUT, PIN_PUSH_PULL, PIN_PULL_UP, 1);
	GpioInit(&Led3, LED_3, PIN_OUTPUT, PIN_PUSH_PULL, PIN_PULL_UP, 1);
}

int get_led_stats(LedColor_t color)//1 measn on; 0 means off
{
	int status = 0;
	switch(color)
	{
		case LED_GREEN:
			status = GpioRead(&Led1);
			break;
		case LED_BLUE:
			status = GpioRead(&Led2);
			break;
		case LED_WHITE:
			status = GpioRead(&Led3);
			break;
		default:
			status = 0;
	}
	return status!=1 ? 1 : 0;
}
