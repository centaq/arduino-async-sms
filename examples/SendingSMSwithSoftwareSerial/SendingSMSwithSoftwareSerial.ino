#include "AsyncSMS.h"
#include <SoftwareSerial.h>

#define RX 1
#define TX 2

SoftwareSerial SerialGSM(RX, TX);
AsyncSMS smsHelper(&SerialGSM);

void setup() {
  SerialGSM.begin(57600);
  smsHelper.init();
  smsHelper.send("+48123456789", "This is my Arduino driven SMS module working", 45);
}

void loop() {
  smsHelper.process();
}
