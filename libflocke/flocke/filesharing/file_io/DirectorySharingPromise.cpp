#include "DirectorySharingPromise.h"
#include "FileSharingPromise.h"
#include "GlobPromise.h"
#include "DirectoryListing.h"
#include <schnee/tools/Log.h>
#include <schnee/tools/FileTools.h>


namespace sf {

ByteArray genericDirectory ("directory");

DirectorySharingPromise::DirectorySharingPromise (const String & dirName, const GlobberPtr & globber) {
	init (dirName);
	mGlobber = globber;
}

DirectorySharingPromise::~DirectorySharingPromise () {
}

DataPromisePtr DirectorySharingPromise::data (const Path & subPath, const String & user) const {
	if (sf::checkDangerousPath(subPath.toString())) {
		Log (LogWarning) << LOGID << subPath << " looks dangerous, rejecting" << std::endl;
	}
	
	if (user.empty()){
		// Check if it is a directory
		DataPromisePtr tryDirectory = directoryListing (subPath);
		if (tryDirectory) return tryDirectory;

	
		// It must be a file
		return subFile (subPath);
	}
	if (user == "list"){
		return directoryListing (subPath);
	}
	if (user == "file"){
		return subFile (subPath);
	}
	if (user == "glob") {
		return DataPromisePtr (new GlobPromise (mGlobber, osPath (subPath)));
	}
	// unknown user flag
	Log (LogWarning) << LOGID << "Unknown user flag: " << user << std::endl;
	return DataPromisePtr ();
}

String DirectorySharingPromise::osPath (const Path & subPath) const {
	return mDirName + sf::gDirectoryDelimiter + subPath.toSystemPath();
}

DataPromisePtr DirectorySharingPromise::directoryListing (const Path & path) const {
	String osp = osPath (path);
	
	DirectoryListing listing;
	Error error = sf::listDirectory(osp, &listing);
	if (error) {
		Log (LogInfo) << LOGID << "List directory of " << osp << " returned " << toString (error) << std::endl;
		return DataPromisePtr();
	}
	ByteArrayPtr result  = createByteArrayPtr();
	*result = toJSON (listing);
	return sf::createDataPromise(result);
}

DataPromisePtr DirectorySharingPromise::subFile (const Path & path) const {
	String osp = osPath (path);
	if (mError) {
		return DataPromisePtr();
	}
	FileSharingPromisePtr p (new FileSharingPromise (osp));
	if (mTransmissionUpdated){
		p->transmissionUpdated() = abind (mTransmissionUpdated, weak_ptr<FileSharingPromise> (p));
	}
	return DataPromisePtr (p);
}

void DirectorySharingPromise::init (const String & dirName) {
	mError = NoError;
	if (!sf::fileExists(dirName)) { mError = error::NotFound; return; }
	if (!sf::isDirectory(dirName)) { mError = error::InvalidArgument; }
	mDirName = dirName;
}



}
