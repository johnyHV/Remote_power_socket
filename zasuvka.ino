#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

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
cas time;

char c = 0;                   // citanie  retazcov poziadavky
String readString;            // ukladanie retazcov poziadavky
#define relay  7          // I/O rele
#define led_on  13
#define led_status  8
#define button  9
bool relay_status = false;
bool ethernet_s = false;
bool button_s = false;
//---------------------------------------------------------------------------------

// function
//---------------------------------------------------------------------------------
void get_time(EthernetUDP *Udp,cas *time);
//---------------------------------------------------------------------------------

void setup() {
  Serial.begin(9600);
  Serial.println("Boot");
  
  Serial.println("Getting IP adres wia DHCP");
  //Ethernet.begin(mac, ip, brana, maska);
  if (Ethernet.begin(mac) == 0)
     Serial.println("Failed to configure Ethernet using DHCP");
  Serial.println(Ethernet.localIP());
  
  // Configuration digital I/O
  Serial.println("Configuration pin");
  pinMode(relay,OUTPUT);
  pinMode(led_on,OUTPUT);
  pinMode(led_status,OUTPUT);
  pinMode(button,INPUT);
  digitalWrite(relay,LOW);
  digitalWrite(led_on,HIGH);
  digitalWrite(led_status,LOW);
  button_s = digitalRead(button);
  
  Serial.println("Starting server");
  // Starting NTP,WHTTP server, DHT sensor
  Udp.begin(localPort);
  server.begin();

  Serial.println("Start");
}

void loop() {
  
  if (digitalRead(button) != button_s)
  {
    digitalWrite(relay,!relay_status);
    digitalWrite(led_status,!relay_status);
    relay_status = !relay_status;
    button_s = digitalRead(button);
    Serial.println("Change status button");
    Serial.print("Relay status: ");
    Serial.println(relay_status);
  }
  
  get_time(&Udp,&time);
  EthernetClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        //read char by char HTTP request
        if (readString.length() < 100) {
          //store characters to string
          readString += c;
          Serial.print(c);
        }
        //if HTTP request has ended
        if (c == '\n') {
          Serial.println(readString); //print to serial monitor for debuging

          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
     	  client.println("Refresh: 10");
          client.println();
          client.println("<!DOCTYPE HTML><HTML><HEAD><center>");
          client.println("<TITLE>Home Automation</TITLE>");
          client.println("</HEAD><BODY>");
          client.println("<H1>Vzdialene ovladanie zasuvky</H1>");
          client.println("<hr />");
          client.println("Cas: ");
          client.print(time.hour);
          client.print(":");
          client.print(time.min);
          client.print(":");
          client.print(time.sec);
          client.println("<br/><br/>");
          client.println("Status: ");
          client.println((relay_status ? "Zapnuta" : "Vypnuta"));
          client.println("<hr /><br />");
          client.println((relay_status ? "<a href='http://192.168.7.59?relayoff'><button style='background:red;width:10%;height:40px'>Vypnut</button></a>" : 
          "<a href='http://192.168.7.59?relayon'><button style='background:lightgreen;width:10%;height:40px'>Zapnut</button></a>"));          
          client.println("</center></BODY></HTML>");
          delay(1);
          client.stop();

          if(readString.indexOf("?relayon") >0){
            digitalWrite(led_status, HIGH);    // set pin 4 high
            digitalWrite(relay,HIGH);
            Serial.println("Led On");
            relay_status=true;
          }
          else{
            if(readString.indexOf("?relayoff") >0){
              digitalWrite(led_status, LOW);    // set pin 4 low
              digitalWrite(relay,LOW);
              Serial.println("Led Off");
              relay_status=false;
            }
          }
          //clearing string for next read
          readString="";
          Serial.print(time.hour);
          Serial.print(":");
          Serial.print(time.min);
          Serial.print(":");
          Serial.println(time.sec);
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

     time->hour = (epoch  % 86400L) / 3600 +2;
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
