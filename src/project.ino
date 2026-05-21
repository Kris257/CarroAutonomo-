#include <Wire.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// CONFIGURAÇÃO DE PINOS - HARDWARE DEFINITIVO
// ==========================================

// --- DRIVER DO LADO ESQUERDO ---
const int IN1_LE = 12; 
const int IN2_LE = 14; 
const int IN3_LE = 27; 
const int IN4_LE = 13; 

// --- DRIVER DO LADO DIREITO ---
const int IN1_LD = 2;  // Corrigido para D2
const int IN2_LD = 4;  
const int IN3_LD = 23; 
const int IN4_LD = 19; 

// --- SENSORES INFRAVERMELHOS ---
const int IR_FRENTE_ESQ = 33; 
const int IR_FRENTE_DIR = 25; 
const int IR_TRAS_ESQ   = 26; 
const int IR_TRAS_DIR   = 32; 

// --- ULTRASSÓNICO E SERVO ---
const int PIN_TRIG = 5; 
const int PIN_ECHO = 18;
const int PIN_SERVO = 15;

// ==========================================
// SERVIDOR WEB E WI-FI AP
// ==========================================
WebServer server(80);
const char* ssid = "VADE_Car_Controller";

// Interface Web
const char HTML_PAGINA[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>VADE - Pro Controller</title>
    <style>
        :root { --bg-color: #0a0f1e; --remote-body: #1e293b; --accent-cyan: #22d3ee; --button-dark: #0f172a; --joystick-base: #334155; --stop-red: #ef4444; --accent-green: #10b981; }
        body { font-family: 'Segoe UI', Roboto, sans-serif; background-color: var(--bg-color); color: white; margin: 0; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; overflow: hidden; user-select: none; }
        .remote-container { background: var(--remote-body); padding: 40px; border-radius: 60px; box-shadow: inset 0 2px 10px rgba(255,255,255,0.1), 0 20px 50px rgba(0,0,0,0.5); border: 2px solid #334155; display: flex; flex-direction: column; align-items: center; position: relative; }
        .brand { font-size: 14px; letter-spacing: 4px; color: var(--accent-cyan); margin-bottom: 20px; font-weight: bold; }
        .lcd-screen { background: #000; width: 180px; height: 50px; border-radius: 8px; margin-bottom: 15px; border: 2px solid #475569; display: flex; align-items: center; justify-content: center; color: var(--accent-cyan); font-family: monospace; font-size: 18px; text-shadow: 0 0 10px var(--accent-cyan); }
        .btn-mode { background: var(--button-dark); color: var(--accent-green); border: 2px solid var(--accent-green); border-radius: 12px; width: 180px; height: 40px; font-weight: bold; font-size: 12px; margin-bottom: 25px; cursor: pointer; box-shadow: 0 4px rgba(0,0,0,0.4); }
        .btn-mode.manual-active { color: var(--accent-cyan); border: 2px solid var(--accent-cyan); }
        .d-pad { display: grid; grid-template-columns: repeat(3, 85px); grid-template-rows: repeat(3, 85px); gap: 10px; background: var(--joystick-base); padding: 15px; border-radius: 100px; }
        .btn { background: var(--button-dark); border: none; border-radius: 50%; color: white; font-size: 24px; display: flex; align-items: center; justify-content: center; cursor: pointer; box-shadow: 0 6px #000; }
        .btn:active { box-shadow: 0 2px #000; transform: translateY(4px); background: var(--accent-cyan); color: var(--bg-color); }
        .btn-stop { background: var(--stop-red); border-radius: 15px; font-size: 18px; font-weight: bold; box-shadow: 0 6px #7f1d1d; }
        .up { grid-area: 1 / 2 / 2 / 3; } .left { grid-area: 2 / 1 / 3 / 2; } .stop { grid-area: 2 / 2 / 3 / 3; } .right { grid-area: 2 / 3 / 3 / 4; } .down { grid-area: 3 / 2 / 4 / 3; }
        .footer { margin-top: 30px; font-size: 10px; color: #475569; letter-spacing: 2px; }
    </style>
</head>
<body>
    <div class="remote-container">
        <div class="brand">IADE PROJECT FACTORY</div>
        <div class="lcd-screen"><span id="display">AUTÓNOMO</span></div>
        <button id="modeBtn" class="btn-mode" onclick="toggleMode()">MUDAR PARA MANUAL</button>
        <div class="d-pad">
            <div class="btn up" onmousedown="control('FRENTE')" onmouseup="control('PARAR')" ontouchstart="control('FRENTE')" ontouchend="control('PARAR')">▲</div>
            <div class="btn left" onmousedown="control('ESQUERDA')" onmouseup="control('PARAR')" ontouchstart="control('ESQUERDA')" ontouchend="control('PARAR')">◀</div>
            <div class="btn stop btn-stop" onclick="control('PARAR')">STOP</div>
            <div class="btn right" onmousedown="control('DIREITA')" onmouseup="control('PARAR')" ontouchstart="control('DIREITA')" ontouchend="control('PARAR')">▶</div>
            <div class="btn down" onmousedown="control('TRAS')" onmouseup="control('PARAR')" ontouchstart="control('TRAS')" ontouchend="control('PARAR')">▼</div>
        </div>
        <div class="footer">HYBRID DRIVING SYSTEM v3.0</div>
    </div>
    <script>
        let modoManualAtivo = false;
        function toggleMode() {
            modoManualAtivo = !modoManualAtivo;
            const btn = document.getElementById('modeBtn');
            const display = document.getElementById('display');
            let modoStr = modoManualAtivo ? "manual" : "auto";
            if (modoManualAtivo) { btn.innerText = "MUDAR PARA AUTÓNOMO"; btn.classList.add('manual-active'); display.innerText = "MANUAL: READY"; }
            else { btn.innerText = "MUDAR PARA MANUAL"; btn.classList.remove('manual-active'); display.innerText = "AUTÓNOMO"; }
            fetch('/setmode?mode=' + modoStr).catch(err => {});
        }
        function control(cmd) {
            if (modoManualAtivo) { document.getElementById('display').innerText = cmd; }
            let path = (cmd === 'PARAR') ? '/stop' : '/move?dir=' + cmd.toLowerCase();
            fetch(path).catch(err => {});
        }
    </script>
</body>
</html>
)rawliteral";

enum Estados { CONDUZIR_NA_PISTA, EVITAR_PAREDE, MARCHA_ATRAS_ASSISTIDA, MODO_MANUAL };
Estados estadoAtual = CONDUZIR_NA_PISTA;
Servo pescoco;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); 
  
  // Motores
  pinMode(IN1_LE, OUTPUT); pinMode(IN2_LE, OUTPUT); pinMode(IN3_LE, OUTPUT); pinMode(IN4_LE, OUTPUT);
  pinMode(IN1_LD, OUTPUT); pinMode(IN2_LD, OUTPUT); pinMode(IN3_LD, OUTPUT); pinMode(IN4_LD, OUTPUT);
  
  // Sensores
  pinMode(IR_FRENTE_ESQ, INPUT); pinMode(IR_FRENTE_DIR, INPUT);
  pinMode(IR_TRAS_ESQ, INPUT);   pinMode(IR_TRAS_DIR, INPUT);
  pinMode(PIN_TRIG, OUTPUT);     pinMode(PIN_ECHO, INPUT);
  
  pescoco.attach(PIN_SERVO); pescoco.write(90);
  inicializarMPU();

  WiFi.softAP(ssid);
  Serial.print("IP do Carro: "); Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send(200, "text/html", HTML_PAGINA); });
  
  server.on("/setmode", []() {
    String modo = server.arg("mode");
    if (modo == "manual") { estadoAtual = MODO_MANUAL; pararMotores(); } 
    else { estadoAtual = CONDUZIR_NA_PISTA; }
    server.send(200, "text/plain", "OK");
  });

  server.on("/move", []() {
    if (estadoAtual == MODO_MANUAL) {
      String direcao = server.arg("dir");
      if (direcao == "frente") andarFrente();
      else if (direcao == "tras") andarTras();
      else if (direcao == "esquerda") rodarParaEsquerda();
      else if (direcao == "direita") rodarParaDireita();
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/stop", []() { pararMotores(); server.send(200, "text/plain", "OK"); });
  server.begin();
}

void loop() {
  server.handleClient();

  if (estadoAtual == MODO_MANUAL) {
    delay(5);
    return; 
  }

  // --- LOGICA AUTÓNOMA ---
  switch (estadoAtual) {
    case CONDUZIR_NA_PISTA:
      if (calcularDistancia() < 18) { pararMotores(); estadoAtual = EVITAR_PAREDE; } 
      else { logicaManterNaPista(); }
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
// CONTROLO DIRETO DOS MOTORES (SKID-STEERING)
// ==========================================

void andarFrente() {
  // Lado Esquerdo Avança
  digitalWrite(IN1_LE, HIGH); digitalWrite(IN2_LE, LOW);
  digitalWrite(IN3_LE, HIGH); digitalWrite(IN4_LE, LOW);
  // Lado Direito Avança
  digitalWrite(IN1_LD, HIGH); digitalWrite(IN2_LD, LOW);
  digitalWrite(IN3_LD, HIGH); digitalWrite(IN4_LD, LOW);
}

void andarTras() {
  // Lado Esquerdo Recua
  digitalWrite(IN1_LE, LOW); digitalWrite(IN2_LE, HIGH);
  digitalWrite(IN3_LE, LOW); digitalWrite(IN4_LE, HIGH);
  // Lado Direito Recua
  digitalWrite(IN1_LD, LOW); digitalWrite(IN2_LD, HIGH);
  digitalWrite(IN3_LD, LOW); digitalWrite(IN4_LD, HIGH);
}

void rodarParaEsquerda() {
  // Lado Esquerdo Recua
  digitalWrite(IN1_LE, LOW); digitalWrite(IN2_LE, HIGH);
  digitalWrite(IN3_LE, LOW); digitalWrite(IN4_LE, HIGH);
  // Lado Direito Avança
  digitalWrite(IN1_LD, HIGH); digitalWrite(IN2_LD, LOW);
  digitalWrite(IN3_LD, HIGH); digitalWrite(IN4_LD, LOW);
}

void rodarParaDireita() {
  // Lado Esquerdo Avança
  digitalWrite(IN1_LE, HIGH); digitalWrite(IN2_LE, LOW);
  digitalWrite(IN3_LE, HIGH); digitalWrite(IN4_LE, LOW);
  // Lado Direito Recua
  digitalWrite(IN1_LD, LOW); digitalWrite(IN2_LD, HIGH);
  digitalWrite(IN3_LD, LOW); digitalWrite(IN4_LD, HIGH);
}

void virarEsquerdaSuave() { rodarParaEsquerda(); }
void virarDireitaSuave() { rodarParaDireita(); }

void pararMotores() {
  digitalWrite(IN1_LE, LOW); digitalWrite(IN2_LE, LOW);
  digitalWrite(IN3_LE, LOW); digitalWrite(IN4_LE, LOW);
  
  digitalWrite(IN1_LD, LOW); digitalWrite(IN2_LD, LOW);
  digitalWrite(IN3_LD, LOW); digitalWrite(IN4_LD, LOW);
}

// ==========================================
// SUBSISTEMAS DE SENSORES
// ==========================================
void inicializarMPU() { Wire.beginTransmission(0x68); Wire.write(0x6B); Wire.write(0); Wire.endTransmission(); }

int calcularDistancia() {
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duracao = pulseIn(PIN_ECHO, HIGH, 30000);
  int dist = duracao * 0.034 / 2;
  return (dist == 0) ? 999 : dist;
}

void logicaRadarEvasao() {
  pescoco.write(30); delay(400); int distDireita = calcularDistancia();
  pescoco.write(150); delay(500); int distEsquerda = calcularDistancia();
  pescoco.write(90); delay(300);
  
  if (distEsquerda > distDireita && distEsquerda > 20) { rodarParaEsquerda(); delay(800); } 
  else if (distDireita > distEsquerda && distDireita > 20) { rodarParaDireita(); delay(800); } 
  else { estadoAtual = MARCHA_ATRAS_ASSISTIDA; }
  
  if (estadoAtual != MARCHA_ATRAS_ASSISTIDA) estadoAtual = CONDUZIR_NA_PISTA;
}

void logicaManterNaPista() {
  int esq = digitalRead(IR_FRENTE_ESQ); int dir = digitalRead(IR_FRENTE_DIR);
  if (esq == 0 && dir == 0) andarFrente();
  else if (esq == 1 && dir == 0) virarDireitaSuave();
  else if (esq == 0 && dir == 1) virarEsquerdaSuave();
  else if (esq == 1 && dir == 1) pararMotores();
}

void logicaMarchaAtras() {
  int esq = digitalRead(IR_TRAS_ESQ); int dir = digitalRead(IR_TRAS_DIR);
  if (esq == 0 && dir == 0) andarTras();
  if (calcularDistancia() > 25) { pararMotores(); estadoAtual = CONDUZIR_NA_PISTA; }
}