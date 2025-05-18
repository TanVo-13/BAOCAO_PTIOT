#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <ESP32Servo.h>

Servo myservo;

// Thông tin MQTT
const char *MQTTServer = "broker.emqx.io";
const char *MQTT_Topic = "HeThongNhaThongMinh";
const char *MQTT_ID = "c868b704-fd6b-44b0-8e69-b6da2c634961";
int Port = 1883;

// Khởi tạo NTPClient
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 7 * 3600, 60000); // Chỉ dùng asia.pool.ntp.org
bool ntpSynced = false;                                             // Biến kiểm tra đồng bộ thành công

WiFiClient espClient;
PubSubClient client(espClient);

// Khai báo LCD
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Khai báo chân cảm biến
#define DHTPIN 23
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
#define khigas 35
#define doamdat 34
#define mua 5
#define echoPin 25
#define trigPin 26
#define anhsang 32
#define chuyendong 12

// Khai báo chân thiết bị
#define dieuhoa 18
#define coi 19
#define maiche 15
#define led 33
#define stepPinHo 27
#define enPinHo 14
#define stepPinTuoi 0
#define enPinTuoi 17

// Biến điều khiển
bool automodeDht = true;
bool automodeMq2 = true;
float nguongDht = 30;
float nguongMq2 = 4000;
bool automodeLdr = true;
bool automodeHcsr04 = true;
float nguongLdr = 100;
float nguongPir = 1000;
float nguongHcsr04 = 20;
bool automodeDoamdat = true;
bool automodeMua = true;
float nguongDoamdat = 2000;

// Biến thời gian trễ cho PIR
unsigned long lastMotionTime = 0;
const long motionDelay = 5000;
bool motionDetected = false;

// Biến trạng thái motor hồ bơi
bool motorHoRunning = false;
unsigned long lastStepTimeHo = 0;
const unsigned long stepIntervalHo = 800;

// Biến trạng thái motor tưới
bool motorTuoiRunning = false;
unsigned long lastStepTimeTuoi = 0;
const unsigned long stepIntervalTuoi = 800;

// Cấu trúc cho lịch bật/tắt đèn
struct Schedule
{
  int year;
  int month;
  int day;
  int hour;
  int minute;
  bool state; // true = ON, false = OFF
};
Schedule schedules[10]; // Lưu tối đa 10 lịch
int scheduleCount = 0;

// Kết nối WiFi
void WIFIConnect()
{
  Serial.println("Connecting to SSID: Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST", "");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.print("WiFi connected, IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("WiFi connection failed!");
  }
}

// Kết nối lại MQTT
void MQTT_Reconnect()
{
  while (!client.connected())
  {
    if (client.connect(MQTT_ID))
    {
      Serial.print("MQTT Topic: ");
      Serial.print(MQTT_Topic);
      Serial.println(" connected");
      client.subscribe(MQTT_Topic);
      client.subscribe("HeThongNhaThongMinh/DHT22/Control/AC");
      client.subscribe("HeThongNhaThongMinh/DHT22/Control/automodeDht");
      client.subscribe("HeThongNhaThongMinh/DHT22/Control/ThresholdDht");
      client.subscribe("HeThongNhaThongMinh/KhiGas/Control/BUZZER");
      client.subscribe("HeThongNhaThongMinh/KhiGas/Control/ThresholdMq2");
      client.subscribe("HeThongNhaThongMinh/KhiGas/Control/automodeMq2");
      client.subscribe("HeThongNhaThongMinh/LDR/Control/LED");
      client.subscribe("HeThongNhaThongMinh/LDR/Control/automodeLdr");
      client.subscribe("HeThongNhaThongMinh/LDR/Control/ThresholdLdr");
      client.subscribe("HeThongNhaThongMinh/PIR/Control/ThresholdPir");
      client.subscribe("HeThongNhaThongMinh/HCSR04/Control/MOTOR");
      client.subscribe("HeThongNhaThongMinh/HCSR04/Control/automodeHcsr04");
      client.subscribe("HeThongNhaThongMinh/HCSR04/Control/ThresholdHcsr04");
      client.subscribe("HeThongNhaThongMinh/LDR/Control/ScheduleLed");
      client.subscribe("HeThongNhaThongMinh/LDR/Control/DeleteScheduleLed");
      client.subscribe("HeThongNhaThongMinh/Doamdat/Control/automodeDoamdat");
      client.subscribe("HeThongNhaThongMinh/Doamdat/Control/nguongbatmaybomtuoicay");
      client.subscribe("HeThongNhaThongMinh/Doamdat/Control/MOTOR");
      client.subscribe("HeThongNhaThongMinh/Mua/Control/automodeMua");
      client.subscribe("HeThongNhaThongMinh/Mua/Control/Mayche");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Điều khiển motor bước hồ bơi
void controlStepperMotorHo(bool state)
{
  if (state && !motorHoRunning)
  {
    digitalWrite(enPinHo, LOW);
    motorHoRunning = true;
    Serial.println("Motor ho boi da BAT");
  }
  else if (!state && motorHoRunning)
  {
    digitalWrite(enPinHo, HIGH);
    motorHoRunning = false;
    Serial.println("Motor ho boi da TAT");
  }
}

// Tạo xung bước không chặn
void runStepperMotorHo()
{
  if (motorHoRunning)
  {
    unsigned long currentTime = micros();
    if (currentTime - lastStepTimeHo >= stepIntervalHo)
    {
      digitalWrite(stepPinHo, !digitalRead(stepPinHo));
      lastStepTimeHo = currentTime;
    }
  }
}

// Điều khiển motor tưới cây mini
void controlStepperMotorTuoi(bool state)
{
  if (state && !motorTuoiRunning)
  {
    digitalWrite(enPinTuoi, LOW);
    motorTuoiRunning = true;
    Serial.println("Motor tuoi cay da BAT");
  }
  else if (!state && motorTuoiRunning)
  {
    digitalWrite(enPinTuoi, HIGH);
    motorTuoiRunning = false;
    Serial.println("Motor tuoi day da TAT");
  }
}

// Tạo xung bước không chặn
void runStepperMotorTuoi()
{
  if (motorTuoiRunning)
  {
    unsigned long currentTime = micros();
    if (currentTime - lastStepTimeTuoi >= stepIntervalTuoi)
    {
      digitalWrite(stepPinTuoi, !digitalRead(stepPinTuoi));
      lastStepTimeTuoi = currentTime;
    }
  }
}

// Xử lý tin nhắn MQTT
void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  String stMessage;
  for (int i = 0; i < length; i++)
  {
    stMessage += (char)message[i];
  }
  Serial.println(stMessage);

  if (String(topic) == "HeThongNhaThongMinh/LDR/Control/ScheduleLed")
  {
    if (scheduleCount < 10)
    {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, stMessage);
      if (!error)
      {
        schedules[scheduleCount].year = doc["year"];
        schedules[scheduleCount].month = doc["month"];
        schedules[scheduleCount].day = doc["day"];
        schedules[scheduleCount].hour = doc["hour"];
        schedules[scheduleCount].minute = doc["minute"];
        schedules[scheduleCount].state = doc["state"];
        Serial.println("Lich moi da duoc them: " + String(schedules[scheduleCount].year) + "/" +
                       String(schedules[scheduleCount].month) + "/" + String(schedules[scheduleCount].day) + " " +
                       String(schedules[scheduleCount].hour) + ":" + String(schedules[scheduleCount].minute) + " - " +
                       (schedules[scheduleCount].state ? "ON" : "OFF"));

        // Kiểm tra lịch mới với thời gian hiện tại
        time_t now = ntpSynced ? timeClient.getEpochTime() : (millis() / 1000) + 1747588089;
        if (now >= 1000000000)
        {
          struct tm *timeinfo = localtime(&now);
          int currentYear = timeinfo->tm_year + 1900;
          int currentMonth = timeinfo->tm_mon + 1;
          int currentDay = timeinfo->tm_mday;
          int currentHour = timeClient.getHours();
          int currentMinute = timeClient.getMinutes();

          if (schedules[scheduleCount].year == currentYear &&
              schedules[scheduleCount].month == currentMonth &&
              schedules[scheduleCount].day == currentDay &&
              schedules[scheduleCount].hour == currentHour &&
              schedules[scheduleCount].minute == currentMinute)
          {
            digitalWrite(led, schedules[scheduleCount].state ? HIGH : LOW);
            automodeLdr = false; // Tắt chế độ tự động
            Serial.println("Thuc thi lich moi ngay lap tuc: " + String(currentYear) + "/" +
                           String(currentMonth) + "/" + String(currentDay) + " " +
                           String(currentHour) + ":" + String(currentMinute) + " - " +
                           (schedules[scheduleCount].state ? "BAT" : "TAT"));
            // Gửi lệnh xóa lịch
            char indexStr[10];
            snprintf(indexStr, sizeof(indexStr), "%d", scheduleCount);
            client.publish("HeThongNhaThongMinh/LDR/Control/DeleteScheduleLed", indexStr);
            // Xóa lịch khỏi mảng
            for (int i = scheduleCount; i < scheduleCount; i++)
            {
              schedules[i] = schedules[i + 1];
            }
            scheduleCount--;
          }
          else
          {
            scheduleCount++;
          }
        }
        else
        {
          scheduleCount++;
        }
      }
      else
      {
        Serial.println("Loi parse JSON lich");
      }
    }
    else
    {
      Serial.println("Da dat gioi han 10 lich");
    }
  }

  if (String(topic) == "HeThongNhaThongMinh/LDR/Control/DeleteScheduleLed")
  {
    int index = stMessage.toInt();
    if (index >= 0 && index < scheduleCount)
    {
      Serial.println("Da xoa lich tai vi tri: " + String(index));
      for (int i = index; i < scheduleCount - 1; i++)
      {
        schedules[i] = schedules[i + 1];
      }
      scheduleCount--;
    }
    else
    {
      Serial.println("Vi tri lich khong hop le");
    }
  }

  if (String(topic) == "HeThongNhaThongMinh/DHT22/Control/AC")
  {
    if (stMessage == "ON")
    {
      digitalWrite(dieuhoa, HIGH);
      Serial.println("Dieu hoa da BAT");
    }
    else if (stMessage == "OFF")
    {
      digitalWrite(dieuhoa, LOW);
      Serial.println("Dieu hoa da TAT");
    }
  }
  if (String(topic) == "HeThongNhaThongMinh/DHT22/Control/automodeDht")
  {
    automodeDht = (stMessage == "ON");
    Serial.println("Chuc nang tu dong DHT22: " + String(automodeDht ? "bat" : "tat"));
  }
  if (String(topic) == "HeThongNhaThongMinh/DHT22/Control/ThresholdDht")
  {
    float newThreshold = stMessage.toFloat();
    if (newThreshold >= 10 && newThreshold <= 50)
    {
      nguongDht = newThreshold;
      Serial.println("Nguong DHT22 moi: " + String(nguongDht));
    }
  }
  if (String(topic) == "HeThongNhaThongMinh/KhiGas/Control/BUZZER")
  {
    if (stMessage == "BUZZER_ON")
    {
      digitalWrite(coi, HIGH);
      tone(coi, 1000);
      Serial.println("Coi da BAT");
    }
    else if (stMessage == "BUZZER_OFF")
    {
      digitalWrite(coi, LOW);
      noTone(coi);
      Serial.println("Coi da TAT");
    }
  }
  if (String(topic) == "HeThongNhaThongMinh/KhiGas/Control/automodeMq2")
  {
    automodeMq2 = (stMessage == "ON");
    Serial.println("Chuc nang tu dong MQ2: " + String(automodeMq2 ? "bat" : "tat"));
  }
  if (String(topic) == "HeThongNhaThongMinh/KhiGas/Control/ThresholdMq2")
  {
    float newThreshold = stMessage.toFloat();
    if (newThreshold >= 0)
    {
      nguongMq2 = newThreshold;
      Serial.println("Nguong khi gas moi: " + String(nguongMq2));
    }
  }
  if (String(topic) == "HeThongNhaThongMinh/LDR/Control/LED")
  {
    if (stMessage == "LIGHT_ON")
    {
      digitalWrite(led, HIGH);
      automodeLdr = false; // Giữ chế độ thủ công
      Serial.println("Den da BAT");
    }
    else if (stMessage == "LIGHT_OFF")
    {
      digitalWrite(led, LOW);
      automodeLdr = false; // Giữ chế độ thủ công
      Serial.println("Den da TAT");
    }
  }
  if (String(topic) == "HeThongNhaThongMinh/LDR/Control/automodeLdr")
  {
    automodeLdr = (stMessage == "ON");
    Serial.println("Chuc nang tu dong LDR: " + String(automodeLdr ? "bat" : "tat"));
  }
  if (String(topic) == "HeThongNhaThongMinh/LDR/Control/ThresholdLdr")
  {
    float newThreshold = stMessage.toFloat();
    if (newThreshold >= 0 && newThreshold <= 1000)
    {
      nguongLdr = newThreshold;
      Serial.println("Ngưỡng LDR mới: " + String(nguongLdr));
    }
  }
  if (String(topic) == "HeThongNhaThongMinh/PIR/Control/ThresholdPir")
  {
    float newThreshold = stMessage.toFloat();
    if (newThreshold >= 1000 && newThreshold <= 2000)
    {
      nguongPir = newThreshold;
      Serial.println("Ngưỡng PIR mới: " + String(nguongPir));
    }
  }
  if (String(topic) == "HeThongNhaThongMinh/HCSR04/Control/MOTOR")
  {
    controlStepperMotorHo(stMessage == "MOTOR_ON");
  }
  if (String(topic) == "HeThongNhaThongMinh/HCSR04/Control/automodeHcsr04")
  {
    automodeHcsr04 = (stMessage == "ON");
    Serial.println("Chuc nang tu dong HCSR04: " + String(automodeHcsr04 ? "bat" : "tat"));
  }
  if (String(topic) == "HeThongNhaThongMinh/HCSR04/Control/ThresholdHcsr04")
  {
    float newThreshold = stMessage.toFloat();
    if (newThreshold >= 20 && newThreshold <= 50)
    {
      nguongHcsr04 = newThreshold;
      Serial.println("Ngưỡng HCSR04 mới: " + String(nguongHcsr04));
    }
  }

  if (String(topic) == "HeThongNhaThongMinh/Doamdat/Control/automodeDoamdat")
  {
    automodeDoamdat = (stMessage == "ON");
    Serial.println("Chuc nang tu dong do am dat: " + String(automodeDoamdat ? "bat" : "tat"));
  }

  if (String(topic) == "HeThongNhaThongMinh/Doamdat/Control/nguongbatmaybomtuoicay")
  {
    float newThreshold = stMessage.toFloat();
    if (newThreshold >= 0 && newThreshold <= 4095)
    {
      nguongDoamdat = newThreshold;
      Serial.println("Ngưỡng độ ẩm đất mới: " + String(nguongDoamdat));
    }
  }

  if (String(topic) == "HeThongNhaThongMinh/Doamdat/Control/MOTOR")
  {
    controlStepperMotorTuoi(stMessage == "MOTOR_ON");
    automodeDoamdat = false; // Giữ chế độ thủ công
  }

  if (String(topic) == "HeThongNhaThongMinh/Mua/Control/automodeMua")
  {
    automodeMua = (stMessage == "ON");
    Serial.println("Chuc nang tu dong may che: " + String(automodeMua ? "bat" : "tat"));
  }

  if (String(topic) == "HeThongNhaThongMinh/Mua/Control/Mayche")
  {
    automodeMua = false; // Giữ chế độ thủ công
    if (stMessage == "ON")
    {
      myservo.write(180);
      delay(500);
      Serial.println("May che da BAT");
    }
    else if (stMessage == "OFF")
    {
      myservo.write(0);
      delay(500);
      Serial.println("May che da TAT");
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Testing buzzer on GPIO 19");
  digitalWrite(coi, HIGH);
  delay(1000);
  digitalWrite(coi, LOW);
  Serial.println("Buzzer test completed");

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Dang ket noi WiFi");

  WIFIConnect();
  if (WiFi.status() == WL_CONNECTED)
  {
    client.setServer(MQTTServer, Port);
    client.setCallback(callback);
    timeClient.begin();
    delay(2000); // Chờ khởi tạo UDP

    // Thử đồng bộ với asia.pool.ntp.org
    for (int j = 0; j < 5; j++)
    {
      timeClient.forceUpdate();
      if (timeClient.getEpochTime() > 1000000000)
      {
        ntpSynced = true;
        Serial.println("Dong bo NTP thanh cong, Thoi gian: " + timeClient.getFormattedTime() +
                       ", Epoch: " + String(timeClient.getEpochTime()));
        break;
      }
      else
      {
        Serial.println("Dong bo NTP that bai, lan " + String(j + 1));
        delay(1000);
      }
    }
    if (!ntpSynced)
    {
      Serial.println("Khong the dong bo thoi gian NTP! Su dung thoi gian mo phong.");
    }
  }
  else
  {
    Serial.println("Khong co ket noi WiFi, khong the dong bo NTP!");
  }

  dht.begin();
  myservo.attach(maiche);
  myservo.write(0);
  pinMode(dieuhoa, OUTPUT);
  pinMode(coi, OUTPUT);
  pinMode(doamdat, INPUT);
  pinMode(mua, INPUT);
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(anhsang, INPUT);
  pinMode(chuyendong, INPUT);
  pinMode(led, OUTPUT);
  pinMode(stepPinHo, OUTPUT);
  pinMode(enPinHo, OUTPUT);
  pinMode(stepPinTuoi, OUTPUT);
  pinMode(enPinTuoi, OUTPUT);
  digitalWrite(enPinHo, HIGH);
  digitalWrite(enPinTuoi, HIGH);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi da ket noi");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
}

// Hàm checkSchedules() sửa đổi: Tắt automodeLdr và xóa lịch sau khi thực thi
void checkSchedules()
{
  time_t now = ntpSynced ? timeClient.getEpochTime() : (millis() / 1000) + 1747588089;
  if (now < 1000000000)
  {
    Serial.println("Thoi gian khong hop le!");
    return;
  }

  struct tm *timeinfo = localtime(&now);
  int currentYear = timeinfo->tm_year + 1900;
  int currentMonth = timeinfo->tm_mon + 1;
  int currentDay = timeinfo->tm_mday;
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();

  // Kiểm tra lịch
  for (int i = 0; i < scheduleCount; i++)
  {
    if (schedules[i].year == currentYear &&
        schedules[i].month == currentMonth &&
        schedules[i].day == currentDay &&
        schedules[i].hour == currentHour &&
        schedules[i].minute == currentMinute)
    {
      digitalWrite(led, schedules[i].state ? HIGH : LOW);
      automodeLdr = false; // Tắt chế độ tự động
      Serial.println("Thuc thi lich: " + String(schedules[i].year) + "/" +
                     String(schedules[i].month) + "/" + String(schedules[i].day) + " " +
                     String(schedules[i].hour) + ":" + String(schedules[i].minute) + " - " +
                     (schedules[i].state ? "BAT" : "TAT"));
      // Gửi lệnh xóa lịch
      char indexStr[10];
      snprintf(indexStr, sizeof(indexStr), "%d", i);
      client.publish("HeThongNhaThongMinh/LDR/Control/DeleteScheduleLed", indexStr);
      // Xóa lịch khỏi mảng
      for (int j = i; j < scheduleCount - 1; j++)
      {
        schedules[j] = schedules[j + 1];
      }
      scheduleCount--;
      i--;
    }
  }
}

void loop()
{
  delay(10);
  if (!client.connected())
  {
    MQTT_Reconnect();
  }
  client.loop();
  runStepperMotorHo();
  runStepperMotorTuoi();
  checkSchedules();

  static unsigned long lastSend = 0;
  if (millis() - lastSend > 2000)
  {
    lastSend = millis();

    // Đọc cảm biến
    float nhietdo = dht.readTemperature();
    float doam = dht.readHumidity();
    int mq2_value = analogRead(khigas);
    int raw_ldr_value = analogRead(anhsang);
    int ldr_value = 4095 - raw_ldr_value;
    int chuyendong_value = digitalRead(chuyendong);
    int doamdat_value = analogRead(doamdat);
    int trangthai_mua = digitalRead(mua);

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duration = pulseIn(echoPin, HIGH);
    float khoangcach = duration * 0.034 / 2;

    if (!isnan(nhietdo) && automodeDht)
    {
      digitalWrite(dieuhoa, nhietdo > nguongDht ? HIGH : LOW);
      Serial.println("Dieu hoa dang " + String(nhietdo > nguongDht ? "bat" : "tat"));
    }

    if (!isnan(mq2_value) && automodeMq2)
    {
      if (mq2_value > nguongMq2)
      {
        digitalWrite(coi, HIGH);
        Serial.println("Coi bat do khi gas vuot nguong: " + String(mq2_value));
      }
      else
      {
        digitalWrite(coi, LOW);
        Serial.println("Coi tat do khi gas duoi nguong: " + String(mq2_value));
      }
    }

    if (!isnan(ldr_value) && !isnan(chuyendong_value) && automodeLdr)
    {
      if (chuyendong_value == HIGH)
      {
        lastMotionTime = millis();
        if (!motionDetected)
        {
          motionDetected = true;
          Serial.println("Phat hien chuyen dong!");
        }
      }

      if (ldr_value < nguongLdr || (ldr_value < nguongPir && motionDetected))
      {
        digitalWrite(led, HIGH);
        Serial.println("Den bat do anh sang thap hoac anh sang thap va co chuyen dong");
      }
      else
      {
        digitalWrite(led, LOW);
        Serial.println("Den tat");
      }

      if (motionDetected && (millis() - lastMotionTime >= motionDelay))
      {
        motionDetected = false;
        Serial.println("Khong con chuyen dong.");
      }
    }

    if (!isnan(khoangcach) && automodeHcsr04)
    {
      controlStepperMotorHo(khoangcach > nguongHcsr04);
    }

    if (!isnan(doamdat_value) && automodeDoamdat)
    {
      controlStepperMotorTuoi(doamdat_value < nguongDoamdat);
    }

    Serial.print("Rain sensor: ");
    Serial.println(trangthai_mua);

    if (automodeMua)
    {
      if (trangthai_mua == LOW)
      {
        myservo.write(0); // đóng mái
        delay(500);
        Serial.println("Đóng mái che");
      }
      else
      {
        myservo.write(180); // mở mái
        delay(500);
        Serial.println("Mở mái che");
      }
    }

    if (!isnan(nhietdo) && !isnan(doam) &&
        !isnan(mq2_value) && !isnan(ldr_value) &&
        !isnan(chuyendong_value) && !isnan(doamdat_value) &&
        !isnan(khoangcach) && !isnan(trangthai_mua))
    {
      Serial.print("Nhiet do: ");
      Serial.print(nhietdo);
      Serial.print(" *C, ");
      Serial.print("Do am: ");
      Serial.print(doam);
      Serial.print(" %, ");
      Serial.print("Khi gas: ");
      Serial.print(mq2_value);
      Serial.print(", ");
      Serial.print("Cuong do anh sang: ");
      Serial.print(ldr_value);
      Serial.print(", ");
      Serial.print("Chuyen dong: ");
      Serial.print(chuyendong_value);
      Serial.print(", ");
      Serial.print("Do am dat: ");
      Serial.print(doamdat_value);
      Serial.print(", ");
      Serial.print("Khoang cach: ");
      Serial.print(khoangcach);
      Serial.print(" cm, ");
      Serial.print("Mua: ");
      Serial.println(trangthai_mua);

      StaticJsonDocument<2048> doc;
      doc["nhietdo"] = nhietdo;
      doc["doam"] = doam;
      doc["khigas"] = mq2_value;
      doc["anhsang"] = ldr_value;
      doc["chuyendong"] = chuyendong_value;
      doc["doamdat"] = doamdat_value;
      doc["khoangcach"] = khoangcach;
      doc["mua"] = trangthai_mua;
      doc["automodeDht"] = automodeDht;
      doc["nguongbatdieuhoa"] = nguongDht;
      doc["automodeMq2"] = automodeMq2;
      doc["nguongbatcoi"] = nguongMq2;
      doc["automodeLdr"] = automodeLdr;
      doc["nguongLdr"] = nguongLdr;
      doc["nguongPir"] = nguongPir;
      doc["automodeHcsr04"] = automodeHcsr04;
      doc["nguongbatmaybomho"] = nguongHcsr04;
      doc["automodeDoamdat"] = automodeDoamdat;
      doc["nguongbatmaybomtuoicay"] = nguongDoamdat;
      doc["automodeMua"] = automodeMua;

      char buffer[2048];
      serializeJson(doc, buffer);
      client.publish(MQTT_Topic, buffer);
    }
    else
    {
      Serial.println("Doc du lieu that bai!");
    }
  }
}