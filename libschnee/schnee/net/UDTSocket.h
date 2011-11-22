#pragma once
#include <udt4/src/udt.h>
#include <schnee/sftypes.h>
#include <schnee/net/UDPSocket.h>
#include "Channel.h"
#include "impl/UDTMainLoop.h"

namespace sf {

/// A socket based upon UDT (stream)
class UDTSocket : public UDTEventReceiver, public Channel {
public:
	UDTSocket ();
	/// Creates a sockt with explicit implementation structure
	UDTSocket (UDTSOCKET s);
	virtual ~UDTSocket ();
	
	/// Tries to connect another host
	/// Note: connect is BLOCKING in UDT
	Error connect (const String & host, int port);

	/// Tries to connect another host (rendezvous mode)
	/// Note: also blocking
	Error connectRendezvous (const String & host, int port);

	/// Tries to connect another host (rendezvous mode)
	/// Note: non blocking, using a temporary thread.
	void connectRendezvousAsync (const String & host, int port, const function <void (Error)> & callback);

	/// Binds the UDTSocket to a specific port (make it possible to connect to each other in rendezvous mode)
	Error bind (int port = 0);

	/// Reuses an UDPSocket. The UDPSocket will be closed
	Error rebind (UDPSocket * socket);

	/// Returns current bound port
	int port () const;

	/// Checks whether socket is connected (readable)
	bool isConnected () const;

	// Additional Tool Methods
	virtual sf::ByteArrayPtr peek (long maxSize = -1);
	virtual long bytesAvailable () const;

	// Implementation of Channel
	virtual Error error () const { LockGuard guard (mMutex); return mError; }
	virtual sf::String errorMessage () const { return ""; }
	virtual State state () const { return isConnected () ? Connected : Unconnected; }
	virtual Error write (const ByteArrayPtr& data, const ResultCallback & callback = ResultCallback());

	virtual sf::ByteArrayPtr read (long maxSize = -1);
	virtual void close (const ResultCallback & resultCallback = ResultCallback());

	virtual ChannelInfo info () const;
	virtual const char * stackInfo () const { return "udt";}
	virtual sf::VoidDelegate & changed () { return mChanged; }

	// For internal use: set default UDT buffer size
	static void setDefaultBufferSize (UDTSOCKET s);
private:
	Error connect_locked (const String & host, int port);

	bool isConnected_locked () const;

	void connectInOtherThread (const String & address, int port, bool rendezvous, const function <void (Error)> & callback);

	/// Initializes the socket
	void init (UDTSOCKET s);

	Error continueWriting_locked ();

	// Implementation of UDTEventReceiver
	virtual void onReadable  ();
	virtual bool onWriteable ();
	virtual UDTSOCKET& impl () { return mSocket; }
	
	
	mutable Mutex mMutex;
	Error       mError;
	UDTSOCKET   mSocket;
	ByteArray   mInputBuffer;
	bool        mConnecting;
	bool		mWriting;
	struct OutputElement {
		OutputElement () {}
		OutputElement (const ByteArrayPtr& _data, const ResultCallback& _callback) : data(_data), callback(_callback) {
		}
		ByteArrayPtr data;
		ResultCallback callback;
	};

	std::deque<OutputElement> mOutputBuffer;
	size_t      mOutputBufferSize;
	
	VoidDelegate mChanged;
	static const int  gInputTransferSize;
	static const int  gMaxInputBufferSize;
	static const int  gMaxOutputBufferSize;
};

}
