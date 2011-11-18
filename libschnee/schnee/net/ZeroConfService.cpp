#include "ZeroConfService.h"
#include <schnee/tools/Log.h>
#ifdef ENABLE_DNS_SD
#include "impl/ZeroConfServicePrivate_dns_sd.h"
#else
#include "impl/ZeroConfServicePrivate_null.h"
#endif

namespace sf {

static bool decodeDNS (const String & dns, String & decoded){
	String target = dns;
	size_t pos = 0;
	// searching for \XYZ where XYZ is numerical
	while ((pos = target.find('\\', pos)) != target.npos){
		if (target[pos+1] == '\\'){
			target.replace (pos, 2, "\\");
			pos++;
			continue;
		}
		if (target[pos+1] == '.'){
			target.replace (pos, 2, ".");
			pos++;
			continue;
		}

		// on error there are values we throw away anyway so we don't check them
		char * last;
		long int number = std::strtol (target.c_str() + pos + 1, &last, 10);
		// only allow small range (visible ASCII, got nore more information about that)
		if (number < 0x20 || number > 0x7E){
			return false;
		}
		char c = (char) number;
		int num = (last - target.c_str() - pos);
		target.replace(pos, num, 1, c);
		pos++;
	}
	decoded = target;
	// Log (LogInfo) << LOGID << "Decoded " << dns << " to " << decoded << std::endl;
	return true;
}

bool ZeroConfService::Service::splitFullName (const String & fullName, String & serviceName, String & serviceType, String & serviceDomain){
	// name is of the form nobAmMac\064DRSMAC03._presence._tcp.local.
	// dots:                                   ^4        ^3   ^2    ^1
	// we want nobAmMac@DRSMAC03 + _presence._tcp + local.
	/*
	/// splitting fullName
	size_t firstDot   = fullName.find('.');
	if (firstDot == fullName.npos) return false;
	size_t lastDot    = fullName.rfind('.');
	if (lastDot == fullName.npos) return false;
	size_t preLastDot = fullName.rfind('.', lastDot - 1);
	if (preLastDot == fullName.npos) return false;
	
	String name   = fullName.substr (0, firstDot);
	String type   = fullName.substr (firstDot + 1, preLastDot - firstDot -1);
	String domain = fullName.substr (preLastDot + 1, fullName.length() - preLastDot);
	*/

	// Splitting:
	size_t ldot1      = fullName.rfind ('.');
	size_t ldot2      = fullName.rfind ('.', ldot1 - 1);
	size_t ldot3      = fullName.rfind ('.', ldot2 - 1);
	size_t ldot4      = fullName.rfind ('.', ldot3 - 1);
	if (ldot1 == fullName.npos || ldot2 == fullName.npos || ldot3 == fullName.npos || ldot4 == fullName.npos){
		return false;
	}

	String name   = String (fullName.begin(), fullName.begin() + ldot4);
	String type   = String (fullName.begin() + ldot4 + 1, fullName.begin() + ldot2);
	String domain = String (fullName.begin() + ldot2 + 1, fullName.end());


	Log (LogInfo) << LOGID << "Splitted " << fullName << " into " << name << " + " << type << " + " << domain << std::endl;
	bool succ = true;
	succ = succ & decodeDNS (name,   serviceName);
	succ = succ & decodeDNS (type,   serviceType);
	succ = succ & decodeDNS (domain, serviceDomain);
	return succ;
}

void ZeroConfService::Service::serialize (Serialization & s) const {
	if (!fullName.empty()) s ("fullName", fullName);
	s ("type", serviceType);
	s ("name", serviceName);
	s ("domain", domain);
	s ("address", address);
	s ("port", port);
	if (!txt.empty()){
		s ("txt", txt);
	}
	s ("own", own);
}


ZeroConfService::ZeroConfService () {
	d = new ZeroConfServicePrivate ();
}

ZeroConfService::~ZeroConfService () {
	delete d;
}

bool ZeroConfService::available () const {
	return d->mAvailable;
}

bool ZeroConfService::errorFlag () const {
	return d->mError;
}

String ZeroConfService::errorMessage ()const {
	return d->mErrorMessage;
}

void ZeroConfService::resetError () {
	d->mError = false;
	d->mErrorMessage = "";
}

void ZeroConfService::reset () {
	d->reset ();
}

bool ZeroConfService::subscribeServices (const String & serviceType) {
	return (d->subscribeServices (serviceType));
}

bool ZeroConfService::unsubscribeServices (const String & serviceType) {
	return (d->unsubscribeServices (serviceType));
}

std::set<String> ZeroConfService::subscribedServices () const {
	return (d->subscribedServices());
}

bool ZeroConfService::publishService (const Service & service) {
	return (d->publishService (service));
}

bool ZeroConfService::updateService (const Service & service) {
	return (d->updateService (service));
}

bool ZeroConfService::cancelService (const Service & service) {
	return (d->cancelService (service));
}


ZeroConfService::ServiceDelegate & ZeroConfService::serviceOnline (){
	return d->mServiceOnlineDelegate;
}


ZeroConfService::ServiceDelegate & ZeroConfService::serviceOffline (){
	return d->mServiceOfflineDelegate;
}

ZeroConfService::ServiceDelegate & ZeroConfService::serviceUpdated () {
	return d->mServiceUpdatedDelegate;
}

VoidDelegate & ZeroConfService::error () {
	return d->mErrorDelegate;
}



}
