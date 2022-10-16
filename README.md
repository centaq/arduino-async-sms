
# arduino-async-sms
Simple Arduino Async Library for sending sms using SIM800L

This is a library allowing sending and receiving SMS messages in asynchronous way.
Is has its limitations but proved to be faster than when using fixed delays and allows your application to do anything else in the meantime.

## Requirements
1. Timers - https://github.com/nettigo/Timers

## Examples
### Simple receiver

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
As you can see, you need to specify HardwareSerial interface and baud rate for it.
Of cource `process` function in main loop has to be placed there as it is responsible for background processing.

### Sending message

    #include "AsyncSMS.h"
    
    AsyncSMS smsHelper(&Serial3, 57600);
    
    void setup() {
      smsHelper.init();
      smsHelper.send("+48123456789", "This is my Arduino driven SMS module working", 45);
    }
    
    void loop() {
      smsHelper.process();
    }
As above, including `process` function in main loop is essential, as this is a asynchronous library.
As you can also see, you have to specify message length while sending sms.


### Sending message using SoftwareSerial

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
Above example shows how SoftwareSerial can be use with asynchronous sms library.
The major difference is with the fact that software serial is initialize outside library native code.

### Full example

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

Both above examples combined together with addition of `logger` which is mainly for logging some of the processing steps and debbuging (if enabled in header file)

## Limitations
There are few know limitations that might be (or might not - depending on your working scenario) blocking and will require changes in library code.
1. The sender/receiver number is limited to 13 chars long (including null at the end) and country id number like '+48' but can be easily changed in header file.
2. You can queue up to 16 cmds (which is more internal lib thing), including 4 for sending SMS
3. Outgoing SMS max length is set to be 160 chars long
4. Incoming SMS max length is set to be 200 chars long
5. Library is using HardwareSerial
6. Resulting from above, memory allocation for the library is quite large. Approximately 4x160 (outgoing queue) + 200 (incoming queue) + some memory reservation for cmd Queue ~160 chars which sum up to almost 1 kB. Of course it can be reduced by reducing above numbers in header file
7. Library is used and developed using Arduino mega2560 and wasn't tested elsewhere
8. Code is developed using SIM800L module
