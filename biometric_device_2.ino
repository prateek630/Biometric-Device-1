// ULTRA CLEAN VERSION (OPEN / NO PASSWORD PROTECTION)
// SAME SERIAL COMMANDS KEPT
// FULL CODE - SINGLE FILE
//
// NOTE:
// LOGIN command always succeeds now.
// No password hashing.
// Open config mode.
//
// BOARD: ESP32
void factoryReset();
void filesReset();
void sensorReset();

#include <SPI.h>
#include <SD.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include "RTClib.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "display_fun.h"
// =====================================================
// FILE PATHS
// =====================================================
#define SENT_FILE "/sent.txt"
#define LOG_FILE     "/logs.txt"
#define USER_FILE    "/users.json"
#define CONFIG_FILE  "/config.json"
#define TEMP_FILE    "/temp.txt"
#define SD_CS       13
#define SD_LOG_FILE "/sdlogs.txt"
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64


// =====================================================
// CONFIG
// =====================================================
String DEVICE_ID = "";

String WIFI_SSID = "";
String WIFI_PASS = "";

String ADMIN_USER = "admin";
String ADMIN_PASS = "1234";

const char* SERVER_URL = "https://script.google.com/macros/s/AKfycbwavJw9ejqcDxqowvAQeQ6Bkuzh2O1VC-Jv8YWJNWP_zzCd4Ul_f-QwvTfvJslIDOn2kA/exec";

const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

// =====================================================
// OBJECTS
// =====================================================
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);
RTC_DS3231 rtc;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// =====================================================
// STATE
// =====================================================
enum DeviceMode
{
  MODE_ATTENDANCE,
  MODE_CONFIG
};

DeviceMode currentMode = MODE_ATTENDANCE;

bool rtcTimeValid = false;
bool clearStep1 = false;

unsigned long  lastSerialActivity = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastNTPSync = 0;

int lastFingerID = -1;
unsigned long lastAttendanceTime = 0;

TaskHandle_t logTask;

// =====================================================
// UTIL
// =====================================================
String generateDeviceID()
{
  uint64_t chipid = ESP.getEfuseMac();

  char id[25];

  sprintf(id, "ESP32_%04X%08X",
          (uint16_t)(chipid >> 32),
          (uint32_t)chipid);

  return String(id);
}

bool isRTCTimeValid()
{
  DateTime now = rtc.now();
  return (now.year() >= 2024 && now.year() <= 2035);
}

bool writeText(String path, String data)
{
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  f.print(data);
  f.close();
  return true;
}

// =====================================================
// CONFIG
// =====================================================
void saveConfig()
{
  DynamicJsonDocument doc(512);

  doc["ssid"] = WIFI_SSID;
  doc["pass"] = WIFI_PASS;
  doc["admin_user"] = ADMIN_USER;
  doc["admin_pass"] = ADMIN_PASS;

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return;

  serializeJson(doc, f);
  f.close();
}

void loadConfig()
{
  if (!LittleFS.exists(CONFIG_FILE))
  {
    saveConfig();
    return;
  }

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) return;

  DynamicJsonDocument doc(512);

  if (!deserializeJson(doc, f))
  {
    WIFI_SSID = doc["ssid"] | "";
    WIFI_PASS = doc["pass"] | "";
    ADMIN_USER = doc["admin_user"] | "admin";
    ADMIN_PASS = doc["admin_pass"] | "1234";
  }

  f.close();
}
//----------------------------------------------
int getSentCount()
{
  if (!LittleFS.exists(SENT_FILE))
    return 0;

  File f = LittleFS.open(SENT_FILE, "r");
  int n = f.readString().toInt();
  f.close();
  return n;
}

void saveSentCount(int n)
{
  File f = LittleFS.open(SENT_FILE, "w");
  f.print(n);
  f.close();
}

// =====================================================
// STORAGE
// =====================================================
void initStorage()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS FAILED");
    while (1);
  }

  if (!LittleFS.exists(LOG_FILE))
    writeText(LOG_FILE, "");

  if (!LittleFS.exists(USER_FILE))
    writeText(USER_FILE, "[]");
}

// =====================================================
// WIFI
// =====================================================
bool syncRTCWithNTP()
{
  if (WiFi.status() != WL_CONNECTED)
    return false;

  configTime(gmtOffset_sec, daylightOffset_sec,
             "time.google.com",
             "time.nist.gov");

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 15000))
    return false;

  rtc.adjust(DateTime(
               timeinfo.tm_year + 1900,
               timeinfo.tm_mon + 1,
               timeinfo.tm_mday,
               timeinfo.tm_hour,
               timeinfo.tm_min,
               timeinfo.tm_sec));

  rtcTimeValid = true;
  lastNTPSync = millis();
  return true;
}

void connectWiFi()
{
  if (WIFI_SSID.length() == 0)
    return;

  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());

  int retry = 0;

  while (WiFi.status() != WL_CONNECTED && retry < 10)
  {
    delay(500);
    Serial.println("CONNECTING...");    
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
      syncRTCWithNTP();
      Serial.println("WIFI_CONNECTED");
  }
  else
  {
      Serial.println("WIFI_NOT_CONNECTED");
  }
}

void maintainWiFi()
{
  if (millis() - lastWiFiCheck < 30000) return;

  lastWiFiCheck = millis();

  if (WIFI_SSID == "") return;

  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("WIFI_RECONNECTING");
  WiFi.reconnect();
}

// =====================================================
// USERS
// =====================================================
DynamicJsonDocument readUsers()
{
  DynamicJsonDocument doc(8192);

  File f = LittleFS.open(USER_FILE, "r");

  if (!f)
  {
    doc.to<JsonArray>();
    return doc;
  }

  if (deserializeJson(doc, f))
  {
    doc.clear();
    doc.to<JsonArray>();
  }

  f.close();
  return doc;
}

void saveUsers(DynamicJsonDocument& doc)
{
  File f = LittleFS.open(USER_FILE, "w");
  if (!f) return;

  serializeJson(doc, f);
  f.close();
}

bool empExists(String empID)
{
  DynamicJsonDocument doc = readUsers();

  for (JsonObject obj : doc.as<JsonArray>())
    if (obj["empID"].as<String>() == empID)
      return true;

  return false;
}

void saveUser(int tid, String empID, String name)
{
  DynamicJsonDocument doc = readUsers();

  JsonArray arr = doc.as<JsonArray>();

  JsonObject o = arr.createNestedObject();
  o["templateID"] = tid;
  o["empID"] = empID;
  o["name"] = name;

  saveUsers(doc);
}

bool getUserByTemplate(int tid, String& empID, String& name)
{
  DynamicJsonDocument doc = readUsers();

  for (JsonObject obj : doc.as<JsonArray>())
  {
    if (obj["templateID"].as<int>() == tid)
    {
      empID = obj["empID"].as<String>();
      name = obj["name"].as<String>();
      return true;
    }
  }

  return false;
}

void deleteUserJSON(int tid)
{
  DynamicJsonDocument doc = readUsers();

  JsonArray arr = doc.as<JsonArray>();

  for (int i = arr.size() - 1; i >= 0; i--)
    if (arr[i]["templateID"].as<int>() == tid)
      arr.remove(i);  

  saveUsers(doc);
}

int getTotalUsers()
{
  DynamicJsonDocument doc = readUsers();
  return doc.as<JsonArray>().size();
}

void listUsers()
{
  DynamicJsonDocument doc = readUsers();

  for (JsonObject obj : doc.as<JsonArray>())
  {
    Serial.print(obj["templateID"].as<int>());
    Serial.print(" | ");
    Serial.print(obj["empID"].as<String>());
    Serial.print(" | ");
    Serial.println(obj["name"].as<String>());
  }
}

// =====================================================
// LOGS
// =====================================================
void saveLog(String empID, DateTime now, String name)
{
  String line = DEVICE_ID + "," + empID + ",";

  char buf[30];

  sprintf(buf,"%04d-%02d-%02d,%02d:%02d:%02d,0,",
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second());

  line += buf;
  line += name;
  line += "\n";

  // FLASH LOG APPEND
  File f = LittleFS.open(LOG_FILE, FILE_APPEND);

  if (f)
  {
    f.print(line);
    f.close();
    Serial.println("FLASH_LOG_SAVED");
  }
  else
  {
    Serial.println("FLASH_SAVE_FAILED");
  }

  // SD CARD APPEND
  File sd = SD.open(SD_LOG_FILE, FILE_APPEND);

  if (sd)
  {
    sd.print(line);
    sd.close();
    Serial.println("SD_LOG_SAVED");
  }
  else
  {
    Serial.println("SD_SAVE_FAILED");
  }
}


void sendLogs()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  File file = LittleFS.open(LOG_FILE, "r");
  if (!file)
  {
    Serial.println("NO_FLASH_LOG");
    return;
  }

  int sentCount = getSentCount();
  int currentLine = 0;

  while (file.available())
  {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line == "") continue;

    currentLine++;

    if (currentLine <= sentCount)
      continue;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(5000);

    http.begin(client, SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    String payload =
      "{\"device\":\"" + DEVICE_ID +
      "\",\"log\":\"" + line + "\"}";

    Serial.println("SENDING_LOG...");

    int code = http.POST(payload);

    String res = http.getString();

    http.end();

    //Serial.print("HTTP_CODE:");
    //Serial.println(code);

    //Serial.print("SERVER_REPLY:");
    //Serial.println(res);

    //if (code == 200)
    //{
    Serial.println("LOG_SENT");
    saveSentCount(currentLine);
    //}
    //else
    //{
    //Serial.println("SEND_FAILED");
    //break;
    //}

    delay(300);
  }

  file.close();
}
// =====================================================
// FINGERPRINT
// =====================================================
int getFreeTemplateID()
{
  for (int i = 1; i <= 64; i++)
    if (finger.loadModel(i) != FINGERPRINT_OK)
      return i;

  return -1;
}

bool enrollUser(int id)
{
  Serial.println("PLACE_FINGER");

  unsigned long start = millis();

  while (finger.getImage() != FINGERPRINT_OK)
  {
      if (millis() - start > 10000)
      {
          Serial.println("FIRST_SCAN_TIMEOUT");
          return false;
      }
      delay(50);
  }

  Serial.println("FIRST_SCAN_OK");

  if (finger.image2Tz(1) != FINGERPRINT_OK)
    return false;

  Serial.println("REMOVE_FINGER");

  while (finger.getImage() != FINGERPRINT_NOFINGER)
  {
      delay(50);
  }

  delay(500);

  Serial.println("PLACE_SAME_FINGER");

  unsigned long t2 = millis();

  while (finger.getImage() != FINGERPRINT_OK)
  {
      if (millis() - t2 > 10000)
      {
          Serial.println("SECOND_SCAN_TIMEOUT");
          return false;
      }

      delay(50);
  }

  Serial.println("SECOND_SCAN_OK");
  if (finger.image2Tz(1) != FINGERPRINT_OK)
  {
      Serial.println("TZ1_FAIL");
      return false;
  }

  Serial.println("TZ1_OK");
  if (finger.image2Tz(2) != FINGERPRINT_OK)
  {
      Serial.println("TZ2_FAIL");
      return false;
  }

  Serial.println("TZ2_OK");
  if (finger.createModel() != FINGERPRINT_OK)
  {
      Serial.println("MODEL_FAIL");
      return false;
  }

  Serial.println("MODEL_OK");
  if (finger.storeModel(id) != FINGERPRINT_OK)
  {
      Serial.println("STORE_FAIL");
      return false;
  }

  Serial.println("STORE_OK");
  /*if (finger.image2Tz(2) != FINGERPRINT_OK)
    return false;

  if (finger.createModel() != FINGERPRINT_OK)
    return false;

  if (finger.storeModel(id) != FINGERPRINT_OK)
    return false;
  */
  return true;
}

bool deleteFinger(int id)
{
  if (finger.deleteModel(id) == FINGERPRINT_OK)
  {
    deleteUserJSON(id);
    return true;
  }

  return false;
}

void clearAllFingerprints()
{
  finger.emptyDatabase();
  writeText(USER_FILE, "[]");
}

// =====================================================
// ATTENDANCE
// =====================================================
void handleAttendance()
{
  /*
  static unsigned long t = 0;

  if (millis() - t > 2000)
  {
      Serial.println("SCAN_LOOP");
      t = millis();
  }
  */
  uint8_t p = finger.getImage();

  if (p != FINGERPRINT_OK) return;

  if (finger.image2Tz() != FINGERPRINT_OK) return;

  if (finger.fingerSearch() != FINGERPRINT_OK)
  {
    Serial.println("NO_MATCH");
    delay(300);
    return;
  }

  int id = finger.fingerID;

  if (id == lastFingerID &&
      millis() - lastAttendanceTime < 3000)
  {
    return;
  }

  String empID="", name="";

  if (!getUserByTemplate(id, empID, name))
  {
    Serial.println("USER_NOT_FOUND");
    return;
  }

  DateTime now = rtc.now();

  lastFingerID = id;
  lastAttendanceTime = millis();

  saveLog(empID, now, name);

  Serial.print("ATTENDANCE_MARKED|");
  Serial.print(empID);
  Serial.print("|");
  Serial.print(name);
  Serial.print("|");
  Serial.printf("%02d:%02d:%02d\n",
      now.hour(), now.minute(), now.second());

  delay(800);
} 
// =====================================================
// SERIAL COMMANDS
// =====================================================
void handleSerial()
{
  if (!Serial.available())
    return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  lastSerialActivity = millis();

  // OPEN LOGIN
  if (cmd.startsWith("LOGIN|"))
  {
    currentMode = MODE_CONFIG;
    Serial.println("LOGIN_SUCCESS");
    return;
  }

  if (cmd == "LOGOUT")
  {
    currentMode = MODE_ATTENDANCE;
    Serial.println("LOGOUT_OK");
    return;
  }

  if (cmd.startsWith("ENROLL|"))
  {
    int p1 = cmd.indexOf('|');
    int p2 = cmd.indexOf('|', p1 + 1);

    String empID = cmd.substring(p1 + 1, p2);
    String name = cmd.substring(p2 + 1);

    if (empExists(empID))
    {
      Serial.println("EMP_ALREADY_EXISTS");
      return;
    }

    int id = getFreeTemplateID();

    if (id == -1)
    {
      Serial.println("FINGER_MEMORY_FULL");
      return;
    }

    if (enrollUser(id))
    {
      saveUser(id, empID, name);
      Serial.println("USER_SAVED");
    }
    else
    {
      Serial.println("ENROLL_FAILED");
    }

    return;
  }

  if (cmd.startsWith("DELETE|"))
  {
    int id = cmd.substring(7).toInt();

    if (deleteFinger(id))
      Serial.println("OK");
    else
      Serial.println("FAIL");

    return;
  }

  if (cmd == "LIST_ALL")
  {
    listUsers();
    return;
  }

  if (cmd == "SHOW_LOGS")
  {
    File f = LittleFS.open(LOG_FILE, "r");

    while (f.available())
      Serial.write(f.read());

    f.close();
    return;
  }

  if (cmd == "CLEAR_LOGS")
  {
    writeText(LOG_FILE, "");
    Serial.println("ALL_LOGS_CLEARED");
    return;
  }

  if (cmd == "CLEAR|CONFIRM1")
  {
    clearStep1 = true;
    Serial.println("STEP1_OK");
    return;
  }

  if (cmd == "CLEAR|CONFIRM2")
  {
    if (clearStep1)
    {
      clearAllFingerprints();
      clearStep1 = false;
    }
    return;
  }

  if (cmd.startsWith("WIFI_SSID_"))
  {
    WIFI_SSID = cmd.substring(10);
    saveConfig();
    Serial.println("SSID_UPDATED");
    return;
  }

  if (cmd.startsWith("WIFI_PASS_"))
  {
    WIFI_PASS = cmd.substring(10);
    saveConfig();
    Serial.println("PASS_UPDATED");
    return;
  }

  if (cmd == "WIFI_RECONNECT")
  {
    connectWiFi();
   // Serial.println("WIFI_RECONNECTED");
    return;
  }

  if (cmd == "WIFI_SHOW")
  {
    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("PASS: ");
    Serial.println(WIFI_PASS);
    return;
  }

  if (cmd == "WIFI_RESET")
  {
    WIFI_SSID = "";
    WIFI_PASS = "";
    saveConfig();
    Serial.println("WIFI_RESET_DONE");
    return;
  }

  if (cmd.startsWith("ADMINP|"))
  {
    ADMIN_PASS = cmd.substring(7);
    saveConfig();
    Serial.println("ADMIN_PASS_UPDATED");
    return;
  }

  if (cmd.startsWith("ADMINU|"))
  {
    ADMIN_USER = cmd.substring(7);
    saveConfig();
    Serial.println("ADMIN_USER_UPDATED");
    return;
  }

  if (cmd == "GET_TOTAL")
  {
    Serial.print("TOTAL_USERS|");
    Serial.println(getTotalUsers());
    return;
  }

  if (cmd == "GET_STATUS")
  {
    DateTime now = rtc.now();

    Serial.print("DEVICE_ID|");
    Serial.print(DEVICE_ID);

    Serial.print("|TIME|");
    Serial.printf("%02d:%02d:%02d",
                  now.hour(),
                  now.minute(),
                  now.second());

    Serial.print("|WIFI|");

    if (WiFi.status() == WL_CONNECTED)
      Serial.print("CONNECTED");
    else
      Serial.print("DISCONNECTED");

    Serial.print("|TF|");
    Serial.println(getTotalUsers());

    return;
  }

  if (cmd == "WIFI_STATUS")
  {
    if (WiFi.status() == WL_CONNECTED)
      Serial.println("Connected");
    else
      Serial.println("NOT-Connected");

    return;
  }
  if (cmd == "FILES_RESET")
  {
    filesReset();
    return;
  }

  if (cmd == "SENSOR_RESET")
  {
    sensorReset();
    return;
  }

  if (cmd == "FACTORY_RESET")
  {
    factoryReset();
    return;
  }

  if (cmd == "SHOW_SD_LOGS")
  {
    File f = SD.open(SD_LOG_FILE, "r");

    if (!f)
    {
      Serial.println("NO_SD_FILE");
      return;
    }

    while (f.available())
      Serial.write(f.read());

    f.close();
    return; 
  }

  if (cmd == "CLEAR_SD_LOGS")
  {
    SD.remove(SD_LOG_FILE);

    File f = SD.open(SD_LOG_FILE, FILE_WRITE);
    f.close();

    Serial.println("SD_LOGS_CLEARED");
    return;
  }
}

// =====================================================
// TASK
// =====================================================
void logSender(void* p)
{
  while(true)
  {
    if(WiFi.status()==WL_CONNECTED)
        sendLogs();

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}
// =====================================================
void initSDCard()
{
  delay(1000);
  bool sdStatus = false;
  if (!SD.begin(SD_CS))
  {
    Serial.println("SD_CARD_FAILED");
    sdStatus = false;
  }
  else
  {
    Serial.println("SD_CARD_READY");
    sdStatus = true;
    if (!SD.exists(SD_LOG_FILE))
    {
      File f = SD.open(SD_LOG_FILE, FILE_WRITE);
      f.close();
    }
  }
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 25);

    if (sdStatus)
      display.print("SD OK");
    else
      display.print("SD FAIL");

    display.display();
    delay(2000);
  }
}
// =====================================================
// SETUP
// =====================================================
void filesReset()
{
  LittleFS.remove("/users.json");
  LittleFS.remove("/logs.txt");
  LittleFS.remove("/config.json");

  File f;

  f = LittleFS.open("/users.json", "w");
  f.print("[]");
  f.close();

  f = LittleFS.open("/logs.txt", "w");
  f.close();

  f = LittleFS.open("/config.json", "w");
  f.print("{}");
  f.close();

  Serial.println("FILES_RESET_DONE");
}

void sensorReset()
{
  if (finger.emptyDatabase() == FINGERPRINT_OK)
    Serial.println("SENSOR_RESET_DONE");
  else
    Serial.println("SENSOR_RESET_FAILED");
}

void factoryReset()
{
  Serial.println("FACTORY_RESET_START");

  sensorReset();
  filesReset();

  WIFI_SSID = "";
  WIFI_PASS = "";
  ADMIN_USER = "admin";
  ADMIN_PASS = "1234";

  Serial.println("FACTORY_RESET_DONE");
}
void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println("BOOT_START");

  // UART for fingerprint
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);

  // Filesystem
  initStorage();
  Wire.begin(25, 26);
  initSDCard();
  loadConfig();

  // Device ID early
  DEVICE_ID = generateDeviceID();

  // Fingerprint sensor check (non-blocking fail)
  if (finger.verifyPassword())
  {
    Serial.println("FINGER_OK");
  }
  else
  {
    Serial.println("FINGER_ERROR");
  }
  Wire.begin(25, 26);
  // OLED check (do NOT halt device)
  /*
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
  Serial.println("OLED_FAIL");
  } 
  else 
  {
  Serial.println("OLED_OK");
  display.clearDisplay();
  display.display();
  drawtext(display,"HI !");
  draw_Happy_blink(display);   
  
  }
  */  
  // RTC check (do NOT halt device)
  // init RTC 
  if (rtc.begin())
  {
    Serial.println("RTC_OK");

    rtcTimeValid = isRTCTimeValid();

    if (!rtcTimeValid)
      Serial.println("RTC_TIME_INVALID");
  }
  else
  {
    Serial.println("RTC_ERROR");
    rtcTimeValid = false;
  }

  // Start WiFi async (no waiting loop)
  if (WIFI_SSID.length() > 0)
  {
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    Serial.println("WIFI_CONNECTING");
  }
  else
  {
    Serial.println("WIFI_NOT_CONFIGURED");
  }

  // Background task
  xTaskCreatePinnedToCore(
    logSender,
    "LOGTASK",
    8000,
    NULL,
    1,
    &logTask,
    1
  );

  Serial.print("DEVICE_ID|");
  Serial.println(DEVICE_ID);
  Serial.println("DEVICE_READY");
}
// =====================================================
// LOOP
// =====================================================
void loop()
{
  handleSerial();

  if (currentMode == MODE_ATTENDANCE)
    handleAttendance();

  maintainWiFi();

  if (currentMode == MODE_CONFIG &&
      millis() - lastSerialActivity > 60000)
  {
    currentMode = MODE_ATTENDANCE;
  }

  if (WiFi.status() == WL_CONNECTED &&
      millis() - lastNTPSync > 3600000)
  {
    syncRTCWithNTP();
  }
  static wl_status_t lastStatus = WiFi.status();
  static bool firstRun = true;

  wl_status_t now = WiFi.status();
  draw_Happy_HI_blink(display,"HI !");
  if (firstRun)
  {
      lastStatus = now;
      firstRun = false;
  }
  else if (now != lastStatus)
  {
      if (now == WL_CONNECTED)
      {
          Serial.println("WIFI_CONNECTED");
          syncRTCWithNTP();
      }
      else if (now == WL_DISCONNECTED)
      {
          Serial.println("WIFI_DISCONNECTED");
      }

      lastStatus = now;
  }
  
  delay(2);
}