#include <stdlib.h>
#include <q40types.h>
#include "q40isa.h"
#include "q40hw.h"

unsigned int ram_size = 0;
extern volatile uint32_t timer_ticks;

void q40_rtc_init(void)
{
    q40_rtc_data_t data_prev, data;
    q40_rtc_read_clock(&data_prev);
    memcpy(&data, &data_prev, sizeof(data));
    /* clear must-be-zero bits in timekeeper registers */
    data.month   &= 0x1F;
    data.day     &= 0x3F;
    data.weekday &= 0x07;
    data.hour    &= 0x3F;
    data.minute  &= 0x7F;
    data.second  &= 0x7F; /* clears the STOP bit, oscillator runs */
    /* write back only if we changed anything */
    if(memcmp(&data, &data_prev, sizeof(data)))
        q40_rtc_write_clock(&data);
}

uint8_t q40_rtc_read_nvram(int offset)
{
    if(offset >= 0 && offset < Q40_RTC_NVRAM_SIZE)
        return *Q40_RTC_NVRAM(offset);
    return 0xff;
}

void q40_rtc_write_nvram(int offset, uint8_t value)
{
    if(offset >= 0 && offset < Q40_RTC_NVRAM_SIZE)
        *Q40_RTC_NVRAM(offset) = value;
}

uint8_t q40_rtc_read_control(void)
{
    return *Q40_RTC_REGISTER(0);
}

void q40_rtc_write_control(uint8_t value)
{
    *Q40_RTC_REGISTER(0) = value;
}

void q40_rtc_read_clock(q40_rtc_data_t *buffer)
{
    uint8_t ctrl = q40_rtc_read_control();
    q40_rtc_write_control(ctrl | 0x40); /* set READ bit */
    buffer->second  = *Q40_RTC_REGISTER(1);
    buffer->minute  = *Q40_RTC_REGISTER(2);
    buffer->hour    = *Q40_RTC_REGISTER(3);
    buffer->weekday = *Q40_RTC_REGISTER(4);
    buffer->day     = *Q40_RTC_REGISTER(5);
    buffer->month   = *Q40_RTC_REGISTER(6);
    buffer->year    = *Q40_RTC_REGISTER(7);
    q40_rtc_write_control(ctrl & ~0xC0); /* unset READ, WRITE bits */
}

void q40_rtc_write_clock(const q40_rtc_data_t *buffer)
{
    uint8_t ctrl = q40_rtc_read_control();
    q40_rtc_write_control(ctrl | 0x80); /* set WRITE bit */
    *Q40_RTC_REGISTER(1) = buffer->second;
    *Q40_RTC_REGISTER(2) = buffer->minute;
    *Q40_RTC_REGISTER(3) = buffer->hour;
    *Q40_RTC_REGISTER(4) = buffer->weekday;
    *Q40_RTC_REGISTER(5) = buffer->day;
    *Q40_RTC_REGISTER(6) = buffer->month;
    *Q40_RTC_REGISTER(7) = buffer->year;
    q40_rtc_write_control(ctrl & ~0xC0); /* unset READ, WRITE bits */
}

timer_t q40_read_timer_ticks(void)
{
    return timer_ticks; /* it's a single aligned long word, so this should be atomic? */
}

timer_t set_timer_ticks(uint32_t duration_ticks)
{
    if(duration_ticks == 0){ 
        /* round up very short durations */
        duration_ticks = 1;
    }else if(duration_ticks & 0x80000000){
        /* limit very long durations (avoid wraparound issues) */
        printf("bad timer duration %ld\n", duration_ticks);
        duration_ticks = 0x7fffffff;
    }
    return q40_read_timer_ticks() + duration_ticks;
}

bool timer_expired(timer_t timer)
{
    return ((timer - q40_read_timer_ticks()) & 0x80000000);
}

void timer_wait(timer_t timeout)
{
    while(!timer_expired(timeout));
}

void q40_setup_interrupts(void)
{
    /* configure MASTER chip */
    *q40_keyboard_interrupt_enable = 0;
    *q40_isa_interrupt_enable = 0;
    *q40_sample_interrupt_enable = 0;
    *q40_keyboard_interrupt_ack = 0xff;
    *q40_frame_interrupt_ack = 0xff;
    *q40_sample_interrupt_ack = 0xff;
    *q40_sample_rate = 0;
#if TIMER_HZ == 200
    *q40_frame_rate = 1;
#elif TIMER_HZ == 50
    *q40_frame_rate = 0;
#else
    #error Unsupported TIMER_HZ value (try 50 or 200)
#endif
    cpu_set_ipl(1);      /* enable interrupt 2 and above */
}

static void q40_delay(uint32_t count)
{
    uint32_t x;
    while(count--)
        x += *q40_interrupt_status;
}

void q40_isa_reset(void)
{
    *q40_isa_bus_reset = 0xff;
    /* assume we don't have timer interrupts yet */
    q40_delay(100000);
    *q40_isa_bus_reset = 0;
}

void q40_led(bool on)
{
    *q40_led_control = on ? 0xff : 0;
}

void q40_graphics_init(int mode)
{
    // behold my field of modes, in it there grow but four
    mode &= 3;

    // program the display controller
    *q40_display_control = mode;

    // clear entire video memory (1MB)
    memset((void*)VIDEO_RAM_BASE, 0, 1024*1024);
}

void q40_measure_ram_size(void)
{
    /* we write a longword at the end of each MB of RAM, from
       the highest possible address downwards. Then we read
       these back, in the reverse order, to confirm how much
       RAM is actually fitted. Because the lowest address we
       write to is just below 1MB we avoid stomping on any of
       our code, data or stack which is all far below 1MB. 

       WARNING: this routine needs modification for boards 
       that support >32MB RAM, where some amounts of memory 
       can result in a discontiguous address space.
    */

    #define UNIT_ADDRESS(unit) ((uint32_t*)(unit * RAM_UNIT_SIZE - sizeof(uint32_t)))
    #define UNIT_TEST_VALUE(unit) ((uint32_t)(unit | ((~unit) << 16)))

    ram_size = 0;

    for(int unit=MAX_RAM_SIZE; unit > 0; unit--)
        *UNIT_ADDRESS(unit) = UNIT_TEST_VALUE(unit);

    for(int unit=1; unit<=MAX_RAM_SIZE; unit++)
        if(*UNIT_ADDRESS(unit) == UNIT_TEST_VALUE(unit))
            ram_size = (unit * RAM_UNIT_SIZE);
        else
            break;
}
