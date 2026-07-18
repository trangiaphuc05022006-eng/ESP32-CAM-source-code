#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <esp_camera.h>

// ================= CẤU HÌNH WIFI & SERVER =================
const char* ssid = "Phuc Ngo";
const char* password = "aimabiet126";
const char* server_url = "https://server-esp32-cam.onrender.com/api/recognize";

unsigned long last_capture_time = 0;
const unsigned long cooldown_time = 10000; // 10 giây

// ================= ĐỊNH NGHĨA CHÂN =================
#define PIN_RELAY_BASE 12   
#define PIN_LDR        13   
#define PIN_LED        2    
#define PIN_SDA        14   
#define PIN_SCL        15   

// Cấu hình chuẩn LCD 2004 (20 cột, 4 hàng)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ================= CẤU HÌNH CAMERA =================
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  
  // Tối ưu tần số xung nhịp xuống 16MHz để không đè tần số của I2C
  config.xclk_freq_hz = 16000000;  
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1; 
  
  // Ép buộc Camera lấy bộ đệm từ bộ nhớ trong (SRAM) thay vì PSRAM để tránh lỗi DMA
  config.fb_location = CAMERA_FB_IN_DRAM; 

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Init Camera failed: 0x%x\n", err);
    lcd.setCursor(0, 2);
    lcd.print("Camera: Error       ");
  } else {
    Serial.println("Camera Init OK");
    // Cấu hình thêm các thông số phụ để cảm biến ảnh chạy ổn định hơn
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
      s->set_framesize(s, FRAMESIZE_VGA);
    }
    lcd.setCursor(0, 2);
    lcd.print("Camera: OK          ");
  }
}

void showDefaultMessage() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("====================");
  lcd.setCursor(0, 1); lcd.print("    SYSTEM READY    ");
  lcd.setCursor(0, 2); lcd.print("  Waiting for LDR   ");
  lcd.setCursor(0, 3); lcd.print("====================");
}

void testServerConnection() {
  lcd.setCursor(0, 3);
  lcd.print("Ping Server...      ");
  
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(10000);
  
  HTTPClient http;
  http.setTimeout(10000);
  
  String ping_url = "https://server-esp32-cam.onrender.com/api/ping";
  http.begin(client, ping_url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    Serial.println("Server OK");
    lcd.setCursor(0, 3);
    lcd.print("Server: Connected   ");
  } else {
    Serial.println("Server Fail");
    lcd.setCursor(0, 3);
    lcd.print("Server: Disconnected");
  }
  http.end();
  delay(1500);
}

void sendImageToServer() {
  digitalWrite(PIN_LED, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Processing...       ");
  lcd.setCursor(0, 1); lcd.print("Capturing Image     ");

  // Giải phóng hàng đợi cũ nếu có
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) { esp_camera_fb_return(fb); }
  
  // Chụp ảnh thực tế
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera DMA Failed");
    lcd.setCursor(0, 2); lcd.print("Capture Failed!     ");
    digitalWrite(PIN_LED, LOW);
    delay(2000);
    showDefaultMessage();
    return;
  }

  lcd.setCursor(0, 1); lcd.print("Uploading data...   ");

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(15000);

  HTTPClient http;
  http.setTimeout(15000);
  http.begin(client, server_url);

  String boundary = "----ESP32CAMBoundary";
  String head = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";
  size_t totalLen = head.length() + fb->len + tail.length();

  // Sử dụng bộ nhớ heap cơ bản
  uint8_t *buf = (uint8_t *)malloc(totalLen);
  if (!buf) {
    Serial.println("Malloc Failed");
    lcd.setCursor(0, 2); lcd.print("Memory Full!        ");
    esp_camera_fb_return(fb);
    digitalWrite(PIN_LED, LOW);
    delay(2000);
    showDefaultMessage();
    return;
  }

  memcpy(buf, head.c_str(), head.length());
  memcpy(buf + head.length(), fb->buf, fb->len);
  memcpy(buf + head.length() + fb->len, tail.c_str(), tail.length());

  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  int httpResponseCode = http.POST(buf, totalLen);

  if (httpResponseCode > 0) {
    String response = http.getString();
    StaticJsonDocument<200> doc;
    if (!deserializeJson(doc, response)) {
      bool is_match = doc["match"];
      lcd.clear();
      if (is_match) {
        lcd.setCursor(0, 1); lcd.print("  ACCESS GRANTED!   ");
        lcd.setCursor(0, 2); lcd.print("   OPENING DOOR     ");
        digitalWrite(PIN_RELAY_BASE, HIGH);
        delay(3000); 
        digitalWrite(PIN_RELAY_BASE, LOW);
      } else {
        lcd.setCursor(0, 1); lcd.print("  UNKNOWN PERSON    ");
        lcd.setCursor(0, 2); lcd.print("   ACCESS DENIED    ");
        delay(3000);
      }
    }
  } else {
    lcd.setCursor(0, 2); lcd.print("Server Timeout!     ");
    delay(2000);
  }

  http.end();
  free(buf);
  esp_camera_fb_return(fb);
  
  digitalWrite(PIN_LED, LOW);
  showDefaultMessage();
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_RELAY_BASE, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_LDR, INPUT);
  digitalWrite(PIN_RELAY_BASE, LOW);
  digitalWrite(PIN_LED, LOW);

  // BƯỚC 1: Khởi tạo I2C với tốc độ chuẩn 100KHz để không bị nhiễu xung với Camera
  Wire.begin(PIN_SDA, PIN_SCL, 100000);
  
  // BƯỚC 2: Khởi tạo LCD ngay lập tức để màn hình chớp sáng báo hiệu
  lcd.init(); 
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("--- SYSTEM BOOT --- ");

  // BƯỚC 3: Kết nối WiFi và in tiến trình trực tiếp lên màn hình
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lcd.setCursor(0, 1); lcd.print("WiFi: Connecting    ");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  lcd.setCursor(0, 1); lcd.print("WiFi: Connected     ");

  // BƯỚC 4: Khởi tạo Camera (Đã được chuyển cấu hình bộ nhớ để không đè I2C)
  initCamera();

  // BƯỚC 5: Kiểm tra kết nối Server
  testServerConnection();

  // Hoàn tất, chuyển về màn hình chính
  showDefaultMessage();
}

void loop() {
  int ldr_state = digitalRead(PIN_LDR);
  unsigned long current_time = millis();

  if (ldr_state == LOW) {
    if (current_time - last_capture_time >= cooldown_time) {
      last_capture_time = current_time;
      sendImageToServer();
    }
  }
  delay(100);
}