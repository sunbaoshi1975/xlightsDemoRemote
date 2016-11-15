/**
 * xlSmartRemote.cpp - contains the major implementation of SmartRemoteClass.
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
 * DESCRIPTION
 * 1.
 *
 * ToDo:
**/
#include "xlSmartRemote.h"
#include "xlxASRInterface.h"
#include "xlxConfig.h"
#include "xlxRF24Client.h"
#include "xlxSerialConsole.h"

#include "ArduinoJson.h"

//------------------------------------------------------------------
// Global Data Structures & Variables
//------------------------------------------------------------------
// make an instance for main program
SmartRemoteClass theSys;

//------------------------------------------------------------------
// Smart Controller Class
//------------------------------------------------------------------
SmartRemoteClass::SmartRemoteClass()
{
	m_isRF = false;
}

// Primitive initialization before loading configuration
void SmartRemoteClass::Init()
{
  // Open Serial Port
  TheSerial.begin(SERIALPORT_SPEED_DEFAULT);

#ifdef SYS_SERIAL_DEBUG
	// Wait Serial connection so that we can see the starting information
	while(!TheSerial.available()) { Particle.process(); }
	SERIAL_LN(F("SmartRemote is starting..."));
#endif

	// Get System ID
	m_SysID = System.deviceID();
	m_SysVersion = System.version();
	m_devStatus = STATUS_INIT;

	// Open ASR Interface
	theASR.Init(SERIALPORT_SPEED_LOW);

	SERIAL_LN("SmartRemote is starting...SysID=%s", m_SysID.c_str());
}

// Second level initialization after loading configuration
/// check RF2.4
void SmartRemoteClass::InitRadio()
{
	// Check RF2.4
	if( CheckRF() ) {
  	if (IsRFGood())
  	{
  		SERIAL_LN(F("RF2.4 is working."));
  		SetStatus(STATUS_NWS);
  	}
  }
}

// Get the remote started
BOOL SmartRemoteClass::Start()
{
	SERIAL_LN(F("SmartRemote started."));
	SERIAL_LN("Product Info: %s-%s-%d",
			theConfig.GetOrganization().c_str(), theConfig.GetProductName().c_str(), theConfig.GetVersion());
	SERIAL_LN("System Info: %s-%s",
			GetSysID().c_str(), GetSysVersion().c_str());

#ifndef SYS_SERIAL_DEBUG
	ResetSerialPort();
#endif

	// Check NodeID & Send initialization Message
	if( IsRFGood() ) ResumeRFNetwork();

	return true;
}

void SmartRemoteClass::Restart()
{
	SetStatus(STATUS_RST);
	delay(1000);
	System.reset();
}

void SmartRemoteClass::ResumeRFNetwork()
{
	// Check NodeID
	String strMsg;
	if( IS_NOT_REMOTE_NODEID(theConfig.GetNodeID()) ) {
		SERIAL_LN("Invalid nodeID:%d. Request new one...", theConfig.GetNodeID());
		// Request for NodeID
		SendDeviceRegister();
	} else {
		// Set Radio Address
		theRadio.setAddress(theConfig.GetNodeID(), theConfig.GetNetworkID());
		//theRadio.setAddress(theConfig.GetNodeID(), RF24_BASE_RADIO_ID); // ToDo:test
		// Send Presentation message
		SendDevicePresentation();
	}
}

UC SmartRemoteClass::GetStatus()
{
	return (UC)m_devStatus;
}

BOOL SmartRemoteClass::SetStatus(UC st)
{
	if( st > STATUS_ERR ) return false;
	SERIAL_LN(F("System status changed from %d to %d"), m_devStatus, st);
	if ((UC)m_devStatus != st) {
		m_devStatus = st;
	}
	return true;
}

// Close and reopen serial port to avoid buffer overrun
void SmartRemoteClass::ResetSerialPort()
{
	TheSerial.end();
	TheSerial.begin(SERIALPORT_SPEED_DEFAULT);
}

String SmartRemoteClass::GetSysID()
{
	return m_SysID;
}

String SmartRemoteClass::GetSysVersion()
{
	return m_SysVersion;
}

BOOL SmartRemoteClass::CheckRF()
{
	// RF Server begins
	m_isRF = theRadio.ClientBegin();
	if( m_isRF ) {
		// Change it if setting is not default value
		theRadio.setPALevel(theConfig.GetRFPowerLevel());
		m_isRF = theRadio.CheckConfig();
	}
	return m_isRF;
}

BOOL SmartRemoteClass::SelfCheck(US ms)
{
	static US tickSaveConfig = 0;				// must be static
	static US tickCheckRadio = 0;				// must be static

	// Save config if it was changed
	if (++tickSaveConfig > 15000 / ms) {	// once per 15 seconds
		tickSaveConfig = 0;
		theConfig.SaveConfig();
	}

  // Slow Checking: once per 30 seconds
  if (++tickCheckRadio > 30000 / ms) {
		// Check RF module
		tickCheckRadio = 0;
    if( !IsRFGood() || !theRadio.CheckConfig() ) {
			// Check RF register values even if RF is marked good
      if( CheckRF() ) {
				ResumeRFNetwork();
        SERIAL_LN(F("RF24 moudle recovered."));
      }
		} /*else if( !theConfig.GetPresent() ) {
			// Send node init message if necessary
			ResumeRFNetwork();
		}*/

		// Daily Cloud Synchronization
		/// TimeSync
		theConfig.CloudTimeSync(false);
  }

	// Check System Status
	if( GetStatus() == STATUS_ERR ) {
		SERIAL_LN("System is about to reset due to STATUS_ERR...");
		Restart();
	}

	// ToDo:add any other potential problems to check
	//...

	delay(ms);
	return true;
}

BOOL SmartRemoteClass::IsRFGood()
{
	return (m_isRF && theRadio.isValid());
}

// Process all kinds of commands
void SmartRemoteClass::ProcessCommands()
{
	// Check and process RF2.4 messages
	theRadio.ProcessReceive();

	// Process Console Command
  theConsole.processCommand();

	// Process ASR Command
	theASR.processCommand();

	// ToDo: process commands from other sources (Wifi, BLE)
	// ToDo: Potentially move ReadNewRules here
}

//------------------------------------------------------------------
// Device Control Functions
//------------------------------------------------------------------
// Turn the switch of specific device and all devices on or off
/// Input parameters:
///   sw: true = on; false = off
///   dev: device id or 0 (all devices under this controller)
int SmartRemoteClass::DevSoftSwitch(BOOL sw, UC dev)
{
	String strCmd = String::format("%d:7:%d", dev, sw ? 1 : 0);
	return theRadio.ProcessSend(strCmd);
}

//------------------------------------------------------------------
// Place all util func below
//------------------------------------------------------------------
// Copy JSON array to Hue structure
void SmartRemoteClass::Array2Hue(JsonArray& data, Hue_t& hue)
{
	hue.State = data[0];
	hue.BR = data[1];
	hue.CCT = data[2];
	hue.R = data[3];
	hue.G = data[4];
	hue.B = data[5];
}

// Format:
/// 1. Date: YYYY-MM-DD
/// 2. Time: hh:mm:ss
/// 3. Date and time: YYYY-MM-DD hh:mm:ss
/// 4. "" or "sync": synchronize with could
int SmartRemoteClass::CldSetCurrentTime(String tmStr)
{
	// synchronize with cloud
	tmStr.trim();
	tmStr.toLowerCase();
	if(tmStr.length() == 0 || tmStr == "sync") {
		theConfig.CloudTimeSync();
		return 0;
	}

	US _YYYY = Time.year();
	UC _MM = Time.month();
	UC _DD = Time.day();
	UC _hh = Time.hour();
	UC _mm = Time.minute();
	UC _ss = Time.second();

	int nPos, nPos2, nPos3;
	String strTime = "";
	nPos = tmStr.indexOf('-');
	if( nPos > 0 ) {
		_YYYY = (US)(tmStr.substring(0, nPos).toInt());
		if( _YYYY < 1970 || _YYYY > 2100 ) return 1;
		nPos2 = tmStr.indexOf('-', nPos + 1);
		if( nPos2 < nPos + 1 ) return 2;
		_MM = (UC)(tmStr.substring(nPos + 1, nPos2).toInt());
		if( _MM <= 0 || _MM > 12 ) return 2;
		nPos3 = tmStr.indexOf(' ', nPos2 + 1);
		if( nPos3 < nPos2 + 1 ) {
			_DD = (UC)(tmStr.substring(nPos2 + 1).toInt());
		} else {
			_DD = (UC)(tmStr.substring(nPos2 + 1, nPos3).toInt());
			strTime = tmStr.substring(nPos3 + 1);
		}
		if( _DD <= 0 || _DD > 31 ) return 3;
	} else {
		strTime = tmStr;
	}

	strTime.trim();
	nPos = strTime.indexOf(':');
	if( nPos > 0 ) {
		_hh = (UC)(strTime.substring(0, nPos).toInt());
		if( _hh > 24 ) return 4;
		nPos2 = strTime.indexOf(':', nPos + 1);
		if( nPos2 < nPos + 1 ) return 5;
		_mm = (UC)(strTime.substring(nPos + 1, nPos2).toInt());
		if( _mm > 59 ) return 5;
		_ss = (UC)(strTime.substring(nPos2 + 1).toInt());
		if( _ss > 59 ) return 6;
	}

	time_t myTime = tmConvert_t(_YYYY, _MM, _DD, _hh, _mm, _ss);
	myTime -= theConfig.GetTimeZoneDSTOffset() * 60;	// Convert to local time
	Time.setTime(myTime);
	SERIAL_LN("setTime to %d-%d-%d %d:%d:%d", _YYYY, _MM, _DD, _hh, _mm, _ss);
	return 0;
}

BOOL SmartRemoteClass::SendDeviceRegister()
{
	String strMsg = "0:1";
	return theRadio.ProcessSend(strMsg);
}

BOOL SmartRemoteClass::SendDevicePresentation()
{
	String strMsg = "0:2";
	return theRadio.ProcessSend(strMsg);
}

BOOL SmartRemoteClass::SendReqLampStatus(UC dev)
{
	/*
	BOOL rc = false;
	String strMsg = "1:6";
	if( theRadio.ProcessSend(strMsg) ) {
		strMsg = "1:8";
		if( theRadio.ProcessSend(strMsg) ) {
			strMsg = "1:10";
			rc = theRadio.ProcessSend(strMsg);
		}
	}
	return rc;*/
	String strCmd = String::format("%d:12", dev);
	return theRadio.ProcessSend(strCmd);
}

BOOL SmartRemoteClass::SendChangeBrightness(UC _br, UC dev)
{
	String strCmd = String::format("%d:9:%d", dev, _br);
	return theRadio.ProcessSend(strCmd);
}

BOOL SmartRemoteClass::SendChangeCCT(US _cct, UC dev)
{
	String strCmd = String::format("%d:11:%d", dev, _cct);
	return theRadio.ProcessSend(strCmd);
}

BOOL SmartRemoteClass::SendChangeBR_CCT(UC _br, US _cct, UC dev)
{
	String strCmd = String::format("%d:13:%d:%d", dev, _br, _cct);
	return theRadio.ProcessSend(strCmd);
}
