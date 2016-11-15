//  xlSmartRemote.h - Xlight xlSmartRemote project scale definitions header

#ifndef xlSmartRemote_h
#define xlSmartRemote_h

#include "xliCommon.h"
#include "xlxConfig.h"
#include "MyMessage.h"
#include "ArduinoJson.h"

//------------------------------------------------------------------
// Smart Remote Class
//------------------------------------------------------------------
class SmartRemoteClass
{
private:
  BOOL m_isRF;

public:
  String m_SysID;
  String m_SysVersion;    // firmware version
  int m_devStatus;
  String m_tzString;
  String m_lastMsg;

  SmartRemoteClass();
  void Init();
  void InitRadio();

  BOOL Start();
  UC GetStatus();
  BOOL SetStatus(UC st);
  void ResetSerialPort();
  void Restart();
  void ResumeRFNetwork();

  String GetSysID();
  String GetSysVersion();

  BOOL CheckRF();
  BOOL SelfCheck(US ms);
  BOOL IsRFGood();

  // Process all kinds of commands
  void ProcessCommands();

  int CldSetCurrentTime(String tmStr = "");

  // Device Control Functions
  int DevSoftSwitch(BOOL sw, UC dev = 0);

  // RF message sending shortcuts
  BOOL SendDeviceRegister();
  BOOL SendDevicePresentation();
  BOOL SendReqLampStatus(UC dev = 0);
  BOOL SendChangeBrightness(UC _br, UC dev = 0);
  BOOL SendChangeCCT(US _cct, UC dev = 0);
  BOOL SendChangeBR_CCT(UC _br, US _cct, UC dev = 0);

  // Utils
  void Array2Hue(JsonArray& data, Hue_t& hue);     // Copy JSON array to Hue structure
};

//------------------------------------------------------------------
// Function & Class Helper
//------------------------------------------------------------------
extern SmartRemoteClass theSys;

#endif /* xlSmartRemote_h */
