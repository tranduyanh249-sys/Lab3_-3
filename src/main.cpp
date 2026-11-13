
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ========== WIFI + MQTT ==========
const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// ========== CAM BIEN ==========
#define MQ2_PIN 34
#define DHT_PIN 15
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

// ========== THIET BI ==========
#define RELAY_FAN 26
#define RELAY_PUMP 27
#define BUZZER 25

#define GAS_LIMIT 2000
#define TEMP_LIMIT 50.0

// ========== CAU TRUC DU LIEU ==========
struct SensorData {
  int gas;
  float temp;
  float hum;
};

// Lenh dieu khien
struct Command {
  int device;  // 0: quat, 1: bom
  int mode;    // 0: TU DONG, 1: TAT, 2: BAT
};

// ========== QUEUE ==========
QueueHandle_t dataQueue;
QueueHandle_t dataAlert;
QueueHandle_t commandQueue;

// ========== DIEU KHIEN THU CONG ==========
bool manualFan = false;
bool manualPump = false;

bool fanManualState = false;
bool pumpManualState = false;

// ========== MQTT TOPIC ==========
const char* sensorTopic = "hoang/fire/sensor";
const char* fanTopic = "hoang/fire/cmd/fan";
const char* pumpTopic = "hoang/fire/cmd/pump";

// ========== MQTT CALLBACK ==========
void mqttCallback(char* topic, byte* payload,
                  unsigned int length) {

  String msg = "";

  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Command cmd;

  if (strcmp(topic, fanTopic) == 0) {
    cmd.device = 0;
  }
  else if (strcmp(topic, pumpTopic) == 0) {
    cmd.device = 1;
  }
  else {
    return;
  }

  if (msg == "ON") cmd.mode = 2;
  else if (msg == "OFF") cmd.mode = 1;
  else if (msg == "AUTO") cmd.mode = 0;
  else return;

  xQueueSend(commandQueue, &cmd, 0);
}

// ========== KET NOI MQTT ==========
void reconnectMQTT() {

  while (!mqtt.connected()) {

    String clientID = "ESP32_Fire_";
    clientID += String((uint32_t)ESP.getEfuseMac(), HEX);

    Serial.println("Dang ket noi MQTT...");

    if (mqtt.connect(clientID.c_str())) {

      Serial.println("Da ket noi MQTT!");

      mqtt.subscribe(fanTopic);
      mqtt.subscribe(pumpTopic);

      Serial.println("Da dang ky nhan lenh dieu khien!");

    } else {

      Serial.println("Ket noi MQTT that bai!");

      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }
}

// =================================================
// TASK 1: WIFI + MQTT - CORE 0
// =================================================
void TaskTransmit(void *pvParameters) {

  Serial.println("Dang ket noi WiFi...");

  WiFi.begin(ssid, password, 6);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  Serial.println("Da ket noi WiFi!");

  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(256);

  SensorData data;

  while (1) {

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Mat ket noi WiFi, dang ket noi lai...");
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    if (!mqtt.connected()) {
      reconnectMQTT();
    }

    mqtt.loop();

    // Nhan du lieu tu Queue
    if (xQueueReceive(dataQueue, &data,
                      pdMS_TO_TICKS(100)) == pdTRUE) {

      char payload[150];

      snprintf(payload, sizeof(payload),
        "{\"gas\":%d,\"temp\":%.1f,\"hum\":%.1f}",
        data.gas, data.temp, data.hum);

      if (mqtt.connected()) {
        mqtt.publish(sensorTopic, payload);
        Serial.println("Da gui du lieu MQTT: " + String(payload));
      }
    }
  }
}

// =================================================
// TASK 2: DOC CAM BIEN - CORE 1
// =================================================
void TaskRead(void *pvParameters) {

  SensorData data;

  while (1) {

    // Doc cam bien MQ2
    data.gas = analogRead(MQ2_PIN);

    // Doc nhiet do va do am DHT22
    data.temp = dht.readTemperature();
    data.hum = dht.readHumidity();

    // Kiem tra loi cam bien
    if (isnan(data.temp) || isnan(data.hum)) {

      Serial.println("Loi doc cam bien DHT22!");

      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    // Hien thi Serial
    Serial.println("\n========== HE THONG BAO CHAY ==========");

    Serial.print("Gia tri khi gas (ADC): ");
    Serial.println(data.gas);

    Serial.print("Nhiet do: ");
    Serial.print(data.temp);
    Serial.println(" do C");

    Serial.print("Do am: ");
    Serial.print(data.hum);
    Serial.println(" %");

    // Gui du lieu vao 2 Queue
    xQueueSend(dataQueue, &data, 0);
    xQueueSend(dataAlert, &data, 0);

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// =================================================
// TASK 3: CANH BAO + DIEU KHIEN - CORE 1
// =================================================
void TaskAlert(void *pvParameters) {

  SensorData data;
  Command cmd;

  bool fanAuto = false;
  bool pumpAuto = false;

  while (1) {

    // Nhan du lieu canh bao
    if (xQueueReceive(dataAlert, &data,
                      pdMS_TO_TICKS(100)) == pdTRUE) {

      fanAuto = (data.gas > GAS_LIMIT);
      pumpAuto = (data.temp > TEMP_LIMIT);

      Serial.println("\n========== KIEM TRA CANH BAO ==========");

      Serial.print("Canh bao khi gas: ");
      Serial.println(fanAuto ? "BAT" : "TAT");

      Serial.print("Canh bao nhiet do: ");
      Serial.println(pumpAuto ? "BAT" : "TAT");
    }

    // Nhan lenh tu Web MQTT
    while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {

      if (cmd.device == 0) {
        manualFan = (cmd.mode != 0);
        fanManualState = (cmd.mode == 2);

        Serial.print("Lenh dieu khien quat: ");
        Serial.println(cmd.mode == 0 ? "TU DONG" :
                       cmd.mode == 1 ? "TAT" : "BAT");
      }

      if (cmd.device == 1) {
        manualPump = (cmd.mode != 0);
        pumpManualState = (cmd.mode == 2);

        Serial.print("Lenh dieu khien bom: ");
        Serial.println(cmd.mode == 0 ? "TU DONG" :
                       cmd.mode == 1 ? "TAT" : "BAT");
      }
    }

    // Dieu khien quat va may bom
    bool fanState = manualFan ? fanManualState : fanAuto;
    bool pumpState = manualPump ? pumpManualState : pumpAuto;

    digitalWrite(RELAY_FAN, fanState ? HIGH : LOW);
    digitalWrite(RELAY_PUMP, pumpState ? HIGH : LOW);

    // Dieu khien coi bao dong
    if (fanAuto || pumpAuto) {
      tone(BUZZER, 2000);
    }
    else {
      noTone(BUZZER);
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// =================================================
// KHOI TAO HE THONG
// =================================================
void setup() {

  Serial.begin(115200);

  dht.begin();

  pinMode(MQ2_PIN, INPUT);

  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(RELAY_FAN, LOW);
  digitalWrite(RELAY_PUMP, LOW);

  noTone(BUZZER);

  // Tao Queue
  dataQueue = xQueueCreate(10, sizeof(SensorData));
  dataAlert = xQueueCreate(10, sizeof(SensorData));
  commandQueue = xQueueCreate(10, sizeof(Command));

  if (!dataQueue || !dataAlert || !commandQueue) {

    Serial.println("Loi khoi tao Queue!");

    while (1) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  // Tao Task 1: MQTT + WiFi - Core 0
  xTaskCreatePinnedToCore(
    TaskTransmit,
    "TaskTransmit",
    8192,
    NULL,
    2,
    NULL,
    0
  );

  // Tao Task 2: Doc cam bien - Core 1
  xTaskCreatePinnedToCore(
    TaskRead,
    "TaskRead",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Tao Task 3: Canh bao + dieu khien - Core 1
  xTaskCreatePinnedToCore(
    TaskAlert,
    "TaskAlert",
    4096,
    NULL,
    3,
    NULL,
    1
  );

  Serial.println("HE THONG BAO CHAY DA KHOI DONG!");
}

// =================================================
// LOOP
// =================================================
void loop() {

  vTaskDelay(portMAX_DELAY);
}
