#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711_ADC.h>
#include <EEPROM.h>

// SDA PIN is A4
// SCL PIN is A5
LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 20 chars and 4 line display

// DT PIN is D4
// SCK PIN is D5
const int HX711_dout = 4;
const int HX711_sck = 5;
HX711_ADC LoadCell(HX711_dout, HX711_sck);
static float currentWeight; // in gramms
const float beerDensity = 1.005;

const byte barrelBtnPin = 2;
const byte coolingBtnPin = 3;

const byte coolingRelayPin = 8;
bool coolingState = false;

// for pause w/o delays
long myTimer = 0;
long myTimeout = 500;


// for defining types of barrels
struct FASS {
  int vol;
  String nameString;
};

// array of barreltypes
static FASS barrelArr[5] = {{15, "15 Liter"}, {20, "20 Liter"}, {25, "25 Liter"}, {30, "30 Liter"}, {50, "50 Liter"}};
// cursor that is used for chosing barrel type
static int barrelArrCur = 0;

static float EEPROM_total = EEPROM.get(0, EEPROM_total);
static float total = EEPROM_total;


static float initVol = EEPROM.get(sizeof(float), initVol); // use sizeof(float), because it's the address after total which is of type float
static float beerLeft = barrelArr[barrelArrCur].vol - initVol;


void setup()
{
  Serial.begin(9600);

  bool nullen = false;
  if (nullen) {
    beerLeft = barrelArr[barrelArrCur].vol;
    total = 0;
    EEPROM.put(0, total);
    EEPROM.put(4, beerLeft);
  }


  /*** LCD Init ***/
  lcd.init();

  pinMode(coolingRelayPin, OUTPUT);
  pinMode(barrelBtnPin, INPUT);
  pinMode(coolingBtnPin, INPUT);

  LoadCell.begin();
  LoadCell.start(2000);
  LoadCell.setCalFactor(27.0);

  lcd.backlight();
  LoadCell.tare();
  lcd.clear();
  // EEPROM_total = EEPROM.get(0, EEPROM_total);
  EEPROM.put(sizeof(float), 0);
}


void hardReset() {
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print(String("RESET"));
  // warten bis beide buttons losgelassen sind
  while (analogRead(barrelBtnPin) < 500 || analogRead(coolingBtnPin) < 500) {}
  lcd.setCursor(1, 0);
  lcd.print(String("Einstellung:"));
  lcd.setCursor(1, 3);
  lcd.print(String("(Kuehlung = OK)"));
  lcd.setCursor(5, 1);
  lcd.print(String("Fass: ") + (String(barrelArr[barrelArrCur].vol)) + String(" Liter"));
  bool blinking = false;
  myTimer = millis();
  while (1) {
    if (millis() > myTimeout + myTimer) {
      myTimer = millis();
      lcd.setCursor(5, 1);
      if (blinking) {
        lcd.print(String("Fass: ") + (String(barrelArr[barrelArrCur].vol)) + String(" Liter"));
        blinking = false;
      }
      else {
        blinking = true;
      }
    }
    // if barrelBtn pressed
    if (analogRead(barrelBtnPin) < 500) {
      while (analogRead(barrelBtnPin) < 500) {}
      barrelArrCur = (barrelArrCur + 1) % (sizeof(barrelArr) / sizeof(barrelArr[0]));
    }
    if (analogRead(coolingBtnPin) < 500) {
      beerLeft = barrelArr[barrelArrCur].vol;
      initVol = barrelArr[barrelArrCur].vol;

      EEPROM.put(0, total);
      EEPROM_total = EEPROM.get(0, EEPROM_total);

      LoadCell.tare();
      break;
    }
  }
}


// changes barrel type
void barrelBtnPushed() {
  // so lange barrelBtn gedrückt
  myTimer = millis();
  while (analogRead(barrelBtnPin) < 500) {
    // wenn coolingBtn gedrückt
    if (analogRead(coolingBtnPin) < 500) {
      if (myTimer + 2000 >= millis() && analogRead(barrelBtnPin) < 500) {
        hardReset();
      }
      while (analogRead(barrelBtnPin) < 500) {}
      while (analogRead(coolingBtnPin) < 500) {}
    }
  }
}



// toggles relay for cooling
void coolingBtnPushed() {
  myTimer = millis();
  // so lange coolingBtn gedrückt
  while (analogRead(coolingBtnPin) < 500) {
    if (analogRead(barrelBtnPin) > 500 && myTimer + 150 > millis()) {
      coolingState = !coolingState;
      digitalWrite(coolingRelayPin, coolingState ? HIGH : LOW);
      while (analogRead(coolingBtnPin) < 500) {}
      return;
    }
    // wenn barrelBtn gedrückt
    if (analogRead(barrelBtnPin) < 500) {
      if (myTimer + 2000 >= millis() && analogRead(barrelBtnPin) < 500) {
        hardReset();
      }
      while (analogRead(barrelBtnPin) < 500) {}
      while (analogRead(coolingBtnPin) < 500) {}
    }
  }
}


// refreshes data on LCD, make sure to measure beforehand
void printLcd() {
  lcd.setCursor(1, 0);
  lcd.print(String("Fass-Art: ") + (String(barrelArr[barrelArrCur].vol)) + String(" Liter"));
  lcd.setCursor(5, 1);
  lcd.print(String("Rest: ") + String(beerLeft) + String(" L "));
  lcd.setCursor(2, 2);
  if (total < 10)
    lcd.print(String("  Total: 0") + total + String(" L  "));
  else
    lcd.print(String("  Total: ") + total + String(" L   "));
  lcd.setCursor(1, 3);
  if (coolingState)
    lcd.print(String("Kuehlung: AN   "));
  else
    lcd.print(String("Kuehlung: AUS  "));
}

// read scale and calculates how much beer is left
void getBeer() {
  LoadCell.update();
  currentWeight = LoadCell.getData();
  currentWeight = currentWeight / 1000;
  beerLeft = initVol + currentWeight*beerDensity;
  total = barrelArr[barrelArrCur].vol - beerLeft + EEPROM_total;
  if (total < 0)
    total = 0;
  if (beerLeft > barrelArr[barrelArrCur].vol)
    beerLeft = barrelArr[barrelArrCur].vol;
  if (beerLeft < 0)
    beerLeft = 0;
}

void loop() {
  getBeer();
  printLcd();
  if (millis() > myTimer + 10000) {
    myTimer = millis();
    // EEPROM.put(0, total);
    EEPROM.put(0 + sizeof(float), beerLeft);
  }

  if (analogRead(barrelBtnPin) < 500)
    barrelBtnPushed();

  if (analogRead(coolingBtnPin) < 500)
    coolingBtnPushed();
}
