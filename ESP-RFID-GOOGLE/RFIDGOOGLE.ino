#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>

#include <WiFiClientSecure.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
#define SS_PIN D4
#define RST_PIN D0    // Configurável, veja a pinagem padrão acima

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Criando instância do MFRC522
#define BUZZ_PIN D8
#define GATE_PIN D3
const char* host = "script.google.com";
const int httpsPort = 443;
const char* fingerprint  = "46 B2 C3 44 9C 59 09 8B 01 B6 F8 BD 4C FB 00 74 91 2F EF F6"; // para https


//*********** Linhas para alterar *******************
const char* ssid = "";
const char* password = "";
String GOOGLE_SCRIPT_ID = ""; // Replace by your GAS service id
const String unitName = "sede"; // qualquer nome sem espaços nem caracteres especiais
//*********** Fim das linhas para alterar *******************
uint64_t openGateMillis = 0;
WiFiClientSecure client;

void LcdClearAndPrint(String text)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(text);
}


void Siren()
{
  for (int hz = 440; hz < 1000; hz++) {
    tone(BUZZ_PIN, hz, 50);
    delay(5);
  }

  for (int hz = 1000; hz > 440; hz--) {
    tone(BUZZ_PIN, hz, 50);
    delay(5);
  }
  digitalWrite(BUZZ_PIN, LOW);
}


void Beep()
{
  for (int i = 0; i < 1000; i++)
  {
    analogWrite(BUZZ_PIN, i);
    delayMicroseconds(50);
  }
  digitalWrite(BUZZ_PIN, LOW);
}

void Beep2()
{
  tone(BUZZ_PIN, 1000, 30);
  delay(300);
  digitalWrite(BUZZ_PIN, LOW);
}

void setup() {


  pinMode(GATE_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(GATE_PIN, LOW);
  digitalWrite(BUZZ_PIN, LOW);
  

  Serial.begin(921600); 

  

  lcd.begin(); // Inicializa com o pino padrão do ESP8266 ou ARDUINO
// lcd.begin(0, 2); //ESP8266-01 I2C com o pino 0-SDA 2-SCL
// Aciona a luz de fundo e exibe uma mensagem.
  lcd.backlight();
  LcdClearAndPrint("Carregando");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("Iniciado");
  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Inicializa a comunicação serial com o PC
  while (!Serial);    // Não faz nada se nenhuma porta serial estiver aberta (adicionado para Arduinos baseados no ATMEGA32U4)
  SPI.begin();      // Inicializa o barramento SPI
  mfrc522.PCD_Init();   // Inicializa o MFRC522
  delay(4);       // Atraso opcional. Algumas placas precisam de mais tempo depois de inicializar para ficarem prontas.
  mfrc522.PCD_DumpVersionToSerial();  // Mostra os detalhes do leitor de cartão MFRC522
  Serial.println(F("Escaneie o PICC para ver UID, SAK, tipo e blocos de dados..."));
  LcdClearAndPrint("Pronto");
}
byte readCard[4];

void HandleDataFromGoogle(String data)
{
  int ind = data.indexOf(":");
  String access = data.substring(0, ind);
  int nextInd = data.indexOf(":", ind + 1);
  String name = data.substring(ind + 1, nextInd);
  String text = data.substring(nextInd + 1, data.length());

  Serial.println(name);
  LcdClearAndPrint(name);
  lcd.setCursor(0, 1);
  lcd.print(text);
  if (access=="-1")
  {
    lcd.print(" " + String("negado"));
    Siren(); 
    LcdClearAndPrint("Pronto");
  }
  else if(access=="any")
  {
    
    lcd.print(" " + String("Pode entrar"));
    OpenGate();
  }
  else if (access=="geladeira")
  {
    
    lcd.print(" " + String("Pode pegar"));
    OpenGate();
  }

}

void OpenGate()
{
  openGateMillis = millis()+5000;
  digitalWrite(GATE_PIN, HIGH);
  Beep();
  delay(100);
  Beep();
}

void CloseGate()
{
  openGateMillis = 0;
  digitalWrite(GATE_PIN, LOW);
  Beep2();
  LcdClearAndPrint("Pronto");
}

void loop() {
  if (openGateMillis > 0 && openGateMillis < millis())
  {
    CloseGate();
  }


  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Seleciona um dos cartões
  // Reseta o loop se nenhum novo cartão estiver presente no leitor. Isto evita que o processo inteiro rode novamente quando ocioso.
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  Serial.println(F("UID escaneado do PICC:"));
  String uid = "";
  for (uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
    uid += String(readCard[i],HEX);
  }
  Serial.println("");

  Beep();
  LcdClearAndPrint("Aguarde...");
  String data = sendData("id=" + unitName + "&uid=" + uid,NULL);
  HandleDataFromGoogle(data);


  mfrc522.PICC_HaltA();
}


String sendData(String params, char* domain) {
  // O google scripts precisa de duas solicitações GET 
  bool needRedir = false;
  if (domain == NULL)
  {
    domain=(char*)host;
    needRedir = true;
    params = "/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + params;
  }
  
  Serial.println(*domain);
  String result = "";
  client.setInsecure();
  Serial.print("Conectando-se a");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("Falha na conexão");
    return "";
  }

  if (client.verify(fingerprint, domain)) {
  }

  Serial.print("Solicitando URL: ");
  Serial.println(params);

  client.print(String("GET ") + params + " HTTP/1.1\r\n" +
    "Host: " + domain + "\r\n" +
    "Connection: close\r\n\r\n");

  Serial.println("solicitação enviada");
  while (client.connected()) {

    String line = client.readStringUntil('\n');
    //Serial.println(line);
    if (needRedir) {

    int ind = line.indexOf("/macros/echo?user");
    if (ind > 0)
    {
      Serial.println(line);
      line = line.substring(ind);
      ind = line.lastIndexOf("\r");
      line = line.substring(0, ind);
      Serial.println(line);
      result = line;
    }
    }

    if (line == "\r") {
      Serial.println("cabeçalhos recebidos");
      break;
    }
  }
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if(!needRedir)
    if (line.length() > 5)
      result = line;
    //Serial.println(line);
    
    }
  if (needRedir)
    return sendData(result, "script.googleusercontent.com");
  else return result;
  

}
