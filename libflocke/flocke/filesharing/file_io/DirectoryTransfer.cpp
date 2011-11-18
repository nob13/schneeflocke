#include "DirectoryTransfer.h"
#include <schnee/tools/Log.h>
#include <schnee/tools/FileTools.h>
namespace sf {

DirectoryTransfer::DirectoryTransfer (){
	SF_REGISTER_ME;
	mInfo.type  = TransferInfo::DIR_TRANSFER;
	mInfo.state = TransferInfo::NOSTATE;
	mClient       = 0;
	mSpeedMeasure = 0;
}

DirectoryTransfer::~DirectoryTransfer () {
	SF_UNREGISTER_ME;
	if (mSpeedMeasure) delete mSpeedMeasure;
	mSpeedMeasure = 0;
}

Error DirectoryTransfer::start (DataSharingClient * client, const Uri & uri, const String & destinationDirName, int timeOutMs) {
	LockGuard guard (mMutex);
	
	// creating destination directory
	Error e = sf::createDirectory (destinationDirName);
	if (e) return errorState_locked (e);
	
	ds::Request r;
	r.path = uri.path();
	r.mark = ds::Request::Transmission;
	r.user = "glob";
	e = client->request (uri.host(), r, dMemFun (this, &DirectoryTransfer::onRequestReply), timeOutMs);
	if (e) {
		return errorState_locked (e);
	}
	
	mClient             = client;
	mInfo.state         = TransferInfo::STARTING;
	mInfo.uri           = uri;
	mInfo.filename   = destinationDirName;
	mInfo.source = uri.host();
	mUri                = uri;
	return NoError;	
}

Error DirectoryTransfer::nextTransfer (FileTransferTask * taskOut) {
	// TODO: CLEANUP
	LockGuard guard (mMutex);
	if (mInfo.state == TransferInfo::FINISHED) return error::Eof;
	if (mInfo.state == TransferInfo::TRANSFERRED_LISTING){
		mInfo.state = TransferInfo::PENDING_FILES;
	}
	if (mInfo.state != TransferInfo::PENDING_FILES){
		return error::InvalidArgument;
	}
	assert (taskOut);
	if (!taskOut) return error::InvalidArgument;
	
	String path, destinationFileName;
	
	// Finding the next file (creating directories on its way)
	while (true) {
		if (mListingIterator == mListing.end()){
			mInfo.state = TransferInfo::FINISHED;
			if (mStateChanged)
				xcall (mStateChanged);
			return error::Eof;
		}

		path   = mListingIterator.path();
		destinationFileName = mInfo.filename + gDirectoryDelimiter + sf::toSystemPath (path);
		
		if (mListingIterator->type != DirectoryListing::Directory) break;

		Error e = sf::createDirectory (destinationFileName);
		if (e) return e;
		
		++mListingIterator;
	}
	
	taskOut->uri                    = Uri (mUri.host(), mUri.path() + Path (path));
	taskOut->destinationFileName 	= destinationFileName; 

	int64_t size  = mListingIterator->size;
	mSpeedMeasure->add (size);
	mInfo.transferred+=size;
	mInfo.speed = mSpeedMeasure->avg();
	
	++mListingIterator;
	if (mStateChanged) xcall (mStateChanged);
	return error::NoError;
}

void DirectoryTransfer::cancel (Error e) {
	LockGuard guard (mMutex);
	if (e) {
		mInfo.state = TransferInfo::ERROR;
		mInfo.error = e;
	} else {
		mInfo.state = TransferInfo::CANCELED;
	}
	notifyAsync (mStateChanged);
}

void DirectoryTransfer::onRequestReply (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data) {
	{
		LockGuard guad (mMutex);
		handleRequestReply_locked (sender, reply, data);
		if ((mInfo.state == TransferInfo::ERROR || mInfo.state == TransferInfo::CANCELED) && reply.mark != ds::RequestReply::TransmissionCancel) {
			mClient->cancelTransmission(sender, reply.id, reply.path);
		}
	}
	notify (mStateChanged);
}

void DirectoryTransfer::handleRequestReply_locked (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data) {
	if (reply.err){
		mInfo.error = reply.err;
		mInfo.state = TransferInfo::ERROR;
		return;
	}
	if (reply.mark == ds::RequestReply::TransmissionCancel) {
		mInfo.state = TransferInfo::CANCELED;
	}
	
	switch (mInfo.state) {
		case TransferInfo::STARTING:{
			handleTransmissionStarting_locked (sender, reply, data);
			break;
		}
		case TransferInfo::TRANSFERRING:{
			handleTransmissionTransferring_locked (sender, reply, data);
			break;
		}
		case TransferInfo::CANCELED:{
			return; // No Change.
			break;
		}	
		default:
			Log (LogError) << LOGID << "Strange state?! " << toString (mInfo.state) << std::endl;
		break;
	}
}

void DirectoryTransfer::handleTransmissionStarting_locked (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data) {
	assert (mInfo.transferred == 0);
	if (reply.mark != ds::RequestReply::TransmissionStart || reply.range.from != 0){
		Log (LogWarning) << LOGID << "Strange protocol" << std::endl;
		mInfo.state = TransferInfo::CANCELED;
		return;
	}
	mInfo.desc        = reply.desc;
	mInfo.size        = reply.range.length();
	mSpeedMeasure = new SpeedMeasure ();
	mInfo.state = TransferInfo::TRANSFERRING;
	// Forwarding to Transferring State
	handleTransmissionTransferring_locked (sender, reply, data);
}

void DirectoryTransfer::handleTransmissionTransferring_locked (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data) {
	if (reply.mark != ds::RequestReply::TransmissionStart
		&& reply.mark != ds::RequestReply::TransmissionFinish
		&& reply.mark != ds::RequestReply::Transmission){
		Log (LogWarning) << LOGID << "Strange protocol" << std::endl;
		mInfo.state = TransferInfo::CANCELED;
		return;
	}
	size_t l = data->size();
	mListingData.append (*data);
	mSpeedMeasure->add (l);
	mInfo.speed = mSpeedMeasure->avg();
	mInfo.transferred += l;
	if (reply.mark == ds::RequestReply::TransmissionFinish){
		if (mInfo.transferred != mInfo.size && mInfo.size != -1) {
			Log (LogWarning) << LOGID << "Transferred Size mismatch" << std::endl;
		}
		// decoding data
		bool suc = fromJSON (mListingData, mListing);
		if (!suc){
			mInfo.state = TransferInfo::ERROR;
			mInfo.error = error::BadDeserialization;
			Log (LogWarning) << LOGID << "Bad deserialization" << std::endl;
			Log (LogInfo) << LOGID << "(RemoveMe)" << mListingData << std::endl;
			return;
		}
		mListingIterator = mListing.begin();
		mInfo.state = TransferInfo::TRANSFERRED_LISTING;
		mInfo.size  = mListing.sizeSum();
		mInfo.transferred = 0;
		delete mSpeedMeasure;
		mSpeedMeasure = new SpeedMeasure;
	}
}



}
