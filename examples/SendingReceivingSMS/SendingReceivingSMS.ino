#include "AsyncSMS.h"

AsyncSMS smsHelper(&Serial3, 57600);
bool sendSingleSMS;

void messageReceived(char * number, char * message) {
  //Do something with your message
  Serial.println("Message received");
  Serial.println(number);
  Serial.println(message);
}

void processingLogger(char *msg) {
  Serial.println(msg);
}

void setup() {
  smsHelper.init();
  smsHelper.smsReceived = *messageReceived;
  smsHelper.logger = *processingLogger;
  sendSingleSMS = true;
}

void loop() {
  smsHelper.process();
  if (sendSingleSMS) { 
    smsHelper.send("+48123456789", "This is my Arduino driven SMS module working", 45);
    sendSingleSMS = false;
  }
}
