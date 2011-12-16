#include "TCPChannelConnector.h"
#include <schnee/net/Tools.h>
#include <schnee/tools/Log.h>

namespace sf {

TCPChannelConnector::TCPChannelConnector () {
	SF_REGISTER_ME;
	mTimeOutMs = 10000;
	mServer.newConnection () = dMemFun (this, &TCPChannelConnector::onNewConnection);
}

TCPChannelConnector::~TCPChannelConnector () {
	SF_UNREGISTER_ME;
}

sf::Error TCPChannelConnector::start (int port) {
	bool suc = mServer.listen (port);
	if (!suc) {
		mProtocol.setOwnDetails (ConnectDetailsPtr());
		return error::ServerError;
	}
	mAddresses.clear();
	sf::net::localIpv4Addresses (&mAddresses, true);	// if it fails, there won't be any addresses
	sf::Log(LogInfo) << LOGID << "Local addresses" << toString (mAddresses) << std::endl;

	ConnectDetailsPtr details = ConnectDetailsPtr (new ConnectDetails);
	details->error     = sf::NoError;
	details->port      = mServer.serverPort();
	details->addresses = mAddresses;
	mProtocol.setOwnDetails (details);
	return sf::NoError;
}

void TCPChannelConnector::stop () {
	mServer.close();
}

bool TCPChannelConnector::isStarted () const {
	return mServer.isListening();
}

int TCPChannelConnector::port () const {
	return mServer.serverPort();
}

TCPChannelConnector::AddressVec TCPChannelConnector::addresses () const {
	return mAddresses;
}

void TCPChannelConnector::setTimeOut (int timeOutMs){
	mTimeOutMs = timeOutMs;
}

HostId TCPChannelConnector::hostId () const {
	return mHostId;
}

sf::Error TCPChannelConnector::createChannel (const HostId & target, const ResultCallback & callback, int timeOutMs) {
	AsyncOpId id = genFreeId ();
	CreateChannelOp * op = new CreateChannelOp (sf::regTimeOutMs(timeOutMs));
	op->callback = callback;
	op->setId(id);
	op->setState(CreateChannelOp::Start);
	op->target   = target;
	addAsyncOp (op);
	// cannot do that yet; as we are called from a possible locked Beacon.
	// and the tcp connect protocoll will also lock the beacon.
	xcall (aOpMemFun  (op, &TCPChannelConnector::startConnecting));
	return NoError;
}

void TCPChannelConnector::setHostId (const sf::HostId & id) {
	mHostId = id;
}

void TCPChannelConnector::startConnecting (CreateChannelOp * op) {
	int timeMs = op->lastingTimeMs(0.5f);
	op->setState (CreateChannelOp::AwaitTcpInfo);
	Error result = mProtocol.requestDetails (op->target, aOpMemFun (op, &TCPChannelConnector::onTcpConnectResult), timeMs);
	if (result) {
		if (op->callback) notifyAsync (op->callback, result);
		delete op;
		return;
	}
	addAsyncOp (op);
}


void TCPChannelConnector::onTcpConnectResult (CreateChannelOp * op, Error result, const ConnectDetails& details) {
	if (result){
		// error, kill op
		if (op->callback) xcall (abind (op->callback, result));
		delete op;
		return;
	}
	op->connectDetails = details;
	op->nextAddress    = op->connectDetails.addresses.begin();
	op->setState (CreateChannelOp::Connecting);
	addAsyncOp (op);
	xcall (aOpMemFun (op, &TCPChannelConnector::connectNext));
}

void TCPChannelConnector::connectNext (CreateChannelOp * op){
	if (op->nextAddress == op->connectDetails.addresses.end()){
		Error e = op->hasFailedAuthentication ? error::AuthError : error::CouldNotConnectHost;
		notifyAsync (op->callback, e);
		delete op;
		return;
	}
	op->socket = TCPSocketPtr (new TCPSocket());
	String next = * (op->nextAddress++);

	int leftTime = op->lastingTimeMs(0.66);
	Log (LogInfo) << LOGID << "Start connecting to " << next << ":" << op->connectDetails.port << " with a timeOut of " << leftTime << "ms" << std::endl;
	op->socket->connectToHost (next, op->connectDetails.port, leftTime, aOpMemFun (op, &TCPChannelConnector::onConnect));
	addAsyncOp (op);
}

void TCPChannelConnector::onConnect (CreateChannelOp * op, Error result) {
	if (result)
		connectNext (op);
	else {
		// Try to encrypt it
		op->setState (CreateChannelOp::TlsHandshaking);
		op->tlsChannel = TLSChannelPtr (new TLSChannel (op->socket));
		Error e = op->tlsChannel->clientHandshake(TLSChannel::DH, aOpMemFun (op, &TCPChannelConnector::onTlsHandshake));
		if (e) {
			xcall (abind (aOpMemFun (op, &TCPChannelConnector::onTlsHandshake), e));
		}
		addAsyncOp (op);
	}
}

void TCPChannelConnector::onTlsHandshake (CreateChannelOp * op, Error result) {
	if (result) {
		op->hasFailedEncryption = true;
		Log (LogProfile) << LOGID << "TLS Handshake failed: " << toString (result) << std::endl;
		connectNext (op);
		return;
	}
	// Try to authenticate it
	Log (LogInfo) << LOGID << "TLS Handshake successfull, authenticating..." << std::endl;
	op->setState (CreateChannelOp::Authenticating);
	op->authProtocol.init (op->tlsChannel, mHostId);
	op->authProtocol.finished() = aOpMemFun (op, &TCPChannelConnector::onAuthProtocolFinished);
	op->authProtocol.connect (op->target, op->lastingTimeMs(0.66));
	addAsyncOp (op);
}

void TCPChannelConnector::onAuthProtocolFinished (CreateChannelOp * op, Error result) {
	if (result) {
		Log (LogInfo) << LOGID << "Authentication failed (" << toString (result) << ") for " << op->target << std::endl;
		// Could not authenticate; maybe connected to wrong entity
		op->hasFailedAuthentication = true;
		op->setState (CreateChannelOp::Connecting);
		// try it with another address
		connectNext (op);
		return;
	}
	// Send out callbacks
	notifyAsync (mChannelCreated, op->target, op->tlsChannel, true);
	notifyAsync (op->callback, NoError);
	delete op;
}

void TCPChannelConnector::onNewConnection (){
	TCPSocketPtr socket = mServer.nextPendingConnection();
	assert (socket);
	if (!socket) return;
	if (mHostId.empty()){
		Log (LogWarning) << LOGID << "Getting incoming connection but no host id set yet, throwing away" << std::endl;
		return;
	}
	AsyncOpId id = genFreeId ();
	AcceptConnectionOp * op = new AcceptConnectionOp (sf::regTimeOutMs(mTimeOutMs));
	op->setId(id);
	op->setState (AcceptConnectionOp::TlsHandshaking);
	op->socket = socket;
	op->tlsChannel = TLSChannelPtr (new TLSChannel (op->socket));
	Error e = op->tlsChannel->serverHandshake(TLSChannel::DH, aOpMemFun (op, &TCPChannelConnector::onAcceptTlsHandshake));
	if (e) {
		Log (LogWarning) << LOGID << "TLS failed immediately on new connection" << toString (e) << std::endl;
		delete op;
		return;
	}
	addAsyncOp (op);
	Log (LogInfo) << LOGID << "New connection, starting tls handshake, time left " << op->lastingTimeMs() << std::endl;
}

void TCPChannelConnector::onAcceptTlsHandshake (AcceptConnectionOp * op, Error result) {
	if (result) {
		Log (LogInfo) << LOGID << "Encrypting failed (" << toString (result) <<") for incoming connection" << std::endl;
		delete op;
		return;
	}
	Log (LogInfo) << LOGID << "TLS Handshake successfull, authenticating..." << std::endl;
	op->setState (AcceptConnectionOp::Authenticating);
	op->authProtocol.init (op->tlsChannel, mHostId);
	op->authProtocol.finished () = aOpMemFun (op, &TCPChannelConnector::onAcceptAuthFinished);
	op->authProtocol.passive(String(), -1);
	addAsyncOp (op);
}

void TCPChannelConnector::onAcceptAuthFinished (AcceptConnectionOp * op, Error result) {
	if (result) {
		Log (LogInfo) << LOGID << "Authentication failed (" << toString (result) << ") for incoming connection from " << op->authProtocol.other() << std::endl;
	} else {
		notifyAsync (mChannelCreated, op->authProtocol.other(), op->tlsChannel, false);
	}
	delete op;
}


}
