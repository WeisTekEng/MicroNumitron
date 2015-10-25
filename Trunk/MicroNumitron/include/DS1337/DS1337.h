/*
  DS1337.h - library for DS1337 rtc
*/

// ensure this library description is only included once
#ifndef DS1337_h
#define DS1337_h

// include types & constants of Wiring core API
#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

// include types & constants of Wire ic2 lib


// indices within the rtc_bcd[] buffer
#define DS1337_SEC	0
#define DS1337_MIN	1
#define DS1337_HR	2
#define DS1337_DOW	3
#define DS1337_DATE     4
#define DS1337_MTH	5
#define DS1337_YR	6

#define DS1337_BASE_YR		2000

#define DS1337_CTRL_ID		B1101000

 // Define register bit masks
#define DS1337_CLOCKHALT	B10000000

#define DS1337_LO_BCD		B00001111
#define DS1337_HI_BCD		B11110000

#define DS1337_HI_SEC		B01110000
#define DS1337_HI_MIN		B01110000
#define DS1337_HI_HR		B00110000
#define DS1337_LO_DOW		B00000111
#define DS1337_HI_DATE		B00110000
#define DS1337_HI_MTH		B00110000
#define DS1337_HI_YR		B11110000

#define DS1337_ARLM1		0x07
#define DS1337_ARLM1_LO_SEC	B00001111
#define DS1337_ARLM1_HI_SEC	B01110000
#define DS1337_ARLM1_LO_MIN	B01110000
#define DS1337_ARLM1_HI_MIN	B00001111

#define DS1337_SP			0x0E
#define	DS1337_SP_EOSC		B10000000
#define	DS1337_SP_RS2		B00010000
#define	DS1337_SP_RS1		B00001000
#define	DS1337_SP_INTCN		B00000100
#define	DS1337_SP_A2IE		B00000010
#define	DS1337_SP_A1IE		B00000001

#define DS1337_STATUS		0x0F
#define DS1337_STATUS_OSF	B10000000
#define DS1337_STATUS_A2F	B00000010
#define DS1337_STATUS_A1F	B00000001

/* Definitions for alarm repeat */
/* The private variable alarm_repeat holds the user's alarm repeat preference. However, the DS1337 encodes these in the topmost bit(s) of the 4 alarm registers. */
/* Splattering these bits across the alarm regs is handled in the writeAlarm() function. */
/* If DY/DT is set, the day field is interpreted as a DayOfWeek (1 ~ 7), else it is interpreted as a DayOfMonth.*/

/* user alarm_repeat bit mask:
       7   6   5    4       3      2       1     0
      [x   x   x   A1M4   DY/DT   A1M3   A1M2   A1M1]
*/

#define EVERY_SECOND       B00010111
#define EVERY_MINUTE       B00010110
#define EVERY_HOUR         B00010100
#define EVERY_DAY          B00010000
#define EVERY_WEEK         B00001000
#define EVERY_MONTH        B00000000


/* typedef struct {
  unsigned int year;
  unsigned char month;
  unsigned char day;
  unsigned char dayOfWeek;
  unsigned char hour;
  unsigned char minute;
  unsigned char second;
} TIME; */

// library interface description
class DS1337
{
	// user-accessible "public" interface
	public:
		DS1337();

		unsigned char time_is_set();
		unsigned char alarm_is_set();
		//unsigned char time_is_valid();

		unsigned char enable_interrupt();
		unsigned char disable_interrupt();
		unsigned char clear_interrupt();

		void    readTime();
		void    readAlarm();
		void    writeTime();
                void    writeTime(unsigned long);
		void    writeAlarm();
                void    writeAlarm(unsigned long);
                void    setAlarmRepeat(byte repeat);


		//void	getTime(int *);
                unsigned char getSeconds();
                unsigned char getMinutes();
                unsigned char getHours();
                unsigned char getDays();
                unsigned char getDayOfWeek();
                unsigned char getMonths();
                unsigned int getYears();
                unsigned long date_to_epoch_seconds(unsigned int year, byte month, byte day, byte hour, byte minute, byte second);
                unsigned long date_to_epoch_seconds();
                void epoch_seconds_to_date(unsigned long);
                //void snooze(unsigned long secondsToSnooze);
                //unsigned long date2seconds();

                //void writeTime(int *);

                void setSeconds(unsigned char);
                void setMinutes(unsigned char);
                void setHours(unsigned char);
                void setDays(unsigned char);
                void setDayOfWeek(unsigned char);
                void setMonths(unsigned char);
                void setYears(unsigned int);


		void	start(void);
		void	stop(void);
		unsigned char getRegister(unsigned char registerNumber);
		void	setRegister(unsigned char registerNumber, unsigned char registerValue);
		//void	unsetRegister(unsigned char registerNumber, unsigned char registerMask);

	// library-accessible "private" interface
	private:
	        byte    time_set;
	        byte alarm_repeat;
		byte	rtc_bcd[7]; // used prior to read/set DS1337 registers;
		void	read(void);
		void	save(void);
		byte bcd2bin(byte);
		byte bin2bcd(byte);
};

//extern DS1337 RTC;

#endif
