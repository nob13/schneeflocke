#include "settings.h"
#include "schnee.h"
#include <schnee/net/TLSCertificates.h>

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
/// (called by schnee init!)
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

static const char * sflxCaCertificate = "-----BEGIN CERTIFICATE-----"
		"MIIEMDCCAxigAwIBAgIJANPa8IW8g0knMA0GCSqGSIb3DQEBBQUAMG0xGTAXBgNV"
		"BAoTEHNmbHgubmV0IFByb2plY3QxCzAJBgNVBAsTAkNBMQ8wDQYDVQQHEwZCZXJs"
		"aW4xDzANBgNVBAgTBkJlcmxpbjELMAkGA1UEBhMCREUxFDASBgNVBAMTC3NmbHgu"
		"bmV0IENBMB4XDTEyMDExMzE3MTgwMFoXDTIyMDExMDE3MTgwMFowbTEZMBcGA1UE"
		"ChMQc2ZseC5uZXQgUHJvamVjdDELMAkGA1UECxMCQ0ExDzANBgNVBAcTBkJlcmxp"
		"bjEPMA0GA1UECBMGQmVybGluMQswCQYDVQQGEwJERTEUMBIGA1UEAxMLc2ZseC5u"
		"ZXQgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDED+J5bVXnlpvk"
		"RxE5szJ1oBJ1neXZj/yAbrXxJ3O1ZKNRepS70Vz8TCyb/XyBxtzkmUOyY4ny/hRK"
		"Ar3mgGKsrN3Qa02hQ014DahdyW/HdUjAuqannMPHuJvPqqfzJY0imCNXmy708aaf"
		"s6M29HKbN+ReUIa0ZNYq40i9Rmkk+uFvpwww/LCFRhrr6JoM+Tr+a46sKRsPevlz"
		"ZUZM+hL4ewwquFd/rvSJxVQxfeiK1/RqsYQFFWb7b0aa0Mx70SVUws9OOMJfzEMQ"
		"OEWxfRcJnjFo4EoBQdX+altEsBymmS/x1H14HkGUQeby2VlKS2CwEb8xIzbpVqej"
		"foUgOoU5AgMBAAGjgdIwgc8wDAYDVR0TBAUwAwEB/zAdBgNVHQ4EFgQUZsPKN1J5"
		"fFSp84Vs/aSEB+ra+hQwgZ8GA1UdIwSBlzCBlIAUZsPKN1J5fFSp84Vs/aSEB+ra"
		"+hShcaRvMG0xGTAXBgNVBAoTEHNmbHgubmV0IFByb2plY3QxCzAJBgNVBAsTAkNB"
		"MQ8wDQYDVQQHEwZCZXJsaW4xDzANBgNVBAgTBkJlcmxpbjELMAkGA1UEBhMCREUx"
		"FDASBgNVBAMTC3NmbHgubmV0IENBggkA09rwhbyDSScwDQYJKoZIhvcNAQEFBQAD"
		"ggEBAGe+uUg9jJiDv5xX0qWgkBgJDmR1exp8QlaoMVnFFirxxTjz6JFuPJa6R/PM"
		"C/quLO9rTCVR2BLUk3+wpr82ZYre+RiyWA6oDSzv0pJoVveTP1Mm/E/hjqr2ufKF"
		"oRpX8fUFW1Dwxrt9TnIYRp6v1cHRZzUeuoMoo69YDcv0v8rllpbRh1PS/HC5YYPU"
		"CLzz3tIIucaWXnSMARdbqGuixLXZeY1te9KbtIOIsptIlNU9Y3AzP3S/gwPj8LXL"
		"Og3d/1PerrJyg4pqx7gz2GqwHkeWMIU8T1DIzeGBSKH2TsWFA5dTLLSb2Ky8TjVq"
		"zMjqqHVo2tgYNlTvabyk9QQCAPI="
		"-----END CERTIFICATE-----";

/// Sets initial TLS Certificates
/// called by schnee init
void setInitialCertificates () {
	Error e = TLSCertificates::instance().add(sflxCaCertificate);
	assert (!e);
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
