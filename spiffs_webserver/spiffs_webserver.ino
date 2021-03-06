
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <FS.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>
#include <esp_task_wdt.h>
#include <esp_int_wdt.h>

/*
  ----------------  PIN SETUP ----------------
    OUTPUTS:
    Smoke machine:  GPI22
    Solenoid:       GPI23
    LED R:          GPI27
    LED G:          GPI26
    LED B:          GPI25
    LED Btn:        GPI32

    INPUTS:
    Btn:            GPI12
    Smoke sensor:   GPI33
  -------------------------------------------
*/

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WebServer server(80);

/* this variable hold queue handle */
TaskHandle_t xTask1;
TaskHandle_t xTask2;

const char* htmlfile = "/index.html";
const char* host = "esp32fs";

String status = "-- WAITING FOR INPUT... --";

byte btn;
byte toggl = 0;

bool pushToggl = 0;
bool fireToggl = 0;
bool toggleVar = 1;
bool LEDtoggl = 0;

int fireCounter = 0;

int globalR;
int globalG;
int globalB;

//------------ VALUES FOR LED PWM -------------//
#define GPIO25 25
#define GPIO26 26
#define GPIO27 27

#define GPIO14 14 //button

int LEDfreq = 1400;
int resolution = 8;   // 8 bit res. (0-255)

byte ledChannel1 = 1;
byte ledChannel2 = 2;
byte ledChannel3 = 3;
byte ledChannel4 = 4;

unsigned int rgbColour[3];

// Start off with red.
//rgbColour[0] = 255;
//rgbColour[1] = 0;
//rgbColour[2] = 0;

int wheelCounter = 0;
int WheelPos;

int colorCounter = 0;
int pushCounter = 0;


float brightness = 0;    // how bright the LED is
int fadeAmount = 1;    // how many points to fade the LED by
int radyCounter = 0;
int colorWheelCounter = 0;

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
  btn = state.toInt();
  Serial.print("Button: "); Serial.println(btn);

  server.send(200, "text/plane", "");
}
void handleRainbow() {
  String state = server.arg("state");
  toggl = state.toInt();
  Serial.print("toggl: "); Serial.println(btn);

  if (toggl && toggleVar) {
    LEDtoggl = 1;
    toggleVar = 0;
  }
  else if (toggl && !toggleVar)  {
    LEDtoggl = 0;
    toggleVar = 1;
  }

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
    globalR = R;
  }
  if (G != 0) {
    globalG = G;
  }
  if (B != 0) {
    globalB = B;
  }
  server.send(200, "text/plane", "");
}

void handleRoot() {
  server.sendHeader("Location", "/index.html", true);  //Redirect to our html web page
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

uint32_t Wheel() {

  colorCounter++;

  if (colorCounter > 0 && colorCounter < 255) {
    globalR++;
    globalB--;
  }
  if (colorCounter > 255 && colorCounter < 255 * 2) {
    globalR--;
    globalG++;
  }
  if (colorCounter > 255 * 2 && colorCounter < 255 * 3) {
    globalG--;
    globalB++;
  }
  if (colorCounter == 255 * 3) {
    colorCounter = 0;
  }

  if (globalR < 0) {
    globalR = 0;
  }
  if (globalG < 0) {
    globalG = 0;
  }
  if (globalB < 0) {
    globalB = 0;
  }

}

void fireCannon() {

  Serial.print("fireCounter: ");Serial.println(fireCounter);;

  if (fireCounter == 1) {
    digitalWrite(17, LOW);
    Serial.print("1");
  }
  else if ( fireCounter == 400) {
    digitalWrite(17, HIGH);
    Serial.print("2");
  }
  else if ( fireCounter == 1000) {
    digitalWrite(16, LOW);
    Serial.print("3");
  }
  else if ( fireCounter == 1100 ) {
    digitalWrite(16, HIGH);
    Serial.print("4");
    status == "-- READY TO FIRE --";
    fireCounter = 0;
    btn = 0;
    fireToggl = LOW;
  }
  radyCounter = 0;
  fireCounter++;
}


void setup() {

  Serial.begin(112500);

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
  pinMode(4, INPUT); //used to check smokemachine
  pinMode(33, INPUT_PULLUP); //Button
  pinMode(17, OUTPUT); //Soleniod
  pinMode(16, OUTPUT); //Smoke

  digitalWrite(16, HIGH);
  digitalWrite(17, HIGH);

  ledcSetup(ledChannel1, LEDfreq, resolution); //RED
  ledcAttachPin(GPIO27, ledChannel1);

  ledcSetup(ledChannel2, LEDfreq, resolution); //GREEN
  ledcAttachPin(GPIO26, ledChannel2);

  ledcSetup(ledChannel3, LEDfreq, resolution); //BLUE
  ledcAttachPin(GPIO25, ledChannel3);

  ledcSetup(ledChannel4, LEDfreq, resolution); //Button
  ledcAttachPin(GPIO14, ledChannel4);

  for (;;) {

    Serial.print("kanp: "); Serial.println(digitalRead(33));

    if (!digitalRead(33) or btn) {
      fireToggl = HIGH;
      status = "-- IN USE, PLEASE WAIT... --";
      Serial.println(status);

    }

    if (fireToggl) {
      fireCannon();
    }

    if (!fireToggl) {

      if (!digitalRead(4)) {
        status = "-- HEATING SMOKE MACHINE, PLEASE WAIT... --";
        ledcWrite(ledChannel4, 0); //button
        Serial.println(status);
        radyCounter = 0;
      }
      if (digitalRead(4)) {
        if (radyCounter > 500) {
          status = "-- READY TO FIRE --";
          Serial.println(status);
          radyCounter = 0;
        }

        Serial.print("radyCounter: "); Serial.println(radyCounter);
        radyCounter++;
      }


      if (status == "-- READY TO FIRE --") {
        Serial.println(status);
        brightness = brightness + fadeAmount;

        if (brightness <= 0 || brightness >= 255) {
          fadeAmount = -fadeAmount;
        }
        ledcWrite(ledChannel4, brightness); //button
      }
      else if (status == "-- HEATING SMOKE MACHINE, PLEASE WAIT... --" or status == "-- IN USE, PLEASE WAIT... --" ) {
        ledcWrite(ledChannel4, 0); //button
      }

    }

    if (LEDtoggl) {

      if (wheelCounter == 1) { //auto control (rainbow)

        Wheel();

        ledcWrite(ledChannel1, globalR);
        ledcWrite(ledChannel2, globalG);
        ledcWrite(ledChannel3, globalB);

        // Serial.print("globalR: "); Serial.println(globalR);
        // Serial.print("globalG: "); Serial.println(globalG);
        // Serial.print("globalB: "); Serial.println(globalB);

        wheelCounter = 0;
      }
      wheelCounter++;
    }

    else if (!LEDtoggl) { //manual control (dials)

      ledcWrite(ledChannel1, globalR);
      ledcWrite(ledChannel2, globalG);
      ledcWrite(ledChannel3, globalB);

      // Serial.print("globalR: "); Serial.println(globalR);
      // Serial.print("globalG: "); Serial.println(globalG);
      // Serial.print("globalB: "); Serial.println(globalB);
    }

    micros(); //update overflow
    loop();
    vTaskDelay(10);
  }
}

void APtask( void * parameter )
{
  //Initialize Webserver
  server.on("/", handleRoot);
  server.on("/setPWM/setLED", handlePWM); //Reads ADC function is called from out index.html /setPWM/?
  server.on("/buttonSet", handleButton);
  server.on("/LEDtoggl", handleRainbow);
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
