#pragma once
#include "XMPPConnection.h"

namespace sf {

/// Tool class for registering on a XMPP service
class XMPPRegistration : public DelegateBase {
public:
	XMPPRegistration ();
	~XMPPRegistration ();

	struct Credentials {
		String username;
		String password;
		String email;
	};
	Error start (const String & server, const Credentials & credentials, int timeOutMs, const ResultCallback & callback);

private:

	// Process...
	void onConnect (Error result);
	void onRegisterGet (Error result, const String & instructions, const std::vector<String> & fields);
	void onRegisterSet (Error result);

	void onTimeout ();

	void end_locked (Error result);
	/// Shutdown connection (uncoupled)
	void onShutdown ();

	Mutex mMutex;
	bool mProcessing;
	shared_ptr<XMPPStream> mStream;
	typedef shared_ptr<XMPPConnection> XMPPConnectionPtr;
	XMPPConnectionPtr mConnection;
	TimedCallHandle mTimeoutHandle;
	ResultCallback mResultCallback;
	Credentials mCredentials;
};

}
