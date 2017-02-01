/*
This is a demo of xlight smart remote control. It demotrates device initializtion and
send control commands to SmartController via RF.

Users can send message in Serial console. Please connect a serial port with speed 115200 bps.
And type ? or help for instructions.

It also demotrate an ASR module.

*/

#include "application.h"
#include "xliCommon.h"
#include "xlxConfig.h"
#include "xlSmartRemote.h"
#include "xlxSerialConsole.h"

// Set "manual" mode
SYSTEM_MODE(MANUAL);
SYSTEM_THREAD(ENABLED);

void setup()
{
  WiFi.on();

  // System Initialization
  theSys.Init();

  // Load Configuration
  theConfig.LoadConfig();

  WiFi.listen(false);
  while(1) {
    if( !WiFi.hasCredentials() || !theConfig.GetWiFiStatus() ) {
      if( !theSys.connectWiFi() ) {
        // get credential from BLE or Serial
        SERIAL_LN(F("will enter listening mode"));
        WiFi.listen();
        break;
      }
    }

    // Connect to Wi-Fi
    if( theSys.connectWiFi() ) {
      if( theConfig.GetUseCloud() == CLOUD_DISABLE ) {
        Particle.disconnect();
      } else {
        // Connect to the Cloud
        if( !theSys.connectCloud() ) {
          if( theConfig.GetUseCloud() == CLOUD_MUST_CONNECT ) {
            // Must connect to the Cloud
            continue;
          }
        }
      }
    } else {
      if( theConfig.GetUseCloud() == CLOUD_MUST_CONNECT ) {
        // Must have network
        continue;
      }
    }
    break;
  }

  // Initialization Radio Interfaces
  theSys.InitRadio();

  // Initialization network Interfaces
  theSys.InitNetwork();

	// Wait the system started
  if( Particle.connected() == true ) {
  	while( millis() < 2000 ) {
  		Particle.process();
  	}
  }

	// Initialize Serial Console
  theConsole.Init();

  // System Starts
  theSys.Start();
}

// Notes: approximate RTE_DELAY_SELFCHECK ms per loop
/// If you need more accurate and faster timer, do it with sysTimer
void loop()
{
  static UC tick = 0;

  // Process commands
  IF_MAINLOOP_TIMER( theSys.ProcessCommands(), "ProcessCommands" );

  // Self-test & alarm trigger, also insert delay between each loop
  IF_MAINLOOP_TIMER( theSys.SelfCheck(RTE_DELAY_SELFCHECK), "SelfCheck" );

  // Process Could Messages
  if( Particle.connected() == true ) {
    IF_MAINLOOP_TIMER( Particle.process(), "ProcessCloud" );
  }
}
