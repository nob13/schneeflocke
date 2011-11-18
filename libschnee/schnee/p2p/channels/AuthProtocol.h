#pragma once

#include <schnee/net/Channel.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/p2p/Datagram.h>
#include <schnee/p2p/DatagramReader.h>

namespace sf {

class Deserialization;

class DeadlineTimer;

/**
 * AuthProtocol checks a channel whether it is connected to another host with a given ID.
 * 
 * The protocol: 
 * 
 * In the header parameters, there is always a "from" and "to" value which maps to the other side of the channel.
 * 
 * createChannel{}     -->
 * 				       <-- createChannelAccept {}
 * channelAccept{}     -->
 * 
 * The protocol wants to be similar to TCP.
 * 
 * 
 * @note
 * - Here we could introduce "real" authentication, or may be better in TCPDispatcher?
 * - There is NO security here. In the diploma thesis it is called channel creation protocol.
 */
class AuthProtocol : public DelegateBase {
public:
	
	/// Initial command for creating channel
	struct CreateChannel {
		HostId from;
		HostId to;
		String version; // libschnee version
		SF_AUTOREFLECT_SDC;
	};
	/// Initial respond for creating an channel
	struct CreateChannelAccept {
		HostId from;
		HostId to;
		String version; // libschnee version
		SF_AUTOREFLECT_SDC;
	};
	/// Final accepting of channel (through initiator)
	struct ChannelAccept {
		HostId from;
		HostId to;
		SF_AUTOREFLECT_SDC;
	};
	/// Fail to create a channel
	struct ChannelFail {
		ChannelFail () : error (error::Other) {}
		Error error;
		String msg;
		SF_AUTOREFLECT_SDC;
	};
	
	/// constructs auth protocol and initializes it
	AuthProtocol (ChannelPtr channel, const sf::HostId & me);
	/// Constructs uninitialized auto protocol
	AuthProtocol ();

	/// Inits the AuthProtocol
	/// Can be done multiple times
	void init (ChannelPtr channel, const sf::HostId & me);

	~AuthProtocol ();
	
	enum State { SENT_CREATE_CHANNEL, WAIT_FOR_CREATE_CHANNEL, SENT_CREATE_CHANNEL_ACCEPT, FINISHED, TIMEOUT, AUTH_ERROR };
	
	/// Start as connecting entitiy, other must be set
	void connect (const sf::HostId & other, int timeOutInMs = 30000);
	
	/// Start as passive entity, other may not be set
	void passive (const sf::HostId & other = String (), int timeOutInMs = 30000);
	
	typedef sf::function< void (sf::Error success) > FinishedAuthDelegate;
	
	/// Delegate which get called when AuthProtocol finished (wether with success or not)
	FinishedAuthDelegate & finished () { return mFinished; }
	
	/// This was an active created connection (initiated via connect())
	/// Only makes sense when you call it in FINISHED state
	bool wasActive () const { return mWasActive; }

	/// Current state of the auth process 
	State state () const { return mState; }
	
	/// ID of other Entity
	const HostId& other () const { return mOther; }
private:
	
	/// Handler on channel changes
	void onChannelChange ();
	
	/// Handler for a timeout
	void onTimeOut ();
	
	/// Handler for error
	void onError (Error err);
	
	/// Handler if it is finished
	void onFinish ();
	
	/// Checks message parameters of an incoming message  (cmd == desiredCommand, and mMe <--> mOther)
	/// If setOther is true, than mOther-ID will be set and not checked
	bool checkParams (const sf::Deserialization & d, const String & cmd, const String & desiredCommand, bool setOther = false);
	
	HostId mMe;
	HostId mOther;
	ChannelPtr mChannel;
	bool mWasActive;
	
	FinishedAuthDelegate mFinished;
	TimedCallHandle mTimer;
	
	Mutex mMutex;
	
	State mState;

	DatagramReader mDatagramReader;
};

typedef shared_ptr<AuthProtocol> AuthProtocolPtr;

}
