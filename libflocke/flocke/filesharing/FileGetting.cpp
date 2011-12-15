#include "FileGetting.h"
#include <flocke/objsync/DataTracker.h>
#include <schnee/tools/Log.h>
#include <schnee/tools/FileTools.h>

namespace sf {

FileGetting::FileGetting (sf::DataSharingClient * client) {
	SF_REGISTER_ME;
	mClient = client;
	mTimeOutMs = 30000;
	mDestinationDirectory = "";
	mNextId = 1;
	mFilesPerDirectory = 2;
}

FileGetting::~FileGetting () {
	SF_UNREGISTER_ME;
}

sf::Error FileGetting::init () {
	return sf::NoError;
}

Error FileGetting::listDirectory (const Uri & uri, AsyncOpId * opIdOut) {
	if (!uri.valid()) return error::InvalidUri;
	ds::Request r;
	r.path = uri.path();
	r.user = "list";
	return mClient->request (uri.host(), r, dMemFun (this, &FileGetting::onListingReply), mTimeOutMs, opIdOut);
}


/// Tries creating a non existing name (by appending 0 .. 256)
/// returns true if it founds a name
static bool nonExistingName (const String & base, String * result){
	String c = base;
	int append = 0;
	String stem      = sf::fileStem (base);
	String ext = sf::fileExtension(base);
	for (; sf::fileExists (c) && append < 256; append++){
		std::ostringstream ss; ss << stem << append << ext;
		c = ss.str();
	}
	assert (result);
	*result = c;
	return append < 256;
}

/// Removes trailing slashes (e.g. on directory names /bla/bla/// --> /bla/bla )
static void removeTrailingSlashes (String & x) {
	while (!x.empty() && x[x.size() - 1] == '/')
		x.erase (x.end() - 1);
}

static bool generateName (const String & dstDir, const Uri & uri, String * result) {
	String dstFileName;
	String nameCandidate;
	String path = uri.path().toString();
	removeTrailingSlashes (path);
	if (dstDir.empty()){
		nameCandidate =  basename (path.c_str());
	} else {
		nameCandidate = dstDir + gDirectoryDelimiter + basename (path.c_str());
	}
	if (!nonExistingName (nameCandidate, result)){
		return false;
	}
	return true;
}

Error FileGetting::request (const Uri & uri, AsyncOpId * opIdOut) {
	String dstFileName;
	if (!generateName (mDestinationDirectory, uri, &dstFileName)){
		return error::ExistsAlready;
	}
	Error err = requestFileTransfer_locked (uri, dstFileName, 0, opIdOut);
	if (err) return err;
	return NoError;
}

sf::Error FileGetting::requestDirectory (const Uri & uri, AsyncOpId * opIdOut) {
	String dstFileName;
	if (!generateName (mDestinationDirectory, uri, &dstFileName)){
		return error::ExistsAlready;
	}

	AsyncOpId opId = nextId_locked ();

	DirectoryTransferPtr dirTransfer = DirectoryTransferPtr (new DirectoryTransfer());
	dirTransfer->stateChanged() = abind (dMemFun (this, &FileGetting::onTransferChange), opId, TransferInfo::Changed);

	assert (mTransfers.count (opId) == 0);
	mTransfers[opId] = dirTransfer;
	if (opIdOut) *opIdOut = opId;
	xcall (abind(dMemFun(this, &FileGetting::onTransferChange), opId, TransferInfo::Added));


	Error err = dirTransfer->start (mClient, uri, dstFileName, mTimeOutMs);
	if (err) {
		Log (LogWarning) << LOGID << "Could not start directory transfer " << err << std::endl;
	}
	return err;
}

Error FileGetting::cancelTransfer (AsyncOpId id) {
	TransferMap::iterator i = mTransfers.find(id);
	if (i == mTransfers.end()) {
		Log (LogWarning) << LOGID << "Warning, transfer " << id << " not found" << std::endl;
		return error::NotFound;
	}
	i->second->cancel();
	return NoError;
}

Error FileGetting::removeTransfer (AsyncOpId id) {
	TransferMap::iterator i = mTransfers.find(id);
	if (i == mTransfers.end()) {
		return error::NotFound;
	}
	i->second->cancel();
	notifyAsync (mUpdatedTransfer, id, TransferInfo::Removed, TransferInfo());
	mTransfers.erase(id);
	return NoError;
}

Error FileGetting::removeFinishedTransfers () {
	TransferMap::iterator i = mTransfers.begin();
	while (i != mTransfers.end()){
		TransferInfo::State s = i->second->info().state;
		if (s == TransferInfo::FINISHED || s == TransferInfo::CANCELED || s == TransferInfo::ERROR){
			notifyAsync (mUpdatedTransfer, i->first, TransferInfo::Removed, TransferInfo());
			mTransfers.erase(i++);
		} else {
			i++;
		}
	}
	return NoError;
}

FileGetting::TransferInfoMap FileGetting::transfers () const {
	TransferInfoMap dest;
	for (TransferMap::const_iterator i = mTransfers.begin(); i != mTransfers.end(); i++){
		dest.insert (std::make_pair (i->first, i->second->info()));
	}
	return dest;
}

String FileGetting::destinationDirectory() const {
	return mDestinationDirectory;
}

void FileGetting::setDestinationDirectory (const String & dir) {
	mDestinationDirectory = dir;
}

void FileGetting::onTransferChange (AsyncOpId id, TransferInfo::TransferUpdateType type) {
	// TODO: Cleanup
	TransferInfo info;
	if (mTransfers.find (id) == mTransfers.end()){
		Log (LogInfo) << LOGID << "Did not found Transfer (maybe deleted)! " << id << std::endl;
		return;
	}
		
	TransferPtr transfer = mTransfers[id];
	info = transfer->info();

	// DirectoryTransfer may just got its data
	if (info.state == TransferInfo::TRANSFERRED_LISTING){
		assert (info.type == TransferInfo::DIR_TRANSFER);
		for (int i = 0; i < mFilesPerDirectory; i++){
			Error e = startNextChildTransfer_locked (id);
			if (e) break;
		}
	}
		
	// A subtransfer ended, lets begin a new one
	if (info.state == TransferInfo::FINISHED && info.parent != 0){
		assert (mTransfers.count (info.parent) > 0);
		Error e = startNextChildTransfer_locked (info.parent);
		(void)e;
	}
	if (mUpdatedTransfer) mUpdatedTransfer (id, type, info);
}

Error FileGetting::startNextChildTransfer_locked (AsyncOpId id) {
	assert (mTransfers.count (id) > 0);
	TransferPtr transfer = mTransfers[id];
	assert (transfer->info().type == TransferInfo::DIR_TRANSFER);
	DirectoryTransferPtr dirTransfer = boost::static_pointer_cast <DirectoryTransfer> (transfer);
	DirectoryTransfer::FileTransferTask task;
	Error e = dirTransfer->nextTransfer (&task);
	if (e){
		if (e == error::Eof) return NoError;
		Log (LogWarning) << LOGID << "Strange directory transfer error " << toString (e) << std::endl;
		dirTransfer->cancel (e);
		return e;
	}
	e = requestFileTransfer_locked (task.uri, task.destinationFileName, id);
	if (e){
		Log (LogWarning) << LOGID << "Cancelling dir transfer because sub transfer failed" << std::endl;
		dirTransfer->cancel (e);
	}
	return e;	
}


Error FileGetting::requestFileTransfer_locked (const Uri & uri, const String & fileName, AsyncOpId parent, AsyncOpId * opIdOut) {
	AsyncOpId id = nextId_locked ();
	FileTransferPtr fileTransfer = FileTransferPtr (new FileTransfer(parent));

	fileTransfer->stateChanged () = abind (dMemFun (this, &FileGetting::onTransferChange), id, TransferInfo::Changed);
	// also holding failed transfers (so that the user can check tem)
	mTransfers[id] = fileTransfer;
	xcall (abind (dMemFun (this, &FileGetting::onTransferChange), id, TransferInfo::Added));

	Error e = fileTransfer->start (mClient, uri, fileName, mTimeOutMs);
	if (e) {
		Log (LogWarning) << LOGID << "Error " << toString (e) << " happend while starting file transfer for " << uri << std::endl;
	}
	if (opIdOut) *opIdOut = id;
	return e;
}


void FileGetting::onListingReply (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data) {
	Error forwardResult = reply.err;
	DirectoryListing listing;

	if (!forwardResult){
		bool suc = sf::fromJSON(*data, listing);
		if (!suc) forwardResult = error::BadDeserialization;
	}
	if (mGotListing) mGotListing (Uri (sender, reply.path), forwardResult, listing);
}

}
