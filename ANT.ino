// kindly provided by:http://digitalhacksblog.blogspot.com/2012/10/heart-rate-monitor-project-phase-1-ant.html
// Adapted for use with the Arduino DUE
// From antmessage.h

#define POUTbit0  38
#define POUTbit1  40
#define POUTbit2  42
#define POUTbit3  44
#define POUTbit4  46
#define POUTbit5  48
#define POUTbit6  50
#define POUTbit7  52

#define UCHAR unsigned char
 
#define MESG_TX_SYNC                      ((UCHAR)0xA4)
#define MESG_ASSIGN_CHANNEL_ID            ((UCHAR)0x42)
#define MESG_CHANNEL_MESG_PERIOD_ID       ((UCHAR)0x43)
#define MESG_CHANNEL_SEARCH_TIMEOUT_ID    ((UCHAR)0x44)
#define MESG_CHANNEL_RADIO_FREQ_ID        ((UCHAR)0x45)
#define MESG_NETWORK_KEY_ID               ((UCHAR)0x46)
#define MESG_SYSTEM_RESET_ID              ((UCHAR)0x4A)
#define MESG_OPEN_CHANNEL_ID              ((UCHAR)0x4B)
#define MESG_CHANNEL_ID_ID                ((UCHAR)0x51)
 
#define MESG_RESPONSE_EVENT_ID            ((UCHAR)0x40)
#define MESG_BROADCAST_DATA_ID            ((UCHAR)0x4E)
#define MESG_CAPABILITIES_ID              ((UCHAR)0x54)
 
//#define DEBUG 1
 
// For Garmin HRM
 
#define ANT_CHAN           0
#define ANT_NET            0    // public network
#define ANT_TIMEOUT       12    // 12 * 2.5 = 30 seconds
#define ANT_DEVICETYPE   120    // bit 7 = 0 pairing requiest bits 6..0 = 120 for HRM
#define ANT_FREQ          57    // Garmin radio frequency
#define ANT_PERIOD      8070    // Garmin search period
#define ANT_NETWORKKEY {0xb9, 0xa5, 0x21, 0xfb, 0xbd, 0x72, 0xc3, 0x45}
 
// For Software Serial
 
#define ANT_CTS             12
#define ANT_TXD              8
#define ANT_RXD              7
#define ANT_BAUD          4800 // Baud for ANT chip
 
#define PACKETREADTIMEOUT  100
#define MAXPACKETLEN        80
 
// it appears that a Garmin cadence sensor uses pretty much the same values as above except it is a deviceType = 121
// the tranmission period is supposedly 4.05 MHz or period = 8085
 
// Global Variables
 
int rxBufCnt = 0;
unsigned char rxBuf[MAXPACKETLEN];
unsigned char oldHeartRate = 0;
unsigned char antNetKey[] = ANT_NETWORKKEY;
 
long packetCount = 0;
 
enum {
  errDefault,
  errPacketSizeExceeded,
  errChecksumError,
  errMissingSync
};
 
void errorHandler(int errIn) {
#ifdef DEBUG
  Serial.println();
  Serial.print("Error: ");
  Serial.println(errIn);
#endif
  //while (true) {};
}
 
unsigned char writeByte(unsigned char out, unsigned char chksum) { 
#ifdef DEBUG
  Serial.print(out, HEX);
  Serial.print(" ");
#endif 
  Serial1.write(out);
  chksum ^= out;
  return chksum;
}
 
void sendPacket(unsigned msgId, unsigned char argCnt, ...) {
  va_list arg;
  va_start (arg, argCnt);
  unsigned char byteOut;
  unsigned char chksum = 0;
  int cnt = 0;
 
#ifdef DEBUG
  Serial.print("TX: ");
#endif
 
  chksum = writeByte(MESG_TX_SYNC, chksum); // send sync
  chksum = writeByte(argCnt, chksum);       // send length
  chksum = writeByte(msgId, chksum);        // send message id
   
  // send data
  for (cnt=1; cnt <= argCnt; cnt++) {
    byteOut = va_arg(arg, unsigned int);
    chksum = writeByte(byteOut, chksum);
  }
  va_end(arg);
   
  writeByte(chksum,chksum);                 // send checksum  
#ifdef DEBUG
  Serial.println();
#endif
}
 
void printPacket(unsigned char * packet) {
  int cnt = 0;
  while (cnt < packet[1]+4) {
    Serial.print(packet[cnt++], HEX);
    Serial.print  (" ");
  }
  Serial.println();
}
 
int readPacket(unsigned char *packet, int packetSize, int readTimeout) {
  unsigned char byteIn;
  unsigned char chksum = 0;
   
  long timeoutExit = millis() + readTimeout;
 
  while (timeoutExit > millis()) {
    if (Serial1.available() > 0) {
      byteIn = Serial1.read();
      timeoutExit = millis() + readTimeout;
      if ((byteIn == MESG_TX_SYNC) && (rxBufCnt == 0)) {
        rxBuf[rxBufCnt++] = byteIn;
        chksum = byteIn;
      } else if ((rxBufCnt == 0) && (byteIn != MESG_TX_SYNC)) {
        errorHandler(errMissingSync);
        return -1;
      } else if (rxBufCnt == 1) {
        rxBuf[rxBufCnt++] = byteIn;       // second byte will be size
        chksum ^= byteIn;
      } else if (rxBufCnt < rxBuf[1]+3) { // read rest of data taking into account sync, size, and checksum that are each 1 byte
        rxBuf[rxBufCnt++] = byteIn;
        chksum ^= byteIn;
      } else {
        rxBuf[rxBufCnt++] = byteIn;
        if (rxBufCnt > packetSize) {
          errorHandler(errPacketSizeExceeded);
          return -1;
        } else {
          memcpy(packet, &rxBuf, rxBufCnt); // should be a complete packet. copy data to packet variable, check checksum and return
          packetCount++;
          if (chksum != packet[rxBufCnt-1]) {
            errorHandler(errChecksumError);
            rxBufCnt = 0;
            return -1;
          } else {
            rxBufCnt = 0;
            return 1;
          }
        }
      }
    }
  }
  return 0;
}
 
int checkReturn() {
  byte packet[MAXPACKETLEN];
  int packetsRead;
 
  packetsRead = readPacket(packet, MAXPACKETLEN, PACKETREADTIMEOUT);
 
  // Data <sync> <len> <msg id> <channel> <msg id being responded to> <msg code> <chksum>
  // <sync> always 0xa4
  // <msg id> always 0x40 denoting a channel response / event
  // <msg code? success is 0.  See page 84 of ANT MPaU for other codes
 
  if (packetsRead > 0) {
    Serial.print("RX: ");
    printPacket(packet);
  }
 
  return packetsRead;
}
 
void setup() {
  Serial.begin(9600);
  Serial1.begin(4800);
  Serial.println("Starting...");
 
  pinMode(OUTPUT, POUTbit0);
  pinMode(OUTPUT, POUTbit1);
  pinMode(OUTPUT, POUTbit2);
  pinMode(OUTPUT, POUTbit3);
  pinMode(OUTPUT, POUTbit4);
  pinMode(OUTPUT, POUTbit5);
  pinMode(OUTPUT, POUTbit6);
  pinMode(OUTPUT, POUTbit7);

  pinMode(INPUT, ANT_CTS);
  pinMode(ANT_RXD, INPUT);
  pinMode(ANT_TXD, OUTPUT);
 
  Serial1.begin(ANT_BAUD);
   
  Serial.println("Config Starting");
 
  // Reset
  sendPacket(MESG_SYSTEM_RESET_ID, 1, 0);
  delay(600);
   
  // Flush read buffer
  while (Serial1.available() > 0) {
    Serial1.read();
  }
   
  // Assign Channel
  //   Channel: 0
  //   Channel Type: for Receive Channel
  //   Network Number: 0 for Public Network
  sendPacket(MESG_ASSIGN_CHANNEL_ID, 3, ANT_CHAN, 0, ANT_NET);  
  if (checkReturn() == 0) errorHandler(errDefault);
   
  // Set Channel ID
  //   Channel Number: 0
  //   Device Number LSB: 0 for a slave to match any device
  //   Device Number MSB: 0 for a slave to match any device
  //   Device Type: bit 7 0 for pairing request bit 6..0 for device type
  //   Transmission Type: 0 to match any transmission type
  sendPacket(MESG_CHANNEL_ID_ID, 5, ANT_CHAN, 0, 0, ANT_DEVICETYPE, 0);
  if (checkReturn() == 0) errorHandler(errDefault);
 
  // Set Network Key
  //   Network Number
  //   Key
  sendPacket(MESG_NETWORK_KEY_ID, 9, ANT_NET, antNetKey[0], antNetKey[1], antNetKey[2], antNetKey[3], antNetKey[4], antNetKey[5], antNetKey[6], antNetKey[7]);
  if (checkReturn() == 0) errorHandler(errDefault);
 
  // Set Channel Search Timeout
  //   Channel
  //   Timeout: time for timeout in 2.5 sec increments
  sendPacket(MESG_CHANNEL_SEARCH_TIMEOUT_ID, 2, ANT_CHAN, ANT_TIMEOUT);
  if (checkReturn() == 0) errorHandler(errDefault);
 
  //ANT_send(1+2, MESG_CHANNEL_RADIO_FREQ_ID, CHAN0, FREQ);
  // Set Channel RF Frequency
  //   Channel
  //   Frequency = 2400 MHz + (FREQ * 1 MHz) (See page 59 of ANT MPaU) 0x39 = 2457 MHz
  sendPacket(MESG_CHANNEL_RADIO_FREQ_ID, 2, ANT_CHAN, ANT_FREQ);
  if (checkReturn() == 0) errorHandler(errDefault);
 
  // Set Channel Period
  sendPacket(MESG_CHANNEL_MESG_PERIOD_ID, 3, ANT_CHAN, (ANT_PERIOD & 0x00FF), ((ANT_PERIOD & 0xFF00) >> 8));
  if (checkReturn() == 0) errorHandler(errDefault);
 
  //Open Channel
  sendPacket(MESG_OPEN_CHANNEL_ID, 1, ANT_CHAN);
  if (checkReturn() == 0) errorHandler(errDefault);
 
  Serial.println("Config Done");
}
 
void printHeader(const char * title) {
  Serial.print(millis());
  Serial.print(" ");
  Serial.print(packetCount);
  Serial.print(" - ");
  Serial.print(title);
}
 
void loop() {
  byte packet[MAXPACKETLEN];
  int packetsRead;
  unsigned char msgId, msgSize;
  unsigned char *msgData;
 
  packetsRead = readPacket(packet, MAXPACKETLEN, PACKETREADTIMEOUT);
  if (packetsRead > 0) {
    msgId = packet[2];
    msgSize = packet[1];
    msgData = &packet[3];
     
    switch (msgId) {
      case MESG_RESPONSE_EVENT_ID:
        printHeader("MESG_RESPONSE_EVENT_ID: ");
        printPacket(packet);
        break;
   
      case MESG_CAPABILITIES_ID:
        printHeader("MESG_CAPABILITIES_ID: ");
        printPacket(packet);
        break;
   
      case MESG_BROADCAST_DATA_ID:
        if (oldHeartRate != msgData[msgSize-1]) {
          oldHeartRate = msgData[msgSize-1];
          printHeader("New Heart Rate: ");
          Serial.println(oldHeartRate);
          digitalWrite(POUTbit0, bitRead(oldHeartRate, 0));
          digitalWrite(POUTbit1, bitRead(oldHeartRate, 1));
          digitalWrite(POUTbit2, bitRead(oldHeartRate, 2));
          digitalWrite(POUTbit3, bitRead(oldHeartRate, 3));
          digitalWrite(POUTbit4, bitRead(oldHeartRate, 4));
          digitalWrite(POUTbit5, bitRead(oldHeartRate, 5));
          digitalWrite(POUTbit6, bitRead(oldHeartRate, 6));
          digitalWrite(POUTbit7, bitRead(oldHeartRate, 7));
        }
        break;
   
      default:
        printHeader("MESG_ID_UKNOWN: ");
        printPacket(packet);
        break;
    }
  }
}
