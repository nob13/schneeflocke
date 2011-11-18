#pragma once
#include <schnee/tools/async/DelegateBase.h>
#include "UDPSocket.h"

namespace sf {

/// Tryes to figure out external addresses of an UDPSocket using shipped UDPEchoServer
class UDPEchoClient : public DelegateBase {
public:
	enum State { NOSTATE, WAIT, READY, TIMEOUT };

	UDPEchoClient (UDPSocket & socket);

	~UDPEchoClient ();

	typedef function<void (Error e)> ResultDelegate;
	ResultDelegate & result () { return mResultDelegate; }

	const String& address () { return mAddress; }
	int port () { return mPort; }

	/// Starts echoing, calls you back via result().
	void start (String echoServer, int echoPort, int timeOutMs);

private:
	void onReadyRead ();

	void onTimeOut ();

	ResultDelegate mResultDelegate;
	UDPSocket & mSocket;
	Mutex mMutex;

	String mAddress;
	int    mPort;
	String mToken;
	State mState;
	TimedCallHandle mTimeoutHandle;

};

}
