
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <FS.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WebServer server(80);

/* this variable hold queue handle */
TaskHandle_t xTask1;
TaskHandle_t xTask2;

const char* htmlfile = "/index.html";
const char* host = "esp32fs";

String status = "WAITING FOR INPUT...";

int gobalR;
int gobalG;
int gobalB;

//------------ VALUES FOR LED PWM -------------//
#define GPIO13 13
#define GPIO12 12
#define GPIO14 14

int LEDfreq = 1400;
int resolution = 8;   // 8 bit res. (0-255)

byte ledChannel1 = 1;
byte ledChannel2 = 2;
byte ledChannel3 = 3;

//holds the current upload
File fsUploadFile;

bool loadFromSpiffs(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) path += "index.htm";

  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".html")) dataType = "text/html";
  else if (path.endsWith(".htm")) dataType = "text/html";
  else if (path.endsWith(".css")) dataType = "text/css";
  else if (path.endsWith(".js")) dataType = "application/javascript";
  else if (path.endsWith(".png")) dataType = "image/png";
  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  else if (path.endsWith(".xml")) dataType = "text/xml";
  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  else if (path.endsWith(".zip")) dataType = "application/zip";
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (server.hasArg("download")) dataType = "application/octet-stream";
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
  }

  dataFile.close();
  return true;
}

void handleButton() {

  String state = server.arg("state");
  int btn = state.toInt();
  Serial.print("Button: ");
  Serial.println(btn);

  if (btn == 1) {
    status = "-- IN USE, PLEASE WAIT... --";
  }
  else {
    status = "-- READY TO FIRE --";
  }
  Serial.println(status);
  server.send(200, "text/plane", "");
}

void handlePWM() {
  String Red = server.arg("R");
  int R = Red.toInt();

  String Green = server.arg("G");
  int G = Green.toInt();

  String Blue = server.arg("B");
  int B = Blue.toInt();

  if (R != 0) {
    gobalR = R;
  }
  if (G != 0) {
    gobalG = G;
  }
  if (B != 0) {
    gobalB = B;
  }
  server.send(200, "text/plane", "");
}

void handleRoot() {
  server.sendHeader("Location", "/index.html", true);  //Redirect to our html web page
  server.send(302, "text/plane", "");
}

void handleImg() {
  server.sendHeader("Location", "/wheel.png", true);
  server.send(302, "text/plane", "");
}

void handleWebRequests() {
  if (loadFromSpiffs(server.uri())) return;
  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.println(message);
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

void getData() {
  String text2 = "{\"data\":[";
  text2 += "{\"dataValue\":\"";
  text2 += status;
  text2 += "\"}";
  text2 += "]}";
  server.send(200, "text/html", text2);
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void setup() {

  Serial.begin(112500);
  pinMode(15, INPUT); //used to check smokemachine

  xTaskCreatePinnedToCore(
    GPIOtask,           /* Task function. */
    "sendTask",        /* name of task. */
    10000,                    /* Stack size of task */
    NULL,                     /* parameter of the task */
    1,                        /* priority of the task */
    &xTask1,                /* Task handle to keep track of created task */
    0);                    /* pin task to core 0 */
  xTaskCreatePinnedToCore(
    APtask,           /* Task function. */
    "receiveTask",        /* name of task. */
    10000,                    /* Stack size of task */
    NULL,                     /* parameter of the task */
    1,                        /* priority of the task */
    &xTask2,            /* Task handle to keep track of created task */
    1);
}

void loop() {} //not really useful but has to be there!

void GPIOtask( void * parameter )
{

  ledcSetup(ledChannel1, LEDfreq, resolution); //RED
  ledcAttachPin(GPIO13, ledChannel1);

  ledcSetup(ledChannel2, LEDfreq, resolution); //GREEN
  ledcAttachPin(GPIO12, ledChannel2);

  ledcSetup(ledChannel3, LEDfreq, resolution); //BLUE
  ledcAttachPin(GPIO14, ledChannel3);

  bool flag = HIGH;

  for (;;) {
    //Serial.print("R: "); Serial.println(gobalR);
    //Serial.print("G: "); Serial.println(gobalG);
    //Serial.print("B: "); Serial.println(gobalB);

    if (!digitalRead(15) && flag == HIGH) {
      status = "-- HEATING SMOKE MACHINE, PLEASE WAIT... --";
      flag = LOW;
      Serial.println(status);
    }
    else if (digitalRead(15) && flag == LOW) {
      status = "-- READY TO FIRE --";
      flag = HIGH;
      Serial.println(status);
    }

    ledcWrite(ledChannel1, gobalR);
    ledcWrite(ledChannel2, gobalG);
    ledcWrite(ledChannel3, gobalB);

    micros(); //update overflow
    loop();
    vTaskDelay(10);
  }
}

void APtask( void * parameter )
{
  //Initialize Webserver
  server.on("/", handleRoot);
  server.on("/img", handleImg);
  server.on("/setPWM/setLED", handlePWM); //Reads ADC function is called from out index.html /setPWM/?
  server.on("/buttonSet", handleButton);
  server.on("/data", getData);

  server.onNotFound(handleWebRequests); //Set setver all paths are not found so we can handle as per URI

  Serial.setDebugOutput(true);
  SPIFFS.begin();
  {
    listDir(SPIFFS, "/", 0);
    Serial.printf("\n");
  }
  Serial.println("SPIFFS located...");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("VORTEX CANNON AP");
  Serial.println("wireless access point created...");

  dnsServer.start(DNS_PORT, "*", apIP);
  MDNS.begin(host);
  Serial.println("captive portal activated...");

  server.begin();
  Serial.println("HTTP server started...");
  Serial.print("IP address: ");
  Serial.println(apIP);

  for (;;) {
    dnsServer.processNextRequest();
    server.handleClient();
  }
}
