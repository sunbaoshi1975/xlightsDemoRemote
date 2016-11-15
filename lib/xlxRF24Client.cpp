/**
 * xlxRF24Client.cpp - Xlight RF2.4 client library based on via RF2.4 module
 * for communication with instances of xlxRF24Client
 *
 * Created by Baoshi Sun <bs.sun@datatellit.com>
 * Copyright (C) 2015-2016 DTIT
 * Full contributor list:
 *
 * Documentation:
 * Support Forum:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * REVISION HISTORY
 * Version 1.0 - Created by Baoshi Sun <bs.sun@datatellit.com>
 *
 * Dependancy
 * 1. Particle RF24 library, refer to
 *    https://github.com/stewarthou/Particle-RF24
 *    http://tmrh20.github.io/RF24/annotated.html
 *
 * DESCRIPTION
 * 1. Mysensors protocol compatible and extended
 * 2. Address algorithm
 * 3. PipePool (0 AdminPipe, Read & Write; 1-5 pipe pool, Read)
 * 4. Session manager, optional address shifting
 * 5.
 *
 * ToDo:
 * 1. Two pipes collaboration for security: divide a message into two parts
 * 2. Apply security layer, e.g. AES, encryption, etc.
 *
**/

#include "xlxRF24Client.h"
#include "xlSmartRemote.h"
#include "MyParserSerial.h"

//------------------------------------------------------------------
// the one and only instance of RF24ClientClass
RF24ClientClass theRadio(PIN_RF24_CE, PIN_RF24_CS);
MyMessage msg;
MyParserSerial msgParser;
UC *msgData = (UC *)&(msg.msg);

RF24ClientClass::RF24ClientClass(uint8_t ce, uint8_t cs, uint8_t paLevel)
	:	MyTransportNRF24(ce, cs, paLevel)
{
	_times = 0;
	_succ = 0;
	_received = 0;
}

bool RF24ClientClass::ClientBegin(const uint8_t bNodeID)
{
  // Initialize RF module
	if( !init() ) {
    SERIAL_LN(F("RF24 module is not valid!"));
		return false;
	}
  ChangeNodeID(bNodeID);
  return true;
}

// Make NetworkID with the right 4 bytes of device MAC address
uint64_t RF24ClientClass::GetNetworkID(bool _full)
{
  uint64_t netID = 0;

  byte mac[6];
  WiFi.macAddress(mac);
	int i = (_full ? 0 : 2);
  for (; i<6; i++) {
    netID += mac[i];
		if( _full && i == 5 ) break;
    netID <<= 8;
  }

  return netID;
}

bool RF24ClientClass::ChangeNodeID(const uint8_t bNodeID)
{
	if( bNodeID == getAddress() ) {
			return true;
	}

	if( bNodeID == 0 ) {
		return false;
	} else {
		setAddress(bNodeID, RF24_BASE_RADIO_ID);
	}

	return true;
}

bool RF24ClientClass::ProcessSend(String &strMsg, MyMessage &my_msg)
{
	bool sentOK = false;
	bool bMsgReady = false;
	uint8_t bytValue;
	int iValue;
	float fValue;
	char strBuffer[64];
	uint8_t payload[MAX_PAYLOAD];

	int nPos = strMsg.indexOf(':');
	uint8_t lv_nNodeID;
	uint8_t lv_nMsgID;
	String lv_sPayload;
	if (nPos > 0) {
		// Extract NodeID & MessageID & Payload
		lv_nNodeID = (uint8_t)(strMsg.substring(0, nPos).toInt());
		int nPos2 = strMsg.indexOf(':', nPos + 1);
		if (nPos2 > 0) {
			lv_nMsgID = (uint8_t)(strMsg.substring(nPos + 1, nPos2).toInt());
			lv_sPayload = strMsg.substring(nPos2 + 1);
		} else {
			lv_nMsgID = (uint8_t)(strMsg.substring(nPos + 1).toInt());
			lv_sPayload = "";
		}
	}
	else {
		// Parse serial message
		lv_nMsgID = 0;
	}

	switch (lv_nMsgID)
	{
	case 0: // Free style
		iValue = min(strMsg.length(), 63);
		strncpy(strBuffer, strMsg.c_str(), iValue);
		strBuffer[iValue] = 0;
		// Serail format to MySensors message structure
		bMsgReady = msgParser.parse(msg, strBuffer);
		if (bMsgReady) {
			SERIAL("Now sending message...");
		}
		break;

	case 1:   // Request new node ID
		if (getAddress() == GATEWAY_ADDRESS) {
			SERIAL_LN("Controller can not request node ID\n\r");
		}
		else {
			UC deviceType = NODE_TYP_REMOTE;
			ChangeNodeID(AUTO);
			msg.build(AUTO, BASESERVICE_ADDRESS, deviceType, C_INTERNAL, I_ID_REQUEST, false);
			msg.set(GetNetworkID(true));		// identity: could be MAC, serial id, etc
			bMsgReady = true;
			SERIAL("Now sending request node id message...");
		}
		break;

	case 2:   // Remote present, req ack
		{
			UC remoteType = (UC)remotetypRFSimply;
			msg.build(getAddress(), lv_nNodeID, remoteType, C_PRESENTATION, S_DIMMER, true);
			msg.set(GetNetworkID(true));
			bMsgReady = true;
			SERIAL("Now sending remote present message...");
		}
		break;

	case 3:   // Temperature sensor present with sensor id 1, req no ack
		msg.build(getAddress(), lv_nNodeID, 1, C_PRESENTATION, S_TEMP, false);
		msg.set("");
		bMsgReady = true;
		SERIAL("Now sending DHT11 present message...");
		break;

	case 4:   // Temperature set to 23.5, req no ack
		msg.build(getAddress(), lv_nNodeID, 1, C_SET, V_TEMP, false);
		fValue = 23.5;
		msg.set(fValue, 2);
		bMsgReady = true;
		SERIAL("Now sending set temperature message...");
		break;

	case 5:   // Humidity set to 45, req no ack
		msg.build(getAddress(), lv_nNodeID, 1, C_SET, V_HUM, false);
		iValue = 45;
		msg.set(iValue);
		bMsgReady = true;
		SERIAL("Now sending set humidity message...");
		break;

	case 6:   // Get main lamp(ID:1) power(V_STATUS:2) on/off, ack
		msg.build(getAddress(), lv_nNodeID, 1, C_REQ, V_STATUS, true);
		bMsgReady = true;
		SERIAL("Now sending get V_STATUS message...");
		break;

	case 7:   // Set main lamp(ID:1) power(V_STATUS:2) on/off, ack
		msg.build(getAddress(), lv_nNodeID, 1, C_SET, V_STATUS, true);
		bytValue = (lv_sPayload == "1" ? 1 : 0);
		msg.set(bytValue);
		bMsgReady = true;
		SERIAL("Now sending set V_STATUS %s message...", (bytValue ? "on" : "off"));
		break;

	case 8:   // Get main lamp(ID:1) dimmer (V_PERCENTAGE:3), ack
		msg.build(getAddress(), lv_nNodeID, 1, C_REQ, V_PERCENTAGE, true);
		bMsgReady = true;
		SERIAL("Now sending get V_PERCENTAGE message...");
		break;

	case 9:   // Set main lamp(ID:1) dimmer (V_PERCENTAGE:3), ack
		msg.build(getAddress(), lv_nNodeID, 1, C_SET, V_PERCENTAGE, true);
		bytValue = constrain(atoi(lv_sPayload), 0, 100);
		msg.set(bytValue);
		bMsgReady = true;
		SERIAL("Now sending set V_PERCENTAGE:%d message...", bytValue);
		break;

	case 10:  // Get main lamp(ID:1) color temperature (V_LEVEL), ack
		msg.build(getAddress(), lv_nNodeID, 1, C_REQ, V_LEVEL, true);
		bMsgReady = true;
		SERIAL("Now sending get CCT V_LEVEL message...");
		break;

	case 11:  // Set main lamp(ID:1) color temperature (V_LEVEL), ack
		msg.build(getAddress(), lv_nNodeID, 1, C_SET, V_LEVEL, true);
		iValue = constrain(atoi(lv_sPayload), CT_MIN_VALUE, CT_MAX_VALUE);
		msg.set((unsigned int)iValue);
		bMsgReady = true;
		SERIAL("Now sending set CCT V_LEVEL %d message...", iValue);
		break;

	case 12:  // Request lamp status in one
		msg.build(getAddress(), lv_nNodeID, 1, C_REQ, V_RGBW, true);
		msg.set((uint8_t)RING_ID_ALL);		// RING_ID_1 is also workable currently
		bMsgReady = true;
		SERIAL("Now sending get dev-status (V_RGBW) message...");
		break;

	case 13:  // Set main lamp(ID:1) status in one, ack
		msg.build(getAddress(), lv_nNodeID, 1, C_SET, V_RGBW, true);
		bytValue = 65;
		iValue = 3000;
		nPos = lv_sPayload.indexOf(':');
		if (nPos > 0) {
			// Extract brightness & cct
			bytValue = (uint8_t)(lv_sPayload.substring(0, nPos).toInt());
			iValue = lv_sPayload.substring(nPos + 1).toInt();
			iValue = constrain(iValue, CT_MIN_VALUE, CT_MAX_VALUE);
		}
		payload[0] = 1;
	  payload[1] = bytValue;
	  payload[2] = iValue % 256;
	  payload[3] = iValue / 256;
		msg.set((void*)payload, 4);
		bMsgReady = true;
		SERIAL("Now sending set CCT V_RGBW (br=%d, cct=%d message...", bytValue, iValue);
		break;
	}

	if (bMsgReady) {
		SERIAL("to %d...", msg.getDestination());
		sentOK = ProcessSend();
		my_msg = msg;
		SERIAL_LN(sentOK ? "OK" : "failed");
	}

	return sentOK;
}

bool RF24ClientClass::ProcessSend(String &strMsg)
{
	MyMessage tempMsg;
	return ProcessSend(strMsg, tempMsg);
}

// ToDo: add message to queue instead of sending out directly
bool RF24ClientClass::ProcessSend(MyMessage *pMsg)
{
	if( !pMsg ) { pMsg = &msg; }

	// Determine the receiver addresse
	UC replyTo;
	_times++;
	if( theConfig.GetPresent() ) {
		// Send to Gateway
		replyTo = NODEID_GATEWAY;
	} else { // Send to destination directly
		replyTo = pMsg->getDestination();
	}
	if( send(replyTo, *pMsg) )
	{
		_succ++;
		return true;
	}

	return false;
}

bool RF24ClientClass::ProcessReceive()
{
	if( !isValid() ) return false;

	bool msgReady = false;
  bool sentOK = false;
  uint8_t to = theRadio.getAddress();
  uint8_t pipe;
	UC replyTo;
  if (!available(&to, &pipe))
    return false;

  uint8_t len = receive(msgData);
  if( len < HEADER_SIZE )
  {
    SERIAL_LN("got corrupt dynamic payload!");
    return false;
  } else if( len > MAX_MESSAGE_LENGTH )
  {
    SERIAL_LN("message length exceeded: %d", len);
    return false;
  }

  char strDisplay[SENSORDATA_JSON_SIZE];
  _received++;
	uint8_t _cmd = msg.getCommand();
	uint8_t _type = msg.getType();
  uint8_t _sender = msg.getSender();  // The original sender
	uint8_t _destination = msg.getDestination();
  uint8_t _sensor = msg.getSensor();
  bool _needAck = msg.isReqAck();
  bool _isAck = msg.isAck();
	uint8_t *payload = (uint8_t *)msg.getCustom();
  SERIAL_LN("Received from pipe %d msg-len=%d, from:%d to:%d dest:%d cmd:%d type:%d sensor:%d payl-len:%d",
        pipe, len, _sender, to, _destination, _cmd, _type, _sensor, msg.getLength());

  switch( _cmd )
  {
    case C_INTERNAL:
      if( _type == I_ID_RESPONSE && (getAddress() == AUTO || getAddress() == BASESERVICE_ADDRESS) ) {
				// Device/client got nodeID from Controller
				uint8_t lv_nodeID = msg.getSensor();
        if( lv_nodeID == NODEID_GATEWAY || lv_nodeID == NODEID_DUMMY ) {
          SERIAL_LN(F("Failed to get NodeID"));
        } else {
					uint64_t lv_networkID = msg.getUInt64();
          SERIAL_LN("Get NodeId: %d, networkId: %s", lv_nodeID, PrintUint64(strDisplay, lv_networkID));
          setAddress(lv_nodeID, lv_networkID);
					theConfig.SetNodeID(lv_nodeID);
					theConfig.SetNetworkID(lv_networkID);
					theSys.SendDevicePresentation();
        }
      }
      break;

		case C_PRESENTATION:
			if( _type == S_LIGHT || _type == S_DIMMER ) {
				US token;
				if( msg.isAck() ) {
					// Device/client got Response to Presentation message, ready to work
					token = msg.getUInt();
					SERIAL_LN("Node:%d got Presentation Response with token:%d", getAddress(), token);
					theConfig.SetToken(token);
					theSys.SendReqLampStatus(NODEID_MAINDEVICE);
				}
			}
			break;

		case C_REQ:
			if( _isAck && _type == V_RGBW ) {
				SERIAL_LN("Ack msg from %d - V_RGBW", _sender);
				if( payload[0] ) {	// Succeed or not
					UC _devType = payload[1];	// payload[2] is present status
					UC _ringID = payload[3];
					theConfig.SetDevType(_devType);
					theConfig.SetDevPresent(payload[2]);
					theConfig.SetDevStatus(payload[4]);
					theConfig.SetDevBrightness(payload[5]);
					if( IS_SUNNY(_devType) ) {
						// Sunny
						US _CCTValue = payload[7] * 256 + payload[6];
						theConfig.SetDevCCT(_CCTValue);
					} else if( IS_RAINBOW(_devType) || IS_MIRAGE(_devType) ) {
						// Rainbow or Mirage
						// ToDo: set RWB
					}
				} else {
					theConfig.SetDevPresent(false);
					theConfig.SetDevStatus(false);
				}
			}  // Note: no break
		case C_SET:
			if( _isAck) {
				UC _OnOff, _Brightness;
		    if( _type == V_STATUS ) {
					_OnOff = msg.getByte();
					SERIAL_LN("Ack msg from %d - lights %s", _sender, _OnOff ? "on" : "off");
					theConfig.SetDevPresent(true);
					theConfig.SetDevStatus(_OnOff);
				} else if( _type == V_PERCENTAGE ) {
					_OnOff = payload[0];
					_Brightness = payload[1];
					SERIAL_LN("Ack msg from %d - lights %s brightness %d", _sender, (_OnOff ? "on" : "off"), _Brightness);
					theConfig.SetDevPresent(true);
					theConfig.SetDevStatus(_OnOff);
					theConfig.SetDevBrightness(_Brightness);
				} else if( _type == V_LEVEL ) {
					US _CCTValue = (US)msg.getUInt();
					SERIAL_LN("Ack msg from %d - CCT level %d", _sender, _CCTValue);
					theConfig.SetDevPresent(true);
					theConfig.SetDevCCT(_CCTValue);
				}
			}
			break;

    default:
      break;
  }

	// Send reply message
	if( msgReady ) {
		_times++;
		if( theConfig.GetPresent() ) {
			// Send to Gateway
			replyTo = NODEID_GATEWAY;
		} else { // Send to destination directly
			replyTo = msg.getDestination();
		}
		sentOK = send(replyTo, msg, pipe);
		if( sentOK ) _succ++;
	}

  return true;
}
