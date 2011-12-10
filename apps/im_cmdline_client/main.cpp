#include <schnee/im/xmpp/XMPPClient.h>
#include <iostream>

#include "CmdLineClient.h"
#include <schnee/schnee.h>
#include <flocke/hardcodedLogin.h>
/*
 * A minimalistic IM client dmonstrating IM connection possibilities with the class IMClient.
 *
 * Note: the client is a bit older, thats why its programmatic design is a bit strange.
 */

int main (int argc, char * argv []){
	sf::schnee::SchneeApp sapp (argc, argv);
	if (argc < 2){
		std::cout << "usage: im_cmdline_client <conncetion-string> [--enableLog] [--autoAuth]" << std::endl;
		std::cout << "connection string of the form xmpp://username:password@server/resource" << std::endl;
		std::cout << "Available protocols: " << sf::toString (sf::IMClient::availableProtocols()) << std::endl;
		return 1;
	}
	// HACK, parsing should be somewhere else
	bool autoAuth = false;
	for (int i = 1; i < argc; i++) {
		if (sf::String(argv[i]) == "--autoAuth") autoAuth = true;
	}
	sf::String cs = sf::hardcodedLogin(argv[1]);
	CmdLineClient client (cs);
	client.setAuthContacts(autoAuth);
	client.start ();
	return 0;
}


