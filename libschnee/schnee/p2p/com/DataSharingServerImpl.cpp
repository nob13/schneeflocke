#include "DataSharingServerImpl.h"
#include <schnee/tools/Log.h>

namespace sf {

DataSharingServer * DataSharingServer::create () {
	return new DataSharingServerImpl;
}

DataSharingServerImpl::DataSharingServerImpl (){
	SF_REGISTER_ME;
	mWaitForNextTransmissionHandler = false;
	mTransmissionTimeOutMs = 60000;
}

DataSharingServerImpl::~DataSharingServerImpl (){
	SF_UNREGISTER_ME;
}

Error DataSharingServerImpl::shutdown () {
	for (SharedDataMap::const_iterator i = mShared.begin(); i != mShared.end(); i++){
		const SharedData & data (i->second);
		Notify n;
		n.path = i->first;
		n.mark = Notify::SubscriptionCancel;
		mCommunicationDelegate->send (data.subscribers, Datagram::fromCmd(n));
	}
	mShared.clear();
	return NoError;
}

Error DataSharingServerImpl::share (const Path & path, const SharingPromisePtr & promise){
	if (path.hasSubPath()){
		sf::Log (LogError) << LOGID << "Path may not have a sub path (implementation limitation)" << std::endl;
		return error::NotSupported;
	}
	SharedDataMap::iterator i = mShared.find(path);
	if (i != mShared.end()){
		sf::Log (LogError) << LOGID << "Resource " << path << " is already shared" << std::endl;
		return error::ExistsAlready;
	}

	SharedData info;
	if (promise) {
		info.currentRevision = 1;
		info.promise = promise;
	}
	mShared.insert (std::pair <Path, SharedData> (path, info));
	return NoError;
}

Error DataSharingServerImpl::update (const Path & path, const SharingPromisePtr & promise){
	if (!promise) {
		sf::Log (LogError) << LOGID << "Invalid 0 promise" << std::endl;
		return error::InvalidArgument;
	}
	SharedDataMap::iterator i = mShared.find (path);
	if (i == mShared.end()){
		sf::Log (LogError) << LOGID << "Resource " << path << " not shared" << std::endl;
		return error::NotFound;
	}
	SharedData & info = i->second;
	if (info.currentRevision <= 0) info.currentRevision = 1;
	else info.currentRevision++;
	info.promise = promise;
	notifySubscriber (path);
	return NoError;
}

Error DataSharingServerImpl::unShare (const Path & path){
	SharedDataMap::iterator i = mShared.find (path);
	if (i == mShared.end()){
		Log (LogWarning) << LOGID << path << " not found" << std::endl;
		return error::NotFound;
	}

	SharedData & data (i->second);
	// we have to send out canceling subscription to our subscibers
	Notify n;
	n.path = i->first;
	n.mark = Notify::SubscriptionCancel;
	mCommunicationDelegate->send (data.subscribers, Datagram::fromCmd(n));
	mShared.erase (i);
	return NoError;
}

Error DataSharingServerImpl::cancelTransfer (AsyncOpId id) {
	AsyncOp * op;
	op = getReadyAsyncOp (id);
	if (!op) return error::NotFound;
	if (op->type() != TRANSMISSION) {
		addAsyncOp (op);
		return error::InvalidArgument; // id was not a transmission!
	}
	Log (LogInfo) << LOGID << "Canceled transfer" << id <<  " (will do in next iteration)" << std::endl;
	xcall (abind (dMemFun (this, &DataSharingServerImpl::continueTransmission), error::Canceled, id));
	addAsyncOp (op);
	return NoError;
}


DataSharingServer::SharedDataDescMap DataSharingServerImpl::shared () const {
	DataSharingServer::SharedDataDescMap result;
	for (SharedDataMap::const_iterator i = mShared.begin(); i != mShared.end(); i++){
		const SharedData & data (i->second); // src
		SharedDataDesc desc;				 // dst
		desc.subscribers     = data.subscribers;
		desc.currentRevision = data.currentRevision;
		result[i->first] = desc;
	}
	return result;
}

DataSharingServer::SharedDataDesc  DataSharingServerImpl::shared (const Path & path, bool * found) const {
	SharedDataMap::const_iterator i = mShared.find (path);
	if (i == mShared.end()) { if (found) *found = false; return SharedDataDesc (); }
	const SharedData & data (i->second);
	SharedDataDesc result;
	result.subscribers     = data.subscribers;
	result.currentRevision = data.currentRevision;
	if (found) *found = true;
	return result;
}

void DataSharingServerImpl::onChannelChange (const HostId & host) {
	int level = mCommunicationDelegate->channelLevel(host);
	if (level > 0) return; // only interested in peers getting offline

	typedef std::pair<Uri, HostId> Subscription;
	typedef std::set<Subscription> SubscriptionSet;
	SubscriptionSet lostSubscriptions;
	assert (!host.empty());
	// stop subscriptions for offline users
	for (SharedDataMap::iterator i = mShared.begin(); i != mShared.end(); i++){
		SharedData & data (i->second);

		// stop subscribers
		HostSet::iterator j = data.subscribers.find(host);
		if (j != data.subscribers.end()){ // host was subscribed
			Notify n;
			n.path = i->first;
			n.mark = Notify::SubscriptionCancel;
			// inform canceled user (he is offline, won't necessary reach the target..)
			mCommunicationDelegate->send(host, Datagram::fromCmd(n));
			data.subscribers.erase(j);
		}
	}
}

void DataSharingServerImpl::onRpc (const HostId & sender, const Request & request, const ByteArray & data) {
	if (request.mark == Request::Transmission || request.mark == Request::TransmissionCancel){
		onRequestTransmission (sender, request, data);
		return;
	}

	RequestReply answer;
	sf::ByteArrayPtr answerContent = sf::createByteArrayPtr();
	do {
		answer.path = request.path;
		answer.id  = request.id;

		String shareName = request.path.head();
		Path   subPath   = request.path.subPath();

		SharedDataMap::iterator i = mShared.find (Path (shareName));

		// Checking Permission
		bool permission = (mCheckPermissions && mCheckPermissions (sender, request.path));
		if (!permission){
			answer.err = sf::error::NoPerm;
			break;
		}

		// Finding shared data
		if (i == mShared.end()) {
			Log (LogInfo) << LOGID << "Did not found " << shareName << std::endl;
			Log (LogInfo) << LOGID << "Valid: " << toJSONEx (mShared, sf::INDENT) << std::endl;

			// Not Found
			answer.err = sf::error::NotFound;
			break;
		}
		SharedData & info (i->second);

		// Checking Revision
		int usedRevision = request.revision > 0 ? request.revision : info.currentRevision;
		if (info.currentRevision != usedRevision) {
			Log (LogInfo) << LOGID << "Did not found revision " << usedRevision << std::endl;
			answer.err = sf::error::RevisionNotFound;
			break;
		}

		// Checking data sub path
		DataPromisePtr data = info.promise->data (subPath, request.user);
		String user = request.user.empty() ? data->dataDescription().user : request.user;

		if (!data){
			answer.err = sf::error::NotFound;
			break;
		}

		// Checking "user" (sub type) validness
		String providedUser = data->dataDescription().user;
		if (!providedUser.empty() && !request.user.empty() && (providedUser != request.user)){
			Log (LogWarning) << LOGID << "Deleliviering data with wrong user value (" << providedUser << "), was asked for " << request.user << std::endl;
		}
		user = request.user.empty() ? providedUser : request.user;


		// Checking data readiness
		if (data->ready() == false){
			Log (LogWarning) << LOGID << "Not-Ready-Promises are not supported on regular request operations" << std::endl;
			answer.err = sf::error::NotSupported;
			break;
		}

		// Checking range
		if (!request.range.isDefault()){
			long size = (long) data->size();
			if (!request.range.inRange (Range (0, size))){
				answer.err = error::InvalidArgument;
				break;
			}
		}

		// Ok, let's send
		answer.err = sf::error::NoError;
		answer.desc = data->dataDescription();
		answer.desc.user = user; // override
		answer.revision = usedRevision;
		answer.range = request.range;
		if (request.range.isDefault()){
			data->read (*answerContent);
		} else {
			data->read (request.range, *answerContent);
		}
	} while (false);
	// Sending non-locked
	mCommunicationDelegate->send (sender, sf::Datagram::fromCmd(answer, answerContent));
}

void DataSharingServerImpl::onRequestTransmission (const HostId & sender, const Request & request, const ByteArray & data) {
	assert (request.mark == Request::Transmission || request.mark == Request::TransmissionCancel);
	RequestReply reply;
	reply.id   = request.id;
	reply.path = request.path;
	do {
		String shareName = request.path.head();
		
		if (request.mark == Request::TransmissionCancel){
			TransmissionFinder finder (request.id);
			forEachAsyncOp (finder);
			if (finder.result.empty()){
				Log (LogInfo) << LOGID << "Got transmissioncancel for nonexisting transmission" << std::endl;
			} else for (TransmissionFinder::ResultVec::iterator i = finder.result.begin(); i != finder.result.end(); i++) {
				Transmission * t = 0;
				getReadyAsyncOp (*i, TRANSMISSION, &t);
				if (t) {
					delete t; // dropping transmission
				}
			}
			return;
		}

		// Check permission
		bool permission = mCheckPermissions && mCheckPermissions (sender, request.path);
		if (!permission) {
			reply.err = error::NoPerm;
			break;
		}
		
		// Is there any data?
		SharedDataMap::const_iterator i = mShared.find (shareName);
		if (i == mShared.end()) { 
			reply.err = error::NotFound; 
			break; 
		}
		const SharedData & info (i->second);

		int usedRevision = request.revision > 0 ? request.revision : info.currentRevision;
		
		// Do we have the revision?
		if (info.currentRevision != usedRevision) { 
			reply.err = error::RevisionNotFound;
			break;
		}
		
		// Do we have the (potential) sub path?
		DataPromisePtr data = info.promise->data (request.path.subPath(), request.user);
		
		String user   = request.user.empty() ? data->dataDescription().user : request.user;
		if (!data) {
			reply.err = error::NotFound;
			break;
		}

		
		// Check range
		Range usedRange;
		int64_t promiseSize = data->size();
		// Check the range
		if (!request.range.isDefault()){
			if (promiseSize != -1 && !request.range.inRange (Range (0, promiseSize))){
				reply.err = error::InvalidArgument; 
				break; 
			}		
			usedRange = reply.range;
		} else {
			usedRange = Range (0, data->size());
		}
		
		
		// Let's go, start transmission
		Transmission * trans = new Transmission (regTimeOutMs (mTransmissionTimeOutMs));
		trans->info.path      = request.path;
		trans->range     = usedRange;
		trans->info.destination  = sender;
		trans->requestId = request.id;
		trans->chunkSize = 8192; // TODO!
		if (promiseSize == -1){
			trans->count = -1;
		} else { 
			trans->count     = usedRange.length() / trans->chunkSize;
			if (usedRange.length() % trans->chunkSize > 0) trans->count++;
			if (usedRange.length() == 0) trans->count = 1; // otherwise it would be 0 (and we need so send something)
		}
		
		
		trans->revision  = usedRevision;
		trans->promise   = data;
		trans->nextChunk = 0;
		AsyncOpId opid = addAsyncOp (trans);
		xcall (abind(dMemFun (this, &DataSharingServerImpl::continueTransmission), NoError, opid));
		// mTransmissions.add (trans);
		
		// Confirmation
		reply.mark = RequestReply::TransmissionStart;
		reply.desc = data->dataDescription();
		reply.desc.user = user; // override
		reply.revision = usedRevision;
		reply.range = usedRange;
		
		trans->info.mark = reply.mark;
		trans->promise->onTransmissionUpdate(opid, trans->info);
		sf::Log (LogInfo) << LOGID << "Adding transmission " << sf::toJSON (trans) << std::endl;
	} while (false);
	mCommunicationDelegate->send (sender, Datagram::fromCmd(reply));
}

void DataSharingServerImpl::onRpc (const HostId & sender, const Subscribe & subscribe, const ByteArray & data) {
	SubscribeReply reply;
	do {
		reply.id  = subscribe.id;
		reply.path = subscribe.path;

		if (subscribe.mark == Subscribe::SubscriptionCancel){
			bool foundData (false), foundSubscriber (false);
			SharedDataMap::iterator i = mShared.find (subscribe.path);
			foundData = (i != mShared.end());
			if (foundData) {
				SharedData & data (i->second);
				if (data.subscribers.count (sender) > 0){
					foundSubscriber = true;
					data.subscribers.erase (sender);
				}
			}
			// There is generally no response on SubscriptionCancel
			Log (LogInfo) << LOGID << "Subscription Cancel: foundData: " << foundData << " foundSubscriber: " << foundSubscriber << std::endl;
			return;
		}

		assert (subscribe.mark != Subscribe::SubscriptionCancel && "Implement subscription cancel"); // TODO!

		bool permission = mCheckPermissions && mCheckPermissions (sender, subscribe.path);

		if (!permission){
			reply.err = error::NoPerm;
			break;
		}

		SharedDataMap::iterator i = mShared.find (subscribe.path);
		if (i == mShared.end()){
			reply.err = error::NotFound;
			break;
		}

		SharedData & info (i->second);
		// we can provide it - either from us or we have a source
		info.subscribers.insert (sender);
		reply.err  = error::NoError;
		if (info.promise) reply.desc = info.promise->dataDescription();

	} while (false);
	mCommunicationDelegate->send (sender, sf::Datagram::fromCmd(reply));
}

void DataSharingServerImpl::onRpc (const HostId & sender, const Push & push, const ByteArray & data) {
	ds::PushReply reply;
	reply.err = error::NotSupported;
	reply.id = push.id;
	if (reply.path.isDefault()) reply.path = push.path;
	mCommunicationDelegate->send(sender, Datagram::fromCmd(reply));
}

Error DataSharingServerImpl::notifySubscriber (const Path & path) {
	SharedDataMap::iterator i = mShared.find (path);
	if (i == mShared.end()){
		sf::Log (LogError) << LOGID << path << " not shared" << std::endl;
		return error::NotFound;
	}

	const HostSet & subscribers = i->second.subscribers;
	Notify n;
	n.path = path;
	n.revision = i->second.currentRevision;
	n.size     = i->second.promise->size();

	mCommunicationDelegate->send (subscribers, Datagram::fromCmd(n));
	return NoError;
}

void DataSharingServerImpl::continueTransmission (Error lastError, AsyncOpId id) {
	Transmission * t;
	getReadyAsyncOp(id, TRANSMISSION, &t);
	if (!t) return; // probably timeouted;
	if (lastError) {
		// stop transmission (receiver won't probably receive it)
		RequestReply reply;
		reply.id  = t->requestId;
		reply.path = t->info.path;
		reply.mark = RequestReply::TransmissionCancel;
		reply.err  = error::ConnectionError;

		t->info.mark  = RequestReply::TransmissionCancel;
		t->info.error = lastError;
		t->promise->onTransmissionUpdate(t->id(), t->info);
		mCommunicationDelegate->send (t->info.destination, Datagram::fromCmd(reply));
		delete t;
		return;
	}

	if (!t->promise->ready()) {
		// not ready yet
		sf::xcallTimed(abind(dMemFun(this, &DataSharingServerImpl::continueTransmission),NoError, id), futureInMs (100)); // try again in 100ms
		t->setTimeOut(sf::regTimeOutMs(mTransmissionTimeOutMs));
		addAsyncOp (t);
		return;
	}

	// Getting data for next chunk
	RequestReply r;
	ByteArrayPtr data;
	bool finished = false;
	if (t->nextChunk == t->count - 1){
		// will be last trasnmission
		r.mark = RequestReply::TransmissionFinish;
		finished = true;
	} else {
		r.mark = RequestReply::Transmission;
	}
	r.id = t->requestId;

	int64_t start = t->range.from + t->info.transferred;
	Range desiredRange;
	if (t->range.to == -1){
		desiredRange = Range (
				start,
				start + t->chunkSize);

	} else {
		desiredRange = Range (start, std::min (t->range.to, start + t->chunkSize));
	}

	data = sf::createByteArrayPtr();
	Error readError = t->promise->read (desiredRange, *data);
	r.range = Range (desiredRange.from, desiredRange.from + (int64_t) data->size());
	if (readError == error::Eof){
		r.mark = RequestReply::TransmissionFinish;
		finished = true;
	} else if (readError) {
		Log (LogWarning) << LOGID << "Had read error, will cancel transmission" << std::endl;
		r.err = error::ReadError;
	}
	t->nextChunk++;
	// Sending it
	sf::Error err;
	Datagram d = Datagram::fromCmd(r, data);
	err = mCommunicationDelegate->send (t->info.destination, d, abind (dMemFun (this, &DataSharingServerImpl::continueTransmission), id));
	t->speedMeasure.add(data->size());

	// Updating callback
	t->info.transferred += data->size();
	t->info.mark         = r.mark;
	t->info.speed        = t->speedMeasure.avg();

	if (r.err) t->info.error = r.err;
	t->promise->onTransmissionUpdate(t->id(), t->info);

	mTransmissionSentBytes += d.encodedSize();

	if (err || r.err || finished) {
		// removing
		delete t;
	} else {
		// reordering
		t->setTimeOut (sf::regTimeOutMs (mTransmissionTimeOutMs));
		addAsyncOp (t);
	}
}




}
