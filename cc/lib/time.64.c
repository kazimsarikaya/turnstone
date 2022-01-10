#include <time.h>
#include <memory.h>


uint64_t TIME_EPOCH = 0;

#if ___TESTMODE != 1

time_t time(time_t* t){
	if(t) {
		*t = (time_t)(TIME_EPOCH / 1000000);
		return *t;
	}

	return (time_t)(TIME_EPOCH / 1000000);
}

time_t time_ns(time_t* t) {
	if(t) {
		*t = (time_t)(TIME_EPOCH);
		return *t;
	}

	return (time_t)(TIME_EPOCH);
}

#endif

timeparsed_t* parse_time(timeparsed_t* tp, time_t t);

int32_t time_days_of_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

boolean_t time_is_leap(int64_t year) {
	return year % 400 == 0 || (year % 4 == 0  && year % 100 != 0);
}

timeparsed_t* time_to_timeparsed(time_t t) {
	timeparsed_t* tp = memory_malloc(sizeof(timeparsed_t));

	return parse_time(tp, t);
}

timeparsed_t* timeparsed(timeparsed_t* tp) {
	time_t t;
	time(&t);

	return parse_time(tp, t);
}

timeparsed_t* parse_time(timeparsed_t* tp, time_t t) {
	int64_t days_till_now, extra_time, extra_days, index, flag = 0;

	days_till_now = t / TIME_SECONDS_OF_DAY;
	extra_time = t % TIME_SECONDS_OF_DAY;

	tp->year = TIME_TIMESTAMP_START_YEAR;

	while (days_till_now >= TIME_DAYS_AT_YEAR) {
		if (time_is_leap(tp->year)) {
			days_till_now -= TIME_DAYS_AT_LEAP_YEAR;
		}
		else {
			days_till_now -= TIME_DAYS_AT_YEAR;
		}
		tp->year += 1;
	}

	extra_days = days_till_now + 1;

	if (time_is_leap(tp->year)) {
		flag = 1;
	}

	tp->month = 0, index = 0;
	if (flag == 1) {
		while (1) {

			if (index == 1) {
				if (extra_days - 29 <= 0)
					break;
				tp->month += 1;
				extra_days -= 29;
			}
			else {
				if (extra_days - time_days_of_month[index] <= 0) {
					break;
				}
				tp->month += 1;
				extra_days -= time_days_of_month[index];
			}
			index += 1;
		}
	}
	else {
		while (1) {

			if (extra_days  - time_days_of_month[index] <= 0) {
				break;
			}
			tp->month += 1;
			extra_days -= time_days_of_month[index];
			index += 1;
		}
	}

	if (extra_days > 0) {
		tp->month += 1;
		tp->day = extra_days;
	}
	else {
		if (tp->month == 2 && flag == 1)
			tp->day = 29;
		else {
			tp->day = time_days_of_month[tp->month - 1];
		}
	}

	tp->hours = extra_time / TIME_SECONDS_OF_HOUR;
	tp->minutes = (extra_time % TIME_SECONDS_OF_HOUR) / TIME_SECONDS_OF_MINUTE;
	tp->seconds = (extra_time % TIME_SECONDS_OF_HOUR) % TIME_SECONDS_OF_MINUTE;

	return tp;
}

time_t timeparsed_to_time(timeparsed_t* tp) {
	time_t t;

	t = tp->seconds;
	t += tp->minutes * TIME_SECONDS_OF_MINUTE;
	t += tp->hours * TIME_SECONDS_OF_HOUR;

	int64_t days = tp->day - 1;

	for(int16_t i = 0; i < tp->month - 1; i++) {
		days += time_days_of_month[i];
	}

	int64_t year = tp->year;

	if(time_is_leap(year)) {
		days++;
	}

	year--;

	while(year >= TIME_TIMESTAMP_START_YEAR) {
		if(time_is_leap(year)) {
			days += TIME_DAYS_AT_LEAP_YEAR;
		} else {
			days += TIME_DAYS_AT_YEAR;
		}

		year--;
	}

	t += days * TIME_SECONDS_OF_DAY;

	return t;
}
