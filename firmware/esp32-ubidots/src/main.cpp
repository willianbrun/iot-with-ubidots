#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "UbidotsEsp32Mqtt.h"
#include "secrets.h"

// === OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// === PINOS ===
const int PIN_LDR = 36;
const int PIN_BTN = 14;
const int PIN_LED = 2;

// --------------------------- CONFIGURACAO AWS ---------------------------
constexpr char AWS_PUB_TOPIC[] = "esp32/pub";
constexpr char AWS_SUB_TOPIC[] = "esp32/sub";
constexpr uint32_t AWS_MQTT_BUFFER = 512;
constexpr uint32_t JSON_BUF_SIZE = 512;

WiFiClientSecure awsNet;
MQTTClient awsMqtt(AWS_MQTT_BUFFER);

// --------------------------- CONFIGURACAO UBIDOTS ------------------------
const char *UBI_DEVICE = "esp32";
Ubidots ubidots(UBIDOTS_TOKEN);

// --------------------------- ESTADO DO SISTEMA --------------------------
volatile int stateMode = 0; // 0=off,1=on,2=sensor-driven
int sensorThreshold = 600;  // default (0..4095)
int ldrValue = 0;
bool ledState = false;

// timers
constexpr unsigned long SENSOR_POLL_INTERVAL = 10;
unsigned long lastSensorPoll = 0;

// --------------------------- DEBOUNCE BOTAO ------------------
bool btnLast = HIGH;               // último estado estável (HIGH = solto)
bool btnReading = HIGH;            // última leitura
unsigned long btnLastDebounce = 0; // tempo da última mudança de leitura
constexpr unsigned long DEBOUNCE_MS = 30;
int buttonClicks = 0;

// --------------------------- UBIDOTS PAYLOAD ---------------------------
void fillPayload(JsonDocument &d)
{
  d["statemode"] = stateMode;
  d["ledstate"] = ledState ? 1 : 0;
  d["sensorthreshold"] = sensorThreshold;
  d["millis"] = millis();
}

// Publica evento para Ubidots
void publishToUbidots(const char *eventName, JsonDocument &payloadDoc)
{
  ubidots.add("event", 1.0);
  ubidots.add("statemode", (float)stateMode);
  ubidots.add("ledstate", ledState ? 1.0f : 0.0f);
  ubidots.add("sensorthreshold", (float)sensorThreshold);
  ubidots.publish(UBI_DEVICE);
}

// Publica evento para AWS
void publishToAWS(const char *eventName)
{
  StaticJsonDocument<JSON_BUF_SIZE> doc;
  doc["event"] = eventName;
  doc["buttonclicks"] = buttonClicks;

  char buf[JSON_BUF_SIZE];
  size_t n = serializeJson(doc, buf);

  if (awsMqtt.connected())
  {
    Serial.println("Sent to AWS 'tenclicks' event");
    awsMqtt.publish(AWS_PUB_TOPIC, String(buf, n));
  }
  else
  {
    Serial.println("[AWS] não conectado: mensagem não publicada");
  }
}

// --------------------------- HELPERS: LED/STATE -------------------------
void applyLedFromState(int state, int lightRaw)
{
  if (state == 0)
  {
    ledState = false;
  }
  else if (state == 1)
  {
    ledState = true;
  }
  else
  { // state == 2 -> sensor-driven
    if (lightRaw < 0)
    {
      // unknown sensor: keep previous state
    }
    else
    {
      ledState = (lightRaw < sensorThreshold);
    }
  }
  digitalWrite(PIN_LED, ledState ? HIGH : LOW);
}

// --------------------------- BUTTON HANDLER ---------------------------
void handleButton()
{
  bool reading = digitalRead(PIN_BTN); // leitura bruta

  // Se mudou a leitura bruta, reinicia o timer
  if (reading != btnReading)
  {
    btnReading = reading;
    btnLastDebounce = millis();
  }

  // Se a leitura está estável além do tempo de debounce
  if ((millis() - btnLastDebounce) > DEBOUNCE_MS)
  {
    // Se o estado estável real mudou (transição confirmada)
    if (btnReading != btnLast)
    {
      btnLast = btnReading; // atualiza estado estável

      // Detecta borda de descida: pressionado (pull-up => LOW)
      if (btnLast == LOW)
      {
        stateMode = (stateMode + 1) % 3;
        buttonClicks++;

        int lightRaw = analogRead(PIN_LDR);
        applyLedFromState(stateMode, lightRaw);

        StaticJsonDocument<200> ev;
        ev["source"] = "button";
        publishToUbidots("button_click", ev);

        if (buttonClicks % 10 == 0)
        {
          publishToAWS("tenClicks");
        }
      }
    }
  }
}

// --------------------------- CALLBACKS -------------------------------
void awsMessageHandler(String &topic, String &payload)
{
  Serial.println("\n[AWS] message:");
  Serial.println(" topic: " + topic);
  Serial.println(" payload: " + payload);
}

void ubiCallback(char *topic, byte *payload, unsigned int length)
{
  String s;
  for (unsigned int i = 0; i < length; i++)
    s += (char)payload[i];
  s.trim();

  String t = String(topic);
  int last = t.lastIndexOf('/');
  int prev = t.lastIndexOf('/', last - 1);
  String variable = t.substring(prev + 1, last);

  float val = s.toFloat();

  bool changed = false;

  if (variable == "statemode")
  {
    int newState = constrain((int)val, 0, 2);
    if (newState != stateMode)
    { // <--- evita loop
      stateMode = newState;
      applyLedFromState(stateMode, analogRead(PIN_LDR));
      changed = true;
    }
  }

  else if (variable == "sensorthreshold")
  {
    int newThreshold = (int)val;
    if (newThreshold != sensorThreshold)
    { // <--- evita loop
      sensorThreshold = newThreshold;
      changed = true;
    }
  }

  else if (variable == "ledstate")
  {
    bool newLed = (val >= 0.5);
    if (newLed != ledState)
    { // <--- evita loop
      ledState = newLed;
      digitalWrite(PIN_LED, ledState ? HIGH : LOW);
      changed = true;
    }
  }

  if (changed)
  {
    StaticJsonDocument<200> ev;
    ev["source"] = "ubidots";
  }
}

// --------------------------- CONEXOES -------------------------------
void connectWiFi()
{
  Serial.print("Conectando WiFi ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(400);
  }
  Serial.println("\nWiFi conectado: " + WiFi.localIP().toString());
}

void connectAWS()
{
  Serial.println("Conectando AWS...");
  awsNet.setCACert(AWS_CERT_CA);
  awsNet.setCertificate(AWS_CERT_CRT);
  awsNet.setPrivateKey(AWS_CERT_PRIVATE);

  awsMqtt.begin(AWS_IOT_ENDPOINT, 8883, awsNet);
  awsMqtt.onMessage(awsMessageHandler);

  unsigned long start = millis();
  while (!awsMqtt.connect(THINGNAME))
  {
    if (millis() - start > 10000)
    {
      Serial.println("\n[AWS] conexão demorando, tentando novamente...");
      start = millis();
    }
    delay(300);
    Serial.print(".");
  }
  Serial.println("\n[AWS] conectado");
  awsMqtt.subscribe(AWS_SUB_TOPIC);
}

void connectUbidots()
{
  ubidots.setDebug(true);
  ubidots.setCallback(ubiCallback);
  ubidots.setup();
  ubidots.reconnect();

  ubidots.subscribeLastValue(UBI_DEVICE, "statemode");
  ubidots.subscribeLastValue(UBI_DEVICE, "sensorthreshold");
  ubidots.subscribeLastValue(UBI_DEVICE, "ledstate");
}

// --------------------------- SETUP ------------------------------------
void setup()
{
  // Pinos
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);

  digitalWrite(PIN_LED, LOW);

  Serial.begin(115200);

  // OLED
  Wire.begin(5, 4); // SDA=5, SCL=4
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED nao encontrado!");
    while (1)
      ;
  }
  display.setRotation(2);
  display.clearDisplay();
  display.display();

  connectWiFi();

  // Ubidots init
  connectUbidots();

  // AWS init
  connectAWS();

  // initial apply
  int lightRaw = analogRead(PIN_LDR);
  applyLedFromState(stateMode, lightRaw);
}

// --------------------------- LOOP PRINCIPAL ----------------------------
void loop()
{
  awsMqtt.loop();
  ubidots.loop();

  if (!awsMqtt.connected())
  {
    Serial.println("[AWS] desconectado, reconectando...");
    connectAWS();
  }
  if (!ubidots.connected())
  {
    Serial.println("[Ubi] desconectado, reconectando...");
    ubidots.reconnect();
    ubidots.subscribeLastValue(UBI_DEVICE, "statemode");
    ubidots.subscribeLastValue(UBI_DEVICE, "sensorthreshold");
    ubidots.subscribeLastValue(UBI_DEVICE, "ledstate");
  }

  // ============ BOTÃO ============
  handleButton();

  // ============ LDR ============
  static unsigned long lastReadLDR = 0;
  if (millis() - lastReadLDR > 10) // lê a cada 10ms
  {
    lastReadLDR = millis();
    ldrValue = analogRead(PIN_LDR);
  }

  // ============ OLED ============
  static unsigned long lastOled = 0;
  if (millis() - lastOled > 50) // atualiza a cada 100ms
  {
    lastOled = millis();

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(15, 0);
    display.println("* LIGHT SENSOR *");

    display.setCursor(0, 20);
    display.print("LDR: ");
    display.println(ldrValue);

    display.setCursor(0, 35);
    display.print("Estado do botao: ");
    display.println(stateMode);

    display.setCursor(0, 50);
    display.print("LED: ");
    display.println(ledState ? "LIGADO" : "DESLIGADO");

    display.display();
  }

  // ============ SENSOR POLLING ============
  if (millis() - lastSensorPoll > SENSOR_POLL_INTERVAL)
  {
    lastSensorPoll = millis();
    int lightRaw = ldrValue;
    float lightPct = map(lightRaw, 0, 4095, 0, 100);

    if (stateMode == 2)
    {
      bool prevLed = ledState;
      applyLedFromState(stateMode, lightRaw);

      if (ledState != prevLed)
      {
        StaticJsonDocument<200> ev;
        ev["light_raw"] = lightRaw;
        publishToUbidots("led_changed_by_sensor", ev);
      }
    }
  }
}
