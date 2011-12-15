#pragma once
#include <schnee/sftypes.h>
#include <schnee/net/Channel.h>
#include <schnee/tools/async/AsyncOpBase.h>
#include "../Url.h"

namespace sf {

/// Describes one http connection
struct HttpConnection {
	ChannelPtr channel;
	ByteArray  inputBuffer;
	String host; // host (including port if given)
	String protocol;
};
typedef shared_ptr<HttpConnection> HttpConnectionPtr;

/**
 * HttpConnectionManager manages open connections to different HTTP/HTTPS servers
 * thus enabling reuse of existing connections.
 */
class HttpConnectionManager : public AsyncOpBase {
public:
	HttpConnectionManager ();
	~HttpConnectionManager ();


	typedef function<void (Error, HttpConnectionPtr)> RequestConnectionCallback;

	/// Request a connection to some server (part of Url is used)
	void requestConnection (const Url & url, int timeOutMs, const RequestConnectionCallback & callback);

	/// Gives back a connection to the pool so it can be reused
	void giveBack (Error lastResult, const HttpConnectionPtr & connection);

	int pendingConnections () const { return mPendingConnectionsCount; }
	int createdConnections () const { return mCreatedConnections; }


private:
	enum AsyncOps { EstablishConnection = 1, PendingConnection };

	struct EstablishConnectionOp : public AsyncOp {

		EstablishConnectionOp (const Time & timeout) : AsyncOp (EstablishConnection, timeout) {
			setState (Null);
		}
		enum State { Null, WaitTcpConnect, WaitTls };

		virtual void onCancel (sf::Error reason) {
			if (callback) callback (reason, HttpConnectionPtr());
		}
		HttpConnectionPtr connection;
		RequestConnectionCallback callback;
	};

	/// AsyncOp provides good timeout behaviour, so we use it
	/// for managing pending connections
	struct PendingConnectionOp : public AsyncOp {
		PendingConnectionOp (const Time & timeout) : AsyncOp (PendingConnection, timeout) {}
		HttpConnectionManager * boss;
		HttpConnectionPtr connection;
		virtual void onCancel (sf::Error reason);
	};
	friend struct PendingConnectionOp;

	void onTcpConnect   (Error result, AsyncOpId id);
	void onTlsHandshake (Error result, AsyncOpId id);
	void doFinish_locked (Error result, EstablishConnectionOp * op);

	/// Channel changes on a pending op
	void onChannelChange (AsyncOpId);

	/// Connection identifier, protocol + host.
	typedef std::pair<String, String> ConId;
	typedef std::list<AsyncOpId> PendingConnectionList;
	typedef std::map<ConId, PendingConnectionList> PendingConnectionMap;

	PendingConnectionMap mPendingConnections;
	void addToPendingConnections_locked (PendingConnectionOp * op);
	void removeFromPendingConnections (ConId c, AsyncOpId id);
	void removeFromPendingConnections_locked (PendingConnectionOp * op);
	void removeFromPendingConnections_locked (ConId c, AsyncOpId id);

	int mGeneralTimeoutMs; ///< General timeout for holding connections
	int mPendingConnectionsCount;
	int mCreatedConnections;
};

}
