#pragma once
#include "../DataSharingServer.h"
#include <schnee/tools/async/AsyncOpBase.h>
#include <schnee/tools/Log.h>
#include <schnee/tools/SpeedMeasure.h>

namespace sf {

using namespace ds;

/**
 * DataSharingServer implementation.
 *
 */
class DataSharingServerImpl : public DataSharingServer, public AsyncOpBase {
public:
	DataSharingServerImpl ();
	virtual ~DataSharingServerImpl ();

	SF_AUTOREFLECT_RPC;

	// Implementation of DataSharingServer
	virtual Error shutdown ();
	// virtual Error share (const Uri & uri, const ds::DataDescription & desc, const sf::ByteArrayPtr & data);
	virtual Error share (const Path & path, const SharingPromisePtr & data);

	virtual Error update  (const Path & path, const SharingPromisePtr & data);
	virtual Error unShare (const Path & path);
	virtual Error cancelTransfer (AsyncOpId id);


	virtual SharedDataDescMap shared () const;
	virtual SharedDataDesc  shared (const Path & path, bool * found) const;

	virtual PermissionDelegate & checkPermissions () { return mCheckPermissions; }

	// Overriding component
	virtual void onChannelChange (const HostId & host);


private:

	void onRpc (const HostId & sender, const Request & request, const ByteArray & data);
	void onRequestTransmission (const HostId & sender, const Request & request, const ByteArray & data);
	void onRpc (const HostId & sender, const Subscribe & subscribe, const ByteArray & data);
	void onRpc (const HostId & sender, const Push & push, const ByteArray & data);

	/// Send out a notification about the given path
	/// if sendIfStream is true, then send out the last revision
	virtual Error notify_locked (const Path & path);

	/// Continue sending a transmission
	void continueTransmission (Error lastError, AsyncOpId id);

	// Information about shared data
	struct SharedData {
		SharedData () : currentRevision (0) {}
		SharingPromisePtr promise;
		int currentRevision;	///< Current revision of the data, <= 0 is invalid

		HostSet subscribers;

		// for debug output
		void serialize (Serialization & s) const {
			s ("currentRevision", currentRevision);
			s ("subscribers", subscribers);
		}
	};
	typedef std::map<Path, SharedData> SharedDataMap;
	friend void serialize (Serialization &, const SharedDataMap &);


	enum AsyncCmdType { TRANSMISSION = 1 };

	/// A currently sending transmission
	/// Initialized by Request (Mode=Transmission)
	struct Transmission : public AsyncOp {
		Transmission (const sf::Time& _timeOut) : AsyncOp (TRANSMISSION, _timeOut), chunkSize (8192) {}
		int revision;					///< Revision to be sent
		Range range;					///< Range of transmission
		ds::TransmissionInfo info;		///< Info (also contains receiver/destination)
		int requestId;					///< Id of the receiver' request
		int chunkSize;					///< Size of one chunk (bytes)
		int count;						///< Number of all chunks
		int nextChunk;					///< Num of next chunk
		SpeedMeasure speedMeasure;		///< Measuring transfer speed
		DataPromisePtr promise;			///< Promise of the transfer
		void serialize (Serialization & s) const {
			s ("revision", revision);
			s ("range", range);
			s ("info", info);
			s ("requestId" ,requestId);
			s ("chunkSize", chunkSize);
			s ("count", count);
			s ("nextChunk", nextChunk);
		}

		void onCancel (sf::Error reason) {
			Log (LogWarning) << LOGID << "Transmission canceled, cause=" << reason << std::endl;
			info.mark  = RequestReply::TransmissionCancel;
			info.error = reason;
			if (promise)
				promise->onTransmissionUpdate_locked(id(), info);
		}
	};
	
	/// Helper struct to find a Transmission with just having its requestId
	struct TransmissionFinder {
		TransmissionFinder (AsyncOpId _requestIdToFind) : requestIdToFind (_requestIdToFind) {}
		AsyncOpId requestIdToFind;
		typedef	std::vector<AsyncOpId> ResultVec;
		ResultVec result;
		void operator()(const AsyncOp * candidate) {
			if (candidate->type() != TRANSMISSION) return;
			const Transmission * t = static_cast<const Transmission*> (candidate);
			if (t->requestId == requestIdToFind)
				result.push_back(t->id());
		}
	};

	int64_t mTransmissionSentBytes;				///< Currently sent bytes (within current handleTransmissions)
	int     mTransmissionTimeOutMs;
	bool    mWaitForNextTransmissionHandler;	///< Waiting for a timeout where next transmission handler is called
	
	/// Holds all the shared data
	SharedDataMap mShared;

	// Delegates
	PermissionDelegate mCheckPermissions;
};

// for debugging purposes
inline void serialize (Serialization & s, const DataSharingServerImpl::SharedDataMap & map) {
	for (DataSharingServerImpl::SharedDataMap::const_iterator i = map.begin(); i != map.end(); i++) {
		sf::String key = i->first.toString();
		s (key.c_str(), i->second);
	}
}



}
