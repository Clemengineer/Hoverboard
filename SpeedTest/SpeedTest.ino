//Hoverboard Manual Speed
//designed for esp32
//based on https://github.com/RoboDurden/Hoverboard-Firmware-Hack-Gen2.x-GD32/tree/main/Arduino%20Examples/TestSpeed
//version 0.20240220 //added adc potentiometer support


#define _DEBUG      // debug output to first hardware serial port
//#define DEBUG_RX    // additional hoverboard-rx debug output
#define REMOTE_UARTBUS

#define SEND_MILLIS 100   // send commands to hoverboard every SEND_MILLIS millisesonds

#include "util.h"
#include "hoverserial.h"

//input method
//serial
#define input_serial
//ble
//rc receiver (PPM)
//servo (PWM)
//WiFi?
//#define input_ADC //potentiometer/twisth throthle (ADC)
//MQTT


//array for motors
//how many motors do you have
const size_t motor_count_total = 2;
//identify the motors by their slave number
int motors_all[motor_count_total] = {0,1};
//how many right motors
const size_t motor_count_right = 1;
//identify the motors by their slave number
int motors_right[motor_count_right] = {1};
//how many left motors
const size_t motor_count_left = 1;
//identify the motors by their slave number
int motors_left[motor_count_left] = {0};
//array for speed
int motor_speed[motor_count_total];
//array for istate
int slave_state[motor_count_total];
//offset
int motoroffset = motors_all[0] - 0;



//
int slaveidin;
int iSpeed;
int ispeedin;
int istatein;
int count=0;
String command;



  #define oSerialHover Serial2    // ESP32

SerialHover2Server oHoverFeedback;



void setup()
{
  #ifdef _DEBUG
    Serial.begin(115200);
    Serial.println("Hello Hoverboard V2.x :-)");
  #endif
  

#ifdef input_serial
 HoverSetupEsp32(oSerialHover,19200,16,17);
 #endif

}

// hover|0|0|0
// hover|0|200|0
// hover|0|-200|0

unsigned long iLast = 0;
unsigned long iNext = 0;
unsigned long iTimeNextState = 3000;
uint8_t  wState = 1;   // 1=ledGreen, 2=ledOrange, 4=ledRed, 8=ledUp, 16=ledDown, 32=Battery3Led, 64=Disable, 128=ShutOff
//id for messages being sent
uint8_t  iSendId = 0;   // only ofr UartBus

int baseSpeed = 100;

int currentLeft = 0;
int currentRight = 0;

void loop()
{
  unsigned long iNow = millis();
  //digitalWrite(39, (iNow%500) < 250);
  //digitalWrite(37, (iNow%500) < 100);

//look for incoming serial command
//hover|slave/motorid|speed|state
//speed can be from -1000 reverse full speed to 1000 forward full speed
//state can be 1=ledGreen, 2=ledOrange, 4=ledRed, 8=ledUp, 16=ledDown, 32=Battery3Led, 64=Disable, 128=ShutOff



#ifdef input_serial

if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    // ---- SPEED SET ----
    if (cmd.startsWith("m")) {
      int val = cmd.substring(1).toInt();
      if (val >= 0 && val <= 1000) {
          baseSpeed = val;

          // Reapply current motion with new speed
          if (currentLeft > 0) currentLeft = baseSpeed;
          else if (currentLeft < 0) currentLeft = -baseSpeed;

          if (currentRight > 0) currentRight = baseSpeed;
          else if (currentRight < 0) currentRight = -baseSpeed;

          Serial.print("Base speed set to: ");
          Serial.println(baseSpeed);
      } else {
          Serial.println("Invalid speed (0–1000)");
      }
  }

    // ---- MOVEMENT COMMANDS ----
    else if (cmd == "f") {
        currentLeft  = baseSpeed;
        currentRight = baseSpeed;
        Serial.println("Forward");
    }
    else if (cmd == "b") {
        currentLeft  = -baseSpeed;
        currentRight = -baseSpeed;
        Serial.println("Backward");
    }
    else if (cmd == "r") {
        currentLeft  = baseSpeed;
        currentRight = -baseSpeed;
        Serial.println("Turn Right");
    }
    else if (cmd == "l") {
        currentLeft  = -baseSpeed;
        currentRight = baseSpeed;
        Serial.println("Turn Left");
    }
    else if (cmd == "s") {
        currentLeft  = 1;
        currentRight = 1;
        Serial.println("Stop");
    }
    else {
        Serial.println("Unknown command");
    }

    // ---- APPLY TO MOTOR ARRAY ----
    motor_speed[0] = currentLeft;

    // invert motor 1
    motor_speed[1] = -currentRight;

    // keep state simple
    slave_state[0] = 0;
    slave_state[1] = 0;

    #ifdef _DEBUG
    Serial.print("Left: ");
    Serial.print(motor_speed[0]);
    Serial.print(" | Right: ");
    Serial.println(motor_speed[1]);
    #endif
}

#endif


  

  int iSteer =0;   // repeats from +100 to -100 to +100 :-)
  //int iSteer = 0;
  //int iSpeed = 500;
  //int iSpeed = 200;
  //iSpeed = iSteer = 0;

  if (iNow > iTimeNextState)
  {
    iTimeNextState = iNow + 3000;
    wState = wState << 1;
    if (wState == 64) wState = 1;  // remove this line to test Shutoff = 128
  }
  
  boolean bReceived;   
  while (bReceived = Receive(oSerialHover,oHoverFeedback))
  {
    DEBUGT("millis",iNow-iLast);
    DEBUGT("iSpeed",iSpeed);
    //DEBUGT("iSteer",iSteer);
    HoverLog(oHoverFeedback);
    iLast = iNow;
  }

  if (iNow > iNext)
  {
    //DEBUGLN("time",iNow)
    #ifdef REMOTE_UARTBUS
      count = 0;
      while (count < motor_count_total){
         HoverSend(oSerialHover,motors_all[count],motor_speed[count],slave_state[count]);
//           #ifdef _DEBUG
//                Serial.print("Sent Motor ");
//                Serial.print(motors_all[count]);
//                Serial.print(" Speed ");
//                Serial.print(motor_speed[count]);
//                Serial.print (" and Slave State ");
//                Serial.println(slave_state[count]);
//            #endif
                   count ++;  
      }


      iNext = iNow + SEND_MILLIS/2;
    #else
      //if (bReceived)  // Reply only when you receive data
        HoverSend(oSerialHover,iSteer,iSpeed,wState,wState);
      
      iNext = iNow + SEND_MILLIS;
    #endif
  }


}