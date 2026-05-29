#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// 1. PINOS DOS MOTORES (ESPECÍFICO ESP32-S2)
// ==========================================
const int IN1_LE = 4;  const int IN2_LE = 5; 
const int IN3_LE = 6;  const int IN4_LE = 7; 
const int IN1_LD = 8;  const int IN2_LD = 9;  
const int IN3_LD = 10; const int IN4_LD = 11; 

// ==========================================
// 2. PINOS DOS SENSORES (INFRAVERMELHOS E 3x ULTRASSÓNICOS)
// ==========================================
const int SENSOR_FRENTE_ESQ = 12; 
const int SENSOR_FRENTE_DIR = 13; 

// Ultrassónico Central
const int PIN_TRIG_CENTRO = 16; 
const int PIN_ECHO_CENTRO = 17; 

// Ultrassónico Esquerdo (Novos Pinos)
const int PIN_TRIG_ESQ = 18;
const int PIN_ECHO_ESQ = 21;

// Ultrassónico Direito (Novos Pinos)
const int PIN_TRIG_DIR = 33;
const int PIN_ECHO_DIR = 34;

#define LINHA_PRETA HIGH 
#define CHAO_CLARO LOW

// Parâmetros de distâncias da nova lógica
const int DISTANCIA_PARAR = 25;    // frentDist <= distanciaParar
const int DISTANCIA_LATERAL = 15;  // esquerdaDist ou direitaDist <= distanciaLateral

// ==========================================
// 3. CONTROLES E VARIÁVEIS DE ESTADO
// ==========================================
int velManual = 210;      
int velAutoReto = 70;      
int velAutoCurva = 130;    

bool modoAutonomo = false;

WebServer server(80);
const char* ssid = "VADE_Car_Controller";

// Armazenamento global da última distância do centro para a interface web
int ultimaDistanciaCentro = 999;

// ==========================================
// 4. INTERFACE WEB
// ==========================================
const char HTML_PAGINA[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>VADE - Pro Controller</title>
    <style>
        :root { --bg-color: #0a0f1e; --remote-body: #1e293b; --accent-cyan: #22d3ee; --button-dark: #0f172a; --joystick-base: #334155; --stop-red: #ef4444; --accent-green: #10b981; }
        body { font-family: 'Segoe UI', Roboto, sans-serif; background-color: var(--bg-color); color: white; margin: 0; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; overflow: hidden; user-select: none; -webkit-user-select: none; touch-action: manipulation;}
        .remote-container { background: var(--remote-body); padding: 40px; border-radius: 60px; box-shadow: inset 0 2px 10px rgba(255,255,255,0.1), 0 20px 50px rgba(0,0,0,0.5); border: 2px solid #334155; display: flex; flex-direction: column; align-items: center; position: relative; }
        .brand { font-size: 14px; letter-spacing: 4px; color: var(--accent-cyan); margin-bottom: 20px; font-weight: bold; }
        .lcd-screen { background: #000; width: 220px; height: 70px; border-radius: 8px; margin-bottom: 15px; border: 2px solid #475569; display: flex; flex-direction: column; align-items: center; justify-content: center; color: var(--accent-cyan); font-family: monospace; text-shadow: 0 0 10px var(--accent-cyan); }
        #display { font-size: 18px; }
        #distancia { font-size: 14px; color: var(--accent-green); margin-top: 5px; }
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
        <div class="lcd-screen">
            <div id="display">MANUAL</div>
            <div id="distancia">Centro: -- cm</div>
        </div>
        <button id="modeBtn" class="btn-mode manual-active" onclick="toggleMode(event)">MUDAR PARA AUTÓNOMO</button>
        <div class="d-pad">
            <div class="btn up" onmousedown="control('FRENTE', event)" onmouseup="control('PARAR', event)" onmouseleave="control('PARAR', event)" ontouchstart="control('FRENTE', event)" ontouchend="control('PARAR', event)">▲</div>
            <div class="btn left" onmousedown="control('ESQUERDA', event)" onmouseup="control('PARAR', event)" onmouseleave="control('PARAR', event)" ontouchstart="control('ESQUERDA', event)" ontouchend="control('PARAR', event)">◀️</div>
            <div class="btn stop btn-stop" onclick="control('PARAR', event)" ontouchstart="control('PARAR', event)">STOP</div>
            <div class="btn right" onmousedown="control('DIREITA', event)" onmouseup="control('PARAR', event)" onmouseleave="control('PARAR', event)" ontouchstart="control('DIREITA', event)" ontouchend="control('PARAR', event)">▶️</div>
            <div class="btn down" onmousedown="control('TRAS', event)" onmouseup="control('PARAR', event)" onmouseleave="control('PARAR', event)" ontouchstart="control('TRAS', event)" ontouchend="control('PARAR', event)">▼</div>
        </div>
        <div class="footer">HYBRID DRIVING SYSTEM v4.0</div>
    </div>
    <script>
        let modoManualAtivo = true; 
        function toggleMode(e) {
            if(e) e.preventDefault();
            modoManualAtivo = !modoManualAtivo;
            const btn = document.getElementById('modeBtn');
            const display = document.getElementById('display');
            let modoStr = modoManualAtivo ? "manual" : "auto";
            if (modoManualAtivo) { 
                btn.innerText = "MUDAR PARA AUTÓNOMO"; btn.classList.add('manual-active'); display.innerText = "MANUAL"; control('PARAR');
            } else { 
                btn.innerText = "MUDAR PARA MANUAL"; btn.classList.remove('manual-active'); display.innerText = "AUTÓNOMO"; control('PARAR');
            }
            fetch('/setmode?mode=' + modoStr).catch(err => {});
        }
        function control(cmd, e) {
            if(e && e.cancelable) e.preventDefault(); 
            if (modoManualAtivo) { document.getElementById('display').innerText = cmd; }
            let path = (cmd === 'PARAR') ? '/stop' : '/move?dir=' + cmd.toLowerCase();
            fetch(path).catch(err => {});
        }
        setInterval(() => {
            fetch('/getdist').then(r => r.text()).then(d => {
                document.getElementById('distancia').innerText = "Centro: " + d + " cm";
            }).catch(err => {});
        }, 500);
    </script>
</body>
</html>
)rawliteral";

// ==========================================
// 5. SETUP E INICIALIZAÇÃO
// ==========================================
void setup() {
  Serial.begin(115200);
  
  pinMode(IN1_LE, OUTPUT); pinMode(IN2_LE, OUTPUT); pinMode(IN3_LE, OUTPUT); pinMode(IN4_LE, OUTPUT);
  pinMode(IN1_LD, OUTPUT); pinMode(IN2_LD, OUTPUT); pinMode(IN3_LD, OUTPUT); pinMode(IN4_LD, OUTPUT);

  pinMode(SENSOR_FRENTE_ESQ, INPUT); 
  pinMode(SENSOR_FRENTE_DIR, INPUT);

  pinMode(PIN_TRIG_CENTRO, OUTPUT); pinMode(PIN_ECHO_CENTRO, INPUT);
  pinMode(PIN_TRIG_ESQ, OUTPUT);    pinMode(PIN_ECHO_ESQ, INPUT);
  pinMode(PIN_TRIG_DIR, OUTPUT);    pinMode(PIN_ECHO_DIR, INPUT);

  WiFi.softAP(ssid);
  Serial.print("IP do Carro: "); Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send(200, "text/html", HTML_PAGINA); });
  
  server.on('/setmode', []() {
    String mode = server.arg("mode");
    if (mode == "auto") { 
      modoAutonomo = true; 
      Serial.println("MODO AUTÓNOMO ATIVADO");
    } else { 
      modoAutonomo = false; 
      parar(); 
      Serial.println("MODO MANUAL ATIVADO");
    }
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/move", []() {
    if(!modoAutonomo) { 
      String dir = server.arg("dir");
      if (dir == "frente") irFrente(velManual);
      else if (dir == "tras") irTras(velManual);
      else if (dir == "esquerda") virarEsquerda(velManual);
      else if (dir == "direita") virarDireita(velManual);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/stop", []() { parar(); server.send(200, "text/plain", "OK"); });
  server.on("/getdist", []() {
    server.send(200, "text/plain", String(ultimaDistanciaCentro));
  });

  server.begin();
  parar(); 
}

// ==========================================
// FUNÇÃO AUXILIAR: MEDIR DISTÂNCIA DE UM SENSOR ESPECÍFICO
// ==========================================
long medirDistancia(int pinTrig, int pinEcho) {
  digitalWrite(pinTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);
  
  long duracao = pulseIn(pinEcho, HIGH, 25000); // Timeout de 25ms para não represar o código
  if (duracao == 0) return 999; 
  return duracao * 0.034 / 2;
}

// ==========================================
// 6. LÓGICA DO PILOTO AUTOMÁTICO (3x ULTRASSÓNICOS + LINHA)
// ==========================================
void pilotoAutomatico() {
  // Leitura sequencial dos 3 Ultrassónicos com delay entre eles para evitar ecos falsos
  long frenteDist = medirDistancia(PIN_TRIG_CENTRO, PIN_ECHO_CENTRO);
  ultimaDistanciaCentro = frenteDist; // Atualiza a variável para a página web
  delay(40);
  
  long direitaDist = medirDistancia(PIN_TRIG_DIR, PIN_ECHO_DIR);
  delay(40);
  
  long esquerdaDist = medirDistancia(PIN_TRIG_ESQ, PIN_ECHO_ESQ);
  delay(40);

  // Leitura dos infravermelhos
  int linhaEsq = digitalRead(SENSOR_FRENTE_ESQ);
  int linhaDir = digitalRead(SENSOR_FRENTE_DIR);

  // Debug via Serial Monitor
  Serial.print("F: "); Serial.print(frenteDist);
  Serial.print(" | E: "); Serial.print(esquerdaDist);
  Serial.print(" | D: "); Serial.print(direitaDist);
  Serial.print(" | IR_E: "); Serial.print(linhaEsq);
  Serial.print(" | IR_D: "); Serial.println(linhaDir);

  // --- REGRAS DE PRIORIDADE E TOMADA DE DECISÃO ---

  // 1. Sensor Esquerdo toca na linha preta isoladamente -> Corrige para a direita
  if (linhaEsq == LINHA_PRETA && linhaDir != LINHA_PRETA) {
    parar(); delay(100);
    irTras(velAutoReto); delay(200);
    virarDireita(velAutoCurva); delay(850);
    parar(); delay(100);
  }
  // 2. Sensor Direito toca na linha preta isoladamente -> Corrige para a esquerda
  else if (linhaDir == LINHA_PRETA && linhaEsq != LINHA_PRETA) {
    parar(); delay(100);
    irTras(velAutoReto); delay(200);
    virarEsquerda(velAutoCurva); delay(850);
    parar(); delay(100);
  }
  // 3. Ambos na linha preta -> Continua em frente de acordo com o teu algoritmo original
  else if (linhaEsq == LINHA_PRETA && linhaDir == LINHA_PRETA) {
    irFrente(velAutoReto);
  }
  // 4. Deteta obstáculo fixo / parede mesmo em frente
  else if (frenteDist <= DISTANCIA_PARAR) {
    parar(); delay(150);
    irTras(velAutoReto); delay(300);
    parar(); delay(100);

    // Decide o caminho baseado no lado que tiver mais espaço livre
    if (esquerdaDist > direitaDist) {
      virarEsquerda(velAutoCurva); delay(850);
    } else {
      virarDireita(velAutoCurva); delay(850);
    }
    parar(); delay(100);
  }
  // 5. Demasiado perto da parede esquerda (Ajuste lateral)
  else if (esquerdaDist <= DISTANCIA_LATERAL) {
    virarDireita(velAutoCurva); delay(250);
    parar(); delay(50);
  }
  // 6. Demasiado perto da parede direita (Ajuste lateral)
  else if (direitaDist <= DISTANCIA_LATERAL) {
    virarEsquerda(velAutoCurva); delay(250);
    parar(); delay(50);
  }
  // 7. Caminho completamente limpo
  else {
    irFrente(velAutoReto);
  }
}

// ==========================================
// 7. FUNÇÕES DE MOVIMENTO ADAPTADAS ÀS TUAS PONTES H
// ==========================================
void irFrente(int vel) { 
  analogWrite(IN1_LE, vel); analogWrite(IN2_LE, 0); 
  analogWrite(IN3_LE, vel); analogWrite(IN4_LE, 0); 
  analogWrite(IN1_LD, 0); analogWrite(IN2_LD, vel); 
  analogWrite(IN3_LD, 0); analogWrite(IN4_LD, vel); 
}

void irTras(int vel) { 
  analogWrite(IN1_LE, 0); analogWrite(IN2_LE, vel); 
  analogWrite(IN3_LE, 0); analogWrite(IN4_LE, vel); 
  analogWrite(IN1_LD, vel); analogWrite(IN2_LD, 0); 
  analogWrite(IN3_LD, vel); analogWrite(IN4_LD, 0); 
}

void virarEsquerda(int vel) { 
  analogWrite(IN1_LE, 0); analogWrite(IN2_LE, vel); 
  analogWrite(IN3_LE, 0); analogWrite(IN4_LE, vel); 
  analogWrite(IN1_LD, 0); analogWrite(IN2_LD, vel); 
  analogWrite(IN3_LD, 0); analogWrite(IN4_LD, vel); 
}

void virarDireita(int vel) { 
  analogWrite(IN1_LE, vel); analogWrite(IN2_LE, 0); 
  analogWrite(IN3_LE, vel); analogWrite(IN4_LE, 0); 
  analogWrite(IN1_LD, vel); analogWrite(IN2_LD, 0); 
  analogWrite(IN3_LD, vel); analogWrite(IN4_LD, 0); 
}

void parar() { 
  analogWrite(IN1_LE, 0); analogWrite(IN2_LE, 0); 
  analogWrite(IN3_LE, 0); analogWrite(IN4_LE, 0); 
  analogWrite(IN1_LD, 0); analogWrite(IN2_LD, 0); 
  analogWrite(IN3_LD, 0); analogWrite(IN4_LD, 0); 
}

// ==========================================
// 8. LOOP PRINCIPAL
// ==========================================
void loop() {
  server.handleClient();
  
  if (modoAutonomo) { 
    pilotoAutomatico(); 
  }
  
  delay(2); 
}