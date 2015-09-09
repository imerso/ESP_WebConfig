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

#include <WebConfig.h>


// Define a software reset function
// to restart when settings are changed
void(*Reset) (void) = 0;


// Constructor without initialization
WebConfig::WebConfig()
{
}


// Constructor with initialization
WebConfig::WebConfig(const char* appName, const char* defAPName, const char* defAPPass, bool doReset)
{
	Init(appName, defAPName, defAPPass, doReset);
}


// Initialize the configurator
// defName  : default AP name, in case of falling back to Access Point mode
// defPass  : default AP password, in case of falling back to Access Point mode
// httpPort : port to always listen to http access for configuration
void WebConfig::Init(const char* appName, const char* defAPName, const char* defAPPass, bool doReset)
{
	// update the application name
	strncpy(name, appName, 32);
	startMillis = millis();

	// try to load settings from the EEPROM;
	// if there are no settings, it fails or user configured to run in AP mode,
	// start the AP, else try to connect to the given router ssid/password.
	if (!LoadSettings() || doReset)
	{
		isAP = true;
		strncpy(apName, defAPName, 32);
		strncpy(apPassword, defAPPass, 32);
		apChannel = 10;
		webPort = 0;
		webLogin[0] = 0;
		webPassword[0] = 0;
		base64Auth[0] = 0;
		ssid[0] = 0;
		password[0] = 0;
		udpPort = 0;
		tcpPort = 0;
		if (doReset) SaveSettings();
	}

	if (isAP)
	{
		// settings do not exist, failed or
		// the user configured to run in AP mode anyway.
		// start in AP mode
		StartAPMode();
	}
	else
	{
		// settings were loaded successfully,
		// and the user has setup to run as a router client,
		// so try to connect to the given router ssid/password.
		byte tries = 11;
		WiFi.begin(ssid, password);
		while (--tries && WiFi.status() != WL_CONNECTED)
		{
			delay(1000);
		}

		if (WiFi.status() != WL_CONNECTED)
		{
			// failed to connect to the router,
			// so force AP mode start
			StartAPMode();
		}
	}

	if (udpPort != 0)
	{
		pUdp = new WiFiUDP();
		pUdp->begin(udpPort);
	} else pUdp = NULL;

	if (tcpPort != 0)
	{
		pTcp = new WiFiServer(tcpPort);
		pTcp->begin();
	} else pTcp = NULL;

	// the web interface is always listening
	if (webPort <= 0 || webPort > 65535) webPort = 80;
	pHttpServer = new WiFiServer(webPort);
	pHttpServer->begin();
}


// Destructor
WebConfig::~WebConfig()
{
	if (pUdp) delete pUdp;
	if (pTcp) delete pTcp;
	delete pHttpServer;
}


// Process HTTP requests -- just call it inside the loop() function
void WebConfig::ProcessHTTP()
{
	// Accept any new web connection
	WiFiClient httpClient = pHttpServer->available();
	if (!httpClient) return;

	// Read the entire request
	String req = httpClient.readString();
	httpClient.flush();

	// after some time, do not open the web interface anymore.
	//if (millis() - startMillis > 2*60000) return;

	// response header
	String s;

	if (strlen(webLogin) > 0 || strlen(webPassword) > 0)
	{
		int authPos = req.indexOf("Authorization: Basic");

		if (authPos == -1)
		{
			// request authentication
			s = "HTTP/1.0 401 Authorization Required\r\nWWW-Authenticate: Basic realm=\"" + String(apName) + "\"\r\n\r\n";
			s += "<h1><b>ACCESS DENIED</b></h1>";
			httpClient.write(s.c_str(), s.length());
			httpClient.flush();
			return;
		}

		// there is authentication info, check it
		String authInfo = req.substring(authPos + 21);
		int endLinePos = authInfo.indexOf("\r");
		if (endLinePos == -1) { httpClient.print("Malformed request."); httpClient.stop(); return; }
		authInfo = authInfo.substring(0, endLinePos);
		//Serial.println(String("AUTH: ") + String(authInfo));
		//Serial.println(String("BASEAUTH: ") + String(base64Auth));
		if (strncmp(base64Auth, authInfo.c_str(), 64))
		{
			s = "<h1><b>ACCESS DENIED</b></h1>";
			httpClient.write(s.c_str(), s.length());
			httpClient.flush();
			return;
		}
	}

	// If there are parms, update variables and save settings
	ProcessParms(req);

	byte mac[6];
	WiFi.macAddress(mac);
	String m = String(mac[0],HEX) + ":" + String(mac[1],HEX) + ":" + String(mac[2],HEX) + ":" + String(mac[3],HEX) + ":" + String(mac[4],HEX) + ":" + String(mac[5],HEX);

	//
	// generate HTTP response
	//

	// authentication succeeded, proceed normally
	s = "HTTP/1.1 200 OK\r\n";
	s += "Content-Type: text/html\r\n\r\n";
	s += "<!DOCTYPE HTML>\r\n<html><body>\r\n";

	// javascript to save configuration
	s += "<script>\r\n";
	s += "function save()\r\n";
	s += "{\r\n";
	s += "var webPort = document.getElementById('web_port').value;\r\n";
	s += "var webLogin = document.getElementById('web_login').value;\r\n";
	s += "var webPassword = document.getElementById('web_pass').value;\r\n";
	s += "var webPassword2 = document.getElementById('web_pass2').value;\r\n";
	s += "var modeap = document.getElementById('modeap').checked;\r\n";
	s += "if (modeap) isAP = true; else isAP = false;\r\n";
	s += "var apName = document.getElementById('ap_ssid').value;\r\n";
	s += "var apPassword = document.getElementById('ap_pass').value;\r\n";
	s += "var apPassword2 = document.getElementById('ap_pass2').value;\r\n";
	s += "var apChannel = document.getElementById('apChannel').value;\r\n";
	s += "var ssid = document.getElementById('ssid').value;\r\n";
	s += "var password = document.getElementById('pass').value;\r\n";
	s += "var password2 = document.getElementById('pass2').value;\r\n";
	s += "var udpPort = document.getElementById('udpPort').value;\r\n";
	s += "var tcpPort = document.getElementById('tcpPort').value;\r\n";
	s += "if (webPassword != webPassword2) { alert('WEB passwords dont match'); return; }\r\n";
	s += "if (apPassword != apPassword2) { alert('AP passwords dont match'); return; }\r\n";
	s += "if (password != password2) { alert('Router passwords dont match'); return; }\r\n";
	s += "window.location.search=webPort + '&' + webLogin + '&' + webPassword + '&' + btoa(webLogin+':'+webPassword) + '&' + (isAP?'1':'0') + '&' + apName + '&' + apPassword + '&' + apChannel + '&' + ssid + '&' + password + '&' + udpPort + '&' + tcpPort;\r\n";
	s += "}\r\n";
	s += "</script>\r\n";

	// write first part of response
	httpClient.write(s.c_str(), s.length());

	// title and mac address
	s = "<b>" + String(name) + "</b><br>\r\n";
	s += "MAC: " + m + "<br>\r\n";

	// web interface configuration
	s += "<table border=1>\r\n";
	s += "<tr><td colspan=2 bgcolor=#E0E0E0><b>WEB INTERFACE</b></td></tr>\r\n";
	s += "<tr><td>Port</td><td><input type=text id='web_port' value='" + String(webPort) + "'></td></tr>\r\n";
	s += "<tr><td>Login</td><td><input type=text id='web_login' value='" + String(webLogin) + "'></td></tr>\r\n";
	s += "<tr><td>Password</td><td><input type=password id='web_pass' value='" + String(webPassword) + "'></td></tr>\r\n";
	s += "<tr><td>Pass Confirm</td><td><input type=password id='web_pass2' value='" + String(webPassword) + "'></td></tr>\r\n";
	s += "</table>\r\n";

	// ap configuration
	s += "<table border=1>\r\n";
	s += "<tr><td colspan=2 bgcolor=#E0E0E0><b>ACCESS POINT</b></td></tr>\r\n";
	s += "<tr><td>Mode</td><td><input type=radio id='modeap' name='mode' value='ap'" + (isAP?String(" checked"):String("")) + ">Access Point</td></tr>\r\n";
	s += "<tr><td>Channel</td><td><select id='apChannel'>";
	for (byte c=1; c<14; c++) s += "<option value='" + String(c) + "'" + (c==apChannel?String(" selected"):String("")) + ">" + String(c) + "</option>";
	s += "</select></td></tr>\r\n";
	s += "<tr><td>SSID</td><td><input type=text id='ap_ssid' value='" + String(apName) + "'></td></tr>\r\n";
	s += "<tr><td>Password</td><td><input type=password id='ap_pass' value='" + String(apPassword) + "'></td></tr>\r\n";
	s += "<tr><td>Pass Confirm</td><td><input type=password id='ap_pass2' value='" + String(apPassword) + "'></td></tr>\r\n";
	s += "</table>\r\n";

	// station configuration
	s += "<table border=1>\r\n";
	s += "<tr><td colspan=2 bgcolor=#E0E0E0><b>STATION</b></td></tr>\r\n";
	s += "<tr><td>Mode</td><td><input type=radio id='modest' name='mode' value='station'" + (isAP?String(""):String(" checked")) + ">Station</td></tr>\r\n";
	s += "<tr><td>SSID</td><td><input type=text id='ssid' value='" + String(ssid) + "'></td></tr>\r\n";
	s += "<tr><td>Password</td><td><input type=password id='pass' value='" + String(password) + "'></td></tr>\r\n";
	s += "<tr><td>Pass Confirm</td><td><input type=password id='pass2' value='" + String(password) + "'></td></tr>\r\n";
	s += "</table>\r\n";

	// udp/tcp ports configuration
	s += "<table border=1>\r\n";
	s += "<tr><td colspan=2 bgcolor=#E0E0E0><b>UDP|TCP LISTENERS</b></td></tr>\r\n";
	s += "<tr><td>UDP Port</td><td><input type=text id='udpPort' value='" + String(udpPort) + "'></td></tr>\r\n";
	s += "<tr><td>TCP Port</td><td><input type=text id='tcpPort' value='" + String(tcpPort) + "'></td></tr>\r\n";
	s += "</table>\r\n";

	// save button
	s += "<input type=button value='Save and Reset' onClick='save()'>\r\n";

	// end of HTTP
	s += "</body></html>\r\n";

	// write second part of response
	httpClient.write(s.c_str(), s.length());
	httpClient.flush();
}


// Read string from EEPROM
bool WebConfig::ReadString(char* pString, short pos, short len)
{
	for (short i=0; i<len; i++)
	{
		pString[i] = EEPROM.read(pos++);
	}

	return true;
}


// Write string to EEPROM
bool WebConfig::WriteString(char* pString, short pos, short len)
{
	for (short i=0; i<len; i++)
	{
		EEPROM.write(pos++, pString[i]);
	}

	return true;
}


// Load settings from EEPROM
bool WebConfig::LoadSettings()
{
	EEPROM.begin(512);

	// first byte must be our signature: 0xAA
	if (EEPROM.read(0) != 0xAA) { EEPROM.end(); return false; }

	// second byte is the operation mode
	byte val = EEPROM.read(1);
	if (val > 1) { EEPROM.end(); return false; }

	// set the operation mode (0 == Router Client, 1 = AP)
	isAP = (val == 1);

	// has settings saved, so read the other settings
	// char apName[32]
	// char apPassword[32]
	// byte apChannel
	// char ssid[32]
	// char password[32]
	// int udpPort;
	// int tcpPort;
	// int webPort;
	// char webLogin[16];
	// char webPassword[16];
	// char base64Auth[64];
	ReadString(apName, 2, 32);
	ReadString(apPassword, 34, 32);
	apChannel = EEPROM.read(66);
	ReadString(ssid, 67, 32);
	ReadString(password, 99, 32);
	EEPROM.get(131, udpPort);
	EEPROM.get(135, tcpPort);
	EEPROM.get(139, webPort);
	ReadString(webLogin, 143, 16);
	ReadString(webPassword, 159, 16);
	ReadString(base64Auth, 175, 64);

	EEPROM.end();
	return true;
}


// Save settings to EEPROM
bool WebConfig::SaveSettings()
{
	EEPROM.begin(512);

	// first byte is our signature: 0xAA
	EEPROM.write(0, 0xAA);

	// second byte is the operation mode (0 == Router Client, 1 == AP)
	EEPROM.write(1, (isAP ? 1 : 0));

	// write other settings
	// char apName[32]
	// char apPassword[32]
	// byte apChannel
	// char ssid[32]
	// char password[32]
	// int udpPort;
	// int tcpPort;
	// int webPort;
	// char webLogin[16];
	// char webPassword[16];
	// char base64Auth[64];
	WriteString(apName, 2, 32);
	WriteString(apPassword, 34, 32);
	EEPROM.write(66, apChannel);
	WriteString(ssid, 67, 32);
	WriteString(password, 99, 32);
	EEPROM.put(131, udpPort);
	EEPROM.put(135, tcpPort);
	EEPROM.put(139, webPort);
	WriteString(webLogin, 143, 16);
	WriteString(webPassword, 159, 16);
	WriteString(base64Auth, 175, 64);

	EEPROM.commit();
	return true;
}


// Start AP Mode
bool WebConfig::StartAPMode()
{
	WiFi.disconnect();
	WiFi.mode(WIFI_AP);

	IPAddress ip(192, 168, 0, 1);
	IPAddress mask(255, 255, 255, 0);
	WiFi.softAPConfig(ip, ip, mask);
	WiFi.softAP(apName, apPassword, apChannel);
	isAP = true;

	return true;
}


// Extract next substring from current position to next separator or end of string
char* WebConfig::Token(char** req, const char* sep)
{
	char* pStart = *req;
	char* pPos = strstr(*req, sep);

	if (!pPos)
	{
		// no more separators, return the entire string
		return *req;
	}

	// return from the start of the string up to the separator
	*pPos = 0;
	*req = pPos+1;
	return pStart;
}


// Parse settings values in case the HTTP request includes them.
// Format is:
// /?(webPort)&(webLogin)&(webPassword)&(base64Auth)&(isAP)&(ap_name)&(ap_password)&(ap_channel)&(ssid)&(password)&(udpport)&(tcpport)
void WebConfig::ProcessParms(String req)
{
	if (req.length() == 0) return;
	if (req.indexOf("/?") == -1) return;

	char* pReq = new char[req.length()];
	strcpy(pReq, req.c_str());

	char* pPos = strstr(pReq, "/?");
	if (!pPos)
	{
		delete pReq;
		return;
	}

	// position over the first parameter
	pPos++;

	// read settings
	webPort = atoi(Token(&pPos, "&"));
	strncpy(webLogin, Token(&pPos,"&"), 16);
	strncpy(webPassword, Token(&pPos,"&"), 16);
	strncpy(base64Auth, Token(&pPos,"&"), 64);
	isAP = (atoi(Token(&pPos, "&")) == 1);                      // isAP
	strncpy(apName, Token(&pPos, "&"), 32);                     // apName
	strncpy(apPassword, Token(&pPos, "&"), 32);                 // apPassword
	apChannel = (byte)atoi(Token(&pPos,"&"));                   // apChannel
	strncpy(ssid, Token(&pPos,"&"), 32);                        // router ssid
	strncpy(password, Token(&pPos,"&"), 32);                    // router password
	udpPort = atoi(Token(&pPos,"&"));                           // udp port
	tcpPort = atoi(Token(&pPos,"&"));                           // tcp port

	// save setting on EEPROM
	SaveSettings();

	// reset the microcontroller
	Reset();
}

