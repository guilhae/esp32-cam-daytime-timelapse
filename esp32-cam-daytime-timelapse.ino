  /********************************************************************************************************************
 *  Board Settings:
 *  Board: "AI Thinker ESP32-CAM"
 *  Upload Speed: "921600"
 *  Flash Frequency: "80MHz"
 *  Flash Mode: "QIO"
 *  Partition Scheme: "Hue APP (3MB No OTA/1MB SPIFFS)"
 *  Core Debug Level: "None"
 *  COM Port: Depends *On Your System*
 *********************************************************************************************************************/

#include "esp_camera.h"
#include "FS.h"
#include "SPI.h"
#include "SD_MMC.h"
#include "EEPROM.h"
#include "driver/rtc_io.h"

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

#define ID_ADDRESS            0x00
#define COUNT_ADDRESS         0x01
#define ID_BYTE               0xAA
#define EEPROM_SIZE           0x0F

#define TIME_TO_SLEEP  600          //time ESP32 will go to sleep (in seconds)
#define uS_TO_S_FACTOR 1000000ULL   //conversion factor for micro seconds to seconds */

int min_light=20;
RTC_DATA_ATTR uint32_t cur_pic = 0;
RTC_DATA_ATTR uint32_t cur_fol;

void sleep()
{
  pinMode(4, OUTPUT);              //GPIO for LED flash
  digitalWrite(4, LOW);            //turn OFF flash LED
  rtc_gpio_hold_en(GPIO_NUM_4);    //make sure flash is held LOW in sleep
  
  Serial.println("Entering deep sleep mode");
  Serial.flush();
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void setup() 
{
  setCpuFrequencyMhz(240);
  
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting...");

  //Se for o primeiro ciclo (imagem 0)
  if (cur_pic==0){
    
    //initialize EEPROM & get file number
    if (!EEPROM.begin(EEPROM_SIZE)){
      Serial.println("Failed to initialise EEPROM"); 
      Serial.println("Exiting now"); 
      while(1);   //wait here as something is not right
    }

    //there will not be a valid picture number
    if(EEPROM.read(ID_ADDRESS) != ID_BYTE){
      Serial.println("Initializing ID byte & restarting picture count");
      cur_fol = 0;
      EEPROM.write(ID_ADDRESS, ID_BYTE);
    }else{ //obtain next picture number
      EEPROM.get(COUNT_ADDRESS, cur_fol);
      cur_fol++;
      EEPROM.write(COUNT_ADDRESS, cur_fol); 
      Serial.print("Folder:");
      Serial.println(cur_fol);
    }
    EEPROM.commit();
  }

  pinMode(4, INPUT);              //GPIO for LED flash
  digitalWrite(4, LOW);
  rtc_gpio_hold_dis(GPIO_NUM_4);  //diable pin hold if it was enabled before sleeping
  
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
  
  //init with high specs to pre-allocate larger buffers
  if(psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 8;
    config.fb_count = 2;
  } else 
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  //initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    Serial.println("Reseting in 10 seconds..."); 
    delay(10000);
    ESP.restart();
  }
  
  
  //sensor on
  camera_fb_t * fb = NULL;
  sensor_t * s = esp_camera_sensor_get();
  
  s->set_denoise(s,1);
  s->set_sharpness(s,0);
  //no sharpening
  s->set_reg(s,0x92,0xff,0x1);
  s->set_reg(s,0x93,0xff,0x0);  

  int light=0;
  s->set_reg(s,0xff,0xff,0x01);//banksel    
  light=s->get_reg(s,0x2f,0xff);

  if (light<min_light){
    sleep();
  }
  
  /*s->set_reg(s,0x2d,0xff,0x0);//extra lines
  s->set_reg(s,0x2e,0xff,0x0);//extra lines
  s->set_reg(s,0x47,0xff,0x0);//Frame Length Adjustment MSBs
  s->set_reg(s,0x2a,0xf0,0xf0);//line adjust MSB
  s->set_reg(s,0x2b,0xff,0xff);//line adjust*/
    
  delay(3000);
  //obtain camera frame buffer
  fb = esp_camera_fb_get();
  
  if (!fb){
    Serial.println("Camera capture failed");
    Serial.println("Reseting in 10 seconds..."); 
    delay(10000);
    ESP.restart();
  }

  //initialize & mount SD card
  if(!SD_MMC.begin())
  {
    Serial.println("Card Mount Failed");
    Serial.println("Reseting in 10 seconds..."); 
    delay(10000);
    ESP.restart();
  }
  
  uint8_t cardType = SD_MMC.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    Serial.println("Reseting in 10 seconds..."); 
    delay(10000);
    ESP.restart();
  }
  
  //generate file path
  String path = "/" + String(cur_fol) + "_N" + String(cur_pic) + "_L" + String(light) + ".jpg";
    
  fs::FS &fs = SD_MMC;

  //create new file
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Failed to create file");
    Serial.println("Reseting in 10 seconds..."); 
    delay(10000);
    ESP.restart();
  }else{
    //save to SD card
    file.write(fb->buf, fb->len); 
  }
  file.close();

  //return camera frame buffer
  esp_camera_fb_return(fb);
  Serial.printf("Image saved: %s\n", path.c_str());

  cur_pic++;

  sleep();
}

void loop() 
{


}
