#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Wire.h>

#define PIN_L293D_EN1 3
#define PIN_L293D_IN1 4
#define PIN_L293D_IN2 5
#define OCR0A_VALUE 199
#define OCR2A_VALUE 78
#define OCR2B_VALUE 0
#define SIZE_LCD_X 16
#define SIZE_LCD_Y 2
#define LCD_RS 8
#define LCD_EN 9
#define LCD_D4 10
#define LCD_D5 11
#define LCD_D6 12
#define LCD_D7 13
#define PULSE_NUMBER 2

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

const long MAX_COUNT_PWM = OCR2A_VALUE;
const float PERIOD = 0.0001;
const float PRESCALER = 256;
const float F_ARDUINO = 16000000;
const float FREQ_PWM = 1 / ((PRESCALER / F_ARDUINO) * 2.0 * (float)OCR2A_VALUE);
volatile long counterPwm = 0;
volatile long counter = 0;
volatile long actualSpeed = 0;
volatile bool isClockwise = false;
String command = "";
String LCD_DISPLAY_ROT_1 = "ROTACAO:";
String LCD_DISPLAY_ROT_2 = " RPM";
String LCD_DISPLAY_ESTIMATIVE = "  (ESTIMATIVA)  ";

int getVelocityByPercentage(long percent)
{
    const long MAX_PERCENT = 100;
    return (int)map(percent, 0, MAX_PERCENT, 0, MAX_COUNT_PWM);
}

String readCommand()
{
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '*')
        {
            String tmpCommand = command;
            command = "";
            return tmpCommand;
        }
        else
        {
            command += c;
        }
    }
    return "";
}

void setSpeed(long speed)
{
    OCR2B = getVelocityByPercentage(speed);
}

void stop()
{
    digitalWrite(PIN_L293D_IN1, LOW);
    digitalWrite(PIN_L293D_IN2, LOW);
    setSpeed(0);
}

void setClockwise()
{
    digitalWrite(PIN_L293D_IN1, LOW);
    digitalWrite(PIN_L293D_IN2, HIGH);
    isClockwise = true;
}

void setAntiClockwise()
{
    digitalWrite(PIN_L293D_IN1, HIGH);
    digitalWrite(PIN_L293D_IN2, LOW);
    isClockwise = false;
}

void setVent()
{
    if (!isClockwise)
    {
        stop();
    }
    setClockwise();
}

void setExaust()
{
    if (isClockwise)
    {
        stop();
    }
    setAntiClockwise();
}

void cleanCounters()
{
    cli();
    counter = 0;
    counterPwm = 0;
    sei();
}


float getFrequency()
{
    float counter1_aux = (float)counter;
    float counter2_aux = (float)counterPwm;
    if (counter >= 20000)
    {
        cleanCounters();
    }
    return (counter1_aux / counter2_aux) * FREQ_PWM * 60.0;
}

void executeCommand(String cmd)
{
    if (cmd == "")
    {
        Serial.println("ERRO: COMANDO INEXISTENTE");
        return;
    }

    if (cmd.startsWith("VEL"))
    {
        if (cmd.length() <= 3)
        {
            Serial.println("ERRO: PARÂMETRO AUSENTE");
            return;
        }
        int speed = cmd.substring(3).toInt();
        if (speed >= 0 && speed <= 100)
        {
            setSpeed(speed);
            actualSpeed = speed;
            Serial.println("OK VEL " + String(speed) + "%");
        }
        else
        {
            Serial.println("ERRO: PARÂMETRO INCORRETO");
        }
    }
    else if (cmd == "VENT")
    {
        setVent();
        setSpeed(actualSpeed);
        Serial.println("OK VENT");
    }
    else if (cmd == "EXAUST")
    {
        setExaust();
        setSpeed(actualSpeed);
        Serial.println("OK EXAUST");
    }
    else if (cmd == "PARA")
    {
        stop();
        actualSpeed = 0;
        Serial.println("OK PARA");
    }
    else if (cmd == "RETVEL")
    {
        Serial.println("VEL: " + String(getFrequency()) + " RPM");
    }
    else
    {
        Serial.println("ERRO: COMANDO INEXISTENTE");
    }
}

void printLCD(int frequency)
{
    lcd.clear();
    lcd.home();
    if (frequency > 999)
    {
        lcd.print(LCD_DISPLAY_ROT_1 + String(frequency) + LCD_DISPLAY_ROT_2);
    }
    else if(frequency > 99)
    {
        lcd.print(LCD_DISPLAY_ROT_1 + " " + String(frequency) + LCD_DISPLAY_ROT_2);
    }
    else if(frequency > 9)
    {
        lcd.print(LCD_DISPLAY_ROT_1 + "  " + String(frequency) + LCD_DISPLAY_ROT_2);
    }
    else
    {
        lcd.print(LCD_DISPLAY_ROT_1 + "   " + String(frequency) + LCD_DISPLAY_ROT_2);
    }
    lcd.setCursor(0, 1);
    lcd.print(LCD_DISPLAY_ESTIMATIVE);
}


void setupLCD()
{
    lcd.begin(SIZE_LCD_X, SIZE_LCD_Y);
    printLCD(0);
}

void setupTemp0()
{
    TCCR0A &= 0b00001100;
    TCCR0A |= 0b00000010;

    OCR0A = OCR0A_VALUE;

    TCCR0B &= 0b00110000;
    TCCR0B |= 0b00000010;

    TIMSK0 &= 0b11111000;
    TIMSK0 |= 0b00000010;
}

void setupTemp1()
{
    OCR2A = OCR2A_VALUE;
    OCR2B = OCR2B_VALUE;
    TIMSK2 &= 0b11111000;

    TCCR2B = 0b00001110;

    TCCR2A = 0b00100001;

    DDRD |= 0b00001000;
}


void setupTemps()
{
    setupTemp1();
    setupTemp0();
}

void incrementCounter()
{
    counterPwm++;
}

void setupPins()
{
    pinMode(2, INPUT);
    attachInterrupt(digitalPinToInterrupt(2), incrementCounter, RISING);
    pinMode(PIN_L293D_IN1, OUTPUT);
    pinMode(PIN_L293D_IN2, OUTPUT);
    digitalWrite(PIN_L293D_IN1, LOW);
    digitalWrite(PIN_L293D_IN2, LOW);
    OCR2B = getVelocityByPercentage(0);
}

ISR(TIMER0_COMPA_vect)
{
    counter++;
}


void setupWire()
{
    Wire.begin();
}

void sendWireInfo(int displayBytes)
{
    Wire.beginTransmission(0x20);
    Wire.write(displayBytes);
    Wire.endTransmission();
}

void sevenSegDisplay()
{
    int displayId = 4;
    int frequency = round(getFrequency());
    while (displayId--)
    {
        printLCD(frequency);
        int addressesDisplay7seg[4] = {0xE0, 0xD0, 0xB0, 0x70};
        int possiblePowers[4] = {1, 10, 100, 1000};
        int divisor = possiblePowers[displayId];
        int display = ((int)(frequency / divisor)) % 10;
        int displayMem = addressesDisplay7seg[displayId];
        int displayBytes = displayMem | display;
        sendWireInfo(displayBytes);
    }
}

void setup()
{
    cli();
    Serial.begin(9600);
    setupWire();
}

void loop()
{
    String cmd = readCommand();
    if (cmd != "")
    {
        executeCommand(cmd);
    }
    if (counter > 2000)
    {
        sevenSegDisplay();
    }
}