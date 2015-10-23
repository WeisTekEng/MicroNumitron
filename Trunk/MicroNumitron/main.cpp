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
 * License:	GNU GENERAL PUBLIC LICENSE Version 3 or newer.
 */ 

//Fuse settings
/*
BOOTSZ = 1024W_1C00
BOOTRST = [ ]
RSTDISBL = [ ]
DWEN = [ ]
SPIEN = [X]
WDTON = [ ]
EESAVE = [X]
BODLEVEL = 1V8
CKDIV8 = [ ]
CKOUT = [ ]
SUT_CKSEL = INTRCOSC_8MHZ_6CK_14CK_65MS

EXTENDED = 0xF9 (valid)
HIGH = 0xD6 (valid)
LOW = 0xE2 (valid)
*/

//Includes
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include "EEPROM.h"				//Store persistent variables in eeprom.

//Defines
#define F_CPU 8000000L

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
int PPS_TIMER = 20;         //How often we want to update the system in seconds
int PPS_COUNT = 0;          //used in interrupt routine.
int TIME_DELAY = 500;
boolean tempDisplay = true;      // used to set whether the display shows temperature
boolean tempCORF = false;	     // false for C and true for F.
float Temperature_C = 0;        //our temp in C
float Temperature = 0;          //value read from temp sensor
int AM_PM = 0;					//0 = AM 1 = PM

//Constants
const int Delay_Time = 200;

RTC_DS1307 rtc;             //Creating a new RTC object.

//Char array, stores display data.
byte DATA_ARRAY[20] = { 0b01000000,//0
						0b01110011,//1
						0b00001010,//2
						0b00100010,//3
						0b00110001,//4
						0b00100100,//5
						0b00000100,//6
						0b01110010,//7
						0b00000000,//8
						0b00110000,//9
						0b00111111,//-
						0b00010001,//H
						0b00001100,//E
						0b01001101,//L
						0b01000000,//O
						0b00111000,//�
						0b00001111,//C
						0b00010000,//A
						0b00011000//P
};
int Temp_Array[5] = {0,0,0,0,0};

///////////////////////////////////////////////////////////
//RTC PWM pin modes, best to leave this setting at 1Hz
///////////////////////////////////////////////////////////
//Modes                           16            17                18              19
Ds1307SqwPinMode modes[] = {SquareWave1HZ, SquareWave4kHz, SquareWave8kHz, SquareWave32kHz};

//Prototypes.
void(* resetFunc)(void)=0; //declare reset function at address 0
void setup();
void loop();
void Interrupt_Update();
void Init();
void Update_Display();
float readTemp (int internal);
int splitInt(int pos, int value);
int convertTime(int value);
void BlankDisplay();

void setup()
{
		
	//Initialize the clock
	Init();
	
	//clear display at start up.
	BlankDisplay();

	//allow interrupts
	sei();
	
	//Interrupts
	attachInterrupt(0, Interrupt_Update, FALLING);

}

void loop()
{
	if (PPS_COUNT == PPS_TIMER)
	{
		detachInterrupt(0);
		//Start the internal update
		//Internal_Update();
		
		//Update the VFD display
		Update_Display();
		PPS_COUNT = 0;
		attachInterrupt(0, Interrupt_Update, RISING);
		
		//Flush the serial buffer for good measure.
		//Serial.flush();
	}
	
	if (digitalRead(Select_BTN)==HIGH)
	{
		//setTime();
	}
	
	if(digitalRead(Next_BTN)==HIGH)
	{
		//setALARM();
	}

}

void Interrupt_Update()
{
	//foo.
}

void Init()
{
	///////////////////////////////////////////////////////////
	//This is where we initialize everything.
	///////////////////////////////////////////////////////////
	
	//Coms will be setup later.
	
	Wire.begin();
	
	rtc.begin();
	
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
	
	///////////////////////////////////////////////////////////
	//Pin Functions
	///////////////////////////////////////////////////////////

	//pinMode(Freq_Adj, INPUT);
	pinMode(INTERUPT_0, INPUT);
	pinMode(DATA_PIN, OUTPUT);
	pinMode(CLOCK, OUTPUT);
	pinMode(LATCH, OUTPUT);
	pinMode(Select_BTN, INPUT);
	pinMode(Next_BTN, INPUT);
	//pinMode(FILL, OUTPUT);
	pinMode(LM35, INPUT);
	//pinMode(ALARM, OUTPUT);
	//digitalWrite(ALARM, LOW); //this may have changed on the board.
	
	///////////////////////////////////////////////////////////
	//If the RTC is not currently set to the correct time set the time
	//based on the time used when compiling this firmware.
	///////////////////////////////////////////////////////////
	if (! rtc.isrunning())
	{
		Serial.println(F("RTC is NOT running!, Check wiring setup.\r\n"));
		delay(Delay_Time);

		rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
		Serial.print(F("RTC Time set!"));
	}
	
	///////////////////////////////////////////////////////////
	//This is setting the RTC Square wave pin output to 32.7kHz
	//We will be using this as our interrupt to update the time.

	rtc.writeSqwPinMode(modes[0]);
	///////////////////////////////////////////////////////////
}

void Update_Display()
{
	///////////////////////////////////////////////////////////
	//This function updates the display on the VFD Tube.
	///////////////////////////////////////////////////////////
	DateTime now = rtc.now();
	int HR = 0;
	int MN = 0;

	MN = now.minute();
	HR = now.hour();

	//Converting 24HR time to 12HR format.
	HR = convertTime(HR);
	
	//display hours first tens then one's position.
	Temp_Array[2] = splitInt(0,HR);
	for(int x = 0; x<=1;x++)
	{
		if(Temp_Array[2] == splitInt(1,HR)){BlankDisplay();}
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[splitInt(x,HR)]);
		digitalWrite(LATCH, HIGH);
		delay(TIME_DELAY);
		
	}
	
	//Dash between HH:MM to separate Hours and minutes.
	digitalWrite(LATCH, LOW);
	shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[10]);
	digitalWrite(LATCH, HIGH);
	delay(TIME_DELAY);


	//display minutes first tens then one's position
	Temp_Array[3] = splitInt(0,MN);
	for(int y = 0;y<=1;y++)
	{
		if(Temp_Array[3] == splitInt(1,MN)){BlankDisplay();}
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[splitInt(y,MN)]);
		digitalWrite(LATCH, HIGH);
		delay(TIME_DELAY);
	}

	//Blank display
	BlankDisplay();
	
	if(tempDisplay == true)
	{
		//Display Temperature.
		Temp_Array[4] = splitInt(0,readTemp(1));
		for (int z = 0; z <= 1; z++)
		{
			if(Temp_Array[4] == splitInt(1,readTemp(1))){BlankDisplay();}
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[splitInt(z,readTemp(1))]);
			digitalWrite(LATCH, HIGH);
			delay(TIME_DELAY);
		}

		//display a degree symbol
		digitalWrite(LATCH, LOW);
		shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[15]);
		digitalWrite(LATCH, HIGH);
		delay(TIME_DELAY);
		
		BlankDisplay();
		if(tempCORF == false)
		{
			//display a Celsius (c) symbol
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[16]);
			digitalWrite(LATCH, HIGH);
			delay(TIME_DELAY);

			BlankDisplay();
		}
		else
		{
			//display a Celsius (c) symbol
			digitalWrite(LATCH, LOW);
			shiftOut(DATA_PIN, CLOCK, LSBFIRST, DATA_ARRAY[16]);
			digitalWrite(LATCH, HIGH);
			delay(TIME_DELAY);

			BlankDisplay();
		}
	}
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
		Temperature_C = Temperature * ((5.0*1000/1024));
		return Temperature_C;
	}
	else
	{
		int iTemp_C = 0;
		int iTemperature = 0;
		for(int x = 0; x <= 99; x++)
		{
			iTemperature += analogRead(LM35);
		}
		iTemperature = iTemperature/100;
		
		//same equation as above but we add 10 to the
		//final calculation since the display only displays
		//ints and not floats, we loose some accuracy, about
		//10degrees C.
		iTemp_C = iTemperature * ((5.0*1000/1024)) + 10;

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

void BlankDisplay()
{
	//Blanks the current display being used.
	digitalWrite(LATCH, LOW);
	shiftOut(DATA_PIN, CLOCK, LSBFIRST, 0b11111111);
	digitalWrite(LATCH, HIGH);
	
	delay(TIME_DELAY);
}