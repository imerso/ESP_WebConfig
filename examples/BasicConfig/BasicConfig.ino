// =================================== ======= === = == =  =  = -- - -
// Basic Web Config example
//
// Written by Vander 'imerso' Nunes | imersiva.com
// ======================================= ======= === == == =  =  = -- - -

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WebConfig.h>

WebConfig* pWebConfig;


// Initialize the system
void setup()
{
  pWebConfig = new WebConfig("BASIC WEBCONFIG v1.0", "ESP8266", "8266", false);
}


// Serve HTTP configuration interface
void loop()
{
  pWebConfig->ProcessHTTP();
  yield();
}

