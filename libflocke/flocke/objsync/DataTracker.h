#pragma once
#include <schnee/p2p/DataSharingClient.h>
#include <schnee/tools/async/DelegateBase.h>
namespace sf {

/**
 * DataTracker automatically follows a shared URI, and informs you
 * on changes along with the current revision.
 *
 * TODO:
 * The Delegate model is unsafe when used with indirect calls.
 */
class DataTracker : public DelegateBase {
public:

	enum TrackState { NONE, ESTABLISHING, TRACKING, LOST, ERROR, TO_DELETE };
	typedef function <void (const sf::Uri & uri, int revision, const sf::ByteArrayPtr & data)> DataUpdateCallback;
	typedef function <void (const sf::Uri & uri, TrackState state)> StateChangeCallback;

	DataTracker (DataSharingClient * client);
	~DataTracker ();

	/// Begins tracking of an URI
	Error track   (const sf::Uri & uri, const DataUpdateCallback & dataUpdateCallback, const StateChangeCallback & stateChangeCallback);

	/// Stops tracking of an URI
	Error untrack (const sf::Uri & uri);
	
private:
	struct TrackInfo {
		TrackInfo () : state (NONE), awaiting (0), revision (-1) {}
		TrackState         			state;
		
		DataUpdateCallback          dataUpdated;
		StateChangeCallback         stateChanged;
		
		sf::Uri            			uri;
		int 						awaiting;
		int							revision;
	};
	typedef sf::shared_ptr<TrackInfo> TrackInfoPtr;

	void onSubscribeReply (const HostId & sender, const ds::SubscribeReply & reply, TrackInfoPtr info);
	void onRequestReply   (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data, TrackInfoPtr info);
	void onNotify         (const HostId & sender, const ds::Notify & notify, TrackInfoPtr info);

	static const int AwaitSubscribe = 0x1;
	static const int AwaitRequest   = 0x2;

	typedef std::map<Uri, TrackInfoPtr> TrackInfoMap;

	TrackInfoMap  mTracked;	///< Current Tracked URIs
	DataSharingClient * mSharingClient;	///< DataSharing client
	sf::Mutex	  mMutex;
	int           mTimeOutMs;
};

}
