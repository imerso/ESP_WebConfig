// =================================== ======= === = == =  =  = -- - -
// ESP8266 Operation Mode Configurator v1.0 - August 2015
//
// With this class you can easily have a generic web configurator for
// the transceiver, where you can change between Access Point and Station
// modes at any time without needing to rewire the module.
//
// Even when it is configured to work as a Station, if it fails to connect
// to the wifi router it will fallback automatically to AP mode, so you
// still can access your module from the web.
//
// On the very first run, it will start in AP mode on 192.168.0.1
// From there, you can use any browser to configure the module.
//
// This code is to be compiled with Arduino IDE + ESP8266 Board Extension.
//
// Written by Vander 'imerso' Nunes | imersiva.com
// ======================================= ======= === == == =  =  = -- - -

#ifndef _WEBCONFIG_H_
#define _WEBCONFIG_H_

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>


class WebConfig
{
	public:

		// Constructor without initialization
		WebConfig();

		// Constructor with initialization
		// appName   : your application name
		// defAPName : default AP name, in case of falling back to Access Point mode
		// defAPPass : default AP password, in case of falling back to Access Point mode
		// doReset   : if your application wants to clear EEPROM settings, ask for doReset
		WebConfig(const char* appName, const char* defAPName, const char* defAPPass, bool doReset);

		// Initialize the configurator
		// appName   : your application name
		// defAPName : default AP name, in case of falling back to Access Point mode
		// defAPPass : default AP password, in case of falling back to Access Point mode
		// doReset   : if your application wants to clear EEPROM settings, ask for doReset
		void Init(const char* appName, const char* defAPName, const char* defAPPass, bool doReset);

		// Destructor
		~WebConfig();

		// Process HTTP requests -- just call it inside the loop() function
		void ProcessHTTP();

		// Tell if running in AP mode
		bool IsAP() { return isAP; }

		// Return pointers to the listening UDP/TCP servers
		WiFiUDP* UDP() { return pUdp; }
		WiFiServer* TCP() { return pTcp; }

	private:

		// Application Name
		// Shown on the webserver interface.
		char name[32];
		char webLogin[16];
		char webPassword[16];
		char base64Auth[64];
		int webPort;

		// When it is in AP mode, it will not connect to any router.
		// On the very first run, it will start in AP mode so we can connect and configure it.
		bool isAP;
		long startMillis;

		// AP info in case it's in AP mode.
		char apName[32];
		char apPassword[32];
		byte apChannel;

		// Router info in case it's not in AP mode.
		char ssid[32];
		char password[32];

		// Ports different than zero will be listening for connections and packets
		int udpPort;
		int tcpPort;

		// Servers (only instatiated when their correspondent ports are not zero)
		WiFiUDP* pUdp;
		WiFiServer* pTcp;

		// http server for the configuration interface - always available
		WiFiServer* pHttpServer;

		// Read string from EEPROM
		bool ReadString(char* pString, short pos, short len);

		// Write string to EEPROM
		bool WriteString(char* pString, short pos, short len);

		// Load settings from EEPROM
		bool LoadSettings();

		// Save settings to EEPROM
		bool SaveSettings();

		// Start AP Mode
		bool StartAPMode();

		// Extract next substring from current position to next separator or end of string
		char* Token(char** req, const char* sep);

		// Parse settings values in case the HTTP request includes them.
		// Format is:
		// /?(webPort)&(webLogin)&(webPassword)&(base64Auth)&(isAP)&(ap_name)&(ap_password)&(ap_channel)&(ssid)&(password)&(udpport)&(tcpport)
		void ProcessParms(String req);
};

#endif

