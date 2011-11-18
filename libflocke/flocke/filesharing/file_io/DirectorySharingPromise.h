#pragma once
#include <schnee/p2p/DataSharingServer.h>
#include "FileSharingPromise.h"
#include "Globber.h"

namespace sf {

/// DirectorySharingPromise is a sharing promise for Directories
/// The Data itself is empty; content is given out through subData methods
/// Example:
/// subData ("/")         --> will give you a list of files
/// subData ("/file.txt") --> will give you the file
class DirectorySharingPromise : public DataSharingServer::SharingPromise {
public:
	typedef DataSharingServer::SharingPromise SharingPromise;
	typedef DataSharingServer::SharingPromisePtr SharingPromisePtr;

	DirectorySharingPromise (const String & dirName, const GlobberPtr& globber);
	~DirectorySharingPromise ();

	virtual DataPromisePtr data (const Path & subPath, const String & user) const;
	virtual Error error () const { return mError; }

	typedef function <void (AsyncOpId id, const ds::TransmissionInfo &, weak_ptr<FileSharingPromise> file)> TransmissionUpdateDelegate;
	TransmissionUpdateDelegate& transmissionUpdated () { return mTransmissionUpdated; }

private:
	/// Translates the given subpath into OS-specific path (including mDirName!) 
	String osPath (const Path & subPath) const;
	
	/// Generates a directory listing for a specific path
	DataPromisePtr directoryListing (const Path & path) const;
	/// Creates a File sharing promise ptr
	DataPromisePtr subFile (const Path & path) const;

	void init (const String & dirName);
	String 		mDirName;
	Error  		mError;
	GlobberPtr  mGlobber;

	TransmissionUpdateDelegate mTransmissionUpdated;

};
typedef shared_ptr<DirectorySharingPromise> DirectorySharingPromisePtr;


}
