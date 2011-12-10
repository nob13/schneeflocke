#include "CmdLineClient.h"
#include <schnee/im/IMClient.h>
#include <schnee/tools/ArgSplit.h>
#include <schnee/tools/Serialization.h>
#include <schnee/tools/ResultCallbackHelper.h>
#include <schnee/schnee.h>

#include <iostream>

CmdLineClient::CmdLineClient (const sf::String & cs){
	SF_REGISTER_ME;
	mConnectionString = cs;
	mAuthContacts = false;
	
	size_t p = cs.find ("://");
	if (p == cs.npos){
		mImClient = 0;
		std::cerr << "Invalid connection string" << std::endl;
		return;
	}
	sf::String protocol;
	protocol = cs.substr(0, p);
	mImClient = sf::IMClient::create(protocol);
	if (!mImClient){
		std::cerr << "Invalid/unsupported protocol: " << protocol << std::endl;
		return;
	}
	
	mImClient->setConnectionString (cs);
	mImClient->connectionStateChanged() = sf::dMemFun (this, &CmdLineClient::connectionStateChanged);
	mImClient->contactRosterChanged()   = sf::dMemFun (this, &CmdLineClient::contactRosterChanged);
	mImClient->messageReceived()        = sf::dMemFun (this, &CmdLineClient::messageReceived);
	mImClient->subscribeRequest()       = sf::dMemFun (this, &CmdLineClient::subscriptionRequest);
	mImClient->streamErrorReceived()    = sf::dMemFun (this, &CmdLineClient::streamErrorReceived);
}

CmdLineClient::~CmdLineClient (){
	SF_UNREGISTER_ME;
	delete mImClient;
}

int CmdLineClient::start () {
	if (!mImClient) {
		return 1;
	}
	if (mImClient->connectionState() == sf::IMClient::CS_ERROR){
		std::cout << "Error in connectionString " << mConnectionString << std::endl;
		return 1;
	}
	
	std::cout << "Connecting to " << mConnectionString << " ..." << std::endl;
	std::vector<sf::String> features;
	features.push_back ("http://sflx.net/protocols/im_cmdline");
	mImClient->setFeatures (features);
	mImClient->setIdentity(sf::String ("sflx.net imclient ") + sf::schnee::version(), "console");
	sf::ResultCallbackHelper waitHelper;
	mImClient->connect(waitHelper.onResultFunc());
	if (!waitHelper.waitReadyAndNoError(30000)){
		std::cerr << "Timeout or Err during connection" << std::endl;
		return 1;
	}
	std::cout << "Connected." << std::endl;
	
	mImClient->setPresence (sf::IMClient::PS_CHAT);
	mImClient->updateContactRoster();
	
	bool quit = false;
	
	while (!quit){
		sf::ArgumentList parts;
		if (!sf::nextCommand (&parts)) { quit = true; break; } // Ctrl+C
		if (parts.empty()) continue;
		
		sf::String cmd = parts[0];
		if (cmd == "") continue;
		if (cmd == "quit") { quit = true; continue; }
		else if (cmd == "connect"){
			mImClient->connect();
		} else if (cmd == "disconnect") {
			mImClient->disconnect();
		} else if (cmd == "state") {
			sf::IMClient::ConnectionState cs = mImClient->connectionState ();
			std::cout << "Online state:  " << toString (cs) << std::endl;
			std::cout << "Id:            " << mImClient->ownId() << std::endl;
			std::cout << "OwnPresence:   " << sf::toJSON (mImClient->ownInfo()) << std::endl;
			std::cout << "Auth Contacts: " << mAuthContacts << std::endl;
		} else if (cmd == "msg") {
			if (parts.size () < 3) {
				std::cerr << "Not enough parts given" << std::endl;
				continue;
			}
			sf::IMClient::Message msg;
			msg.to = parts[1];
			msg.body = parts[2];
			if (parts.size() >= 4) msg.subject = parts[3];
			if (parts.size() >= 5) msg.id = parts[4];
			if (parts.size() >= 6) {
				sf::String t = parts[5];
				if (t == "normal") msg.type = sf::IMClient::MT_NORMAL;
				else if (t == "chat") msg.type = sf::IMClient::MT_CHAT;
				else if (t == "groupchat") msg.type = sf::IMClient::MT_GROUPCHAT;
				else if (t == "error") msg.type = sf::IMClient::MT_ERROR;
				else if (t == "headline") msg.type = sf::IMClient::MT_HEADLINE;
				else {
					std::cerr << "Unknown chat type " << t << std::endl;
					continue;
				}
			}
			mImClient->sendMessage(msg);
		}
		else if (cmd == "subscribe" && parts.size() >= 2) {
			sf::Error e = mImClient->subscribeContact (parts[1]);
			if (e) {
				std::cerr << "Subscribing contact failed: " << toString (e) << std::endl;
			}
		} else if (cmd == "cancelSubscription" && parts.size() >= 2) {
			sf::Error e = mImClient->cancelSubscription(parts[1]);
			if (e) {
				std::cerr << "Canceling subscription failed: " << toString (e) << std::endl;
			}
		} else if (cmd == "remove" && parts.size() >= 2) {
			sf::Error e = mImClient->removeContact(parts[1]);
			if (e) {
				std::cerr << "Removing contact failed: " << toString (e) << std::endl;
			}
		} else if (cmd == "auth" && parts.size() >= 2) {
			bool v = (parts[1] == "true" || parts[1] == "TRUE" || parts[1] == "1");
			mAuthContacts = v;
			std::cout << "Set authentication to " << v << std::endl;
		} else if (cmd == "features" && parts.size() >= 2) {
			sf::Error e = mImClient->requestFeatures(parts[1], sf::abind (sf::dMemFun(this, &CmdLineClient::onReceivedFeatureSet), parts[1]));
			if (e) {
				std::cerr << "Could not request features, error=" <<  toString (e) << std::endl;
			}
		}
		else if (cmd == "cl") {
			std::cout << "Contact List: " << sf::toJSONEx (mImClient->contactRoster(), sf::INDENT | sf::COMPRESS | sf::COMPACT) << std::endl;
		} else if (cmd == "ucl") {
			mImClient->updateContactRoster ();
		} else {
			const char * info =
					"** Available Commands **\n"
					"connect                             - Connects to the IM network\n"
					"disconnect                          - Disconnects from the IM network\n"
					"cl                                  - Print out contact list\n"
					"ucl                                 - Update contact list\n"
					"state                               - Current online state\n"
					"msg recv body [subject] [id] [type] - Sends a message\n"
					"subscribe name                      - subscribes a contact\n"
					"cancelSubscription name             - cancels subscription to a contact\n"
					"remove name                         - remove a contact from the list (also deleting it)\n"
					"auth true / false                   - accept contacts (default=false)\n"
					"features host                       - asks host for its feature set"
					" -- \n"
					"help                                - print this help\n"
					"quit                                - quit the program\n";
			std::cout << info << std::endl;;
		}
	}
	return 0;
}

void CmdLineClient::connectionStateChanged (sf::IMClient::ConnectionState state){
	std::cout << "ConnectionStateChange: " << toString (state) << std::endl;
}

void CmdLineClient::messageReceived (const sf::IMClient::Message & msg){
	std::cout << "Message received " << sf::toJSON (msg) << std::endl;
}


void CmdLineClient::contactRosterChanged (){
	std::cout << "Contact Roster Changed." << std::endl;
}

void CmdLineClient::subscriptionRequest (const sf::UserId & user) {
	std::cout << "User " << user << " asks for authentication for presence, answering: " << mAuthContacts << std::endl;
	mImClient->subscriptionRequestReply(user, mAuthContacts, true);
}

void CmdLineClient::streamErrorReceived (const sf::String & text) {
	std::cout << "Recevied Stream Error: " << text << std::endl;
}

void CmdLineClient::onReceivedFeatureSet (sf::Error result, const sf::IMClient::FeatureInfo & info, const sf::HostId & source) {
	std::cout << "Received features of " << source << " went " << toString (result) << " in " << sf::toJSON (info) << std::endl;
}



