#include <Arduino.h>

String command = "";

String readCommand()
{
  String s = "";
  while (Serial.available())
  {
    s = Serial.readString();
  }
  return s;
}

void tratativaVelocidade(String cmd)
{

  if (cmd.length() <= 3)
  {
    Serial.println("ERRO: PARÂMETRO AUSENTE");
    return;
  }
  int speed = cmd.substring(3).toInt();
  if (speed >= 0 && speed <= 100)
  {
    Serial.println("OK VEL " + String(speed) + "%");
  }
  else
  {
    Serial.println("ERRO: PARÂMETRO INCORRETO");
  }
}

void printaVent()
{
  Serial.println("OK VENT");
}

void printaExaust()
{
  Serial.println("OK EXAUST");
}

void printaPara()
{
  Serial.println("OK PARA");
}

void leEPrintaRetVel()
{
  // Implementação da lógica para retornar a velocidade atual em RPM
  // Supondo que X seja a velocidade atual
  int X = analogRead(A0); // Exemplo de leitura da velocidade atual
  Serial.println("VEL: " + String(X) + " RPM");
}

void printaErroPadrao() 
{
Serial.println("ERRO: COMANDO INEXISTENTE");
}

void executeCommand(String cmd)
{
  if (cmd.startsWith("VEL"))
  {
    tratativaVelocidade(cmd);
    return;
  }
  else if (cmd == "VENT")
  {
    printaVent();
    return;
  }
  else if (cmd == "EXAUST")
  {
    printaExaust();
    return;
  }
  else if (cmd == "PARA")
  {
    printaPara();
    return;
  }
  else if (cmd == "RETVEL")
  {
    leEPrintaRetVel();
  }
  else
  {
    printaErroPadrao();
  }
}

void setup()
{
  Serial.begin(9600);
}

void loop()
{
  String cmd = readCommand();
  if (cmd != "")
  {
    executeCommand(cmd);
  }
}
