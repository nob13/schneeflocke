#pragma once

#include <schnee/sftypes.h>
#include <schnee/tools/Serialization.h>

#include <set>

namespace sf {

struct ZeroConfServicePrivate;

/**
 * An easy API for finding and publishing services (open TCP conncetions ;)) using ZeroConf.
 * 
 * This class only deals with services published by mDNSResponder (Service Directory)
 * 
 * Most calls are done asynchronously. If you want to track errors, use the error delegate.
 * 
 * The implementation is to be considered stable, as most state machine work is done inside
 * the ZeroConf daemon. So you don't have to care about network devices, changing network and so on.
 * 
 * Implementation may use Apple Bonjour or Avahi (currently onoly Bonjour compatibility API of Avahi).
 * 
 */
class ZeroConfService {
public:
	/// A service as we can found it through Zeroconf.
	/// Services are found using serviceType.
	/// The adress makes them reachable. All other fields
	/// are more or less user defined.
	struct Service {
		Service () : port (0), own (false) {}
		~Service() {}
		/// full name for the service (so that you find it again when subscribing)
		/// this is serviceName.serviceType.domain
		/// it is in DNS name escaping \\064 for @ e.g.
		String fullName;
		/// type of the service (e.g._presence._tcp)
		/// this is mandatory when publishing your own service
		String serviceType;
		/// Name of the service 
		/// serviceName = "" means not defined
		String serviceName;
		/// domain of the service (mostly local)
		/// domain = "" means not defined
		String domain;
		/// address of the service (in DNS notation)
		/// address = "" means not defined
		String address;
		/// Port of the service
		/// port = 0 means not defined
		int port;
		/// DNS txt resource record used by mDNS
		/// Description about the restrictions:
		/// May not contain '=' 
		/// http://www.zeroconf.org/Rendezvous/txtrecords.html
		std::map<String,String> txt;
		/// the service is published by our own
		/// (gets set internally)
		bool own;
		
		/**
		 * Splits a full DNS name into its parts.
		 * 
		 * E.g. nos\064myComputer._presence._tcp.local.
		 * will be split into "nos@myComputer" "_presence._tcp" and "local."
		 * 
		 * @param fullName    the full DNS name
		 * @param serviceName saves the service name here
		 * @param serviceType saves the service type here
		 * @param domain      saves the domain here
		 * @return success state
		 */
		static bool splitFullName (const String & fullName, String & serviceName, String & serviceType, String & domain);
		
		void serialize (Serialization & serialization) const;
	};
	
	
	/// A delegate delivering a Service
	typedef function <void (const Service &)> ServiceDelegate;
	
	/// @name State
	/// @{
	
	/// Constructor
	ZeroConfService ();
	/// Destructor
	~ZeroConfService ();
	
	/// Whether ZeroConf services are available
	bool available () const;
	/// Whether there was an error
	bool   errorFlag () const;
	/// A description of the error, if there was one
	String errorMessage ()const;
	/// Resets the error, shall work in most cases.
	/// If the error is non correctable, available() will come to false.
	void resetError ();
	
	/// Resets the Service and everything
	void reset ();
	/// @}
	
	/// @name Subscribing
	/// @{
	
	/**
	 * Subscribe all services of a specific type
	 * @param  serviceType the type to subscribe (e.g. "_workstation._tcp")
	 * @return whether call was successful. If not, call errorMessage for a description
	 */
	bool subscribeServices (const String & serviceType);
	
	/**
	 * Unsubscribe a kind of service type.
	 * @param serviceType the type to unsubscribe
	 * @return whether call was successfull. If not, call errorMessage for a description.
	 */
	bool unsubscribeServices (const String & serviceType);
	
	/**
	 * Gives you access to subscribed service types
	 * @return a set of all service types you subscribed
	 */
	std::set<String> subscribedServices () const;
	/// @}
	
	/// @name Publishing
	/**
	 * Publish your own service on the Net. If you want it easy, just 
	 * define some fields of the Service (like serviceType and Port)
	 * 
	 * @return success state
	 */
	bool publishService (const Service & service);

	/**
	 * Update the TXT record of a published service. 
	 * 
	 * Note: you can only update services you published for yourself and only the TXT records.
	 * 
	 * @return success state
	 */
	bool updateService (const Service & service);
	
	/**
	 * Removes service publication which was added through publishService
	 * 
	 * @param service service to unpublish (serviceName and serviceType will be used)
	 * @return success state
	 */
	bool cancelService (const Service & service);
	
	
	/// @name Delegates
	/// @{
	
	/// A Service (of the type you subscribed) went online
	ServiceDelegate & serviceOnline ();
	
	/// A service (of the type you subscribed) went offline
	ServiceDelegate & serviceOffline ();
	
	/// A service (of the type you subscribed) updated
	ServiceDelegate & serviceUpdated ();
	
	/// A signal that there was an error
	VoidDelegate & error ();
	
	/// @}
private:
	ZeroConfServicePrivate * d;
	
};

}
