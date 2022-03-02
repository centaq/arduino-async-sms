#include "AsyncSMS.h"

AsyncSMS smsHelper(&Serial3, 57600);

void setup() {
  smsHelper.init();
  smsHelper.send("+48123456789", "This is my Arduino driven SMS module working", 45);
}

void loop() {
  smsHelper.process();
}
