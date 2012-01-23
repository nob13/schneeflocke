#include "BoshXMPPConnection.h"

namespace sf {

BoshXMPPConnection::BoshXMPPConnection() {
	SF_REGISTER_ME;
	mState = IMClient::CS_OFFLINE;
	mConnecting = false;
}

BoshXMPPConnection::~BoshXMPPConnection () {
	SF_UNREGISTER_ME;
}

#define LOG_STATE(X) Log(LogProfile) << LOGID << "Enter state " << X << std::endl;

XMPPConnection::State BoshXMPPConnection::state () const {
	return mState;
}

void BoshXMPPConnection::setConnectionDetails (XmppConnectDetails & details) {
	mDetails = details;
}

String BoshXMPPConnection::fullId () const {
	return mDetails.fullId();
}

Error BoshXMPPConnection::setPassword (const String & p) {
	mDetails.password = p;
	return NoError;
}

Error BoshXMPPConnection::connect (const XMPPStreamPtr& stream, int timeOutMs, const ResultCallback & callback) {
	return startConnect (true, stream, timeOutMs, callback);
}

Error BoshXMPPConnection::pureConnect (const XMPPStreamPtr & stream, int timeOutMs, const ResultCallback & callback) {
	return startConnect (false, stream, timeOutMs, callback);
}

String BoshXMPPConnection::errorText () const {
	return String();
}

Error BoshXMPPConnection::startConnect (bool withLogin, const XMPPStreamPtr & stream, int timeOutMs, const ResultCallback & callback) {
	if (mConnecting){
		Log (LogError) << LOGID << "There is already a connection process" << std::endl;
		return error::ExistsAlready;
	}
	ConnectingOp * op = new ConnectingOp (sf::regTimeOutMs (timeOutMs));
	op->setId (genFreeId());
	op->resultCallback = callback;
	op->withLogin = withLogin;
	op->transport = BoshTransportPtr (new BoshTransport());
	op->stream = stream;
	op->boss = this;

	if (mDetails.port == 0)
		mDetails.port = 443;

	// Starting connect
	Url url = Url (String ("https://") + mDetails.server + ":" + toString (mDetails.port) +  "/http-bind/");
	BoshTransport::StringMap addArgs;
	addArgs["to"] = mDetails.server;
	addArgs["xmpp:version"] = "1.0";
	addArgs["xmlns:xmpp"] = "urn:xmpp:xbosh"; // is important for ejabberd

	op->transport->connect(url, addArgs, op->lastingTimeMs(), abind (dMemFun (this, &BoshXMPPConnection::onBoshConnect), op->id()));
	op->setState (ConnectingOp::WAIT_BOSH_CONNECT);
	setState (IMClient::CS_CONNECTING);
	addAsyncOp (op);
	return NoError;
}

void BoshXMPPConnection::onBoshConnect (Error result, AsyncOpId id) {
	ConnectingOp * op;
	getReadyAsyncOpInState (id, CONNECT_OP, ConnectingOp::WAIT_BOSH_CONNECT, &op);
	if (!op) return;

	if (result) {
		return finalize (result, op);
	}

	op->stream->startInitAfterHandshake(op->transport);
	op->setState (ConnectingOp::WAIT_FEATURE);
	LOG_STATE ("WAIT_FEATURE");
	result = op->stream->waitFeatures(aOpMemFun (op, &BoshXMPPConnection::onFeatures));
	if (result) return finalize (result, op);
	addAsyncOp (op);
}

void BoshXMPPConnection::onFeatures (ConnectingOp * op, Error result) {
	if (result) {
		return finalize (result, op);
	}
	if (!op->withLogin) {
		// Yeah
		return finalize (NoError, op);
	}
	op->setState (ConnectingOp::WAIT_LOGIN);
	LOG_STATE ("WAIT_LOGIN");
	result = op->stream->authenticate(mDetails.username, mDetails.password, aOpMemFun (op, &BoshXMPPConnection::onLogin));
	if (result) return finalize (result, op);
	addAsyncOp (op);
}

void BoshXMPPConnection::onLogin (ConnectingOp * op, Error result) {
	if (result) {
		return finalize (result, op);
	}
	op->setState (ConnectingOp::WAIT_FEATURE2);
	op->transport->restart();
	op->stream->startInitAfterHandshake(op->transport);
	result = op->stream->waitFeatures(aOpMemFun (op, &BoshXMPPConnection::onFeatures2));
	if (result)
		return finalize (result, op);
	LOG_STATE ("WAIT_FEATURE2"); // Note: Internal state, not external state
	setState (IMClient::CS_AUTHENTICATING);

	addAsyncOp (op);
}

void BoshXMPPConnection::onFeatures2 (ConnectingOp * op, Error result) {
	if (result) {
		return finalize (result, op);
	}
	op->setState (ConnectingOp::WAIT_BIND);
	LOG_STATE ("WAIT_BIND");

	function <void (Error, const String &)> xx = aOpMemFun (op, &BoshXMPPConnection::onResourceBind);

	result = op->stream->bindResource(mDetails.resource, aOpMemFun (op, &BoshXMPPConnection::onResourceBind));
	if (result)
		return finalize (result, op);
	addAsyncOp (op);
}

void BoshXMPPConnection::onResourceBind (ConnectingOp * op, Error result, const String& fullJid) {
	if (result) {
		return finalize (result, op);
	}
	String expectedId = mDetails.fullId();
	if (fullJid != mDetails.fullId()){
		// Forbit this: Full JID must be predictable in order to get crypt/authentication working.
		Log (LogWarning) << LOGID << "Bound to wrong full jid! expected: " << expectedId << " found: " << fullJid << std::endl;
		return finalize (error::BadProtocol, op);
	}

	op->setState (ConnectingOp::WAIT_SESSION);
	LOG_STATE ("WAIT_SESSION");
	result = op->stream->startSession(aOpMemFun (op, &BoshXMPPConnection::onStartSession));
	if (result)
		return finalize (result, op);
	addAsyncOp (op);
}

void BoshXMPPConnection::onStartSession (ConnectingOp * op, Error result) {
	LOG_STATE ("READY");
	// we are through it
	finalize (result, op);
}


void BoshXMPPConnection::finalize (Error e, ConnectingOp * op, const char * errorText) {
	mConnecting = false;
	if (e) {
		Log (LogProfile) << LOGID << "Connect op failed in state " << op->state() << " due " << toString (e) << std::endl;
		notifyAsync (op->resultCallback, e);
		if (errorText)
			mErrorText = errorText;
		setState (IMClient::CS_ERROR);
		delete op;
		return;
	}
	Log (LogProfile) << LOGID << "Successfully logged in, rest time = " << op->lastingTimeMs();
	setState (IMClient::CS_CONNECTED);
	notifyAsync (op->resultCallback, NoError);
	delete op;
}

void BoshXMPPConnection::setState (State s) {
	if (mConnectionStateChanged)
		xcall (abind (mConnectionStateChanged, s));
	mState = s;
}


}
