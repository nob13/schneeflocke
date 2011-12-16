#pragma once
#include "../ConnectionManagement.h"
#include "../CommunicationComponent.h"
#include "../CommunicationMultiplex.h"
#include <schnee/tools/async/AsyncOpBase.h>
#include "SmoothingFilter.h"
#include <schnee/p2p/channels/ChannelProvider.h>
#include <schnee/p2p/Datagram.h>
#include <schnee/p2p/DatagramReader.h>

#include "ChannelHolder.h"
#include "ChannelPinger.h"


namespace sf {

/// Implementation of ConnectionManagement with different channel providers
class GenericConnectionManagement : public CommunicationDelegate, public ConnectionManagement, public AsyncOpBase {
public:
	typedef shared_ptr<ChannelProvider>  ChannelProviderPtr;
	typedef ChannelHolder::ChannelId ChannelId;

	GenericConnectionManagement ();
	virtual ~GenericConnectionManagement();

	/// Adds and initializes an ChannelProvider
	/// Do it before init()
	Error addChannelProvider  (ChannelProviderPtr channelProvider, int priority = 0);

	/// Init generic connection management
	/// Connect the delegates first!
	/// Do it after adding channel providers
	Error init (CommunicationMultiplex * multiplex);

	/// Uninitialize the connection management
	/// Must be done before disconnecting channels (or unregistering delegate!)
	void uninit ();

	/// Clears all connections
	void clear ();

	/// Clears connections to a specific user
	void clear (const HostId & id);

	/// Set host id to all Channel Providers
	void setHostId (const HostId & hostId);

	/// Starts ping measurement
	void startPing ();

	/// Stops delay measurement
	void stopPing ();

	//	Implementation of ConnectionManagement
	virtual ConnectionInfos connections () const;
	virtual Error liftConnection (const HostId & hostId, const ResultCallback & callback, int timeOutMs);
	virtual Error liftToAtLeast  (int level, const HostId & hostId, const ResultCallback & callback, int timeOutMs);
	virtual Error closeChannel (const HostId & host, int level);

	virtual VoidDelegate & conDetailsChanged () { return mConDetailsChanged; }

	// Implementation of CommunicationDelegate
	virtual Error send     (const HostId & receiver, const Datagram & datagram, const ResultCallback & callback = ResultCallback ());
	virtual Error send     (const HostSet & receivers, const sf::Datagram & datagram);
	virtual int channelLevel (const HostId & receiver);



private:

	enum ConnectionManagementOperations { LIFT_CONNECTION = 1};

	/// Lift connection to another peer
	struct LiftConnectionOp : public AsyncOp {
		enum State { Start, CreateInitial, Lift };
		LiftConnectionOp (Time timeOut) : AsyncOp (LIFT_CONNECTION, timeOut) { mState = Start; lastLevelTried = -1; minLevel = 1; }

		int lastLevelTried;				///< Last level tried to lift to
		int minLevel;					///< Minimum level which must be reached to not count as an error
		HostId target;
		ResultCallback callback;

		virtual void onCancel (sf::Error reason) {
			notify (callback, reason);
		}
	};

	///@name Lifting a connection, async operations
	///@{

	/// Does trying to create an initial channel (op is not yet added!)
	void startLifting (LiftConnectionOp * op);

	/// Does acutal lifting (op is not yet added!)
	void lift (LiftConnectionOp * op);

	/// Callback for channel creation on Lifting operation
	void onChannelCreate (Error error, bool wasInitial, AsyncOpId id);

	///@}

	/// Callback if a channel was created
	void onChannelCreated (const HostId & target, ChannelPtr channel, bool requested, int level);

	/// Received a datagram
	void onIncomingDatagram (const HostId & source, const String & cmd, const Deserialization & ds, const ByteArray & data);

	/// Callback if some channel is changed
	void onChannelChanged (ChannelId id, const HostId & target, int level);

	/// Returns best channel provider with given maximum level
	/// if maxLevel is < 0 it returns the best provider at all
	/// if initial is true it returns only providers who can give initial channels
	/// returns ChannelProviderPtr() if no such provider is found
	/// level will be set to the provider level if given
	ChannelProviderPtr bestProvider (int maxLevel, bool initial, int * level = 0);

	typedef std::map<int, ChannelProviderPtr> ChannelProviderMap;
	ChannelProviderMap  mChannelProviders; ///< Channel Providers associated with their Priority

	HostId         mHostId;

	CommunicationMultiplex * mCommunicationMultiplex; ///< Bound communication components

	// Components
	ChannelHolder mChannels;
	ChannelPinger mChannelPinger;

	// Delegates
	VoidDelegate mConDetailsChanged;
};

}
