/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com  
*********/

// Including the ESP8266 WiFi library
#include <ESP8266WiFi.h>
#include <EEPROM.h>

  // D1, D2, D3, D4, D5 D6 D7 D8 ->  GPIO 5, 4, 0, 2,   14,12,13,15 (esp8266 digitalPins)
  int D[8]={5,4,0,2, 14,12,13,15};

#include "DHT.h"
#include <OneWire.h>

int target_temperature = 0; // control temp:  +1 grad stops heating -1 starts heating
int heater_d = 7; // D for connecting relay to heater control
int heater_reverse = 1; // level for cooling command

OneWire ds(0); //pin4=D2
float ds_celsius;
//OneWire ds(13); //pin15=D8

// Uncomment one of the lines below for whatever DHT sensor type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// Replace with your network details

#include "/mnt/c/prj/const.h"
#ifndef WIFI
#define WIFI
const char* ssid = "..SSID..";
const char* password = "..password..";
#endif

// Web Server on port 80
WiFiServer server(80);

// DHT Sensor
const int DHTPin = 5; // d1

// Initialize DHT sensor.
DHT dht(DHTPin, DHTTYPE);

// Temporary variables
static char celsiusTemp[7];
static char fahrenheitTemp[7];
static char humidityTemp[7];

// only runs once on boot
void setup() {
  // Initializing serial port for debugging purposes
  Serial.begin(115200);
  delay(10);
  EEPROM.begin(1);
  target_temperature = EEPROM.read(0); // first byte is contolled temp
  EEPROM.end();
  Serial.println("TargetTemp="+String(target_temperature));
  
  for(int i=0;i<8;i++) if (D[i]!=DHTPin)  {
    
     pinMode(D[i],OUTPUT);
     digitalWrite(D[i],LOW);
     }

  ctl_heat(0); // set to cooling


  dht.begin();
  read_temp();

  every_second();
  
  // Connecting to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  // Starting the web server
  server.begin();
  Serial.println("Web server running. Waiting for the ESP IP...");
  delay(10000);
  
  // Printing the ESP IP address
  Serial.println(WiFi.localIP());
}

int read_ds() {
    byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return 0;
  }
  
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return 0;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return 0;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  ds_celsius = celsius; // remember
  Serial.print(" Celsius, ");
  Serial.print(fahrenheit);
  Serial.println(" Fahrenheit");
  return 1;
}

void read_temp() {
  read_ds() || read_ds(); // try twice
  
float h = dht.readHumidity();
            // Read temperature as Celsius (the default)
            float t = dht.readTemperature();
            // Read temperature as Fahrenheit (isFahrenheit = true)
            float f = dht.readTemperature(true);
            // Check if any reads failed and exit early (to try again).
            if (isnan(h) || isnan(t) || isnan(f)) {
              Serial.println("Failed to read from DHT sensor!");
              strcpy(celsiusTemp,"Failed");
              strcpy(fahrenheitTemp, "Failed");
              strcpy(humidityTemp, "Failed");         
            }
            else{
              // Computes temperature values in Celsius + Fahrenheit and Humidity
              float hic = dht.computeHeatIndex(t, h, false);       
              dtostrf(hic, 6, 2, celsiusTemp);             
              float hif = dht.computeHeatIndex(f, h);
              dtostrf(hif, 6, 2, fahrenheitTemp);         
              dtostrf(h, 6, 2, humidityTemp);
              // You can delete the following Serial.print's, it's just for debugging purposes
              Serial.print("Humidity: ");
              Serial.print(h);
              Serial.print(" %\t Temperature: ");
              Serial.print(t);
              Serial.print(" *C ");
              Serial.print(f);
              Serial.print(" *F\t Heat index: ");
              Serial.print(hic);
              Serial.print(" *C ");
              Serial.print(hif);
              Serial.print(" *F");
              Serial.print("Humidity: ");
              Serial.print(h);
              Serial.print(" %\t Temperature: ");
              Serial.print(t);
              Serial.print(" *C ");
              Serial.print(f);
              Serial.print(" *F\t Heat index: ");
              Serial.print(hic);
              Serial.print(" *C ");
              Serial.print(hif);
              Serial.println(" *F");
            }  
}

void flash_http200(WiFiClient client) {
 client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
}

void flash_temp(WiFiClient client) {
flash_http200(client);
            // your actual web page that displays temperature and humidity
            client.println("<!DOCTYPE HTML>");
            client.println("<html>");
            client.println("<head></head><body><h1>ESP8266 - Temperature and Humidity</h1><h3>Temperature in Celsius: ");
            client.println(celsiusTemp);
            client.println("*C</h3><h3>Temperature in Fahrenheit: ");
            client.println(fahrenheitTemp);
            client.println("*F</h3><h3>Humidity: ");
            client.println(humidityTemp);
              client.println("%</h3><h3>OnBoard Temp: ");
            client.println(ds_celsius);
            client.println("*C</h3>");
            client.println("<h3>Target temperature:"+String(target_temperature)+"</h3>");
            client.println("</body></html>");     
}

int setD(int pin, int mode) {
  if (pin<1 || pin>8) return 0;
  pin=D[pin-1]; // decode
  pinMode(pin,OUTPUT);
  digitalWrite(pin,mode?HIGH:LOW);
  return 1;
}

void do_onoff(WiFiClient client,int pin, int mode) {
  flash_http200(client);
  if (!setD(pin,mode)) {
     client.println("ERROR USAGE pin 1..8");
     } else { // OK
    client.print("PIN:");  client.println(pin); 
    client.print("MODE:"); client.println(mode);
    }
}


 

char url[80]; int l=0; int line_no=0;
int  reported = 0;


void ctl_heat(int ON) {
  if (!heater_d) return; 
  // where connected relay and what is normal state...?
  if (heater_reverse) ON=!ON; 
  setD(heater_d,(ON?1:0)); // do it on D4 (GPIO2)
  
}

void every_second() {
   
   read_temp(); // do reading temperature
   int temp =  ds_celsius;;
   
   Serial.println("Uptime:"+String(reported)+" seconds,Temp="+String(temp)+" Target="+String(target_temperature));
   
   if (target_temperature) { // if set - need check
      if ( temp>= target_temperature +1) { 
         Serial.println("Need cooling");
         ctl_heat(0);      // do stop
          }
      else if ( temp <= target_temperature-1) {
          Serial.println("Need heat");
          ctl_heat(1);     // do start
          }
      
   }
}


// runs over and over again
void loop() {
int sec = millis()/1000;
if (sec != reported) {
   reported =  sec;
   every_second();
}

  
  // Listenning for new clients
  WiFiClient client = server.available();
  
  if (client) {
    Serial.println("New client");
    url[0]=0; l=0; line_no=0;
    // bolean to locate when the http request ends
    boolean blank_line = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();        
        if (c == '\n' && blank_line) {
            char *u=url;
            if (strncmp(u,"GET ",4)==0) u+=4;
            if (strncmp(u,"/on",3)==0) { // switch on/off
               do_onoff(client,atoi(u+3),1);
            } 
            else if (strncmp(u,"/off",4)==0) { // switch on/off
               do_onoff(client,atoi(u+4),0);
            } 
            else if (strncmp(u,"/temp",5)==0) { // flash just temp
              read_temp();
              flash_http200(client);
              client.println(celsiusTemp);
               // send temp
            }
            else if (strncmp(u,"/target",7)==0) { // set target temp
              target_temperature=atoi(u+7);
          
              EEPROM.begin(1);
              EEPROM.write(0,target_temperature);
              EEPROM.commit();
              EEPROM.end();

              EEPROM.begin(1);
              target_temperature=EEPROM.read(0);
              EEPROM.end();
                  client.println(target_temperature);
              
            }
            else { // default page
            read_temp();
            Serial.print("Flash on URL="); Serial.println(url);
            flash_temp(client);
            }
            break;
        }
        if (c == '\n') {
          blank_line = true; line_no++;
        }
        else if (c != '\r') {
          if ( (line_no == 0) && (l<79) && (c!='\n') && (c!='\r')) { url[l]=c; l++; url[l]=0;} // collect URL
          blank_line = false;
        }
      }
    }  
    // closing the client connection
    delay(1);
    client.stop();
    Serial.println("Client disconnected.");
  }
}   
