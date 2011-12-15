#include "FileSharing.h"
#include <schnee/tools/Log.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <schnee/tools/FileTools.h>

namespace sf {

FileSharing::FileSharing (DataSharingServer * server, SharedListServer * sharedListServer) {
	SF_REGISTER_ME;
	mSharingServer = server;
	mSharedListServer = sharedListServer;
	mInitialized = false;
	mGlobber = GlobberPtr (new Globber());
}

FileSharing::~FileSharing () {
	SF_UNREGISTER_ME;
}

Error FileSharing::init () {
	mInitialized = true;
	return NoError;
}

Error FileSharing::share (const String & shareName, const String & fileName , bool forAll, const UserSet & whom){
	if (!mInitialized) return error::NotInitialized;
	if (mInfos.count (shareName) > 0) return error::ExistsAlready;

	// 1. selecting uri
	Path path = createPath_locked (shareName);

	// 2. Loading Promise
	bool dir = sf::isDirectory(fileName);
	DataSharingServer::SharingPromisePtr promise;
	if (dir) {
		DirectorySharingPromisePtr dp (new DirectorySharingPromise (fileName, mGlobber));
		dp->transmissionUpdated() = dMemFun (this, &FileSharing::onFileTransmissionUpdate);
		promise = DirectorySharingPromisePtr (dp);
	} else {
		FileSharingPromisePtr dp (new FileSharingPromise (fileName));
		dp->transmissionUpdated() = abind (dMemFun (this, &FileSharing::onFileTransmissionUpdate), weak_ptr<FileSharingPromise> (dp));
		promise = DataSharingServer::createFixedPromise (dp);
	}
	if (promise->error())  return promise->error();

	// 3. Trying to share it
	Error error = mSharingServer->share (path, promise);
	if (error) return error;

	// 5. Updating shared list
	ds::DataDescription desc;
	desc.user = dir ? "dir" : "file";
	error = mSharedListServer->add (shareName, SharedElement (path, promise->size(), desc));
	if (error) {
		mSharingServer->unShare (path);
		return error;
	}
	
	// 4. Putting into list
	FileShareInfoImpl info;
	info.shareName = shareName;
	info.fileName  = fileName;
	info.forAll    = forAll;
	info.size      = promise->size();
	info.whom      = whom;
	info.path      = path;
	info.type      = dir ? DirectoryListing::Directory : DirectoryListing::File;
	info.promise   = promise;
	mInfos[shareName] = info;
	return NoError;
}

Error FileSharing::editShare (const String & shareName, const String & fileName, bool forAll, const UserSet & whom) {
	Error e = unshare (shareName);
	if (e != NoError && e != error::NotFound) return e;
	e = share (shareName, fileName, forAll, whom);
	return e;
}

Error FileSharing::unshare (const String & shareName) {
	if (mInfos.count (shareName) == 0) return error::NotFound;

	{
		FileShareInfo & info = mInfos[shareName];
		Error err = mSharingServer->unShare (info.path);
		if (err){
			Log (LogError) << LOGID << "Could not unshare " << err << " security breach!" << std::endl;
			Log (LogError) << LOGID << "Stoppign rights on it" << std::endl;
			info.forAll = false;
			info.whom.clear();
			return err;
		}
	}

	mInfos.erase (shareName);
	mSharedListServer->remove(shareName);
	return NoError;
}

Error FileSharing::putInfo (const String & shareName, FileShareInfo * dest) const {
	InfoMapImpl::const_iterator i = mInfos.find (shareName);
	if (i == mInfos.end()) return error::NotFound;
	if (dest) *dest =  i->second;
	return NoError;
}

Error FileSharing::cancelTransfer (AsyncOpId id) {
	TransferInfoMap::iterator i = mTransfers.find(id);
	if (i == mTransfers.end())
		return error::NotFound;
	mSharingServer->cancelTransfer(id);
	// DataSharingServer will tell us that its canceled.
	return NoError;
}

Error FileSharing::removeFinishedTransfers () {
	TransferInfoMap::iterator i = mTransfers.begin();
	while (i != mTransfers.end()){
		TransferInfo::State s = i->second.state;
		if (s == TransferInfo::CANCELED || s == TransferInfo::ERROR || s == TransferInfo::FINISHED){
			notifyAsync (mUpdatedTransfer, i->first, TransferInfo::Removed, TransferInfo());
			mTransfers.erase(i++);
		} else {
			i++;
		}
	}
	return NoError;
}


FileSharing::FileShareInfoVec FileSharing::shared() const {
	FileShareInfoVec result;
	for (InfoMapImpl::const_iterator i = mInfos.begin(); i != mInfos.end(); i++) {
		result.push_back (i->second);
	}
	return result;
}

Error FileSharing::checkPermissions(const UserId & user, const String & shareName) const {
	InfoMapImpl::const_iterator i = mInfos.find(shareName);
	if (i == mInfos.end()) return error::NotFound;
	const FileShareInfo & info (i->second);
	if (info.forAll) return NoError;
	if (info.whom.count(user) > 0) return NoError;
	return error::NoPerm;
}

FileSharing::TransferInfoMap FileSharing::transfers () const {
	return mTransfers;
}

static String replace (const String & src, char c, char r){
	String result = src;
	for (size_t i = result.find(c); i != result.npos; i = result.find(c, i+1)){
		result[i] = r;
	}
	return result;
}

Path FileSharing::createPath_locked (const String & shareName){
	// replacing notallowed characters '/' and ':'
	String fit = replace (replace (shareName, '/', '_'), ':', '_');
	// checking for non-existance, adding numbers
	bool found;
	Path candidate (fit);
	mSharingServer->shared(candidate, &found);
	if (!found) return candidate;
	// hack, adding a number
	if (fit.length() > 250) fit.resize(250);
	for (int i = 0; i < 10; i++) {
		char buffer[256];
		snprintf (buffer, 256, "%s_%d",fit.c_str(), i);
		candidate = buffer;
		mSharingServer->shared (candidate, &found);
		if (!found) return candidate;
	}
	Log (LogWarning) << LOGID << "Forced giving out a propably failing uri " << candidate << std::endl;
	// Will fail later when being used :(
	return candidate;
}

void FileSharing::onFileTransmissionUpdate (AsyncOpId id, const ds::TransmissionInfo & info, const weak_ptr<FileSharingPromise> & promise) {
	TransferInfo::TransferUpdateType change = TransferInfo::Changed;
	TransferInfo result;
	FileSharingPromisePtr realInstance = promise.lock();
	// Interpreting data and order it in
	if (mTransfers.count(id) == 0){
		change = TransferInfo::Added;
		// there is currently no 'removed', we always add to the list.
	}
	TransferInfo & t = mTransfers[id];
	t.speed       = info.speed;
	t.transferred = info.transferred;
	t.type        = TransferInfo::FILE_TRANSFER;
	switch (info.mark) {
	case ds::RequestReply::Transmission: {
		t.state = TransferInfo::TRANSFERRING;
		break;
	}
	case ds::RequestReply::TransmissionStart: {
		// only update expensive fileds through transmission start
		if (realInstance) {
			t.desc     = realInstance->dataDescription();
			t.size     = realInstance->size();
			t.filename = realInstance->fileName();
		}
		t.receiver = info.destination;
		t.state = TransferInfo::STARTING;
		t.uri   = Uri (HostId(), info.path);
		break;
	}
	case ds::RequestReply::TransmissionFinish: {
		t.state = TransferInfo::FINISHED;
		break;
	}
	case ds::RequestReply::TransmissionCancel: {
		t.state = TransferInfo::CANCELED;
		break;
	}
	default:
		assert (!"Unknown state");
		break;
	}
	result = t;
	notify (mUpdatedTransfer, id, change, result);
}

}
