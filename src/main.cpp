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
    const long MAX_PERCENT = 100; //define valor maximo
    const long REFERENCE = 0; // define valor minimo
    return (int)map(percent, REFERENCE, MAX_PERCENT, REFERENCE, MAX_COUNT_PWM); // retorna normalização entre a config do pwm e percentual
}

/**
 * Funçao que realiza a leitura de comando
 * @return retorna o comando enviado
 */
String readCommand()
{
    while (bluetooth.available()) //verifica se o comando está disponível
    {
        char c = bluetooth.read(); // le comando
        if (c == '*') //se caso o char for * armazena o comando e limpa variavel global comando
        {
            String tmpCommand = command;
            command = "";
            return tmpCommand; //retorna comando
        }
        else
        {
            command += c; //fica lendo comando até mandar estrela
        }
    }
    return ""; //retorna vazio caso n esteja disponivel
}

/**
 * Configura velocidade atribuindo o valor de velocidade ao registrador OCR2B
 * @param speed recebe parametro velocidade em percentual
 */
void setSpeed(long speed)
{
    OCR2B = getVelocityByPercentage(speed); //manda velocidade normalizada para o registrador OCR2B
}

/**
 * Funçao que para o motor "setando" os as duas entradas In1 e In2 em nivel logico baixo, e coloca o valor tambem no registrador OCR2B
 * 
 */
void stop()
{
    //desabilita as entradas 1 e 2 fazendo com que o motor fique parado
    digitalWrite(PIN_L293D_IN1, LOW);
    digitalWrite(PIN_L293D_IN2, LOW);
    setSpeed(0); // configura velocidade para 0
}

/**
 * Funçao que configura o sentido de rotaçao para horario, colocando a In2 em nivel logico alto e In1 em nivel logico baixo
 * 
 */
void setClockwise()
{
    //habilita a entrada 2 e desabilita a 1 fazendo com que o motor fique no sentido horario
    digitalWrite(PIN_L293D_IN1, LOW);
    digitalWrite(PIN_L293D_IN2, HIGH);
    isClockwise = true; // armazena estado atual de rotação
}


/**
 * Funçao que configura o sentido de rotaçao para anti-horario, colocando a In1 em nivel logico alto e In2 em nivel logico baixo
 * 
 */
void setAntiClockwise()
{
    //habilita a entrada 1 e desabilita a 2 fazendo com que o motor fique no sentido antihorario
    digitalWrite(PIN_L293D_IN1, HIGH);
    digitalWrite(PIN_L293D_IN2, LOW);
    isClockwise = false; // armazena estado atual de rotação
}

/**
 * Funçao que muda para o estado VENT, colocando o motor no sentido horario
 * 
 */
void setVent()
{
    if (!isClockwise) //se não estiver no sentido horario para o motor
    {
        stop();
    }
    setClockwise(); // chama função que configura o motor para girar no sentido horario
}

/**
 * Funçao que muda para o estado EXAUST, colocando o motor no sentido anti-horario
 * 
 */
void setExaust()
{
    if (isClockwise) //se estiver no sentido horario para motor
    {
        stop();
    }
    setAntiClockwise(); // chama função que configura o motor para girar no sentido antihorario
}


/**
 * Limpa os contadores de pulso
 */
void cleanCounters()
{
    cli(); // desabilita interruḉões 
    counter = 0; // limpa counter de ciclos de maquina
    counterPwm = 0; // limpa counter de ciclos do motor
    sei(); // habilita interrupções
}

/**
 * A partir do valor armazenado nos contadores, essa funçao e capaz de retornar a frequencia em rpm, onde por regra de 3, conseguimos definir a frequencia do sensor
 * @return retorna a frequencia em rpm
 */
float getFrequency()
{
    float counter1_aux = (float)counter;
    float counter2_aux = (float)counterPwm;
    if (counter >= 20000) // se caso for maior que 20000 ele limpa, apenas para pegar uma medida precisa e não pegar uma velocidade errada
    {
        cleanCounters();
    }
     // basicamente uma regra de tres, onde normaliza o contador de pulsos do pwm com o contador de tempo interno do arduino, divide pelo numero de pás que é igual a 2
     // e divide pela constante de tempo que é o período, e por fim multiplica por 60 para obter a velocidade em rpm
    return (counter2_aux / counter1_aux) * 60.0 / 2.0 / PERIOD;
}

/**
 * A partir do comando enviado, esta funcao trata erros e executa os comandos de acordo com sua chave
 * @param cmd recebe comando em formato de string
 * 
 */
void executeCommand(String cmd)
{
    if (cmd == "") // caso o comando venha vazio ele dá como comando inexistente
    {
        bluetooth.write("ERRO: COMANDO INEXISTENTE\n");
        return; // n continua a função
    }

    if (cmd.startsWith("VEL")) // se iniciar com VEL ele começa a trabalhar no ciclo de velocidade
    {
        if (cmd.length() <= 3) //verifica o tamanho da string para o erro de parametro ausente
        {
            bluetooth.write("ERRO: PARÂMETRO AUSENTE\n");
            return;
        }
        int speed = cmd.substring(3).toInt(); //captura valor numerico
        if (speed >= 0 && speed <= 100) // verifica range de velocidade
        {
            setSpeed(speed); // configura velocidade
            actualSpeed = speed; // armazena na variavel global a velocidade atual
            bluetooth.write(("OK VEL " + String(speed) + "%\n").c_str()); // manda pra serial a mensagem requirida
        }
        else
        {
            bluetooth.write("ERRO: PARÂMETRO INCORRETO\n"); // caso o parametro venha fora do range manda pra serial o erro de parametro incorreto
        }
    }
    else if (cmd == "VENT") // caso vent inicia função vent
    {
        setVent(); // muda o sentido de rotação
        setSpeed(actualSpeed); // configura velocidade
        bluetooth.write("OK VENT\n"); // manda mensagem de confirmação para o modulo bluetooth
    }
    else if (cmd == "EXAUST") // caso exaust inicia função exaust
    {
        setExaust(); // muda o sentido de rotação
        setSpeed(actualSpeed); // configura velocidade
        bluetooth.write("OK EXAUST\n"); // manda mensagem de confirmação para o modulo bluetooth
    }
    else if (cmd == "PARA") // caso para
    {
        stop(); // chama função para parar o ventilador
        actualSpeed = 0; // muda velocidade atual para zero
        bluetooth.write("OK PARA\n"); // manda mensagem de confirmação para o modulo bluetooth
    }
    else if (cmd == "RETVEL") // caso retvel
    {
        bluetooth.write(("VEL: " + String(getFrequency()) + " RPM\n").c_str()); // apenas manda para a serial com a velocidade atual do sistema
    }
    else
    {
        bluetooth.write("ERRO: COMANDO INEXISTENTE\n"); // caso o comando não seja enviado (string vazia), manda para serial esse erro
    }
}

/**
 * A partir da frequencia mostra no LCD a rotaçao e abaixo a string estimativa
 * @param frequency recebe frequencia
 */
void printLCD(int frequency)
{
    lcd.clear(); // limpa lcd
    lcd.home(); // seta cursor para posição 0,0
    if (frequency > 999) // caso maior q 999
    {
        lcd.print(LCD_DISPLAY_ROT_1 + String(frequency) + LCD_DISPLAY_ROT_2); //printa sem espaço
    }
    else if(frequency > 99) //caso maior q 99
    {
        lcd.print(LCD_DISPLAY_ROT_1 + " " + String(frequency) + LCD_DISPLAY_ROT_2); //printa com um espaço
    }
    else if(frequency > 9) //caso maior q 9
    {
        lcd.print(LCD_DISPLAY_ROT_1 + "  " + String(frequency) + LCD_DISPLAY_ROT_2); //printa com dois espaços
    }
    else
    {
        lcd.print(LCD_DISPLAY_ROT_1 + "   " + String(frequency) + LCD_DISPLAY_ROT_2); //printa com tres espaços
    }
    lcd.setCursor(0, 1); // seta cursor para a posição 0, 1 ou seja segunda linha do lcd
    lcd.print(LCD_DISPLAY_ESTIMATIVE); // printa estimativa
}

/**
 * função que realiza o setup do lcd
 * 
 */
void setupLCD()
{
    lcd.begin(SIZE_LCD_X, SIZE_LCD_Y); // configura tamanho do lcd, largura e altura, ou melhor, linhas e colunas
    printLCD(0); // manda valor zero inicial
}

/**
 * Função que configura temporizador zero
 */
void setupTemp0()
{
    TCCR0A &= 0b00001100; // limpa os bits presentes e mantem os valores reservados
    TCCR0A |= 0b00000010; // configura 10 habilitando modo ctc, onde uma função é acionada a cada interrupção

    OCR0A = OCR0A_VALUE; // armazena o valor de 199 para detectar 200 pulsos, que para um prescaler de 8, e a frequencia do arduino sendo 16MHz, significam um periodo de 0,1 ms

    TCCR0B &= 0b00110000; // mantem reservados e limpa o restante
    TCCR0B |= 0b00000010; // configura prescaler pra 8

    TIMSK0 &= 0b11111000; // mantem reservados e limpa o restante 
    TIMSK0 |= 0b00000010; // modo que compara com o registrador OCR0A
}

/**
 * Função que configura o temporizador um
 */
void setupTemp1()
{
    //seta os valores dos registradores OCR2A e OCR2B de modo que o periodo de que a saída do pwm fique com
    // periodo (prescaler / freqArduino) * 2 * OCR2A_VALUE que no nosso caso eh 256 * 2 * 78 / 16M = 2.5ms
    OCR2A = OCR2A_VALUE;
    OCR2B = OCR2B_VALUE;
    TIMSK2 &= 0b11111000; //mantem reservados e limpa restante

    TCCR2B = 0b00001110; // seta prescaler para 256

    TCCR2A = 0b00100001; //configura metodo de comparação com pinos OCR2A e OCR2B

    DDRD |= 0b00001000; //configura pino 3 como saída
}

/**
 * Apenas uma função para chamar a função setup de cada temporizador
 */
void setupTemps()
{
    setupTemp1();
    setupTemp0();
}

/**
 * Função acionada a cada borda de subida do sensor PHCT202, ou seja incrementa o counter q vai que vinha do motor
 */
void incrementCounter()
{
    counterPwm++;
}

/**
 * Configura a pinagem
 */
void setupPins()
{
    pinMode(2, INPUT); //entrada do circuito do sensor
    attachInterrupt(digitalPinToInterrupt(2), incrementCounter, RISING); //configura interrupção para modo RISING, subida do pulso no pino 2, chamando a função incrementCounter

    // essas entradas são responsaveis pelo sentido de rotação
    pinMode(PIN_L293D_IN1, OUTPUT); //configura como saida o pino do ci L293 para a entrada 1
    pinMode(PIN_L293D_IN2, OUTPUT); //configura como saida o pino do ci L293 para a entrada 2
    // inicia com ambas em nivel logico baixo
    digitalWrite(PIN_L293D_IN1, LOW);
    digitalWrite(PIN_L293D_IN2, LOW);
    // armazena no registrador responsavel pela velocidade o valor de zero
    OCR2B = getVelocityByPercentage(0);
}

/**
 * Função ativada pela interrupção que define o contador de tempo do sistema, o que é crucial para calcular a frequencia do motor
 */
ISR(TIMER0_COMPA_vect)
{
    counter++;
}

/**
 * inicializa biblioteca Wire
 */ 
void setupWire()
{
    Wire.begin();
}

/**
 * Função responsavel por enviar dados para os displays de 7 segmentos, onde os primeiros quatro bits
 * sao referentes a posição do display de 7 segmentos, e os ultimos 4 bits é o valor a ser mostrado no
 * display
 * @param displayBytes byte contendo as informações citadas acima
 */
void sendWireInfo(int displayBytes)
{
    Wire.beginTransmission(0x20); // inicia transmissão com protocolo i2C neste canal
    Wire.write(displayBytes); //escreve
    Wire.endTransmission(); //finaliza transmissão
}

/**
 * Função que cria e manda o byte para os 4 displays de sete segmentos 
 */
void sevenSegDisplay()
{
    int displayId = 4; // inicializa a variavel display id, onde os displays tem ids 0,1,2,3. Esta inicializa em 4 devido ao primeiro decrement no while
    int frequency = round(getFrequency()); // captura a frequencia e arredonda
    while (displayId--)
    {
        printLCD(frequency); //printa frequencia no lcd
        int addressesDisplay7seg[4] = {0xE0, 0xD0, 0xB0, 0x70}; // define array de posições de cada display de 7 segmentos
        int possiblePowers[4] = {1, 10, 100, 1000}; // define array de divisores 
        int divisor = possiblePowers[displayId]; // captura divisor referente de acordo com o display id
        int display = ((int)(frequency / divisor)) % 10; // captura e armazena algarismo referente, onde caso tenhamos a frequencia de 1234, e o divisor seja 1234, o algarismo 1 eh capturado
        int displayMem = addressesDisplay7seg[displayId]; // armazena posição referente ao displayId
        int displayBytes = displayMem | display; // concatena ambos os nibbles transformando num byte, onde os primeiros 4 digitos eh a posição do display e os ultimos 4 o valor a ser enviado
        sendWireInfo(displayBytes); // chama função que manda o byte para os displays de sete segmentos  
    }
}

/**
 * função setup padrao do arduino que chama todos os setups necessários
 */
void setup()
{
    cli(); //desabilita interrupções
    bluetooth.begin(BAUD_RATE); //inicializa bluetooth
    setupWire(); // inicializa wire
    setupTemps(); // inicializa temporizadores que configuram as interrupções
    setupPins(); // inicializa pinos
    setupLCD(); // inicializa lcd
    sei(); // habilita interrupções
}

/**
 * função loop padrao do arduino
 */
void loop()
{
    String cmd = readCommand(); // chama função que lê comando
    if (cmd != "") //caso o comando seja diferente de uma string vazia chama função que executa comando
    {
        executeCommand(cmd);
    }
    if (counter > 1000) //caso tenha se passado sem ciclos de relogio chama função que printa a frequencia do motorem rpm tanto no display lcd quanto no 7 segmentos
    {
        sevenSegDisplay();
    }
}