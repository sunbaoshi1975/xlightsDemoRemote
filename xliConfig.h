//  xliConfig.h - Xlight SmartRemote configuration header

#ifndef xliConfig_h
#define xliConfig_h

/*** USER DEFINES:  ***/
#define FAILURE_HANDLING
//#define SYS_SERIAL_DEBUG
//#define SERIAL_DEBUG
//#define MAINLOOP_TIMER

/**********************/

#define PRIPSTR "%s"
#define pgm_read_word(p) (*(p))
#ifdef SERIAL_DEBUG
  #define IF_SERIAL_DEBUG(x) ({x;})
#else
  #define IF_SERIAL_DEBUG(x)
#endif

#ifdef MAINLOOP_TIMER
  #define IF_MAINLOOP_TIMER(x, name) ({unsigned long ulStart = millis(); x; SERIAL_LN("%s spent %lu ms", (name), millis() - ulStart);})
#else
  #define IF_MAINLOOP_TIMER(x, name) ({x;})
#endif

// Xlight Application Identification
#define XLA_ORGANIZATION          "xlight.ca"               // Default value. Read from EEPROM
#define XLA_PRODUCT_NAME          "XRemote"                 // Default value. Read from EEPROM

//------------------------------------------------------------------
// System level working constants
//------------------------------------------------------------------
// Running Time Environment Parameters
#define RTE_DELAY_PUBLISH         60          // Maximum publish data refresh time in seconds
#define RTE_DELAY_SYSTIMER        50          // System Timer interval, can be very fast, e.g. 50 means 25ms
#define RTE_DELAY_SELFCHECK       100         // Self-check interval
#define RTE_WIFI_CONN_TIMEOUT     20000       // Timeout for attempting to connect WIFI
#define RTE_CLOUD_CONN_TIMEOUT    3000        // Timeout for connecting to the Cloud

// Number of ticks on System Timer
#define RTE_TICK_FASTPROCESS			1						// Pace of execution of FastProcess

// Maximum number of rows for any working memory table implimented using ChainClass
#define MAX_TABLE_SIZE    8

// Change it only if Config_t structure is updated
#define VERSION_CONFIG_DATA         1

// Maximum number of device associated to one controller
#define MAX_DEVICE_PER_CONTROLLER   16

// Maximum number of nodes under one controller
#define MAX_NODE_PER_CONTROLLER   64

// Default value for maxBaseNetworkDuration (in seconds)
#define MAX_BASE_NETWORK_DUR    180

// Maximum JSON data length
#define COMMAND_JSON_SIZE				64
#define SENSORDATA_JSON_SIZE			196

// NodeID Convention
#define NODEID_GATEWAY          0
#define NODEID_MAINDEVICE       1
#define NODEID_MIN_DEVCIE       8
#define NODEID_MAX_DEVCIE       63
#define NODEID_MIN_REMOTE       64
#define NODEID_MAX_REMOTE       127
#define NODEID_DUMMY            255

#define PIN_RF24_CE		   		      A0
#define PIN_RF24_CS		   	 	      A2

#define CT_MIN_VALUE            2700
#define CT_MAX_VALUE            6500
#define CT_SCOPE                38
#define CT_STEP                 ((CT_MAX_VALUE-CT_MIN_VALUE)/10)

// Button behavior
#define BTN_STEP_PAGES          5
#define BTN_STEP_SHORT_BR       10
#define BTN_STEP_SHORT_CCT      300
#define BTN_STEP_LONG_BR        25
#define BTN_STEP_LONG_CCT       800
#define BTN_BR_LOW              10
#define BTN_FN1_BR              100
#define BTN_FN1_CCT             2700
#define BTN_FN2_BR              25
#define BTN_FN2_CCT             3000
#define BTN_FN3_BR              85
#define BTN_FN3_CCT             5000
#define BTN_FN4_BR              100
#define BTN_FN4_CCT             6000

#endif /* xliConfig_h */
