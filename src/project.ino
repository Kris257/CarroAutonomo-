#include <Wire.h>
#include <ESP32Servo.h> // Biblioteca padrão para Servos no ESP32

// ==========================================
// CONFIGURAÇÃO DE PINOS (Ajusta se necessário)
// ==========================================
// Motores (Lado Esquerdo)
const int ENA = 13; const int IN1 = 12; const int IN2 = 14;
// Motores (Lado Direito)
const int ENB = 19; const int IN3 = 16; const int IN4 = 17; // Pinos exemplo (visto que D4 e D2 estão no Ultrassónico)

// Sensores Infravermelhos (TCRT5000)
const int IR_FRENTE_ESQ = 26;
const int IR_FRENTE_DIR = 25;
const int IR_TRAS_ESQ   = 33;
const int IR_TRAS_DIR   = 32;

// Sensor Ultrassónico
const int PIN_TRIG = 4;
const int PIN_ECHO = 2;

// Servomotor (Pescoço do Radar)
Servo pescoço;
const int PIN_SERVO = 5;

// ==========================================
// VARIÁVEIS DE CONTROLO
// ==========================================
int velocidadeNormal = 120; // Velocidade em piso plano
int velocidadeRampa  = 220; // Super binário para vencer a subida de 10cm
int velocidadeAtual  = 120;

// Estados do Robô
enum Estados { SEGUIR_LINHA_FRENTE, EVITAR_PAREDE, SEGUIR_LINHA_TRAS };
Estados estadoAtual = SEGUIR_LINHA_FRENTE;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // Inicializa I2C para o MPU-6050 (SDA=21, SCL=22)
  
  // Configurar Motores
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  
  // Configurar IRs
  pinMode(IR_FRENTE_ESQ, INPUT); pinMode(IR_FRENTE_DIR, INPUT);
  pinMode(IR_TRAS_ESQ, INPUT);   pinMode(IR_TRAS_DIR, INPUT);
  
  // Configurar Ultrassónico e Servo
  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_ECHO, INPUT);
  pescoço.attach(PIN_SERVO);
  pescoço.write(90); // Olhar em frente por padrão
  
  inicializarMPU();
}

void loop() {
  // 1. O GIROSCÓPIO DOSA A POTÊNCIA (Funciona em background)
  verificarRampa();

  // 2. MÁQUINA DE ESTADOS AUTÓNOMA
  switch (estadoAtual) {
    
    case SEGUIR_LINHA_FRENTE:
      // Verifica primeiro se há uma parede à frente
      if (calcularDistancia() < 15) { // Parede a menos de 15cm
        pararMotores();
        estadoAtual = EVITAR_PAREDE;
      } else {
        logicaLinhaFrente();
      }
      break;
      
    case EVITAR_PAREDE:
      logicaRadarEvasao();
      break;
      
    case SEGUIR_LINHA_TRAS:
      // Se necessário recuar por estar encurralado
      logicaLinhaTras();
      break;
  }
}

// ==========================================
// 1. SUB-SISTEMA: GIROSCÓPIO (MPU-6050)
// ==========================================
void inicializarMPU() {
  Wire.beginTransmission(0x68);
  Wire.write(0x6B); // Registro de gestão de energia
  Wire.write(0);    // Desperta o MPU-6050
  Wire.endTransmission();
}

void verificarRampa() {
  Wire.beginTransmission(0x68);
  Wire.write(0x3B); // Começa a ler os dados do acelerómetro
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6, true);
  
  int16_t AcX = Wire.read() << 8 | Wire.read();
  int16_t AcY = Wire.read() << 8 | Wire.read();
  int16_t AcZ = Wire.read() << 8 | Wire.read();

  // Calcular a inclinação em graus aproximados baseados no eixo Y ou Z
  float inclinacao = atan2(AcY, AcZ) * 180 / PI;

  if (abs(inclinacao) > 15.0) { // Se inclinar mais de 15 graus (Rampa!)
    velocidadeAtual = velocidadeRampa;
    Serial.println("Rampa detetada! Injetando potência máxima.");
  } else {
    velocidadeAtual = velocidadeNormal;
  }
}

// ==========================================
// 2. SUB-SISTEMA: RADAR ULTRASSÓNICO + SERVO
// ==========================================
int calcularDistancia() {
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duracao = pulseIn(PIN_ECHO, HIGH, 30000); // Timeout de 30ms
  int dist = duracao * 0.034 / 2;
  return (dist == 0) ? 999 : dist; // Evita falsos zeros
}

void logicaRadarEvasao() {
  // Parado, o robô roda o pescoço para mapear o ambiente
  pescoço.write(30); // Olha para a Direita
  delay(400);
  int distDireita = calcularDistancia();
  
  pescoço.write(150); // Olha para a Esquerda
  delay(500);
  int distEsquerda = calcularDistancia();
  
  pescoço.write(90); // Volta ao centro
  delay(300);
  
  // Decide para onde virar baseado em onde há mais espaço livre
  if (distEsquerda > distDireita && distEsquerda > 20) {
    rodarParaEsquerda();
    delay(800); // Tempo necessário para girar e sair da rota da parede
  } else if (distDireita > distEsquerda && distDireita > 20) {
    rodarParaDireita();
    delay(800);
  } else {
    // Se ambos os lados estiverem bloqueados, o carro está num beco sem saída. Ativa a marcha-atrás!
    estadoAtual = SEGUIR_LINHA_TRAS;
  }
  
  if (estadoAtual != SEGUIR_LINHA_TRAS) {
    estadoAtual = SEGUIR_LINHA_FRENTE; // Volta a navegar para a frente
  }
}

// ==========================================
// 3. SUB-SISTEMA: SEGUIMENTO DE LINHA
// ==========================================
void logicaLinhaFrente() {
  int esq = digitalRead(IR_FRENTE_ESQ);
  int dir = digitalRead(IR_FRENTE_DIR);
  
  if (esq == 1 && dir == 1) {
    andarFrente();
  } else if (esq == 1 && dir == 0) {
    virarEsquerdaSuave();
  } else if (esq == 0 && dir == 1) {
    virarDireitaSuave();
  } else {
    andarFrente(); // Se perder a linha, continua em frente à procura
  }
}

void logicaLinhaTras() {
  int esq = digitalRead(IR_TRAS_ESQ);
  int dir = digitalRead(IR_TRAS_DIR);
  
  // Se os sensores de trás detetarem fita preta, ele guia-se em marcha-atrás
  if (esq == 1 && dir == 1) {
    andarTras();
  } // Se bater noutra parede ou recuperar espaço seguro, voltamos ao loop frontal:
  if (calcularDistancia() > 25) { 
    pararMotores();
    estadoAtual = SEGUIR_LINHA_FRENTE;
  }
}

// ==========================================
// DRIVERS DE MOVIMENTO (DIREÇÃO DIFERENCIAL)
// ==========================================
void andarFrente() {
  analogWrite(ENA, velocidadeAtual); analogWrite(ENB, velocidadeAtual);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void andarTras() {
  analogWrite(ENA, velocidadeNormal); analogWrite(ENB, velocidadeNormal);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void virarEsquerdaSuave() {
  analogWrite(ENA, velocidadeAtual / 3); analogWrite(ENB, velocidadeAtual);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void virarDireitaSuave() {
  analogWrite(ENA, velocidadeAtual); analogWrite(ENB, velocidadeAtual / 3);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void rodarParaEsquerda() { // Roda sobre o próprio eixo
  analogWrite(ENA, velocidadeNormal); analogWrite(ENB, bandwidth);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void rodarParaDireita() { // Roda sobre o próprio eixo
  analogWrite(ENA, velocidadeNormal); analogWrite(ENB, velocidadeNormal);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}

void pararMotores() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}