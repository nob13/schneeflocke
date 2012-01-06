#pragma once
#include <schnee/sftypes.h>
#include <sfserialization/autoreflect.h>
#include <schnee/net/Channel.h>
#include <sfserialization/autoreflect.h>


namespace sf {

/**
 * Channel management manages communication channels to other peers and is 
 * part of the InterplexBeacon facade.
 */
class ConnectionManagement {
public:
	virtual ~ConnectionManagement() {}
	
	typedef Channel::ChannelInfo ChannelInfo;

	/// Info about a connection to another host.
	struct ConnectionInfo {
		ConnectionInfo () : id (0) {}
		HostId      target;		///< Destination of connection
		int         level;		///< (internal) level of the connction, bigger is better
		ChannelInfo cinfo;		///< Info about the outer channel
		String      stack;		///< ',' separated names of network stack structure
		AsyncOpId   id;			///< A special channel id
		SF_AUTOREFLECT_SERIAL;
	};
	/// All available connections
	typedef std::vector<ConnectionInfo> ConnectionInfos;

	/// Request all connections (for GUIs etc.)
	virtual ConnectionInfos connections () const = 0;

	/// Returns info about best connection to a specific target. All data are empty if none found
	virtual ConnectionInfo connectionInfo (const HostId & target) const = 0;

	/// Returns level of channel (also see ConnectionManagement)
	virtual int channelLevel (const HostId & receiver) = 0;

	/// Lift the connection to the best possible value (asynchronous with callback)
	virtual Error liftConnection (const HostId & hostId, const ResultCallback & callback = ResultCallback(), int timeOutMs = 60000) = 0;
	
	/// Tries to lift the connection to someone to a given min level and returns CouldNotConnectHost if that level could not reached.
	virtual Error liftToAtLeast (int level, const HostId & hostId, const ResultCallback & callback = ResultCallback(), int timeOutMs = 60000) = 0;

	/// Close a channel to someone
	virtual Error closeChannel (const HostId & host, int level) = 0;


	///@name Delegates
	///@{

	/// Connection details of someone changed
	virtual VoidDelegate & conDetailsChanged () = 0;
	
	///@}
};

}
