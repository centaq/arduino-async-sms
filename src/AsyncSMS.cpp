#include "AsyncSMS.h"

AsyncSMS::AsyncSMS(Stream *gsm, uint32_t baudRate, bool autoStateRefresh, bool withSerialInit) {
	_gsm = gsm;
	_baudRate = baudRate;
	_autoStateRefresh = autoStateRefresh;
	_withSerialInit = withSerialInit;
}

void AsyncSMS::init() {
	if (_withSerialInit && _hardwareSerial != NULL) {
		_hardwareSerial->begin(_baudRate);
	}
	while(!_gsm) {}
	
	reinitGSMModuleConnection();
	#ifdef ASYNC_SMS_DEBUG_MODE
	enqueue("AT+CPIN?");
	enqueue("AT+GSN=?");
	enqueue("AT+GSN");
	enqueue("AT+CCID");
	enqueue("AT+CNUM=?");
	enqueue("AT+CNUM");
	enqueue("AT+COPS=?");
	#endif
}

void AsyncSMS::reinitGSMModuleConnection() {
	_stateRefreshTimer.stop();
	enqueue("AT");
	enqueue("AT+CMGF=1");
	enqueue("AT+CNMI=1,2,0,0,0");
	enqueue("AT+CREG=1");
	enqueue("ATE0");
	if (_autoStateRefresh) {
		_stateRefreshTimer.start(5000);
	}
	_registrationErrorsCount = 0;
}

void AsyncSMS::send(String number, char *message, uint8_t len) {
	enqueueWithoutNewLine("SMS");
		
	uint8_t maxLen = len;
	if (maxLen > SMS_SEND_MSG_LEN - 1) {
		maxLen = SMS_SEND_MSG_LEN - 1;
	}
	SmsQueueItem item;
	strncpy(item.number, number.c_str(), PHONE_NUMBER_LEN);
	strncpy(item.message, message, maxLen);
	item.message[maxLen] = '\0';
	
	_smsSendQueue[_smsSendQueueEndIndex] = item;
	
	_smsSendQueueEndIndex++;
	if (_smsSendQueueEndIndex == SMS_SEND_QUEUE_LENGTH)
		_smsSendQueueEndIndex = 0;
}

void AsyncSMS::process() {
	if (!_receiving && !_waitingForResponse) {
		if (queueAvailable()) {
			_waitingForResponse = true;
			_waitingForResponseTimer.start(15000);
			
			String cmd = dequeue();
			
			#ifdef ASYNC_SMS_DEBUG_MODE
			Serial.println("deque occurred");
			Serial.println(cmd);
			Serial.println(_cmdQueueStart);
			#endif
			if (cmd == "SMS") {
				_sendingStage = SMSSendingStageEnum::Starting;
				_gsm->write("AT+CMGF=1\n");
			} else {
				_gsm->write(cmd.c_str());
			}
		}
	}

	while(_gsm->available()) {
		if (!_receiving) {
			_receivingMessageTimer.start(100);
			_receivedMessageIndex = 0;
			_receiving = true;
		} else {
			_receivingMessageTimer.restart();
		}
		char tmp = _gsm->read();

		if (_receivedMessageIndex >= RECEIVED_MESSAGE_MAX_LENGTH) {
			log("RECEIVED MESSAGE MAX LENGTH EXCEEDED");
		} else {
			_receivedMessage[_receivedMessageIndex++] = tmp;
		}
	}
  
	if (_receivingMessageTimer.available()) {
		_receivingMessageTimer.stop();
		_receiving = false;
		
		#ifdef ASYNC_SMS_DEBUG_MODE
		Serial.println(_receivedMessageIndex);
		for (uint16_t i = 0 ; i < _receivedMessageIndex ; i ++){
			Serial.print(_receivedMessage[i]);
			Serial.print(" ");
		}
		Serial.println("");
		#endif
		
		if (_waitingForResponse) {
			_waitingForResponseTimer.stop();
			_waitingForResponse = false;
			
			if (_sendingStage != SMSSendingStageEnum::Idle) {
				processSMSSending();
			}
			handleCommandResponse();
		}
		if (isNewSMS()) {
			log("SMS received");
			parseSMS();
		}
		clearSMSBuffer();
	}
	if (_waitingForResponseTimer.available()) {
		_waitingForResponseTimer.stop();
		_waitingForResponse = false;
		log("Waiting for Response Timeout");
	}
	if (_stateRefreshTimer.available()) {
		enqueue("AT+CREG?");
		enqueue("AT+CSQ");
		_stateRefreshTimer.restart();
	}
}

void AsyncSMS::processSMSSending() {
	bool success = isOK();
	if (_smsSendRetry == SMS_RETRY_COUNT) {
		_smsSendRetry = 0;
		_sendingStage = SMSSendingStageEnum::Idle;
		_smsSendQueueBeginIndex++;
		if (_smsSendQueueBeginIndex == SMS_SEND_QUEUE_LENGTH)
			_smsSendQueueBeginIndex = 0;
	} else if (_sendingStage == SMSSendingStageEnum::Starting) {
		if (success) {
			_waitingForResponse = true;
			_sendingStage = SMSSendingStageEnum::SendingText;
			log("Sending SMS");
			log(_smsSendQueue[_smsSendQueueBeginIndex].number);
			log(_smsSendQueue[_smsSendQueueBeginIndex].message);
			_gsm->write("AT+CMGS=\"");
			_gsm->write(_smsSendQueue[_smsSendQueueBeginIndex].number);
			_gsm->write("\"\r");
			_gsm->write(_smsSendQueue[_smsSendQueueBeginIndex].message);
		} else {
			retrySMSSend();
		}
	} else if (_sendingStage == SMSSendingStageEnum::SendingText) {
		if (success) {
			_waitingForResponse = true;
			_sendingStage = SMSSendingStageEnum::Finishing;
			_gsm->write((char)26);
			_gsm->write('\n');
		} else {
			retrySMSSend();
		}
	} else if (_sendingStage == SMSSendingStageEnum::Finishing) {
		if (isSMSSend()) {
			_smsSendRetry = 0;
			_sendingStage = SMSSendingStageEnum::Idle;
			_smsSendQueueBeginIndex++;
			if (_smsSendQueueBeginIndex == SMS_SEND_QUEUE_LENGTH)
				_smsSendQueueBeginIndex = 0;
		} else {
			retrySMSSend();
		}
	} 
}

void AsyncSMS::retrySMSSend() {
	_sendingStage = SMSSendingStageEnum::Idle;
	_smsSendRetry++;
	enqueueWithoutNewLine("SMS");
}

void AsyncSMS::handleCommandResponse() {
	if (isOK())
		log("OK--");
	else if (isError()) 
		log("ERROR--");
	else if (isSMSSend()) 
		log("SMS SEND--");
	else if (checkFunctionResult("+CREG:"))  {
		log("CREG");
		String res = getResultParameter("+CREG:");
		parseResultValues(res);
		_state[0] = _resValues[0];
		_state[1] = _resValues[1];
	} else if (checkFunctionResult("+CSQ:")) {
		log("CSQ");
		String res = getResultParameter("+CSQ:");
		parseResultValues(res);
		_state[2] = _resValues[0];
		_state[3] = _resValues[1];
	} else 
		log("NIEZNNAE");
}

void AsyncSMS::checkRegistrationState(uint8_t registrationEnabledState, uint8_t registrationState) {
	if (registrationEnabledState == 0 || registrationState == 0 || registrationState == 3) {
		_registrationErrorsCount++;
		if (_registrationErrorsCount >= REGISTRATION_ERROR_CHECKS) {
			reinitGSMModuleConnection();
		}
	}
}

void AsyncSMS::deleteAllSMS() {
	_gsm->write("AT+CMGDA=DEL ALL\r\n");
}

bool AsyncSMS::isNewSMS() {
	return checkFunctionResult("+CMT:");
}

bool AsyncSMS::isSMSSend() {
	return checkFunctionResult("+CMGS:");
}

bool AsyncSMS::isOK() {
	return checkFunctionResult("OK") || checkFunctionResult(">");
}

bool AsyncSMS::isError() {
	return checkFunctionResult("ERROR");
}

bool AsyncSMS::checkFunctionResult(String toCheck) {
	String tmp;
	uint16_t index = findLineBreak(_receivedMessage, _receivedMessageIndex);
	uint16_t border = index + toCheck.length();
	if (border > _receivedMessageIndex)
		border = _receivedMessageIndex;
	for (uint16_t i = index ; i < border; i++) {
		tmp += _receivedMessage[i]; 
	}
	return tmp == toCheck;
}

String AsyncSMS::getResultParameter(String cmd) {
	String tmp;
	uint16_t index = findLineBreak(_receivedMessage, _receivedMessageIndex);
	index += cmd.length() + 1;
	
	for (uint16_t i = index; i < _receivedMessageIndex - 1; i++) {
		if (_receivedMessage[i] == 13 && _receivedMessage[i + 1] == 10) {
			index = i + 2;
			break;
		}
		tmp += _receivedMessage[i]; 
	}
	return tmp;
}

uint8_t AsyncSMS::parseResultValues(String res) {
	uint8_t len = 0;
	
	for (uint8_t i = 0 ; res[i] != 0; i++) {
		len = i + 1;
	}
	uint8_t x = 0;
	for (uint8_t i = 0; i < 4 ; i++) {
		_resValues[i] = 0;
	}
	uint8_t val = 0;
	for (uint8_t i = 0; i < len && x < 4; i++) {
		if (res[i] == ',') {
			_resValues[x++] = val;
			val = 0;
		} else {
			val *= 10;
			val += (res[i] - '0');
		}
	}
	if (len > 0 && x < 4) {
		_resValues[x] = val; 
	}
	return x;
}

void AsyncSMS::enqueue(String cmd) {
	enqueueWithoutNewLine(cmd + "\n");
}

void AsyncSMS::enqueueWithoutNewLine(String cmd) {
	_cmdQueue[_cmdQueueEnd++] = cmd;
	if (_cmdQueueEnd == GSM_CMD_QUEUE_LEN)
		_cmdQueueEnd = 0;
}

String AsyncSMS::dequeue() {
	String cmd = _cmdQueue[_cmdQueueStart++];
	if (_cmdQueueStart == GSM_CMD_QUEUE_LEN) 
		_cmdQueueStart = 0;
	return cmd;
}

bool AsyncSMS::queueAvailable() {
	return _cmdQueueStart != _cmdQueueEnd;
}

void AsyncSMS::clearSMSBuffer() {
  for (uint16_t i = 0; i < RECEIVED_MESSAGE_MAX_LENGTH; i++) {
    _receivedMessage[i] = 0;
  }
  _receivedMessageIndex = 0;
}

void AsyncSMS::parseSMS() {
	char senderNumber[PHONE_NUMBER_LEN], message[RECEIVED_SMS_MAX_LENGTH], dt[20];
	
	for (uint8_t i = 0; i < PHONE_NUMBER_LEN; i++) {
		senderNumber[i] = 0;
	}
	for (uint8_t i = 0; i < RECEIVED_SMS_MAX_LENGTH; i++) {
		message[i] = 0;
	}
	SMSParseStageEnum stage = SMSParseStageEnum::WaitingForNumber;
	uint16_t index = 0;
	for (uint16_t i = 0; i < _receivedMessageIndex - 2; i++) {
		if (_receivedMessage[i] == '"') {
			if (stage == SMSParseStageEnum::WaitingForNumber) {
				stage = SMSParseStageEnum::ParsingNumber;
				index = 0;
			} else if (stage == SMSParseStageEnum::ParsingNumber) {
				stage = SMSParseStageEnum::WaitingForAlpha;
			} else if (stage == SMSParseStageEnum::WaitingForAlpha) {
				stage = SMSParseStageEnum::ParsingAlpha;
				index = 0;
			} else if (stage == SMSParseStageEnum::ParsingAlpha) {
				stage = SMSParseStageEnum::WaitingForDateTime;
			} else if (stage == SMSParseStageEnum::WaitingForDateTime) {
				stage = SMSParseStageEnum::ParsingDateTime;
				index = 0;
			} else if (stage == SMSParseStageEnum::ParsingDateTime) {
				stage = SMSParseStageEnum::WaitingForData;
			}
		} else if (_receivedMessage[i] == 13 && stage != SMSParseStageEnum::ParsingData) {
			if (stage == SMSParseStageEnum::WaitingForData) {
				stage = SMSParseStageEnum::WaitingForData2;
			}
		} else if (_receivedMessage[i] == 10 && stage != SMSParseStageEnum::ParsingData) {
			if (stage == SMSParseStageEnum::WaitingForData2) {
				stage = SMSParseStageEnum::ParsingData;
				index = 0;
			}
		} else {
			if (stage == SMSParseStageEnum::ParsingNumber && index < 12) {
				senderNumber[index++] = _receivedMessage[i];
			} else if (stage == SMSParseStageEnum::ParsingAlpha) {
			} else if (stage == SMSParseStageEnum::ParsingDateTime && index < 20) {
				dt[index++] = _receivedMessage[i];
			} else if (stage == SMSParseStageEnum::ParsingData && index < RECEIVED_SMS_MAX_LENGTH) {
				message[index++] = _receivedMessage[i];
			}
		}
	}
	if (smsReceived)
		smsReceived(senderNumber, message);
}

void AsyncSMS::log(char *msg) {
	if (logger)
		logger(msg);
}

void AsyncSMS::log(String msg) {
	if (logger)
		logger(msg.c_str());
}

uint8_t AsyncSMS::fillState(uint8_t index, uint8_t * data) {
	for (uint8_t i = 0; i < 4; i++) {
		data[index++] = _state[i];
	}
	return index;
}

uint16_t AsyncSMS::findLineBreak(char *msg, uint16_t len) {
	for (uint16_t i = 0; i < len - 1; i++) {
		if (msg[i] == 13 && msg[i + 1] == 10) {
			return i + 2;
		}
	}
}