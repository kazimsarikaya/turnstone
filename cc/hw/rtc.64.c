/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <device/rtc.h>
#include <ports.h>
#include <acpi.h>

MODULE("turnstone.kernel.hw.rtc");

#define RTC_CURRENT_YEAR        2022

#define RTC_CMOS_ADDRESS       0x70
#define RTC_CMOS_DATA          0x71

#define RTC_REGISTER_SECONDS   0x00
#define RTC_REGISTER_MINUTES   0x02
#define RTC_REGISTER_HOURS     0x04
#define RTC_REGISTER_WEEKDAY   0x06
#define RTC_REGISTER_DAY       0x07
#define RTC_REGISTER_MONTH     0x08
#define RTC_REGISTER_YEAR      0x09
#define RTC_REGISTER_STATUS_A  0x0A
#define RTC_REGISTER_STATUS_B  0x0B

uint8_t rtc_is_in_update(void);
uint8_t rtc_read_register_value(uint16_t reg);

uint8_t rtc_is_in_update(void) {
    outb(RTC_CMOS_ADDRESS, 0x0A);
    return (inb(RTC_CMOS_DATA) & 0x80);
}

uint8_t rtc_read_register_value(uint16_t reg) {
    outb(RTC_CMOS_ADDRESS, reg);
    return inb(RTC_CMOS_DATA);
}

time_t rtc_get_time(void) {

    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;

    uint8_t century = 0;
    uint8_t last_second;
    uint8_t last_minute;
    uint8_t last_hour;
    uint8_t last_day;
    uint8_t last_month;
    uint16_t last_year;
    uint8_t last_century = 0;
    uint8_t register_b;

    uint16_t century_register = ACPI_CONTEXT->fadt->century;

    while (rtc_is_in_update());

    second = rtc_read_register_value(RTC_REGISTER_SECONDS);
    minute = rtc_read_register_value(RTC_REGISTER_MINUTES);
    hour = rtc_read_register_value(RTC_REGISTER_HOURS);
    day = rtc_read_register_value(RTC_REGISTER_DAY);
    month = rtc_read_register_value(RTC_REGISTER_MONTH);
    year = rtc_read_register_value(RTC_REGISTER_YEAR);

    if(century_register != 0) {
        century = rtc_read_register_value(century_register);
    }

    do {
        last_second = second;
        last_minute = minute;
        last_hour = hour;
        last_day = day;
        last_month = month;
        last_year = year;
        last_century = century;

        while (rtc_is_in_update());

        second = rtc_read_register_value(RTC_REGISTER_SECONDS);
        minute = rtc_read_register_value(RTC_REGISTER_MINUTES);
        hour = rtc_read_register_value(RTC_REGISTER_HOURS);
        day = rtc_read_register_value(RTC_REGISTER_DAY);
        month = rtc_read_register_value(RTC_REGISTER_MONTH);
        year = rtc_read_register_value(RTC_REGISTER_YEAR);

        if(century_register != 0) {
            century = rtc_read_register_value(century_register);
        }

    } while( (last_second != second) || (last_minute != minute) || (last_hour != hour) ||
             (last_day != day) || (last_month != month) || (last_year != year) ||
             (last_century != century) );

    register_b = rtc_read_register_value(RTC_REGISTER_STATUS_B);

    if (!(register_b & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
        day = (day & 0x0F) + ((day / 16) * 10);
        month = (month & 0x0F) + ((month / 16) * 10);
        year = (year & 0x0F) + ((year / 16) * 10);

        if(century_register != 0) {
            century = (century & 0x0F) + ((century / 16) * 10);
        }
    }

    if (!(register_b & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    if(century_register != 0) {
        year += century * 100;
    } else {
        year += (RTC_CURRENT_YEAR / 100) * 100;

        if(year < RTC_CURRENT_YEAR) {
            year += 100;
        }
    }

    timeparsed_t tp = {year, month, day, hour, minute, second};

    return timeparsed_to_time(&tp);
}
