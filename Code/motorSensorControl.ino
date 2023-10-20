// --------------------------------------------------------------------------- //
// --------------------------------------------------------------------------- //
// ----         __   __   ________  ______   __ __ __   ______            ---- //
// ----        /_/\ /_/\ /_______/\/_____/\ /_//_//_/\ /_____/\           ---- //
// ----        \:\ \\ \ \\__.::._\/\::::_\/_\:\\:\\:\ \\:::_ \ \          ---- //
// ----         \:\ \\ \ \  \::\ \  \:\/___/\\:\\:\\:\ \\:(_) ) )_        ---- //
// ----          \:\_/.:\ \ _\::\ \__\::___\/_\:\\:\\:\ \\: __ `\ \       ---- //
// ----           \ ..::/ //__\::\__/\\:\____/\\:\\:\\:\ \\ \ `\ \ \      ---- //
// ----            \___/_( \________\/ \_____\/ \_______\/ \_\/ \_\/      ---- //
// ----                                                                   ---- //
// ----                                                     v.0.1         ---- //
// --------------------------------------------------------------------------- //
// --------------------------------------------------------------------------- //

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //

// ----- Filename ----- // 
// ++ motorSensorControl
// -------------------- // 

// ----- Description ----- // 
// ++ Configuration code to preload to microcontroller to initiate WiFi/MQTT and specify pinout information.                                             //
// ----------------------- // 

// ----- Prerequisite packages ----- // 
// ++ esp32 (board manager): See tutorials https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/
// ++ PubSubClient
// ++ BufferedOutput
// ++ MillisDelay
// --------------------------------- // 

// ----- Configuration ----- // 
// ++ User-based configuration is only needed in the parameter setting section. Codes following this section shall NOT be modified.
// ++ See comments after each variable for more information. 
// ------------------------- // 

// ----- Author ----- // 
// ++ Jianyi Du, Piled Higher and Deeper
// ++ Stanford University
// ++ Sept 2022
// ----------------- // 

// ----- License ----- // 
// ++ CC BY-NC-SA 4.0
// ++ (a.k.a you are free to use, modify and redistribute as long as not using for commercials, keep the same license for the redistribution and cite me)
// ----------------- // 

// ------------------------------------ Header files (NO MODIFICATIONS) ----------------------------------- //
#include <WiFi.h>
#include <PubSubClient.h>
#include <millisDelay.h>
#include <BufferedOutput.h>

// ---------------------------- User-defined parameters (MODIFICATIONS NEEDED) ---------------------------- //

// ----- Note:
// ++ These parameters are the default values onlyZ. Real values can be modified in the Node-RED interface.
// ++ Refer to Table 4 for more information. 

int dirPin = 19;                         // pinout for DIR of A4988 driver
int stepPin = 18;                        // pinout for STEP of A4988 driver 
int pPinIn = 34;                         // pinout for analog input of inlet pressure sensor
int pPinOut = 35;                        // pinout for analog input of outlet pressure sensor
int stepsPerRevolution = 3200;           // steps per revolution for stepper motor, reconfigured by MS1/2/3 on the A4988 driver
int sampleTimeGap = 100;                 // time gap for pressure sensor reading, unit in ms
double ID = 7.29;                        // inner diameter of the syringe, unit in mm
double pitchL = 1.00;                    // pitch length (distance in z per rotation on the linear rail)
double L = 9.0;                          // inner width of the capillary channel, unit in mm
double H = 0.9;                          // inner height of the capillary channel, unit in mm
double minimalStrain = 25.0;             // minimal strain or time for each shear rate
double sr = 0;                           // initial shear rate; put 0 as a safe value, unit in 1/s
int dir = 1;                             // 1 corresponds to forward (extrusion) direction for the syringe; only change if needed
unsigned long nowTime;


const char* ssid = "xxx";                // SSID for the WiFi hotspot 
const char* password = "wantSensors";    // password for the WiFi hotspot
const char* mqtt_server = "192.168.4.1"; // IP address of the host server where hotspot is created and Node-RED is running


// ------------------------------------- NO MODIFICATIONS NEEDED BELOW ------------------------------------ //

createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);
millisDelay analogInputDelay;
WiFiClient espClient;
PubSubClient client(espClient);

long now = millis();
long lastMeasure = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(String topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if(topic=="motor/config/pin"){
      Serial.print("Motor dir/step pins: ");
      Serial.print(messageTemp);
      int mlen = messageTemp.length();
      int index = messageTemp.indexOf(' ', 0);
      dirPin = messageTemp.substring(0, index).toInt();
      stepPin = messageTemp.substring(index, length).toInt();
      pinMode(dirPin, OUTPUT);
      pinMode(stepPin, OUTPUT);
  }
  Serial.println();

  if(topic=="motor/config/step"){
      Serial.print("Step per revolution: ");
      Serial.print(messageTemp);
      stepsPerRevolution = messageTemp.length();
  }
  Serial.println();

  // motor pins config
  if(topic=="psensor/config/pin"){
      Serial.print("Pressure Sensor Pin: ");
      Serial.print(messageTemp);
      int mlen = messageTemp.length();
      int index = messageTemp.indexOf(' ', 0);
      pPinIn = messageTemp.substring(0, index).toInt();
      pPinOut = messageTemp.substring(index, length).toInt();
  }
  Serial.println();

  // geometry config/channel 
  if(topic=="geometry/config/channel"){
      Serial.print("Geometry dimension: ");
      Serial.print(messageTemp);
      int mlen = messageTemp.length();
      int index = messageTemp.indexOf(' ', 0);
      L = messageTemp.substring(0, index).toInt();
      H = messageTemp.substring(index, length).toInt();
  }
  Serial.println();

  // geometry config/device 
  if(topic=="geometry/config/device"){
      Serial.print("ID (mm) / Pitch (mm/rev): ");
      Serial.print(messageTemp);
      int mlen = messageTemp.length();
      int index = messageTemp.indexOf(' ', 0);
      ID = messageTemp.substring(0, index).toDouble();
      pitchL = messageTemp.substring(index, length).toDouble();
  }
  Serial.println();

  // runtime config/sampling
  if(topic=="runtime/config/sampling"){
      Serial.print("Sampling time period (ms): ");
      Serial.print(messageTemp);
      sampleTimeGap = messageTemp.toInt();
  }
  Serial.println();

  // runtime config/max strain
  if(topic=="runtime/config/strain"){
      Serial.print("Minimal strain: ");
      Serial.print(messageTemp);
      minimalStrain = messageTemp.toDouble();
  }
  Serial.println();

  // This will trigger the execution
  if(topic=="runtime/srstart"){
      int mlen = messageTemp.length();
      int index = messageTemp.indexOf(' ', 0);
      sr = messageTemp.substring(0, index).toDouble();
      dir = messageTemp.substring(index, length).toInt();

      Serial.println("--- START ---");
      Serial.print("Shear rate (1/s): ");
      Serial.println(sr);
      double currentQ = H*H*L*sr/6; // mm^3/s
      Serial.print("Flow rate (mm^3/s): ");
      Serial.println(currentQ);
      double timeShear = max(minimalStrain, minimalStrain/sr);
      Serial.print("Time of shearing (s): ");
      Serial.println(timeShear);
      double speedRev = currentQ*stepsPerRevolution/pitchL/(3.1416*ID*ID/4);
      Serial.print("Speed of steps (steps/s): ");
      Serial.println(speedRev);
      Serial.println("------ Data sent to MQTT ------");

      nowTime = millis();
      analogInputDelay.start(sampleTimeGap);
      double currentX = 6521.4046875/currentQ; // microseconds
      if (dir == 1){
        digitalWrite(dirPin, HIGH);
      }else{
        digitalWrite(dirPin, LOW);
      }
      while (millis() - nowTime < timeShear*1000) {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(currentX);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(currentX);
        analogInput13();
      }      
            
      client.publish("runtime/pDiff", "--- END ---");
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");  
      client.publish("status", "WiFi and MQTT connected.");
      // Subscribe or resubscribe to a topic
      client.subscribe("motor/config/pin");
      client.subscribe("motor/config/step");
      client.subscribe("psensor/config/pin");
      client.subscribe("geometry/config/channel");
      client.subscribe("geometry/config/device");
      client.subscribe("runtime/config/sampling");
      client.subscribe("runtime/config/strain");
      client.subscribe("runtime/srstart");      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {  
  Serial.begin(9600);
  bufferedOut.connect(Serial);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void analogInput13(){
  if (analogInputDelay.justFinished()) {   // check if delay has timed out
      analogInputDelay.repeat(); // start delay again without drift
      char pinOut[100];
      int inValue = analogRead(pPinIn);
      int outValue = analogRead(pPinOut);
      sprintf(pinOut,"%i %i", inValue, outValue);
      client.publish("runtime/pDiff", pinOut);
      
    }
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  if(!client.loop())
    client.connect("ESP32Client");

} 

// ------------------------------------- NO MODIFICATIONS NEEDED ABOVE ------------------------------------ //
