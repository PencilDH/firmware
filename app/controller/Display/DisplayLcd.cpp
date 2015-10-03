/*
 * Copyright 2012-2013 BrewPi/Elco Jacobs.
 *
 * This file is part of BrewPi.
 * 
 * BrewPi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later v7ersion.
 * 
 * BrewPi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with BrewPi.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Brewpi.h"
#include "temperatureFormats.h"
#include "BrewpiStrings.h"
#include "Display.h"
#include "DisplayLcd.h"
#include "TempControl.h"
#include "Board.h"
#include "fixstl.h"
#include <algorithm>

using std::min;


uint8_t LcdDisplay::stateOnDisplay;
uint8_t LcdDisplay::flags;
LcdDriver LcdDisplay::lcd;

// Constant strings used multiple times
static const char STR_Beer_[] PROGMEM = "Beer ";
static const char STR_Fridge_[] PROGMEM = "Fridge ";
static const char STR_Const_[] PROGMEM = "Const.";
static const char STR_Cool[] PROGMEM = "Cool";
static const char STR_Heat[] PROGMEM = "Heat";
static const char STR_ing_for[] PROGMEM = "ing for";
static const char STR_Wait_to_[] PROGMEM = "Wait to ";
static const char STR__time_left[] PROGMEM = " time left";
static const char STR_empty_string[] PROGMEM = "";

void LcdDisplay::init(void){
	stateOnDisplay = 0xFF; // set to unknown state to force update
	flags = LCD_FLAG_ALTERNATE_ROOM;
	lcd.init(); // initialize LCD
	lcd.begin(20, 4);
	lcd.clear();
}

#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif

//print all temperatures on the LCD
void LcdDisplay::printAllTemperatures(void){
	// alternate between beer and room temp
	if (flags & LCD_FLAG_ALTERNATE_ROOM) {
		bool displayRoom = ((ticks.seconds()&0x08)==0) && !BREWPI_SIMULATE && tempControl.ambientSensor->isConnected();
		if (displayRoom ^ ((flags & LCD_FLAG_DISPLAY_ROOM)!=0)) {	// transition
			flags = displayRoom ? flags | LCD_FLAG_DISPLAY_ROOM : flags & ~LCD_FLAG_DISPLAY_ROOM;
			printStationaryText();
		}
	}
	
	printBeerTemp();
	printBeerSet();
	printFridgeTemp();
	printFridgeSet();
}

void LcdDisplay::setDisplayFlags(uint8_t newFlags) {
	flags = newFlags;
	printStationaryText();
	printAllTemperatures();
}



void LcdDisplay::printBeerTemp(void){
	printTemperatureAt(6, 1, tempControl.getBeerTemp());
}

void LcdDisplay::printBeerSet(void){
	temp_t beerSet = tempControl.getBeerSetting();
	printTemperatureAt(12, 1, beerSet);	
}

void LcdDisplay::printFridgeTemp(void){	
	printTemperatureAt(6,2, flags & LCD_FLAG_DISPLAY_ROOM ?
		tempControl.ambientSensor->read() :
		tempControl.getFridgeTemp());
}

void LcdDisplay::printFridgeSet(void){	
	temp_t fridgeSet = tempControl.getFridgeSetting();
	if(flags & LCD_FLAG_DISPLAY_ROOM) // beer setting is not active
		fridgeSet = temp_t::disabled();
	printTemperatureAt(12, 2, fridgeSet);	
}

void LcdDisplay::printTemperatureAt(uint8_t x, uint8_t y, temp_t temp){
	lcd.setCursor(x,y);
	printTemperature(temp);
}


void LcdDisplay::printTemperature(temp_t temp){
	if (temp.isDisabledOrInvalid()) {
		lcd.print_P(PSTR(" --.-"));
		return;
	}
	char tempString[9];
	temp.toString(tempString, 1, 9);
	lcd.print(tempString);
}

//print the stationary text on the lcd.
void LcdDisplay::printStationaryText(void){
	printAt_P(0, 0, PSTR("Mode"));
	printAt_P(0, 1, STR_Beer_);
	printAt_P(0, 2, (flags & LCD_FLAG_DISPLAY_ROOM) ?  PSTR("Room  ") : STR_Fridge_); 
	printDegreeUnit(18, 1);
	printDegreeUnit(18, 2);
}

//print degree sign + temp unit
void LcdDisplay::printDegreeUnit(uint8_t x, uint8_t y){
	lcd.setCursor(x,y);
	lcd.write(0b11011111);
	lcd.write(tempControl.cc.tempFormat);	
}

void LcdDisplay::printAt_P(uint8_t x, uint8_t y, const char* text){
	lcd.setCursor(x, y);
	lcd.print_P(text);
}


void LcdDisplay::printAt(uint8_t x, uint8_t y, char* text){
	lcd.setCursor(x, y);
	lcd.print(text);
}

// print mode on the right location on the first line, after "Mode   "
void LcdDisplay::printMode(void){
	lcd.setCursor(7,0);
	// Factoring prints out of switch has negative effect on code size in this function
	switch(tempControl.getMode()){
		case MODE_FRIDGE_CONSTANT:
			lcd.print_P(STR_Fridge_);
			lcd.print_P(STR_Const_);
			break;
		case MODE_BEER_CONSTANT:
			lcd.print_P(STR_Beer_);
			lcd.print_P(STR_Const_);
			break;
		case MODE_BEER_PROFILE:
			lcd.print_P(STR_Beer_);
			lcd.print_P(PSTR("Profile"));
			break;
		case MODE_OFF:
			lcd.print_P(PSTR("Off"));
			break;
		case MODE_TEST:
			lcd.print_P(PSTR("** Testing **"));
			break;
		default:
			lcd.print_P(PSTR("Invalid mode"));
			break;
	}
	lcd.printSpacesToRestOfLine();
}

// print the current state on the last line of the lcd
void LcdDisplay::printState(void){
	uint16_t time = UINT16_MAX; // init to max
	uint8_t state = tempControl.getDisplayState();
	if(state != stateOnDisplay){ //only print static text when state has changed
		stateOnDisplay = state;
		// Reprint state and clear rest of the line
		const char * part1 = STR_empty_string;
		const char * part2 = STR_empty_string;
		switch (state){
			case IDLE:
				part1 = PSTR("Idl");
				part2 = STR_ing_for;
				break;
			case COOLING:
				part1 = STR_Cool;
				part2 = STR_ing_for;
				break;
			case HEATING:
				part1 = STR_Heat;
				part2 = STR_ing_for;
				break;
			case DOOR_OPEN:
				part1 = PSTR("Door open");
				break;
			case STATE_OFF:
				part1 = PSTR("Temp. control OFF");
				break;
			default:
				part1 = PSTR("Unknown status!");
				break;
		}
		printAt_P(0, 3, part1);
		lcd.print_P(part2);		
		lcd.printSpacesToRestOfLine();
	}
	uint16_t sinceIdleTime = tempControl.timeSinceIdle();
	if(state==IDLE){
		time = 	min(tempControl.timeSinceCooling(), tempControl.timeSinceHeating());
	}
	else if(state==COOLING || state==HEATING){
		time = sinceIdleTime;
	}
	if(time != UINT16_MAX){
		char timeString[10];
#if DISPLAY_TIME_HMS  // 96 bytes more space required. 
		unsigned int minutes = time/60;		
		unsigned int hours = minutes/60;
		int stringLength = sprintf_P(timeString, PSTR("%dh%02dm%02d"), hours, minutes%60, time%60);
		char * printString = timeString;
		if(!hours){
			printString = &timeString[2];
			stringLength = stringLength-2;
		}
		printAt(20-stringLength, 3, printString);
#else
		int stringLength = sprintf_P(timeString, STR_FMT_U, (unsigned int)time);
		printAt(20-stringLength, 3, timeString);
#endif		
	}
}