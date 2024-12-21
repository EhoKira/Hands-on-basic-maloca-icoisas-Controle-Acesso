#include <Wire.h>               // Inclui a biblioteca Wire para comunicação I2C
#include <Adafruit_PN532.h>     // Inclui a biblioteca Adafruit para o PN532
#include <LiquidCrystal_I2C.h>

// Definição dos pinos I2C no ESP32
#define SDA_PIN 21              // Define o pino 21 do ESP32 como SDA
#define SCL_PIN 22              // Define o pino 22 do ESP32 como SCL
#define LED_SUCESS 18

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Inicializa o objeto para o módulo NFC PN532 usando I2C
Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);

void setup(void) {
  Serial.begin(115200);         
  pinMode(LED_SUCESS, OUTPUT);
  Serial.println("Inicializando sistema de controle de acessos...");


  nfc.begin();

  
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Não foi possível encontrar o leitor NFC.");
    while (1);
  }

  
  Serial.print("Firmware do leitor NFC: 0x");
  Serial.println(versiondata, HEX);

  // Configura o leitor NFC para modo passivo (para ler tags RFID/NFC)
  nfc.SAMConfig(); // Necessário para inicializar o modo de leitura passiva

  Serial.println("Monitorando acessos...");
  lcd.init(); //Inicializa a comunicação com o display já conectado
  lcd.clear(); //Limpa a tela do display
  lcd.backlight();
}

void loop(void) {
  uint8_t success;         
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 }; 
  uint8_t uidLength;    
  lcd.setCursor(0, 0); //Coloca o cursor do display na coluna 1 e linha 1
  lcd.print("Security Acess");         

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    digitalWrite(LED_SUCESS, HIGH);
    Serial.println("Tag NFC detectada!");
    Serial.print("UID da tag: ");
    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print(" 0x"); Serial.print(uid[i], HEX); 
    }
    lcd.setCursor(0, 1);  // Posiciona o cursor na segunda linha do display
    lcd.print("Acesso Autorizado");
    delay(1000);
    lcd.clear(); 
    Serial.println();
    digitalWrite(LED_SUCESS, LOW);
  }
}
