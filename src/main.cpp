// import libs necessarias
#include <Arduino.h>
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <Wire.h>

/*

Define variaveis globais

*/
#define PIN_BLUETOOTH_TX 0 // pino tx bluetooth hc-05
#define PIN_BLUETOOTH_RX 1 // pino rx bluetooth hc-05
#define BAUD_RATE 9600 // define baud rate

#define PIN_L293D_EN1 3 //pino enable l293d
#define PIN_L293D_IN1 4 //pino in1 l293d
#define PIN_L293D_IN2 5 //pino in2 l293d

#define OCR0A_VALUE 199 // valor do registrador OCROA que define a quantidade de pulsos a serem detectados
#define OCR2A_VALUE 78 // valor do registrador OCR2A que define o multiplicador do periodo na constante FREQ_PWM
#define OCR2B_VALUE 0 // valor do registrador OCR2B

#define SIZE_LCD_X 16 //tamanho x do display LCD
#define SIZE_LCD_Y 2 //tamanho y do display LCD

//define pinos de saida do display lcd
#define LCD_RS 8
#define LCD_EN 9
#define LCD_D4 10
#define LCD_D5 11
#define LCD_D6 12
#define LCD_D7 13


LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7); //inicializa LiquidCrystal 
SoftwareSerial bluetooth(PIN_BLUETOOTH_TX, PIN_BLUETOOTH_RX); //inicializa Serial Bluetooth 

const long MAX_COUNT_PWM = OCR2A_VALUE; //define o valor maximo do pwm

const float PERIOD = 0.0001; // armazena periodo do counter da interrupçao
const float PRESCALER = 256; // armazena prescaler
const float F_ARDUINO = 16000000; // armazena frequencia do arduino
const float FREQ_PWM = 1 / ((PRESCALER / F_ARDUINO) * 2.0 * (float)OCR2A_VALUE); //calcula frequencia do pwm

volatile long counterPwm = 0; // inicializa counter do pwm
volatile long counter = 0; // inicializa counter da interrupçao (utilizada basicamente para sincronia)

volatile long actualSpeed = 0; // armazena velocidade atual do motor
volatile bool isClockwise = false; // armazena sentido de rotacao do motor

// Variaveis globais do tipo string
String command = "";
String LCD_DISPLAY_ROT_1 = "ROTACAO:";
String LCD_DISPLAY_ROT_2 = " RPM";
String LCD_DISPLAY_ESTIMATIVE = "  (ESTIMATIVA)  ";

/**
 * A partir da percentagem extrair a velocidade
 * @param percent percentual requerido
 * @return a velocidade referente 
 */
int getVelocityByPercentage(long percent)
{
    const long MAX_PERCENT = 100;
    const long REFERENCE = 0;
    return (int)map(percent, REFERENCE, MAX_PERCENT, REFERENCE, MAX_COUNT_PWM);
}

/**
 * Funçao que realiza a leitura de comando
 * @return retorna o comando enviado
 */
String readCommand()
{
    while (bluetooth.available())
    {
        char c = bluetooth.read();
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

/**
 * Configura velocidade atribuindo o valor de velocidade ao registrador OCR2B
 * @param speed recebe parametro velocidade em percentual
 */
void setSpeed(long speed)
{
    OCR2B = getVelocityByPercentage(speed);
}

/**
 * Funçao que para o motor "setando" os as duas entradas In1 e In2 em nivel logico baixo, e coloca o valor tambem no registrador OCR2B
 * 
 */
void stop()
{
    digitalWrite(PIN_L293D_IN1, LOW);
    digitalWrite(PIN_L293D_IN2, LOW);
    setSpeed(0);
}

/**
 * Funçao que configura o sentido de rotaçao para horario, colocando a In2 em nivel logico alto e In1 em nivel logico baixo
 * 
 */
void setClockwise()
{
    digitalWrite(PIN_L293D_IN1, LOW);
    digitalWrite(PIN_L293D_IN2, HIGH);
    isClockwise = true;
}


/**
 * Funçao que configura o sentido de rotaçao para anti-horario, colocando a In1 em nivel logico alto e In2 em nivel logico baixo
 * 
 */
void setAntiClockwise()
{
    digitalWrite(PIN_L293D_IN1, HIGH);
    digitalWrite(PIN_L293D_IN2, LOW);
    isClockwise = false;
}

/**
 * Funçao que muda para o estado VENT, colocando o motor no sentido horario
 * 
 */
void setVent()
{
    if (!isClockwise)
    {
        stop();
    }
    setClockwise();
}

/**
 * Funçao que muda para o estado EXAUST, colocando o motor no sentido anti-horario
 * 
 */
void setExaust()
{
    if (isClockwise)
    {
        stop();
    }
    setAntiClockwise();
}


/**
 * Limpa os contadores de pulso
 */
void cleanCounters()
{
    cli();
    counter = 0;
    counterPwm = 0;
    sei();
}

/**
 * A partir do valor armazenado nos contadores, essa funçao e capaz de retornar a frequencia em rpm, onde por regra de 3, conseguimos definir a frequencia do sensor
 * @return retorna a frequencia em rpm
 */
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

/**
 * A partir do comando enviado, esta funcao trata erros e executa os comandos de acordo com sua chave
 * @param cmd recebe comando em formato de string
 * 
 */
void executeCommand(String cmd)
{
    if (cmd == "")
    {
        bluetooth.write("ERRO: COMANDO INEXISTENTE\n");
        return;
    }

    if (cmd.startsWith("VEL"))
    {
        if (cmd.length() <= 3)
        {
            bluetooth.write("ERRO: PARÂMETRO AUSENTE\n");
            return;
        }
        int speed = cmd.substring(3).toInt();
        if (speed >= 0 && speed <= 100)
        {
            setSpeed(speed);
            actualSpeed = speed;
            bluetooth.write(("OK VEL " + String(speed) + "%\n").c_str());
        }
        else
        {
            bluetooth.write("ERRO: PARÂMETRO INCORRETO\n");
        }
    }
    else if (cmd == "VENT")
    {
        setVent();
        setSpeed(actualSpeed);
        bluetooth.write("OK VENT\n");
    }
    else if (cmd == "EXAUST")
    {
        setExaust();
        setSpeed(actualSpeed);
        bluetooth.write("OK EXAUST\n");
    }
    else if (cmd == "PARA")
    {
        stop();
        actualSpeed = 0;
        bluetooth.write("OK PARA\n");
    }
    else if (cmd == "RETVEL")
    {
        bluetooth.write(("VEL: " + String(getFrequency()) + " RPM\n").c_str());
    }
    else
    {
        bluetooth.write("ERRO: COMANDO INEXISTENTE\n");
    }
}

/**
 * A partir da frequencia mostra no LCD a rotaçao e abaixo a string estimativa
 * @param frequency recebe frequencia
 */
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
    bluetooth.begin(BAUD_RATE);
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
    if (counter > 2000)
    {
        sevenSegDisplay();
    }
}