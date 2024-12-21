#include <Wire.h>               // Biblioteca para comunicação I2C
#include <Adafruit_PN532.h>     // Biblioteca para o módulo PN532
#include <WiFi.h>               // Biblioteca para conexão Wi-Fi
#include <PubSubClient.h>       // Biblioteca para MQTT
#include <NTPClient.h>          // Biblioteca para sincronização com servidor NTP
#include <WiFiUdp.h>            // Biblioteca para comunicação UDP (necessária para NTP)
#include <WiFiClientSecure.h>   // Biblioteca para comunicação Wi-Fi segura
#include <TimeLib.h>            // Biblioteca para manipulação de tempo
#include <LiquidCrystal_I2C.h>
 
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Definição dos pinos I2C e LED
#define SDA_PIN 21
#define SCL_PIN 22
#define LED_SUCESS 18

// Inicializa o objeto para o módulo NFC PN532 usando I2C
Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);

// Credenciais Wi-Fi
const char* ssid = "SSD_ID";
const char* password = "PASSWORD";

// Detalhes do broker MQTT
const char* mqtt_broker = "BROKER_URL";
const char* mqtt_topic = "TOPIC";
const char* mqtt_username = "USERNAME";
const char* mqtt_password = "HIVE_PASSWORD";
const int mqtt_port = 8883;

// Inicializa o cliente Wi-Fi seguro e MQTT
WiFiClientSecure espClient;  // Altere para WiFiClientSecure
PubSubClient client(espClient);

// Configurações para NTP (horário)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -14400);  // Fuso horário de Manaus (GMT-4)

String authorizedTags[] = { "0x04 0x7A 0x6C 0x1B", "0x93 0x82 0xa7 0xd" };

void setup_wifi() {
  delay(10);
  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Wi-Fi conectado.");
}

// Função de callback MQTT
void callback(char* topic, byte* message, unsigned int length) {
    String messageTemp;
    for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
    }
    if (messageTemp == "on_led") {
        digitalWrite(23, HIGH);  // Lock the lock
    } else if (messageTemp == "off_led") {
        digitalWrite(23, LOW);   // Unlock the lock
    }
}

// Função para reconectar ao MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando se reconectar ao MQTT...");
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("Conectado.");
      client.subscribe(mqtt_topic);  // Inscreve-se no tópico
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos.");
      delay(5000);
    }
  }
}

// Função para verificar se a tag é autorizada
bool isAuthorized(String uid) {
  for (String tag : authorizedTags) {
    if (tag == uid) {
      return true;
    }
  }
  return false;
}

void setup(void) {
  Serial.begin(115200);
  pinMode(LED_SUCESS, OUTPUT);
  
  setup_wifi();
  espClient.setInsecure();  // Desabilitar verificação de certificado SSL/TLS
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  timeClient.begin();
  
  Serial.println("Inicializando sistema de controle de acessos...");
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Não foi possível encontrar o leitor NFC.");
    while (1);
  }

  nfc.SAMConfig();
  lcd.init(); //Inicializa a comunicação com o display já conectado
  lcd.clear(); //Limpa a tela do display
  lcd.backlight();
  Serial.println("Monitorando acessos...");
}

void loop(void) {
  lcd.setCursor(0, 0); //Coloca o cursor do display na coluna 1 e linha 1
  lcd.print("Security Acess");
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  timeClient.update();
  
  // Sincronizar o horário do NTP com a biblioteca TimeLib
  setTime(timeClient.getEpochTime());

  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    digitalWrite(LED_SUCESS, HIGH);
    Serial.println("Tag NFC detectada!");
    
    // Converter UID para String
    String uidString = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      uidString += "0x";
      uidString += String(uid[i], HEX);
      if (i < uidLength - 1) uidString += " ";
    }
    Serial.print("UID da tag: ");
    Serial.println(uidString);

    // Obter data e hora atual usando TimeLib
    String currentTime = String(hour()) + ":" + String(minute()) + ":" + String(second());
    String currentDate = String(day()) + "/" + String(month()) + "/" + String(year());

    // Verificar se a tag é autorizada
    String message;
    if (isAuthorized(uidString)) {
      message = "Acesso autorizado às " + currentTime + " no dia " + currentDate + ", por " + uidString;
      lcd.setCursor(0, 1);  // Posiciona o cursor na segunda linha do display
      lcd.print("Acesso Autorizado");
      delay(1000);
      lcd.clear();  // Limpa o display antes de imprimir o texto

    } else {
      message = "Acesso negado às " + currentTime + " no dia " + currentDate + ", por " + uidString;
      lcd.setCursor(0, 1);  // Posiciona o cursor na segunda linha do display
      lcd.print("Acesso Negado");
      delay(1000);
      lcd.clear();  // Limpa o display antes de imprimir o texto
    }

    // Enviar mensagem para o tópico MQTT
    if (client.publish(mqtt_topic, message.c_str())) {
      Serial.println("Mensagem enviada com sucesso!");
    } else {
      Serial.println("Falha ao enviar mensagem.");
    }
    
    Serial.println(message.c_str());

    delay(500);
    digitalWrite(LED_SUCESS, LOW);
  }
}
