#include "HttpConnectionManager.h"
#include <schnee/net/TCPSocket.h>
#include <schnee/net/TLSChannel.h>

namespace sf {

HttpConnectionManager::HttpConnectionManager () {
	SF_REGISTER_ME;
	mGeneralTimeoutMs = 30000;
	mPendingConnectionsCount  = 0;
	mCreatedConnections = 0;
}

HttpConnectionManager::~HttpConnectionManager() {
	SF_UNREGISTER_ME;
}

void HttpConnectionManager::requestConnection (const Url & url, int timeOutMs, const RequestConnectionCallback & callback) {
	assert (callback);
	{
		// Check the pool
		PendingConnectionMap::iterator i = mPendingConnections.find(ConId(url.protocol(), url.host()));
		if (i != mPendingConnections.end()){
			while (!i->second.empty()){
				AsyncOpId id = i->second.front();
				i->second.pop_front();
				PendingConnectionOp * op;
				getReadyAsyncOp (id, PendingConnection, &op);
				mPendingConnectionsCount--;
				if (op) {
					Log (LogInfo) << LOGID << "Reusing connection to " << url.protocol() << "/" << url.host() << " id=" << op->id() << std::endl;
					xcall (abind(callback, NoError, op->connection));
					delete op;
					return;
				}
			}
			// not existant anymore
			mPendingConnections.erase (i);
		}
	}
	EstablishConnectionOp * op = new EstablishConnectionOp (regTimeOutMs (timeOutMs));

	const String & protocol = url.protocol();
	if (protocol != "http" && protocol != "https") {
		Log (LogWarning) << LOGID << "Unsupported protocol: " << protocol << std::endl;
		return xcall(abind(callback, error::NotSupported, HttpConnectionPtr()));
	}
	op->callback = callback;
	op->connection = HttpConnectionPtr (new HttpConnection());
	op->connection->host = url.host();
	op->connection->protocol = url.protocol();
	TCPSocketPtr sock = TCPSocketPtr (new TCPSocket ());
	op->connection->channel = sock;
	op->setId(genFreeId());
	op->setState(EstablishConnectionOp::WaitTcpConnect);
	Error e = sock->connectToHost(url.pureHost(), url.port(), timeOutMs, abind(dMemFun(this, &HttpConnectionManager::onTcpConnect), op->id()));
	if (e) {
		delete op;
		return xcall (abind(callback, e, HttpConnectionPtr()));
	}
	addAsyncOp (op);
}

static void throwAway (const HttpConnectionPtr & con) {
	// nothing
}

void HttpConnectionManager::giveBack (Error lastResult, const HttpConnectionPtr & connection) {
	if (lastResult) {
		// throw away, asynchronous
		// we do not trust this anymore...
		xcall (abind (&throwAway, connection));
		return;
	}
	PendingConnectionOp * op = new PendingConnectionOp (regTimeOutMs (mGeneralTimeoutMs));
	AsyncOpId id = genFreeId();
	op->setId(id);
	op->connection = connection;
	op->boss = this;
	op->connection->channel->changed() = abind (dMemFun (this, &HttpConnectionManager::onChannelChange), id);
	addToPendingConnections_locked (op);
	Log (LogInfo) << LOGID << "Storing pending connection to " << connection->host <<  " id=" << id << std::endl;
	addAsyncOp (op);
}

void HttpConnectionManager::PendingConnectionOp::onCancel (sf::Error reason) {
	Log (LogInfo) << LOGID << "Closing connection to " << connection->host << " due " << toString (reason) << std::endl;
	// Removing indirekt
	// As asyncop mutex is locked
	// if we would lock boss->mMutex here this would lead to a deadlock
	// As a consequence mPendingConnections can always hold more ids than available.
	xcall (abind (dMemFun (boss, &HttpConnectionManager::removeFromPendingConnections), ConId (connection->protocol, connection->host), id()));
}

void HttpConnectionManager::onTcpConnect (Error result, AsyncOpId id) {
	EstablishConnectionOp * op;
	getReadyAsyncOpInState (id, EstablishConnection, EstablishConnectionOp::WaitTcpConnect, &op);
	if (!op) return;

	if (result) {
		return doFinish_locked (result, op);
	}

	if (op->connection->protocol == "https") {
		// Add another TLS layer onto it...
		TLSChannelPtr tlsChannel = TLSChannelPtr (new TLSChannel (op->connection->channel));
		op->connection->channel = tlsChannel;
		tlsChannel->clientHandshake(TLSChannel::X509, abind (dMemFun(this, &HttpConnectionManager::onTlsHandshake), id));
		op->setState (EstablishConnectionOp::WaitTls);
		addAsyncOp (op);
		return;
	}
	// plain http, can finish now..
	doFinish_locked (result, op);
}

void HttpConnectionManager::onTlsHandshake (Error result, AsyncOpId id) {
	EstablishConnectionOp * op;
	getReadyAsyncOpInState (id, EstablishConnection, EstablishConnectionOp::WaitTls, &op);
	if (!op) return;
	doFinish_locked (result, op);
}

void HttpConnectionManager::doFinish_locked (Error result, EstablishConnectionOp * op) {
	RequestConnectionCallback cb = op->callback;
	HttpConnectionPtr con = result ? HttpConnectionPtr() : op->connection;
	if (!result) {
		mCreatedConnections++;
	} else {
		// TLSChannel doesn't like to be deleted while giving out a callback
		xcall (abind (&throwAway, op->connection));
	}
	delete op;
	xcall (abind (cb, result, con));
}

void HttpConnectionManager::onChannelChange (AsyncOpId id) {
	PendingConnectionOp * op;
	getReadyAsyncOp (id, PendingConnection, &op);
	if (!op) return;

	ByteArrayPtr all = op->connection->channel->read();
	if (all && !all->empty()){
		Log (LogInfo) << LOGID << "Recv in pending connection: " << *all << std::endl;
		op->connection->inputBuffer.append(*all);
	}

	Error e = op->connection->channel->error();
	if (e) {
		// adieu
		Log (LogInfo) << LOGID << "Closing channel to " << op->connection->host << " as channel reported error: " << toString (e) << std::endl;
		xcall (abind (&throwAway, op->connection));
		removeFromPendingConnections_locked (op);
		delete op;
		return;
	}
	// data ready? nobody knows, add it again...
	addAsyncOp (op);
}

void HttpConnectionManager::addToPendingConnections_locked (PendingConnectionOp * op) {
	mPendingConnections[std::make_pair(op->connection->protocol, op->connection->host)].push_back(op->id());
	mPendingConnectionsCount++;

}

void HttpConnectionManager::removeFromPendingConnections (ConId c, AsyncOpId id) {
	removeFromPendingConnections_locked (c,id);
}

void HttpConnectionManager::removeFromPendingConnections_locked (PendingConnectionOp * op) {
	return removeFromPendingConnections_locked (ConId (op->connection->protocol, op->connection->host), op->id());
}

void HttpConnectionManager::removeFromPendingConnections_locked (ConId c, AsyncOpId id) {
	PendingConnectionMap::iterator i = mPendingConnections.find (c);
	if (i != mPendingConnections.end()){
		for (PendingConnectionList::iterator j = i->second.begin(); j != i->second.end(); j++) {
			if (*j == id){
				i->second.erase(j);
				if (i->second.empty()){
					// list is empty now, we can remove the whole key
					mPendingConnections.erase(i);
				}
				mPendingConnectionsCount--;
				return;
			}
		}
	}
	// Not finding may happen due to asynchronicity between giving out connections
	// And deleting them on TimeOuts.
}


}
