#include "BoshTransport.h"
#include "BoshNodeBuilder.h"
#include "BoshNodeParser.h"
#include <schnee/tools/Log.h>
#include <boost/lexical_cast.hpp>

namespace sf {

BoshTransport::BoshTransport () {
	SF_REGISTER_ME;
	mRid   = 0;
	mState = Unconnected;
	mOpenRidCount = 0;

	mError = NoError;
	mErrorCount    = 0;
	mSumErrorCount = 0;
	mSuccessCount  = 0;
	mMaxErrorCount = 10;
	mReconnectWaitMs = 10;
	mAuthenticated = false;
}

BoshTransport::~BoshTransport(){
	SF_UNREGISTER_ME;
	close();
}

void setIfNotSet (BoshTransport::StringMap * dst, const String& key, const String & value) {
	if (dst->find(key) == dst->end()){
		(*dst)[key] = value;
	}
}

static int64_t randomRid () {
	assert (RAND_MAX >= 2147483647);
	int64_t a = rand();
	int64_t b = rand();
	assert (a>=0 && b>=0);
	// 36bit (leaving min 19bit for us to 2^53-1)
	int64_t r = (a << 4) + b;
	assert (r>=0);
	return r;
}

void BoshTransport::connect(const sf::Url & url, const StringMap & additionalArgs, int timeOutMs, const ResultCallback & callback){
	if (mState != Unconnected) {
		Log (LogError) << LOGID << "Wrong State " << toString (mState) << std::endl;
		return xcall (abind (callback, error::WrongState));
	}

	mUrl = url;
	mRid = randomRid();
	mRidRecv = mRid;

	// Setting Parameters
	StringMap args = additionalArgs;
	args["rid"] = toString (mRid);
	setIfNotSet (&args, "ver", "1.10");
	setIfNotSet (&args, "wait", "60");
	setIfNotSet (&args, "hold", "1");
	setIfNotSet (&args, "xmlns", "http://jabber.org/protocol/httpbind");

	BoshNodeBuilder builder;
	for (StringMap::const_iterator i = args.begin(); i != args.end(); i++){
		builder.addAttribute(i->first,i->second);
	}

	// Send it out...
	mState = Connecting;
	executeRequest (sf::createByteArrayPtr(builder.toString()), timeOutMs, abind (dMemFun (this, &BoshTransport::onConnectReply), callback));
}

void BoshTransport::restart () {
	if (mState != Connected) {
		Log (LogWarning) << LOGID << "Wrong state for restart " << toString (mState) << std::endl;
		return;
	}
	StringMap args;
	args["xmpp:restart"] = "true";
	args["xmlns:xmpp"] = "urn:xmpp:xbosh";
	startNextRequest (args);
}

Error BoshTransport::write (const ByteArrayPtr& data, const ResultCallback & callback) {
	if (mState != Connected)
		return error::WrongState;
	if (callback) {
		Log (LogError) << LOGID << "BoshTransport does not support any write callbacks yet!" << std::endl;
	}
	mOutputBuffer.push_back(data);
	/// some braking
	sf::xcallTimed(dMemFun (this, &BoshTransport::continueWorking), sf::futureInMs(mReconnectWaitMs));
	return NoError;
}

sf::ByteArrayPtr BoshTransport::read (long maxSize) {
	ByteArrayPtr result = sf::createByteArrayPtr();
	if (maxSize < 0 || maxSize >= (long) mInputBuffer.size()) {
		result->swap (mInputBuffer);
	} else {
		result->append (mInputBuffer.const_c_array(), maxSize);
		mInputBuffer.l_truncate(maxSize);
	}
	return result;
}

void BoshTransport::close (const ResultCallback & callback) {
	if (mState == Connected) {
		StringMap map;
		map["type"] = "terminate";
		startNextRequest(map, callback);
		mState = Closing;
	}
}

Channel::ChannelInfo BoshTransport::info () const {
	Channel::ChannelInfo info;
	info.virtual_ = true;
	if (mState == Connected){
		info.raddress = mUrl.host();
	}
	info.authenticated = mAuthenticated;
	return info;
}

const char * BoshTransport::stackInfo () const {
	return "bosh";
}

static void asyncNotify (const ResultCallback & callback, Error e){
	if (callback) xcall (abind (callback, e));
}

void BoshTransport::onConnectReply (Error result, const HttpResponsePtr & response, const ResultCallback & callback) {
	if (mState != Connecting) {
		Log (LogError) << LOGID << "Strange state " << toString (mState) << std::endl;
		return;
	}
	if (result) {
		return failConnect (result, callback, "error on http connect");
	}
	if (!(response->resultCode == 200))
		return failConnect (error::ConnectionError, callback, String ("bad http result=" + toString (response->resultCode)).c_str());
	mAuthenticated = response->authenticated;

	BoshNodeParser parser;
	Error e = parser.parse(response->data->const_c_array(), response->data->size());
	if (e) {
		return failConnect (e, callback, "parse");
	}

	mSid = parser.attribute("sid");
	if (mSid.empty()){
		return failConnect (error::BadProtocol, callback, "parse");
	}

	String wait = parser.attribute("wait");
	if (wait.empty()){
		Log (LogWarning) << LOGID << "Server did not respond with wait attribute!" << std::endl;

		// workaround. wait is mandatory, but at least the local Prosody instance does not send it
		// http://code.google.com/p/lxmppd/issues/detail?id=219
		mLongPollTimeoutMs = 60000;
	} else {
		try {
			mLongPollTimeoutMs = boost::lexical_cast<int> (wait) * 1000;
		} catch (boost::bad_lexical_cast & e){
			return failConnect (error::BadProtocol, callback, "bad wait response");
		}
	}

	String type = parser.attribute ("type");
	if (!type.empty()) { // error or terminate
		return failConnect (error::CouldNotConnectHost, callback, String ("opponent sent type=" + type).c_str());
	}
	mState = Connected;

	String content = parser.content();
	if (!content.empty()){
		mInputBuffer.append(content.c_str(), content.length());
		Log (LogInfo) << LOGID << "Server sent " << content.length() << " bytes on session create response" << std::endl;
	}
	mRidRecv++; // cannot receive first message out of order.

	asyncNotify (callback, NoError);
	// initial pending request
	continueWorking ();
	if (mChanged) {
		xcall (mChanged);
	}
}

void BoshTransport::failConnect (Error result, const ResultCallback & callback, const String & msg) {
	Log(LogWarning) << LOGID << "Connect failed: " << toString(result) << " " <<  msg << std::endl;
	asyncNotify (callback, result);
	if (mChanged) xcall (mChanged);
	mState = Other;
	mError = result;
	mErrorMessage = msg;
}

void BoshTransport::continueWorking () {
	if (mOpenRidCount >= 2) {
		Log (LogInfo) << LOGID << "Not continuing " << mOpenRidCount << "(>=2) connections are open..." << std::endl;
		return;
	}
	if (mOpenRidCount >= 1 && mOutputBuffer.empty()){
		Log (LogInfo) << LOGID << "Not continuing, pending connection and no data to be sent" << std::endl;
		return;
	}
	startNextRequest ();
}

void BoshTransport::startNextRequest (const StringMap & additionalArgs, const ResultCallback & callback) {
	if (mState != Connected && mState != Closing){
		Log (LogWarning) << LOGID << "Strange state" << std::endl;
		notifyAsync (callback, error::WrongState);
		return;
	}
	// Building Message
	mRid++;
	BoshNodeBuilder builder;
	builder.addAttribute("rid", toString(mRid));
	builder.addAttribute("sid", mSid);
	builder.addAttribute("xmlns", "http://jabber.org/protocol/httpbind");
	for (StringMap::const_iterator i = additionalArgs.begin(); i != additionalArgs.end(); i++) {
		builder.addAttribute(i->first.c_str(), i->second);
	}

	// Adding data..
	for (std::deque<ByteArrayPtr>::const_iterator i = mOutputBuffer.begin(); i != mOutputBuffer.end(); i++) {
		builder.addContent(*i);
	}
	mOutputBuffer.clear();

	// Sending
	sf::ByteArrayPtr data = sf::createByteArrayPtr (builder.toString());
	mOpenRids[mRid] = data;
	mOpenRidCount++;
	executeRequest (data, mLongPollTimeoutMs, abind (dMemFun (this, &BoshTransport::onRequestReply), mRid, callback));
}

void BoshTransport::onRequestReply (Error result, const HttpResponsePtr & response, int64_t rid, const ResultCallback & originalCallback) {
	Log (LogInfo) << LOGID << "Reply of RID " << rid << ":" << toString (result) << " (" << (response ? response->resultCode : 0) << ")" << std::endl;
	if (mState != Connected && mState != Closing){
		if (mState != Unconnected)
			Log (LogWarning) << LOGID << "Strange state" << std::endl;
		return;
	}
	if (mAuthenticated && !response->authenticated){
		Log (LogWarning) << LOGID << "Lost authentication status" << std::endl;
		return failRequest (error::AuthError, rid, "Lost authentication", originalCallback);
	}
	if (result || response->resultCode != 200) {
		// Recovering from errors
		if (result) Log (LogWarning) << LOGID << "Got HTTP error " << toString (result) << std::endl;
		else
			if (response->resultCode != 200) Log (LogWarning) << LOGID << "Got bad HTTP result code " << response->resultCode << std::endl;

		mErrorCount++;
		mSumErrorCount++;
		if (mErrorCount > mMaxErrorCount) {
			return failRequest (result, rid, "To many HTTP errors", originalCallback);
		}
		// Try it again
		sf::ByteArrayPtr data = mOpenRids[rid];
		assert (data);
		Log (LogInfo) << LOGID << "Try to recover (ErrCnt=" << mErrorCount << " SucCnt=" << mSuccessCount << ")" << std::endl;
		return executeRequest (data, mLongPollTimeoutMs, abind (dMemFun (this, &BoshTransport::onRequestReply), rid, originalCallback));
	}
	mSuccessCount++;
	if (mErrorCount > 0) mErrorCount--; // so we can recover from errors
	BoshNodeParser parser;
	Error e = parser.parse(response->data->const_c_array(), response->data->size());
	if (e) {
		return failRequest (e, rid, "Parse error", originalCallback);
	}
	mOpenRids.erase(rid);
	mOpenRidCount--;

	mInWaitingRids[rid] = sf::createByteArrayPtr (parser.content());
	if (!parser.attribute ("type").empty()){
		// server sent terminate or something
		failRequest (error::Eof, rid, String ("Server sent type=" ) + parser.attribute ("type"), originalCallback);
	} else {
		notifyAsync (originalCallback, NoError);
	}
	insertWaitingInputData ();

	/// some braking
	sf::xcallTimed(dMemFun (this, &BoshTransport::continueWorking), sf::futureInMs(mReconnectWaitMs));
	if (mChanged)
		xcall (mChanged);
}

void BoshTransport::insertWaitingInputData () {
	RidDataMap::iterator i = mInWaitingRids.begin();
	while (i != mInWaitingRids.end()){
		if (i->first < mRidRecv) {
			Log (LogError) << LOGID << "Waiting data I already received?" << std::endl;
			mInWaitingRids.erase(i++);
		} else
		if (i->first == mRidRecv) {
			mInputBuffer.append(*i->second);
			mRidRecv++;
			mInWaitingRids.erase(i++);
		} else {
			// just data in the future..
			Log (LogInfo) << LOGID << "Receiving data out of order, received rid=" << i->first << " waiting Rid: " << mRidRecv << std::endl;
			return;
		}
	}
}

void BoshTransport::failRequest (Error result, int64_t rid, const String & msg, const ResultCallback & originalCallback) {
	notifyAsync (mChanged);
	Log (LogWarning) << LOGID << "Terminating connection with " << toString (result) << " " << msg << std::endl;
	Log (LogWarning) << LOGID << "Open Output data: " << mOutputBuffer.size() << " parts" << std::endl;
	Log (LogWarning) << LOGID << "Open Connections: " << mOpenRidCount << std::endl;

	if (result == error::Eof) {
		// not so bad, just terminated session
		mState = Unconnected;
		notifyAsync (originalCallback, NoError);
		return;
	} else {
		notifyAsync (originalCallback, mError);
	}
	mState = Other;
	mError = result;
	mErrorMessage = msg;
}


void BoshTransport::executeRequest (const ByteArrayPtr & data, int timeOutMs, const HttpContext::RequestCallback & callback) {
	HttpRequest req;
	req.start("POST", mUrl);
	req.addHeader("Content-Type", "text/xml; charset=utf-8");
	req.addContent(data);
	req.end();
	mContext.request(req, timeOutMs, callback);
}


}
