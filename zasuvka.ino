#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SD.h>
#include <EEPROM.h>
#include <avr/wdt.h>

// Ethernet config
//---------------------------------------------------------------------------------
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer server(80);
//---------------------------------------------------------------------------------
// http://startingelectronics.com/tutorials/arduino/ethernet-shield-web-server-tutorial/SD-card-gauge/

// NTP config
//---------------------------------------------------------------------------------
IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov NTP server
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
char c = 0; // citanie  retazcov poziadavky
String readString; // ukladanie retazcov poziadavky
#define relay  7          // I/O rele
#define led_on  6
#define button  9
bool relay_status = false;
bool button_s = false;
cas a_time = {22, 25, 0};
cas time;
cas dhcp_time = {0, 0, 0};
uint8_t akcia = 0;
bool stav = false;
File webFile; // the web page file on the SD card
#define e_hour 0
#define e_min 1

//---------------------------------------------------------------------------------

// function
//---------------------------------------------------------------------------------
void get_time(EthernetUDP *Udp, cas *time);
unsigned long sendNTPpacket(IPAddress& address);
void json_response(EthernetClient client);
//---------------------------------------------------------------------------------

void setup() {
    Serial.begin(19200);
    Serial.println(F("start"));
    // disable Ethernet chip
    pinMode(10, OUTPUT);

    digitalWrite(10, HIGH);
    pinMode(53, OUTPUT);
    if (!SD.begin(4)) {
        Serial.println(F("SD initial failed!"));
    } else {
        Serial.println(F("SD initial OK."));
    }

    Ethernet.begin(mac);
    Serial.println(Ethernet.localIP());

    // Configuration digital I/O
    pinMode(relay, OUTPUT);
    pinMode(led_on, OUTPUT);
    pinMode(button, INPUT);
    digitalWrite(relay, HIGH);
    digitalWrite(led_on, HIGH);
    button_s = digitalRead(button);

    // Starting NTP,HTTP server
    Udp.begin(localPort);
    server.begin();
    delay(10000);
    get_time(&Udp, &dhcp_time);
    
    // nacitanie konstant z EEPROM
    a_time.hour = EEPROM.read(e_hour);
    a_time.min = EEPROM.read(e_min);
    
    wdt_enable(WDTO_8S); // reset after one second, if no "pat the dog" received
}

void loop() {
    wdt_reset();
    delay(250);

    // synchronizacia casu
    if (ntp_delay >= 200) {
        get_time(&Udp, &time);
        ntp_delay = 0;
        Serial.print(F("DHCP: "));
        Serial.print(dhcp_time.hour);
        Serial.print(F(":"));
        Serial.println(dhcp_time.min);

        Serial.print(F("Time: "));
        Serial.print(time.hour);
        Serial.print(F(":"));
        Serial.println(time.min);

        if ((time.hour == 0) && (dhcp_time.min == time.min)) {
            Serial.println(Ethernet.maintain());
            dhcp_time.hour = time.hour;
            dhcp_time.min = time.min;
            Serial.println(F("Ziadam IP"));
        } else {
            if (((dhcp_time.hour + 1) == time.hour) && (dhcp_time.min == time.min)) {
                Serial.println(F("Ziadam IP"));
                Serial.println(Ethernet.maintain());
                dhcp_time.hour = time.hour;
                dhcp_time.min = time.min;
            }
        }
    } else
        ntp_delay++;

    // kontrola zmeny stavu tlacidla pre zapnutie rele
    if (digitalRead(button) != button_s) {
        digitalWrite(relay, relay_status);
        relay_status = !relay_status;
        button_s = digitalRead(button);
    }

    // kontrola ci nastal cas definovany na WEB pre zapnutie rele
    if ((time.hour == a_time.hour)&&(time.min == a_time.min)&&(!stav)) {
        Serial.println(F("Cas sa naplnil"));
        stav = true;
        if (akcia == 0) {   // vypni RELE
            digitalWrite(relay, LOW);
            relay_status = true;
        } else {
            if (akcia == 1) { // zapni rele
                digitalWrite(relay, HIGH);
                relay_status = false;
            } else if (akcia == 2) // restartni rele
                digitalWrite(relay, relay_status);
        }
    } else if ((time.hour == a_time.hour)&&(time.min != a_time.min)&&(stav)) {
        if (akcia == 2)
            digitalWrite(relay, !relay_status);
        stav = false;
    }

    // ethernet komunikacia
    EthernetClient client = server.available();
    if (client) {
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();

                if (readString.length() < 100) {
                    readString += c;
                    //Serial.print(c);
                }
                
                //if HTTP request has ended
                if (c == '\n') {

                    if (readString.indexOf(F("?relayon")) > 0) {
                        digitalWrite(relay, LOW);
                        relay_status = true;
                    } else {
                        if (readString.indexOf(F("?relayoff")) > 0) {
                            digitalWrite(relay, HIGH);
                            relay_status = false;
                        } else {
                            if (readString.indexOf(F("?akcia")) > 0) {
                                if (akcia >= 3)
                                    akcia = 0;
                                else
                                    akcia++;
                            } else {
                                if (readString.indexOf(F("?hourinc")) > 0) {
                                    a_time.hour++;
                                    EEPROM.write(e_hour,a_time.hour);
                                } else {
                                    if (readString.indexOf(F("?hourdec")) > 0) {
                                        a_time.hour--;
                                        EEPROM.write(e_hour,a_time.hour);
                                    } else {
                                        if (readString.indexOf(F("?mininc")) > 0) {
                                            a_time.min += 10;
                                            EEPROM.write(e_min,a_time.min);
                                        } else {
                                            if (readString.indexOf(F("?mindec")) > 0) {
                                                a_time.min -= 10;
                                                EEPROM.write(e_min,a_time.min);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    client.println(F("HTTP/1.1 200 OK"));
                    if (readString.indexOf(F("json_input")) > 0) {
                        client.println(F("Content-Type: application/json"));
                        client.println("Connection: close"); 
                        client.println();
                        json_response(client);
                    } else {
                        client.println(F("Content-Type: text/html"));
                        client.println(F("Connection: close")); 
                        client.println();

                        webFile = SD.open("index.htm"); // open web page file
                        if (webFile) {
                            while (webFile.available()) {
                                client.write(webFile.read()); // send web page to client
                            }
                            webFile.close();
                        }
                    }

                    delay(10);
                    client.stop();
                    readString = "";
                }
            }
        }
    }
}

void get_time(EthernetUDP *Udp, cas *time) {
    wdt_reset();
    sendNTPpacket(timeServer); // send an NTP packet to a time server

    // wait to see if a reply is available
    delay(1000);

    if (Udp->parsePacket()) {
        Udp->read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secsSince1900 = highWord << 16 | lowWord;

        const unsigned long seventyYears = 2208988800UL;
        unsigned long epoch = secsSince1900 - seventyYears;

        time->hour = (epoch % 86400L) / 3600 + 2 - 1;
        time->min = (epoch % 3600) / 60;
        time->sec = epoch % 60;
    }
}

unsigned long sendNTPpacket(IPAddress& address) {
    wdt_reset();
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011; // LI, Version, Mode
    packetBuffer[1] = 0; // Stratum, or type of clock
    packetBuffer[2] = 6; // Polling Interval
    packetBuffer[3] = 0xEC; // Peer Clock Precision
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
       
    Udp.beginPacket(address, 123); //NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}

void json_response(EthernetClient client) {
    wdt_reset();
    //cas
    client.print(F("{\"hour\":\""));
    client.print(time.hour);
    client.print(F("\",\"min\":\""));
    client.print(time.min);
    client.print(F("\",\"sec\":\""));
    client.print(time.sec);

    // Akcia
    client.print(F("\",\"akcia\":\""));
    if (akcia == 0)
        client.print(F("Zapnut"));
    else if (akcia == 1)
        client.print(F("Vypnut"));
    else if (akcia == 2)
        client.print(F("Restartovat"));
    else if (akcia == 3)
        client.print(F("Ziadna akcia"));

    // cas vypnutia
    client.print(F("\",\"a_hour\":\""));
    client.print(a_time.hour);
    client.print(F("\",\"a_min\":\""));
    client.print(a_time.min);
    client.print(F("\",\"a_sec\":\""));
    client.print(a_time.sec);

    // stav rele
    client.print(F("\",\"relatko\":\""));
    client.print((relay_status ? F("on") : F("off")));
    client.print(F("\"}"));
}
