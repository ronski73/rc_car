#include <ESP8266WiFi.h>
#include <Wire.h>
#include <LOLIN_I2C_MOTOR.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

LOLIN_I2C_MOTOR motor; //I2C address 0x30
// LOLIN_I2C_MOTOR motor(DEFAULT_I2C_MOTOR_ADDRESS); //I2C address 0x30
// LOLIN_I2C_MOTOR motor(your_address); //using customize I2C address

// config:

const char *ssid = "rc_car";  // You will connect your phone to this Access Point
const char *pw = "qwerty123"; // and this is the password
IPAddress ip(192, 168, 0, 1); // From RoboRemo app, connect to this IP
IPAddress netmask(255, 255, 255, 0);

int chVal[] = {0, 0, 0, 0}; // default value (middle)

// Roboremo connection
const int port = 9876;        // and this port
WiFiServer RemoServer(port);
WiFiClient RemoClient;
// OTA connection
WiFiServer TelnetServer(23);  // Telnet Server Declaration port 23
WiFiClient SerialOTA;         // Telnet Client Declaration 
bool haveClient = false;      // Telnet client detection flag

//variabls for blinking an LED with Millis
const int led = LED_BUILTIN;        // ESP8266 Pin to which onboard LED is connected
unsigned long previousMillis = 0;   // will store last time LED was updated
const long interval = 1000;         // interval at which to blink (milliseconds)
int ledState = HIGH;                // ledState used to set the LED

// vars to control car
char cmd[100];                      // stores the command chars received from RoboRemo
int cmdIndex;                
int vel,inttotsteer;
float steer,totsteer;

// keepalives from app for safety
unsigned long lastCmdTime = 60000;
unsigned long aliveSentTime = 0;

// send debug message to serial AND OTA if available
void DbgMessage(String logmessage)
{ // write line to log
  if (haveClient) {
    SerialOTA.println(logmessage);
  }
  Serial.println(logmessage);
}

// check which command is send by roboremo (ie: ch0 100, Cho is command, 100 is controlvalue)
boolean cmdStartsWith(const char *st) { // checks if cmd starts with st
  for(int i=0; ; i++) {
    if(st[i]==0) return true;
    if(cmd[i]==0) return false;
    if(cmd[i]!=st[i]) return false;;
  }
  return false;
}

void exeCmd() { // executes the command from cmd

  lastCmdTime = millis();

  DbgMessage(cmd);
  
  // example: set RoboRemo slider id to "ch0"
  // set min -100 (full backward)
  // and set max 100 (full forward)
  
  if( cmdStartsWith("ch") ) {
    int ch = cmd[2] - '0';
//    DbgMessage("slider detected");
    if(ch>=0 && ch<=9 && cmd[3]==' ') {
      chVal[ch] = (int)atof(cmd+4);
//      DbgMessage("setting motor");
//      DbgMessage(chVal[ch]);

      if (chVal[0] > 0) {
        motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CCW);
        motor.changeStatus(MOTOR_CH_B, MOTOR_STATUS_CW);
//        vel = chVal[0];
      }
      else if (chVal[0] < 0) {
        motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CW);
        motor.changeStatus(MOTOR_CH_B, MOTOR_STATUS_CCW);
//        vel = (-1*chVal[0]); 
      } 

      vel = abs(chVal[0]);
      steer = vel - abs(chVal[1]);
      if (steer<0) { steer = 0; }
          
      DbgMessage("steer");
      DbgMessage(String(steer));
      DbgMessage("vel");
      DbgMessage(String(vel));
      
      //motor.changeDuty(MOTOR_CH_BOTH, chVal[1]);
      if (chVal[1] > 0) {
        motor.changeDuty(MOTOR_CH_A, vel);
        motor.changeDuty(MOTOR_CH_B, steer);
      }
      else if (chVal[1] < 0) { // left
        motor.changeDuty(MOTOR_CH_A, steer);
        motor.changeDuty(MOTOR_CH_B, vel);
      }
      else {           // straight
        motor.changeDuty(MOTOR_CH_BOTH, vel);
      }
    }
  }
}

void setup() {

  // serial output
  Serial.begin(115200);
  // turn led on to show that we are active
  pinMode(LED_BUILTIN, OUTPUT);
  // turn on I2C
  Wire.begin();
  delay(1000);  // wait to give everything to time to start

  // scan I2C bus (making sure the motor shield is on   
  DbgMessage("\nI2C Scanner");

  byte error, address;
  int nDevices;
 
  DbgMessage("Scanning...");
 
  nDevices = 0;
  for(address = 1; address < 127; address++ ) 
  { 
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.print(address,HEX);
      DbgMessage("  !");
 
      nDevices++;
    }
    else if (error==4) 
    {
      DbgMessage("Unknow error at address 0x");
      if (address<16) 
        DbgMessage("0");
      DbgMessage(String(address,HEX));
    }    
  }
  if (nDevices == 0)
    DbgMessage("No I2C devices found\n");
  else
    DbgMessage("done\n");

  cmdIndex = 0;
  
  while (motor.PRODUCT_ID != PRODUCT_ID_I2C_MOTOR) //wait motor shield ready.
  {
    motor.getInfo();  // just do something
  }

  // init motorshield
  motor.changeFreq(MOTOR_CH_BOTH, 10000);               // Change A & B 's Frequency to 10000Hz.
  motor.changeDuty(MOTOR_CH_BOTH, 100);                 // Change A & B 's Duty to 100%
  motor.changeStatus(MOTOR_CH_BOTH, MOTOR_STATUS_CCW);  // Give motors some direction

  // setup WIFI AP
  WiFi.softAPConfig(ip, ip, netmask); // configure ip address for softAP 
  WiFi.softAP(ssid, pw); // configure ssid and password for softAP

  // setup Roboremo server for reveiving commands via App
  RemoServer.begin(); // start TCP server

  DbgMessage("ESP8266 RC receiver 1.1 powered by RoboRemo");
  DbgMessage((String)"SSID: " + ssid + "  PASS: " + pw);
  DbgMessage((String)"RoboRemo app must connect to " + ip.toString() + ":" + port);

  //Setup Telnet server for remote debugging
  TelnetServer.begin();
  TelnetServer.setNoDelay(true);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DbgMessage("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DbgMessage("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DbgMessage("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DbgMessage("Receive Failed");
    else if (error == OTA_END_ERROR) DbgMessage("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  for(int i=0; i<4;i++){      //builtin led flashes 4 times setup succesfull
    digitalWrite(LED_BUILTIN,LOW);
    delay(100);
    digitalWrite(LED_BUILTIN,HIGH);
    delay(100);
  }

}

void loop() {

  ArduinoOTA.handle();

  // ========================================
  // Roboremo part
  // ========================================
  if(!RemoClient.connected()) {
    RemoClient = RemoServer.available();
    //loop to blink without delay
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      // save the last time you blinked the LED
      previousMillis = currentMillis;
      // if the LED is off turn it on and vice-versa:
      ledState = not(ledState);
      // set the LED with the ledState of the variable:
      digitalWrite(led,  ledState);
//    DbgMessage("no client......");
    }
  }

  if(RemoClient.available()) {
    digitalWrite(BUILTIN_LED,LOW);
//    DbgMessage("here we have a connected client!");
    char c = (char)RemoClient.read(); // read char from client (RoboRemo app)

    if(c=='\n') { // if it is command ending
//      DbgMessage("command ending");
      cmd[cmdIndex] = 0;
      exeCmd();  // execute the command
      cmdIndex = 0; // reset the cmdIndex
    } else {      
      cmd[cmdIndex] = c; // add to the cmd buffer
      if(cmdIndex<99) cmdIndex++;
    }
  } 

  // ========================================
  // OTA part
  // ========================================
  // Handle new/disconnecting clients.
  if (!haveClient) {
    // Check for new client connections.
    SerialOTA = TelnetServer.available();
    if (SerialOTA) {
      haveClient = true;
      SerialOTA.println("Hello there! :)");
    }
  } else if (!SerialOTA.connected()) {
    // The current client has been disconnected.
    SerialOTA.stop();
    SerialOTA = WiFiClient();
    haveClient = false;
  }

}
