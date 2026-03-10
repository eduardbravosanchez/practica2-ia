#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// --- Configuración de Hardware ---
const int PIN_ENTRADA = 4;      // Pin donde entra el pulso (GPIO4)
const int TAMANO_COLA = 50;     // Tamaño de la cola circular

// --- Punto 2: Estructura de Cola Circular ---
volatile unsigned long colaCircular[TAMANO_COLA];
volatile int indiceEscritura = 0;
volatile int contadorElementos = 0;

// Variables de tiempo (Timer interno micros())
volatile unsigned long ultimoTiempo = 0;

// Servidor Web en puerto 80
AsyncWebServer server(80);

// --- Punto 1: Interrupción para medir tiempo de pulso ---
void IRAM_ATTR manejadorPulsos() {
    unsigned long tiempoActual = micros(); // Usa el timer interno del sistema
    unsigned long deltaT = tiempoActual - ultimoTiempo;

    if (deltaT > 500) { // Pequeño debounce de seguridad (0.5ms)
        // Guardar en cola circular
        colaCircular[indiceEscritura] = deltaT;
        indiceEscritura = (indiceEscritura + 1) % TAMANO_COLA;
        
        if (contadorElementos < TAMANO_COLA) {
            contadorElementos++;
        }
    }
    ultimoTiempo = tiempoActual;
}

// Función para procesar estadísticas de la cola
String obtenerDatosJSON() {
    if (contadorElementos == 0) return "{\"med\":0,\"max\":0,\"min\":0}";

    unsigned long suma = 0;
    unsigned long tMax = 0;
    unsigned long tMin = 0xFFFFFFFF;

    // Bloqueamos brevemente interrupciones para lectura segura si es necesario
    for (int i = 0; i < contadorElementos; i++) {
        unsigned long val = colaCircular[i];
        suma += val;
        if (val > tMax) tMax = val;
        if (val < tMin) tMin = val;
    }

    // Punto 3: Cálculo de Frecuencia (Hz = 1,000,000 / microsegundos)
    float fMed = 1000000.0 / (suma / contadorElementos);
    float fMax = 1000000.0 / tMin; // Menor tiempo = Mayor frecuencia
    float fMin = 1000000.0 / tMax;

    return "{\"med\":"+String(fMed)+",\"max\":"+String(fMax)+",\"min\":"+String(fMin)+"}";
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_ENTRADA, INPUT_PULLUP);

    // Configurar interrupción: flanco de subida
    attachInterrupt(digitalPinToInterrupt(PIN_ENTRADA), manejadorPulsos, RISING);

    // --- Punto 3: Punto de Acceso WiFi y Servidor Web ---
    WiFi.softAP("ESP32S3_Monitor", "12345678");
    Serial.println("IP del Servidor: " + WiFi.softAPIP().toString());

    // Ruta principal con HTML y JS para actualización dinámica (Punto 3)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<html><head><meta charset='UTF-8'>";
        html += "<style>body{font-family:sans-serif; text-align:center; background:#f4f4f4;}";
        html += ".card{background:white; padding:20px; border-radius:10px; display:inline-block; margin:10px; shadow: 2px 2px 10px #ccc;}</style>";
        html += "<script>setInterval(function(){fetch('/update').then(r=>r.json()).then(d=>{";
        html += "document.getElementById('med').innerText=d.med.toFixed(2);";
        html += "document.getElementById('max').innerText=d.max.toFixed(2);";
        html += "document.getElementById('min').innerText=d.min.toFixed(2);});}, 500);</script>";
        html += "</head><body><h1>Panel de Frecuencias (IA P2)</h1>";
        html += "<div class='card'><h2>Media</h2><p id='med'>0</p> Hz</div>";
        html += "<div class='card'><h2>Máxima</h2><p id='max'>0</p> Hz</div>";
        html += "<div class='card'><h2>Mínima</h2><p id='min'>0</p> Hz</div>";
        html += "</body></html>";
        request->send(200, "text/html", html);
    });

    // Endpoint para datos dinámicos
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", obtenerDatosJSON());
    });

    server.begin();
}

void loop() {
    // El loop queda libre para otras tareas, el servidor e interrupciones son asíncronos.
}