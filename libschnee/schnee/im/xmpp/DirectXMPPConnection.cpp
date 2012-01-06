#include "DirectXMPPConnection.h"

namespace sf {

DirectXMPPConnection::DirectXMPPConnection(){
	SF_REGISTER_ME;
	mState = IMClient::CS_OFFLINE;
	mWithLogin = false;
}

DirectXMPPConnection::~DirectXMPPConnection() {
	SF_UNREGISTER_ME;
}


DirectXMPPConnection::State DirectXMPPConnection::state () const {
	return mState;
}

Error DirectXMPPConnection::connect (const XMPPStreamPtr& stream, int timeOutMs, const ResultCallback & callback) {
	mWithLogin = true;
	return startConnectingProcess (stream, timeOutMs, callback);
}

Error DirectXMPPConnection::pureConnect (const XMPPStreamPtr & stream, int timeOutMs, const ResultCallback & callback) {
	mWithLogin = false;
	return startConnectingProcess (stream, timeOutMs, callback);
}

String DirectXMPPConnection::errorText () const {
	return mErrorText;
}

Error DirectXMPPConnection::startConnectingProcess (const XMPPStreamPtr & stream, int timeOutMs,const ResultCallback & callback){
	if (mWithLogin && (mDetails.username.empty() || mDetails.server.empty())) return error::NotInitialized;
	mStream    = stream;
	stateChange (IMClient::CS_CONNECTING);
	mErrorText.clear();
	mTcpSocket = TCPSocketPtr (new TCPSocket());
	Log (LogInfo) << LOGID << "Attempting to connect XMPP Service " << mDetails.server << ":" << mDetails.port << " timeout=" << timeOutMs << "ms" << std::endl;
	if (mDetails.port == 0)
		mDetails.port = 5222;
	Error e = mTcpSocket->connectToHost(mDetails.server,mDetails.port, timeOutMs, dMemFun (this, &DirectXMPPConnection::onTcpConnect));
	if (e) return e;
	if (timeOutMs > 0){
		mTimeoutHandle = xcallTimed (dMemFun (this, &DirectXMPPConnection::onConnectTimeout), sf::regTimeOutMs(timeOutMs));
	}
	mConnectCallback = callback;
	stateChange (IMClient::CS_CONNECTING);
	mConnecting = true;
	setPhase ("TCP Connect");
	return NoError;
}

void DirectXMPPConnection::onTcpConnect (Error result) {
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	mStream->setInfo ("", mDetails.server);
	Error e = mStream->startInit (mTcpSocket, dMemFun (this, &DirectXMPPConnection::onBasicStreamInit));
	setPhase ("Basic Stream Init");
	if (e) onConnectError (e);
}

void DirectXMPPConnection::onBasicStreamInit (Error result) {
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	Error e = mStream->waitFeatures(dMemFun (this, &DirectXMPPConnection::onBasicStreamFeatures));
	setPhase ("Basic Feature Wait");
	if (e) onConnectError (e);
}

void DirectXMPPConnection::onBasicStreamFeatures (Error result) {
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	Error e = mStream->requestTls(dMemFun (this, &DirectXMPPConnection::onTlsRequestReply));
	setPhase ("Request TLS");
	if (e) onConnectError (e);
}

void DirectXMPPConnection::onTlsRequestReply (Error result) {
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	mStream->uncouple();
	// Kicking up TLS
	mTlsChannel = TLSChannelPtr (new TLSChannel (mTcpSocket));
	mTcpSocket.reset();
	Error e = mTlsChannel->clientHandshake(TLSChannel::X509, dMemFun (this, &DirectXMPPConnection::onTlsHandshakeResult));
	setPhase ("TLS Handshake");
	if (e) onConnectError (e);
}

void DirectXMPPConnection::onTlsHandshakeResult(Error result){
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	Error e = mStream->startInit(mTlsChannel, dMemFun (this, &DirectXMPPConnection::onTlsStreamInit));
	setPhase ("TLS Stream Reinit");
	if (e) onConnectError (e);
}

void DirectXMPPConnection::onTlsStreamInit (Error result) {
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	Error e = mStream->waitFeatures(dMemFun (this, &DirectXMPPConnection::onTlsStreamFeatures));
	setPhase ("TLS Stream Feature Wait");
	if (e) onConnectError (e);
}

void DirectXMPPConnection::onTlsStreamFeatures (Error result) {
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	if (!mWithLogin) {
		mConnecting = false;
		sf::cancelTimer (mTimeoutHandle);
		notifyAsync (mConnectCallback, NoError);
		mConnectCallback.clear();
		return;
	}
	stateChange (IMClient::CS_AUTHENTICATING);
	Error e = mStream->authenticate(mDetails.username, mDetails.password, dMemFun (this, &DirectXMPPConnection::onAuthenticate));
	setPhase ("Authenticate");
	if (e) onConnectError (e);
}

void DirectXMPPConnection::onAuthenticate(Error result) {
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	Error e = mStream->startInit(mTlsChannel, dMemFun (this, &DirectXMPPConnection::onFinalStreamInit));
	setPhase ("Final Stream Init");
	if (e) onConnectError (e);
}

void DirectXMPPConnection::onFinalStreamInit(Error result) {
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	Error e = mStream->waitFeatures(dMemFun (this, &DirectXMPPConnection::onFinalStreamFeatures));
	setPhase ("Final Stream Features");
	if (e) onConnectError (result);
}

void DirectXMPPConnection::onFinalStreamFeatures (Error result) {
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	Error e = mStream->bindResource(mDetails.resource, dMemFun (this, &DirectXMPPConnection::onFinalBind));
	setPhase ("Bind Resource");
	if (e) onConnectError (result);
}

void DirectXMPPConnection::onFinalBind (Error result, const String & fullJid) {
	if (result) return onConnectError (result);
	if (!mConnecting) return;
	String expectedId = mDetails.fullId();
	if (fullJid != mDetails.fullId()){
		// Forbit this: Full JID must be predictable in order to get crypt/authentication working.
		Log (LogWarning) << LOGID << "Bound to wrong full jid! expected: " << expectedId << " found: " << fullJid << std::endl;
		return onConnectError (error::BadProtocol);
	}
	Error e = mStream->startSession (dMemFun (this, &DirectXMPPConnection::onSessionStart));
	setPhase ("Session Start");
	if (e) onConnectError (result);
}

void DirectXMPPConnection::onSessionStart (Error result) {
	if (result) return onConnectError (result);
	// we are done!
	mConnecting = false;
	stateChange (IMClient::CS_CONNECTED);
	mTlsChannel.reset(); // stored in Stream now
	sf::cancelTimer(mTimeoutHandle);
	notifyAsync (mConnectCallback, NoError);
	mConnectCallback.clear();
}

void DirectXMPPConnection::onConnectTimeout () {
	mTimeoutHandle = TimedCallHandle();
	onConnectError (error::TimeOut);
}

void DirectXMPPConnection::onConnectError(Error e){
	if (!mConnecting) return; // to late?
	Log (LogWarning) << LOGID << "Connect failed due " << toString(e) << " in state " << toString (mState) << std::endl;
	mErrorText = String ("Connect error ") + toString (e) + " in phase " + mPhase;
	mConnecting = false;
	mState = IMClient::CS_ERROR;
	mTlsChannel.reset();
	mTcpSocket.reset();
	notifyAsync (mConnectCallback, e);
	mConnectCallback.clear();
}

void DirectXMPPConnection::setPhase (const String & phase) {
	Log (LogInfo) << LOGID << "Entering phase " << phase << std::endl;
}

void DirectXMPPConnection::stateChange (State s) {
	mState = s;
	if (mConnectionStateChanged)
		xcall (abind (mConnectionStateChanged, s));
}


}
