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

// --- Estado SOS ---
bool sosAtivo = false;

// Sequência Morse SOS: . . . _ _ _ . . .
// true = sinal ligado, false = pausa
// durações em ms
const int SOS_PONTO = 150;
const int SOS_TRACO = 450;
const int SOS_PAUSA_SIMBOLO = 150;  // entre pontos/traços
const int SOS_PAUSA_LETRA = 400;    // entre letras S e O
const int SOS_PAUSA_CICLO = 1000;   // entre ciclos SOS completos

// Sequência: duração, ligado?
// S = . . .   O = _ _ _   S = . . .
struct Sinal { int duracao; bool ligado; };

const Sinal sequenciaSOS[] = {
  // S
  {SOS_PONTO, true},  {SOS_PAUSA_SIMBOLO, false},
  {SOS_PONTO, true},  {SOS_PAUSA_SIMBOLO, false},
  {SOS_PONTO, true},  {SOS_PAUSA_LETRA,   false},
  // O
  {SOS_TRACO, true},  {SOS_PAUSA_SIMBOLO, false},
  {SOS_TRACO, true},  {SOS_PAUSA_SIMBOLO, false},
  {SOS_TRACO, true},  {SOS_PAUSA_LETRA,   false},
  // S
  {SOS_PONTO, true},  {SOS_PAUSA_SIMBOLO, false},
  {SOS_PONTO, true},  {SOS_PAUSA_SIMBOLO, false},
  {SOS_PONTO, true},  {SOS_PAUSA_CICLO,   false},
};
const int TOTAL_SINAIS = sizeof(sequenciaSOS) / sizeof(sequenciaSOS[0]);

int sosIndex = 0;
unsigned long sosUltimoMillis = 0;

void atualizarSOS() {
  if (!sosAtivo) {
    // Desliga tudo se SOS foi cancelado
    sosIndex = 0;
    return;
  }

  unsigned long agora = millis();
  Sinal atual = sequenciaSOS[sosIndex];

  if (agora - sosUltimoMillis >= (unsigned long)atual.duracao) {
    sosUltimoMillis = agora;
    sosIndex = (sosIndex + 1) % TOTAL_SINAIS;

    if (atual.ligado) {
      digitalWrite(LED_VERMELHO, HIGH);
      digitalWrite(LED_VERDE, HIGH);
      digitalWrite(LED_AZUL, HIGH);
      tone(BUZZER, 1000);
    } else {
      digitalWrite(LED_VERMELHO, LOW);
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_AZUL, LOW);
      noTone(BUZZER);
    }
  }
}

// --- Mede distância ---
float medirDistancia() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duracao = pulseIn(ECHO, HIGH, 60000);
  if (duracao == 0) return 999;
  return duracao * 0.034 / 2.0;
}

// --- Buzzer sensor de ré ---
unsigned long buzzerUltimoMillis = 0;
bool buzzerEstado = false;

void atualizarBuzzer(float dist) {
  if (sosAtivo) return; // SOS tem prioridade

  unsigned long agora = millis();
  int intervalo = 0;

  if (dist < 20)        intervalo = 0;   // contínuo
  else if (dist < 50)   intervalo = 200;
  else if (dist < 100)  intervalo = 500;
  else { noTone(BUZZER); return; }

  if (intervalo == 0) {
    tone(BUZZER, 1000);
    return;
  }

  if (agora - buzzerUltimoMillis >= (unsigned long)intervalo) {
    buzzerUltimoMillis = agora;
    buzzerEstado = !buzzerEstado;
    if (buzzerEstado) tone(BUZZER, 800);
    else noTone(BUZZER);
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
    sosAtivo = false;
    noTone(BUZZER);
    digitalWrite(LED_VERMELHO, LOW);
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_AZUL, LOW);
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
    if (valor == "SOS:1") { sosAtivo = true;  Serial.println("SOS ATIVADO!"); }
    if (valor == "SOS:0") { sosAtivo = false; Serial.println("SOS DESATIVADO!"); }
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

  BLEDevice::init("ESP32-Caminhao");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(UUID_SERVICE);

  ledCharacteristic = service->createCharacteristic(
    UUID_LED, BLECharacteristic::PROPERTY_WRITE
  );
  ledCharacteristic->setCallbacks(new LedCallbacks());

  distanciaCharacteristic = service->createCharacteristic(
    UUID_DISTANCIA, BLECharacteristic::PROPERTY_NOTIFY
  );
  distanciaCharacteristic->addDescriptor(new BLE2902());

  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(UUID_SERVICE);
  advertising->start();

  Serial.println("ESP32 pronto!");
}

// --- Sensor: controle de tempo ---
unsigned long sensorUltimoMillis = 0;

void loop() {
  // Atualiza SOS a cada ciclo (sem delay!)
  atualizarSOS();

  // Lê sensor a cada 300ms sem travar
  unsigned long agora = millis();
  if (agora - sensorUltimoMillis >= 1000) {
    sensorUltimoMillis = agora;

    if (deviceConnected) {
      float dist = medirDistancia();
      Serial.print("Distância: ");
      Serial.print(dist);
      Serial.println(" cm");

      distanciaCharacteristic->setValue(String(dist, 1).c_str());
      distanciaCharacteristic->notify();

      atualizarBuzzer(dist);
    }
  }
}
