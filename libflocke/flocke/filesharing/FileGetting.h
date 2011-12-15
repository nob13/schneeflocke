#pragma once
#include <schnee/sftypes.h>
#include <schnee/p2p/DataSharingClient.h>
#include <stdio.h>
#include <schnee/tools/async/DelegateBase.h>
#include "file_io/DirectoryListing.h"
#include "file_io/FileTransfer.h"
#include "file_io/DirectoryTransfer.h"

namespace sf {
class SpeedMeasure;
/**
 * File getting makes it possible to
 * - track files of other users
 * - to download them
 * - to watch download speed etc.
 *
 * It can download files which are shared by FileSharing.
 *
 * Tracking is done via a simple delegate interface; only
 * one delegate object is allowed.
 */
class FileGetting : public DelegateBase {
public:
	typedef sf::TransferInfo TransferInfo;

	FileGetting (DataSharingClient * client);
	~FileGetting ();

	/// Init File Getting component
	/// Can be done multiple times
	sf::Error init ();

	// List a directory from someone elese
	Error listDirectory (const Uri & uri, AsyncOpId * opIdOut = 0);

	/// Begin transfering some uri (file transfer)
	/// Saves the transfer id, if opIdOut is non null and request was successfull
	sf::Error request (const Uri & uri, AsyncOpId * opIdOut = 0);
	
	/// Begins transfering a whole directory
	Error requestDirectory (const Uri & uri, AsyncOpId * opIdOut = 0);

	/// Cancels a transfer
	Error cancelTransfer (AsyncOpId id);

	/// Remove a transfer (canceling if its ongoing)
	Error removeTransfer (AsyncOpId id);

	/// Remove all finished transfers (in CANCELED, ERROR or FINISHED state)s
	Error removeFinishedTransfers ();

	typedef std::map<AsyncOpId, TransferInfo> TransferInfoMap;

	/// Current transfers
	TransferInfoMap transfers () const;

	/// Returns default destination directory
	String destinationDirectory () const;

	/// Sets default destination directory
	void setDestinationDirectory (const String & dir);
	
	///@name Delegates
	///@{

	typedef function<void (AsyncOpId id, TransferInfo::TransferUpdateType type, const TransferInfo & t)> UpdatedTransferDelegate;
	typedef function<void (const Uri & uri, Error error, const DirectoryListing& listing)> GotListingDelegate;
	
	/// A transfer is updated
	UpdatedTransferDelegate & updatedTransfer () { return mUpdatedTransfer; }
	
	/// A listing is transferred
	GotListingDelegate  & gotListing () { return mGotListing; }
	
	///@}
private:
	
	typedef shared_ptr<Transfer> TransferPtr;
	typedef shared_ptr<FileTransfer> FileTransferPtr;
	typedef shared_ptr<DirectoryTransfer> DirectoryTransferPtr;
	typedef std::map<AsyncOpId, TransferPtr> TransferMap;
	
	// Transfer changed
	void onTransferChange (AsyncOpId id, TransferInfo::TransferUpdateType type);
	
	// Starts the next child transfer of a DirectoryTransfer
	Error startNextChildTransfer_locked (AsyncOpId id);
	
	/// Starts a file transfer
	Error requestFileTransfer_locked (const Uri & uri, const String & fileName, AsyncOpId parent, AsyncOpId * opIdOut = 0);
	
	// for directory listing
	void onListingReply (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data);

	AsyncOpId nextId_locked () { return mNextId++; }

	DataSharingClient * mClient;

	UpdatedTransferDelegate mUpdatedTransfer;
	GotListingDelegate mGotListing;
	
	
	TransferMap mTransfers;
	AsyncOpId   mNextId;
	int mTimeOutMs;
	int mFilesPerDirectory;					///< How many files at once while downloading a directory

	String mDestinationDirectory;	///< Directory where downloaded files gets placed (if no other name given)
};

}
