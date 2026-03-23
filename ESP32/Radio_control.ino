#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- Pinos ---
#define LED_VERMELHO 21
#define LED_VERDE    19
#define LED_AZUL     18
#define TRIG         23
#define ECHO         22
#define BUZZER       5

// --- UUIDs BLE ---
#define UUID_SERVICE   "12345678-1234-1234-1234-123456789abc"
#define UUID_LED       "abcdefab-1234-1234-1234-abcdefabcdef"
#define UUID_DISTANCIA "abcdefef-1234-1234-1234-abcdefabcdef"

BLECharacteristic* ledCharacteristic;
BLECharacteristic* distanciaCharacteristic;
bool deviceConnected = false;

// --- Mede distância em cm ---
float medirDistancia() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duracao = pulseIn(ECHO, HIGH, 30000);
  if (duracao == 0) return 999;
  return duracao * 0.034 / 2.0;
}

// --- Buzzer proporcional à distância ---
void atualizarBuzzer(float dist) {
  if (dist < 20) {
    // Muito perto — bipe contínuo
    tone(BUZZER, 1000);
  } else if (dist < 50) {
    // Perto — bipe rápido
    tone(BUZZER, 800);
    delay(100);
    noTone(BUZZER);
    delay(100);
  } else if (dist < 100) {
    // Médio — bipe lento
    tone(BUZZER, 600);
    delay(100);
    noTone(BUZZER);
    delay(300);
  } else {
    noTone(BUZZER);
  }
}

// --- Callbacks BLE ---
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) {
    deviceConnected = true;
    Serial.println("Celular conectado!");
  }
  void onDisconnect(BLEServer* server) {
    deviceConnected = false;
    noTone(BUZZER);
    Serial.println("Celular desconectado!");
    server->startAdvertising();
  }
};

class LedCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) {
    String valor = c->getValue().c_str();
    Serial.print("Recebido: ");
    Serial.println(valor);
    if (valor == "LED:VERMELHO:1") digitalWrite(LED_VERMELHO, HIGH);
    if (valor == "LED:VERMELHO:0") digitalWrite(LED_VERMELHO, LOW);
    if (valor == "LED:VERDE:1")    digitalWrite(LED_VERDE, HIGH);
    if (valor == "LED:VERDE:0")    digitalWrite(LED_VERDE, LOW);
    if (valor == "LED:AZUL:1")     digitalWrite(LED_AZUL, HIGH);
    if (valor == "LED:AZUL:0")     digitalWrite(LED_AZUL, LOW);
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(LED_VERMELHO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);
  noTone(BUZZER);

  // --- BLE ---
  BLEDevice::init("ESP32-Control");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(UUID_SERVICE);

  // Característica LED (escrita)
  ledCharacteristic = service->createCharacteristic(
    UUID_LED, BLECharacteristic::PROPERTY_WRITE
  );
  ledCharacteristic->setCallbacks(new LedCallbacks());

  // Característica Distância (notify — ESP32 envia pro app)
  distanciaCharacteristic = service->createCharacteristic(
    UUID_DISTANCIA,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  distanciaCharacteristic->addDescriptor(new BLE2902());

  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(UUID_SERVICE);
  advertising->start();

  Serial.println("ESP32 pronto!");
}

void loop() {
  if (deviceConnected) {
    float dist = medirDistancia();
    Serial.print("Distância: ");
    Serial.print(dist);
    Serial.println(" cm");

    // Envia distância via BLE notify
    String valor = String(dist, 1); // ex: "45.3"
    distanciaCharacteristic->setValue(valor.c_str());
    distanciaCharacteristic->notify();

    atualizarBuzzer(dist);
  }
  delay(300);
}