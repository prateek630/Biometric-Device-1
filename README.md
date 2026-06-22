Smart Biometric Attendance System using ESP32

A fully featured industrial-grade biometric attendance system built using ESP32, AS608 Fingerprint Sensor, DS3231 RTC, SD Card Module, OLED Display, Wi-Fi Connectivity, and Cloud Logging.

Designed for schools, offices, factories, and organizations requiring reliable attendance tracking with local backup, cloud synchronization, self-diagnostics, and autonomous operation.

🚀 Features
🔐 Biometric Attendance
Fingerprint-based authentication using AS608 sensor
Fast fingerprint matching and attendance marking
Duplicate attendance prevention
User enrollment and deletion
Supports up to 64 fingerprint templates
📅 Accurate Time Management
DS3231 RTC integration
Automatic NTP synchronization when Wi-Fi is available
Continues operation even without internet
☁️ Cloud Synchronization
Automatic log upload to Google Apps Script backend
Background synchronization task
Retry mechanism for unsent logs
Secure HTTPS communication
💾 Dual Data Backup

Attendance records are stored in:

ESP32 LittleFS Flash Storage
SD Card Storage

This ensures data remains safe even during:

Power failures
Network outages
Cloud server downtime
📶 Smart Wi-Fi Management
Automatic Wi-Fi reconnection
Runtime Wi-Fi configuration
Connection status monitoring
Remote network updates through serial commands
🖥 OLED Display Support
Real-time system status display
SD card status indication
User interaction animations
Visual diagnostics
⚙️ Administrative Controls
User management
Attendance log access
Wi-Fi configuration
Factory reset functionality
Sensor database reset
File system reset
🛠 Self-Diagnostic System

The device automatically checks:

Fingerprint sensor
RTC module
SD card
Wi-Fi connectivity
Cloud synchronization

and reports failures through serial output.

🏗 Hardware Used
Component	Description
ESP32	Main controller
AS608 Fingerprint Sensor	Biometric authentication
DS3231 RTC	Real-time clock
SD Card Module	Attendance backup storage
SSD1306 OLED Display	Status display
Wi-Fi	Cloud synchronization
📂 Project Architecture
ESP32
│
├── Fingerprint Authentication
├── Attendance Logger
│    ├── LittleFS Storage
│    └── SD Card Storage
│
├── RTC Time Management
│
├── WiFi Manager
│
├── Google Sheets Cloud Sync
│
├── OLED User Interface
│
└── Serial Command Interface
📁 Data Storage
Users Database
[
  {
    "templateID": 1,
    "empID": "EMP001",
    "name": "John Doe"
  }
]
Attendance Log Format
DEVICE_ID,EMP_ID,YYYY-MM-DD,HH:MM:SS,0,EMPLOYEE_NAME

Example:

ESP32_1234ABCD,EMP001,2026-06-22,09:15:21,0,John Doe
🔌 Pin Configuration
Fingerprint Sensor
Sensor	ESP32
TX	GPIO16
RX	GPIO17
RTC
DS3231	ESP32
SDA	GPIO25
SCL	GPIO26
SD Card
SD Module	ESP32
CS	GPIO13
📟 Serial Commands
Login
LOGIN|username|password
Logout
LOGOUT
Enroll User
ENROLL|EMP001|John Doe
Delete User
DELETE|1
List Users
LIST_ALL
View Attendance Logs
SHOW_LOGS
Clear Logs
CLEAR_LOGS
Get Device Status
GET_STATUS
Get Total Users
GET_TOTAL
Configure Wi-Fi
WIFI_SSID_MyWiFi
WIFI_PASS_MyPassword
Reconnect Wi-Fi
WIFI_RECONNECT
Show Wi-Fi Settings
WIFI_SHOW
Reset Wi-Fi
WIFI_RESET
Factory Reset
FACTORY_RESET
Sensor Reset
SENSOR_RESET
File System Reset
FILES_RESET
View SD Card Logs
SHOW_SD_LOGS
Clear SD Logs
CLEAR_SD_LOGS
☁️ Cloud Integration

The system uploads attendance logs to a Google Apps Script endpoint:

https://script.google.com/macros/...

Benefits:

Centralized attendance records
Remote monitoring
Real-time synchronization
Automatic retry of failed uploads
🔄 Workflow
Fingerprint Scan
        │
        ▼
Fingerprint Match
        │
        ▼
User Identification
        │
        ▼
RTC Timestamp
        │
        ▼
Store Log
 ├─ LittleFS
 └─ SD Card
        │
        ▼
Background Cloud Sync
🛡 Reliability Features

✔ Offline attendance support

✔ Automatic cloud synchronization

✔ Local flash backup

✔ SD card backup

✔ RTC fallback timing

✔ Wi-Fi auto reconnect

✔ Duplicate attendance prevention

✔ Sensor diagnostics

✔ Factory reset support

✔ Industrial-grade continuous operation

📈 Future Improvements
Web Dashboard
OTA Firmware Updates
MQTT Support
Face Recognition Integration
Mobile App
Employee Shift Management
Email/SMS Notifications
Multi-device Centralized Management
📷 Project Showcase

This project was showcased as an industrial-grade biometric attendance solution demonstrating:

Embedded Systems Engineering
IoT Integration
Cloud Connectivity
Data Management
Real-Time Systems
Hardware-Software Co-design
👨‍💻 Author

Prateek Kumar
BS in Data Science and Applications, IIT Madras

Skills Demonstrated
Embedded Systems
ESP32 Development
IoT Systems
Biometric Authentication
Cloud Integration
Data Logging
FreeRTOS
Hardware Design
Industrial Automation

⭐ If you found this project useful, consider giving the repository a star.
