#include <Wire.h>
#include <ESP32Servo.h>

// ==========================================
// CONFIGURAÇÃO DE PINOS (Conforme Esquema EasyEDA)
// ==========================================

// --- MOTORES (L298N) ---
// Lado Esquerdo (Motores 1 e 3)
const int ENA = 13; 
const int IN1 = 12; 
const int IN2 = 14;

// Lado Direito (Motores 2 e 4)
const int ENB = 19; 
const int IN3 = 4;  
const int IN4 = 2;  

// --- SENSORES INFRAVERMELHOS (TCRT5000) ---
const int IR_FRENTE_ESQ = 33; 
const int IR_FRENTE_DIR = 25; 
const int IR_TRAS_ESQ   = 26; 
const int IR_TRAS_DIR   = 27; 

// --- ULTRASSÓNICO (HC-SR04) ---
const int PIN_TRIG = 5;
const int PIN_ECHO = 18;

// --- SERVOMOTOR (Pescoço do Radar) ---
Servo pescoco;
const int PIN_SERVO = 23;

// ==========================================
// VARIÁVEIS DE CONTROLO DE VELOCIDADE
// ==========================================
int velocidadeNormal = 130; // Velocidade base para o plano (0-255)
int velocidadeRampa  = 230; // Binário extra para subir a rampa de 10cm
int velocidadeAtual  = 130;

// Estados de Condução Autónoma
enum Estados { SEGUIR_LINHA_FRENTE, EVITAR_PAREDE, SEGUIR_LINHA_TRAS };
Estados estadoAtual = SEGUIR_LINHA_FRENTE;

// ==========================================
// INICIALIZAÇÃO DO SISTEMA
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // Comunicação I2C para o Giroscópio MPU-6050 (Padrão: SDA=21, SCL=22)
  Wire.begin(21, 22); 
  
  // Configurar Pinos dos Motores
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  
  // Configurar Pinos dos Sensores IR
  pinMode(IR_FRENTE_ESQ, INPUT); pinMode(IR_FRENTE_DIR, INPUT);
  pinMode(IR_TRAS_ESQ, INPUT);   pinMode(IR_TRAS_DIR, INPUT);
  
  // Configurar Ultrassónico
  pinMode(PIN_TRIG, OUTPUT); 
  pinMode(PIN_ECHO, INPUT);
  
  // Configurar Servo
  pescoco.attach(PIN_SERVO);
  pescoco.write(90); // Inicializa a olhar para a frente (90 graus)
  
  inicializarMPU();
  delay(500);
}

// ==========================================
// LOOP PRINCIPAL (EXECUÇÃO EM TEMPO REAL)
// ==========================================
void loop() {
  // 1. O giroscópio avalia a inclinação e ajusta a potência em background
  verificarRampa();

  // 2. Execução da Máquina de Estados de Condução
  switch (estadoAtual) {
    
    case SEGUIR_LINHA_FRENTE:
      // Se detetar parede a menos de 18cm, pára e ativa o radar
      if (calcularDistancia() < 18) { 
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
      logicaLinhaTras();
      break;
  }
  
  delay(10); // Pequena estabilização do ciclo
}

// ==========================================
// SUB-SISTEMA 1: GIROSCÓPIO / ACELERÓMETRO
// ==========================================
void inicializarMPU() {
  Wire.beginTransmission(0x68); // Endereço I2C do MPU-6050
  Wire.write(0x6B);             // Registo de gestão de energia
  Wire.write(0);                // Desperta o chip
  Wire.endTransmission();
}

void verificarRampa() {
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);             // Começa a ler a partir do sensor de aceleração X
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6, true);
  
  int16_t AcX = Wire.read() << 8 | Wire.read();
  int16_t AcY = Wire.read() << 8 | Wire.read();
  int16_t AcZ = Wire.read() << 8 | Wire.read();

  // Calcular a inclinação em graus no eixo Y (Pitch)
  float inclinacao = atan2(AcY, AcZ) * 180 / PI;

  // Se detetar uma inclinação superior a 15 graus, aumenta a força para subir a rampa
  if (abs(inclinacao) > 15.0) { 
    velocidadeAtual = velocidadeRampa;
    Serial.println("Rampa detetada! Modo High-Torque ativo.");
  } else {
    velocidadeAtual = velocidadeNormal;
  }
}

// ==========================================
// SUB-SISTEMA 2: RADAR ULTRASSÓNICO (Mapeamento de Paredes)
// ==========================================
int calcularDistancia() {
  digitalWrite(PIN_TRIG, LOW); 
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); 
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duracao = pulseIn(PIN_ECHO, HIGH, 30000); // Timeout de 30ms para não bloquear o código
  int dist = duracao * 0.034 / 2;
  
  return (dist == 0) ? 999 : dist; // Retorna um valor alto se falhar o pulso
}

void logicaRadarEvasao() {
  // 1. Move o pescoço para a Direita e mede
  pescoco.write(30); 
  delay(400);
  int distDireita = calcularDistancia();
  
  // 2. Move o pescoço para a Esquerda e mede
  pescoco.write(150); 
  delay(500);
  int distEsquerda = calcularDistancia();
  
  // 3. Centraliza o sensor novamente
  pescoco.write(90); 
  delay(300);
  
  // Tomada de Decisão com Direção Diferencial
  if (distEsquerda > distDireita && distEsquerda > 20) {
    rodarParaEsquerda(); // Gira sobre o próprio eixo
    delay(700);          // Tempo para desviar da parede
  } else if (distDireita > distEsquerda && distDireita > 20) {
    rodarParaDireita();
    delay(700);
  } else {
    // Sem saída em ambos os lados? Aciona marcha-atrás assistida pelos sensores de trás
    estadoAtual = SEGUIR_LINHA_TRAS;
  }
  
  if (estadoAtual != SEGUIR_LINHA_TRAS) {
    estadoAtual = SEGUIR_LINHA_FRENTE; // Retoma seguimento frontal se saiu do bloqueio
  }
}

// ==========================================
// SUB-SISTEMA 3: SEGUIMENTO DE LINHA POR INFRAVERMELHOS
// ==========================================
void logicaLinhaFrente() {
  int esq = digitalRead(IR_FRENTE_ESQ);
  int dir = digitalRead(IR_FRENTE_DIR);
  
  // Ambos no preto -> Linha centrada, avança reto
  if (esq == 1 && dir == 1) {
    andarFrente();
  } 
  // Só o esquerdo deteta preto -> Carro a fugir para a direita, corrige para a esquerda
  else if (esq == 1 && dir == 0) {
    virarEsquerdaSuave();
  } 
  // Só o direito deteta preto -> Carro a fugir para a esquerda, corrige para a direita
  else if (esq == 0 && dir == 1) {
    virarDireitaSuave();
  } 
  // Se perder a linha (chão claro), mantém em frente com velocidade moderada à procura dela
  else {
    andarFrente(); 
  }
}

void logicaLinhaTras() {
  int esq = digitalRead(IR_TRAS_ESQ);
  int dir = digitalRead(IR_TRAS_DIR);
  
  // Mantém a marcha-atrás guiada se os sensores traseiros lerem a linha preta
  if (esq == 1 && dir == 1) {
    andarTras();
  }
  
  // Se recuou o suficiente e a parede frontal está a mais de 25cm, regressa à marcha normal
  if (calcularDistancia() > 25) { 
    pararMotores();
    estadoAtual = SEGUIR_LINHA_FRENTE;
  }
}

// ==========================================
// MOVIMENTOS FÍSICOS (Controlo de Direção Skid-Steering)
// ==========================================
void andarFrente() {
  analogWrite(ENA, velocidadeAtual); 
  analogWrite(ENB, velocidadeAtual);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  // Esquerda Avança
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  // Direita Avança
}

void andarTras() {
  analogWrite(ENA, velocidadeNormal); 
  analogWrite(ENB, velocidadeNormal);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); // Esquerda Recua
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); // Direita Recua
}

void virarEsquerdaSuave() {
  // Roda a esquerda mais devagar que a direita para curvar suavemente
  analogWrite(ENA, velocidadeAtual / 3); 
  analogWrite(ENB, velocidadeAtual);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void virarDireitaSuave() {
  // Roda a direita mais devagar que a esquerda para curvar suavemente
  analogWrite(ENA, velocidadeAtual); 
  analogWrite(ENB, velocidadeAtual / 3);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void rodarParaEsquerda() {
  // Inversão total de eixos (gira no próprio ponto para desvios de emergência)
  analogWrite(ENA, velocidadeNormal); 
  analogWrite(ENB, velocidadeNormal);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); // Esquerda para trás
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  // Direita para a frente
}

void rodarParaDireita() {
  // Inversão total de eixos (gira no próprio ponto para desvios de emergência)
  analogWrite(ENA, velocidadeNormal); 
  analogWrite(ENB, velocidadeNormal);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  // Esquerda para a frente
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); // Direita para trás
}

void pararMotores() {
  analogWrite(ENA, 0); 
  analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}