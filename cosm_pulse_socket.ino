#include <SPI.h>
#include <Ethernet.h>
#include <HttpClient.h>
#include <Cosm.h>

/*
>> Pulse Sensor Amped 1.1 <<
 This code is for Pulse Sensor Amped by Joel Murphy and Yury Gitman
 www.pulsesensor.com 
 >>> Pulse Sensor purple wire goes to Analog Pin 0 <<<
 Pulse Sensor sample aquisition and processing happens in the background via Timer 2 interrupt. 2mS sample rate.
 PWM on pins 3 and 11 will not work when using this code, because we are using Timer 2!
 The following variables are automatically updated:
 Signal :    int that holds the analog signal data straight from the sensor. updated every 2mS.
 IBI  :      int that holds the time interval between beats. 2mS resolution.
 BPM  :      int that holds the heart rate value, derived every beat, from averaging previous 10 IBI values.
 QS  :       boolean that is made true whenever Pulse is found and BPM is updated. User must reset.
 Pulse :     boolean that is true when a heartbeat is sensed then false in time with pin13 LED going out.
 
 This code is designed with output serial data to Processing sketch "PulseSensorAmped_Processing-xx"
 The Processing sketch is a simple data visualizer. 
 All the work to find the heartbeat and determine the heartrate happens in the code below.
 Pin 13 LED will blink with heartbeat.
 If you want to use pin 13 for something else, adjust the interrupt handler
 It will also fade an LED on pin fadePin with every beat. Put an LED and series resistor from fadePin to GND.
 Check here for detailed code walkthrough:
 http://pulsesensor.myshopify.com/pages/pulse-sensor-amped-arduino-v1dot1
 
 Code Version 02 by Joel Murphy & Yury Gitman  Fall 2012
 This update changes the HRV variable name to IBI, which stands for Inter-Beat Interval, for clarity.
 Switched the interrupt to Timer2.  500Hz sample rate, 2mS resolution IBI value.
 Fade LED pin moved to pin 5 (use of Timer2 disables PWM on pins 3 & 11).
 Tidied up inefficiencies since the last version. 
 */


//  VARIABLES
int pulsePin = 0;                 // Pulse Sensor purple wire connected to analog pin 0
int blinkPin = 9;                // pin to blink led at each beat
int fadePin = 5;                  // pin to do fancy classy fading blink at each beat
int fadeRate = 0;                 // used to fade LED on with PWM on fadePin
int numberOfBeats = 0;
int averageBPM = 0;
int averageNumber = 10;
long previousUploadTime = 0;
long uploadInterval = 100;

// these variables are volatile because they are used during the interrupt service routine!
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, the Inter-Beat Interval
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.

// MAC address for your Ethernet shield
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x91, 0x2D }; //CHANGE THIS TO MATCH THE NUMBER ON YOUR STICKER
char serverName[] = "api.cosm.com";

// Your Cosm key to let you upload data
char feedId[] = "104810"; //FEED ID
char cosmKey[] = "YOUR API KEY HERE"; //API KEY
char cosmKey[] = "ewTCG0qri8i6jXsXxwXxnrAZpnKSAKxHL0tnbndNeEpPdz0g";

// Define the strings for our datastream IDs
char sensorId[] = "sensor_reading"; //This should not contain a space ' ' char

EthernetClient client;


void setup() {
  // put your setup code here, to run once:
  pinMode(blinkPin,OUTPUT);         // pin that will blink to your heartbeat!
  pinMode(fadePin,OUTPUT);          // pin that will fade to your heartbeat!
  Serial.begin(115200);             // we agree to talk fast!
  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS 

  Serial.println("Starting PULSE upload to Cosm...");
  Serial.println();

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore: 
    while(true);
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("connecting...");

  // if you get a connection, report back via serial:

  if (client.connect(serverName, 8081)) {
    Serial.println("connected");
  } 
  else {
    // kf you didn't get a connection to the server:
    Serial.println("connection failed");
  }
}

void loop() {
  unsigned long currentMillis = millis();
  //sendDataToProcessing('S', Signal);     // send Processing the raw Pulse Sensor data
  if (QS == true){                       // Quantified Self flag is true when arduino finds a heartbeat
    fadeRate = 255;                  // Set 'fadeRate' Variable to 255 to fade LED with pulse
    //sendDataToProcessing('B',BPM);   // send heart rate with a 'B' prefix
    //sendDataToProcessing('Q',IBI);   // send time between beats with a 'Q' prefix
     sendDataToCosm(BPM);             //send data to cosm sockets using arduino ethernet/shield
    QS = false;                      // reset the Quantified Self flag for next time    
  }

  //ledFadeToBeat(); //This will fade the led to the beat if you have one plugged into a PWM pin (definded above)
  
  // from the server, read them and print them:
  if (client.available()) {
    Serial.println(client.parseInt()); //This line will give you the HTTP response code from the server ie 200, 404...
    //char c = client.read(); //This line will read all the response data... Use this line and the next for more verbose logging
    //Serial.print(c); //Don't forget me for verbose logging!
    client.flush();
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    
    //Lets try to reconnect...
    Serial.println("connecting...");

    // if you get a connection, report back via serial:
    if (client.connect(serverName, 8081)) {
      Serial.println("connected");
    } 
    else {
      // if you didn't get a connection to the server:
      Serial.println("connection failed");
    }
  }

  delay(20);                             //  take a break
}

void ledFadeToBeat(){
  fadeRate -= 15;                         //  set LED fade value
  fadeRate = constrain(fadeRate,0,255);   //  keep LED fade value from going into negative numbers!
  analogWrite(fadePin,fadeRate);          //  fade LED
}


void sendDataToProcessing(char symbol, int data ){
  Serial.print(symbol);                // symbol prefix tells Processing what type of data is coming
  Serial.println(data);                // the data to send culminating in a carriage return
}

void sendDataToCosm(int data){
  client.print("{\"method\" : \"put\",");
  client.print("\"resource\" : \"/feeds/");
  client.print(feedId);
  client.print("\", \"headers\" :{\"X-ApiKey\" : \"");
  client.print(cosmKey);
  client.print("\"},\"body\" :{ \"version\" : \"1.0.0\",\"datastreams\" : [{\"id\" : \"");
  client.print(sensorId);
  client.print("\",\"current_value\" : \"");
  client.print(data);
  client.print("\"}]}}");
  client.println();
}


