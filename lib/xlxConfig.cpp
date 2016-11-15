/**
 * xlxConfig.cpp - Xlight Config library for loading & saving configuration
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
 * 1. configurations are stored in the emulated EEPROM and P1 external Flash
 * 1.1 The Photon and Electron have 2047 bytes of emulated EEPROM
 *     with address space from 0x0000 to 0x07FF.
 * 1.2 The P1 has 1M bytes of external Flash, but can only be accessed via
 *     3rd party API currently.
 * 2. Use EEPROM class (high level API) to access the emulated EEPROM.
 * 3. Use spark-flashee-eeprom (low level 3rd party API) to access P1 external Flash.
 * 4. Please refer to xliMemoryMap.h for memory allocation.
 *
 * ToDo:
 * 1. Move default config values to header as global #define's
 * 2.
 * 3.
**/

#include "xlxConfig.h"
#include "xliMemoryMap.h"
#include "xlxRF24Client.h"
#include "xlSmartRemote.h"

#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)

// the one and only instance of ConfigClass
ConfigClass theConfig = ConfigClass();

//------------------------------------------------------------------
// Xlight Config Class
//------------------------------------------------------------------
ConfigClass::ConfigClass()
{
  m_isLoaded = false;
  m_isChanged = false;
  m_isDSTChanged = false;
	m_lastTimeSync = millis();
  InitConfig();
}

void ConfigClass::InitConfig()
{
  memset(&m_config, 0x00, sizeof(m_config));
  m_config.version = VERSION_CONFIG_DATA;
  m_config.timeZone.id = 90;              // Toronto
  m_config.timeZone.offset = -300;        // -5 hours
  m_config.timeZone.dst = 1;              // 1 or 0
  m_config.nodeID = AUTO;
  m_config.networkID = RF24_BASE_RADIO_ID;
  m_config.token = 0;
  strcpy(m_config.Organization, XLA_ORGANIZATION);
  strcpy(m_config.ProductName, XLA_PRODUCT_NAME);
  m_config.indBrightness = 0;
  m_config.enableSpeaker = false;
	m_config.enableDailyTimeSync = true;
	m_config.rfPowerLevel = RF24_PA_LEVEL_NODE;
}

void ConfigClass::InitDevStatus(UC nodeID)
{
	Hue_t whiteHue;
	whiteHue.B = 0;
	whiteHue.G = 0;
	whiteHue.R = 0;
	whiteHue.BR = 50;
	whiteHue.CCT = 2700;
	whiteHue.State = 1; // 1 for on, 0 for off

	m_devStatus.uid = 0;
	m_devStatus.node_id = nodeID;
	m_devStatus.present = 0;
	m_devStatus.type = devtypWRing3;  // White 3 rings
	m_devStatus.ring[0] = whiteHue;
	m_devStatus.ring[1] = whiteHue;
	m_devStatus.ring[2] = whiteHue;
}

BOOL ConfigClass::LoadConfig()
{
  // Load System Configuration
  if( sizeof(Config_t) <= MEM_CONFIG_LEN )
  {
    EEPROM.get(MEM_CONFIG_OFFSET, m_config);
    if( m_config.version == 0xFF
      || m_config.timeZone.id == 0
      || m_config.timeZone.id > 500
      || m_config.timeZone.dst > 1
      || m_config.timeZone.offset < -780
      || m_config.timeZone.offset > 780
			|| m_config.rfPowerLevel > RF24_PA_MAX )
    {
      InitConfig();
      m_isChanged = true;
      SERIAL_LN(F("Sysconfig is empty, use default settings."));
      SaveConfig();
    }
    else
    {
      m_config.token = 0; // Clear previous token
      SERIAL_LN(F("Sysconfig loaded."));
    }
    m_isLoaded = true;
    m_isChanged = false;
		UpdateTimeZone();
  } else {
    SERIAL_LN(F("Failed to load Sysconfig, too large."));
  }

	// Load Device Status
	LoadDeviceStatus();
  return m_isLoaded;
}

BOOL ConfigClass::SaveConfig()
{
  if( m_isChanged )
  {
    EEPROM.put(MEM_CONFIG_OFFSET, m_config);
    m_isChanged = false;
    SERIAL_LN(F("Sysconfig saved."));
  }

	// Save Device Status
	SaveDeviceStatus();

  return true;
}

BOOL ConfigClass::IsConfigLoaded()
{
  return m_isLoaded;
}

BOOL ConfigClass::IsConfigChanged()
{
  return m_isChanged;
}

void ConfigClass::SetConfigChanged(BOOL flag)
{
	m_isChanged = flag;
}

BOOL ConfigClass::IsDSTChanged()
{
  return m_isDSTChanged;
}

void ConfigClass::SetDSTChanged(BOOL flag)
{
	m_isDSTChanged = flag;
}

UC ConfigClass::GetVersion()
{
  return m_config.version;
}

BOOL ConfigClass::SetVersion(UC ver)
{
  if( ver != m_config.version )
  {
    m_config.version = ver;
    m_isChanged = true;
  }
	return true;
}

US ConfigClass::GetTimeZoneID()
{
  return m_config.timeZone.id;
}

BOOL ConfigClass::SetTimeZoneID(US tz)
{
  if( tz == 0 || tz > 500 )
    return false;

  if( tz != m_config.timeZone.id )
  {
    m_config.timeZone.id = tz;
    m_isChanged = true;
    theSys.m_tzString = GetTimeZoneJSON();
  }
  return true;
}

void ConfigClass::UpdateTimeZone()
{
	// Change System Timezone
	Time.zone((float)GetTimeZoneOffset() / 60 + GetDaylightSaving());
}

UC ConfigClass::GetDaylightSaving()
{
  return m_config.timeZone.dst;
}

BOOL ConfigClass::SetDaylightSaving(UC flag)
{
  if( flag > 1 )
    return false;

  if( flag != m_config.timeZone.dst )
  {
    m_config.timeZone.dst = flag;
    m_isChanged = true;
    theSys.m_tzString = GetTimeZoneJSON();
		UpdateTimeZone();
  }
  return true;
}

SHORT ConfigClass::GetTimeZoneOffset()
{
  return m_config.timeZone.offset;
}

SHORT ConfigClass::GetTimeZoneDSTOffset()
{
  return (m_config.timeZone.offset + m_config.timeZone.dst * 60);
}

BOOL ConfigClass::SetTimeZoneOffset(SHORT offset)
{
  if( offset >= -780 && offset <= 780)
  {
    if( offset != m_config.timeZone.offset )
    {
      m_config.timeZone.offset = offset;
      m_isChanged = true;
      theSys.m_tzString = GetTimeZoneJSON();
			// Change System Timezone
			UpdateTimeZone();
    }
    return true;
  }
  return false;
}

String ConfigClass::GetTimeZoneJSON()
{
  String jsonStr = String::format("{\"id\":%d,\"offset\":%d,\"dst\":%d}",
      GetTimeZoneID(), GetTimeZoneOffset(), GetDaylightSaving());
  return jsonStr;
}

String ConfigClass::GetOrganization()
{
  String strName = m_config.Organization;
  return strName;
}

void ConfigClass::SetOrganization(const char *strName)
{
  strncpy(m_config.Organization, strName, sizeof(m_config.Organization) - 1);
  m_isChanged = true;
}

String ConfigClass::GetProductName()
{
  String strName = m_config.ProductName;
  return strName;
}

void ConfigClass::SetProductName(const char *strName)
{
  strncpy(m_config.ProductName, strName, sizeof(m_config.ProductName) - 1);
  m_isChanged = true;
}

UC ConfigClass::GetNodeID()
{
  return m_config.nodeID;
}

void ConfigClass::SetNodeID(UC _nodeID)
{
  if( _nodeID != m_config.nodeID )
  {
    m_config.nodeID = _nodeID;
    m_isChanged = true;
  }
}

uint64_t ConfigClass::GetNetworkID()
{
  return m_config.networkID;
}

void ConfigClass::SetNetworkID(uint64_t _netID)
{
  if( _netID != m_config.networkID )
  {
    m_config.networkID = _netID;
    m_isChanged = true;
  }
}

US ConfigClass::GetToken()
{
  return m_config.token;
}

BOOL ConfigClass::GetPresent()
{
  return(m_config.token > 0);
}

void ConfigClass::SetToken(US _token)
{
  if( _token != m_config.token )
  {
    m_config.token = _token;
    m_isChanged = true;
  }
}

BOOL ConfigClass::ConfigClass::IsDailyTimeSyncEnabled()
{
	return m_config.enableDailyTimeSync;
}

void ConfigClass::SetDailyTimeSyncEnabled(BOOL sw)
{
	if( sw != m_config.enableDailyTimeSync ) {
		m_config.enableDailyTimeSync = sw;
		m_isChanged = true;
	}
}

BOOL ConfigClass::ConfigClass::IsSpeakerEnabled()
{
	return m_config.enableSpeaker;
}

void ConfigClass::SetSpeakerEnabled(BOOL sw)
{
	if( sw != m_config.enableSpeaker ) {
		m_config.enableSpeaker = sw;
		m_isChanged = true;
	}
}

void ConfigClass::DoTimeSync()
{
	// Request time synchronization from the Particle Cloud
	Particle.syncTime();
	m_lastTimeSync = millis();
	SERIAL_LN("Time synchronized");
}

BOOL ConfigClass::CloudTimeSync(BOOL _force)
{
	if( _force ) {
		DoTimeSync();
		return true;
	} else if( theConfig.IsDailyTimeSyncEnabled() ) {
		if( (millis() - m_lastTimeSync) / 1000 > SECS_PER_DAY ) {
			DoTimeSync();
			return true;
		}
	}
	return false;
}

UC ConfigClass::GetBrightIndicator()
{
  return m_config.indBrightness;
}

BOOL ConfigClass::SetBrightIndicator(UC level)
{
  if( level != m_config.indBrightness )
  {
    m_config.indBrightness = level;
    m_isChanged = true;
    return true;
  }

  return false;
}

UC ConfigClass::GetRFPowerLevel()
{
	return m_config.rfPowerLevel;
}

BOOL ConfigClass::SetRFPowerLevel(UC level)
{
	if( level > RF24_PA_MAX ) level = RF24_PA_MAX;
	if( level != m_config.rfPowerLevel ) {
		m_config.rfPowerLevel = level;
		theRadio.setPALevel(level);
		m_isChanged = true;
		return true;
	}
	return false;
}

// Load Device Status
BOOL ConfigClass::LoadDeviceStatus()
{
	if (DST_ROW_SIZE <= MEM_DEVICE_STATUS_LEN)
	{
		EEPROM.get(MEM_DEVICE_STATUS_OFFSET, m_devStatus);
		if (m_devStatus.uid != 0
      || m_devStatus.ring[0].BR > 100
      || m_devStatus.ring[0].CCT < CT_MIN_VALUE
      || m_devStatus.ring[0].CCT > CT_MAX_VALUE)
		{
			InitDevStatus(NODEID_MAINDEVICE);
      m_isDSTChanged = true;
      SERIAL_LN(F("DST is empty, use default settings."));
      SaveDeviceStatus();
    }
    else
    {
      SERIAL_LN(F("DST loaded."));
    }
		m_isDSTChanged = false;
	}
	else
	{
		SERIAL_LN(F("Failed to load DST, too large."));
		return false;
	}

	return true;
}

// Save Device Status
BOOL ConfigClass::SaveDeviceStatus()
{
	if (m_isDSTChanged)
	{
		EEPROM.put(MEM_DEVICE_STATUS_OFFSET, m_devStatus);
    m_isDSTChanged = false;
    SERIAL_LN(F("DST saved."));
	}
	return true;
}

BOOL ConfigClass::GetDevPresent()
{
  return m_devStatus.present;
}

void ConfigClass::SetDevPresent(BOOL _present)
{
  if( _present != m_devStatus.present )
  {
    m_devStatus.present = _present;
    m_isDSTChanged = true;
    SERIAL_LN("Dev present %s", _present ? "Yes" : "No");
  }
}

UC ConfigClass::GetDevType()
{
  return m_devStatus.type;
}

void ConfigClass::SetDevType(UC _type)
{
  if( _type != m_devStatus.type )
  {
    m_devStatus.type = _type;
    m_isDSTChanged = true;
  }
}

BOOL ConfigClass::GetDevStatus()
{
  return m_devStatus.ring[0].State;
}

void ConfigClass::SetDevStatus(BOOL _status)
{
  if( _status != m_devStatus.ring[0].State )
  {
    m_devStatus.ring[0].State = _status;
    m_devStatus.ring[1].State = _status;
    m_devStatus.ring[2].State = _status;
    m_isDSTChanged = true;
    SERIAL_LN("Dev Lights %s", _status ? "On" : "Off");
  }
}

UC ConfigClass::GetDevBrightness()
{
  return m_devStatus.ring[0].BR;
}

void ConfigClass::SetDevBrightness(UC _level)
{
  if( _level != m_devStatus.ring[0].BR )
  {
    m_devStatus.ring[0].BR = _level;
    m_devStatus.ring[1].BR = _level;
    m_devStatus.ring[2].BR = _level;
    m_isDSTChanged = true;
    SERIAL_LN("Dev Brightness changed %d", _level);
  }
}

US ConfigClass::GetDevCCT()
{
  return m_devStatus.ring[0].CCT;
}

void ConfigClass::SetDevCCT(US _cct)
{
  if( _cct != m_devStatus.ring[0].CCT )
  {
    m_devStatus.ring[0].CCT = _cct;
    m_devStatus.ring[1].CCT = _cct;
    m_devStatus.ring[2].CCT = _cct;
    m_isDSTChanged = true;
    SERIAL_LN("Dev CCT changed %d", _cct);
  }
}

String ConfigClass::hue_to_string(Hue_t hue)
{
	String out = "";
	out.concat("State:");
	out.concat(hue.State);
	out.concat("|");
	out.concat("BR:");
	out.concat(hue.BR);
	out.concat("|");
	out.concat("CCT:");
	out.concat(hue.CCT);
	out.concat("|");
	out.concat("R:");
	out.concat(hue.R);
	out.concat("|");
	out.concat("G:");
	out.concat(hue.G);
	out.concat("|");
	out.concat("B:");
	out.concat(hue.B);

	return out;
}

void ConfigClass::print_devStatus()
{
	SERIAL_LN("==== DevStatus ====");
	SERIAL_LN("uid = %d", m_devStatus.uid);
	SERIAL_LN("node_id = %d", m_devStatus.node_id);
	SERIAL_LN("type = %d", m_devStatus.type);
  SERIAL_LN("Present = %s", GetDevPresent() ? "Yes" : "No");
  SERIAL_LN("Status = %s", GetDevStatus() ? "On" : "Off");
  SERIAL_LN("Brightness = %d", GetDevBrightness());
  SERIAL_LN("Color Temperature = %d\n\r", GetDevCCT());
}
