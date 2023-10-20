/*
  This code is the one used on the ESP32 which is performing the measurements in the Black Soldier Flies project.
*/

//make sure Arduino IDE is not open when using VS code /!\ -> otherwise USB PORT COM problem will occur

/*
Example:
A fatal error occurred: Could not open COM8, the port doesn't exist
*** [upload] Error 2
*/



//uncomment the wifi you want to use
#define WIFI_WORKSHOP
// #define ESP32

#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>

#include <Wire.h>
#include "time.h"

//AHT temperature sensors:
#include <Wire.h>
#include <Adafruit_AHTX0.h>
Adafruit_AHTX0 aht; // I2C

//include the following for the Firebase library to work.
// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials and make sure the define at the beginning of the document is correct
#if defined(WIFI_WORKSHOP)
  #define WIFI_SSID "Workshop"
  #define WIFI_PASSWORD "@@vigyan@@"

  //#define WIFI_NAME "WS"
#endif

// Insert Firebase project API Key
#define API_KEY "AIzaSyB6g30vxXbnz8L7ZSJ5Fu18mJdOs0yikt0" //"va-black-soldier-flies" firebase project

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "bsf_test@gmail.com" //you can also choose: va_esp32_test@gmail.com
#define USER_PASSWORD "123456789"

// Insert RTDB URLefine the RTDB URL
#define DATABASE_URL "https://va-bsf-temp-humid-v2-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Data wire is plugged TO GPIO 4
/* #define ONE_WIRE_BUS 4
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire); */
float temp=0;
float rh=0;
int hours;
int minutes;

/* // Number of temperature devices found
int numberOfDevices;*/
// We'll use this variable to store a found device address
//DeviceAddress tempDeviceAddress;

// Database main path (to be updated in setup with the user UID)
String databasePath;

// Database child nodes
String temppath = "/temperature0";
String prespath = "/temperature1";
String humPath = "/humidity";
String timePath = "/timestamp";

// Parent Node (to be updated in every loop)
String parentPath;

int timestamp;
FirebaseJson json;

const char* ntpServer = "pool.ntp.org";

float humidity;

// Timer variables (send new readings every three minutes)
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 15*60*1000; //30 minute delay, 1 sec = 1000 for timerDelay // change for testing
//unsigned long timerDelay = 10*1000; //10s delay change for testing

//actuator 
//#define RELAY_CHOICE_1
#define RELAY_CHOICE_2

#if defined(RELAY_CHOICE_1)
  #define ACTIVATED HIGH
  #define DEACTIVATED LOW
#endif
#if defined(RELAY_CHOICE_2)
  #define ACTIVATED LOW
  #define DEACTIVATED HIGH
#endif

//variables for actuators
unsigned long delayTime;
//bool fan_state = 0;
bool heater_state = 0;
bool humidifier_state = 0;
bool pad_state = 0;

const int heater = 26;
const int humidifier = 27;
const int pad = 33;
const int ir_read = A3;//39;
const int ir_power = 34; // not used Vin instead
 

// Initialize WiFi
void initWiFi() {
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.print("\nConnected to WiFi!\n");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Function that gets current epoch time
unsigned long getTime() 
{
  time_t now;
  struct tm timeinfo;
  //5h30 dECALAGE
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  hours = timeinfo.tm_hour;
  minutes = timeinfo.tm_min;
  time(&now);
  return now;
}

int ir_value() 
{
	//digitalWrite(ir_power, HIGH);	// Turn the sensor ON
	delay(10);							// Allow power to settle
	int val = analogRead(ir_read);	// Read the analog value form sensor
	//digitalWrite(ir_power, LOW);		// Turn the sensor OFF
	return val;							// Return analog moisture value
}

void control()
{
  // test //
 // temp = 20;

  //end test //


  Serial.println("Entering control....");


  //test begin
  int check_ir = ir_value();
  Serial.print("moisture level = ");
  Serial.println(check_ir);

  //test end 
  if(temp>37||rh<55)
  {
    if(hours>=4 && hours<=12)
    {
      int check_ir = ir_value();
      if (check_ir >= 3400)
      {
        digitalWrite(pad, ACTIVATED /*ACTIVATED*/);
        Serial.println("pads on...");
      }
      else if (check_ir <= 2500)
      {
      digitalWrite(pad, DEACTIVATED /*DEACTIVATED*/);
      Serial.println("pads off...");
      }
    }
    else
    {
      digitalWrite(pad, DEACTIVATED /*DEACTIVATED*/);
      Serial.println("pads off...");
    }  
  }
  else if (temp<35||rh>70)
  {
    digitalWrite(pad, DEACTIVATED /*DEACTIVATED*/);
    Serial.println("pads off...");
  }

  if(temp<30)
  {
    digitalWrite(heater, ACTIVATED /*ACTIVATED*/);
    Serial.println("Heater turned on");
  }
  else if(temp>35)
  {
    digitalWrite(heater, DEACTIVATED /*DEACTIVATED*/);
    Serial.println("heater turned off");
  }
  
  if(rh<50)
  {
    Serial.println(hours);
    if(hours>=5 && hours<=11)
    {
      digitalWrite(humidifier, ACTIVATED /*ACTIVATED*/);
      Serial.println("Humidifier turned on");
    }
    else
    {
      digitalWrite(humidifier, DEACTIVATED /*DEACTIVATED*/);
      Serial.println("Humidifier turned off");  
    }  
  }
  else if(rh>70)
  {
    digitalWrite(humidifier, DEACTIVATED /*DEACTIVATED*/);
    Serial.println("Humidifier turned off");  
  }
}


/************************************SETUP************************************/

void setup(){

  //relay
  pinMode(pad,OUTPUT);
  pinMode(heater,OUTPUT);
  pinMode(humidifier,OUTPUT);
  pinMode(ir_power, OUTPUT);

  digitalWrite(pad, DEACTIVATED);
  digitalWrite(heater, DEACTIVATED);
  digitalWrite(humidifier, DEACTIVATED);
  digitalWrite(ir_power, LOW);
  // Initialize WiFi
  initWiFi();

  Serial.begin(115200);
   bool status;
  // default settings
    if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found"); 
  

  configTime(0, 0, ntpServer);

  // Assign the api key (required)
  config.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RTDB URL (required)
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.print(uid);

  // Update database path
  databasePath = "/UsersData/"+ uid + "/readings";

  Serial.println("\n\nEnd of void setup function");
}

//************************************LOOP************************************

void loop(){

  Serial.println("entering loop...");
  
  sensors_event_t humidity, tempread;
  aht.getEvent(&humidity, &tempread);// populate temp and humidity objects with fresh data
  
  temp = tempread.temperature;
  rh = humidity.relative_humidity;

  Serial.print("\nTemperature device= ");
  Serial.print(temp);
  Serial.print("°C");
  Serial.print("\nHumidity= ");
  Serial.print(rh);
  Serial.print("%\n");

  
  timestamp = getTime();
  Serial.print ("current time: ");
  Serial.println (timestamp);
  Serial.println (hours);Serial.println (minutes);

  // //Check variables and activate controls
  control();

  // Send new readings to database
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    //Get current timestamp
    timestamp = getTime();
    Serial.print ("current time: ");
    Serial.println (timestamp);

    parentPath= databasePath + "/" + String(timestamp);

    json.set(temppath.c_str(), String(temp));
    json.set(humPath.c_str(), String(rh));
    json.set(timePath, String(timestamp));

    //We can call that instruction inside a Serial.printf() command to print the results in the Serial Monitor at the same time the command runs.
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  }
}