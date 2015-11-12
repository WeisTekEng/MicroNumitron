/*
 * NumiClockMicro
 *
 * Created: 9/29/2015 8:28:38 PM
 * Author: Weistek Engineering (Jeremy G.)
 * Micro:	AtMega 168A
 * Notes:	This code is a stripped down version of its bigger brother.
 *			This micro only has 16KB of program space instead of 32KB
 *			thus the reduced functionality. This code in its current
 *			state may or may not compile properly.
 *
 *			Micro is running at 1Mhz internal crystal, this slows things
 *			way down instead of waiting 
 *
 * License:	GNU GENERAL PUBLIC LICENSE Version 3 or newer.
 */ 

//Fuse settings
/*

*/

//Includes
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include "EEPROM.h"				//Store persistent variables in eeprom.
#include <DS1337.h>
#include <avr/power.h>
#include <avr/sleep.h>

//Defines

//DS1337 defines do not mess with.
#define DS1337_ADDR  					0x68
#define DS1337_SQW_MASK					(0x03 << 2)
#define DS1337_SQW_1HZ        			(0x00 << 2)
#define DS1337_REG_CONTROL              0x0E
#define READ_ERROR  					5
#define DS1337_SQW_A2         			(0x01 << 2)
#define DS1337_SQW_32768HZ    			(0x03 << 2)
#define DS1337_SQW_8192HZ     			(0x02 << 2)
#define DS1337_SQW_4096HZ     			(0x01 << 2)
#define DS1337_SQW_1HZ        			(0x00 << 2)

//#define I2C_PULL_UP			//used to activate or deactivate I2C pull ups.
#define SCL_PORT    PORTC		// pin assignments specific to the ATmega328p
#define SCL_BIT     PC5
#define SDA_PORT    PORTC
#define SDA_BIT     PC4

//Pin definitions.
int INTERUPT_0 = 2;             //This is the PPS signal from the RTC.
int Select_BTN = 9;
int Next_BTN = 10;
int DATA_PIN = 5;           //Serial Data input
int LATCH = 6;              // Shift Register clock input
int CLOCK = 7;              // Storage register clock input
int LM35 = A1;              //ADC used for the Temp sensor LM35

//Global variables
int PPS_TIMER = 15;         //How often we want to update the system in seconds
boolean tempDisplay = false;//true;      // used to set whether the display shows temperature
boolean tempCORF = false;	     // false for C and true for F.
float Temperature_C = 0;        //our temp in C
float Temperature = 0;          //value read from temp sensor
int AM_PM = 0;					//0 = AM 1 = PM
int SECONDS_LAST = 0;
int count = 0;
int count_internal = 0;
bool BTN_TIME = false;			//this tells us if we want to press a button to display time. (saves battery.)
//clock settings trimmings.
int BTN_DELAY = 5;
int DIGIT_DELAY = 20;
int debounce=50;
//alarm settings.
int alarmHH = 0;
int alarmMM = 0;
int alarm_check = 0;
int alarm_Silence = 0;

//RTC_DS1307 rtc;             //Creating a new RTC object.
DS1337 RTC = DS1337();

//Char array, stores display data.
byte DATA_ARRAY[20] = { 0b00000001,//0
						0b01001111,//1
						0b00010010,//2
						0b00000110,//3
						0b01001100,//4
						0b00100100,//5
						0b00100000,//6
						0b00001111,//7
						0b00000000,//8
						0b00001100,//9
						0b01111110,//-
						0b00010001,//H
						0b00001100,//E
						0b01001101,//L
						0b01000000,//O
						0b00011100,//�
						0b01110010,//C
						0b00001000,//A
						0b00011000//P
};
int Temp_Array[5] = {0,0,0,0,0};
int EEPROM_ARRAY[6] = {0,0,0,0,0,0};
int ALARM_ARRAY[6] = {0,0,0,0,0,0};
byte MENU_ARRAY[4] = {0b00100100,//S = set time
					 0b00001000,//A = Set Alarm
					 0b00011100,//Degree symble, Show temp?
					 0b01110010};//Display time only when a butten is pressed?}

//Prototypes.
void(* resetFunc)(void)=0; //declare reset function at address 0
void setup();
void loop();
void gotoSleep(int _delay);
void Interrupt_Update();
void Init();
void Internal_Update();
void Update_Display();
float readTemp (int internal);
int splitInt(int pos, int value);
int convertTime(int value);
void BlankDisplay(int nap);
void clockSettings();
void alarmEEPROM(byte hour, byte minute, int AP,int AT);
int checkAlarm(int valuex, int valuey);
void menu();
void soundAlarm();
void setTime();
void setALARM();



//DS1337 control functions
uint8_t ds1337_set_control_bits(uint8_t mask);
uint8_t ds1337_set_control(uint8_t ctrl);
uint8_t i2c_write(uint8_t addr, uint8_t* buf, uint8_t num);
uint8_t ds1337_get_control(uint8_t* ctrl);
uint8_t i2c_write_1(uint8_t addr, uint8_t b);
uint8_t i2c_read(uint8_t addr, uint8_t* buf, uint8_t num);
uint8_t ds1337_clear_control_bits(uint8_t mask);

void setup()
{
		
	//Initialize the clock
	Init();
	
	//clear display at start up.
	BlankDisplay(1);
}

void loop()
{
	RTC.readTime();
	if(RTC.getSeconds() < PPS_TIMER){SECONDS_LAST = 0;}
	
	//checking if the alarm has been
	//triggered.
	int HR = RTC.getHours();
	int MIN = RTC.getMinutes();
	convertTime(HR);
	
	if(checkAlarm(HR,MIN) == 1)
	{
		soundAlarm();
	}
	
	if (digitalRead(Select_BTN)==HIGH)
	{
		//setTime();
		menu();
	}
	
	if(BTN_TIME == true)
	{
		if(digitalRead(Next_BTN) == HIGH){
			Update_Display();
		}
	}
	else
	{	
		//update the RTC buffers
		if(RTC.getSeconds() - SECONDS_LAST >= PPS_TIMER)
		{
			//Internal_Update();
		
			//Update the VFD display
			Update_Display();
			SECONDS_LAST = RTC.getSeconds();
		}
	}
}

void gotoSleep(int _delay)
{
	sleep_enable();
    byte adcsraSave = ADCSRA;
    ADCSRA &= ~ bit(ADEN); // disable the ADC
    set_sleep_mode(SLEEP_MODE_STANDBY);
    sleep_cpu();
    sleep_disable();
    // re-enable what we disabled
    ADCSRA = adcsraSave;
	delay(_delay);	
}


void Interrupt_Update(){} //This is just to attach an interrupt

void Init()
{
	///////////////////////////////////////////////////////////
	//This is where we initialize everything.
	///////////////////////////////////////////////////////////
	
	//Coms are not enabled on this clock.
	
	Wire.begin();
	
	#ifdef I2C_PULL_UP
	//force TWI pull ups just in case.
	//assuming the TWI library or "wire" transfers at a rate
	//of 100Khz 4.7k ohm to 10K ohm resistors are suggested as
	//pullups on the SDA(), and SCL() pins.
	
	SCL_PORT |= _BV(SCL_BIT);   // enable pull up on TWI clock line
	SDA_PORT |= _BV(SDA_BIT);   // enable pull up on TWI data line
	#else
	SCL_PORT &= _BV(SCL_BIT);   // disable pull up on TWI clock line
	SDA_PORT &= _BV(SDA_BIT);   // disable pull up on TWI data line
	#endif
	
	RTC.start(); //starts the DS1337
	///////////////////////////////////////////////////////////
	//Pin Functions
	///////////////////////////////////////////////////////////

	//pinMode(Freq_Adj, INPUT);
	pinMode(INTERUPT_0, INPUT);
	pinMode(DATA_PIN, OUTPUT);
	pinMode(CLOCK, OUTPUT);
	pinMode(LATCH, OUTPUT);
	pinMode(Select_BTN, INPUT);
	//digitalWrite(Select_BTN, LOW);
	pinMode(Next_BTN, INPUT);
	//digitalWrite(Next_BTN, LOW);
	pinMode(LM35, INPUT);
	
	///////////////////////////////////////////////////////////
	//If the RTC is not currently set to the correct time set the time
	//based on the time used when compiling this firmware.
	///////////////////////////////////////////////////////////
	
	if(EEPROM.read(10) == 0){
		if(!RTC.time_is_set())
		{
			DateTime now = DateTime(F(__DATE__),F(__TIME__));
			/*
			Serial.print(F("RTC is NOT running!, Setting to "));
			Serial.print(now.day(),DEC);
			Serial.print("/");
			Serial.print(now.month(),DEC);
			Serial.print("/");
			Serial.print(now.year(),DEC);
			Serial.print(" ");
			Serial.print(now.hour(),DEC);
			Serial.print(":");
			Serial.print(now.minute(),DEC);
			Serial.print(":");
			Serial.print(now.second(),DEC);
			Serial.print("\r\n");
			*/
			//setting to build header time.
			RTC.setSeconds(now.second());
			RTC.setMinutes(now.minute());
			RTC.setHours(now.hour());
			RTC.setDays(now.day());
			RTC.setMonths(now.month());
			RTC.setYears(now.year());
			RTC.writeTime();
		}
	}
	
	EEPROM.write(10,1);
	
	delay(10);
	
	if(!RTC.time_is_set())
	{
		Serial.print(F("Time did not set correctly, check wiring.\r\n"));
	}
	else
	{
		Serial.print(F("Time should be set.\r\n"));
	}
	
	///////////////////////////////////////////////////////////
	//This is setting the RTC Square wave pin output to 1Hz
	//We will be using this as our interrupt to update the time.

	//INTA alarm 1 will be set to 500ms cycles to set the delay
	//between display digits.
	
	ds1337_clear_control_bits(DS1337_SQW_MASK);
	ds1337_set_control(DS1337_SQW_1HZ);
	delay(10);
	///////////////////////////////////////////////////////////
	
	// Setting up sleep mode.
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();
	
	//allow interrupts
	sei();
	
	//Interrupts
	attachInterrupt(0, Interrupt_Update, RISING);
	
	//load clock settings from eeprom.
	clockSettings();
}

#if 0
void Internal_Update()
{
	///////////////////////////////////////////////////////////
	//This function internally updates everything.
	///////////////////////////////////////////////////////////
	int HR = 0;
	
	//DateTime now = rtc.now();
	RTC.readTime();
	
	//This is mostly for debug purposes.
	Serial.print(F(" Time/Date: "));
	Serial.print(RTC.getYears(), DEC);
	Serial.print('/');
	Serial.print(RTC.getMonths(), DEC);
	Serial.print('/');
	Serial.print(RTC.getDays(), DEC);
	Serial.print(' ');
	//Converting from 24hr to 12hr time.
	HR = RTC.getHours();
	
	//This tells us if its AM or PM
	//clock defaults to AM when first
	//started until stored in eeprom.
	if((HR <= 23) && (HR > 11))
	{
		AM_PM = 1;
	}
	else
	{
		AM_PM = 0;
	}
	
	//convert 24hr clock to 12hr clock
	HR = convertTime(HR);
	
	Serial.print(HR, DEC);
	Serial.print(':');
	//Serial.print(now.minute(), DEC);
	Serial.print(RTC.getMinutes(), DEC);
	Serial.print(':');
	//Serial.print(now.second(), DEC);
	Serial.print(RTC.getSeconds(), DEC);
	Serial.print(" ");
	if(AM_PM == 0)
	{
		Serial.print(F("AM"));
	}
	else{Serial.print(F("PM"));}
	
	//outputting temp.
	Serial.print(" ");
	//Temperature_C = readTemp(0);
	Serial.print(readTemp(0));
	//Serial.print(Temperature_C);
	Serial.print(F("°C "));
	/*
	if(checkAlarm(HR,now.minute()) == 1)
	{
		soundAlarm();
	}
*/
	//Clearing the Terminal screen
	//This is hacked.
	//Serial.write(27);
	//Serial.print("[H");
	Serial.print(count_internal);
	count_internal +=1;
	Serial.print("\r\n");
}
#endif

void Update_Display()
{
	///////////////////////////////////////////////////////////
	//This function updates the display on the VFD Tube.
	///////////////////////////////////////////////////////////
	//DateTime now = rtc.now();
	RTC.readTime();
	
	int SLEEP_DELAY = 2;
	int HR = 0;
	int MN = 0;

	//MN = now.minute();
	MN = RTC.getMinutes();
	//HR = now.hour();
	HR = RTC.getHours();
	
	//Converting 24HR time to 12HR format.
	HR = convertTime(HR);
	
	//display hours first tens then one's position.
	Temp_Array[2] = splitInt(0,HR);
	for(int x = 0; x<=1;x++)
	{
		if(Temp_Array[2] == splitInt(1,HR)){BlankDisplay(1);}
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[splitInt(x,HR)]);
		digitalWrite(LATCH, HIGH);
		//delay(TIME_DELAY);
		gotoSleep(SLEEP_DELAY);
	}
	
	//Dash between HH:MM to separate Hours and minutes.
	digitalWrite(LATCH, LOW);
	shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[10]);
	digitalWrite(LATCH, HIGH);
	//delay(TIME_DELAY);
	gotoSleep(SLEEP_DELAY);

	//display minutes first tens then one's position
	Temp_Array[3] = splitInt(0,MN);
	for(int y = 0;y<=1;y++)
	{
		if(Temp_Array[3] == splitInt(1,MN)){BlankDisplay(1);}
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[splitInt(y,MN)]);
		digitalWrite(LATCH, HIGH);
		//delay(TIME_DELAY);
		gotoSleep(SLEEP_DELAY);
	}

	//Blank display
	BlankDisplay(1);
	
	if(tempDisplay == true)
	{
		//Display Temperature.
		Temp_Array[4] = splitInt(0,readTemp(1));
		for (int z = 0; z <= 1; z++)
		{
			if(Temp_Array[4] == splitInt(1,readTemp(1))){BlankDisplay(1);}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[splitInt(z,readTemp(1))]);
			digitalWrite(LATCH, HIGH);
			//delay(TIME_DELAY);
			gotoSleep(SLEEP_DELAY);
		}

		//display a degree symbol
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[15]);
		digitalWrite(LATCH, HIGH);
		//delay(TIME_DELAY);
		gotoSleep(SLEEP_DELAY);
		
		BlankDisplay(1);
		if(tempCORF == false)
		{
			//display a Celsius (c) symbol
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[16]);
			digitalWrite(LATCH, HIGH);
			//delay(TIME_DELAY);
			gotoSleep(SLEEP_DELAY);

			BlankDisplay(1);
		}
		else
		{
			//display a Celsius (c) symbol
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[16]);
			digitalWrite(LATCH, HIGH);
			//delay(TIME_DELAY);
			gotoSleep(SLEEP_DELAY);

			BlankDisplay(1);
		}
	}
}

void clockSettings()
{
	//Display contents of EEPROM (Clock settings, alarm etc.).
	for(int x = 0;x<=6-1;x++)
	{
		EEPROM_ARRAY[x] = EEPROM.read(x);
		delay(1);
	}
	
	//check for AM or PM
	ALARM_ARRAY[4] = EEPROM_ARRAY[4];
}

void menu()
{
	//will implement and add more menu items later.
	/*
	byte MENU_ARRAY[4] = {0b00100100,//S = set time
						  0b00001000,//A = Set Alarm
					      0b00011100,//Degree symbol, Show temp?
						  0b01110010//Display time only when a butten is pressed?}
	*/
	bool set = false;
	int opt = 0;
	do
	{
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, MENU_ARRAY[opt]);
		digitalWrite(LATCH, HIGH);
		if (digitalRead(Select_BTN)==HIGH)
		{
			delay(debounce);
			opt++;
			if (opt>3)
			{
				opt=0;
			}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[opt]);
			digitalWrite(LATCH, HIGH);
		}
		if (digitalRead(Next_BTN)==HIGH)
		{
			delay(debounce);
			set=true;
		}
	}
	while (set!=true);
	delay(BTN_DELAY);
	
	switch(opt)
	{
		case 0:
			setTime();
			break;
		case 1:
			setALARM();
			break;
		case 2:
			if(tempDisplay == false){tempDisplay = true; PPS_TIMER = 20;}
			else{tempDisplay = false; PPS_TIMER = 15;}
			break;
		case 3:
			if(BTN_TIME == false){BTN_TIME = true;}
			else{BTN_TIME = false;}
			break;
		default:
			break;	
	}
	set = false;
	BlankDisplay(0);
	delay(BTN_DELAY);
}

void setTime()
{
	// user presses button A (digital 8) to enter 'set time' mode while time being displayed
	// then user presses A to advance value, B to lock in. Repeat for four digits of time
	
	//mini state machine
	int State = 0;
	detachInterrupt(0);
	byte minute, hour;
	int h1=0;
	int h2=0;
	int m1=0;
	int m2=0;
	boolean set=false;
	BlankDisplay(0);

	do // get first digit of hours from user
	// press digital 8 button to change from 0>1>2>0>1...
	// press digital 9 to set value
	{
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[h1]);
		digitalWrite(LATCH, HIGH);
		if (digitalRead(Select_BTN)==HIGH)
		{
			delay(debounce);
			h1++;
			if (h1>2)
			{
				h1=0;
			}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[h1]);
			digitalWrite(LATCH, HIGH);
		}
		if (digitalRead(Next_BTN)==HIGH)
		{
			delay(debounce);
			set=true;
		}
	}
	while (set!=true);
	BlankDisplay(0);
	set=false;
	delay(BTN_DELAY);
	

	do // get second digit of hours from user
	// press digital 8 button to change from 0>1>2>...9>0...
	// press digital 9 to set value
	{
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[h2]);
		digitalWrite(LATCH, HIGH);
		if (digitalRead(Select_BTN)==HIGH)
		{
			delay(debounce);
			h2++;
			if (h2>9)
			{
				h2=0;
			}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[h2]);
			digitalWrite(LATCH, HIGH);
		}
		if (digitalRead(Next_BTN)==HIGH)
		{
			delay(debounce);
			set=true;
		}
	}
	while (set!=true);
	BlankDisplay(0);
	set=false;
	delay(BTN_DELAY);

	do // get first digit of minutes from user
	// press digital 8 button to change from 0>1>..5>0..
	// press digital 9 to set value
	{
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[m1]);
		digitalWrite(LATCH, HIGH);
		if (digitalRead(Select_BTN)==HIGH)
		{
			delay(debounce);
			m1++;
			if (m1>5)
			{
				m1=0;
			}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[m1]);
			digitalWrite(LATCH, HIGH);
		}
		if (digitalRead(Next_BTN)==HIGH)
		{
			set=true;
			delay(debounce);
		}
	}
	while (set!=true);
	BlankDisplay(0);
	set=false;
	delay(BTN_DELAY);

	do // get second digit of minutes from user
	// press digital 8 button to change from 0>1>..9>0..
	// press digital 9 to set value
	{
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[m2]);
		digitalWrite(LATCH, HIGH);
		if (digitalRead(Select_BTN)==HIGH)
		{
			delay(debounce);
			m2++;
			if (m2>9)
			{
				m2=0;
			}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[m2]);
			digitalWrite(LATCH, HIGH);
		}
		if (digitalRead(Next_BTN)==HIGH)
		{
			set=true;
			delay(debounce);
		}
	}
	while (set!=true);
	BlankDisplay(0);
	set=false;
	delay(BTN_DELAY);

	// now convert the user time data to variables to write to DS1307
	hour = (h1*10)+h2;
	minute = (m1*10)+m2;
	if (hour<24 && minute <60) // in case user enters invalid time e.g. 2659h
	{

		RTC.setMinutes(minute);
		RTC.setHours(hour);
		RTC.writeTime();
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[10]);
		digitalWrite(LATCH, HIGH); // display hyphen
		delay(DIGIT_DELAY);
		BlankDisplay(0);
		delay(DIGIT_DELAY);
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[10]);
		digitalWrite(LATCH, HIGH); // display hyphen
		delay(DIGIT_DELAY);
		BlankDisplay(0);
		
		//this needs to happen, cpu will hang
		//without it.
		SECONDS_LAST = RTC.getSeconds();
		attachInterrupt(0, Interrupt_Update, RISING);
	}
}

void setALARM()
// user presses button A (digital 8) to enter 'set time' mode while time being displayed
// then user presses A to advance value, B to lock in. Repeat for four digits of time
{
	detachInterrupt(0);
	int minute, hour;
	int h1=0;
	int h2=0;
	int m1=0;
	int m2=0;
	int AP = 17;
	boolean set=false;
	BlankDisplay(0);
	
	digitalWrite(LATCH, LOW);
	shiftOut(DATA_PIN,CLOCK,LSBFIRST,DATA_ARRAY[17]);
	digitalWrite(LATCH,HIGH);
	delay(DIGIT_DELAY);
	
	BlankDisplay(0);
	
	digitalWrite(LATCH, LOW);
	shiftOut(DATA_PIN,CLOCK,LSBFIRST,DATA_ARRAY[17]);
	digitalWrite(LATCH,HIGH);
	delay(DIGIT_DELAY);
	
	BlankDisplay(0);
	
	digitalWrite(LATCH, LOW);
	shiftOut(DATA_PIN,CLOCK,LSBFIRST,DATA_ARRAY[17]);
	digitalWrite(LATCH,HIGH);
	delay(DIGIT_DELAY);
	
	BlankDisplay(0);
	

	do // get first digit of hours from user
	// press digital 8 button to change from 0>1>2>0>1...
	// press digital 9 to set value
	{
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[h1]);
		digitalWrite(LATCH, HIGH);
		if (digitalRead(Select_BTN)==HIGH)
		{
			delay(debounce);
			h1++;
			if (h1>1)
			{
				h1=0;
			}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[h1]);
			digitalWrite(LATCH, HIGH);
		}
		if (digitalRead(Next_BTN)==HIGH)
		{
			delay(debounce);
			set=true;
		}
	}
	while (set!=true);
	BlankDisplay(0);
	set=false;
	delay(BTN_DELAY);

	do // get second digit of hours from user
	// press digital 8 button to change from 0>1>2>...9>0...
	// press digital 9 to set value
	{
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[h2]);
		digitalWrite(LATCH, HIGH);
		if (digitalRead(Select_BTN)==HIGH)
		{
			delay(debounce);
			if(h1 == 0)
			{
				if(h2>=9)
				{
					h2=0;
				}
				else
				{
					h2++;
				}
			}
			else if(h1 == 1)
			{
				if(h2>=2)
				{
					h2 = 0;
				}
				else
				{
					h2++;
				}
			}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[h2]);
			digitalWrite(LATCH, HIGH);
		}
		if (digitalRead(Next_BTN)==HIGH)
		{
			delay(debounce);
			set=true;
		}
	}
	while (set!=true);
	BlankDisplay(0);
	set=false;
	delay(BTN_DELAY);

	do // get first digit of minutes from user
	// press digital 8 button to change from 0>1>..5>0..
	// press digital 9 to set value
	{
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[m1]);
		digitalWrite(LATCH, HIGH);
		if (digitalRead(Select_BTN)==HIGH)
		{
			delay(debounce);
			m1++;
			if (m1>5)
			{
				m1=0;
			}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[m1]);
			digitalWrite(LATCH, HIGH);
		}
		if (digitalRead(Next_BTN)==HIGH)
		{
			set=true;
			delay(debounce);
		}
	}
	while (set!=true);
	BlankDisplay(0);
	set=false;
	delay(BTN_DELAY);

	do // get second digit of minutes from user
	// press digital 8 button to change from 0>1>..9>0..
	// press digital 9 to set value
	{
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[m2]);
		digitalWrite(LATCH, HIGH);
		if (digitalRead(Select_BTN)==HIGH)
		{
			delay(debounce);
			m2++;
			if (m2>9)
			{
				m2=0;
			}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[m2]);
			digitalWrite(LATCH, HIGH);
		}
		if (digitalRead(Next_BTN)==HIGH)
		{
			set=true;
			delay(debounce);
		}
	}
	while (set!=true);
	BlankDisplay(0);
	set=false;
	delay(BTN_DELAY);

	do
	{
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[AP]);
		digitalWrite(LATCH, HIGH);
		if (digitalRead(Select_BTN)==HIGH)
		{
			delay(debounce);
			AP++;
			if (AP>18)
			{
				AP=17;
			}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[AP]);
			digitalWrite(LATCH, HIGH);
		}
		if (digitalRead(Next_BTN)==HIGH)
		{
			delay(debounce);
			set=true;
		}
	}
	while (set!=true);
	BlankDisplay(0);
	set=false;
	delay(BTN_DELAY);

	//This portion is experimental.
	hour = (h1*10)+h2;
	//terminal_Display("Hour :",hour);
	minute = (m1*10)+m2;
	//terminal_Display("Minute :",minute);
	
	if (hour<=12 && minute <60) // in case user enters invalid time e.g. 2659h
	{
		alarmEEPROM(hour, minute,AP,1);
		
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[10]);
		digitalWrite(LATCH, HIGH); // display hyphen
		delay(DIGIT_DELAY);
		BlankDisplay(0);
		delay(DIGIT_DELAY);
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[10]);
		digitalWrite(LATCH, HIGH); // display hyphen
		delay(DIGIT_DELAY);
		BlankDisplay(0);
		
		//this needs to happen, cpu will hang
		//without it.
		//PPS_COUNT = 0;
		SECONDS_LAST = RTC.getSeconds();
		attachInterrupt(0, Interrupt_Update, RISING);
	}
	//get current clock settings.
	clockSettings();
}

int checkAlarm(int HR,int minute)
{
	//load the ALARM array with current time
	//variables.
	for(int x = 0;x<=1;x++)
	{
		ALARM_ARRAY[x] = splitInt(x,HR);
	}
	for(int x = 0;x<=1;x++)
	{
		ALARM_ARRAY[x+2] = splitInt(x,minute);
	}
	
	int same = 0;
	//check our set alarm in ALARM array against
	//EEPROM array to see if we have a match.
	for (int x =0;x<=3;x++)
	{
		if(ALARM_ARRAY[x] == EEPROM_ARRAY[x])
		{
			same +=1;
		}
		else
		{
			//Alarm is not the same.
			alarm_Silence = 0;
			return 0;
		}
		
	}
	//Check to see if we are set to go off at
	//AM or PM.
	if(AM_PM == EEPROM_ARRAY[4])
	{
		same +=1;
	}
	//Alarm is ready
	if(same == 5){return 1;}
}

void soundAlarm()
{
	detachInterrupt(0);
	
	RTC.readTime();
	int min = RTC.getMinutes();
	int min_last = RTC.getMinutes();
	
	//check to see if alarm is already silenced
	if(!alarm_Silence){
		do
		{
			/*This portion is not used as we have no buzzer.
			if(ALARM_ARRAY[5] == 1){
				digitalWrite(ALARM, HIGH);
				
				digitalWrite(LATCH, LOW);
				shiftOut(DATA_PIN,CLOCK,LSBFIRST,DATA_ARRAY[17]);
				digitalWrite(LATCH,HIGH);
				delay(300);
				
				BlankDisplay();
				
				digitalWrite(ALARM, LOW);
			}
			else
			{
				*/
				digitalWrite(LATCH, LOW);
				shiftOut(DATA_PIN,CLOCK,LSBFIRST,DATA_ARRAY[17]);
				digitalWrite(LATCH,HIGH);
				delay(5);
				
				BlankDisplay(0);
			//}
			//keep beeping if next button is not press
			//to silence the alarm.
			if(min_last != min){break;}
			RTC.readTime();
			min = RTC.getMinutes();
			
		} while (digitalRead(Next_BTN) == LOW);
		//This keeps us from sounding the alarm once
		//the user has silenced it.
		alarm_Silence = 1;
	}
	//keep the peizo buzzer pin high so we don't
	//keep sounding.
	//digitalWrite(ALARM, LOW);
	
	//PPS_COUNT = 0;
	SECONDS_LAST = RTC.getSeconds();
	attachInterrupt(0, Interrupt_Update, RISING);
}

void alarmEEPROM(byte hour, byte minute, int AP,int AT)
{
	// store alarm time in eeprom.
	for(int x = 0;x<=1;x++)//0-1 iterates twice.
	{
		EEPROM.put(x,splitInt(x,hour));
		delay(1);
	}

	for(int y = 2;y<=3;y++)
	{
		EEPROM.put(y,splitInt(y-2,minute));
		delay(1);
	}

	//store wether AM or PM in EEPROM.
	if(AP == 17)
	{
		EEPROM.put(4,0);
	}
	else
	{
		EEPROM.put(4,1);
	}
	
	EEPROM.put(5,AT);
}

float readTemp (int internal)
{
	//We are oversampling the temp sensor by 100 cycles
	//for better overall average temperature, this gives
	//us a more stable reading.

	Temperature = 0;
	if (!internal) //if internal == 0
	{
		for(int x = 0; x <= 99; x++)
		{
			Temperature += analogRead(LM35);
		}
		Temperature/100;

		// -11.69mv/C
		//The LM20 degree per C is inversely proportional
		//to its output at 1.574V T = 25C @ 303mV T = 130C
		//this equation has been calibrated for my office. Thus the
		//-7.4C.
		Temperature_C = (((11.69/Temperature)*(1024))*100)-7.4;//(1024))*100;
		return Temperature_C;
	}
	else
	{
		int iTemp_C = 0;
		int iTemperature = 0;
		/*
		for(int x = 0; x <= 99; x++)
		{
			iTemperature += analogRead(LM35);
		}
		iTemperature/100;
		*/
		iTemperature = analogRead(LM35);
		//same equation as above but we add 10 to the
		//final calculation since the display only displays
		//ints and not floats, we loose some accuracy, about
		//10degrees C.
		iTemp_C = (((12/iTemperature)*(1024))*100);

		return iTemp_C;
	}
}

int splitInt(int pos, int value)
{
	//This function requires you to create
	//a temp_array[] of at least 1 length.
	//this can be substituted for a normal
	//int provided in global variables.
	
	int x = 0;
	if(!pos)	//if pos == 0;
	{
		x = value % 10;
		Temp_Array[0] = x;
		value = (value - Temp_Array[0]) / 10;
		return value; //tens digit
	}
	else
	{
		return Temp_Array[0]; //ones digit
	}
}

int convertTime(int value)
{
	//This function is used to convert
	//a time eg a 24hr clock into a 12 hr
	//clock.
	
	if (value > 12)
	{
		value -= 12;
		return value;
	}
	else if (!value)
	{
		value = 12;
		return value;
	}
}

void BlankDisplay(int nap)
{
	if(!nap){
		//Blanks the current display being used.
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, 0b11111111);
		digitalWrite(LATCH, HIGH);
	}
	else
	{
		//Blanks the current display being used.
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, 0b11111111);
		digitalWrite(LATCH, HIGH);
		
		//delay(TIME_DELAY);
		gotoSleep(10);
	}
	
}

/**
 * \brief Set the specified bits in the control register.
 *
 * \param mask A mask specifying which bits to set. (High bits will be set.)
 *
 * \return 0 on success; otherwise an I2C error.
 */
uint8_t ds1337_set_control_bits(uint8_t mask) { // set bits
	uint8_t ctrl;
	uint8_t res = ds1337_get_control(&ctrl);
	if (res) { return res; }
	ctrl |= mask;
	return ds1337_set_control(ctrl);
}


/**
 * \brief Set the value of the control register.
 *
 * \param ctrl The value to set. 
 *
 * \return 0 on success; otherwise an I2C error.
 */
uint8_t ds1337_set_control(uint8_t ctrl) {
   uint8_t buf[2];
   buf[0] = DS1337_REG_CONTROL;
   buf[1] = ctrl;
   return i2c_write(DS1337_ADDR, buf, 2);
}

/**
 * \brief Write data to an I2C device. 
 *
 * \param addr The address of the device to which to write. 
 * \param buf A pointer to a buffer from which to read the data. 
 * \param num The number of bytes to write. 
 *
 * \return 0 on success; otherwise an I2C error.
 */
uint8_t i2c_write(uint8_t addr, uint8_t* buf, uint8_t num) {
  Wire.beginTransmission(addr);
  for (uint8_t i = 0; i < num; i++) {
    Wire.write(buf[i]);
  }
  return Wire.endTransmission();
}

/**
 * \brief Get the value of the control register.
 *
 * \param ctrl A pointer to a value in which to store the value of the control register. 
 *
 * \return 0 on success; otherwise an I2C error.
 */
uint8_t ds1337_get_control(uint8_t* ctrl) {
   uint8_t res = i2c_write_1(DS1337_ADDR, DS1337_REG_CONTROL);
   
   if (res) {
     return res;
   }
   
   res = i2c_read(DS1337_ADDR, ctrl, 1);
   
   if (res) {
     return res;
   }
   
   return 0;
}

/**
 * \brief Write a single byte to an I2C device. 
 *
 * \param addr The address of the device to which to write. 
 * \param b The byte to write. 
 *
 * \return 0 on success; otherwise an I2C error.
 */
uint8_t i2c_write_1(uint8_t addr, uint8_t b) {
  Wire.beginTransmission(addr);
  Wire.write(b);
  return Wire.endTransmission();
}

/**
 * \brief Read data from an I2C device. 
 *
 * \param addr The address of the device from which to read. 
 * \param buf A pointer to a buffer in which to store the data. 
 * \param num The number of bytes to read. 
 *
 * \return 0 on success; otherwise an I2C error.
 */
uint8_t i2c_read(uint8_t addr, uint8_t* buf, uint8_t num) {
  Wire.requestFrom(addr, num);
  
  if (Wire.available() < num) {
    return READ_ERROR;
  }
  
  for (uint8_t i = 0; i < num; i++) {
    buf[i] = Wire.read();
  }
  
  return 0;
}

/**
 * \brief Clear the specified bits in the control register.
 *
 * \param mask A mask specifying which bits to clear. (High bits will be cleared.) 
 *
 * \return 0 on success; otherwise an I2C error.
 */
uint8_t ds1337_clear_control_bits(uint8_t mask) {
	return ds1337_set_control(~mask);
}
