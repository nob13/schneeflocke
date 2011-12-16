#include "FileTransfer.h"
#include <schnee/tools/Log.h>
#include <schnee/tools/FileTools.h>

#include <stdio.h>

namespace sf {

FileTransfer::FileTransfer (AsyncOpId parent) { 
	SF_REGISTER_ME;
	mDestination  = 0;
	mSpeedMeasure = 0; 
	mInfo.type   = TransferInfo::FILE_TRANSFER;
	mInfo.state  = TransferInfo::NOSTATE;
	mInfo.parent = parent;
	mClient = 0;
	mId = 0;
}

FileTransfer::~FileTransfer () {
	SF_UNREGISTER_ME;
	cleanup ();
}

Error FileTransfer::start (DataSharingClient * client, const Uri & uri, const String & destinationFileName, int timeOutMs) {
	ds::Request r;
	r.path = uri.path();
	r.mark = ds::Request::Transmission;
	r.user = "file";
	
	if (fileExists (destinationFileName)) return error::ExistsAlready;
	
	Error err = client->request (uri.host(), r, dMemFun (this, &FileTransfer::onRequestReply), timeOutMs);
	if (err) return errorState (err);
	
	mClient = client;
	mInfo.state = TransferInfo::STARTING;
	mInfo.uri   = uri;
	mInfo.filename = destinationFileName;
	mInfo.source = uri.host();
	Log (LogInfo) << LOGID << "Will start transfer " << toJSON (mInfo) << " dst filename: " << mInfo.filename << std::endl;

	return NoError;
}

void FileTransfer::cancel (Error cause) {
	if (mInfo.state == TransferInfo::FINISHED || mInfo.state == TransferInfo::ERROR) {
		// too late..
		return;
	}
	if (mInfo.state == TransferInfo::TRANSFERRING) {
		mClient->cancelTransmission(mInfo.uri.host(), mId);
	}
	if (cause) {
		errorState (cause);
	} else
		mInfo.state = TransferInfo::CANCELED;
	notifyAsync (mStateChanged);
}


void FileTransfer::onRequestReply (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data) {
	handleRequestReply (sender, reply, data);
	if ((mInfo.state == TransferInfo::ERROR || mInfo.state == TransferInfo::CANCELED) && reply.mark != ds::RequestReply::TransmissionCancel) {
		mClient->cancelTransmission(sender, reply.id, reply.path);
	}
	notify (mStateChanged);
}

void FileTransfer::handleRequestReply (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data) {
	if (reply.err){
		if (mInfo.state != TransferInfo::CANCELED){
			mInfo.error = reply.err;
			mInfo.state = TransferInfo::ERROR;
		}
		cleanup ();
		return;
	}
	if (reply.mark == ds::RequestReply::TransmissionCancel) {
		mInfo.state = TransferInfo::CANCELED;
	}
	if (mId == 0) mId = reply.id;
	
	switch (mInfo.state) {
		case TransferInfo::STARTING:{
			handleTransmissionStarting (sender, reply, data);
			break;
		}
		case TransferInfo::TRANSFERRING:{
			handleTransmissionTransferring (sender, reply, data);
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
	if (mInfo.state == TransferInfo::CANCELED || 
		mInfo.state == TransferInfo::ERROR || 
		mInfo.state == TransferInfo::FINISHED){
		cleanup();
	}
}

void FileTransfer::handleTransmissionStarting (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data) {
	assert (mInfo.transferred == 0);
	if (reply.mark != ds::RequestReply::TransmissionStart || reply.range.from != 0){
		Log (LogWarning) << LOGID << "Strange protocol" << std::endl;
		mInfo.state = TransferInfo::CANCELED;
		return;
	}
	mInfo.desc        = reply.desc;
	mInfo.size        = reply.range.length();
	mDestination = fopen (mInfo.filename.c_str(), "wb");
	mSpeedMeasure = new SpeedMeasure ();
	if (!mDestination) {
		Log (LogError) << LOGID << "Could not open " << mInfo.filename << " for writing" << std::endl;
		mInfo.state = TransferInfo::CANCELED;
		return;
	}
	mInfo.state = TransferInfo::TRANSFERRING;
	// Forwarding to Transferring State
	handleTransmissionTransferring (sender, reply, data);
}

void FileTransfer::handleTransmissionTransferring (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data) {
	if (reply.mark != ds::RequestReply::TransmissionStart
		&& reply.mark != ds::RequestReply::TransmissionFinish
		&& reply.mark != ds::RequestReply::Transmission){
		Log (LogWarning) << LOGID << "Strange protocol" << std::endl;
		mInfo.state = TransferInfo::CANCELED;
		return;
	}
	size_t l = data->size();
	size_t w = 0;
	if (l > 0){
		w = fwrite (data->const_c_array(), 1, l, mDestination);
		if (w != l) {
			Log (LogError) << LOGID << "Could not write " << mInfo.filename << std::endl;
			mInfo.state = TransferInfo::ERROR;
			return;
		}
		mSpeedMeasure->add (w);
		mInfo.speed = mSpeedMeasure->avg();
	}
	mInfo.transferred += w;
	if (reply.mark == ds::RequestReply::TransmissionFinish){
		if (mInfo.transferred != mInfo.size) {
			Log (LogWarning) << LOGID << "Transferred Size mismatch" << std::endl;
			mInfo.state = TransferInfo::ERROR;
			return;
		}
		mInfo.state = TransferInfo::FINISHED;
	}
}

void FileTransfer::cleanup () {
	if (mDestination){
		fclose (mDestination);
		mDestination = 0;
	}
	delete mSpeedMeasure;
	mSpeedMeasure = 0;
}

}
