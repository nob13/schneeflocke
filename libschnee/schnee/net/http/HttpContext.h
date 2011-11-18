#pragma once
#include <schnee/sftypes.h>
#include "HttpResponse.h"
#include "HttpRequest.h"

#include "impl/HttpResponseParser.h"
#include "impl/HttpConnectionManager.h"

#include "Url.h"

#include <schnee/tools/async/AsyncOpBase.h>

namespace sf {

/*
 * HTTP Functionality playground.
 * Target: Enough HTTP to implement XMPP over BOSH.
 */
class HttpContext : public AsyncOpBase {
public:
	HttpContext ();
	~HttpContext();

	typedef function <void (Error result, const HttpResponsePtr & response)> RequestCallback;

	// Instantiates a simple HTTP get call
	void get (const Url & url, int timeOutMs = 60000, const RequestCallback & callback = RequestCallback ());

	// Starts a custom HTTP call
	void request (const HttpRequest & request, int timeOutMs = 60000, const RequestCallback & callback = RequestCallback ());

	// Starts a custom HTTP call, comes back synchronously
	// (For testcases!)
	std::pair<Error, HttpResponsePtr> syncRequest (const HttpRequest & request, int timeOutMs = 60000);

	///@name Statistics
	///@{
	// Current active connections
	int pendingConnections () const { return mConnectionManager.pendingConnections(); }
	// Sum of all created connections
	int createdConnections () const { return mConnectionManager.createdConnections(); }
	///@}
private:
	enum AsyncOps { GetOperation = 1 };
	struct HttpGetOperation : public AsyncOp {
		enum State {
			HG_NULL,
			HG_WAITCONNECT,
			HG_RECEIVE,
			HG_READY
		};

		HttpGetOperation (const Time & timeout) : AsyncOp (GetOperation, timeout) {
			setState (HG_NULL);
			finished = false;
		}

		// Note: response may not be changed anymore, as it is given
		// to the originator via SharedPtr
		virtual void onCancel (sf::Error reason) {
			if (callback) callback (reason, response);
		}
		
		ByteArrayPtr request;
		RequestCallback callback;
		Url url;
		HttpConnectionPtr con;
		HttpResponsePtr response;
		HttpResponseParser parser;
		bool finished;
	};

	void onConnect (Error result, HttpConnectionPtr con, AsyncOpId id);
	void onChanged (AsyncOpId id);
	// Finish the operation
	void finish_locked (Error result, HttpGetOperation * op);
	HttpConnectionManager mConnectionManager;
};

}

