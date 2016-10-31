/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com  
*********/

// Including the ESP8266 WiFi library
#include <ESP8266WiFi.h>

  // D1, D2, D3, D4, ->  GPIO 5, 4, 0, 2  (esp8266 digitalPins)
  int D[4]={5,4,0,2};

#include "DHT.h"

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

  for(int i=0;i<4;i++) {
     pinMode(D[i],OUTPUT);
     digitalWrite(D[i],LOW);
     }

  dht.begin();
  
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


void read_temp() {
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
            client.println("%</h3><h3>");
            client.println("</body></html>");     
}

void do_onoff(WiFiClient client,int pin, int mode) {

  flash_http200(client);
  if (pin<1 || pin>4) {
     client.println("ERROR USAGE pin 1..4");
     
    } else { // OK
      pin=D[pin-1]; // decode
  pinMode(pin,OUTPUT);
  digitalWrite(pin,mode?HIGH:LOW);
  client.print("PIN:");  client.println(pin); 
  client.print("MODE:"); client.println(mode);
    }
}

 

char url[80]; int l=0; int line_no=0;

// runs over and over again
void loop() {
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
