#pragma once
#include <schnee/p2p/DataSharingServer.h>
#include <schnee/tools/async/DelegateBase.h>

#include <flocke/sharedlists/SharedListServer.h>

#include "file_io/FileSharingPromise.h"
#include "file_io/DirectorySharingPromise.h"
#include "file_io/DirectoryListing.h"
#include "file_io/Globber.h"
#include "file_io/FileTransfer.h"


namespace sf {

/**
 * Shares files / directories and updates SharedListServer.
 */
class FileSharing : public DelegateBase {
public:
	/// Info about own shared data
	struct FileShareInfo {
		FileShareInfo () : size(0), forAll(false), type (DirectoryListing::File) {}
		typedef DirectoryListing::EntryType EntryType;
		String    shareName;
		String    fileName;
		int64_t   size;
		UserSet   whom;
		bool      forAll;
		Path      path;
		EntryType type;
		SF_AUTOREFLECT_SERIAL;
	};

	/// Initializes FileSharing with a DataSharing instance
	FileSharing (DataSharingServer * server, SharedListServer * listServer);
	~FileSharing ();

	/// Initialzie the file sharing
	/// May be done multiple times
	Error init ();

	/// Share a file
	Error share (const String & shareName, const String & fileName, bool forAll = true, const UserSet & whom = UserSet());

	/// Edits a sharing (delete and add)
	/// Does not return an error if already existing yet.
	Error editShare (const String & shareName, const String & fileName, bool forAll, const UserSet & whom = UserSet());

	/// Stops sharing a file
	Error unshare (const String & shareName);

	/// Gives information about one share element.
	Error putInfo (const String & shareName, FileShareInfo * dest) const;

	/// Cancel an ongoing transfer
	Error cancelTransfer (AsyncOpId id);

	/// Remove all finished transfers (in CANCELED, ERROR or FINISHED state)s
	Error removeFinishedTransfers ();


	/// Copies information about shared files
	typedef std::vector<FileShareInfo> FileShareInfoVec;
	FileShareInfoVec shared () const;

	/// Checks if a given path is accessible for a user
	/// Return: NoError, if permission granted, NoPerm on no permission and NotFound if not found
	Error checkPermissions (const UserId & user, const String & shareName) const;

	typedef std::map<AsyncOpId, TransferInfo> TransferInfoMap;

	/// Current transfers
	TransferInfoMap transfers () const;

	typedef function<void (AsyncOpId id, TransferInfo::TransferUpdateType, const TransferInfo & info)> UpdatedTransferDelegate;
	UpdatedTransferDelegate & updatedTransfer ()  { return mUpdatedTransfer; }

private:
	/// Creates an unique path for that shareName
	Path createPath_locked (const String & shreName);

	struct FileShareInfoImpl : public FileShareInfo {
		DataSharingServer::SharingPromisePtr promise;
	};
	typedef std::map<String, FileShareInfoImpl> InfoMapImpl;
	InfoMapImpl mInfos;						///< Info about all shared files

	/// Callback for outgoing transmissions
	void onFileTransmissionUpdate (AsyncOpId id, const ds::TransmissionInfo & info, const weak_ptr<FileSharingPromise> & promise);

	TransferInfoMap mTransfers;				///< Outgoing transfers


	DataSharingServer * mSharingServer;		///< DataSharing instance
	SharedListServer  * mSharedListServer;	///< SharedList Server instance
	bool mInitialized;						///< Initialized status
	GlobberPtr mGlobber;					///< Globber (for DirectorySharing)

	UpdatedTransferDelegate mUpdatedTransfer;

};

}
