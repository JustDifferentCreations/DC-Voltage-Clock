#ifndef DS3231_H_
#define DS3231_H_

#include <stdint.h>
#include <stdbool.h>
#include <float.h>

typedef struct {
    uint8_t hh, mm;
} Time_t;

void RTC_init();
bool RTC_lost_power();
bool RTC_set_time(Time_t time);
Time_t RTC_get_time();
float RTC_get_temperature();

#endif