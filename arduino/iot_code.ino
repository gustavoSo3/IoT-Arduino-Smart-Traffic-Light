#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <base64.h>
#include <PubSubClient.h>

// Pin definition for camera
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 21
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 19
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 5
#define Y2_GPIO_NUM 4
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Replace with your network credentials
const char* ssid = "no_wifi_connection";
const char* password = "Ex6nuZ-uW6Fu";

const char* mqtt_server = "ec2-18-234-62-133.compute-1.amazonaws.com";
const char* mqtt_user = "iot";
const char* mqtt_password = "1234";
const char* mqtt_topic = "traffic_light_control";

// aws_lambda_endpoint
const char* awsEndpoint = "https://icztd2u7gggo5hjbpcy4vbv2p40tblhb.lambda-url.us-east-1.on.aws/";

const int car_led_pin = 33;
const int pedestrian_led_pin = 32;

const int car_time = 90;
const int pedestrian_time = 30;

unsigned long last_transition = 0;
unsigned long last_picture = 0;
unsigned long current_time = 0;

enum TrafficState {UNKNOWN, CARS, PEDESTRIANS};
TrafficState current_state = PEDESTRIANS;

WiFiClient espClient;
PubSubClient client(espClient);

// Initialize camera
void initCamera() {
  // Define camera configuration
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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic ");
  Serial.print(topic);
  Serial.print(": msg_len(");
  Serial.print(length);
  Serial.print(") ##");
  
  char test[length + 1];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    test[i] = (char)payload[i];
  }
  test[length] = '\0';
  
  Serial.println("##");
  
  
  if (strcmp((char*)test, "pedestrian") == 0) {
    
    Serial.println("Deberia encender el pedestrians");
    current_state = PEDESTRIANS;
    digitalWrite(car_led_pin, LOW);
    digitalWrite(pedestrian_led_pin, HIGH);
    last_transition = current_time;
  } else if (strcmp((char*)test, "cars") == 0) {
    Serial.println("Deberia encender el cars");
    current_state = CARS;
    digitalWrite(car_led_pin, HIGH);
    digitalWrite(pedestrian_led_pin, LOW);
    last_transition = current_time;
  }
}

void connectMQTT(){

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  while (!client.connected()) {
    Serial.println("Connecting to MQTT server...");
    
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Connected to MQTT server");
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("Failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}


void setup() {
  pinMode(car_led_pin, OUTPUT);
  pinMode(pedestrian_led_pin, OUTPUT);
  Serial.begin(115200);

  // Connect to Wi-Fi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  initCamera();
  connectMQTT();
  digitalWrite(car_led_pin, HIGH);
  digitalWrite(pedestrian_led_pin, HIGH);
}

void takeAndSendPicture(){

  // Take a picture
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // Encode the picture to base64
  String base64Image = base64::encode(fb->buf, fb->len);

  // Send the picture to AWS Lambda function
  HTTPClient http;
  http.begin(String(awsEndpoint));
  http.addHeader("Content-Type", "application/json");

  String requestBody = "{\"image\": \"" + base64Image + "\"}";
  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(response);
  } else {
    Serial.printf("HTTP error: %d", httpResponseCode);
  }

  http.end();

  // Free the memory used by the picture
  esp_camera_fb_return(fb);
}

void loop() {

  client.loop();
  
  current_time = millis() / 1000;
  
  if (current_state == CARS) {
    if (current_time - last_transition >= car_time) {
      current_state = PEDESTRIANS;
      digitalWrite(car_led_pin, LOW);
      digitalWrite(pedestrian_led_pin, HIGH);
      last_transition = current_time;
    }
  } else if (current_state == PEDESTRIANS) {
    if (current_time - last_transition >= pedestrian_time) {
      current_state = CARS;
      digitalWrite(car_led_pin, HIGH);
      digitalWrite(pedestrian_led_pin, LOW);
      last_transition = current_time;
    }
  }

  // Wait 5 seconds before taking another picture
  if(current_time - last_picture >= 5){
    takeAndSendPicture();
    last_picture = current_time;
  }
}

