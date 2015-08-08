#include <asm/delay.h>
#include <asm/machine.h>

void udelay(unsigned long usecs)
{
    if (usecs >= 1000)
        mdelay(usecs / 1000);
    usecs %= 1000;

    for (volatile int i = 0; i < usecs * LOOP_PER_USEC; i++)
        asm volatile("nop");
}

void mdelay(unsigned long msecs)
{
    for (volatile int i = 0; i < msecs * LOOP_PER_MSEC; i++)
        asm volatile("nop");
}
