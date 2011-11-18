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
	virtual Error setConnectionString (const String & s) {
		LockGuard guard (mMutex);
		bool suc = mDetails.setTo(s);
		return suc ? NoError : error::InvalidArgument;
	};

	virtual Error setPassword (const String & p){
		LockGuard guard (mMutex);
		mDetails.password = p;
		return NoError;
	}

	virtual Error connect (const XMPPStreamPtr& stream, int timeOutMs, const ResultCallback & callback = ResultCallback());
	virtual Error pureConnect (const XMPPStreamPtr & stream, int timeOutMs, const ResultCallback & callback = ResultCallback());
	virtual IMClient::ConnectionStateChangedDelegate & connectionStateChanged () { return mConnectionStateChanged; }
	virtual String errorText () const;

private:
	/// Starts connecting/registration
	Error startConnectingProcess_locked (const XMPPStreamPtr & stream, int timeOutMs,const ResultCallback & callback);

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
	void onConnectError_locked (Error e);
	void setPhase_locked (const String & phase);

	/// Change connection state
	void stateChange_locked (State s);

	XMPPStreamPtr mStream;
	State mState;
	mutable Mutex mMutex;
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
