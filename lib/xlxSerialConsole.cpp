/**
 * xlxSerialConsole.cpp - Xlight Device Management Console via Serial Port
 * This module offers monitoring, testing, device control and setting features
 * in both interactive mode and command line mode.
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
 * 1. Instruction output
 * 2. Select mode: Interactive mode or Command line
 * 3. In interactive mode, navigate menu and follow the screen instruction
 * 4. In command line mode, input the command and press enter. Need user manual
 * 5.
 *
 * Command category:
 * - do: execute command or function, e.g. control the lights, send a message, etc.
 * - help: show help information.
 * - ping: ping an IP or domain name
 * - send: send testing message
 * - set: log level, flags, system settings, communication parameters, etc.
 * - show: system info, status, variable, statistics, etc.
 * - sys: system level operation, like reset, recover, safe mode, etc.
 * - test: generate test message or simulate user operation
 *
 * ToDo:
 * 1.
 *
**/

#include "xlxSerialConsole.h"
#include "xlSmartRemote.h"
#include "xlxASRInterface.h"
#include "xlxConfig.h"
#include "xlxRF24Client.h"

//------------------------------------------------------------------
// the one and only instance of SerialConsoleClass
SerialConsoleClass theConsole;
String gstrWiFi_SSID;
String gstrWiFi_Password;
int gintWiFi_Auth;

//------------------------------------------------------------------
// Global Callback Function Helper
bool gc_nop(const char *cmd) { return true; }
bool gc_doHelp(const char *cmd) { return theConsole.doHelp(cmd); }
bool gc_doCheck(const char *cmd) { return theConsole.doCheck(cmd); }
bool gc_doShow(const char *cmd) { return theConsole.doShow(cmd); }
bool gc_doPing(const char *cmd) { return theConsole.doPing(cmd); }
bool gc_doAction(const char *cmd) { return theConsole.doAction(cmd); }
bool gc_doTest(const char *cmd) { return theConsole.doTest(cmd); }
bool gc_doSend(const char *cmd) { return theConsole.doSend(cmd); }
bool gc_doSet(const char *cmd) { return theConsole.doSet(cmd); }
bool gc_doSys(const char *cmd) { return theConsole.doSys(cmd); }
bool gc_doSysSub(const char *cmd) { return theConsole.doSysSub(cmd); }
bool gc_doSysSetupWiFi(const char *cmd) { return theConsole.SetupWiFi(cmd); }
bool gc_doSysSetWiFiCredential(const char *cmd) { return theConsole.SetWiFiCredential(cmd); }

//------------------------------------------------------------------
// State Machine
/// State
typedef enum
{
  consoleRoot = 0,
  consoleHelp,
  consoleCheck,
  consoleShow,
  consolePing,
  consoleDo,
  consoleTest,
  consoleSend,
  consoleSet,
  consoleSys,

  consoleSetupWiFi = 51,
  consoleWF_YesNo,
  consoleWF_GetSSID,
  consoleWF_GetPassword,
  consoleWF_GetAUTH,
  consoleWF_Confirm,

  consoleDummy = 255
} consoleState_t;

/// Matrix: actual definition for command/handler array
const StateMachine_t fsmMain[] = {
  // Current-State    Next-State      Event-String        Function
  {consoleRoot,       consoleRoot,    "?",                gc_doHelp},
  {consoleRoot,       consoleRoot,    "help",             gc_doHelp},
  {consoleRoot,       consoleRoot,    "check",            gc_doCheck},
  {consoleRoot,       consoleRoot,    "show",             gc_doShow},
  {consoleRoot,       consoleRoot,    "ping",             gc_doPing},
  {consoleRoot,       consoleRoot,    "do",               gc_doAction},
  {consoleRoot,       consoleRoot,    "test",             gc_doTest},
  {consoleRoot,       consoleRoot,    "send",             gc_doSend},
  {consoleRoot,       consoleRoot,    "set",              gc_doSet},
  {consoleRoot,       consoleSys,     "sys",              gc_doSys},
  /// Menu default
  {consoleRoot,       consoleRoot,    "",                 gc_doHelp},

  /// Shared function
  {consoleSys,        consoleRoot,    "reset",            gc_doSysSub},
  {consoleSys,        consoleRoot,    "safe",             gc_doSysSub},
  {consoleSys,        consoleRoot,    "dfu",              gc_doSysSub},
  {consoleSys,        consoleRoot,    "update",           gc_doSysSub},
  {consoleSys,        consoleRoot,    "sync",             gc_doSysSub},
  {consoleSys,        consoleRoot,    "clear",            gc_doSysSub},
  {consoleSys,        consoleRoot,    "base",             gc_doSysSub},
  {consoleSys,        consoleRoot,    "private",          gc_doSysSub},
  {consoleSys,        consoleRoot,    "serial",           gc_doSysSub},
  /// Workflow
  {consoleSys,        consoleWF_YesNo,   "setup",         gc_doSysSetupWiFi},
  /// Menu default
  {consoleSys,        consoleRoot,    "",                 gc_doHelp},

  {consoleWF_YesNo,   consoleWF_GetSSID, "yes",           gc_doSysSetupWiFi},
  {consoleWF_YesNo,   consoleWF_GetSSID, "y",             gc_doSysSetupWiFi},
  {consoleWF_YesNo,   consoleRoot,    "no",               gc_nop},
  {consoleWF_YesNo,   consoleRoot,    "n",                gc_nop},
  {consoleWF_YesNo,   consoleRoot,    "",                 gc_nop},
  {consoleWF_GetSSID, consoleWF_GetPassword, "",          gc_doSysSetupWiFi},
  {consoleWF_GetPassword, consoleWF_GetAUTH, "",          gc_doSysSetupWiFi},
  {consoleWF_GetAUTH, consoleWF_Confirm, "0",             gc_doSysSetupWiFi},
  {consoleWF_GetAUTH, consoleWF_Confirm, "1",             gc_doSysSetupWiFi},
  {consoleWF_GetAUTH, consoleWF_Confirm, "2",             gc_doSysSetupWiFi},
  {consoleWF_GetAUTH, consoleWF_Confirm, "3",             gc_doSysSetupWiFi},
  {consoleWF_GetAUTH, consoleRoot,    "q",                gc_nop},
  {consoleWF_Confirm, consoleRoot,    "yes",              gc_doSysSetWiFiCredential},
  {consoleWF_Confirm, consoleRoot,    "y",                gc_doSysSetWiFiCredential},
  {consoleWF_Confirm, consoleRoot,    "no",               gc_nop},
  {consoleWF_Confirm, consoleRoot,    "n",                gc_nop},
  {consoleWF_Confirm, consoleRoot,    "",                 gc_nop},

  /// System default
  {consoleDummy,      consoleRoot,    "",                 gc_doHelp}
};

const char *strAuthMethods[4] = {"None", "WPA2", "WEP", "TKIP"};

//------------------------------------------------------------------
// Xlight SerialConsole Class
//------------------------------------------------------------------
SerialConsoleClass::SerialConsoleClass()
{
  isInCloudCommand = false;
}

void SerialConsoleClass::Init()
{
  SetStateMachine(fsmMain, sizeof(fsmMain) / sizeof(StateMachine_t), consoleRoot);
  addDefaultHandler(NULL);    // Use virtual callback function
}

bool SerialConsoleClass::processCommand()
{
  bool retVal = readSerial();
  if( !retVal ) {
    SERIAL_LN(F("Unknown command or incorrect arguments\n\r"));
  }

  return retVal;
}

bool SerialConsoleClass::callbackCommand(const char *cmd)
{
  IF_SERIAL_DEBUG(SERIAL_LN(F("do callback %s"), cmd));

  return true;
}

bool SerialConsoleClass::callbackDefault(const char *cmd)
{
  return doHelp(cmd);
}

//--------------------------------------------------
// Command Functions
//--------------------------------------------------
bool SerialConsoleClass::showThisHelp(String &strTopic)
{
  if(strTopic.equals("check")) {
    SERIAL_LN(F("--- Command: check <object> ---"));
    SERIAL_LN(F("To check component status, where <object> could be:"));
    SERIAL_LN(F("   ble:   check BLE module availability"));
    SERIAL_LN(F("   flash: check flash space"));
    SERIAL_LN(F("   rf:    check RF availability"));
    SERIAL_LN(F("   wifi:  check Wi-Fi module status"));
    SERIAL_LN(F("   wlan:  check internet status"));
    SERIAL_LN(F("e.g. check rf\n\r"));
    CloudOutput(F("check ble|flash|rf|wifi|wlan"));
  } else if(strTopic.equals("show")) {
    SERIAL_LN(F("--- Command: show <object> ---"));
    SERIAL_LN(F("To show value or summary information, where <object> could be:"));
    SERIAL_LN(F("   ble:     show BLE summary"));
    SERIAL_LN(F("   debug:   show debug channel and level"));
    SERIAL_LN(F("   dev:     show device list"));
    SERIAL_LN(F("   flag:    show system flags"));
    SERIAL_LN(F("   net:     show network summary"));
    SERIAL_LN(F("   node:    show node summary"));
    SERIAL_LN(F("   nlist:   show NodeID list"));
    SERIAL_LN(F("   rf:      print RF details"));
    SERIAL_LN(F("   time:    show current time and time zone"));
    SERIAL_LN(F("   var:     show system variables"));
    SERIAL_LN(F("   table:   show working memory tables"));
    SERIAL_LN(F("   version: show firmware version"));
    SERIAL_LN(F("e.g. show rf\n\r"));
    CloudOutput(F("show ble|debug|dev|flag|net|node|rf|time|var|table|version"));
  } else if(strTopic.equals("ping")) {
    SERIAL_LN(F("--- Command: ping <address> ---"));
    SERIAL_LN(F("To ping an IP or domain name, default address is 8.8.8.8"));
    SERIAL_LN(F("e.g. ping www.google.com"));
    SERIAL_LN(F("e.g. ping 192.168.0.1\n\r"));
    CloudOutput(F("ping <address>"));
  } else if(strTopic.equals("do")) {
    SERIAL_LN(F("--- Command: do <action parameters> ---"));
    SERIAL_LN(F("To execute action, e.g. turn on the lamp"));
    SERIAL_LN(F("e.g. do on"));
    SERIAL_LN(F("e.g. do off"));
    SERIAL_LN(F("e.g. do color R,G,B\n\r"));
    CloudOutput(F("do on|off|color"));
  } else if(strTopic.equals("test")) {
    SERIAL_LN(F("--- Command: test <action parameters> ---"));
    SERIAL_LN(F("To perform testing, where <action> could be:"));
    SERIAL_LN(F("   ping <ip address>: ping ip address"));
    SERIAL_LN(F("   send <NodeId:MessageId>: send test message to node"));
    SERIAL_LN(F("   send <message>: send MySensors format message"));
    SERIAL_LN(F("   asr <cmd>: send command to ASR module\n\r"));
    CloudOutput(F("test ping|send"));
  } else if(strTopic.equals("send")) {
    SERIAL_LN(F("--- Command: send <message> or <NodeId:MessageId> ---"));
    SERIAL_LN(F("To send testing message"));
    SERIAL_LN(F("e.g. send 0:1"));
    SERIAL_LN(F("e.g. send 0;1;0;0;6;"));
    SERIAL_LN(F("e.g. send 0;1;1;0;0;23.5\n\r"));
    CloudOutput(F("send <message> or <NodeId:MessageId>"));
  } else if(strTopic.equals("set")) {
    char *sObj = next();
    if( sObj ) {
      if (strnicmp(sObj, "flag", 4) == 0) {
        SERIAL_LN(F("--- Command: set flag <flag name> [0|1] ---"));
        SERIAL_LN(F("<flag name>: csc, cdts"));
        SERIAL_LN(F("e.g. set flag csc [0|1]"));
        SERIAL_LN(F("     , enable or disable Cloud Serial Command"));
        SERIAL_LN(F("e.g. set flag cdts [0|1]"));
        SERIAL_LN(F("     , enable or disable Cloud Daily Time Sync"));
        CloudOutput(F("set flag csc|cdts"));
      } else if (strnicmp(sObj, "var", 3) == 0) {
        SERIAL_LN(F("--- Command: set var <var name> <value> ---"));
        SERIAL_LN(F("<var name>: senmap, devst, rfpl"));
        SERIAL_LN(F("e.g. set var senmap 23"));
        SERIAL_LN(F("     , set Sensor Bitmap to 0x17"));
        SERIAL_LN(F("e.g. set var devst 5"));
        SERIAL_LN(F("     , set Device Status to sleep mode"));
        SERIAL_LN(F("e.g. set var rfpl [0..3]"));
        SERIAL_LN(F("     , set RF Power Level to min(0), low(1), high(2) or max(3)"));
        CloudOutput(F("set var senmap|devst|rfpl"));
      } else if (strnicmp(sObj, "spkr", 4) == 0) {
        SERIAL_LN(F("--- Command: set spkr [0|1] ---"));
        SERIAL_LN(F("To disable or enable speaker"));
        CloudOutput(F("set spkr 0|1"));
      }
    } else {
      SERIAL_LN(F("--- Command: set <object value> ---"));
      SERIAL_LN(F("To change config"));
      SERIAL_LN(F("e.g. set tz -5"));
      SERIAL_LN(F("e.g. set dst [0|1]"));
      SERIAL_LN(F("     , set daylight saving time"));
      SERIAL_LN(F("e.g. set time sync"));
      SERIAL_LN(F("     , synchronize time with Cloud"));
      SERIAL_LN(F("e.g. set time hh:mm:ss"));
      SERIAL_LN(F("e.g. set date YYYY-MM-DD"));
      SERIAL_LN(F("e.g. set nodeid [0..250]"));
      SERIAL_LN(F("e.g. set base [0|1]"));
      SERIAL_LN(F("     , to enable or disable base network"));
      SERIAL_LN(F("e.g. set maxebn <duration>"));
      SERIAL_LN(F("     , to set maximum base network enable duration"));
      SERIAL_LN(F("e.g. set spkr [0|1]"));
      SERIAL_LN(F("     , to enable or disable speaker"));
      SERIAL_LN(F("set flag <flag name> [0|1]"));
      SERIAL_LN(F("     , to set system flag value, use '? set flag' for detail"));
      SERIAL_LN(F("set var <var name> <value>"));
      SERIAL_LN(F("     , to set value of variable, use '? set var' for detail"));
      SERIAL_LN(F("e.g. set debug [log:level]"));
      SERIAL_LN(F("     , where log is [serial|flash|syslog|cloud|all"));
      SERIAL_LN(F("     and level is [none|alter|critical|error|warn|notice|info|debug]\n\r"));
      CloudOutput(F("set tz|dst|nodeid|base|spkr|flag|var|debug"));
    }
  } else if(strTopic.equals("sys")) {
    SERIAL_LN(F("--- Command: sys <mode> ---"));
    SERIAL_LN(F("To control the system status, where <mode> could be:"));
    SERIAL_LN(F("   base <duration>: switch to base network and accept new device for <duration=60> seconds"));
    SERIAL_LN(F("   private: switch to private network"));
    SERIAL_LN(F("   reset:   reset the system"));
    SERIAL_LN(F("   safe:    enter safe/recover mode"));
    SERIAL_LN(F("   setup:   setup Wi-Fi crediential"));
    SERIAL_LN(F("   dfu:     enter DFU mode"));
    SERIAL_LN(F("   update:  update firmware"));
    SERIAL_LN(F("   serial reset: reset serial port"));
    SERIAL_LN(F("   sync <object>: object synchronize with Cloud"));
    SERIAL_LN(F("   clear <object>: clear object, such as nodeid"));
    SERIAL_LN(F("e.g. sys sync time"));
    SERIAL_LN(F("e.g. sys clear nodeid 1"));
    SERIAL_LN(F("e.g. sys reset\n\r"));
    CloudOutput(F("sys base|private|reset|safe|setup|dfu|update|serial|sync|clear"));
  } else {
    SERIAL_LN(F("Available Commands:"));
    SERIAL_LN(F("    check, show, ping, do, test, send, set, sys, help or ?"));
    SERIAL_LN(F("Use 'help <command>' for more information\n\r"));
    CloudOutput(F("check, show, ping, do, test, send, set, sys, help or ?"));
  }

  return true;
}

bool SerialConsoleClass::doHelp(const char *cmd)
{
  IF_SERIAL_DEBUG(SERIAL_LN(F("doHelp(%s)\n\r"), cmd));

  String strTopic = "";
  char *sTopic = next();
  if( sTopic ) {
    // Get Menu Position according to Topic
    strTopic = sTopic;
  } else if(cmd) {
    strTopic = cmd;
  } else {
    sTopic = first();
    if( sTopic )
      strTopic = sTopic;
  }
  strTopic.toLowerCase();

  return showThisHelp(strTopic);
}

// check - Check commands
bool SerialConsoleClass::doCheck(const char *cmd)
{
  IF_SERIAL_DEBUG(SERIAL_LN(F("doCheck(%s)\n\r"), cmd));

  bool retVal = true;

  char *sTopic = next();
  if( sTopic ) {
    if (strnicmp(sTopic, "rf", 2) == 0) {
      SERIAL_LN("**RF module is %s\n\r", (theRadio.isValid() ? "available" : "not available!"));
      CloudOutput("RF module is %s", (theRadio.isValid() ? "available" : "not available!"));
    } else if (strnicmp(sTopic, "wifi", 4) == 0) {
        SERIAL("**Wi-Fi module is %s, ", (WiFi.ready() ? "ready" : "not ready!"));
        int lv_RSSI = WiFi.RSSI();
        if( lv_RSSI < 0 ) {
          SERIAL_LN("RSSI=%ddB\n\r", WiFi.RSSI());
        } else if( lv_RSSI == 1 ) {
          SERIAL_LN("Wi-Fi chip error\n\r");
        } else {
          SERIAL_LN("time-out\n\r");
        }
        CloudOutput("Wi-Fi module is %s, RSSI=%ddB", (WiFi.ready() ? "ready" : "not ready!"), lv_RSSI);
    } else if (strnicmp(sTopic, "wlan", 4) == 0) {
        SERIAL_LN("**Resolving IP for www.google.com...%s\n\r", (WiFi.resolve("www.google.com") ? "OK" : "failed!"));
        CloudOutput("WLAN is OK");
    } else if (strnicmp(sTopic, "flash", 5) == 0) {
        SERIAL_LN("** Free memory: %lu bytes, total EEPROM space: %lu bytes\n\r", System.freeMemory(), EEPROM.length());
        CloudOutput("Free memory: %lu bytes, total EEPROM space: %lu bytes", System.freeMemory(), EEPROM.length());
    } else {
      retVal = false;
    }
  } else {
    retVal = false;
  }

  return retVal;
}

// show - Show commands: config, status, variable, statistic data, etc.
bool SerialConsoleClass::doShow(const char *cmd)
{
  IF_SERIAL_DEBUG(SERIAL_LN(F("doShow(%s)\n\r"), cmd));

  bool retVal = true;
  char strDisplay[64];

  char *sTopic = next();
  if( sTopic ) {
    if (strnicmp(sTopic, "net", 3) == 0) {
      SERIAL_LN("** Network Summary **");
      SERIAL_LN("  Current RF NetworkID: %s", PrintUint64(strDisplay, theRadio.getCurrentNetworkID()));
      SERIAL_LN("  Private RF NetworkID: %s", PrintUint64(strDisplay, theRadio.getMyNetworkID()));
      SERIAL_LN("  Base RF Network is %s", (theRadio.isBaseNetworkEnabled() ? "enabled" : "disabled"));
      uint8_t mac[6];
      WiFi.macAddress(mac);
      SERIAL_LN("  MAC address: %s", PrintMacAddress(strDisplay, mac));
      if( WiFi.ready() ) {
        SERIAL("  IP Address: ");
        TheSerial.println(WiFi.localIP());
        SERIAL("  Subnet Mask: ");
        TheSerial.println(WiFi.subnetMask());
        SERIAL("  Gateway IP: ");
        TheSerial.println(WiFi.gatewayIP());
        SERIAL_LN("  SSID: %s", WiFi.SSID());
      }
      SERIAL_LN("");
      CloudOutput("RF NetworkID %s, MAC %s, SSID %s",
          PrintUint64(strDisplay, theRadio.getCurrentNetworkID()),
          PrintMacAddress(strDisplay, mac),
          WiFi.SSID());
	} else if (strnicmp(sTopic, "node", 4) == 0) {
      uint8_t lv_NodeID = theRadio.getAddress();
      SERIAL_LN("**NodeID: %d (%s), Status: %d", lv_NodeID, (lv_NodeID==GATEWAY_ADDRESS ? "Gateway" : (lv_NodeID==AUTO ? "AUTO" : "Node")), theSys.GetStatus());
      SERIAL_LN("  Present: %s, Token: %d", (theConfig.GetPresent() ? "Yes" : "No"), theConfig.GetToken());
      SERIAL_LN("  Product Info: %s-%s-%d", theConfig.GetOrganization().c_str(), theConfig.GetProductName().c_str(), theConfig.GetVersion());
      SERIAL_LN("  System Info: %s-%s\n\r", theSys.GetSysID().c_str(), theSys.GetSysVersion().c_str());
      CloudOutput("NodeID: %d (%s), Status: %d", lv_NodeID, (lv_NodeID==GATEWAY_ADDRESS ? "Gateway" : (lv_NodeID==AUTO ? "AUTO" : "Node")), theSys.GetStatus());
  } else if (strnicmp(sTopic, "dev", 3) == 0) {
      theConfig.print_devStatus();
	} else if (strnicmp(sTopic, "ble", 3) == 0) {
      // ToDo: show BLE summay
      SERIAL_LN("");
      CloudOutput("");
	} else if (strnicmp(sTopic, "rf", 2) == 0) {
      theRadio.PrintRFDetails();
      SERIAL_LN("");
	} else if (strnicmp(sTopic, "time", 4) == 0) {
      time_t time = Time.now();
      SERIAL_LN("Now is %s, %s\n\r", Time.format(time, TIME_FORMAT_ISO8601_FULL).c_str(), theSys.m_tzString.c_str());
      CloudOutput("Local time %s, %s", Time.format(time, TIME_FORMAT_ISO8601_FULL).c_str(), theSys.m_tzString.c_str());
	} else if (strnicmp(sTopic, "var", 3) == 0) {
		SERIAL_LN("theSys.m_lastMsg = \t\t\t%s", theSys.m_lastMsg.c_str());
    SERIAL_LN("");
  } else if (strnicmp(sTopic, "flag", 4) == 0) {
		SERIAL_LN("theSys.m_isRF = \t\t\t%s", (theSys.IsRFGood() ? "true" : "false"));
    SERIAL_LN("theConfig.enableSpeaker = \t\t%s", (theConfig.IsSpeakerEnabled() ? "true" : "false"));
    SERIAL_LN("theRadio._bBaseNetworkEnabled = \t%s", (theRadio.isBaseNetworkEnabled() ? "true" : "false"));
    SERIAL_LN("");
		SERIAL_LN("theConfig.m_isLoaded = \t\t\t%s", (theConfig.IsConfigLoaded() ? "true" : "false"));
		SERIAL_LN("theConfig.m_isChanged = \t\t%s", (theConfig.IsConfigChanged() ? "true" : "false"));
	} else if (strnicmp(sTopic, "version", 7) == 0) {
      SERIAL_LN("System version: %s\n\r", System.version().c_str());
      CloudOutput("System version: %s", System.version().c_str());
	} else {
      retVal = false;
    }
  } else {
    retVal = false;
  }

  return retVal;
}

// ping - ping IP address or hostname
bool SerialConsoleClass::doPing(const char *cmd)
{
  char *sIPaddress = next();
  PingAddress(sIPaddress);

  return true;
}

// do - execute action, e.g. turn on the lamp
bool SerialConsoleClass::doAction(const char *cmd)
{
  IF_SERIAL_DEBUG(SERIAL_LN(F("doAction(%s)\n\r"), cmd));

  bool retVal = false;

  char *sTopic = next();
  if( sTopic ) {
    if (strnicmp(sTopic, "on", 2) == 0) {
      // ToDo:
      SERIAL_LN("**Light is ON\n\r");
      retVal = true;
    } else if (strnicmp(sTopic, "off", 3) == 0) {
      // ToDo:
      SERIAL_LN("**Light is OFF\n\r");
      retVal = true;
    } else if (strnicmp(sTopic, "color", 5) == 0) {
      // ToDo:
      SERIAL_LN("**Color changed\n\r");
      retVal = true;
    }
  }

  return retVal;
}

// test - Test commands, e.g. ping
bool SerialConsoleClass::doTest(const char *cmd)
{
  IF_SERIAL_DEBUG(SERIAL_LN(F("doTest(%s)\n\r"), cmd));

  bool retVal = false;

  char *sTopic = next();
  if( sTopic ) {
    if (strnicmp(sTopic, "ping", 4) == 0) {
      char *sIPaddress = next();
      PingAddress(sIPaddress);
      retVal = true;
    } else if (strnicmp(sTopic, "send", 4) == 0) {
      char *sParam = next();
      if( strlen(sParam) >= 3 ) {
        String strMsg = sParam;
        theRadio.ProcessSend(strMsg);
        retVal = true;
      }
    } else if (strnicmp(sTopic, "asr", 3) == 0) {
      char *sParam = next();
      if( strlen(sParam) > 0 ) {
        theASR.sendCommand(atoi(sParam));
        retVal = true;
      }
    }
  }

  return retVal;
}

// send - Send message, shortcut of test send
bool SerialConsoleClass::doSend(const char *cmd)
{
  IF_SERIAL_DEBUG(SERIAL_LN(F("doSend(%s)\n\r"), cmd));

  bool retVal = false;

  char *sParam = next();
  if( strlen(sParam) >= 3 ) {
    String strMsg = sParam;
    theRadio.ProcessSend(strMsg);
    retVal = true;
  }

  return retVal;
}

// set - Config command
bool SerialConsoleClass::doSet(const char *cmd)
{
  IF_SERIAL_DEBUG(SERIAL_LN(F("doSet(%s)\n\r"), cmd));

  bool retVal = false;

  char *sTopic = next();
  char *sParam1, *sParam2;
  if( sTopic ) {
    if (strnicmp(sTopic, "tz", 2) == 0) {
      sParam1 = next();
      if( sParam1) {
        float fltTmp = atof(sParam1);
        if( theConfig.SetTimeZoneOffset((SHORT)(fltTmp * 60)) ) {
          SERIAL_LN("Set Time Zone to %f\n\r", fltTmp);
          CloudOutput("Set Time Zone to %f", fltTmp);
        } else {
          SERIAL_LN("Failed to SetTimeZone:%s\n\r", sParam1);
          CloudOutput("Failed to SetTimeZone:%s", sParam1);
        }
        retVal = true;
      }
    } else if (strnicmp(sTopic, "dst", 3) == 0) {
      sParam1 = next();
      if( atoi(sParam1) == 0 ) {
        theConfig.SetDaylightSaving(0);
        SERIAL_LN("Unset Daylight Saving Time\n\r");
        CloudOutput("Daylight Saving Time: 0");
      } else {
        theConfig.SetDaylightSaving(1);
        SERIAL_LN("Set Daylight Saving Time\n\r");
        CloudOutput("Daylight Saving Time: 1");
      }
      retVal = true;
    } else if (strnicmp(sTopic, "time", 4) == 0) {
      sParam1 = next();
      String strTemp = sParam1;
      retVal = (theSys.CldSetCurrentTime(strTemp) == 0);
    } else if (strnicmp(sTopic, "date", 4) == 0) {
      sParam1 = next();
      String strTemp = sParam1;
      retVal = (theSys.CldSetCurrentTime(strTemp) == 0);
    } else if (strnicmp(sTopic, "nodeid", 6) == 0) {
      sParam1 = next();
      if( sParam1) {
        uint8_t bNodeID = (uint8_t)(atoi(sParam1) % 256);
        retVal = theRadio.ChangeNodeID(bNodeID);
      }
    } else if (strnicmp(sTopic, "base", 4) == 0) {
      sParam1 = next();
      if( sParam1) {
        theRadio.enableBaseNetwork(atoi(sParam1) > 0);
        SERIAL_LN("Base RF network is %s\n\r", (theRadio.isBaseNetworkEnabled() ? "enabled" : "disabled"));
        CloudOutput("Base RF network is %s", (theRadio.isBaseNetworkEnabled() ? "enabled" : "disabled"));
        retVal = true;
      }
    } else if (strnicmp(sTopic, "spkr", 4) == 0) {
      // Enable or disable speaker
      sParam1 = next();   // Get speaker flag
      if( sParam1) {
        theConfig.SetSpeakerEnabled(atoi(sParam1) > 0);
        SERIAL_LN("Speaker is %s\n\r", (theConfig.IsSpeakerEnabled() ? "enabled" : "disabled"));
        CloudOutput("Speaker is %s", (theConfig.IsSpeakerEnabled() ? "enabled" : "disabled"));
        retVal = true;
      } else {
        SERIAL_LN("Require spkr flag value [0|1], use '? set spkr' for detail\n\r");
        retVal = true;
      }
    }
  }

  return retVal;
}

// sys - System control
bool SerialConsoleClass::doSys(const char *cmd)
{
  IF_SERIAL_DEBUG(SERIAL_LN(F("doSys(%s)\n\r"), cmd));

  // Interpret deeply
  return scanStateMachine();
}

bool SerialConsoleClass::doSysSub(const char *cmd)
{
  IF_SERIAL_DEBUG(SERIAL_LN(F("doSysSub(%s)"), cmd));

  char strDisplay[64];
  const char *sTopic = CommandList[currentCommand].event;
  char *sParam1;
  if( sTopic ) {
    if (strnicmp(sTopic, "reset", 5) == 0) {
      SERIAL_LN(F("System is about to reset..."));
      CloudOutput("System is about to reset");
      theSys.Restart();
    }
    else if (strnicmp(sTopic, "safe", 4) == 0) {
      SERIAL_LN(F("System is about to enter safe mode..."));
      CloudOutput("System is about to enter safe mod");
      delay(1000);
      System.enterSafeMode();
    }
    else if (strnicmp(sTopic, "dfu", 3) == 0) {
      SERIAL_LN(F("System is about to enter DFU mode..."));
      CloudOutput("System is about to enter DFU mode");
      delay(1000);
      System.dfu();
    }
    else if (strnicmp(sTopic, "update", 6) == 0) {
      // ToDo: to OTA
    }
    else if (strnicmp(sTopic, "serial", 6) == 0) {
      sParam1 = next();
      if(sParam1) {
        if( stricmp(sParam1, "reset") == 0 ) {
          theSys.ResetSerialPort();
        } else {
          return false;
        }
      } else {
        SERIAL_LN("Serial speed:%d\r\n", SERIALPORT_SPEED_DEFAULT);
        CloudOutput("Serial speed:%d", SERIALPORT_SPEED_DEFAULT);
      }
    }
    else if (strnicmp(sTopic, "sync", 4) == 0) {
      sParam1 = next();
      if(sParam1) {
        if( stricmp(sParam1, "time") == 0 ) {
          theSys.CldSetCurrentTime();
        } else {
          // ToDo: other synchronization
        }
      } else {
        return false;
      }
    }
    else if (strnicmp(sTopic, "clear", 5) == 0) {
      sParam1 = next();
      if(sParam1) {
      } else {
        return false;
      }
    }
    else if (strnicmp(sTopic, "base", 4) == 0) {
      // Switch to Base Network
      theRadio.switch2BaseNetwork();
      SERIAL_LN(F("Switched to base network\n\r"));
      CloudOutput(F("Switched to base network"));
    }
    else if (strnicmp(sTopic, "private", 7) == 0) {
      // Switch to Private Network
      theRadio.switch2MyNetwork();
      SERIAL_LN(F("Switched to private network: %s\n\r"), PrintUint64(strDisplay, theRadio.getCurrentNetworkID()));
      CloudOutput(F("Switched to private network"));
    }
  } else { return false; }

  return true;
}

//--------------------------------------------------
// Support Functions
//--------------------------------------------------
// Get WI-Fi crediential from serial Port
bool SerialConsoleClass::SetupWiFi(const char *cmd)
{
  String sText;
  switch( (consoleState_t)currentState ) {
  case consoleWF_YesNo:
    // Confirm menu choice
    sText = "Sure to setup Wi-Fi crediential? (y/N)";
    SERIAL_LN(sText.c_str());
    CloudOutput(sText.c_str());
    break;

  case consoleWF_GetSSID:
    sText = "Please enter SSID";
    SERIAL_LN("%s [%s]:", sText.c_str(), gstrWiFi_SSID.c_str());
    CloudOutput(sText.c_str());
    break;

  case consoleWF_GetPassword:
    // Record SSID
    if( strlen(cmd) > 0 )
      gstrWiFi_SSID = cmd;
    sText = "Please enter PASSWORD";
    SERIAL_LN("%s:", sText.c_str());
    CloudOutput(sText.c_str());
    break;

  case consoleWF_GetAUTH:
    // Record Password
    if( strlen(cmd) > 0 )
      gstrWiFi_Password = cmd;
    SERIAL_LN("Please select authentication method: [%d]", gintWiFi_Auth);
    SERIAL_LN("  0. %s", strAuthMethods[0]);
    SERIAL_LN("  1. %s", strAuthMethods[1]);
    SERIAL_LN("  2. %s", strAuthMethods[2]);
    SERIAL_LN("  3. %s", strAuthMethods[3]);
    SERIAL_LN("  (q)uit");
    CloudOutput("Select authentication method [0..3]");
    break;

  case consoleWF_Confirm:
    // Record authentication method
    if( strlen(cmd) > 0 )
      gintWiFi_Auth = atoi(cmd) % 4;
    SERIAL_LN("Are you sure to apply the Wi-Fi credential? (y/N)");
    SERIAL_LN("  SSID: %s", gstrWiFi_SSID.c_str());
    SERIAL_LN("  Password: ******");
    SERIAL_LN("  Authentication: %s", strAuthMethods[gintWiFi_Auth]);
    CloudOutput("Sure to apply the Wi-Fi credential? (y/N)");
    break;
  }

  return true;
}

bool SerialConsoleClass::SetWiFiCredential(const char *cmd)
{
  WiFi.listen();
  if( gintWiFi_Auth = 0 ) {
    WiFi.setCredentials(gstrWiFi_SSID);
  } else if( gintWiFi_Auth = 1 ) {
    WiFi.setCredentials(gstrWiFi_SSID, gstrWiFi_Password, WPA2, WLAN_CIPHER_AES);
  } else if( gintWiFi_Auth = 2 ) {
    WiFi.setCredentials(gstrWiFi_SSID, gstrWiFi_Password, WEP);
  } else if( gintWiFi_Auth = 3 ) {
    WiFi.setCredentials(gstrWiFi_SSID, gstrWiFi_Password, WPA, WLAN_CIPHER_TKIP);
  }
  SERIAL("Wi-Fi credential saved...reconnecting...");
  CloudOutput("Wi-Fi credential saved...reconnecting");
  WiFi.connect();
  SERIAL_LN("%s", (WiFi.ready() ? "OK" : "Failed"));

  return true;
}

bool SerialConsoleClass::PingAddress(const char *sAddress)
{
  if( !WiFi.ready() ) {
    SERIAL_LN("Wi-Fi is not ready!");
    CloudOutput("Wi-Fi is not ready");
    return false;
  }

  // Get IP address
  IPAddress ipAddr;
  if( !String2IP(sAddress, ipAddr) )
    return false;

  // Ping 4 times
  int pingStartTime = millis();
  SERIAL("Pinging %s (", sAddress);
  TheSerial.print(ipAddr);
  SERIAL(")...");
  int myByteCount = WiFi.ping(ipAddr, 3);
  int elapsedTime = millis() - pingStartTime;
  SERIAL_LN("received %d bytes over %d ms", myByteCount, elapsedTime);
  CloudOutput("Pinging %s recieved %d", sAddress, myByteCount);

  return true;
}

bool SerialConsoleClass::String2IP(const char *sAddress, IPAddress &ipAddr)
{
  int lv_len = strlen(sAddress);
  bool bHost = false;
  ipAddr = IPAddress(0UL);      // NULL IP
  int i;

  // Blank -> google
  if( lv_len == 0 ) {
    ipAddr = IPAddress(8,8,8,8);
    return true;
  }

  // IP address or hostname
  for( i=0; i<lv_len; i++ ) {
    if( (sAddress[i] >= '0' && sAddress[i] <= '9') || sAddress[i] == '.' ) {
      continue;
    }
    bHost = true;
    break;
  }

  // Resole IP from hostname
  if( bHost ) {
    ipAddr = WiFi.resolve(sAddress);
    //IF_SERIAL_DEBUG(SERIAL("Resoled %s is ", sAddress));
    //IF_SERIAL_DEBUG(TheSerial.println(ipAddr));
  } else if( lv_len > 6 ) {
    // Split IP segments
    UC ipSeg[4];
    String sTemp = sAddress;
    int nPos = 0, nPos2;
    for( i = 0; i < 3; i++ ) {
      nPos2 = sTemp.indexOf('.', nPos);
      if( lv_len > 0 ) {
        ipSeg[i] = (UC)(sTemp.substring(nPos, nPos2).toInt());
        nPos = nPos2 + 1;
      } else {
        nPos = 0;
        break;
      }
    }
    if( nPos > 0 && nPos < lv_len ) {
      ipSeg[i] = (UC)(sTemp.substring(nPos).toInt());
      for( i = 0; i < 4; i++ ) {
        ipAddr[i] = ipSeg[i];
      }
    } else {
      return false;
    }
  } else {
    return false;
  }

  return true;
}

// Simulate to run a command string
bool SerialConsoleClass::ExecuteCloudCommand(const char *cmd)
{
  clearBuffer();
  setCommandBuffer(cmd);
  isInCloudCommand = true;
  bool rc = scanStateMachine();
  isInCloudCommand = false;
  return rc;
}

// Output concise result to the cloud
void SerialConsoleClass::CloudOutput(const char *msg, ...)
{
  if( !isInCloudCommand ) return;

  int nSize = 0;
  char buf[512];

  // Prepare message
  va_list args;
  va_start(args, msg);
  nSize = vsnprintf(buf, 512, msg, args);
  buf[nSize] = NULL;

  // Set message
  theSys.m_lastMsg = buf;
}
