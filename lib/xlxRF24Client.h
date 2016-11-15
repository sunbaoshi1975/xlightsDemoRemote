//  xlxRF24Client.h - Xlight RF2.4 client library

#ifndef xlxRF24Client_h
#define xlxRF24Client_h

#include "MyTransportNRF24.h"

// RF24 Server class
class RF24ClientClass : public MyTransportNRF24
{
public:
  RF24ClientClass(uint8_t ce=RF24_CE_PIN, uint8_t cs=RF24_CS_PIN, uint8_t paLevel=RF24_PA_LEVEL_NODE);

  bool ClientBegin(const uint8_t bNodeID = AUTO);
  uint64_t GetNetworkID(bool _full = false);
  bool ChangeNodeID(const uint8_t bNodeID);
  bool ProcessSend(String &strMsg, MyMessage &my_msg);
  bool ProcessSend(String &strMsg); //overloaded
  bool ProcessSend(MyMessage *pMsg = NULL);
  bool ProcessReceive();

  unsigned long _times;
  unsigned long _succ;
  unsigned long _received;
};

//------------------------------------------------------------------
// Function & Class Helper
//------------------------------------------------------------------
extern RF24ClientClass theRadio;

#endif /* xlxRF24Client_h */
