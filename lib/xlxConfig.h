//  xlxConfig.h - Xlight Configuration Reader & Writer

#ifndef xlxConfig_h
#define xlxConfig_h

#include "xliCommon.h"
#include "xliMemoryMap.h"

#define PACK //MSVS intellisense doesn't work when structs are packed
//------------------------------------------------------------------
// Xlight Configuration Data Structures
//------------------------------------------------------------------
typedef struct
{
  US id;                                    // timezone id
  SHORT offset;                             // offser in minutes
  UC dst                      :1;           // daylight saving time flag
} Timezone_t;

typedef struct
#ifdef PACK
	__attribute__((packed))
#endif
{
  UC State                    :1;           // Component state
  UC BR                       :7;           // Brightness of white [0..100]
  US CCT                      :16;          // CCT (warm or cold) [2700..6500]
  UC R                        :8;           // Brightness of red
  UC G                        :8;           // Brightness of green
  UC B                        :8;           // Brightness of blue
} Hue_t;

typedef struct
{
  UC version                  :8;           // Data version, other than 0xFF
  UC indBrightness            :4;           // Indicator of brightness
  UC nodeID;                                // Remote Node ID
  uint64_t networkID;
  US token;
  Timezone_t timeZone;                      // Time zone
  char Organization[24];                    // Organization name
  char ProductName[24];                     // Product name
  BOOL enableDailyTimeSync    :1;           // Whether enable daily time synchronization
  BOOL enableSpeaker          :1;           // Whether enable speaker
  BOOL Reserved_bool          :5;           // Reserved for boolean flags
  UC rfPowerLevel             :2;           // RF Power Level 0..3
  BOOL stWiFi                 :1;           // Wi-Fi status: On / Off
  UC Reserved1                :5;           // Reserved bits
  UC useCloud;                              // How to depend on the Cloud
} Config_t;

//------------------------------------------------------------------
// Xlight Device Status Table Structures
//------------------------------------------------------------------
typedef struct
#ifdef PACK
	__attribute__((packed))
#endif
{
  UC uid;						   // required
  UC node_id;          // RF nodeID
  UC present              :1;  // 0 - not present; 1 - present
  UC reserved             :7;
  UC type;                         // Type of lamp
  Hue_t ring[MAX_RING_NUM];
} DevStatusRow_t;

#define DST_ROW_SIZE sizeof(DevStatusRow_t)

//------------------------------------------------------------------
// Xlight Configuration Class
//------------------------------------------------------------------
class ConfigClass
{
private:
  BOOL m_isLoaded;
  BOOL m_isChanged;         // Config Change Flag
  BOOL m_isDSTChanged;      // Device Status Table Change Flag
  UL m_lastTimeSync;

  Config_t m_config;
  DevStatusRow_t m_devStatus;

  void UpdateTimeZone();
  void DoTimeSync();

public:
  ConfigClass();
  void InitConfig();
  void InitDevStatus(UC nodeID);

  BOOL LoadConfig();
  BOOL SaveConfig();
  BOOL IsConfigLoaded();

  BOOL LoadDeviceStatus();
  BOOL SaveDeviceStatus();
  void print_devStatus();

  BOOL IsConfigChanged();
  void SetConfigChanged(BOOL flag);

  BOOL IsDSTChanged();
  void SetDSTChanged(BOOL flag);

  UC GetVersion();
  BOOL SetVersion(UC ver);

  US GetTimeZoneID();
  BOOL SetTimeZoneID(US tz);

  UC GetDaylightSaving();
  BOOL SetDaylightSaving(UC flag);

  SHORT GetTimeZoneOffset();
  SHORT GetTimeZoneDSTOffset();
  BOOL SetTimeZoneOffset(SHORT offset);
  String GetTimeZoneJSON();

  String GetOrganization();
  void SetOrganization(const char *strName);

  String GetProductName();
  void SetProductName(const char *strName);

  UC GetNodeID();
  void SetNodeID(UC _nodeID);

  uint64_t GetNetworkID();
  void SetNetworkID(uint64_t _netID);

  US GetToken();
  BOOL GetPresent();
  void SetToken(US _token);

  BOOL IsDailyTimeSyncEnabled();
  void SetDailyTimeSyncEnabled(BOOL sw = true);
  BOOL CloudTimeSync(BOOL _force = true);

  BOOL IsSpeakerEnabled();
  void SetSpeakerEnabled(BOOL sw = true);

  UC GetBrightIndicator();
  BOOL SetBrightIndicator(UC level);

  UC GetRFPowerLevel();
  BOOL SetRFPowerLevel(UC level);

  UC GetUseCloud();
  BOOL SetUseCloud(UC opt);

  BOOL GetWiFiStatus();
  BOOL SetWiFiStatus(BOOL _st);

  // DevStatusRow_t interfaces
  BOOL GetDevPresent();
  void SetDevPresent(BOOL _present);

  UC GetDevType();
  void SetDevType(UC _type);

  BOOL GetDevStatus();
  void SetDevStatus(BOOL _status);

  UC GetDevBrightness();
  void SetDevBrightness(UC _level);

  US GetDevCCT();
  void SetDevCCT(US _cct);

  String hue_to_string(Hue_t hue);
};

//------------------------------------------------------------------
// Function & Class Helper
//------------------------------------------------------------------
extern ConfigClass theConfig;

#endif /* xlxConfig_h */
