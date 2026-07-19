#include "stm32h7xx.h"

/*
 * This variable gives the debugger an easy way to confirm that the CPU is
 * executing. It continuously increments after reset.
 */
volatile uint32_t g_heartbeat = 0U;

int main(void)
{
    SystemCoreClockUpdate();

    for (;;)
    {
        ++g_heartbeat;
        __NOP();
    }
}
