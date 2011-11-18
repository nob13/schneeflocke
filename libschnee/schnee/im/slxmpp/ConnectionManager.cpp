#include "ConnectionManager.h"
#include <schnee/tools/Log.h>
#include <assert.h>

namespace sf {

Connection::Connection (String ownJid, String dstJid, String addr, int port) :
	mAddress (addr), mPort (port), mState (OFFLINE) {
	SF_REGISTER_ME;
	init ();
	mStream->setInfo(ownJid,dstJid);
}

Connection::Connection(String ownJid, TCPSocketPtr _socket) {
	SF_REGISTER_ME;
	mState = OFFLINE;
	mSocket  = _socket;
	mPort = 0;
	mAddress = "";
	init ();
	mStream->setInfo (ownJid);
}

Connection::~Connection () { 
	SF_UNREGISTER_ME;
	close (); 
}	

bool Connection::prepare (){
	LockGuard guard (mMutex);
	if (mState != OFFLINE) return false;
	mState = PREPARE;
	return true;
}

Error Connection::initStream (const ResultCallback & callback){
	LockGuard guard (mMutex);
	if (mState != PREPARE) return error::WrongState;
	mState = INIT;
	mSocket = shared_ptr<TCPSocket> (new TCPSocket ());
	Error e = mSocket->connectToHost (mAddress, mPort, 10000, abind (dMemFun (this, &Connection::onTcpConnectResult), callback));
	return e;
}

Error Connection::answerStream (const ResultCallback & callback){
	LockGuard guard (mMutex);
	if (mState != PREPARE) return error::WrongState;

	mState = RESPOND;
	// there must be a conncetion already
	if (!mSocket->isConnected()){
		Log (LogWarning) << LOGID << "No connection available for answering" << std::endl;
		setState_locked (OFFLINE);
		return error::NotInitialized;
	}
	Error e = mStream->respondInit(mSocket, abind (dMemFun (this, &Connection::onStreamInitResult), callback));
	if (e) {
		setState_locked (OFFLINE);
	}
	return e;
}

Error Connection::startSession() {
	LockGuard guard (mMutex);
	assert (mStream);
	if (mState != TO_OPENED) return error::WrongState;

	for (std::vector<xmpp::Message>::const_iterator i = mToSend.begin(); i != mToSend.end(); i++){
		mStream->sendMessage(*i);
	}
	sf::Log (LogInfo) << LOGID << "Sent " << mToSend.size() << " messages after going online " << std::endl;
	mToSend.clear();
	mState = FINISHED;
	return NoError;
}

void Connection::close(){
	LockGuard guard (mMutex);
	if (mStream) mStream->close();
	if (mSocket) {
		mSocket->close();
	}
	setState_locked (OFFLINE);
}

bool Connection::send (const xmpp::Message & m) {
	sf::LockGuard guard (mMutex);
	if (mState == OFFLINE) return false;
	if (mState == FINISHED) {
		mStream->sendMessage(m);
		return true;
	}
	mToSend.push_back (m);
	return true;
}

void Connection::onTcpConnectResult (Error result, const ResultCallback & originalCallback) {
	if (result) {
		originalCallback(result);
		return;
	}
	LockGuard guard (mMutex);
	Error e = mStream->startInit(mSocket, abind (dMemFun (this, &Connection::onStreamInitResult), originalCallback));
	if (e) {
		originalCallback (e);
	}

}

void Connection::onStreamInitResult (Error result, const ResultCallback & originalCallback) {
	Log (LogInfo) << LOGID << "On Stream init result: " << toString (result) << std::endl;
	if (result) {
		originalCallback (result);
		return;
	}
	{
		LockGuard guard (mMutex);
		setState_locked (TO_OPENED);
	}
	originalCallback (NoError);
}

void Connection::init () {
	mStream = shared_ptr<XMPPStream> (new XMPPStream());
	mStream->incomingMessage() = dMemFun (this, &Connection::onMessage);
	mStream->asyncError() = dMemFun (this, &Connection::onStreamClosed);
	mStream->closed() = dMemFun (this, &Connection::onStreamClosed);
}

void Connection::onStreamClosed (){
	close ();
}

ConnectionManager::ConnectionManager (){
	SF_REGISTER_ME;
	mServer.newConnection() = dMemFun (this, &ConnectionManager::onNewConnection);
}

ConnectionManager::~ConnectionManager (){
	SF_UNREGISTER_ME;
	stop ();
}

bool ConnectionManager::startServer (const String & jid, int port){
	LockGuard guard (mMutex);
	if (isOnline()){
		Log (LogError) << LOGID << "Server already running" << std::endl;
		return false;
	}
	mJid = jid;
	bool ret = mServer.listen(port);
	if (!ret) return false;
	return true;
}

void ConnectionManager::stop() {
	LockGuard guard (mMutex);
	mServer.close();
	// will wait for pending operations automatically
}

bool ConnectionManager::isOnline () const {
	return mServer.isListening();
}

void ConnectionManager::updateTarget (const String & id, const String & address, int port){
	LockGuard guard (mMutex);
	ConnectionMap::iterator i = mConnections.find(id);
	if (i == mConnections.end()){
		// Generate a new one
		ConnectionPtr c = ConnectionPtr (new Connection(mJid, id, address, port));
		mConnections[id] = c;
	} else {
		// Update existing one
		ConnectionPtr & c = i->second;
		assert (c->dstJid() == id);
		c->updateNet (address, port);
	}
}		

void ConnectionManager::removeTarget (const String & id){
	LockGuard guard (mMutex);
	ConnectionMap::iterator i = mConnections.find (id);
	if (i == mConnections.end()){
		Log (LogProfile) << LOGID << "Did not found a connection to " << id << std::endl;
	} else {
		i->second->close();
		mConnections.erase (i);
	}
}

bool ConnectionManager::send (const xmpp::Message & m) {
	LockGuard guard (mMutex);
	ConnectionPtr c = createGetConnection_locked (m.to);
	if (c) {
		c->send (m);
		return true;
	}
	return false;
}

// PRIVATE

void ConnectionManager::onNewConnection (){
	if (!mServer.isListening()) return; // maybe already closed
	LockGuard guard (mMutex);
	TCPSocketPtr sock = mServer.nextPendingConnection();

	ConnectionPtr c = ConnectionPtr (new Connection(mJid, sock));
	c->prepare();
	Error e = c->answerStream(abind (dMemFun(this, &ConnectionManager::onAnswerResult), c));
	if (e) {
		Log (LogWarning) << LOGID << "Could not answer incoming connection: " << toString (e) << std::endl;
	}
}

void ConnectionManager::onAnswerResult (Error result, const ConnectionPtr & con) {
	LockGuard guard (mMutex);
	if (result) {
		Log (LogWarning) << LOGID << "Could not answer connection" << std::endl;
		return;
	}
	String id = con->dstJid();
	ConnectionMap::iterator i = mConnections.find (id);
	if (i != mConnections.end()){
		// Oh, we have already one connection to this client?!
		// We should not allow hijacking
		Log (LogInfo) << LOGID << "Multiple connections for " << id << " first: " << i->second->address() << ":" << i->second->port() << " second: " << con->address() << ":" << con->port() << std::endl;

		ConnectionPtr s = i->second;
		if (s->state () != Connection::OFFLINE){
			// There was already a real connection open
			Log (LogError) << LOGID << "Multiple connections for " << id << " first: " << s->address() << ":" << s->port() << " second: " << con->address() << ":" << con->port() << std::endl;
			Log (LogError) << LOGID << "Rejecting the second.. " << std::endl;
			con->close ();
			return;
		} // else: we will overwrite the original
	}
	mConnections[id] = con;
	con->messageReceived() = dMemFun (this, &ConnectionManager::onIncomingMessage);
	con->startSession();
}

Error ConnectionManager::buildConnection_locked (ConnectionPtr con) {
	Error e = con->initStream(abind (dMemFun (this, &ConnectionManager::onStreamInitResult), con));
	if (e) {
		Log (LogWarning) << LOGID << "Could not build connection to " << con->dstJid() << std::endl;
		return e;
	}
	con->messageReceived() = dMemFun (this, &ConnectionManager::onIncomingMessage);
	return NoError;
}

void ConnectionManager::onStreamInitResult (Error result, ConnectionPtr con) {
	LockGuard guard (mMutex);
	if (result){
		Log(LogWarning) << LOGID << "Could not init stream to " << con->dstJid() << std::endl;
		return;
	}
	Error e = con->startSession();
	if (e) {
		Log (LogWarning) << LOGID << "Could not start session to " << con->dstJid() << std::endl;
	}
}

ConnectionManager::ConnectionPtr ConnectionManager::createGetConnection_locked (const String & target) {
	ConnectionMap::iterator i = mConnections.find (target);
	if (i == mConnections.end()){
		Log (LogWarning) << LOGID << "Did not found target " << target << " in database" << std::endl;
#ifdef _DEBUG
		Log (LogWarning) << LOGID << "Possible Targets: " << std::endl;
		for (i = mConnections.begin(); i != mConnections.end(); i++){
			Log (LogWarning) << i->first << std::endl;
		}
#endif
		return ConnectionPtr ();
	} else {
		ConnectionPtr & c = i->second;
		if (!c->prepare ()) return c;
		Error e = buildConnection_locked (c);
		if (e) return ConnectionPtr ();
		return c;
	}
}

}
