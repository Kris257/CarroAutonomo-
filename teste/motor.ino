#include <ESP32Servo.h>

const int escPin = 13;
Servo myESC;

void setup() {
  Serial.begin(115200);
  
  
  myESC.attach(escPin, 1000, 2000);

  Serial.println("--- INICIALIZANDO ESC DE CARRO ---");
  
  
  Serial.println("Enviando sinal NEUTRO (90)...");
  myESC.write(90); 
  
  Serial.println("AGORA ligue a bateria do ESC (ou reinicie o botão do ESC).");
  Serial.println("Aguarde os bips de confirmação...");
  delay(7000); 
}
void loop() {
  Serial.println("Acelerando suavemente...");
  
  
  for(int velocidade = 90; velocidade <= 110; velocidade++) {
    myESC.write(velocidade);
    delay(3000); 
  }
  
  delay(2000);

  Serial.println("Parando suavemente...");
  for(int velocidade = 110; velocidade >= 90; velocidade--) {
    myESC.write(velocidade);
    delay(3000);
  }
  
  delay(4000);
}