#include "settings.h"
#include "schnee.h"

namespace sf {
namespace schnee {

Settings::Settings () {
	enableLog = false;
	enableFileLog = false;
	logFile = "";

	noLineNoise = false;
	echoServer  = "82.211.19.149";
	echoServerPort = 1234;
	disableTcp = false;
	disableUdt = false;

	forceBoshXmpp = false;
}
static Settings gSettings;

const Settings & settings () {
	return gSettings;
}

/// Checks for simple set boolean arguments
#define CHECK_BOOL_ARGUMENT(NAME) if (s == "--" #NAME) gSettings.NAME = true;

/// parse arguments and set the approciate settings
/// (done by schnee init!)
void parseArguments (int argc, const char * argv[]) {
	if (argv){
		for (int i = 1; i < argc; i++){
			String s(argv[i]);
			String t; if (i < argc - 1) t = argv[i+1];
			String u; if (i < argc - 2) u = argv[i+2];
			bool tIsArgument = t.substr(0,2) == "--";

			if (s == "--enableLog"){
				gSettings.enableLog = true;
			}
			if (s == "--enableFileLog"){
				gSettings.enableLog = true;
				gSettings.enableFileLog = true;
				gSettings.logFile = "mylog.txt";
				if (!t.empty() && !tIsArgument) {
					gSettings.logFile = t;
				}
			}
			if (s == "--echoServer") {
				gSettings.echoServer = t;
				gSettings.echoServerPort = atoi (u.c_str());
			}
			CHECK_BOOL_ARGUMENT (noLineNoise);
			CHECK_BOOL_ARGUMENT (disableTcp);
			CHECK_BOOL_ARGUMENT (disableUdt);
			CHECK_BOOL_ARGUMENT (forceBoshXmpp);
		}
	}
}



void setEchoServer (const String & server) {
	gSettings.echoServer = server;
}

void setEchoServerPort (int port) {
	gSettings.echoServerPort = port;
}

void setForceBoshXmpp (bool v) {
	gSettings.forceBoshXmpp = v;
}

}
}
