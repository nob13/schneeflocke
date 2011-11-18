#include "ZeroConfServicePrivate_dns_sd.h"
#include <schnee/tools/async/DelegateBase.h>

#ifdef ENABLE_DNS_SD

namespace sf {

/// Protecting all DNSServiceCalls, doesn't seem very threadsafe
sf::Mutex DNSServiceMutex;

DNSServiceRunLoop::DNSServiceRunLoop (){
	mThread = 0;
	mStop = false;
}

DNSServiceRunLoop::~DNSServiceRunLoop (){
	stop ();
}

void DNSServiceRunLoop::add (DNSServiceRef service, bool inCallback){
	RecursiveLockGuard lg (mMutex);
	mServices.insert (service);
	if (!mThread) run();
}

void DNSServiceRunLoop::del (DNSServiceRef service, bool inCallback){
	RecursiveLockGuard lg (mMutex);
	mServices.erase (service);
	if (!inCallback) {
		DNSServiceMutex.lock();
	} else {
		assert (DNSServiceMutex.try_lock () == false);
	}
	DNSServiceRefDeallocate (service);
	if (!inCallback) {
		DNSServiceMutex.unlock();
	}
	if (mServices.empty()) stop ();
}

void DNSServiceRunLoop::iteration () {
	fd_set rfds;
	FD_ZERO (&rfds);
	int max = 0;

	// Collecting file descriptors...
	{
		RecursiveLockGuard lg (mMutex);
		// set of file descriptors we are waiting for
		FD_ZERO (&rfds);
		
		for (std::set<DNSServiceRef>::const_iterator i = mServices.begin(); i != mServices.end(); i++){
			DNSServiceMutex.lock ();
			int sock = DNSServiceRefSockFD (*i);
			DNSServiceMutex.unlock();
			if (sock > max) max = sock;
			FD_SET (sock, &rfds);
		}
	}
	// waiting (max 1sec)
	struct timeval tv;
	tv.tv_sec  = 1;
	tv.tv_usec = 0;

	int retval = select (max + 1, &rfds, NULL, NULL, &tv);
	if (retval < 0){
		int err = errno;
		if (err == EBADF) return; // may happen, was closed already or something
		Log (LogError) << LOGID << "There was an error in select " << strerror (err) << std::endl;
	}
	if (retval == 0){
		/// regular timeout
		return;
	}
	// let's see, which service made the select going to stop
	RecursiveLockGuard lg (mMutex);
	for (std::set<DNSServiceRef>::const_iterator i = mServices.begin(); i != mServices.end(); i++){
		DNSServiceMutex.lock ();
		int sock = DNSServiceRefSockFD (*i);
		DNSServiceMutex.unlock();
		if (FD_ISSET(sock, &rfds)){
			DNSServiceMutex.lock ();
			DNSServiceProcessResult (*i);
			DNSServiceMutex.unlock();
			// DNSServiceProcessResult may have changed mServices!
			// so mServices may be incoherent now.
			// i = mServices.begin() won't always work, we could Process the same DNSServiceRef again.
			return;
		}
	}
}

void DNSServiceRunLoop::run (){
	RecursiveLockGuard lg (mMutex);
	Log (LogInfo) << LOGID << "Starting dns_sd run loop" << std::endl;
	mThread = new boost::thread (boost::bind (&DNSServiceRunLoop::runLoop, this));
}

void DNSServiceRunLoop::stop (){
	Log (LogInfo) << LOGID << "Stopping dns_sd run loop " << std::endl;
	if (mThread){
		mStop = true;
		mThread->join();
	}
	RecursiveLockGuard lg (mMutex);
	delete mThread;
	mThread = 0;
}

void DNSServiceRunLoop::runLoop (){
	while (!mStop){
		iteration ();
	}
}

static std::map<String, String> decodeTxtField (int txtLength, const unsigned char * txtRecord){
	std::map<String,String> target;
	if (!txtRecord || txtLength == 0 ) return target;
	const unsigned char * length = txtRecord;
	while (*length != 0 && length < txtRecord + txtLength){
		assert (*length > 0 && *length < txtLength);
		char entry[256];
		memcpy (entry, length+1, *length);
		entry[*length] = 0;
		String it (entry);
		size_t equal = it.find('=');
		String first;
		String last;
		if (equal == it.npos){
			first = it;
		} else {
			first = it.substr (0, equal);
			last  = it.substr (equal + 1, it.length() - equal - 1);
		}
		if (!first.empty()) // should be silently ignored (according to standard)
			target[first] = last;
		length = length + *length + 1;			
	}
	return target;
}

static ByteArrayPtr encodeTxtField (const std::map<String, String> & field){
	if (field.empty()) return ByteArrayPtr();
	ByteArrayPtr result = ByteArrayPtr (new ByteArray());
	for (std::map<String,String>::const_iterator i = field.begin(); i != field.end(); i++){
		const String & first = i->first;
		const String & last  = i->second;
		if (first.find('=') != first.npos){
			Log (LogError) << LOGID << "Cannot encode '=' in first txt field, skipping" << std::endl;
			continue;
		}
		String enc = first + "=" + last;
		if (enc.length() > 255) {
			Log (LogError) << LOGID << "Field " << enc << " is to long, skipping" << std::endl;
			continue;
		}
		unsigned char c = (unsigned char) enc.length();
		result->append (c);
		ByteArray toAppend (enc.c_str(), c);
		result->append (toAppend);
	}
	return result;
}

ZeroConfServicePrivate::ZeroConfServicePrivate (){
	reset ();
}

ZeroConfServicePrivate::~ZeroConfServicePrivate () {
	mRunLoop.stop ();
}


void ZeroConfServicePrivate::signalError (const String & msg){
	Log (LogError) << LOGID << msg << std::endl;
	mError       = true;
	mErrorMessage = msg;
	if (mErrorDelegate) xcall (mErrorDelegate);
}

void ZeroConfServicePrivate::reset (){
	mRunLoop.stop();
	for (std::map<String, DNSServiceRef>::const_iterator i = mSubscriptions.begin(); i != mSubscriptions.end(); i++){
		mRunLoop.del(i->second);
	}
	for (std::map<String, DNSServiceRef>::const_iterator i = mPublications.begin(); i != mPublications.end(); i++){
		mRunLoop.del(i->second);
	}
	mSubscriptions.clear();
	mPublications.clear();
	mError = false;
	mErrorMessage = "";
	mServices.clear();
	
	// Checking availability
	
	// DNSServiceCreateConnection is not supported by AVAI, so we fake avaiability in Linux
#ifndef LINUX	
	DNSServiceRef service;
	DNSServiceMutex.lock ();
	DNSServiceErrorType error = DNSServiceCreateConnection (&service);
	DNSServiceMutex.unlock();
	if (error){
		mAvailable     = false;
		mError        = true;
		mErrorMessage = "Could not connect to mDNS service, code=" + toHex(error);
		return;
	} 
	DNSServiceMutex.lock ();
	DNSServiceRefDeallocate (service);
	DNSServiceMutex.unlock();
#endif
	mAvailable = true;
}

bool ZeroConfServicePrivate::subscribeServices (const String & serviceType) {
	if (!mAvailable) return false;
	if (mSubscriptions.find(serviceType) != mSubscriptions.end()){
		String errString = serviceType + " already subscribed";
		Log (LogError) << LOGID << errString << std::endl;
		signalError (errString);
		return false;
	}
	DNSServiceRef service;
	DNSServiceMutex.lock ();
	DNSServiceErrorType err = DNSServiceBrowse (
			&service, 
			/*flags*/ 0, 
			/*interface*/ 0, 
			/*ifType*/serviceType.c_str(), 
			/* domain (0=all)*/0,
			&ZeroConfServicePrivate::serviceBrowseReply,
			this);
	DNSServiceMutex.unlock();
	if (err){
		signalError ("Could not subscribe type " + serviceType + " error code=" + toHex (err));
		return false;
	}
	mSubscriptions[serviceType] = service;
	mRunLoop.add(service);			
	return true;
}

bool ZeroConfServicePrivate::unsubscribeServices (const String & serviceType) {
	std::map<String, DNSServiceRef>::iterator i = mSubscriptions.find (serviceType);
	if (i == mSubscriptions.end()){
		signalError ("Did not find subscription " + serviceType);
		return false;
	}
	mRunLoop.del(i->second);
	mSubscriptions.erase(i);
	return true;
}

std::set<String> ZeroConfServicePrivate::subscribedServices () const {
	std::set<String> subscriptions;
	for (std::map<String, DNSServiceRef>::const_iterator i = mSubscriptions.begin(); i != mSubscriptions.end(); i++){
		subscriptions.insert (i->first);
	}
	return subscriptions;
}

bool ZeroConfServicePrivate::publishService (const Service & service) {
	if (!mAvailable) return false;
	// we use name just as key to find it, so we do not have to escape
	String name = service.serviceName + "." + service.serviceType + "." + service.domain;
	DNSServiceRef ref;
	
	ByteArrayPtr txtField = encodeTxtField (service.txt);
	int txtLen = txtField.get() ? txtField->size() : 0;
	if (txtLen > 65535){
		signalError ("Txt field too long, max 65535 (in publishService)");
		return false;
	}

	const char * data = txtLen == 0 ? 0 : txtField->c_array();
	
	DNSServiceMutex.lock();
	DNSServiceErrorType err = DNSServiceRegister (
			&ref, 
			/* flags*/0,
			/* interface Index*/0,
			service.serviceName.empty() ? 0 : service.serviceName.c_str(), 
			service.serviceType.c_str(),
			service.domain.empty() ? 0 : service.domain.c_str(),
			service.address.empty() ? 0 : service.address.c_str(),
			htons (service.port),
			txtLen, 
			data,
			&ZeroConfServicePrivate::serviceRegisterReply,
			this);
	DNSServiceMutex.unlock();
	
	if (err){
		signalError ("There was an error registering the service, code=" + toHex (err));
		return false;
	}
	mPublications[name] = ref;
	mRunLoop.add (ref);
	return true;
}

bool ZeroConfServicePrivate::updateService (const Service & service) {
	if (!mAvailable) return false;
	String name = service.serviceName + "." + service.serviceType + "." + service.domain;
	
	std::map<String, DNSServiceRef>::const_iterator i = mPublications.find (name);
	if (i == mPublications.end()){
		signalError ("Service to update(" + toJSON (service) + ") not found");
		return false;
	}
	RecursiveLockGuard lg (mRunLoop.mutex());

	ByteArrayPtr txtField = encodeTxtField (service.txt);
	int txtLen = txtField.get() ? txtField->size() : 0;
	if (txtLen > 65535){
		signalError ("Txt field too long, max 65535 (in updateService)");
		return false;
	}
	const char * data = txtLen == 0 ? 0 : txtField->c_array();
	
	// can update only txt record
	DNSServiceMutex.lock ();
	DNSServiceErrorType err = DNSServiceUpdateRecord (i->second, NULL, 0, txtLen, data, 0);
	DNSServiceMutex.unlock();
	if (err){
		signalError ("There was an error updating the service, code=" + toHex (err));
		return false;
	}
	return true;
}

bool ZeroConfServicePrivate::cancelService (const Service & service) {
	if (!mAvailable) return false;
	String name = service.serviceName + "." + service.serviceType + "." + service.domain;
	
	std::map<String, DNSServiceRef>::iterator i = mPublications.find (name);
	
	if (i == mPublications.end()){
		signalError ("Service to remove does not exist " + toJSON (service));
		return false;
	}
	
	mRunLoop.del(i->second); // will kill it
	mPublications.erase(i);
	return true;
}

void ZeroConfServicePrivate::serviceBrowseReply (
		DNSServiceRef ref, 
		DNSServiceFlags flags, 
		uint32_t interface, 
		DNSServiceErrorType errorCode, 
		const char * serviceName, 
		const char * serviceType, 
		const char * serviceDomain, 
		void * context){
	// Callback; DNSServiceMutex must be already locked
	
	ZeroConfServicePrivate * me = static_cast<ZeroConfServicePrivate*> (context);
	
	if (errorCode){
		me->signalError ("Error while browsing, code=" + toHex (errorCode));
		return;			
	}
	
	bool online = flags & kDNSServiceFlagsAdd;
	
	// Looking for the record
	char full [kDNSServiceMaxDomainName];
	assert (DNSServiceMutex.try_lock () == false); // must be locked by us already
	DNSServiceErrorType err = DNSServiceConstructFullName (full, serviceName, serviceType, serviceDomain);
	if (err){
		Log (LogError) << LOGID << "Bad values" << std::endl;
		return;
	}
	String fullName (full); // already have right escape quoting
	// DNSServiceConstructFullName puts no "." at the end, at least sometimes
	// WORKAROUND
	if ((fullName.size() > 0) && (fullName[fullName.size()-1] != '.')) fullName+='.';
	
	ServiceTable::iterator i = me->mServices.find(fullName);
#ifndef NDEBUG
	// Sometimes the iterator crashes...
	int numElements = me->mServices.size();
#endif
	if (i == me->mServices.end()){
		if (!online){
			// we never saw him and he went offline
			Log (LogWarning) << LOGID << "Found a ghost record serviceType: " << serviceType << " serviceName: " << serviceName << "( fullname=" << fullName << ")" << std::endl;
			return;
		}
		// lets find out something about him
		DNSServiceRef resolveService;
		DNSServiceErrorType err = DNSServiceResolve (
				&resolveService, 
				0, 
				interface,
				serviceName,
				serviceType,
				serviceDomain,
				&serviceResolveReply,
				me);
		if (err){
			me->signalError ("Error while resolving, code=" + toHex (errorCode));
			return;
		}
		me->mRunLoop.add(resolveService, true);
		return;
	}
	
	ZeroConfService::Service & s = i->second;
	
	if (online){
		// we know this service already...
		Log (LogInfo) << LOGID << "Knowing service " << toJSON (s) << " already" << std::endl;
	} else {
		Log (LogInfo) << LOGID << "Service " << toJSON (s) << " went offline" << std::endl;
		if (me->mServiceOfflineDelegate) xcall (abind (me->mServiceOfflineDelegate, s));
		if (s.own){
			me->signalError ("Own service went offline?!");
		} else {
#ifndef NDEBUG
			assert (numElements == (int) me->mServices.size());
			assert (i == me->mServices.find (fullName));
#endif
			me->mServices.erase(i);
		}
	}
}

void ZeroConfServicePrivate::serviceResolveReply (
		DNSServiceRef ref,
		DNSServiceFlags flags,
		uint32_t interface,
		DNSServiceErrorType errorCode,
		const char * cFullName,
		const char * cHostTarget,
		uint16_t port,
		uint16_t txtLen,
		const unsigned char * txtRecord,
		void * context){
	// Callback: DNSServiceMutex must be already locked
	
	
	ZeroConfServicePrivate * me = static_cast<ZeroConfServicePrivate*> (context);
	// removing from loop
	me->mRunLoop.del(ref, true);
	
	if (errorCode){
		me->signalError ("Error while resolving, code=" + toHex (errorCode));
		return;
	}
	
	String fullName   (cFullName);
	String hostTarget (cHostTarget);
	ServiceTable::iterator i = me->mServices.find (fullName);
	
	if (i == me->mServices.end()){
		// it must be new
		Service s;
		s.fullName = fullName;
		/// splitting fullName
		bool succ = Service::splitFullName(s.fullName, s.serviceName, s.serviceType, s.domain);
		if (!succ){
			me->signalError ("Error decoding DNS name");
			return;
		}
		if (cHostTarget) s.address = cHostTarget;
		s.port = ntohs (port);
		s.txt  = decodeTxtField (txtLen, txtRecord);
		me->mServices [s.fullName] = s;
		if (me->mServiceOnlineDelegate) xcall (abind (me->mServiceOnlineDelegate, s));
		return;
	}
	// must be changed
	Service & s = i->second;
	if (cHostTarget) s.address = cHostTarget;
	else s.address = "";
	s.port = ntohs (port);
	s.txt = decodeTxtField (txtLen, txtRecord);
	if (me->mServiceUpdatedDelegate) xcall (abind (me->mServiceUpdatedDelegate, s));
	return;
}

void ZeroConfServicePrivate::serviceRegisterReply (
		DNSServiceRef ref,
		DNSServiceFlags flags,
		DNSServiceErrorType errorCode,
		const char * name,
		const char * regType,
		const char * domain,
		void * context){
	// Callback DNSServiceMutex must be already locked
	ZeroConfServicePrivate * me = static_cast<ZeroConfServicePrivate*> (context);
	if (errorCode){
		me->signalError ("Error on registering service, code=" + toHex (errorCode));
		me->mRunLoop.del(ref, true);
		return;
	}
	// do not remove service registration, it will cancel the service
}

}

#endif
