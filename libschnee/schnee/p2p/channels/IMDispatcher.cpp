#include "IMDispatcher.h"
#include <schnee/tools/Log.h>

#include <schnee/tools/Deserialization.h>
#include <schnee/net/impl/IOService.h>


namespace sf {

IMDispatcher::IMDispatcher () {
	SF_REGISTER_ME;
	mClient = 0;
	mTimeOutMs = 10000;
}

IMDispatcher::~IMDispatcher (){
	SF_UNREGISTER_ME;
	disconnect ();
}

sf::Error IMDispatcher::createChannel (const HostId & target, const ResultCallback & callback, int timeOutMs) {
	if (!mClient){
		sf::Log (LogError) << LOGID << "No im client available" << std::endl;
		return error::InvalidArgument;
	}
	ChannelMap::iterator i = mChannels.find (target);
	if (i != mChannels.end()){
		// There is already a channel
		IMChannelPtr & channel (i->second);
		if (channel->state () != OS_OFFLINE){
			Log (LogWarning) << LOGID << "Deleting existing channel, will lead to a reconnection" << std::endl;
		}
		channel->invalidate();
		mChannels.erase (i);
	}
	AsyncOpId id = genFreeId ();
	CreateChannelOp * op = new CreateChannelOp (sf::regTimeOutMs(timeOutMs));
	op->setId (id);
	op->target   = target;
	op->callback = callback;
	op->channel  = IMChannelPtr (new IMChannel (this, target, onlineState_locked (target)));
	op->auth.finished() = abind (dMemFun (this, &IMDispatcher::onAuthFinishedCreatingChannel), id);
	op->auth.init (op->channel, mClient->ownId());
	op->auth.connect (target, timeOutMs);
	op->setState (CreateChannelOp::AwaitAuth);
	op->cancelOp = abind (dMemFun (this, &IMDispatcher::onOpCanceled), target, op->channel);

	mChannels[target] = op->channel;

	addAsyncOp (op);
	return NoError;
}


void IMDispatcher::setHostId (const sf::HostId & id) {
	if (id != mHostId){
		Log (LogWarning) << LOGID << "Set host id " << id << " differs from presence id " << mHostId  << " (ignoring)"<< std::endl;
	}
}

sf::Error IMDispatcher::connect (const sf::String & connectionString, const sf::String & password, const ResultDelegate & callback) {
	disconnect ();
	// figure out protocol
	size_t pos = connectionString.find("://");
	if (pos == connectionString.npos) return sf::error::InvalidArgument;
	sf::String protocol = connectionString.substr(0, pos);
	sf::IMClient * client = sf::IMClient::create(protocol);
	if (!client) {
		sf::Log(LogError) << LOGID << "Did not found protocol \"" << protocol << "\"" << std::endl;
		return sf::error::InvalidArgument;
	}
	mClient = client;
	mClient->setIdentity(mClientName, "bot");
	mClient->setFeatures (mFeatures);
	mClient->setConnectionString (connectionString);
	if (!password.empty()) mClient->setPassword (password);
	mClient->subscribeRequest ()      = dMemFun (this, &IMDispatcher::onSubscribeRequest);
	mClient->connectionStateChanged() = dMemFun (this, &IMDispatcher::onConnectionStateChanged);
	mClient->contactRosterChanged()   = dMemFun (this, &IMDispatcher::onContactRosterChanged);
	mClient->messageReceived()        = dMemFun (this, &IMDispatcher::onMessageReceived);
	mClient->streamErrorReceived()    = dMemFun (this, &IMDispatcher::onServerStreamErrorRecevied);
	mClient->connect(callback);

	return sf::error::NoError;
}

void IMDispatcher::disconnect (){
	for (ChannelMap::iterator i = mChannels.begin(); i != mChannels.end(); i++){
		i->second->invalidate();
	}
	mChannels.clear();
	if (mClient) {
		mClient->disconnect();	// callback will be send asynchronous
		// TODO: We have to give it  some time here in order to mark us offline!!
		// BUG: #159
		delete mClient;
		mClient = 0;
	}
	mHosts.clear();
	mUsers.clear();
	if (mPeersChanged) xcall (mPeersChanged);
}

sf::HostId IMDispatcher::hostId() const {
	return mHostId;
}

OnlineState IMDispatcher::onlineState () const {
	if (!mClient) return OS_OFFLINE;
	if (mClient->isConnected()) return OS_ONLINE;
	else return OS_OFFLINE;
}

OnlineState IMDispatcher::onlineState (const HostId & user) const {
	return onlineState_locked (user);
}

IMDispatcher::HostInfoMap IMDispatcher::hosts(const UserId & user) const {
	HostInfoMap result;
	for (HostInfoMap::const_iterator i = mHosts.begin(); i != mHosts.end(); i++){
		if (i->second.userId == user) {
			result[i->first] = i->second;
		}
	}
	return result;
}

IMDispatcher::HostInfo IMDispatcher::hostInfo (const HostId & host) const {
	HostInfoMap::const_iterator i = mHosts.find (host);
	if (i == mHosts.end()) return HostInfo();
	return i->second;
}

Error IMDispatcher::updateFeatures (const HostId & host, const ResultCallback & callback) {
	HostInfoMap::const_iterator i = mHosts.find(host);
	if (i == mHosts.end()) return error::NotFound;
	if (!mClient) return error::ConnectionError;
	return mClient->requestFeatures(host, abind (dMemFun (this, &IMDispatcher::onFeatureRequestReply), host, callback));
}

Error IMDispatcher::setOwnFeature (const String & client, const std::vector<String> & features) {
	mClientName = client;
	mFeatures = features;
	return NoError;
}

Error IMDispatcher::subscribeContact (const sf::UserId & user) {
	if (!mClient) return error::ConnectionError;
	return mClient->subscribeContact(user);
}

Error IMDispatcher::subscriptionRequestReply (const sf::UserId & user, bool allow, bool alsoAdd) {
	if (!mClient) return error::ConnectionError;
	return mClient->subscriptionRequestReply(user, allow, alsoAdd);
}

Error IMDispatcher::removeContact (const sf::UserId & user) {
	if (!mClient) return error::ConnectionError;
	return mClient->removeContact(user);
}

bool IMDispatcher::send (const sf::IMClient::Message & m){
	if (mClient){
		return mClient->sendMessage (m);
	} else {
		sf::Log (LogError) << LOGID << "Not connected, cannot send a message" << std::endl;
		return false;
	}
}

void IMDispatcher::onOpCanceled (const HostId& target, const IMChannelPtr & channel) {
	if (mChannels[target] == channel){
		mChannels.erase(target);
	}
	channel->invalidate();
}

void IMDispatcher::onAuthFinishedCreatingChannel (Error err, AsyncOpId id) {
	CreateChannelOp * op;
	getReadyAsyncOp (id, CREATE_CHANNEL, &op);
	if (!op){
		Log (LogWarning) << LOGID << "Auth result on non-existing channel, propably timeouted" << std::endl;
		return;
	}
	if (op->state() != CreateChannelOp::AwaitAuth){
		Log (LogError) << LOGID << "Wrong state" << std::endl;
		return;
	}
	if (err){
		op->channel->invalidate();
		if (mChannels[op->target] == op->channel) { // perhaps there is another connect process already going on
			mChannels.erase (op->target);
		}
		if (op->callback) xcall (abind (op->callback, error::AuthError));
		delete op;
		return;
	}
	// Success!
	xcall (abind (mChannelCreated, op->target, op->channel, true));
	if (op->callback) xcall (abind (op->callback, error::NoError));
	delete op;
}

void IMDispatcher::onAuthFinishedRespondingChannel (Error err, AsyncOpId id) {
	RespondChannelOp * op;
	getReadyAsyncOp (id, RESPOND_CHANNEL, &op);
	if (!op){
		Log (LogWarning) << LOGID << "Auth result on non-existing channel, probably timeouted" << std::endl;
		return;
	}
	if (op->state != RespondChannelOp::AwaitAuth){
		Log (LogError) << LOGID << "Wrong state" << std::endl;
		return;
	}
	if (err) {
		op->channel->invalidate();
		if (mChannels[op->source] == op->channel) {
			mChannels.erase (op->source);
		}
		Log (LogInfo) << LOGID << "Could not authenticate channel from " << op->source << " error: " << toString (err) << std::endl;
		// no callbacks, as channel was responded
		delete op;
		return;
	}
	// Success!
	xcall (abind (mChannelCreated, op->source, op->channel, false));
	delete op;
}



sf::OnlineState IMDispatcher::onlineState_locked (const sf::String & id) const {
	HostInfoMap::const_iterator i = mHosts.find (id);
	if (i == mHosts.end()) return OS_OFFLINE;
	return OS_ONLINE;
}

void IMDispatcher::onSubscribeRequest (const sf::UserId & from) {
	if (mSubscribeRequest) {
		mSubscribeRequest (from);
	} else {
		Log (LogWarning) << LOGID << "No subscribe request delegate, using default behaviour (denying)" << std::endl;
		mClient->subscriptionRequestReply(from,false,false);
	}
}

void IMDispatcher::onConnectionStateChanged (sf::IMClient::ConnectionState state) {
	Log (LogProfile) << LOGID << "IMDispatcher::onConnectionStateChanged " << toString (state) << std::endl;
	if (state == sf::IMClient::CS_CONNECTED) {
		mClient->setPresence (IMClient::PS_CHAT, "Schneeflocke (http://sflx.net)", -10);
		mClient->updateContactRoster();
		mHostId = mClient->ownId();
	} else {
		// If we are not online, all channels go offline
		for (ChannelMap::iterator i = mChannels.begin(); i != mChannels.end(); i++){
			i->second->setState (OS_OFFLINE);
		}
		// TODO: maybe better, if peers are just marked as being offline?
		// But Skype and other Chat programs delete also all from the List. Tricky.
		mHosts.clear();
		mUsers.clear();
		mRoster.clear ();
		if (mPeersChanged) xcall (mPeersChanged);

	}
	if (mOnlineStateChanged){
		OnlineState os;
		switch (state){
		case sf::IMClient::CS_CONNECTED:
			os = OS_ONLINE;
		break;
		case sf::IMClient::CS_OFFLINE:
		case sf::IMClient::CS_ERROR:
			os = OS_OFFLINE;
		break;
		default:
			os = OS_UNKNOWN;
			break;
		}
		xcall (abind (mOnlineStateChanged, os));
	}
}

/// Splits HostId in UserId and resource name
/// TODO: Move function as soon there is a HostId class.
static void splitHostId (const sf::HostId & hostId, sf::UserId * userId, sf::String * resource){
	size_t p = hostId.rfind('/');
	if (p  == hostId.npos) {
		if (userId) *userId = hostId;
		if (resource) resource->clear();
	} else {
		if (userId) *userId     = hostId.substr (0,p);
		if (resource) *resource = hostId.substr (p + 1, hostId.npos);
	}
}
static String getResourceName (const sf::HostId & hostId){
	sf::String r;
	splitHostId (hostId, 0, &r);
	return r;
}

void IMDispatcher::onContactRosterChanged (){
	if (!mClient) { // may happen - if client is already shut down and signal is async
		return;
	}
	mRoster = mClient->contactRoster();
	Log (LogInfo) << LOGID << "Updating roster to " << sf::toJSONEx (mRoster, sf::COMPRESS | sf::COMPACT) << std::endl;
	mUsers.clear();
	mHosts.clear();

	for (sf::IMClient::Contacts::const_iterator i = mRoster.begin(); i != mRoster.end(); i++) {
		const sf::IMClient::ContactInfo & info = i->second;
		if (info.hide) continue;

		UserInfo & userInfo = mUsers[info.id];
		userInfo.userId    = info.id;
		userInfo.name      = info.name;
		userInfo.waitForSubscription = info.waitForSubscription;
		userInfo.error     = info.error;
		userInfo.errorText = info.errorText;
		
		for (sf::IMClient::ClientPresences::const_iterator j = info.presences.begin(); j != info.presences.end(); j++) {
			const IMClient::ClientPresence & p = *j;
			if (p.presence == IMClient::PS_OFFLINE) continue; // ignoring offline presences
			if (j->id == mHostId) continue; // ignoring own id
			HostInfo & hostInfo = mHosts[j->id];
			hostInfo.hostId = p.id;
			hostInfo.userId = userInfo.userId;
			hostInfo.name  = getResourceName (p.id);
		}
	}

	// Updating all Channels...
	for (ChannelMap::iterator i = mChannels.begin(); i != mChannels.end(); i++){
		OnlineState s = onlineState_locked (i->first);
		i->second->setState (s);
	}
	if (mPeersChanged) mPeersChanged ();
}

void IMDispatcher::onMessageReceived (const sf::IMClient::Message & message){
	IMChannelPtr channel;
	ChannelMap::iterator i = mChannels.find (message.from);

	bool isCreateChannel = false;
	{
		sf::String cmd;
		sf::Deserialization ds (message.body, cmd);
		if (!ds.error() && cmd == "createChannel") isCreateChannel = true;
	}

	if (i == mChannels.end() || isCreateChannel){
		if (!isCreateChannel) {
			sf::Log (LogWarning) << LOGID << "Creating channel on a not isCreateChannel operation, will fail" << std::endl;
			IMClient::Message reply;
			reply.to   = message.from;
			reply.body = "This is a schneeflocke client who cannot chat! See http://sflx.net for more information! :)";
			reply.id = message.id;
			reply.type = message.type;
			mClient->sendMessage (reply);
			return;
		}
		if (i != mChannels.end()){
			// deactivate old channel
			i->second->invalidate();
		}
		String source = message.from;
		AsyncOpId id = genFreeId ();
		RespondChannelOp * op = new RespondChannelOp (sf::regTimeOutMs (mTimeOutMs));
		op->setId(id);

		op->source   = source;
		op->state    = RespondChannelOp::AwaitAuth;
		op->channel  = IMChannelPtr (new IMChannel (this, source, onlineState_locked (source)));
		op->cancelOp = abind (dMemFun (this, &IMDispatcher::onOpCanceled), source, op->channel);
		op->auth.init     (op->channel, mClient->ownId());
		op->auth.finished() = abind (dMemFun (this, &IMDispatcher::onAuthFinishedRespondingChannel), id);
		op->auth.passive (source);

		mChannels[source] = op->channel;
		channel = op->channel;
		addAsyncOp (op);
	} else {
		channel = i->second;
	}
	channel->pushMessage (message);
}

void IMDispatcher::onServerStreamErrorRecevied (const String & text) {
	// forward
	if (mServerStreamErrorReceived) mServerStreamErrorReceived (text);
}

void IMDispatcher::onFeatureRequestReply (Error result, const IMClient::FeatureInfo & features, const HostId & host, const ResultCallback & originalCallback) {
	if (result)
		return notify (originalCallback, result);
	HostInfoMap::iterator i = mHosts.find(host);
	if (i == mHosts.end()) {
		return notifyAsync (originalCallback, error::NotFound);
	}
	i->second.client   = features.clientName;
	i->second.features = features.features;
	notifyAsync (mPeersChanged);
}



}
