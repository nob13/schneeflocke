#pragma once

#include "../ZeroConfService.h"

///@cond DEV

/**
 * @file Empty ZeroConfService implementation if no such service is compiled in
 */

namespace sf {

struct ZeroConfServicePrivate {
	typedef ZeroConfService::Service Service;
	
	bool 				mAvailable;
	bool 				mError;
	String 				mErrorMessage;
	
	ZeroConfService::ServiceDelegate mServiceOnlineDelegate;
	ZeroConfService::ServiceDelegate mServiceOfflineDelegate;
	ZeroConfService::ServiceDelegate mServiceUpdatedDelegate;
	VoidDelegate mErrorDelegate;
	
	ZeroConfServicePrivate (){
		reset ();
	}
	
	void reset (){
		mAvailable    = false;
		mError       = true;
		mErrorMessage = "No ZeroConfService compiled in"; 
	}
	
	bool subscribeServices (const String & serviceType) {
		return false;
	}
	
	
	bool unsubscribeServices (const String & serviceType) {
		return false;
	}
	
	std::set<String> subscribedServices () const {
		return std::set<String> ();
	}

	bool publishService (const Service & service) {
		return false;
	}

	bool updateService (const Service & service) {
		return false;
	}

	bool cancelService (const Service & service) {
		return false;
	}
};


}

///@endcond
