/* 13-02-2022 Проект устройства № 5 */
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
// Библиотека Arduino_JSON для упрощения обработки строк JSON
#include <Arduino_JSON.h>
#include <SPI.h>
#include <ESP8266mDNS.h> // Multicast Domain Name System

// LITTLEFS
#include <LittleFS.h>
FS* fileSystem = &LittleFS;
LittleFSConfig fileSystemConfig = LittleFSConfig();

// контакт для передачи данных подключен к D1 на ESP8266 12-E (GPIO5):
const byte ONE_WIRE_BUS = 5;

byte op_time_num = 0;
uint addr = 0;
char start_operation[10] = { '1', '3', '-', '0', '2', '-', '2', '0', '2', '2' }; // = ""; // Начало работы МК 13-02-2022
//char start_operation[11] = "13-02-2022"; // Начало работы МК 13-02-2022
unsigned long operating_time; // = 0; // Наработка МК - 4 байта

// Wi-Fi
const char* ssid = "TP-LINK_F53126";  // SSID
const char* password = "T-ER2-748-ZV6-6MN"; // пароль

unsigned long previousMillis = 0;
unsigned long previousMillis_blink = 0;
const long interval = 30000; // 30 c
long interval_blink = 1000; // 2 c

#define TEMPERATURE_PRECISION 12 // точность измерений (9 ... 12)
enum Mode_operation {WORK_MODE, SEARCH_MODE};

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
// Создать новый источник событий на /events
AsyncEventSource events("/events");

// создаем экземпляр класса oneWire;
OneWire oneWire(ONE_WIRE_BUS);

// передаем объект oneWire объекту sensors:
DallasTemperature sensors(&oneWire);
// Variables to store temperature values
//String temperatureF = "";
String temperatureC = "";
String startOperation = "";
String operatingTime[3];
Mode_operation mode_operation = WORK_MODE;
unsigned long frequency;// = 1730000;
unsigned long frequency_opt = 0; // Если оптимальная частота = 0,
// значит она еще не определена в процессе настройки или
// уже передана клиенту (и не определена перед следующим сеансом настройки).
String device_current_init; // текущий ток устройства, мА
int measurement_num = 0; // Номер измерения напряжения
float voltage = 0; // переменная для хранения считываемого значения напряжения на эмиттере
float voltage_average = 0;
const int NUMBER_MEASUREMENTS = 99;
const byte NUM_STEP_FREQ = 41; // Количество шагов приращений частоты в режиме настройки
int current_freq_arr[NUM_STEP_FREQ] = {0}; // Значения напряжений для 41 частоты (1,50..1,90 МГц)
int freq_num; // Текущий номер частоты (для правильного расположения напряжений в массиве current_freq_arr
unsigned long df = 10000;

// Генератор
const float refFreq = 25000000.0;           // On-board crystal reference frequency
const int wSine     = 0b0000000000000000; // 0x00
const int wTriangle = 0b0000000000000010; // 0x02
const int wSquare   = 0b0000000000101000; // 0x28
int waveType = wSquare;
const int SG_fsyncPin = 15;//2;
const int SG_CLK = 14;//4;
const int SG_DATA = 13;//3;

const int CURRENT_SETUP[41] = {1100, 1140, 1150, 1140, 1160, 1170, 1190, 1230, 1280, 1320, 1340, 1420, 1480, 1510, 1580, 1660, 1740, 1870, 2000, 2140, 2350, 2530, 2660, 2670, 2430, 2110, 1680, 1370, 1170, 1070, 1060, 1010, 950, 860, 820, 830, 790, 790, 790, 810, 830};

#define FREQ_BEGIN 1500000

// 5. Переменная readings_op_time — это переменная JSON для хранения
// показаний датчика в формате JSON
JSONVar readings_op_time, readings_temperature, readings_current;

// Адреса датчиков DS18B20
DeviceAddress ds18b20_1_address = { 0x28, 0x89, 0x55, 0x48, 0xF6, 0x0B, 0x3C, 0xBB }; // Ближний
DeviceAddress ds18b20_2_address = { 0x28, 0xEB, 0xA3, 0x48, 0xF6, 0xE8, 0x3C, 0xC }; // Дальний
DeviceAddress ds18b20_3_address = { 0x28, 0xC4, 0x7B, 0x48, 0xF6, 0x2A, 0x3C, 0xDB }; // 2-й дальний

String TemperatureC_init;

void operating_time_add();
String readDSTemperatureC();
void format_operating_time();
void notFound(AsyncWebServerRequest *request);
void setting_begin(); // Начало режима настройки
void InitSigGen();
void SG_WriteRegister(word dat);
void SG_Reset();
void SG_freqReset();
void SG_freqSet();

void setup()
{
  // The blue LED on the ESP-01 module is connected to GPIO1
  // (which is also the TXD pin; so we cannot use Serial.print() at the same time)
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
  // but actually the LED is on; this is because it is active low on the ESP-01)

  // 1. Инициализация последовательного монитора
  Serial.begin(115200);
  // 2. Инициализация файловой системы
  delay(100);
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  } else {
    Serial.println("Filesystem LittleFS initialized.");
  }
  // 3. Инициализация EEPROM
  /*
   EEPROM
   0-9B: start_operation
   10-13B: operating_time
   14-17B: frequency optimal
  */
  // Адрес для записи наработки МК
  addr += 10 * sizeof(char); // Первые 10 байт - дата начала работы

  EEPROM.begin(512);

  /*
    EEPROM
    0-9B: start_operation
    10-13B: operating_time
    14-17B: frequency optimal
  */
  /*
    //start_operation[10] = { '1', '3', '-', '0', '2', '-', '2', '0', '2', '2' };
    EEPROM.put(0, start_operation);
    operating_time = 5120; // 3 d 10 h 0 m
    EEPROM.put(10, operating_time);
    frequency = 1730000;
    EEPROM.put(14, frequency);
    EEPROM.commit(); // сохранить изменения.
  */

  // 4. Считывание даты начала работы, времени наработки МК, оптимальной частоты
  EEPROM.get(0, start_operation);
  EEPROM.get(10, operating_time);
  EEPROM.get(14, frequency);

  // 5. Присвоение строковой String startOperation значения start_operation, прочитанного из EEPROM
  startOperation = start_operation;

  // 6. Присвоение строковому массиву String operatingTime[3] значений дней, часов, минут
  format_operating_time();

  // 8. Инициализация переменной DallasTemperature sensors датчиков
  sensors.begin(); // по умолчанию разрешение датчика – 9-битное;
  // 9. Изменение точности датчиков (от 9 до 12)
  //sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);

  // 10. Присвоение первоначальных значений переменным температуры и тока
  // Сначала запросим т-ру, чтобы ответить на первоначальный вызов
  // (её нельзя напрямую запрашивать из обратного вызова)
  // String TemperatureC_init String device_current_init
  TemperatureC_init = readDSTemperatureC();
  // Прочитать ток device_current - это voltage_average...
  // int current_imit()
  device_current_init = String(CURRENT_SETUP[23] + random(30)); // Временное решение

  // 11. Подключение к сети WiFi
  Serial.println("Connecting to ");
  Serial.println(ssid);
  // подключиться к вашей локальной wi-fi сети
  WiFi.begin(ssid, password);
  // проверить, подключился ли wi-fi модуль к wi-fi сети
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {              // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  // 12 Ответы на запросы клиента

  // 12.1. Ответ на запрос по корневому URL-адресу.
  // Отправляется текст, хранящийся на /index.html для создания веб-страницы
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  // 12.2 Обслуживание других статических файлов, запрошенных клиентом (style.css и script.js)
  server.on("/main.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/main.css", "text/css");
  });
  server.on("/main.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/main.js", "text/javascript");
  });

  server.on("/highcharts.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/highcharts.js", "text/javascript");
  });

  server.on("/Oil", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/Oil.png", "image/png");
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/favicon.ico", "image/x-icon");
  });

  // 12.3 Ответ на запрос даты начала работы
  server.on("/start_operation", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", startOperation.c_str());
  });

  // 12.4 Ответ на запрос текущей частоты
  server.on("/set_frequency", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(frequency).c_str());
  });

  // 12.5 Ответ на запрос увеличения частоты
  server.on("/freq_inc", HTTP_GET, [](AsyncWebServerRequest * request) {
    frequency += 10000;
    measurement_num = 0;
    voltage = 0;
    SG_freqSet();
    delay(100);
    request->send_P(200, "text/plain", String(frequency).c_str());
  });

  // 12.6 Ответ на запрос уменьшения частоты
  server.on("/freq_dec", HTTP_GET, [](AsyncWebServerRequest * request) {
    frequency -= 10000;
    measurement_num = 0;
    voltage = 0;
    SG_freqSet();
    delay(100);
    request->send_P(200, "text/plain", String(frequency).c_str());
  });

  // 12.7 Ответ на начальный запрос температуры
  server.on("/temperature_init", HTTP_GET, [](AsyncWebServerRequest * request) {
    // Здесь (из асинхронного обратного вызова веб-сервера) нельзя напрямую вызывать методы DallasTemperature.
    // Т.к. методы DallasTemperature вызывают delay().
    // Вместо этого нужно, чтобы в loop() периодически считывалась температура,
    // сохранялась в глобальных переменных, а затем значения этих переменных возвращались в обратных вызовах,
    // без вызова библиотек Dallas Temperature напрямую.
    request->send_P(200, "text/plain", TemperatureC_init.c_str());
  });

  // 12.8 Ответ на начальный запрос текущего тока
  server.on("/device_current_init", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", device_current_init.c_str());
  });

  // 12.9 Ответ на начальный запрос времени нработки
  server.on("/operating_time_init", HTTP_GET, [](AsyncWebServerRequest * request) {
    // Формируем строку для отправки
    readings_op_time["days"] = operatingTime[0];
    readings_op_time["hours"] = operatingTime[1];
    readings_op_time["mins"] = operatingTime[2];
    String jsonString = JSON.stringify(readings_op_time);
    request->send_P(200, "text/plain", jsonString.c_str());
  });

  // 12.10 Ответ на запрос на запуск режима настройки
  server.on("/run_setup", HTTP_GET, [](AsyncWebServerRequest * request) {
    mode_operation = SEARCH_MODE;
    request->send_P(200, "text/plain", String(mode_operation).c_str());
    setting_begin(); // Начинаем настройку
  });

  server.onNotFound(notFound);

  // 13. Настройка источника событий сервера
  events.onConnect([](AsyncEventSourceClient * client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  // 14. Init AD9833
  InitSigGen();

  // 15. Запуск сервера
  server.begin();
  Serial.println("HTTP server started");

  //MDNS.update();
}

void loop() {
  // Если обычный режим, то выполняем
  // 1 Читаем данные датчика т-ры и тока
  unsigned long currentMillis = millis(); // Записываем текущее время в currentMillis
  if (currentMillis - previousMillis_blink >= interval_blink) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));  // Turn the LED
    previousMillis_blink = currentMillis;
    interval_blink = random(1000);
  }
  if (currentMillis - previousMillis >= interval) {
    if (mode_operation == WORK_MODE) {
      // Измеряем температуру
      events.send(readDSTemperatureC().c_str(), "temperature_new" , millis());
      delay(50);
      // Измеряем ток
      events.send(String(CURRENT_SETUP[23] + random(30)).c_str(), "device_current_new" , millis());
      delay(50);
    }
    op_time_num++;
    // Здесь мы оказываемся каждые 30 с, но добавляем минуту в два раза реже
    if (op_time_num == 2) {
      // Время наработки МК
      operating_time_add();
      // Формируем строку для отправки
      readings_op_time["days"] = operatingTime[0];
      readings_op_time["hours"] = operatingTime[1];
      readings_op_time["mins"] = operatingTime[2];
      String jsonString = JSON.stringify(readings_op_time);
      if (mode_operation == WORK_MODE) {
        events.send(jsonString.c_str(), "operating_time_new" , millis());
        delay(50);
      }
      op_time_num = 0;
    }
    previousMillis = currentMillis;
  }

  // 2 Измеряем напряжение на эмиттере (ток устройства)
  int voltage_curr = 1238; //analogRead(voltagePin);     // считываем значение
  measurement_num++; // Увеличиваем номер измерения
  // Усредняяем
  voltage = (measurement_num * voltage + voltage_curr) / (measurement_num + 1); // Yi+1=0.9Yi+0.1Xi
  // При достижении числа измерений величины NUMBER_MEASUREMENTS запоминаем среднее
  // для передачи клиентам или для размещения в массиве в режиме насройки
  if (measurement_num == NUMBER_MEASUREMENTS) {
    voltage_average = voltage;
    // Если режим настройки, то выполняем
    if (mode_operation == SEARCH_MODE) {
      // Запоминаем напряжение для этого значения частоты
      current_freq_arr[freq_num] = voltage_average * 1000; // mA
      // Для имитации. Удалить в ральной схеме. y=2043,7x4 - 13920x3+35447x2-39997x+16875
      //current_freq_arr[freq_num] = current_imit();
      current_freq_arr[freq_num] = CURRENT_SETUP[freq_num] + random(30);

      // Отправляем значения частоты frequency, тока current_freq_arr[freq_num]
      // Формируем строку для отправки
      readings_current["freq"] = String(frequency);
      readings_current["curr"] = String(current_freq_arr[freq_num]);
      String jsonString = JSON.stringify(readings_current);
      events.send(jsonString.c_str(), "setup_current_new" , millis());
      delay(50);

      freq_num++;
      // Если не достигли конца диапазона частот
      if (freq_num < NUM_STEP_FREQ) {
        // Устанавливаем следующую частоту
        frequency += 10000;
        SG_freqSet();
        delay(100);
      } else { // Достигли конца диапазона частот, заполнен весь массив current_freq_arr
        // Определяем максимальное значение и соответствующую частоту
        byte maxIndex = 0;
        int maxValue = current_freq_arr[maxIndex];
        for (byte i = 1; i < NUM_STEP_FREQ; i++)
        {
          if (current_freq_arr[i] > maxValue) {
            maxValue = current_freq_arr[i];
            maxIndex = i;
          }
        }
        // Фактическая наилучшая частота
        frequency_opt = FREQ_BEGIN + (unsigned long)maxIndex * 10000;
        frequency = frequency_opt;
        // Отображаем результат поиска
        /*
          display.println(maxIndex);
          display.println(frequency); // freq_max
          display.println(maxValue); // current max
          for (byte i = 0; i < num_step_freq_inc; i++)
          {
          display.print(current_freq_arr[i]);
          }
        */
        // Отправляем значения оптимальной частоты frequency_opt и
        // тока current_freq_arr[freq_num]
        // Формируем строку для отправки
        readings_current["freq"] = String(frequency_opt);
        readings_current["curr"] = String(32000);
        String jsonString = JSON.stringify(readings_current);
        events.send(jsonString.c_str(), "setup_current_new" , millis());
        delay(50);

        // Задержка для просмотра результата м переход в рабочий режим с новой лучшей частотой
        //delay(15000);
        // Записываем наилучшую частоту в EEPROM для использования при старте МК в следующий раз
        EEPROM.put(14, frequency);
        // Записываем измеренные для всех частот напряжения в EEPROM
        /*
          int addr = 6;
          int volt_int;
          for (byte i = 0; i < num_step_freq_inc; i++)
          {
          volt_int = current_freq_arr[i];
          EEPROM.put(addr, volt_int); // Записываем напряжение
          addr += 2;
          }
        */
        EEPROM.commit(); // сохранить изменения
        // Переходим в рабочий режим
        mode_operation = WORK_MODE;
        // Устанавливаем наилучшую частоту
        SG_freqSet();
      }
    }
    measurement_num = 0;
    voltage = 0;
  }
  MDNS.update();
}

// Имитация получения тока для данной частоты
int current_imit() {
  float freq_imit = frequency / 1000000.;
  return random(30) + 1000 * (2043.7 * pow(freq_imit, 4) - 13920 * pow(freq_imit, 3) + 35447 * pow(freq_imit, 2) - 39997 * freq_imit + 16875);
}

// Начало режима настройки
void setting_begin() {
  frequency = FREQ_BEGIN;
  measurement_num = 0; // Номер измерения напряжения
  freq_num = 0; // Текущий номер частоты (для правильного расположения напряжений в массиве current_freq_arr
  SG_freqSet();
  delay(100); // Задержка 100 мс
}

void InitSigGen() {

  pinMode(SG_DATA, OUTPUT);
  pinMode(SG_CLK, OUTPUT);
  pinMode(SG_fsyncPin, OUTPUT);
  digitalWrite(SG_fsyncPin, HIGH);
  digitalWrite(SG_CLK, HIGH);
  SG_Reset();
  SG_freqSet();

}

void SG_WriteRegister(word dat) {

  digitalWrite(SG_CLK, LOW);
  digitalWrite(SG_CLK, HIGH);

  digitalWrite(SG_fsyncPin, LOW);
  for (byte i = 0; i < 16; i++) {
    if (dat & 0x8000)
      digitalWrite(SG_DATA, HIGH);
    else
      digitalWrite(SG_DATA, LOW);
    dat = dat << 1;
    digitalWrite(SG_CLK, HIGH);
    digitalWrite(SG_CLK, LOW);
  }
  digitalWrite(SG_CLK, HIGH);
  digitalWrite(SG_fsyncPin, HIGH);

}

void SG_Reset() {

  delay(100);
  SG_WriteRegister(0x100);
  delay(100);

}

void SG_freqReset() {

  long fl = frequency * (0x10000000 / refFreq);
  SG_WriteRegister(0x2100);
  SG_WriteRegister((int)(fl & 0x3FFF) | 0x4000);
  SG_WriteRegister((int)((fl & 0xFFFC000) >> 14) | 0x4000);
  SG_WriteRegister(0xC000);
  SG_WriteRegister(waveType);

}
void SG_freqSet() {

  long fl = frequency * (0x10000000 / refFreq);
  SG_WriteRegister(0x2000 | waveType);
  SG_WriteRegister((int)(fl & 0x3FFF) | 0x4000);
  SG_WriteRegister((int)((fl & 0xFFFC000) >> 14) | 0x4000);

}

// Добавление минуты ко времени наработки и измерение температуры (раз в минуту)
void operating_time_add()
{
  operating_time += 1;
  EEPROM.put(addr, operating_time);
  EEPROM.commit(); // сохранить изменения.
  //Serial.println("Found: " + String(start_operation) + "," + String(operating_time));
  // Формируем строку наработки МК
  format_operating_time();
}

void format_operating_time() {
  // Наработка
  int days = operating_time / 1440; // Дней. Целочисленное деление (без округления, остаток отбрасывается)
  int min_remainder = operating_time % 1440; // Минут в остатке. Возвращает остаток от деления одного целого (int) операнда на другой.
  int hours = min_remainder / 60; // Часов от 0 до 24
  int minutes = min_remainder % 60; // % Возвращает остаток от деления одного целого (int) операнда на другой.
  String days_s = String(days);
  if (days < 10) days_s = "0" + days_s;
  String hours_s = String(hours);
  if (hours < 10) hours_s = "0" + hours_s;
  String minutes_s = String(minutes);
  if (minutes < 10) minutes_s = "0" + minutes_s;
  operatingTime[0] = days_s;
  operatingTime[1] = hours_s;
  operatingTime[2] = minutes_s;
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String readDSTemperatureC() {
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  //Serial.println("Ответ на запрос температуры 1");
  sensors.requestTemperatures(); // Вызов метода на объекте датчиков
  //Serial.println("Ответ на запрос температуры 2");
  // Сохранить показания в строке JSON (переменная jsonString) и вернуть эту переменную
  readings_temperature["Heatsink"] = String(sensors.getTempC(ds18b20_1_address));
  readings_temperature["Ambient"] = String(sensors.getTempC(ds18b20_2_address));
  readings_temperature["3rd sensor"] = String(sensors.getTempC(ds18b20_3_address));

  //Serial.println(readings_temperature);
  String jsonString = JSON.stringify(readings_temperature);
  return jsonString;
}
