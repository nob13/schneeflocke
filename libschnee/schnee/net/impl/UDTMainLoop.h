#pragma once
#include <udt4/src/udt.h>
#include <schnee/sftypes.h>
#include <schnee/tools/Singleton.h>
#include <schnee/tools/async/DelegateBase.h>

#define SF_UDT_CHECK(X) \
	if (X < 0) {\
		sf::Log (LogError) << LOGID << #X << " failed: " << UDT::getlasterror().getErrorMessage() << std::endl;\
	}

namespace sf{
class UDTSocket;

/// An Event receiver for the UDTMainLoop
struct UDTEventReceiver : public DelegateBase {
	virtual ~UDTEventReceiver () {}
	virtual void onReadable  () = 0;
	/// UDT socket is now writeable.
	/// Return value: continue waiting for write events
	virtual bool onWriteable () = 0;
	virtual UDTSOCKET& impl  () = 0;
};

/// The main loop waiting for UDTSockets
/// Lock hierarchy: Never lock UDTMainLoop if you have your socket locked!
class UDTMainLoop : public Singleton <UDTMainLoop> {
public:
	friend class Singleton<UDTMainLoop>;

	/// Adds socket to read / error epoll set.
	void add    (UDTEventReceiver * socket);

	/// Adds socket for write events
	void addWrite (UDTEventReceiver * socket);

	/// Removes socket from all sets
	void remove (UDTEventReceiver * socket);

	// Main loop is currently running
	// (Does so if > 0 UDTEventReceivers are connected)
	bool isRunning ();

private:
	void start_locked ();

	void stop_locked ();

	void threadEntry ();

	UDTMainLoop ();
	~UDTMainLoop ();

	typedef std::map<UDTSOCKET, UDTEventReceiver *> SocketMap;
	typedef std::set<UDTSOCKET> SocketSet;
	SocketMap mSockets;
	SocketSet mWriteableWaiting;

	Mutex     mMutex;
	Condition mCondition;
	bool      mRunning;
	bool      mToQuit;
};

}
