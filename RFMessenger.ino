#include <SPI.h>
#include <RH_RF95.h>
#include "Adafruit_FRAM_SPI.h"
#include "RTClib.h"
//#include <SD.h> //For later :)

//Packet struct
typedef struct packet_t {
  uint8_t callsign[6];
  uint8_t pkt_type;
  uint32_t utc_stamp;
  uint8_t data[240];
} packet_t;

//Define time
#define RF95_CS 10
#define RF95_INT 2
#define RF95_RST 8
#define RF95_FREQ 430.50

#define FRAM_CS 9

#define LED_ERROR 3
#define LED_RX 5
#define LED_TX 6

#define MESSAGE_TEXT 0
#define MESSAGE_FILE_PREAMBLE 1
#define MESSAGE_FILE_DATA 2
#define MESSAGE_FILE_END 3
#define STATUS_ACK 4
#define STATUS_INVALID_PACKET 5

RTC_PCF8523 rtc;
RH_RF95 rf95(RF95_CS, RF95_INT);
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(FRAM_CS);

//Some perhaps important global variables
uint8_t lastCommandSent = 0;
uint8_t lastCommandGot = 0;

void displayTime() { //Prints out our time nice and pretty
  DateTime now = rtc.now();
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print('/');
  Serial.print(now.year(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  if(now.minute() < 10) {
    Serial.print("0");
  }
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  if(now.second() < 10) {
    Serial.print("0");
  }
  Serial.print(now.second(), DEC);
  Serial.println();
}

void(* reset) (void) = 0; //software reset call

//Simply adds callsign to each packet. Abstraction!
void callsignAppend(packet_t *packet) {
  packet->callsign[0] = 'K'; packet->callsign[1] = 'N'; packet->callsign[2] = '4'; packet->callsign[3] = 'G'; packet->callsign[4] = 'P'; packet->callsign[5] = 'O';
  return packet;
}

void dumpPacket(uint8_t *data, int message_len) {
  for(int x = 0; x < 6; x++) {
    Serial.print((packet_t *)data->callsign[x]);
  }
  Serial.print('\n');
  Serial.println(data->pkt_type);
  Serial.println(data->utc_stamp);
  for(int x = 0; x < message_len; x++) {
    Serial.print((char)data->data[x]); //Serial.print(' ');
  }
  Serial.println();
}

void radioReset() {
  digitalWrite(RF95_RST, LOW);
  delay(50);
  digitalWrite(RF95_RST, HIGH);
}

//Takes data through serial port, sends packets.
/*bool fileSender(char *filename) {
  if(sizeof(filename) > 240) { //Our filename is too long. Why would you even have this long of a filename?
    Serial.println("File is too large.");
    return false;
  }
  packet_t preamble;
  preamble = callsignAppend(preamble);
  preamble.pkt_type = MESSAGE_FILE_PREAMBLE;
  preamble.utc_stamp = DateTime().unixtime();
  for(int x = 0; x < sizeof(filename); x++) { //copy filename to data area
    preamble.data[x] = filename[x];
  }
  //Send preamble packet Wait for ACK.
  rf95.send((uint8_t *)&preamble, sizeof(packet_t));
  if(rf95.available()) {
    packet_t potential_ack;
    uint8_t chungis;
    if(rf95.recv((uint8_t *)&potential_ack, &chungis)) {
      if(potential_ack.pkt_type != STATUS_ACK) {
        //We got a packet but it isn't an ACK. Respond with invalid packet type?
        packet_t invalid;
        invalid = callsignAppend(invalid);
        invalid.pkt_type = STATUS_INVALID_PACKET;
        invalid.utc_stamp = DateTime().unixtime();
        rf95.send((uint8_t *)&invalid, sizeof(invalid));
      } else { //we send packets in a loop or something? I want to optimize this part
        
      }
    }
  }
}*/

void sendText() {
  Serial.println(F("Enter message:"));
  while(Serial.available() < 1) {
    //Do nothing until we have something on serial.
    //Loop should break when we have something available on serial.
  }
  if(Serial.available() > 0) {
    /*String cheg = Serial.readString();
    Serial.print(F("length of cheg is ")); Serial.println(cheg.length());
    Serial.print(F("String is: ")); Serial.println(cheg);
    byte leg[240];*/
    //cheg.getBytes(leg, cheg.length());
    uint8_t leg[240];
    int reader = Serial.readBytes(leg, sizeof(leg));
    Serial.print(F("We read in this many: ")); Serial.println(reader);
    digitalWrite(LED_ERROR, LOW);
    digitalWrite(LED_RX, LOW);
    digitalWrite(LED_TX, HIGH);
    packet_t *packet = NULL;
    packet = (packet_t *)malloc(sizeof(packet_t));
    callsignAppend(packet);
    packet->pkt_type = MESSAGE_TEXT;
    packet->utc_stamp = rtc.now().unixtime();
    memcpy(packet->data, leg, sizeof(leg));
    //Properly terminate string. WHY DOESN'T THIS WORK
    packet->data[reader] = '\0';
    Serial.println("Dumping packet.");
    //debug function: dump packet data to verify it actually works
    dumpPacket((uint8_t *)&packet, reader);
    //We need to calculate how many bytes to NOT send. Prevent overflow and all that.
    uint8_t sendlength = sizeof(packet_t) - (240 - reader);
    if(rf95.send((uint8_t *)&packet, sendlength)) {
      lastCommandSent = MESSAGE_TEXT;
      Serial.println("Message sent.");
      free(packet);
    }
  }
  digitalWrite(LED_ERROR, LOW);
  digitalWrite(LED_TX, HIGH);
  digitalWrite(LED_RX, HIGH);
}

void setup() {
  pinMode(LED_ERROR, OUTPUT);
  pinMode(LED_TX, OUTPUT);
  pinMode(LED_RX, OUTPUT);
  digitalWrite(LED_TX, LOW);
  digitalWrite(LED_RX, LOW);
  digitalWrite(LED_ERROR, LOW);
  pinMode(RF95_RST, OUTPUT);
  digitalWrite(RF95_RST, HIGH); //Prevent perpetual radio bootloop
  Serial.begin(9600); //Default port rate. Switchable when sending files.
  if(rtc.begin()) {
    Serial.println(F("RTC active."));
    Serial.print(F("Time: ")); displayTime();
  } else {
    Serial.println(F("RTC init failure. Reset board."));
    digitalWrite(LED_ERROR, HIGH);
    for(;;);
  }
  if(fram.begin()) {
    Serial.println(F("FRAM active."));
  } else {
    Serial.println(F("FRAM not found. Reset board."));
    digitalWrite(LED_ERROR, HIGH);
    for(;;);
  }
  if(rf95.init()) {
    Serial.println(F("Radio active."));
  } else {
    Serial.println(F("Radio init failure. Reset board."));
    digitalWrite(LED_ERROR, HIGH);
    for(;;);
  }
  //Once we reach this point everything is alive. Start setting radio up.
  if(rf95.setFrequency(RF95_FREQ)) {
    Serial.print(F("Frequency set to ")); Serial.println(RF95_FREQ);
    rf95.setTxPower(23, false);
    rf95.setSignalBandwidth(62500); //Default is 125KHz, but I think max on 70cm is 100KHz
  } else {
    Serial.println(F("Invalid frequency. Halt."));
    for(;;);
  }
  Serial.println(F("Startup complete. Waiting for commands."));
  digitalWrite(LED_TX, HIGH);
  digitalWrite(LED_RX, HIGH);
}

void loop() {
  if(Serial.available() > 0) {
    char command;
    Serial.readBytes(&command, 1);
    while(Serial.available() > 0) {
      Serial.println(F("Do we even get here?"));
      Serial.read(); //Empty out all bytes except command byte
    }
    switch(command) {
      case '0':
        sendText();
        break;
      case '1':
        Serial.println(F("File sending not yet supported."));
        break;
      case '2':
        Serial.println(F("Going down for reset."));
        delay(1000);
        reset();
        break;
      case '3':
        Serial.println(F("Board hanging self. Hit reset button."));
        digitalWrite(LED_TX, LOW);
        digitalWrite(LED_RX, LOW);
        digitalWrite(LED_ERROR, HIGH);
        for(;;);
        break;
      default:
        Serial.println(F("Invalid command."));
        break;
    }
  }
  if(rf95.available()) {
    digitalWrite(LED_RX, HIGH);
    digitalWrite(LED_TX, LOW);
    packet_t *new_packet = NULL;
    new_packet = (packet_t *)malloc(sizeof(packet_t));
    uint8_t len = sizeof(packet_t);
    if(rf95.recv((uint8_t *)new_packet, &len)) {
      RH_RF95::printBuffer("Received: ", (uint8_t *)new_packet, len);
      Serial.println(F("Packet received! Here's what's in it."));
      dumpPacket(new_packet, len);
      Serial.print(F("Signal strength at: ")); Serial.println(rf95.lastRssi(), DEC);
      free(new_packet);
      digitalWrite(LED_TX, HIGH);
    }
  }
}
