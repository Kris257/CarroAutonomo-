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
// Configurados para detetar as linhas das margens esquerda e direita
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
int velocidadeRampa  = 230; // Torque máximo para subir a rampa de 10cm
int velocidadeAtual  = 130;

// Estados de Condução Autónoma
enum Estados { CONDUZIR_NA_PISTA, EVITAR_PAREDE, MARCHA_ATRAS_ASSISTIDA };
Estados estadoAtual = CONDUZIR_NA_PISTA;

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
  pescoco.write(90); // Inicializa a olhar em frente (centro da pista)
  
  inicializarMPU();
  delay(500);
}

// ==========================================
// LOOP PRINCIPAL (EXECUÇÃO EM TEMPO REAL)
// ==========================================
void loop() {
  // 1. O giroscópio avalia a inclinação e injeta potência se estiver na rampa
  verificarRampa();

  // 2. Máquina de Estados de Condução Autónoma
  switch (estadoAtual) {
    
    case CONDUZIR_NA_PISTA:
      // Verifica primeiro se há uma parede física à frente (Obstáculo)
      if (calcularDistancia() < 18) { 
        pararMotores();
        estadoAtual = EVITAR_PAREDE;
      } else {
        logicaManterNaPista();
      }
      break;
      
    case EVITAR_PAREDE:
      logicaRadarEvasao();
      break;
      
    case MARCHA_ATRAS_ASSISTIDA:
      logicaMarchaAtras();
      break;
  }
  
  delay(10); 
}

// ==========================================
// SUB-SISTEMA 1: COMPENSAÇÃO DE RAMPA (MPU-6050)
// ==========================================
void inicializarMPU() {
  Wire.beginTransmission(0x68); 
  Wire.write(0x6B);             
  Wire.write(0);                
  Wire.endTransmission();
}

void verificarRampa() {
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);             
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6, true);
  
  int16_t AcX = Wire.read() << 8 | Wire.read();
  int16_t AcY = Wire.read() << 8 | Wire.read();
  int16_t AcZ = Wire.read() << 8 | Wire.read();

  // Calcular a inclinação em graus (Eixo Pitch)
  float inclinacao = atan2(AcY, AcZ) * 180 / PI;

  // Se a inclinação passar dos 15 graus, ativa o modo de alta potência para a rampa
  if (abs(inclinacao) > 15.0) { 
    velocidadeAtual = velocidadeRampa;
    Serial.println("Rampa detetada! Força extra ativada.");
  } else {
    velocidadeAtual = velocidadeNormal;
  }
}

// ==========================================
// SUB-SISTEMA 2: RADAR DE EVASÃO (Ultrassónico + Servo)
// ==========================================
int calcularDistancia() {
  digitalWrite(PIN_TRIG, LOW); 
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); 
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duracao = pulseIn(PIN_ECHO, HIGH, 30000); 
  int dist = duracao * 0.034 / 2;
  
  return (dist == 0) ? 999 : dist; 
}

void logicaRadarEvasao() {
  // 1. Roda o pescoço para a Direita e mede a distância da parede
  pescoco.write(30); 
  delay(400);
  int distDireita = calcularDistancia();
  
  // 2. Roda o pescoço para a Esquerda e mede a distância da parede
  pescoco.write(150); 
  delay(500);
  int distEsquerda = calcularDistancia();
  
  // 3. Centraliza o radar novamente
  pescoco.write(90); 
  delay(300);
  
  // Escolhe o caminho que tiver mais espaço livre longe das paredes
  if (distEsquerda > distDireita && distEsquerda > 20) {
    rodarParaEsquerda(); // Gira sobre o próprio eixo
    delay(650);          
  } else if (distDireita > distEsquerda && distDireita > 20) {
    rodarParaDireita();  // Gira sobre o próprio eixo
    delay(650);
  } else {
    // Se ambos os lados tiverem paredes bloqueadas, entra em marcha-atrás autónoma
    estadoAtual = MARCHA_ATRAS_ASSISTIDA;
  }
  
  if (estadoAtual != MARCHA_ATRAS_ASSISTIDA) {
    estadoAtual = CONDUZIR_NA_PISTA; 
  }
}

// ==========================================
// SUB-SISTEMA 3: NAVEGAÇÃO ENTRE MARGENS (LÓGICA INVERSA)
// ==========================================
void logicaManterNaPista() {
  int esq = digitalRead(IR_FRENTE_ESQ);
  int dir = digitalRead(IR_FRENTE_DIR);
  
  // NOTA: Assume-se que '1' significa que o sensor pisou a fita da margem.
  // Se o teu sensor atuar ao contrário (0 na fita), altera as igualdades abaixo.

  // 1. Caminho Limpo: Ambos os sensores leem o chão livre do centro da pista
  if (esq == 0 && dir == 0) {
    andarFrente();
  } 
  // 2. Alerta Esquerdo: O carro aproximou-se demasiado da margem ESQUERDA. Foge para a DIREITA!
  else if (esq == 1 && dir == 0) {
    virarDireitaSuave();
  } 
  // 3. Alerta Direito: O carro aproximou-se demasiado da margem DIREITA. Foge para a ESQUERDA!
  else if (esq == 0 && dir == 1) {
    virarEsquerdaSuave();
  } 
  // 4. Emergência: Cruzamento de linhas ou erro. Trava por segurança.
  else if (esq == 1 && dir == 1) {
    pararMotores();
  }
}

void logicaMarchaAtras() {
  int esq = digitalRead(IR_TRAS_ESQ);
  int dir = digitalRead(IR_TRAS_DIR);
  
  // Recua mantendo-se longe das margens traseiras
  if (esq == 0 && dir == 0) {
    andarTras();
  } 
  // Se a traseira esquerda tocar na margem esquerda, compensa a direção ao recuar
  else if (esq == 1 && dir == 0) {
    // Ajusta a rotação para alinhar a traseira
    analogWrite(ENA, velocidadeNormal); analogWrite(ENB, velocidadeNormal / 2);
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  }
  else if (esq == 0 && dir == 1) {
    // Ajusta a rotação para alinhar a traseira
    analogWrite(ENA, velocidadeNormal / 2); analogWrite(ENB, velocidadeNormal);
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  }
  
  // Se a parede da frente já estiver a uma distância segura (mais de 25cm), regressa à marcha normal
  if (calcularDistancia() > 25) { 
    pararMotores();
    estadoAtual = CONDUZIR_NA_PISTA;
  }
}

// ==========================================
// DRIVERS DE MOVIMENTO (Skid-Steering DIREÇÃO DIFERENCIAL)
// ==========================================
void andarFrente() {
  analogWrite(ENA, velocidadeAtual); 
  analogWrite(ENB, velocidadeAtual);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  // Lado Esquerdo avança
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  // Lado Direito avança
}

void andarTras() {
  analogWrite(ENA, velocidadeNormal); 
  analogWrite(ENB, velocidadeNormal);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); // Lado Esquerdo recua
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); // Lado Direito recua
}

void virarEsquerdaSuave() {
  analogWrite(ENA, velocidadeAtual / 3); // Desacelera a esquerda para curvar
  analogWrite(ENB, velocidadeAtual);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void virarDireitaSuave() {
  analogWrite(ENA, velocidadeAtual); 
  analogWrite(ENB, velocidadeAtual / 3); // Desacelera a direita para curvar
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void rodarParaEsquerda() {
  analogWrite(ENA, velocidadeNormal); 
  analogWrite(ENB, velocidadeNormal);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); // Esquerda para trás
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  // Direita para a frente
}

void rodarParaDireita() {
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