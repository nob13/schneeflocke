#include "ServerlessXMPPClient.h"

#include <schnee/tools/Log.h>
#include <schnee/net/Tools.h>
#include <schnee/tools/async/DelegateBase.h>

namespace sf {

const String ServerlessXMPPClient::serviceType = "_presence._tcp";

ServerlessXMPPClient::ServerlessXMPPClient (){
	mConnectionState = CS_OFFLINE;
	mInfo = ContactInfo ();
	SF_REGISTER_ME;

	mZeroConf.serviceOnline  () = dMemFun (this, &ServerlessXMPPClient::onServiceOnline);
	mZeroConf.serviceOffline () = dMemFun (this, &ServerlessXMPPClient::onServiceOffline);
	mZeroConf.serviceUpdated () = dMemFun (this, &ServerlessXMPPClient::onServiceUpdated);

}

ServerlessXMPPClient::~ServerlessXMPPClient (){
	SF_UNREGISTER_ME;
}

void ServerlessXMPPClient::setConnectionString (const String & connectionString) {
	mConnectionString = connectionString;
}

void ServerlessXMPPClient::connect (const ResultDelegate & callback) {
	if (!mZeroConf.available()){
		connectionError ("No ZeroConf Service: " + mZeroConf.errorMessage());
		return notifyAsync (callback, sf::error::NoZeroConf);
	}
	if (mConnectionManager.isOnline()){
		Log (LogProfile) << LOGID << "Killing old ConnectionManager..." << std::endl;
		mConnectionManager.stop();
	}
	// parsing of connection string...
	// format: slxmpp://username
	if (!boost::starts_with (mConnectionString, "slxmpp://")){
		connectionError ("Wrong protocol");
		return notifyAsync (callback, error::InvalidArgument);
	}
	String username = mConnectionString;
	
	username.replace (0, String("slxmpp://").length(), "");
	
	if (username == ""){
		connectionError ("No username given");
		return notifyAsync (callback, error::InvalidArgument);
	}
	
	boost::replace_all (username, "@", "\\064");
	String hostName = net::hostName();
	String jid = username + "@" + hostName;
	
	
	
	
	Log (LogInfo) << LOGID << "Logging in with username " << username << std::endl;
	Log (LogInfo) << LOGID << "Your jid will be " << jid << std::endl;
	Log (LogInfo) << LOGID << "Starting ConnectionManager" << std::endl;
	bool result = mConnectionManager.startServer(jid);
	if (!result){
		connectionError ("ConnectionManager failed to start server");
		return notifyAsync (callback, error::ServerError);
	}
	Log (LogInfo) << LOGID << "Port: " << mConnectionManager.serverPort() << std::endl;
	
	mInfo.id = jid;
	ClientPresence presence;
	presence.id = mInfo.id;
	presence.presence = IMClient::PS_UNKNOWN;
	mInfo.presences.push_back (presence);
	updateService ();
	if (!mZeroConf.publishService (mService)){
		connectionError ("Could not publish service: " + mZeroConf.errorMessage());
		return notifyAsync (callback, error::ZeroConfError);
	}
	if (!mZeroConf.subscribeServices (serviceType)){
		connectionError ("Could not subscribe service _presence._tcp: " + mZeroConf.errorMessage());
		return notifyAsync (callback, error::ZeroConfError);
	}
	connectionStateChange (IMClient::CS_CONNECTED);
	return notifyAsync (callback, error::NoError);
}

void ServerlessXMPPClient::disconnect () {
	if (!isConnected()) return;
	mZeroConf.cancelService (mService);
	mConnectionManager.stop();
	connectionStateChange (IMClient::CS_OFFLINE);
}

IMClient::ContactInfo ServerlessXMPPClient::ownInfo () {
	return mInfo;
}

String ServerlessXMPPClient::ownId () {
	return mInfo.id;
}

void ServerlessXMPPClient::setPresence (const PresenceState & state, const String & desc, int priority) {
	if (isConnected()){
		assert (mInfo.presences.size() == 1);
		IMClient::ClientPresence & p = mInfo.presences[0];
		p.presence = state;
		// ignoring desc
		updateService ();
		if (!mZeroConf.updateService(mService)){
			connectionError ("Could not update service " + mZeroConf.errorMessage());
			return;
		}
	}
}

IMClient::Contacts ServerlessXMPPClient::contactRoster () const {
	return mContacts;
}

void ServerlessXMPPClient::updateContactRoster () {
	// is automatically updated!
}

bool ServerlessXMPPClient::sendMessage (const Message & msg){
	String data;
	xmpp::Message m (msg);
	return mConnectionManager.send (msg);
//	m.encode(data);
//	return mConnectionManager.sendAsync (msg.to, sf::createByteArrayPtr(data));
}

void ServerlessXMPPClient::updateService () {
	if (!mConnectionManager.isOnline()){
		Log (LogWarning) << LOGID << "Updating service without online ConnectionManager..." << std::endl;
	}
	int port = mConnectionManager.serverPort();
	assert (mInfo.presences.size() == 1);
	const ClientPresence & p = mInfo.presences[0];
	
	mService.port = port;
	mService.serviceName = mInfo.id;
	mService.serviceType = serviceType;

	mService.txt["txtvers"] = "1";
	mService.txt["vc"] = "!"; // No Video Support
	mService.txt["port.p2pj"] = sf::toString (port);
	
	String status = "avail";
	if (p.presence == IMClient::PS_AWAY) status = "away";
	// if (p.presence == IMClient::PS_CHAT) status = "avail"; // standard
	if (p.presence == IMClient::PS_DO_NOT_DISTURB) status = "dnd";
	if (p.presence == IMClient::PS_EXTENDED_AWAY) status = "away";
	if (p.presence == IMClient::PS_OFFLINE) status = "away"; // own state to offline doesn't really makes sense
	
	mService.txt["status"] = status;	
}

void ServerlessXMPPClient::connectionStateChange (ConnectionState state){
	mConnectionState = state;
	if (mConnectionStateChangedDelegate)
		xcall (abind(mConnectionStateChangedDelegate, state));
}

void ServerlessXMPPClient::connectionError (const String & msg){
	Log (LogError) << LOGID << msg << std::endl;
	mErrorMessage = msg;
	connectionStateChange (CS_ERROR);
}

IMClient::ContactInfo ServerlessXMPPClient::decodeInfo (const ZeroConfService::Service & service){
	ContactInfo result;
	result.id = service.serviceName;
	String first, last, status;
	std::map<String,String>::const_iterator i;
	if ((i = service.txt.find ("1st"))    != service.txt.end()){
		first  = i->second;
	}
	if ((i = service.txt.find ("last"))   != service.txt.end()){
		last   = i->second;
	}
	if ((i = service.txt.find ("status")) != service.txt.end()){
		status = i->second;
	}
	if (!first.empty() || !last.empty()) result.name = first + " " + last;
	ClientPresence client;
	client.id = service.serviceName;
	if (!status.empty()){
		if (status == "avail") client.presence = IMClient::PS_UNKNOWN; // regular available
		if (status == "dnd") client.presence = IMClient::PS_DO_NOT_DISTURB;
		if (status == "away") client.presence = IMClient::PS_AWAY;
	} else client.presence = IMClient::PS_UNKNOWN;
	result.presences.push_back(client);
	return result;
}

void ServerlessXMPPClient::onServiceOnline  (const ZeroConfService::Service & service){
	Log (LogInfo) << LOGID << "Service went online " << toJSON (service) << std::endl;
	assert (service.serviceType == serviceType);
	assert (service.serviceName != "");
	if (service.serviceName == mInfo.id) return; // ignoring ourself
	mConnectionManager.updateTarget(service.serviceName, service.address, service.port);
	
	ContactInfo info = decodeInfo (service);
	mContacts [info.id] = info;
	if (mContactRosterChangedDelegate) xcall (mContactRosterChangedDelegate);
}

void ServerlessXMPPClient::onServiceOffline (const ZeroConfService::Service & service) {
	Log (LogInfo) << LOGID << "Service went offline " << toJSON (service) << std::endl;
	assert (service.serviceType == serviceType);
	assert (service.serviceName != "");
	if (service.serviceName == mInfo.id) return; // ignoring ourself
	mConnectionManager.removeTarget (service.serviceName);
	
	ContactInfo info = decodeInfo (service);
	Contacts::iterator i = mContacts.find (info.id);
	if (i == mContacts.end()){
		Log (LogWarning) << LOGID << "Warning, I found a Ghost " << info.id << std::endl;
		return;
	}
	mContacts.erase (i);
	if (mContactRosterChangedDelegate) xcall (mContactRosterChangedDelegate);
}

void ServerlessXMPPClient::onServiceUpdated (const ZeroConfService::Service & service) {
	Log (LogInfo) << LOGID << "Service updated " << toJSON (service) << std::endl;
	assert (service.serviceType == serviceType);
	assert (service.serviceName != "");
	if (service.serviceName == mInfo.id) return; // ignoring ourself
	mConnectionManager.updateTarget (service.serviceName, service.address, service.port);
	ContactInfo info = decodeInfo (service);
	Contacts::iterator i = mContacts.find (info.id);
	if (i == mContacts.end()){
		Log (LogWarning) << LOGID << "Warning, I found a Ghost " << info.id << " inserting it " << std::endl;
		mContacts[info.id] = info;
	} else {
		i->second = info;
	}
	if (mContactRosterChangedDelegate) xcall (mContactRosterChangedDelegate);
}
}
