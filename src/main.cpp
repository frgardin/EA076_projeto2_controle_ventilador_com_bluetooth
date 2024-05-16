#include <Arduino.h>

String command = "";

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
      Serial.println("OK VEL " + String(speed) + "%");
    }
    else
    {
      Serial.println("ERRO: PARÂMETRO INCORRETO");
    }
  }
  else if (cmd == "VENT")
  {
    Serial.println("OK VENT");
  }
  else if (cmd == "EXAUST")
  {
    Serial.println("OK EXAUST");
  }
  else if (cmd == "PARA")
  {
    Serial.println("OK PARA");
  }
  else if (cmd == "RETVEL")
  {
    // Implementação da lógica para retornar a velocidade atual em RPM
    // Supondo que X seja a velocidade atual
    int X = analogRead(A0); // Exemplo de leitura da velocidade atual
    Serial.println("VEL: " + String(X) + " RPM");
  }
  else
  {
    Serial.println("ERRO: COMANDO INEXISTENTE");
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