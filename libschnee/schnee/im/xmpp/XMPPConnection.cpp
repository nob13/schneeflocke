#include "XMPPConnection.h"

namespace sf {

void XMPPConnection::XmppConnectDetails::defaultValues (){
	username = "";
	password = "";
	server   = "";
	port     = 5222;
	resource = "schneeflocke";
}

bool XMPPConnection::XmppConnectDetails::setTo (const String & cs){
	String s = cs;
	if (!boost::starts_with (s, "xmpp://")) return false;

	s.replace (0, String ("xmpp://").length(), "");
	// splitting user part / server part
	std::vector<String> split;
	boost::split (split, s, boost::is_any_of("@"));
	if (split.size() != 2) return false;

	String userPart = split[0];   // before "@"
	String serverPart = split[1]; // after  "@"

	// user part
	split.clear();
	boost::split (split, userPart, boost::is_any_of (":"));
	if (split.size () == 1){
		// no ':'
		username = userPart;
	} else  if (split.size () == 2){
		username = split[0];
		password = split[1];
	} else {
		return false; // wrong user part
	}

	// server part
	split.clear ();
	boost::split (split, serverPart, boost::is_any_of ("/"));
	if (split.size () == 1){
		// no "/"
		server = split[0];
	} else if (split.size() == 2){
		server = split[0];
		resource = split[1];
	} else {
		return false;
	}

	// server port
	split.clear ();
	boost::split (split, server, boost::is_any_of (":"));
	if (split.size() == 1){
		// everything ok, already in server
	} else if (split.size() == 2){
		port = ::atoi (split[1].c_str());
		server = split[0];
	} else {
		return false;
	}
	return true;
}


}
