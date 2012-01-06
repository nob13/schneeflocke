#pragma once
#include "XMPPConnection.h"
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/net/TCPSocket.h>
#include <schnee/net/TLSChannel.h>

namespace sf {

/**
 * Implements the state machine for connecting XMPP.
 *
 * TLS/TCP version
 */
class DirectXMPPConnection : public XMPPConnection, public DelegateBase {
public:

	DirectXMPPConnection();
	virtual ~DirectXMPPConnection();

	// Implementation of XMPPConnection
	virtual State state () const;

	virtual void setConnectionDetails (XmppConnectDetails & details) {
		mDetails = details;
	}
	virtual String fullId () const {
		return mDetails.fullId();
	}

	virtual Error setPassword (const String & p){
		mDetails.password = p;
		return NoError;
	}

	virtual Error connect (const XMPPStreamPtr& stream, int timeOutMs, const ResultCallback & callback = ResultCallback());
	virtual Error pureConnect (const XMPPStreamPtr & stream, int timeOutMs, const ResultCallback & callback = ResultCallback());
	virtual IMClient::ConnectionStateChangedDelegate & connectionStateChanged () { return mConnectionStateChanged; }
	virtual String errorText () const;

private:
	/// Starts connecting/registration
	Error startConnectingProcess (const XMPPStreamPtr & stream, int timeOutMs,const ResultCallback & callback);

	// Connecting And Registration Process..
	void onTcpConnect (Error result);
	void onBasicStreamInit (Error result);
	void onBasicStreamFeatures (Error result);
	void onTlsRequestReply (Error result);
	void onTlsHandshakeResult (Error result);
	void onTlsStreamInit (Error result);
	void onTlsStreamFeatures (Error result);
	// Authenticating...
	void onAuthenticate (Error result);
	void onFinalStreamInit (Error result);
	void onFinalStreamFeatures (Error result);
	void onFinalBind (Error result, const String& fullJid);
	void onSessionStart (Error result);

	void onConnectTimeout ();
	void onConnectError (Error e);
	void setPhase (const String & phase);

	/// Change connection state
	void stateChange (State s);

	XMPPStreamPtr mStream;
	State mState;
	XmppConnectDetails mDetails;
	TCPSocketPtr  mTcpSocket;
	TLSChannelPtr mTlsChannel;
	bool mConnecting;
	bool mWithLogin; // != Skip authentication
	TimedCallHandle mTimeoutHandle;
	ResultCallback  mConnectCallback;
	String mPhase;
	String mErrorText;
	IMClient::ConnectionStateChangedDelegate mConnectionStateChanged;
};

}
