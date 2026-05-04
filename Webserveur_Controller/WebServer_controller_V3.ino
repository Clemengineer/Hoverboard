//Hoverboard WIFI controller
//designed for esp32
//based on https://github.com/RoboDurden/Hoverboard-Firmware-Hack-Gen2.x-GD32/tree/main/Arduino%20Examples/TestSpeed

#define _DEBUG
//#define DEBUG_RX
#define REMOTE_UARTBUS

#define SEND_MILLIS  100 
#define RAMP_STEP_DEFAULT 40 

#include "util.h"
#include "hoverserial.h"
#include <WiFi.h>
#include <ESPmDNS.h>

//WiFi credentials
const char* ssid     = "Tel"; //Name of the hotspot
const char* password = "ouicamarche"; // Password

//Serial communication between Boards and ESP32
#define oSerialHover Serial2   // RX=16, TX=17
SerialHover2Server oHoverFeedback;

//Motor configuration
const size_t motor_count_total  = 2; // nb of motors
int motors_all[motor_count_total] = {0, 1}; // Slave ID of motors
int motor_speed[motor_count_total];   //values sent to motors
int slave_state[motor_count_total];

// To increase speed gradually
int    baseSpeed    = 100; // speed by default
int    rampStep     = RAMP_STEP_DEFAULT;
String currentCommand = "stop";

int targetLeft  = 0; // traget speed values
int targetRight = 0;

int actualLeft  = 0; // current speed left
int actualRight = 0; // current speed right

unsigned long iNext          = 0;
unsigned long iTimeNextState = 3000;
unsigned long iLast          = 0;
uint8_t wState = 1;

WiFiServer server(80); // server on port 80
String header;
unsigned long webPreviousTime = 0;
const long    timeoutTime     = 2000;

// To increase the speed gradually
int rampToward(int current, int target, int step) {
  if (current < target) 
    {
      if (current + step == 0) // avoid 0, to avoid shutting down the motors
      {
        return min(current + 2*step, target);
      }
      else
      {
        return min(current + step, target);
      }
      
    }
  if (current > target) 
  {
    if (current - step == 0) // Same reason as before
    {
      return max(current - 2*step, target);
    }
    else
    {
      return max(current - step, target);
    }
    
  }
  return current;
}
// set target values
void applyCommand(String cmd) {
  currentCommand = cmd;

  if (cmd == "forward") {
    targetLeft  =  baseSpeed;
    targetRight =  baseSpeed;
  } else if (cmd == "backward") {
    targetLeft  = -baseSpeed;
    targetRight = -baseSpeed;
  } else if (cmd == "right") {
    targetLeft  =  baseSpeed;
    targetRight = -1;
  } else if (cmd == "left") {
    targetLeft  = -1;
    targetRight =  baseSpeed;
  } else {   // stop
    targetLeft  = 1; // 1 instead of 0 to avoid shutting down the board
    targetRight = 1; // Same thing
    currentCommand = "stop";
  }

  DEBUGN("CMD", cmd); // shows in terminal for debug
}

// HTML web page
void sendPage(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html>");
  client.println("<head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<link rel='icon' href='data:,'>");
  client.println("<title>HoverBoard Controller</title>");
  client.println("<style>");
  client.println("*{box-sizing:border-box;margin:0;padding:0;}");
  client.println("body{background:#1a1a2e;display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:100vh;font-family:Arial,sans-serif;color:#eee;gap:0;}");
  client.println("h1{font-size:1.3rem;letter-spacing:3px;color:#a0a8d0;margin-bottom:6px;}");
  client.println("#status{font-size:0.85rem;color:#5a6080;letter-spacing:2px;margin-bottom:14px;}");
  client.println("#status span{color:#7eb8f7;font-weight:bold;}");
  client.println(".row{display:flex;align-items:center;gap:10px;margin-bottom:10px;font-size:0.8rem;color:#5a6080;width:260px;}");
  client.println(".row label{width:90px;text-align:right;}");
  client.println(".row input[type=range]{flex:1;accent-color:#7eb8f7;}");
  client.println(".row .val{color:#7eb8f7;font-weight:bold;width:36px;text-align:left;}");
  client.println(".row button{background:#0f3460;border:1px solid #2a3560;color:#7eb8f7;padding:3px 8px;border-radius:6px;cursor:pointer;font-size:0.75rem;}");
  client.println(".spacer{margin-bottom:18px;}");
  client.println(".dpad{display:grid;grid-template-columns:repeat(3,80px);grid-template-rows:repeat(3,80px);gap:8px;}");
  client.println(".btn{display:flex;align-items:center;justify-content:center;background:#16213e;border:2px solid #2a3560;border-radius:12px;font-size:2rem;color:#c8d0f0;text-decoration:none;}");
  client.println(".btn:active{background:#0f3460;border-color:#7eb8f7;}");
  client.println(".stop{background:#2e1a1a;border-color:#6b2b2b;color:#f07070;}");
  client.println(".stop:active{background:#5c1c1c;border-color:#f07070;}");
  client.println(".empty{visibility:hidden;}");
  client.println("</style></head>");
  client.println("<body>");

  client.println("<h1>Hoverboard Controller</h1>");
  client.println("<div id='status'>Commande : <span>" + currentCommand + "</span></div>"); // To show the current sent command

  // Speed
  client.println("<form class='row' action='/speed' method='get'>");
  client.println("<label>Vitesse</label>");
  client.println("<input type='range' name='val' min='50' max='600' step='50' value='" + String(baseSpeed) + "' oninput='this.nextElementSibling.textContent=this.value'>"); // Only up to 600 to avoid too much speed if not on the ground
  client.println("<span class='val'>" + String(baseSpeed) + "</span>");
  client.println("<button type='submit'>OK</button>"); // To send the order
  client.println("</form>");

  client.println("<div class='spacer'></div>");

  // directions
  client.println("<div class='dpad'>");
  client.println("<div class='empty'></div>");
  client.println("<a class='btn' href='/forward'>&#8679;</a>"); // Up arrow
  client.println("<div class='empty'></div>");
  client.println("<a class='btn' href='/left'>&#8678;</a>"); // Lefft arrow
  client.println("<a class='btn stop' href='/stop'>&#9632;</a>"); // Stop/Middle button
  client.println("<a class='btn' href='/right'>&#8680;</a>");// Right arrow
  client.println("<div class='empty'></div>");
  client.println("<a class='btn' href='/backward'>&#8681;</a>"); // Down arrow
  client.println("<div class='empty'></div>");
  client.println("</div>");

  client.println("</body></html>");
  client.println();
}


void setup() {
  Serial.begin(115200);
  Serial.println("Hello Hoverboard V2.x :-)");

  HoverSetupEsp32(oSerialHover, 19200, 16, 17);

  applyCommand("stop"); // to start instantly the motors and not be blocked 
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); // Connects to the Wifi set up before
  while (WiFi.status() != WL_CONNECTED) { // Wait until it's connected
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP:");
  Serial.println(WiFi.localIP()); // gives ESP IP
  if (MDNS.begin("hoverboard")) { // Allow searching for hoverboard.local on any browser for any IP of the ESP32
    Serial.println("mDNS OK  ->  http://hoverboard.local");
  }
  MDNS.addService("http", "tcp", 80); // Type of com
  server.begin();
}


void loop() {
  unsigned long iNow = millis();

// for an led on the charger, makes it blink
  if (iNow > iTimeNextState) {
    iTimeNextState = iNow + 3000;
    wState = wState << 1;
    if (wState == 64) wState = 1;
  }

  // feedback from the board
  while (Receive(oSerialHover, oHoverFeedback)) {
    DEBUGT("millis", iNow - iLast);
    HoverLog(oHoverFeedback);
    iLast = iNow;
  }

  // Speed control, increase gradually the speed toward the target
  if (iNow > iNext) {
    actualLeft  = rampToward(actualLeft,  targetLeft,  20); // Step of 20 to go quickly
    actualRight = rampToward(actualRight, targetRight, 20);

    motor_speed[0] =  actualLeft; 
    motor_speed[1] = -actualRight; // because motors are inverted 

    int count = 0;
    while (count < (int)motor_count_total) { // for all motors
      HoverSend(oSerialHover, motors_all[count], motor_speed[count], slave_state[count]); // sends orders to the the master and slave board
      count++;
    }

    DEBUGT("actualL", actualLeft); // Shows speed in terminal for debug
    DEBUGN("actualR", actualRight);

    iNext = iNow + SEND_MILLIS / 2;
  }

  // shows HTML webpage
  WiFiClient client = server.available();
  if (client) {
    webPreviousTime = millis();
    Serial.println("New web client.");
    String currentLine = "";
    header = "";

    while (client.connected() && millis() - webPreviousTime <= timeoutTime) {

      // To avoid crashing the board if connection is empty (same thing as before)
      unsigned long wNow = millis();
      if (wNow > iNext) {
        actualLeft  = rampToward(actualLeft,  targetLeft,  20);
        actualRight = rampToward(actualRight, targetRight, 20);
        motor_speed[0] =  actualLeft;
        motor_speed[1] = -actualRight;
        int c2 = 0;
        while (c2 < (int)motor_count_total) {
          HoverSend(oSerialHover, motors_all[c2], motor_speed[c2], slave_state[c2]);
          c2++;
        }
        iNext = wNow + SEND_MILLIS / 2;
      }

      if (client.available()) {
        char c = client.read(); // reads input made by client
        header += c;

        if (c == '\n') {
          if (currentLine.length() == 0) {

            // apply the correct commabd
            if      (header.indexOf("GET /forward")  >= 0) applyCommand("forward");
            else if (header.indexOf("GET /backward") >= 0) applyCommand("backward");
            else if (header.indexOf("GET /left")     >= 0) applyCommand("left");
            else if (header.indexOf("GET /right")    >= 0) applyCommand("right");
            else if (header.indexOf("GET /stop")     >= 0) applyCommand("stop");
            else if (header.indexOf("GET /speed")    >= 0) {
              int idx = header.indexOf("val=");
              if (idx >= 0) {
                String valStr = header.substring(idx + 4);
                valStr = valStr.substring(0, valStr.indexOf(' '));
                int v = valStr.toInt();
                if (v >= 50 && v <= 1000) {
                  baseSpeed = v;
                  applyCommand(currentCommand); // target with new speed
                }
              }
            }

            sendPage(client); // Send the Html page to user
            break;

          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println("Client disconnected.");
  }
}
