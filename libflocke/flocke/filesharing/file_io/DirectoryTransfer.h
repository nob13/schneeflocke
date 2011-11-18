#pragma once
#include "FileTransfer.h"
#include <schnee/p2p/DataSharingClient.h>
#include <schnee/tools/SpeedMeasure.h>
#include "RecursiveDirectoryListing.h"

namespace sf {

/**
 * Transfers a whole dirctory from another host. This is done by 
 * first fetching the RecursiveDirectoryListing and then (after getting ready)
 * getting the download files via nextFileTransfer()
 */
class DirectoryTransfer : public Transfer {
public:
	DirectoryTransfer ();
	virtual ~DirectoryTransfer ();

	/// Starts a directory transfer
	Error start (DataSharingClient * client, const Uri & uri, const String & destinationDirName, int timeOutMs = -1);
	
	/// Struct describing the next transfer (for nextTransfer())
	struct FileTransferTask {
		Uri uri;
		String destinationFileName;
	};
	
	/// Saves the next transfer task in taskOut
	/// Returns Eof if there is no pending file transfer anymore
	/// (Also info()'s state will get to FINISHED)
	Error nextTransfer (FileTransferTask * taskOut);
	
	/// Cancel the whole transfer (if error is set it goes into ERROR state)
	void cancel (Error e = NoError);
	
	///@name Delegates
	///@{
	
	/// State has  changed
	VoidDelegate & stateChanged ()  { return mStateChanged; }

	///@}
	
	
private:
	
	/// Callback for listing request reply
	void onRequestReply (const HostId & source, const ds::RequestReply & reply, const ByteArrayPtr & data);
	void handleRequestReply_locked (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data); 
	// Handlers for the different states
	void handleTransmissionStarting_locked     (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data);
	void handleTransmissionTransferring_locked (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data);
	
	Uri    mUri;							///< Uri to be transferred
	ByteArray mListingData;					///< Collected listing data
	SpeedMeasure      * mSpeedMeasure;		///< Speed measure for the listing data, later for the whole directory download
	RecursiveDirectoryListing mListing;		///< The file listing to download
	RecursiveDirectoryListing::const_iterator mListingIterator; ///< Next file to be downloaded
	DataSharingClient * mClient;			///< Used DataSharingClient (for canceling transfers..)
	
	// Delegates
	VoidDelegate mStateChanged;
};

}
