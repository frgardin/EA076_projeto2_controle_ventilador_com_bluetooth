
#include <LiquidCrystal.h>
#include <avr/io.h>
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
volatile long counterPwm = 0;
volatile long counter = 0;
volatile long actualSpeed = 0;
volatile bool isClockwise = false;
volatile int displayId = 0;
String command = "";

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

void stop()
{
    digitalWrite(PIN_L293D_IN1, LOW);
    digitalWrite(PIN_L293D_IN2, LOW);
    setSpeed(0);
}

void setSpeed(long speed)
{
    OCR2B = getVelocityByPercentage(speed);
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

void printLCDVent()
{
    lcd.home();
    lcd.print("VENTILADOR      ");
}

void printLCDExaust()
{
    lcd.home();
    lcd.print("EXAUSTOR        ");
}

void printLCDStop()
{
    lcd.home();
    lcd.print("PARADO          ");
    lcd.setCursor(0, 1);
    lcd.print("VEL: 0%         ");
}

void printLCDVel(int speed)
{
    lcd.setCursor(0, 1);
    lcd.print("VEL: " + String(speed) + "%         ");
}

void cleanCounters()
{
    cli();
    counter = 0;
    counterPwm = 0;
    sei();
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
            printLCDVel(speed);
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
        printLCDVent();
        Serial.println("OK VENT");
    }
    else if (cmd == "EXAUST")
    {
        setExaust();
        setSpeed(actualSpeed);
        printLCDExaust();
        Serial.println("OK EXAUST");
    }
    else if (cmd == "PARA")
    {
        stop();
        printLCDStop();
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

void setupLCD()
{
    lcd.begin(SIZE_LCD_X, SIZE_LCD_Y);
    lcd.clear();
    lcd.home();
    lcd.print("PARADO");
    lcd.setCursor(0, 1);
    lcd.print("VEL: 0%");
}

void setupTemps()
{
    setupTemp1();
    setupTemp0();
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

void setupPins()
{
    pinMode(2, INPUT);
    attachInterrupt(digitalPinToInterrupt(2), incrementCounter, CHANGE);
    pinMode(PIN_L293D_IN1, OUTPUT);
    pinMode(PIN_L293D_IN2, OUTPUT);
    digitalWrite(PIN_L293D_IN1, LOW);
    digitalWrite(PIN_L293D_IN2, LOW);
    OCR2B = getVelocityByPercentage(0);
}

void incrementCounter()
{
    counterPwm++;
}

ISR(TIMER0_COMPA_vect)
{
    counter++;
}

float getFrequency()
{

    float counter1_aux = (float)counter;
    float counter2_aux = (float)counterPwm;
    if (counter >= 20000)
    {
        cleanCounters();
    }
    return (counter1_aux * 0.01 / (PULSE_NUMBER * PERIOD * counter2_aux)) * 60 / 2;
}

void setupWire()
{
    Wire.begin();
}

String getByteLsb(int divisor, String actualLsb)
{
    for (int i = 3; i >= 0; i--)
    {
        actualLsb += bitRead((round(getFrequency())) % divisor, i);
    }
    return actualLsb;
}

void sevenSegDisplay()
{
    String byteMsb = "1111";
    String byteLsb = "\0";
    if (!(counter % 2))
    {
        displayId = (displayId + 1) % 4;
        if (displayId == 0)
        {
            byteLsb = getByteLsb(1000, byteLsb);
            byteMsb = "0111";
        }
        else if (displayId == 1)
        {
            byteLsb = getByteLsb(100, byteLsb);
            byteMsb = "1011";
        }
        else if (displayId == 2)
        {
            byteLsb = getByteLsb(10, byteLsb);
            byteMsb = "1101";
        }
        else
        {
            byteLsb = getByteLsb(1, byteLsb);
            byteMsb = "1110";
        }
    }
    String display_bytes = byteMsb + byteLsb;
    int I2C_number = strtol(display_bytes.c_str(), NULL, 2);
    Wire.beginTransmission(0x20);
    Wire.write(I2C_number);
    Wire.endTransmission();
}

void setup()
{
    cli();
    Serial.begin(9600);
    setupWire();
    setupTemps();
    setupPins();
    setupLCD();
    sei();
}

void loop()
{
    String cmd = readCommand();
    if (cmd != "")
    {
        executeCommand(cmd);
    }
    sevenSegDisplay();
}
