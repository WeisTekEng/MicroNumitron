#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif



extern "C" {
//	#include <Wire/Wire.h>
	#include <Wire.h>
	#include <avr/pgmspace.h>
}

#include "DS1337.h"
//#include "programStrings.h"

// NOTE: To keep the math from getting even more lengthy/annoying than it already is, the following constraints are imposed:
//   1) All times are in 24-hour format (military time)
//   2) DayOfWeek field is not used internally or checked for validity. Alarm functions may optionally set alarms repeating on DayOfWeek, but this feature has not been tested yet.
//   3) This library's buffer stores all times in raw BCD format, just as it is sent from the RTC.
//      It is not converted to/from 'real' (binary) values until needed via get...() and set...() functions.
//      In other words, don't go hacking around and reading from the rtc_bcd[] buffer directly, unless you want the raw BCD results.


// Cumulative number of days elapsed at the start of each month, assuming a normal (non-leap) year.
unsigned int monthdays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

DS1337::DS1337()
{
	Wire.begin();
}

// Aquire data from the RTC chip in BCD format
// refresh the buffer
void DS1337::readTime(void)
{
// use the Wire lib to connect to tho rtc
// reset the register pointer to zero
	Wire.beginTransmission(DS1337_CTRL_ID);
	Wire.write(0x00);
	Wire.endTransmission();

// request the 7 bytes of data    (secs, min, hr, dow, date. mth, yr)
	Wire.requestFrom(DS1337_CTRL_ID, 7);
	for(int i=0; i<7; i++)
	{
	// store data in raw bcd format
		if (Wire.available())
			rtc_bcd[i]=Wire.read();
	}
}

// Read the current alarm value. Note that the repeat flags and DY/DT are removed from the result.
void DS1337::readAlarm(void)
{
        //alarm_repeat = 0;
        byte temp;
// use the Wire lib to connect to tho rtc
// point to start of Alarm1 registers
	Wire.beginTransmission(DS1337_CTRL_ID);
	Wire.write(DS1337_ARLM1);
	Wire.endTransmission();

// request the *4* bytes of data (secs, min, hr, dow/date). Note the format is nearly identical, except for the choice of dayOfWeek vs. date,
// and that the topmost bit of each helps determine if/how the alarm repeats.
	Wire.requestFrom(DS1337_CTRL_ID, 4);
	for(int i=0; i<4; i++)
	{
                // store data in raw bcd format
		if (Wire.available())
                        temp = Wire.read();
                        rtc_bcd[i] = temp & B01111111;
	}

	// 4th byte read may contain either a date or DayOfWeek, depending on the value of the DY/DT flag.
	// For laziness sake we read it into the DayOfWeek field regardless (rtc_bcd[3]). Correct as needed...
        if(rtc_bcd[3] & B01000000) // DY/DT set: DayOfWeek
        {
           rtc_bcd[3] &= B10111111; // clear DY/DT flag
           rtc_bcd[4] = 0; // alarm *date* undefined
        }
        else
        {
            rtc_bcd[4] = rtc_bcd[3];
            rtc_bcd[3] = 0; // alarm dayOfWeek undefined
        }
}

// update the data on the IC from the bcd formatted data in the buffer

void DS1337::writeTime(void)
{
        //byte temp;
	Wire.beginTransmission(DS1337_CTRL_ID);
	Wire.write(0x00); // reset register pointer
	for(int i=0; i<7; i++)
	{
		Wire.write(rtc_bcd[i]);
	}
	Wire.endTransmission();

	// clear the Oscillator Stop Flag
        setRegister(DS1337_STATUS, getRegister(DS1337_STATUS) & !DS1337_STATUS_OSF);
        //temp = getRegister(DS1337_STATUS);
        //temp &= (!DS1337_STATUS_OSF);
        //setRegister(DS1337_STATUS, temp);
}

void DS1337::writeTime(unsigned long sse)
{
        epoch_seconds_to_date(sse);
        writeTime();
}

// FIXME: automatically set alarm interrupt after writing new alarm? Nah...

// Write the BCD alarm value in the buffer to the alarm registers.
// If an alarm repeat mode has been specified, poke those bytes into the buffer before writeing.
void DS1337::writeAlarm(void)
{
	Wire.beginTransmission(DS1337_CTRL_ID);
	Wire.write(DS1337_ARLM1); // set register pointer

        Wire.write(rtc_bcd[DS1337_SEC] | ((alarm_repeat & B00000001 ) << 7)); // A1M1
        Wire.write(rtc_bcd[DS1337_MIN] | ((alarm_repeat & B00000010 ) << 6)); // A1M2
        Wire.write(rtc_bcd[DS1337_HR] | ((alarm_repeat & B00000100 ) << 5)); // A1M3

        // Check if we are using date or DayOfWeek and write the appropriate value
        if(alarm_repeat & B00001000) // DayOfWeek
        {
            // write DOW as 4th alarm reg byte
            Wire.write(rtc_bcd[DS1337_DOW] | ((alarm_repeat & B00011000 ) << 3)); // A1M4 and DY/DT
        }
        else // date
        {
            // write date as 4th alarm reg byte
            Wire.write(rtc_bcd[DS1337_DATE] | ((alarm_repeat & B00011000 ) << 3)); // A1M4 and DY/DT
        }

	Wire.endTransmission();
}


void DS1337::writeAlarm(unsigned long sse)
{
        epoch_seconds_to_date(sse);
        writeAlarm();
}

void DS1337::setAlarmRepeat(byte repeat)
{
        alarm_repeat = repeat;
}


// Decided to let the user implement any needed snooze function, since we don't know in advance if they are currently handling the interrupt,
// and if we optionally handle it ourselves, switching around pointers between member and user functions here will get messy...

/* void DS1337::snooze(unsigned long secondsToSnooze)
{
...
} */


unsigned char DS1337::getRegister(unsigned char registerNumber)
{
	Wire.beginTransmission(DS1337_CTRL_ID);
	Wire.write(registerNumber);
	Wire.endTransmission();

	Wire.requestFrom(DS1337_CTRL_ID, 1);

	return Wire.read();
}

void DS1337::setRegister(unsigned char registerNumber, unsigned char value)
{
	Wire.beginTransmission(DS1337_CTRL_ID);
	Wire.write(registerNumber); // set register pointer

	Wire.write(value);

	Wire.endTransmission();
}

unsigned char DS1337::time_is_set()
{
  // Return TRUE if Oscillator Stop Flag is clear (osc. not stopped since last time setting), FALSE otherwise
  byte asdf = ((getRegister(DS1337_STATUS) & DS1337_STATUS_OSF) == 0);
  return asdf;
}
unsigned char DS1337::alarm_is_set()
{
  // Return TRUE if the alarm interrupt flag is enabled.
  byte asdf = (getRegister(DS1337_SP) & DS1337_SP_A1IE);
  return asdf;
}

unsigned char DS1337::enable_interrupt()
{
   clear_interrupt();
   setRegister(DS1337_SP, getRegister(DS1337_SP) | DS1337_SP_INTCN | DS1337_SP_A1IE); // map alarm interrupt to INT1 and enable interrupt
}

unsigned char DS1337::disable_interrupt()
{
   setRegister(DS1337_SP, getRegister(DS1337_SP) & !DS1337_SP_A1IE);
}

unsigned char DS1337::clear_interrupt()
{
   setRegister(DS1337_STATUS, getRegister(DS1337_STATUS) & !DS1337_STATUS_A1F);
}

unsigned char DS1337::getSeconds()
{
    return bcd2bin(rtc_bcd[DS1337_SEC]);
}

unsigned char DS1337::getMinutes()
{
    return bcd2bin(rtc_bcd[DS1337_MIN]);
}
unsigned char DS1337::getHours()
{
    return bcd2bin(rtc_bcd[DS1337_HR]);
}
unsigned char DS1337::getDays()
{
    return bcd2bin(rtc_bcd[DS1337_DATE]);
}
unsigned char DS1337::getDayOfWeek()
{
    return bcd2bin(rtc_bcd[DS1337_DOW]);
}
unsigned char DS1337::getMonths()
{
    return bcd2bin(rtc_bcd[DS1337_MTH]);
}
unsigned int DS1337::getYears()
{
    return 2000 + bcd2bin(rtc_bcd[DS1337_YR]);
}


unsigned long DS1337::date_to_epoch_seconds(unsigned int year, byte month, byte day, byte hour, byte minute, byte second)
{

  //gracefully handle 2- and 4-digit year formats
  if (year > 1999)
  {
     year -= 2000;
  }


// Between year 2000 and 2100, a leap year occurs in every year divisible by 4.

//   sse_y = (((unsigned long)year)*365*24*60*60);
//   sse_ly = ((((unsigned long)year+3)>>2) + ((unsigned long)year%4==0 && (unsigned long)month>2))*24*60*60;
//   sse_d = ((unsigned long)monthdays[month-1] + (unsigned long)day-1) *24*60*60;
//   sse_h = ((unsigned long)hour*60*60);
//   sse_m = ((unsigned long)minute*60);
//   sse_s = (unsigned long)second;
//
//   sse = sse_y + sse_ly + sse_d + sse_h + sse_m + sse_s;



// NB: The multiplication-by-constants below is intentionally left expanded for readability; GCC is smart and will optimize them to single constants during compilation.


  //         Whole year seconds                      Cumulative total of seconds contributed by elapsed leap year days
  unsigned long sse = (((unsigned long)year)*365*24*60*60)   +   ((((unsigned long)year+3)>>2) + ((unsigned long)year%4==0 && (unsigned long)month>2))*24*60*60   +   \
         ((unsigned long)monthdays[month-1] + (unsigned long)day-1) *24*60*60   +   ((unsigned long)hour*60*60)   +   ((unsigned long)minute*60)   + (unsigned long)second;
         // Seconds in days since start of year                      hours                      minutes           sec

  return sse;
}

unsigned long DS1337::date_to_epoch_seconds()
{
     unsigned long asdf = date_to_epoch_seconds(int(bcd2bin(rtc_bcd[DS1337_YR])), bcd2bin(rtc_bcd[DS1337_MTH]), bcd2bin(rtc_bcd[DS1337_DATE]), bcd2bin(rtc_bcd[DS1337_HR]), bcd2bin(rtc_bcd[DS1337_MIN]), bcd2bin(rtc_bcd[DS1337_SEC]));
     return asdf;
}


void DS1337::epoch_seconds_to_date(unsigned long binary)
{
   // This routine taken from Dallas/Maxim application note 517
   // http://www.maxim-ic.com/app-notes/index.mvp/id/517
   // Arn't the fastest thing, but it produces correct results.
   // Slightly modified for epoch date of 1/1/2000.

   // TODO: Optimize (especially eliminate/fake as much sluggish long-division as possible); eliminate redundant variables

   unsigned long hour;
   unsigned long day;
   unsigned long minute;
   unsigned long second;
   unsigned long month;
   unsigned long year;

   unsigned long whole_minutes;
   unsigned long whole_hours;
   unsigned long whole_days;
   unsigned long whole_days_since_1968;
   unsigned long leap_year_periods;
   unsigned long days_since_current_lyear;
   unsigned long whole_years;
   unsigned long days_since_first_of_year;
   unsigned long days_to_month;
   unsigned long day_of_week;

   whole_minutes = binary / 60;
   second = binary - (60 * whole_minutes);                 // leftover seconds

   whole_hours  = whole_minutes / 60;
   minute = whole_minutes - (60 * whole_hours);            // leftover minutes

   whole_days   = whole_hours / 24;
   hour         = whole_hours - (24 * whole_days);         // leftover hours

   whole_days_since_1968 = whole_days;// + 365 + 366;
   leap_year_periods = whole_days_since_1968 / ((4 * 365) + 1);

   days_since_current_lyear = whole_days_since_1968 % ((4 * 365) + 1);

   // if days are after a current leap year then add a leap year period
   if ((days_since_current_lyear >= (31 + 29))) {
      leap_year_periods++;
   }
   whole_years = (whole_days_since_1968 - leap_year_periods) / 365;
   days_since_first_of_year = whole_days_since_1968 - (whole_years * 365) - leap_year_periods;

   if ((days_since_current_lyear <= 365) && (days_since_current_lyear >= 60)) {
      days_since_first_of_year++;
   }
   year = whole_years;// + 68;

   // walk across monthdays[] to find what month it is based on how many days have passed
   //   within the current year
   month = 13;
   days_to_month = 366;
   while (days_since_first_of_year < days_to_month) {
       month--;
       days_to_month = monthdays[month-1];
       if ((month > 2) && ((year % 4) == 0)) {
           days_to_month++;
        }
   }
   day = days_since_first_of_year - days_to_month + 1;

   day_of_week = (whole_days  + 4) % 7;


   rtc_bcd[DS1337_SEC] = bin2bcd(second);
   rtc_bcd[DS1337_MIN] = bin2bcd(minute);
   rtc_bcd[DS1337_HR] = bin2bcd(hour);
   rtc_bcd[DS1337_DATE] = bin2bcd(day);
   rtc_bcd[DS1337_DOW] = bin2bcd(day_of_week);
   rtc_bcd[DS1337_MTH] = bin2bcd(month);
   rtc_bcd[DS1337_YR] = bin2bcd(year);
}


void DS1337::setSeconds(unsigned char v)
{
    rtc_bcd[DS1337_SEC] = bin2bcd(v);

}
void DS1337::setMinutes(unsigned char v)
{
    rtc_bcd[DS1337_MIN] = bin2bcd(v);

}
void DS1337::setHours(unsigned char v)
{
    rtc_bcd[DS1337_HR] = bin2bcd(v);

}
void DS1337::setDays(unsigned char v)
{
    rtc_bcd[DS1337_DATE] = bin2bcd(v);

}
void DS1337::setDayOfWeek(unsigned char v)
{
    rtc_bcd[DS1337_DOW] = bin2bcd(v);

}
void DS1337::setMonths(unsigned char v)
{
    rtc_bcd[DS1337_MTH] = bin2bcd(v);

}
void DS1337::setYears(unsigned int v)
{
    if (v>1999)
    {
        v -= 2000;
    }
    rtc_bcd[DS1337_YR] = bin2bcd(v);

}

byte DS1337::bcd2bin(byte v)
{
   return (v&0x0F) + ((v>>4)*10);
}

byte DS1337::bin2bcd(byte v)
{
   return ((v / 10)<<4) + (v % 10);
}

void DS1337::stop(void)
{
	setRegister(DS1337_SP, getRegister(DS1337_SP) | DS1337_SP_EOSC);
}

void DS1337::start(void)
{
	setRegister(DS1337_SP, getRegister(DS1337_SP) & !DS1337_SP_EOSC);
}
