#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP_Mail_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// data collection includes
#include "SparkFun_ACS37800_Arduino_Library.h"
#include <Wire.h>
#include <SPI.h>

// data encryption includes
#include <CryptoAES_CBC.h>
#include <AES.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//////// AMRIT WIFI CONNECTIONS ////////
//#define WIFI_SSID "WhiteSky-Aspire"
//#define WIFI_PASSWORD "r6agd8b9"
//#define WIFI_SSID "meep"        
//#define WIFI_PASSWORD "hellothere"
////////////////////////////////////////

//////// WILL WIFI CONNECTIONS ////////
//#define WIFI_SSID "Appelt"
//#define WIFI_PASSWORD "appelt2023"
#define WIFI_SSID "will"
#define WIFI_PASSWORD "aaaaaaaa"
///////////////////////////////////////

// Email Notification Definitions
#define SMTP_server "smtp.gmail.com"
#define SMTP_Port 465

#define sender_email "seniordesignteam6tamu@gmail.com"
#define sender_password "zdjg nhsq kbju dohr"

#define Recipient_email "williamappelt@gmail.com"
#define Recipient_name ""

// Mail Client Definitions
SMTPSession smtp;
ESP_Mail_Session session;
SMTP_Message message;

// Firebase Definitions
#define API_KEY "AIzaSyDb7RFk0ft9ZHKPIgFTZw192EHjcGu30hY"
#define DATABASE_URL "https://samsung-generator-test-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Firebase Address Definitions
String path_current = "Current/value";
String path_fuel = "Level/value";
String path_pressure = "Pressure/value";
String path_temp = "Temperature/value";
String path_voltage = "Voltage/value";

//// establish variables to be used in both setup and loop, or variables to be continuously updated in loop
// startup variables
bool signupOK = false;
int startup = 0;
int startupPrev = 0;
int count = 1; // data upload iteration counter
// data collection & upload variables
int adc0Value = 0;
int adc1Value = 0;
int adc2Value = 0;
float adc0Voltage = 0.0;
float adc1Voltage = 0.0;
float adc2Voltage = 0.0;
float fuel_level_data = 0;
float pressure_data = 0.0;
float temperature_data = 0.0;
float voltage_data = 0.0;
float current_data = 0.0;
float power = 0.0; // We currently only need voltage and current, but the ACS function requires a power variable.
String leading_zeroes = "";
// data encryption variables
byte key[16]={0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
byte pt_voltage[16];
byte pt_current[16];
byte pt_level[16];
byte pt_pressure[16];
byte pt_temp[16];
byte ct_voltage[16];
byte ct_current[16];
byte ct_level[16];
byte ct_pressure[16];
byte ct_temp[16];
byte decrypt_voltage[16];
byte decrypt_current[16];
byte decrypt_level[16];
byte decrypt_pressure[16];
byte decrypt_temp[16];
String hex_voltage = "";
String hex_current = "";
String hex_pressure = "";
String hex_level = "";
String hex_temp = "";
////

AES128 aes128;

//////// Sensor Initialization ////////
ACS37800 acs; // Create an object of the ACS37800 class
const int csPin = 2; // Update Chip Select pin
const int resolution = 15; // ADC resolution (bits) - real resolution is 16, but last bit is for sign. thus effective resolution is 15.
const float referenceVoltage = 2.5; // Reference voltage (V)
///////////////////////////////////////


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  aes128.setKey(key, 16);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  Serial.print("...");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println();
  Serial.println("Wi-Fi connection established.");

  //////// EMAIL NOTIFICATION SETUP ////////
  //smtp.debug(1); // Uncomment this line to print useful debug messages to Serial (for ESP Mail Client)

  session.server.host_name = SMTP_server ;
  session.server.port = SMTP_Port;
  session.login.email = sender_email;
  session.login.password = sender_password;
  session.login.user_domain = "";

  message.sender.name = "ESP32 - Samsung MGTA";
  message.sender.email = sender_email;
  message.subject = "Successful Data Collection and Upload";
  message.addRecipient(Recipient_name,Recipient_email);

  String textMsg = "This email is sent to notify you that an attempt to poll data for the diesel generator has been conducted. Please check the database or GUI to view updated generator data. Thank you!";
  message.text.content = textMsg.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!smtp.connect(&session)) {
    Serial.println("Error connecting to SMTP. Board reset is recommended to ensure reliable email functionality.");
    delay(5000);
  } else {
    Serial.println("Email notifications ready.");
  }
  //////// END EMAIL NOTIFICATION SETUP ////////

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("Successfully connected to Firebase.");
    signupOK = true; 
  //anon sign in
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // after connecting to wifi, now initialize the sensor stuff
  pinMode(csPin, OUTPUT);
  pinMode(16, OUTPUT);
  pinMode(17, OUTPUT);

  // Write 0V to DIO0(IO16) and DIO1(IO17)
  digitalWrite(16, LOW);
  digitalWrite(17, LOW);

  // Start I2C and SPI protocols
  Wire.begin();
  SPI.begin();

  //acs.enableDebugging(); // Uncomment this line to print useful debug messages to Serial (for ACS37800)

  // Initialize ACS37800 using default I2C address
  if (acs.begin() == false)
  {
    Serial.print("ACS37800 not detected. Check connections and I2C address. Freezing...");
    while (1) {
      Serial.print(".");
      delay(1000);
    }
  }

  // NOT SURE WHAT THE FOLLOWING SEGMENT DOES YET:
  // IF IT AINT BROKE DONT FIX IT!
  //////////////////////////////////////////////////

  // From the ACS37800 datasheet:
  // CONFIGURING THE DEVICE FOR DC APPLICATIONS : FIXED SETTING OF N
  // Set bypass_n_en = 1. This setting disables the dynamic calculation of n based off voltage zero crossings
  // and sets n to a fixed value, which is set using EERPOM field n
  //
  // Sample rate is 32kHz. Maximum number of samples is 1023 (0x3FF) (10-bit)
  acs.setNumberOfSamples(1023, true); // Set the number of samples in shadow memory and eeprom
  acs.setBypassNenable(true, true); // Enable bypass_n in shadow memory and eeprom

  //////////////////////////////////////////////////

  // Adjust default sense resistance for ACS37800 sensor
  acs.setSenseRes(15000); // Change the value of _senseResistance (Ohms)

  Serial.println("Setup complete!");
}


void loop() {
  // put your main code here, to run repeatedly:

  if (Firebase.ready() && signupOK) {
    
    // read startup signal from Firebase
    if (Firebase.RTDB.getInt(&fbdo, "startup")) {
      if(fbdo.dataType() == "int") {
        startupPrev = startup; // set previous startup signal value before updating new startup signal
        startup = fbdo.intData(); // update new startup signal
        Serial.println();
        Serial.print("startupPrev = ");
        Serial.println(startupPrev);
        Serial.print("startup = ");
        Serial.println(startup);

        // now if statement for startup signal implementation
        if (startup == 1) {
          // pull data and upload

          //////// DATA COLLECTION CODE ////////
          // Read ADC data (all channels)
          adc0Value = readADS8341(B10010111); // channel 0: Level
          adc1Value = readADS8341(B11010111); // channel 1: Pressure
          adc2Value = readADS8341(B10100111); // channel 2: Temperature

          // Convert ADC values to voltage
          adc0Voltage = adc0Value / (pow(2, resolution) - 1) * referenceVoltage * 2; // multiply by 2 to fix the range from 0-2.5 to 0-5 (on-board voltage divider)
          adc1Voltage = adc1Value / (pow(2, resolution) - 1) * referenceVoltage * 2; // multiply by 2 to fix the range from 0-2.5 to 0-5 (on-board voltage divider)
          adc2Voltage = adc2Value / (pow(2, resolution) - 1) * referenceVoltage;     // no voltage divider used, this is the true voltage output of the temp sensor

          // Convert voltage0 value to level
          if (adc0Voltage < 2.38) {
            fuel_level_data = 0;
          } else if (adc0Voltage > 3.43) {
            fuel_level_data = 100;
          } else {
            fuel_level_data = 100 * (2.75 - sqrt(24.91 - 7.29*adc0Voltage)) / 2.3;
            //fuel_level_data = 100/(3.43-2.38) * (adc0Voltage - 2.38);
          } // Approximation based on updated regression equation from the data in the Subsystems Validation Report

          // Convert voltage1 value to pressure
          pressure_data = (adc1Voltage - 0.5) * 400;
          if (pressure_data < 0) {
            pressure_data = 0;
          }
          // Pressure equation from sensor datasheet including the offset value measured in ECEN 403 (see subsystems validation report)
          // Based on sensor validation, the pressure value should only increase by roughly 2.4 kPa under full load versus zero load

          // Convert voltage2 value to temperature
          temperature_data = 100 * (adc2Voltage - 0.5);
          // Based on linear trendline from datasheet transfer table

          // Read Voltage/Current data
          acs.readInstantaneous(&voltage_data, &current_data, &power); // Read the instantaneous voltage, current, and power.
          if (voltage_data < 0) {
            voltage_data = voltage_data * -1;
          }
          if (current_data < 0) {
            current_data = current_data * -1;
          }

          // Round Floats to 2 Decimal Places
          voltage_data = ((int)round(voltage_data * 100)) / 100.0f;
          current_data = ((int)round(current_data * 100)) / 100.0f;
          fuel_level_data = ((int)round(fuel_level_data * 100)) / 100.0f;
          pressure_data = ((int)round(pressure_data * 100)) / 100.0f;
          temperature_data = ((int)round(temperature_data * 100)) / 100.0f;
          //////// END DATA COLLECTION CODE ////////

          //////// SETUP FOR DATA UPLOAD ////////
          if (count < 10) {           // example: 0009
            leading_zeroes = "000";
          } else if (count < 100) {   // example: 0099
            leading_zeroes = "00";
          } else if (count < 1000) {  // example: 0999
            leading_zeroes = "0";
          } else {                    // example: 1799
            leading_zeroes = "";
          }
          //////// END SETUP FOR DATA UPLOAD ////////

          //////// DATA ENCRYPTION ////////
          floatToPlaintextArray(voltage_data, pt_voltage);
          floatToPlaintextArray(current_data, pt_current);
          floatToPlaintextArray(fuel_level_data, pt_level);
          floatToPlaintextArray(pressure_data, pt_pressure);
          floatToPlaintextArray(temperature_data, pt_temp);

          aes128.encryptBlock(ct_voltage, pt_voltage);
          aes128.encryptBlock(ct_current, pt_current);
          aes128.encryptBlock(ct_level, pt_level);
          aes128.encryptBlock(ct_pressure, pt_pressure);
          aes128.encryptBlock(ct_temp, pt_temp);

          hex_voltage = toHexString(ct_voltage, sizeof(ct_voltage));
          hex_current = toHexString(ct_current, sizeof(ct_current));
          hex_level = toHexString(ct_level, sizeof(ct_level));
          hex_pressure = toHexString(ct_pressure, sizeof(ct_pressure));
          hex_temp = toHexString(ct_temp, sizeof(ct_temp));
          //////// END DATA ENCRYPTION ////////

          //////// DATA UPLOAD ////////
          if (Firebase.RTDB.setString(&fbdo, path_voltage + leading_zeroes + count, hex_voltage)) {
            aes128.decryptBlock(decrypt_voltage, ct_voltage);
            printByte(decrypt_voltage);
            Serial.print("\t - Successfully saved to: " + fbdo.dataPath());
            Serial.print("\t  (Expected: ");
            Serial.print(voltage_data);
            Serial.println(")");
          } else {
            Serial.println("Voltage upload failed: " + fbdo.errorReason());
          }

          if (Firebase.RTDB.setString(&fbdo, path_current + leading_zeroes + count, hex_current)) {
            aes128.decryptBlock(decrypt_current, ct_current);
            printByte(decrypt_current);
            Serial.print("\t - Successfully saved to: " + fbdo.dataPath());
            Serial.print("\t  (Expected: ");
            Serial.print(current_data);
            Serial.println(")");
          } else {
            Serial.println("Current upload failed: " + fbdo.errorReason());
          }

          if (Firebase.RTDB.setString(&fbdo, path_fuel + leading_zeroes + count, hex_level)) {
            aes128.decryptBlock(decrypt_level, ct_level);
            printByte(decrypt_level);
            Serial.print("\t - Successfully saved to: " + fbdo.dataPath());
            Serial.print("\t  (Expected: ");
            Serial.print(fuel_level_data);
            Serial.println(")");
          } else {
            Serial.println("Level upload failed: " + fbdo.errorReason());
          }

          if (Firebase.RTDB.setString(&fbdo, path_pressure + leading_zeroes + count, hex_pressure)) {
            aes128.decryptBlock(decrypt_pressure, ct_pressure);
            printByte(decrypt_pressure);
            Serial.print("\t - Successfully saved to: " + fbdo.dataPath());
            Serial.print("\t  (Expected: ");
            Serial.print(pressure_data);
            Serial.println(")");
          } else {
            Serial.println("Pressure upload failed: " + fbdo.errorReason());
          }

          if (Firebase.RTDB.setString(&fbdo, path_temp + leading_zeroes + count, hex_temp)) {
            aes128.decryptBlock(decrypt_temp, ct_temp);
            printByte(decrypt_temp);
            Serial.print("\t - Successfully saved to: " + fbdo.dataPath());
            Serial.print("  (Expected: ");
            Serial.print(temperature_data);
            Serial.println(")");
          } else {
            Serial.println("Temperature upload failed: " + fbdo.errorReason());
          }

          Serial.print("Data upload ");
          Serial.print(count);
          Serial.println(" complete!");

          count = count + 1; // data upload iteration counter
          //////// END DATA UPLOAD ////////

          // If counter runs well past 1800, write a zero to startup in Firebase as a failsafe.
          if (count >= 2000) {
            Serial.println();
            Serial.print("Data collection overflow reached! Attempting to manually disable startup signal...");
            delay(1000);

            while (1) { // continuously attempt to override startup signal until it is successfully updated.
              if (Firebase.RTDB.setInt(&fbdo, "startup", 0)) {
                Serial.println();
                Serial.print("0 - Successfully saved to: " + fbdo.dataPath());
                Serial.println(" (" + fbdo.dataType() + ") ");
                break;
              } else {
                Serial.print(".");
                delay(1000);
              }
            }

          }

        } else { // startup == 0
          // do nothing, except:

          // RESET COUNTER
          count = 1;

          // ONLY SEND EMAIL NOTIFICATION ONCE: ON THE FALLING EDGE OF startup
          if (startupPrev == 1) {
            // SEND EMAIL NOTIFICATION
            Serial.print("Sending email notification to ");
            Serial.print(Recipient_email);
            Serial.println("...");
            smtp.connect(&session);
            if (!MailClient.sendMail(&smtp, &message)) {
              Serial.println("Error sending email: " + smtp.errorReason());
            } else {
              Serial.println("Email notification sent!");
              delay(1000);
            }
          }

        }

      } // implied else: if fbdo somehow pulls something other than an int, something has gone wrong, so do nothing.

    } else { // fbdo failed to pull value for startup
      Serial.println();
      Serial.println("Failed to read startup signal: " + fbdo.errorReason());
    }

  } else { // firebase is not ready, or signupOK was not set to TRUE in initial setup
    Serial.println("ERROR: Firebase is not ready.");
  }

  //delay(600); // OVERALL DELAY. ADJUST TO MEET 1s REQUIREMENT
}


int readADS8341(byte commandBits) {
  // Select ADS8341
  digitalWrite(csPin, LOW);

  // Send command bits
  SPI.transfer(commandBits);

  // Read the 16-bit ADC value
  int adcValue = (SPI.transfer(0) << 8) | SPI.transfer(0);

  // Deselect ADS8341
  digitalWrite(csPin, HIGH);

  return adcValue;
}

void floatToPlaintextArray(float value, byte plaintext[16]) {
  // Initialize the byte array to zero
  memset(plaintext, 0, 16); // Fill the array with zeroes

  // Split the value into whole and fractional parts
  int wholePart = (int)value;
  int fractionalPart = round((value - wholePart) * 100); // Assuming two decimal places

  // Place the whole part into the byte array (14th and 15th spots)
  plaintext[13] = (wholePart >> 8) & 0xFF; // Higher byte of the whole part
  plaintext[14] = wholePart & 0xFF; // Lower byte of the whole part
  
  // Place the fractional part into the 16th spot of the array
  plaintext[15] = fractionalPart; // Adjusting for zero-based indexing
}

String toHexString(byte *buffer, int bufferSize) {
  // Allocate enough space for the hexadecimal representation (2 characters per byte)
  char bufferStr[bufferSize * 2 + 1];
  bufferStr[0] = '\0'; // Initialize the string

  for (int i = 0; i < bufferSize; i++) {
    sprintf(bufferStr + i * 2, "%02X", buffer[i]); // Format and append each byte
  }

  return String(bufferStr); // Convert to Arduino String and return
}

void printByte(byte *buffer) {
  if (buffer[13] != 0) {
    Serial.print(buffer[13]);
  }
  Serial.print(buffer[14]);
  Serial.print(".");
  if (buffer[15] < 10) {
    Serial.print("0");
  }
  Serial.print(buffer[15]);
}

// For Debugging Purposes:
void printHex(byte *buffer, int bufferSize) {
  for (int i = 0; i < bufferSize; i++) {
    if (buffer[i] < 0x10) {
      Serial.print("0"); // print leading zero for values less than 0x10
    }
    Serial.print(buffer[i], HEX);
  }
}

