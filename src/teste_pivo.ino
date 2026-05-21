#include <Wire.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// CONFIGURAÇÃO 8 PINOS (2 DRIVERS)
// ==========================================

// --- DRIVER ESQUERDO (Trás: IN1/2, Frente: IN3/4) ---
const int IN1_LE = 26; 
const int IN2_LE = 2; // (LED Azul para diagnóstico)
const int IN3_LE = 27; 
const int IN4_LE = 13; 

// --- DRIVER DIREITO (Trás: IN1/2, Frente: IN3/4) ---
const int IN1_LD = 32; 
const int IN2_LD = 4;  
const int IN3_LD = 23; 
const int IN4_LD = 19; 

WebServer server(80);
const char* ssid = "VADE_Car_Controller";

// ==========================================
// NOVA INTERFACE WEB PRO CONTROLLER
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
        <button id="modeBtn" class="btn-mode" onclick="toggleMode(event)">MUDAR PARA MANUAL</button>
        <div class="d-pad">
            <div class="btn up" onmousedown="control('FRENTE', event)" onmouseup="control('PARAR', event)" onmouseleave="control('PARAR', event)" ontouchstart="control('FRENTE', event)" ontouchend="control('PARAR', event)">▲</div>
            <div class="btn left" onmousedown="control('ESQUERDA', event)" onmouseup="control('PARAR', event)" onmouseleave="control('PARAR', event)" ontouchstart="control('ESQUERDA', event)" ontouchend="control('PARAR', event)">◀</div>
            <div class="btn stop btn-stop" onclick="control('PARAR', event)" ontouchstart="control('PARAR', event)">STOP</div>
            <div class="btn right" onmousedown="control('DIREITA', event)" onmouseup="control('PARAR', event)" onmouseleave="control('PARAR', event)" ontouchstart="control('DIREITA', event)" ontouchend="control('PARAR', event)">▶</div>
            <div class="btn down" onmousedown="control('TRAS', event)" onmouseup="control('PARAR', event)" onmouseleave="control('PARAR', event)" ontouchstart="control('TRAS', event)" ontouchend="control('PARAR', event)">▼</div>
        </div>
        <div class="footer">HYBRID DRIVING SYSTEM v3.0</div>
    </div>
    <script>
        let modoManualAtivo = false;
        
        function toggleMode(e) {
            if(e) e.preventDefault();
            modoManualAtivo = !modoManualAtivo;
            const btn = document.getElementById('modeBtn');
            const display = document.getElementById('display');
            let modoStr = modoManualAtivo ? "manual" : "auto";
            
            if (modoManualAtivo) { 
                btn.innerText = "MUDAR PARA AUTÓNOMO"; 
                btn.classList.add('manual-active'); 
                display.innerText = "MANUAL: READY"; 
            } else { 
                btn.innerText = "MUDAR PARA MANUAL"; 
                btn.classList.remove('manual-active'); 
                display.innerText = "AUTÓNOMO"; 
                control('PARAR');
            }
            fetch('/setmode?mode=' + modoStr).catch(err => {});
        }

        function control(cmd, e) {
            if(e && e.cancelable) e.preventDefault(); 
            
            if (modoManualAtivo) { 
                document.getElementById('display').innerText = cmd; 
            }
            
            let path = (cmd === 'PARAR') ? '/stop' : '/move?dir=' + cmd.toLowerCase();
            fetch(path).catch(err => {});
        }
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // Configurar todos os 8 pinos como Saída
  pinMode(IN1_LE, OUTPUT); pinMode(IN2_LE, OUTPUT); pinMode(IN3_LE, OUTPUT); pinMode(IN4_LE, OUTPUT);
  pinMode(IN1_LD, OUTPUT); pinMode(IN2_LD, OUTPUT); pinMode(IN3_LD, OUTPUT); pinMode(IN4_LD, OUTPUT);

  WiFi.softAP(ssid);
  Serial.print("IP do Carro: "); Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send(200, "text/html", HTML_PAGINA); });
  
  server.on("/setmode", []() {
    String mode = server.arg("mode");
    Serial.print("Modo alterado para: "); Serial.println(mode);
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/move", []() {
    String dir = server.arg("dir");
    Serial.print("Movimento: "); Serial.println(dir);
    
    if (dir == "frente") {
      digitalWrite(IN1_LE, HIGH); digitalWrite(IN2_LE, LOW);
      digitalWrite(IN3_LE, HIGH); digitalWrite(IN4_LE, LOW);
      digitalWrite(IN1_LD, HIGH); digitalWrite(IN2_LD, LOW);
      digitalWrite(IN3_LD, HIGH); digitalWrite(IN4_LD, LOW);
    } 
    else if (dir == "tras") {
      digitalWrite(IN1_LE, LOW); digitalWrite(IN2_LE, HIGH);
      digitalWrite(IN3_LE, LOW); digitalWrite(IN4_LE, HIGH);
      digitalWrite(IN1_LD, LOW); digitalWrite(IN2_LD, HIGH);
      digitalWrite(IN3_LD, LOW); digitalWrite(IN4_LD, HIGH);
    } 
    else if (dir == "esquerda") {
      // CURVA DE PIVÔ: Motores esquerdos parados, motores direitos empurram
      digitalWrite(IN1_LE, LOW); digitalWrite(IN2_LE, LOW);
      digitalWrite(IN3_LE, LOW); digitalWrite(IN4_LE, LOW);
      digitalWrite(IN1_LD, HIGH); digitalWrite(IN2_LD, LOW);
      digitalWrite(IN3_LD, HIGH); digitalWrite(IN4_LD, LOW);
    } 
    else if (dir == "direita") {
      // CURVA DE PIVÔ: Motores esquerdos empurram, motores direitos parados
      digitalWrite(IN1_LE, HIGH); digitalWrite(IN2_LE, LOW);
      digitalWrite(IN3_LE, HIGH); digitalWrite(IN4_LE, LOW);
      digitalWrite(IN1_LD, LOW); digitalWrite(IN2_LD, LOW);
      digitalWrite(IN3_LD, LOW); digitalWrite(IN4_LD, LOW);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/stop", []() {
    parar();
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  parar(); // Garante que começa parado
}

void parar() {
  digitalWrite(IN1_LE, LOW); digitalWrite(IN2_LE, LOW);
  digitalWrite(IN3_LE, LOW); digitalWrite(IN4_LE, LOW);
  digitalWrite(IN1_LD, LOW); digitalWrite(IN2_LD, LOW);
  digitalWrite(IN3_LD, LOW); digitalWrite(IN4_LD, LOW);
}

void loop() {
  server.handleClient();
}