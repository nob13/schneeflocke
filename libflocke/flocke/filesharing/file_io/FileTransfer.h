#pragma once
#include <schnee/sftypes.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/p2p/DataSharingClient.h>
#include <schnee/tools/SpeedMeasure.h>

namespace sf {

/// Information about one transfer, visible to the user
struct TransferInfo {
	enum Type { DIR_TRANSFER, FILE_TRANSFER };
	enum State { NOSTATE = 0,	///< No state yet 
		STARTING, 				///< Transfer is up to start
		TRANSFERRING, 			///< Is transferring somethign
		TRANSFERRED_LISTING,	///< Directorytransfer transferred the whole listing  (directory transfer only)
		PENDING_FILES, 			///< Waits for files to be downloaded (directory transfer only)
		FINISHED, 				///< Transfer is finished
		CANCELED, 				///< Transfer is canceled
		ERROR					///< There was an error 
	};

	/// General Transfer change
	enum TransferUpdateType {
		Changed		///< Transfer was just changed
		, Added		///< Transfer was added
		, Removed	///< Transfer was removed
	};

	TransferInfo () : state (NOSTATE), size(0), transferred(0), speed(0), error (NoError), parent (0) {}
	
	State state;
	Type type;
	HostId source;
	HostId receiver;
	Uri uri;
	String filename;	///< Destination file name or destination directory, platform specific
	ds::DataDescription desc;
	int64_t size;
	int64_t transferred;
	float speed;
	Error error;
	AsyncOpId parent;	///< a parent (directory) transfer (0 = no Parent)
	SF_AUTOREFLECT_SERIAL;
};
SF_AUTOREFLECT_ENUM (TransferInfo::Type);
SF_AUTOREFLECT_ENUM (TransferInfo::State);

/// Base class for File/Directory-Transfers
class Transfer : public DelegateBase {
public:
	TransferInfo info () const { LockGuard (mMutex); return mInfo; };

	/// Cancel a transfer
	virtual void cancel (Error cause = NoError) = 0;

	virtual ~Transfer() {}

protected:

	/// Go into an error state (also returns the error, so that you can return it again)
	Error errorState_locked (Error e) {
		mInfo.state = TransferInfo::ERROR;
		mInfo.error = e;
		return e;
	}
	mutable Mutex mMutex;
	TransferInfo mInfo;
};


// A File Transfer
class FileTransfer : public Transfer {
public:
	FileTransfer (AsyncOpId parent = 0);
	virtual ~FileTransfer ();

	VoidDelegate & stateChanged () { return mStateChanged; }
	Error start (DataSharingClient * client, const Uri & uri, const String & destinationFileName, int timeOutMs = -1);
	
	/// Handles answers of datasharing client and sets state
	void onRequestReply (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data);
	
	// Implementation of Transfer
	virtual void cancel (Error cause = NoError);

private:
	
	void handleRequestReply_locked (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data); 
	
	// Handlers for the different states
	void handleTransmissionStarting_locked     (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data);
	void handleTransmissionTransferring_locked (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data);
	
	/// Cleanup handlers and subtypes
	void cleanup_locked ();
	
	FILE * mDestination;
	SpeedMeasure * mSpeedMeasure;
	VoidDelegate mStateChanged;
	DataSharingClient * mClient;
	AsyncOpId mId;///< Data sharing client id
};


}
