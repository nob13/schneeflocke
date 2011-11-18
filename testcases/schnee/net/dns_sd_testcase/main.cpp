#include <schnee/schnee.h>
#include <schnee/net/ZeroConfService.h>

#include <boost/thread.hpp>

/*
 * Manual test case for testing the ZeroConf DNS SD connection.
 * It just prints out offline/online SLXMPP instances in your network and registers one of its own
 * (Which is not backed by a real service)
 */

typedef sf::ZeroConfService::Service Service;

void serviceOnline (const Service & service){
	std::cout << "Service online: " << sf::toJSON (service) << std::endl; 
}

void serviceOffline (const Service & service){
	std::cout << "Service offline: " << sf::toJSON (service) << std::endl;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	
	sf::ZeroConfService z;
	if (!z.available()){
		std::cerr << "ZeroConf not available " << z.errorMessage()<< std::endl;
		return 1;
	}
	z.serviceOnline() = &serviceOnline;
	z.serviceOffline() = &serviceOffline;
	
	z.subscribeServices ("_presence._tcp");
	
	// register one Service
	Service s;
	s.serviceName = "faked@DRSMAC03";
	s.serviceType = "_presence._tcp";
	s.port = 1234;
	s.txt["1st"] = "MyLittle";
	s.txt["last"] = "Service";
	s.txt["msg"] = "is online";
	s.txt["txtvers"] = "1";
	s.txt["vc"] = "!"; // dunno
	s.txt["port.p2pj"] = "1234";
	s.txt["status"] = "avail";
	z.publishService (s);
	
	
	boost::thread::sleep (boost::get_system_time() + boost::posix_time::seconds (100));
	
	return 0;
}
