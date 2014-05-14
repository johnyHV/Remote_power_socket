#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SD.h>

// Ethernet config
//---------------------------------------------------------------------------------
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetServer server(80);
//---------------------------------------------------------------------------------
// http://startingelectronics.com/tutorials/arduino/ethernet-shield-web-server-tutorial/SD-card-gauge/

// NTP config
//---------------------------------------------------------------------------------
IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov NTP server
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov NTP server
//IPAddress timeServer(194, 160, 23, 2); // time-c.timefreq.bldrdoc.gov NTP server
#define NTP_PACKET_SIZE 48 // NTP time stamp is in the first 48 bytes of the message
#define localPort  8888
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets 
EthernetUDP Udp;
//---------------------------------------------------------------------------------

// variable
//---------------------------------------------------------------------------------
struct cas {
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
};

uint8_t ntp_delay = 0;
char c = 0;               // citanie  retazcov poziadavky
String readString;        // ukladanie retazcov poziadavky
#define relay  7          // I/O rele
#define led_on  6
#define button  9
bool relay_status = false;
bool button_s = false;
cas a_time = {23,0,0};
cas time;
cas dhcp_time = {0,0,0};
uint8_t akcia = 0;
bool stav = false;
File webFile;               // the web page file on the SD card

//---------------------------------------------------------------------------------

// function
//---------------------------------------------------------------------------------
void get_time(EthernetUDP *Udp,cas *time);
unsigned long sendNTPpacket(IPAddress& address);
void json_response(EthernetClient client);
//---------------------------------------------------------------------------------

void setup() {
  Serial.begin(9600);
  //Serial.println("start");
  // disable Ethernet chip
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  SD.begin(4);
 /* Serial.println("Memmory card");
  if (!SD.begin(4)) 
    Serial.println("Failed initialize SD card"); */
  
  //Serial.println("Getting IP adres wia DHCP");
  //Ethernet.begin(mac, ip, brana, maska);
//  if (Ethernet.begin(mac) == 0)
//     Serial.println("Failed to configure Ethernet using DHCP");
     
  Ethernet.begin(mac);
  Serial.println(Ethernet.localIP());
  get_time(&Udp,&dhcp_time);
  
  
  // Configuration digital I/O
  pinMode(relay,OUTPUT);
  pinMode(led_on,OUTPUT);
  pinMode(button,INPUT);
  digitalWrite(relay,HIGH);
  digitalWrite(led_on,HIGH);
  button_s = digitalRead(button);

  // Starting NTP,HTTP server
  Udp.begin(localPort);
  server.begin();
}

void loop() {
  delay(100);
  
  if ((dhcp_time.hour == time.hour-1) && (dhcp_time.min == time.min)){
    Ethernet.localIP();
    get_time(&Udp,&dhcp_time);
  }
  
  if (ntp_delay >= 150) {
    get_time(&Udp,&time);
    ntp_delay=0;
  }
  else
    ntp_delay++;
/*   
  Serial.print(time.hour);
  Serial.print(":");
  Serial.print(time.min);
  Serial.print(":");
  Serial.println(time.sec);
  Serial.println("Slucka");
  Serial.println(ntp_delay);
*/
  if (digitalRead(button) != button_s)
  {
    digitalWrite(relay,relay_status);
    relay_status = !relay_status;
    button_s = digitalRead(button);
  }
  
  if ((time.hour == a_time.hour)&&(time.min == a_time.min)&&(!stav)){
    //Serial.println("Cas sa naplnil");
    stav = true;
    if (akcia == 0)
      digitalWrite(relay,LOW);
    else if (akcia == 1)
      digitalWrite(relay,HIGH);
    else if (akcia == 2)
      digitalWrite(relay,relay_status);
  }
  else if ((time.hour == a_time.hour)&&(time.min != a_time.min)&&(stav)) {
    if (akcia == 2) 
      digitalWrite(relay,!relay_status);
    stav = false;
  }
    

  EthernetClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        //read char by char HTTP request
        if (readString.length() < 100) {
          readString += c;
          //Serial.print(c);
        }
        //if HTTP request has ended
        if (c == '\n') {

          if(readString.indexOf("?relayon") >0){
            digitalWrite(relay,LOW);
            relay_status=true;
          }
          else { 
            if(readString.indexOf("?relayoff") >0){
              digitalWrite(relay,HIGH);
              relay_status=false;
            }
            else{ 
              if (readString.indexOf("?akcia") >0){
                if (akcia >=3)
                  akcia =0;
                else
                  akcia++;
              }
              else{ 
                if (readString.indexOf("?hourinc") >0){
                  a_time.hour++;
                }
                else {
                  if (readString.indexOf("?hourdec") >0){
                    a_time.hour--;
                  }
                  else {
                    if (readString.indexOf("?mininc") >0){
                      a_time.min+=10;
                    }
                    else {
                      if (readString.indexOf("?mindec") >0){
                        a_time.min-=10;
                      }
                    }
                  } 
                }
              }
            }
          }
          
          client.println("HTTP/1.1 200 OK");
          if(readString.indexOf("json_input") >0)
          {
            client.println("Content-Type: application/json");
            client.println("Connection: close");  // the connection will be closed after completion of the response
            client.println();
            json_response(client);
          }
          else {
            client.println("Content-Type: text/html");
            client.println("Connection: close");  // the connection will be closed after completion of the response
            client.println();
          
            webFile = SD.open("index.htm");        // open web page file
            if (webFile) {
              while(webFile.available()) {
              client.write(webFile.read()); // send web page to client
              }
            webFile.close();
            }
          }

            
          delay(10);
          client.stop();
          //clearing string for next read
          readString="";
        }
      }
    }
  }
}

void get_time(EthernetUDP *Udp,cas *time) {
   sendNTPpacket(timeServer); // send an NTP packet to a time server

     // wait to see if a reply is available
   delay(1000);  

   if ( Udp->parsePacket() ) {  
     // We've received a packet, read the data from it
     Udp->read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

     //the timestamp starts at byte 40 of the received packet and is four bytes,
     // or two words, long. First, esxtract the two words:

     unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
     unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
     // combine the four bytes (two words) into a long integer
     // this is NTP time (seconds since Jan 1 1900):
     unsigned long secsSince1900 = highWord << 16 | lowWord;  
     //Serial.print("Seconds since Jan 1 1900 = " );
     //Serial.println(secsSince1900);               

     // now convert NTP time into everyday time:
     // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
     const unsigned long seventyYears = 2208988800UL;     
     // subtract seventy years:
     unsigned long epoch = secsSince1900 - seventyYears;  
     // print Unix time:

     time->hour = (epoch  % 86400L) / 3600 +2 -1;
     time->min = (epoch  % 3600) / 60;
     time->sec = epoch %60;
   }
   // wait ten seconds before asking for the time again
   //delay(10000);
}

unsigned long sendNTPpacket(IPAddress& address){
   // set all bytes in the buffer to 0
   memset(packetBuffer, 0, NTP_PACKET_SIZE); 
   // Initialize values needed to form NTP request
   // (see URL above for details on the packets)
   packetBuffer[0] = 0b11100011;   // LI, Version, Mode
   packetBuffer[1] = 0;     // Stratum, or type of clock
   packetBuffer[2] = 6;     // Polling Interval
   packetBuffer[3] = 0xEC;  // Peer Clock Precision
   // 8 bytes of zero for Root Delay & Root Dispersion
   packetBuffer[12]  = 49; 
   packetBuffer[13]  = 0x4E;
   packetBuffer[14]  = 49;
   packetBuffer[15]  = 52;

   // all NTP fields have been given values, now
   // you can send a packet requesting a timestamp:         
   Udp.beginPacket(address, 123); //NTP requests are to port 123
   Udp.write(packetBuffer,NTP_PACKET_SIZE);
   Udp.endPacket(); 
}

void json_response(EthernetClient client)
{
  //cas
  client.print("{\"hour\":\"");
  client.print(time.hour);
  client.print("\",\"min\":\"");
  client.print(time.min);
  client.print("\",\"sec\":\"");
  client.print(time.sec);

  // Akcia
  client.print("\",\"akcia\":\"");
  if (akcia == 0)
    client.print("Zapnut");
  else if (akcia == 1)
    client.print("Vypnut");
  else if (akcia == 2)
    client.print("Restartovat");
  else if (akcia == 3)
    client.print("Ziadna akcia");
    
  // cas vypnutia
  client.print("\",\"a_hour\":\"");
  client.print(a_time.hour);
  client.print("\",\"a_min\":\"");
  client.print(a_time.min);
  client.print("\",\"a_sec\":\"");
  client.print(a_time.sec);         
      
  // stav rele
  client.print("\",\"relatko\":\"");
  client.print((relay_status ? "on" : "off"));          
  client.print("\"}");
}
