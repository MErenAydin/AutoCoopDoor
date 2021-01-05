//#define FIRST
//#define SERIAL_DEBUG

#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>


#define LED 6
#define MOTOR_A 5
#define MOTOR_B 4
#define ENDSWITCH A6
#define STARTSWITCH A7
#define BUTTON_LEFT A0
#define BUTTON_MID A1
#define BUTTON_RIGHT A2
#define MOVECURSOR 1
#define MOVELIST 2

LiquidCrystal lcd(12, 11, 7, 8, 9, 10);
RTC_DS1307  myRTC;
DateTime  t;

boolean first = true;
boolean open;
boolean isOneSwitch;
boolean click = false;
boolean doubleClick = false;
int second, minute, hour, day, month, year;
int openingHour, openingMinute;
int closingHour, closingMinute;
byte LCDRows = 2;
byte LCDCols = 8;
byte topItemDisplayed = 0;
byte cursorPosition = 0;
unsigned long timeoutTime = 0;
unsigned long lastButtonPressed;
unsigned long openingTime, closingTime;
const int menuTimeout = 10000;

int debounceTime = 250;
float distance;

void setup() {
	int multO, adjO, multC, adjC;
	pinMode(LED, OUTPUT);
	pinMode(MOTOR_A, OUTPUT);
	pinMode(MOTOR_B, OUTPUT);

	lcd.begin(8, 2);
  #ifdef SERIAL_DEBUG
	  Serial.begin(115200);
  #endif
	if (!myRTC.begin()) {
		lcd.setCursor(0, 0);
		lcd.print("RTC HATA");
    #ifdef SERIAL_DEBUG
      Serial.println("RTC HATA...");
    #endif
	}

  #ifndef FIRST
  
    openingHour = EEPROM.read(0);
    openingMinute = EEPROM.read(1);
    closingHour = EEPROM.read(2);
    closingMinute = EEPROM.read(3);
    multO = EEPROM.read(4);
    adjO = EEPROM.read(5);
    multC = EEPROM.read(6);
    adjC = EEPROM.read(7);
    
  #else

    EEPROM.update(0, 7);
    EEPROM.update(1, 0);
    EEPROM.update(2, 19);
    EEPROM.update(3, 0);
    EEPROM.update(4, 3);
    EEPROM.update(5, 235);
    EEPROM.update(6, 3);
    EEPROM.update(7, 235);
    // January 21, 2014 at 3am you would call:
    // myRTC.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  
  #endif

	digitalWrite(LED, LOW);

	openingTime = (multO * 255) + adjO;
	closingTime = (multC * 255) + adjC;
	if (readSwitch(ENDSWITCH))
		open = false;
	else if (readSwitch(STARTSWITCH))
		open = true;
}

void loop() {
  #ifdef SERIAL_DEBUG
    Serial.print("RTC DEBUG: ");
    t = myRTC.now();
    Serial.print(t.hour());
    Serial.print(":");
    Serial.print(t.minute());
    Serial.print(":");
    Serial.print(t.second());
    Serial.print("\t");
    Serial.print(t.day());
    Serial.print("/");
    Serial.print(t.month());
    Serial.print("/");
    Serial.println(t.year());
    delay(1000);
  #endif
  basicMenu();
}

void basicMenu() {
label1:
	topItemDisplayed = 0;
	cursorPosition = 0;
	byte redraw = MOVELIST;
	byte i = 0;
	byte totalMenuItems = 0;

	char* menuItems[] = {
		"AC/KAP",
		"YUKARI",
		"ASAGI",
		"A.SAAT",
		"K.SAAT",
		"MESAFE",
		"TARIH",
		""
	};

	while (menuItems[totalMenuItems] != "") {
		totalMenuItems++;
	}
	totalMenuItems--;

	lcd.clear();
	boolean stillSelecting = true;
	timeoutTime = millis() + menuTimeout;

	do
	{
		adjust();
		
		switch (readButton())
		{
		case 1:

			timeoutTime = millis() + menuTimeout;
			if (cursorPosition == 0 && topItemDisplayed > 0)
			{
				topItemDisplayed--;  // move top menu item displayed up one. 
				redraw = MOVELIST;  // redraw the entire menu
			}

			// if cursor not at top, move it up one.
			if (cursorPosition>0)
			{
				cursorPosition--;  // move cursor up one.
				redraw = MOVECURSOR;  // redraw just cursor.
			}
			break;

		case 2:

			timeoutTime = millis() + menuTimeout;  // reset timeout timer
												   // this sees if there are menu items below the bottom of the LCD screen & sees if cursor is at bottom of LCD 
			if ((topItemDisplayed + (LCDRows - 1)) < totalMenuItems && cursorPosition == (LCDRows - 1))
			{
				topItemDisplayed++;  // move menu down one
				redraw = MOVELIST;  // redraw entire menu
			}
			if (cursorPosition<(LCDRows - 1))  // cursor is not at bottom of LCD, so move it down one.
			{
				cursorPosition++;  // move cursor down one
				redraw = MOVECURSOR;  // redraw just cursor.
			}
			break;

		case 4:

			timeoutTime = millis() + menuTimeout;
			switch (topItemDisplayed + cursorPosition) {
			case 0:
				turnMotor(open);
				goto label1;
				break;

			case 1:
				while (!readSwitch(ENDSWITCH) && readSwitch(BUTTON_MID))
				{
					digitalWrite(MOTOR_A, HIGH);
					digitalWrite(MOTOR_B, LOW);
				}
				if (readSwitch(ENDSWITCH))
					open = false;
					
				digitalWrite(MOTOR_A, LOW);
				digitalWrite(MOTOR_B, LOW);
				break;

			case 2:
				while (!readSwitch(STARTSWITCH) && readSwitch(BUTTON_MID)) {
					digitalWrite(MOTOR_A, LOW);
					digitalWrite(MOTOR_B, HIGH);
				}
				if (readSwitch(STARTSWITCH))
					open = true;
				digitalWrite(MOTOR_A, LOW);
				digitalWrite(MOTOR_B, LOW);
				break;

			case 3:
				setOpeningTime();
				goto label1;
				break;

			case 4:
				setClosingTime();
				goto label1;
				break;

			case 5:
				Calibrate();
				goto label1;
				break;

			case 6:
				setDateAndTime();
				goto label1;
				break;
			}
			break;

		case 8:
			displayDate();
			goto label1;
			break;

		}

		switch (redraw) {  //  checks if menu should be redrawn at all.
		case MOVECURSOR:  // Only the cursor needs to be moved.
			redraw = false;  // reset flag.
			if (cursorPosition > totalMenuItems) // keeps cursor from moving beyond menu items.
				cursorPosition = totalMenuItems;
			for (i = 0; i < (LCDRows); i++) {  // loop through all of the lines on the LCD
				lcd.setCursor(0, i);
				lcd.print(" ");                      // and erase the previously displayed cursor
				lcd.setCursor((LCDCols - 1), i);
				lcd.print(" ");
			}
			lcd.setCursor(0, cursorPosition);      // go to LCD line where new cursor should be & display it.
			lcd.print(">");
			lcd.setCursor((LCDCols - 1), cursorPosition);
			lcd.print("<");
			break;  // MOVECURSOR break.

		case MOVELIST:  // the entire menu needs to be redrawn
			redraw = MOVECURSOR;  // redraw cursor after clearing LCD and printing menu.
			lcd.clear(); // clear screen so it can be repainted.
			if (totalMenuItems>((LCDRows - 1))) {  // if there are more menu items than LCD rows, then cycle through menu items.
				for (i = 0; i < (LCDRows); i++) {
					lcd.setCursor(1, i);
					lcd.print(menuItems[topItemDisplayed + i]);
				}
			}
			else {  // if menu has less items than LCD rows, display all available menu items.
				for (i = 0; i < totalMenuItems + 1; i++) {
					lcd.setCursor(1, i);
					lcd.print(menuItems[topItemDisplayed + i]);
				}
			}
			break;
		}

		if (timeoutTime<millis()) {
			displayDate();
			goto label1;
		}
	}


	while (stillSelecting == true);  //
}

void displayDate(){
	int enc;
	do{
		adjust();
		enc = readButton();
		t = myRTC.now();
		lcd.setCursor(0, 0);
		printTwoDigit(t.hour());
		lcd.print(":");
		printTwoDigit(t.minute());
		lcd.print(":");
		printTwoDigit(t.second());
		lcd.setCursor(0, 1);
		printTwoDigit(t.day());
		lcd.print("/");
		printTwoDigit(t.month());
		lcd.print("/");
		printTwoDigit(t.year() % 100);
	} while (enc != 4);
}

void printTwoDigit(uint8_t x) {
	if (x < 10) {
		lcd.print("0");
		lcd.print(x);
	}
	else
		lcd.print(x);
}

void turnMotor(bool opening) {
	unsigned long turnTime = millis();

	if (opening) {
		while (!readSwitch(ENDSWITCH) && millis() - turnTime <= openingTime + 500)
		{
			digitalWrite(MOTOR_A, HIGH);
			digitalWrite(MOTOR_B, LOW);
		}
		digitalWrite(MOTOR_A, LOW);
		digitalWrite(MOTOR_B, LOW);
		open = false;
	}
	else {
		while (!readSwitch(STARTSWITCH) && millis() - turnTime <= closingTime + 500)
		{
			digitalWrite(MOTOR_A, LOW);
			digitalWrite(MOTOR_B, HIGH);
		}
		digitalWrite(MOTOR_A, LOW);
		digitalWrite(MOTOR_B, LOW);
		open = true;
	}
}

void Calibrate() {
	int multO, adjO, multC, adjC;
	unsigned long startingTimeO , startingTimeC;
	while (!readSwitch(ENDSWITCH))
	{
		digitalWrite(MOTOR_A, HIGH);
		digitalWrite(MOTOR_B, LOW);
	}
	digitalWrite(MOTOR_A, LOW);
	digitalWrite(MOTOR_B, LOW);

	startingTimeC = millis();

	while (!readSwitch(STARTSWITCH))
	{
		digitalWrite(MOTOR_A, LOW);
		digitalWrite(MOTOR_B, HIGH);
	}
	digitalWrite(MOTOR_A, LOW);
	digitalWrite(MOTOR_B, LOW);

	closingTime = millis() - startingTimeC;
	startingTimeO = millis();

	while (!readSwitch(ENDSWITCH))
	{
		digitalWrite(MOTOR_A, HIGH);
		digitalWrite(MOTOR_B, LOW);
	}
	digitalWrite(MOTOR_A, LOW);
	digitalWrite(MOTOR_B, LOW);
	turnMotor(!open);

	openingTime = millis() - startingTimeO;
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("A:");
	lcd.print(openingTime); 
	lcd.setCursor(0, 1);
	lcd.print("K:");
	lcd.print(closingTime);
	
	multO = openingTime / 255;
	adjO = openingTime % 255;
	multC = closingTime / 255;
	adjC = closingTime % 255;

	EEPROM.update(4, multO);
	EEPROM.update(5, adjO);
	EEPROM.update(6, multC);
	EEPROM.update(7, adjC);

	delay(2000);
}

boolean readSwitch(int x) {
	if (analogRead(x) >= 500)
		return true;
	else
		return false;
}

void setDateAndTime() {
	int nday, nmonth, nyear, nsecond, nminute, nhour, ndayofweek;
	t = myRTC.now();
	nsecond = t.second();
	nminute = t.minute();
	nhour = t.hour();
	nday = t.day();
	nmonth = t.month();
	nyear = t.year();
	int enc;
	int maxBorder;
	bool show = true;
	bool finish = false;
	unsigned long temptime = millis();

	lcd.clear();
	lcd.setCursor(0, 0);
	printTwoDigit(nday);
	lcd.print("/");
	printTwoDigit(nmonth);
	lcd.print("/");
	printTwoDigit(nyear);
	lcd.setCursor(0, 1);
	printTwoDigit(nhour);
	lcd.print(":");
	printTwoDigit(nminute);
	lcd.print(":");
	printTwoDigit(nsecond);

	//year adjusting
	do {
		adjust();
		if (millis() - temptime >= 500) {
			show = !show;
			temptime = millis();
		}
		enc = readButton();
		if (enc == 1 && nyear < 2099)
			nyear++;
		else if (enc == 2 && nyear > 2000)
			nyear--;
		else if (enc == 8)
			finish = true;
		lcd.setCursor(6, 0);
		if (show)
			printTwoDigit(nyear % 100);
		else
			lcd.print("  ");

	} while (enc != 4 && !finish);
	lcd.setCursor(6, 0);
	printTwoDigit(nyear % 100);

	// month adjusting

	do {
		adjust();
		if (millis() - temptime >= 500) {
			show = !show;
			temptime = millis();
		}
		enc = readButton();
		if (enc == 1 && nmonth < 12)
			nmonth++;
		else if (enc == 2 && nmonth > 1)
			nmonth--;
		else if (enc == 8)
			finish = true;
		lcd.setCursor(3, 0);
		if (show)
			printTwoDigit(nmonth);
		else
			lcd.print("  ");
	} while (enc != 4 && !finish);

	lcd.setCursor(3, 0);
	printTwoDigit(nmonth);

	//day adjusting
	do {
		adjust();
		if (millis() - temptime >= 500) {
			show = !show;
			temptime = millis();
		}
		enc = readButton();
		if (nyear % 4 == 0 && nmonth == 2)
			maxBorder = 29;
		else if (nmonth == 2)
			maxBorder = 28;
		else if (nmonth == 4 || nmonth == 6 || nmonth == 9 || nmonth == 11)
			maxBorder = 30;
		else
			maxBorder = 31;


		if (enc == 1 && nday < maxBorder)
			nday++;
		else if (enc == 2 && nday > 1)
			nday--;
		else if (enc == 8)
			finish = true;
		lcd.setCursor(0, 0);
		if (show)
			printTwoDigit(nday);
		else
			lcd.print("  ");
	} while (enc != 4 && !finish);

	lcd.setCursor(0, 0);
	printTwoDigit(nday);

	//hour adjusment

	do {
		adjust();
		if (millis() - temptime >= 500) {
			show = !show;
			temptime = millis();
		}
		enc = readButton();
		if (enc == 1 && nhour < 23)
			nhour++;
		else if (enc == 2 && nhour > 0)
			nhour--;
		else if (enc == 8)
			finish = true;
		lcd.setCursor(0, 1);
		if (show)
			printTwoDigit(nhour);
		else
			lcd.print("  ");
	} while (enc != 4 && !finish);

	lcd.setCursor(0, 1);
	printTwoDigit(nhour);


	do {
		adjust();
		if (millis() - temptime >= 500) {
			show = !show;
			temptime = millis();
		}
		enc = readButton();
		if (enc == 1 && nminute < 59)
			nminute++;
		else if (enc == 2 && nminute > 0)
			nminute--;
		else if (enc == 8)
			finish = true;
		lcd.setCursor(3, 1);
		if (show)
			printTwoDigit(nminute);
		else
			lcd.print("  ");
	} while (enc != 4 && !finish);

	lcd.setCursor(3, 1);
	printTwoDigit(nminute);

	do {
		adjust();
		if (millis() - temptime >= 500) {
			show = !show;
			temptime = millis();
		}
		enc = readButton();
		if (enc == 1 && nsecond < 59)
			nsecond++;
		else if (enc == 2 && nsecond > 0)
			nsecond--;
		else if (enc == 8)
			finish = true;
		lcd.setCursor(6, 1);
		if (show)
			printTwoDigit(nsecond);
		else
			lcd.print("  ");
	} while (enc != 4 && !finish);


	lcd.setCursor(6, 1);
	printTwoDigit(nsecond);

	

	lcd.setCursor(0, 1);
	printTwoDigit(ndayofweek);

	if (finish) return;
	else
	{	
		myRTC.adjust(DateTime(nyear, nmonth, nday, nhour, nminute, nsecond));
	}
}

void setOpeningTime() {
	int enc;
	int nopeningHour = openingHour;
	int nopeningMinute = openingMinute;
	bool show = true;
	bool finish = false;
	unsigned long temptime = millis();

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("ACILIS:");
	lcd.setCursor(0, 1);
	printTwoDigit(nopeningHour);
	lcd.print(":");
	printTwoDigit(nopeningMinute);

	do {
		adjust();
		if (millis() - temptime >= 500) {
			show = !show;
			temptime = millis();
		}
		enc = readButton();
		if (enc == 1 && nopeningHour < 23)
			nopeningHour++;
		else if (enc == 2 && nopeningHour > 0)
			nopeningHour--;
		else if (enc == 8)
			finish = true;
		lcd.setCursor(0, 1);
		if (show)
			printTwoDigit(nopeningHour);
		else
			lcd.print("  ");
	} while (enc != 4 && !finish);

	lcd.setCursor(0, 1);
	printTwoDigit(nopeningHour);

	do {
		adjust();
		if (millis() - temptime >= 500) {
			show = !show;
			temptime = millis();
		}
		enc = readButton();
		if (enc == 1 && nopeningMinute < 59)
			nopeningMinute++;
		else if (enc == 2 && nopeningMinute > 0)
			nopeningMinute--;
		else if (enc == 8)
			finish = true;
		lcd.setCursor(3, 1);
		if (show)
			printTwoDigit(nopeningMinute);
		else
			lcd.print("  ");
	} while (enc != 4 && !finish);

	lcd.setCursor(3, 1);
	printTwoDigit(nopeningMinute);

	if (finish) return;
	else {
		EEPROM.update(0,nopeningHour);
		EEPROM.update(1, nopeningMinute);
		openingHour = nopeningHour;
		openingMinute = nopeningMinute;
	}
}

void setClosingTime() {
	int enc;
	int nclosingHour = closingHour;
	int nclosingMinute = closingMinute;
	bool show = true;
	bool finish = false;
	unsigned long temptime = millis();

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("KAPANIS:");
	lcd.setCursor(0, 1);
	printTwoDigit(nclosingHour);
	lcd.print(":");
	printTwoDigit(nclosingMinute);

	do {
		adjust();
		if (millis() - temptime >= 500) {
			show = !show;
			temptime = millis();
		}
		enc = readButton();
		if (enc == 1 && nclosingHour < 23)
			nclosingHour++;
		else if (enc == 2 && nclosingHour > 0)
			nclosingHour--;
		else if (enc == 8)
			finish = true;
		lcd.setCursor(0, 1);
		if (show)
			printTwoDigit(nclosingHour);
		else
			lcd.print("  ");
	} while (enc != 4 && !finish);

	lcd.setCursor(0, 1);
	printTwoDigit(nclosingHour);

	do {
		adjust();
		if (millis() - temptime >= 500) {
			show = !show;
			temptime = millis();
		}
		enc = readButton();
		if (enc == 1 && nclosingMinute < 59)
			nclosingMinute++;
		else if (enc == 2 && nclosingMinute > 0)
			nclosingMinute--;
		else if (enc == 8)
			finish = true;
		lcd.setCursor(3, 1);
		if (show)
			printTwoDigit(nclosingMinute);
		else
			lcd.print("  ");
	} while (enc != 4 && !finish);

	lcd.setCursor(3, 1);
	printTwoDigit(nclosingMinute);

	if (finish) return;
	else {
		EEPROM.update(2, nclosingHour);
		EEPROM.update(3, nclosingMinute);
		closingHour = nclosingHour;
		closingMinute = nclosingMinute;
	}
}

void adjust() {

	t = myRTC.now();
	// a��l�� saati
	if (t.hour() == openingHour && t.minute() == openingMinute) {
		
		if (first) {
			turnMotor(true);
			open = false;
			first = false;
		}
	}
	//kapan�� saati
	else if (t.hour() == closingHour && t.minute() == closingMinute) {
		if (first) {
			turnMotor(false);
			open = true;
			first = false;
		}
	}

	else first = true;
}

byte readButton() {
	int a = topItemDisplayed + cursorPosition;
	byte returndata = 0;
	unsigned long time = millis();
	if ((lastButtonPressed + debounceTime) < millis() ){

		if (readSwitch(BUTTON_RIGHT)) {
			returndata = 1;
			lastButtonPressed = millis();
		}
		else if (readSwitch(BUTTON_LEFT)) {
			returndata = 2;
			lastButtonPressed = millis();
		}

		else if (readSwitch(BUTTON_MID)) {
			time = millis();
			returndata = 4;
			while (readSwitch(BUTTON_MID) && a != 1 && a != 2) {
				if (millis() - time >= 300){
					returndata = 8;
				}
			}
			lastButtonPressed = millis();
		}
		
		else returndata = 0;
	}
	return returndata; // this spits back to the function that calls it the variable returndata.
}
