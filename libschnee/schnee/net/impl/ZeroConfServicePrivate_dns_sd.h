#pragma once

#ifdef ENABLE_DNS_SD

///@cond DEV

/**
 * @file
 * ZeroConf Implementation when dns_sd.h / dns_sd.lib is available
 */


#include "../ZeroConfService.h"
#include <schnee/tools/Log.h>

#include <boost/thread.hpp>

// mDNSResponder interface
// Also supported by avahi using a compaibility layer, although not supported.
#include <dns_sd.h>

// errno handling
#include <errno.h>
#include <string.h>
#include <netinet/in.h> // for htons
// conversion
#include <cstdlib>

namespace sf {

class DNSServiceRunLoop {
public:
	DNSServiceRunLoop ();
	
	~DNSServiceRunLoop ();
	
	/// Add one of the DNSServiceRefs
	void add (DNSServiceRef service, bool inCallback = false);
	
	/// Del one of the DNSServiceRefs
	/// If you are in a DNSResponder callback, set inCallback to true (because then the DNSServiceMutex is already locked)
	void del (DNSServiceRef service, bool inCallback = false);
	
	/// one iteration in the run loop
	void iteration ();	
	
	/// runs the loop
	void run ();
	
	/// Stops the loop
	void stop ();

	/// Access to the mutex
	RecursiveMutex & mutex () { return mMutex; }
private:
	
	/// the run loop (in it's own thread)
	void runLoop ();
	
	/// currently running DNSServiceRefs...
	std::set<DNSServiceRef> mServices;

	/// Thread for the run loop
	boost::thread * mThread;
	
	/// Mutex for the run loop
	RecursiveMutex mMutex;
	
	/// indicating stop
	bool mStop;
};

/// Private part of ZeroConfService
/// Not for use for public
class ZeroConfServicePrivate {
public:
	typedef ZeroConfService::Service Service;

	ZeroConfServicePrivate ();
	~ZeroConfServicePrivate ();
	
	
	void signalError (const String & msg);
	
	void reset ();
	
	bool subscribeServices (const String & serviceType);
	
	bool unsubscribeServices (const String & serviceType);
	
	std::set<String> subscribedServices () const ;

	bool publishService (const Service & service);
	
	bool updateService (const Service & service);
	
	bool cancelService (const Service & service);
	
private:
	
	/// callback for browse actions
	static void serviceBrowseReply (
			DNSServiceRef ref, 
			DNSServiceFlags flags, 
			uint32_t interface, 
			DNSServiceErrorType errorCode, 
			const char * serviceName, 
			const char * serviceType, 
			const char * serviceDomain, 
			void * context);
	
	/// callback for resolving reply
	static void serviceResolveReply (
			DNSServiceRef ref,
			DNSServiceFlags flags,
			uint32_t interface,
			DNSServiceErrorType errorCode,
			const char * cFullName,
			const char * cHostTarget,
			uint16_t port,
			uint16_t txtLen,
			const unsigned char * txtRecord,
			void * context);
	
	
	/// callback for register reply
	static void serviceRegisterReply (
			DNSServiceRef ref,
			DNSServiceFlags flags,
			DNSServiceErrorType errorCode,
			const char * name,
			const char * regType,
			const char * domain,
			void * context);

	friend class ZeroConfService;
	
	bool 				mAvailable;
	DNSServiceRunLoop 	mRunLoop;
	bool 				mError;
	String 				mErrorMessage;
	
	std::map <String, DNSServiceRef> mSubscriptions; // service subscriptions   (from serviceType to DNSServiceRef)
	std::map <String, DNSServiceRef> mPublications;  // service publications	(from serviceName.serviceType to DNSServiceRef)
	
	typedef std::map<String, Service> ServiceTable;
	
	/// Services we are browsing currently
	ServiceTable mServices;

	ZeroConfService::ServiceDelegate mServiceOnlineDelegate;
	ZeroConfService::ServiceDelegate mServiceOfflineDelegate;
	ZeroConfService::ServiceDelegate mServiceUpdatedDelegate;
	VoidDelegate mErrorDelegate;
};

}


///@endcond

#endif
