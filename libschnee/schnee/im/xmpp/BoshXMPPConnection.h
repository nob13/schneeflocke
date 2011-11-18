#pragma once
#include <schnee/im/xmpp/XMPPConnection.h>
#include <schnee/tools/async/AsyncOpBase.h>
#include "bosh_impl/BoshTransport.h"

namespace sf {

/**
 * XMPP via BOSH.
 * Only HTTPS is supported at the Moment.
 *
 * The connection string is the same like in XMPPConnection.
 *
 * Default Port: 443 (HTTPS)
 * Always uses /http-bind/ for URL.
 *
 *
 */
class BoshXMPPConnection : public XMPPConnection, public AsyncOpBase {
public:
	BoshXMPPConnection();
	~BoshXMPPConnection ();

	// Implementation of XMPPConnection
	virtual State state () const;
	virtual Error setConnectionString (const String & s);
	virtual Error setPassword (const String & p);
	virtual Error connect (const XMPPStreamPtr& stream, int timeOutMs, const ResultCallback & callback = ResultCallback());
	virtual Error pureConnect (const XMPPStreamPtr & stream, int timeOutMs, const ResultCallback & callback = ResultCallback());
	virtual IMClient::ConnectionStateChangedDelegate & connectionStateChanged () { return mConnectionStateChanged; }
	virtual String errorText () const;

private:
	enum OpTypes { CONNECT_OP };
	struct ConnectingOp : public AsyncOp {
		enum State { CO_NULL, WAIT_BOSH_CONNECT, WAIT_FEATURE, WAIT_LOGIN, WAIT_FEATURE2, WAIT_BIND, WAIT_SESSION };
		ConnectingOp (const Time & timeout) : AsyncOp (CONNECT_OP, timeout) {
			withLogin = false;
			setState (CO_NULL);
		}
		virtual void onCancel (sf::Error reason) {
			boss->mConnecting = false;
			if (resultCallback) {
				resultCallback (reason);
			}
		}

		BoshXMPPConnection * boss;
		ResultCallback resultCallback;
		XMPPStreamPtr stream;
		bool withLogin;
		BoshTransportPtr transport; //< Bosh Transport (HTTP core!)
	};

	Error startConnect_locked (bool withLogin, const XMPPStreamPtr & stream, int timeOutMs, const ResultCallback & callback);
	void onBoshConnect (Error result, AsyncOpId id);
	void onFeatures_locked (ConnectingOp * op, Error result);
	void onLogin_locked (ConnectingOp * op, Error result);
	void onFeatures2_locked (ConnectingOp * op, Error result);
	void onResourceBind_locked (ConnectingOp * op, Error result, const String& fullJid);
	void onStartSession_locked (ConnectingOp * op, Error result);
	// Stops operation, if e == NoError it is assumed to be ready.
	// Note: op will be deleted
	void finalize_locked (Error e, ConnectingOp * op, const char * errorText = 0);
	void setState_locked (State s);


	XmppConnectDetails mDetails; //< Connection details

	State  mState;
	String mErrorText;

	// Connecting Process
	bool mConnecting;

	// Delegates
	IMClient::ConnectionStateChangedDelegate mConnectionStateChanged;
};

}
