#include "esp_camera.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"   // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>            // read and write from flash memory

// define the number of bytes you want to access
#define EEPROM_SIZE 1

int pictureNumber = 0;

// Email and WiFi setup
SMTPSession smtp;
Session_Config config;
const char* ssid = "realme 10";
const char* password = "s4sviuz4";

// PIR sensor and LED flash pins
#define LED_FLASH_PIN 4

// Camera model configuration
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Email settings
#define SMTP_SERVER "smtp.gmail.com"
#define SMTP_PORT 587 // SSL port
#define SMTP_USER "Your Gmail"
#define SMTP_PASSWORD "gzxertwaoxqvhnbi" // Your mail app password
#define RECIPIENT "Recipient Gmail"
#define SUBJECT "Motion Detected"
#define BODY "Motion has been detected. Photo attached."

camera_fb_t * fb = NULL;
String path;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  Serial.begin(115200);
  pinMode(13,INPUT_PULLDOWN);
  
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  config.frame_size = FRAMESIZE_UXGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;
  
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Get a pointer to the camera sensor settings
  sensor_t *s = esp_camera_sensor_get();
  // Adjust various camera settings
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 1);       // -2 to 2
  s->set_saturation(s, -1);     // -2 to 2
  s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  delay(1000);
    
  fb = NULL;
  
  for(int i=0;i<5;i++){
    // Take Picture with Camera
    fb = esp_camera_fb_get();  
    if(!fb) {
      Serial.println("Camera capture failed");
      return;
    }
    if(i<4){
      esp_camera_fb_return(fb); 
      fb=NULL;
    }
    delay(200);
  }
  // initialize EEPROM with predefined size ( erase before upload clear sketch)
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0) + 1;
  path = "/picture" + String(pictureNumber) +".jpg";
  
  // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  rtc_gpio_hold_en(GPIO_NUM_4);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");

  int response=SendMail(SMTP_USER,SMTP_PASSWORD,RECIPIENT,SMTP_SERVER,SUBJECT,BODY,path);
  if(response==200){
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();
  }

  esp_camera_fb_return(fb); 
  WiFi.disconnect(true,true);
  delay(1000);
  Serial.println("Going to sleep now");
  delay(2000);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, HIGH);
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void loop() {

}

int SendMail(String Sender, String Pass, String Receiver, String Hostname, String Subject, String Message, String path) {
  
  size_t imageSize = fb->len;  // Get the size of the image
  uint8_t *imageData = fb->buf;  // Get the image data
  // Set up the SMTP configuration
  config.server.host_name = Hostname.c_str();
  config.server.port = SMTP_PORT;
  config.login.email = Sender.c_str();
  config.login.password = Pass.c_str();
  config.time.ntp_server = "time.nist.gov";
  config.time.gmt_offset = 3;  // Adjust for your time zone
  config.time.day_light_offset = 0;

  // Compose the email
  SMTP_Message message;
  message.sender.name = "Motion Sensor";
  message.sender.email = Sender.c_str();
  message.subject = Subject.c_str();
  message.text.content = Message.c_str();
  message.addRecipient("Recipient Name", Receiver.c_str());

  // Prepare the image attachment
  SMTP_Attachment att;
  att.descr.filename = path.c_str();  
  att.descr.mime = "image/jpg";      
  att.blob.data = imageData;               // Image data
  att.blob.size = imageSize;           // Size of the image file
  att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;

  // Add the attachment to the message
  message.addAttachment(att);

  // Connect to the server
  smtp.connect(&config);

  // Send the email
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Error sending Email: " + smtp.errorReason());
    return 400;
  } else {
    Serial.println("Mail sent successfully!");
    return 200;
  }

}
