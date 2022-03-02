#include "AsyncSMS.h"

AsyncSMS smsHelper(&Serial3, 57600);

void messageReceived(char * number, char * message) {
  //Do something with your message
  Serial.println("Message received");
  Serial.println(number);
  Serial.println(message);
}

void setup() {
  smsHelper.init();
  smsHelper.smsReceived = *messageReceived;
}

void loop() {
  smsHelper.process();
}
