#pragma once
#include "initHelpers.h"

namespace sf {
namespace test {

/// A sceneario which can be used in multiple testcases
class StandardScenario {
public:
	StandardScenario () : mServer(0), mNodeCount (0), mSimulated (false) {
		mCollector = LocalChannelUsageCollectorPtr(new LocalChannelUsageCollector());
	}
	virtual ~StandardScenario ();

	// Base class  for all peers in the scenario
	struct Peer {
		Peer (InterplexBeacon * b) { beacon = b;}
		virtual ~Peer () { delete beacon; }


		InterplexBeacon * beacon;

		/// Returns host id of the peer
		HostId hostId () const { return beacon->presences().hostId();}

		/// Returns online state of the peer
		OnlineState onlineState () const { return beacon->presences().onlineState(); }

		/// Returns true if peer can see another peer
		/// TODO: Too complicated, behaviour needs to be in the Beacon
		bool canSee (Peer * p) const {
			PresenceManagement::HostInfoMap hostMap = beacon->presences().hosts();
			PresenceManagement::HostInfoMap::const_iterator i = hostMap.find (p->hostId());
			if (i == hostMap.end()) return false;
			return true;
		}

		/// Returns true if peer is fast connected to another
		bool fastConnected (Peer * p) const {
			return beacon->connections().channelLevel(p->hostId()) >= 10;
		}

		/// Peer dependants con overwrite this, will be started before conneting it all
		virtual void beforeConnect () {}
	};

	/// Overwrite this in order to get your own peers in here
	virtual Peer * createPeer (InterplexBeacon * beacon) { return new Peer (beacon); }

	/// Inits the StandardScenario
	/// (Comfort variant)
	sf::Error init (int nodeCount = 8, bool withServer = true, bool simulated = true);

	typedef sf::function <InterplexBeacon * ()> BeaconCreator;

	/// Inits the Standardscenario using a given BeaconCreator
	sf::Error initWithBeaconCreator (int nodeCount, bool withServer, const BeaconCreator & beaconCreator, bool simulated);

	/// Connect the beacons to the service and waits that all peers see each other
	/// (But does not connect peers to each other)
	sf::Error connectThem (int timeOutMs = 2000);
	
	/// Builds fast connections between each other (must be connected to the network)
	sf::Error liftThem (int timeOutMs = 1000);

	/// Do it all 
	sf::Error initConnectAndLift (int nodeCount = 8, bool withServer = true, bool simulated = true);

	/// Close all channels between all parties
	sf::Error closeThem ();

	/// Checks if there is any open connection
	bool noOpenConnections () const;

	/// Wait until there is no open connection anymore
	bool waitForNoOpenConnections (int timeOutMs) const;

	/// Checks if there are no redundant connections between peers
	bool noRedundantConnections () const;

	/// Wait until all redundant connections are gone
	bool waitForNoRedundantConnections (int timeOutMs) const;

	/// Returns true if all peers are online
	bool allOnline () const;

	/// Returns true if all peers are seing each other
	bool peersSeeEachOther () const;

	/// Returns true if all peers are lifted / fast connected to each other
	bool fastConnected () const;

	/// Wait for no pending data (simulated only)
	bool waitForNoPendingData (int timeOutMs = 1000) const;

	/// Returns true if scenario uses a simulated network
	bool isSimulated () const { return mSimulated; }

protected:
	typedef std::vector<Peer*> PeerVec;
	PeerVec mPeers;							///< Peers playing in the scenario
	Peer *  mServer;							///< The server peer, if any
	int     mNodeCount;						///< Number of nodes
	bool    mSimulated;						///< Its a simulated network

	Network mNetwork;						///< Simulation network (if simulated)
	LocalChannelUsageCollectorPtr mCollector;	///< Collected data (if simulated)
};


/// A Test scenario with a data sharing client and a server inside the peers
/// Note: permission testing is disabled by default.
class DataSharingScenario : public StandardScenario {
public:
	virtual ~DataSharingScenario () {}

	static bool alwaysTrue () { return true; }

	struct DataSharingPeer : public Peer {
		DataSharingClient * client; ///< will automatically be deleted by the beacon
		DataSharingServer * server; ///< will be automatically be deleted by the beacon
		DataSharingPeer (InterplexBeacon * beacon) : Peer (beacon) {
			client = DataSharingClient::create();
			server = DataSharingServer::create();
			server->checkPermissions()    = bind (&DataSharingScenario::alwaysTrue);
			beacon->components().addComponent(client);
			beacon->components().addComponent(server);
		}
	};

	/// Gives you up cast access to the peers
	DataSharingPeer * peer (int i) { assert (i < mNodeCount); return (DataSharingPeer*) mPeers[i]; }

	virtual Peer * createPeer (InterplexBeacon * beacon) { return new DataSharingPeer (beacon); }
};

}
}
