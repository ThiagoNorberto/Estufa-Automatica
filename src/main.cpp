/************************************************************************************************
 * * Projeto de TCC: Estufa Automatizada com ESP32
 ************************************************************************************************/

//==============================================================================================
// 1. CONFIGURAÇÕES DO BLYNK
//==============================================================================================
#define BLYNK_TEMPLATE_ID "TMPL270lgcqwq"
#define BLYNK_TEMPLATE_NAME "TCC"
#define BLYNK_AUTH_TOKEN "QEJBnxgA9E7NxO_NSpE1nuHrdn82THeq"
#define BLYNK_PRINT Serial

//==============================================================================================
// 2. INCLUSÃO DE BIBLIOTECAS
//==============================================================================================
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <BlynkSimpleEsp32.h>

//==============================================================================================
// 3. CONFIGURAÇÕES DO PROJETO
//==============================================================================================
// --- Credenciais Wi-Fi ---
char auth[] = BLYNK_AUTH_TOKEN;
//char ssid[] = "Wokwi-GUEST";
//char pass[] = "";
//char ssid[] = "Turjillio_2G";
//char pass[] = "Juju17vivi05";

char ssid[] = "Desktop_F1612557";
char pass[] = "14032620th";

// --- Endpoint do Google Apps Script ---
String googleScriptUrl = "https://script.google.com/macros/s/AKfycbz7H9NlGFZZvSKtc6r4OWo14iC5t1PCYhYkWMLmppAgjSN0C2MgSwDyOxII4sDU2ZBqiQ/exec";

//==============================================================================================
// 4. MAPEAMENTO DE HARDWARE (PINOS)
//==============================================================================================
#define PINO_SENSOR_DHT 18
#define PINO_RELE_BOMBA 13
#define PINO_RELE_COOLER 4
#define PINO_SENSOR_MQ135 32 // Mude para o pino que era da umidade do solo
#define PINO_SENSOR_UMIDADE_SOLO 34 // Mude para um pino ADC1


// --- PINOS VIRTUAIS BLYNK ---
#define VPIN_UMIDADE_SOLO V0

#define VPIN_STATUS_BOMBA V1
#define VPIN_UMIDADE_AR V2
#define VPIN_TEMPERATURA_AR V3
#define VPIN_CO2 V4
#define VPIN_STATUS_COOLER V5

//==============================================================================================
// 5. CONSTANTES E PARÂMETROS
//==============================================================================================
// --- Parâmetros de Irrigação ---
const int UMIDADE_MINIMA_IRRIGACAO = 30;
const int UMIDADE_MAXIMA_IRRIGACAO = 50;

// --- Parâmetros de Ventilação ---
const float TEMPERATURA_MAXIMA_LIGAR = 28.0;
const float TEMPERATURA_SEGURA_DESLIGAR = 26.0;
const float UMIDADE_AR_MAXIMA_LIGAR = 65.0;
const float UMIDADE_AR_SEGURA_DESLIGAR = 55.0;
const float CO2_NIVEL_MAXIMO_LIGAR = 1250.0;
const float CO2_IDEAL_SUPERIOR = 1200.0;
const float CO2_IDEAL_INFERIOR = 800.0;
const float CO2_NIVEL_MINIMO_LIGAR = 750.0;

// --- Parâmetros de Temporização ---
const unsigned long INTERVALO_LEITURAS_MS = 2000;
const unsigned long INTERVALO_ENVIO_HTTP_MS = 60000;
const unsigned long INTERVALO_TELA_LCD_MS = 5000;

// --- Parâmetros de Sensores ---
#define TIPO_SENSOR_DHT DHT22
#define MQ135_PARAM_A 116.6020682
#define MQ135_PARAM_B 2.769034857
#define MQ135_RL_KOHM 10.0

//==============================================================================================
// 6. OBJETOS GLOBAIS
//==============================================================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

DHT dht(PINO_SENSOR_DHT, TIPO_SENSOR_DHT);
BlynkTimer timer;

//==============================================================================================
// 7. VARIÁVEIS GLOBAIS
//==============================================================================================
int umidadeSolo = 0;
float temperaturaAr = 0;
float umidadeAr = 0;
float co2PPM = 0.0;
float mq135_R0 = 0.0;
bool bombaLigada = false;
bool coolerLigado = false;
int lcdTelaAtual = 0;


//==============================================================================================
// 8. PROTÓTIPOS DE FUNÇÕES
//==============================================================================================
void executarCicloPrincipal();
float calibrarMQ135();
void lerUmidadeSolo();
void lerUmidadeTemperaturaAr();
void lerNivelGas();
void tomarDecisao();
void gerenciarLCD();
void enviarDadosGoogleSheet();
void configurarHora();


//==============================================================================================
// 9. SETUP
//==============================================================================================
void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");

  pinMode(PINO_RELE_BOMBA, OUTPUT);
  digitalWrite(PINO_RELE_BOMBA, LOW);
  pinMode(PINO_RELE_COOLER, OUTPUT);
  digitalWrite(PINO_RELE_COOLER, LOW);

  lcd.setCursor(0, 1);
  lcd.print("WiFi e Blynk...");
  Blynk.begin(auth, ssid, pass, "blynk.cloud", 80);

  lcd.clear();
  if (Blynk.connected()) {
    lcd.setCursor(0, 0);
    lcd.print("Conectado!");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Falha na conexao");
    lcd.setCursor(0, 1);
    lcd.print("Verifique a rede");
    while(true);
  }
  delay(2000);

  configurarHora();

  dht.begin();
  mq135_R0 = calibrarMQ135();
 
  timer.setInterval(INTERVALO_LEITURAS_MS, executarCicloPrincipal);
  timer.setInterval(INTERVALO_TELA_LCD_MS, gerenciarLCD);
  timer.setInterval(INTERVALO_ENVIO_HTTP_MS, enviarDadosGoogleSheet);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistema Online");
}

//==============================================================================================
// 10. LOOP
//==============================================================================================
void loop() {
  Blynk.run();
  timer.run();
}

//==============================================================================================
// 11. IMPLEMENTAÇÃO DAS FUNÇÕES
//==============================================================================================


void executarCicloPrincipal() {
  lerUmidadeSolo();
  lerUmidadeTemperaturaAr();
  lerNivelGas();
  tomarDecisao();
}

void enviarDadosGoogleSheet() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Abortando envio para Google.");
    return;
  }

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  Serial.println("\nEnviando dados para Google Sheet...");

  if (http.begin(client, googleScriptUrl)) {
    http.addHeader("Content-Type", "application/json");

    String estadoBombaString = bombaLigada ? "Ligada" : "Desligada";
    String estadoCoolerString = coolerLigado ? "Ligado" : "Desligado";

    String jsonPayload = "{\"umidadeSolo\":" + String(umidadeSolo) +
                          ",\"umidadeAr\":" + String(umidadeAr, 1) +
                          ",\"temperaturaAr\":" + String(temperaturaAr, 1) +
                          ",\"co2PPM\":" + String(co2PPM, 2) +
                          ",\"estadoBomba\":\"" + estadoBombaString + "\"" +
                          ",\"estadoCooler\":\"" + estadoCoolerString + "\"}";

    Serial.println("Payload: " + jsonPayload);

    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
      Serial.printf("[HTTP] Codigo de resposta: %d\n", httpCode);
    } else {
      Serial.printf("[HTTP] Falha no POST, erro: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end(); //algo errado aqui talvez
    } else {
    Serial.printf("[HTTP] Nao foi possivel conectar\n");
    }
}

void configurarHora() {
  const char* ntpServer = "br.pool.ntp.org";
  const long gmtOffset_sec = -10800;
  const int daylightOffset_sec = 0;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void lerUmidadeSolo() {
  int valorBruto = analogRead(PINO_SENSOR_UMIDADE_SOLO);
  umidadeSolo = map(valorBruto, 1600, 3300, 100, 0);
  umidadeSolo = constrain(umidadeSolo, 0, 100);
  Blynk.virtualWrite(VPIN_UMIDADE_SOLO, umidadeSolo);
}

void lerUmidadeTemperaturaAr() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    umidadeAr = h;
    temperaturaAr = t;
    Blynk.virtualWrite(VPIN_UMIDADE_AR, umidadeAr);
    Blynk.virtualWrite(VPIN_TEMPERATURA_AR, temperaturaAr);
  } else {
    Serial.println("Falha na leitura do sensor DHT!");
  }
}

void lerNivelGas() {
  if (mq135_R0 <= 0) return;
  int valorBruto = analogRead(PINO_SENSOR_MQ135);
  float vOut = (valorBruto / 4095.0) * 3.3;
  float rs = MQ135_RL_KOHM * (3.3 - vOut) / vOut;
  float ratio = rs / mq135_R0;
  co2PPM = MQ135_PARAM_A * pow(ratio, -MQ135_PARAM_B);
  Blynk.virtualWrite(VPIN_CO2, co2PPM);
  } 

float calibrarMQ135() {
  const int leiturasCalibracao = 20;
  float r0Acumulado = 0.0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrando MQ135");
  for (int i = 0; i < leiturasCalibracao; i++) {
    float vOut = (analogRead(PINO_SENSOR_MQ135) / 4095.0) * 3.3;
    float rs = MQ135_RL_KOHM * (3.3 - vOut) / vOut;
    r0Acumulado += rs / pow(420.0 / 
    MQ135_PARAM_A, -1.0 / MQ135_PARAM_B);
    lcd.setCursor(i, 1);
    lcd.print(".");
    delay(3000);
  }
  return r0Acumulado / leiturasCalibracao;
}

void tomarDecisao() {
  // --- LÓGICA DA BOMBA ---
  if (umidadeSolo < UMIDADE_MINIMA_IRRIGACAO && !bombaLigada) {
    digitalWrite(PINO_RELE_BOMBA, HIGH);
    bombaLigada = true;
    Serial.println(">>> BOMBA LIGADA: Solo seco.");
  } else if (umidadeSolo >= UMIDADE_MAXIMA_IRRIGACAO && bombaLigada) {
    digitalWrite(PINO_RELE_BOMBA, LOW);

    bombaLigada = false;
    Serial.println(">>> BOMBA DESLIGADA: Solo úmido.");
  }
  
  // --- LÓGICA DO COOLER ---
  bool precisaLigarCooler = (temperaturaAr > TEMPERATURA_MAXIMA_LIGAR) ||
                            (umidadeAr > UMIDADE_AR_MAXIMA_LIGAR) ||
                            (co2PPM > CO2_NIVEL_MAXIMO_LIGAR);

  bool podeDesligarCooler = (temperaturaAr < TEMPERATURA_SEGURA_DESLIGAR) &&
                            (umidadeAr < UMIDADE_AR_SEGURA_DESLIGAR) &&
                            (co2PPM > CO2_IDEAL_INFERIOR && co2PPM < CO2_IDEAL_SUPERIOR);

  if (precisaLigarCooler && !coolerLigado) {
    digitalWrite(PINO_RELE_COOLER, HIGH);
    coolerLigado = true;
    Serial.println(">>> COOLER LIGADO: Condição crítica atingida.");
  } else if (podeDesligarCooler && coolerLigado) {
    digitalWrite(PINO_RELE_COOLER, LOW);
    coolerLigado = false;
    Serial.println(">>> COOLER DESLIGADO: Ambiente estabilizado.");
  }

  Blynk.virtualWrite(VPIN_STATUS_BOMBA, bombaLigada);
  Blynk.virtualWrite(VPIN_STATUS_COOLER, coolerLigado);
}


void gerenciarLCD() {
  lcdTelaAtual = (lcdTelaAtual + 1) % 3;
  lcd.clear();
  switch (lcdTelaAtual) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Umid. Solo: ");
      lcd.print(umidadeSolo);
      lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("Bomba: ");
      lcd.print(bombaLigada ? "Ligada" : "Desligada");
      break;
    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Umid. Ar: ");
      lcd.print((int)umidadeAr);
      lcd.print("%");
      lcd.setCursor(0, 1);

      lcd.print("Temp. Ar: ");
      lcd.print((int)temperaturaAr);
      lcd.write(223);
      lcd.print("C");
      break;
    case 2:
      lcd.setCursor(0, 0);
      lcd.print("CO2: ");
      lcd.print((int)co2PPM);
      lcd.print(" PPM");
      lcd.setCursor(0, 1);
      if (co2PPM < 800) lcd.print("Nivel: Baixo");
      else if (co2PPM <= 1200) lcd.print("Nivel: Ideal");
      else lcd.print("Nivel: Elevado");
      break;
  }
}