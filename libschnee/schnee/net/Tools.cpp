#include "Tools.h"

#include <boost/asio.hpp>
#include <schnee/net/impl/IOService.h>
#include <schnee/tools/Log.h>

using boost::asio::ip::tcp;

#ifndef WIN32
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace sf {
namespace net {
String hostName (){
	boost::system::error_code ec;
	sf::String result = boost::asio::ip::host_name(ec);
	if (ec) {
		sf::Log (LogError) << LOGID << "Error getting hostname, returning localhost" << std::endl;
		return "localhost";
	}
	return result;
}

#ifdef WIN32
// In Linux this function always returns 127.0.0.1
// In Windows howewer it seems to work.
bool localAddresses (std::vector<sf::String> * dest, bool withoutLocal) {
	boost::system::error_code ec;
	sf::String name = boost::asio::ip::host_name(ec);
	if (ec) return false;
	boost::asio::ip::tcp::resolver resolver (IOService::service());
	// using http as protocol, shall be Ok
	tcp::resolver_query query (name, "http");
	tcp::resolver_iterator i = resolver.resolve(query, ec);
	if (ec) return false;
	dest->clear();
	tcp::resolver_iterator end;
	while (i!=end){
		tcp::endpoint ep = *i;
		std::string s = ep.address().to_string(ec);
		if (ec) break;
		if (!(withoutLocal && (s == "127.0.0.1" || s == "::1")))
			dest->push_back (s);
		i++;
	}
	if (dest->empty()) return false;
	return true;
}

bool localIpv4Addresses (std::vector<sf::String> * dest, bool withoutLocal) {
	boost::system::error_code ec;
	sf::String name = boost::asio::ip::host_name(ec);
	if (ec) return false;
	boost::asio::ip::tcp::resolver resolver (IOService::service());
	// using http as protocol, shall be Ok
	tcp::resolver_query query (name, "http");
	tcp::resolver_iterator i = resolver.resolve(query, ec);
	if (ec) return false;
	dest->clear();
	tcp::resolver_iterator end;
	for (;i!=end;i++){
		tcp::endpoint ep = *i;
		if (!ep.address().is_v4()) continue;
		std::string s = ep.address().to_string(ec);
		if (ec) break;
		if (!(withoutLocal && (s == "127.0.0.1" || s == "::1")))
			dest->push_back (s);
	}
	if (dest->empty()) return false;
	return true;
}

#else

bool localAddresses (std::vector<sf::String> * dest, bool withoutLocal) {
	struct ifaddrs * addrs;
	int result = getifaddrs (&addrs);
	if (result) return false;
	struct ifaddrs * current = addrs;
	while (current){
		struct sockaddr * addr = current->ifa_addr;
		if (addr) {
			if (addr->sa_family == AF_INET){
				struct sockaddr_in *sin = (struct sockaddr_in*) addr;
				char buffer [256];
				if (inet_ntop (AF_INET, &sin->sin_addr, buffer, 256)){
					std::string s (buffer);
					if (!(withoutLocal && s.substr(0,4) == "127."))
						dest->push_back (s);
				}
			}
			if (addr->sa_family == AF_INET6){
				struct sockaddr_in6 * sin6 = (struct sockaddr_in6*) addr;
				char buffer [256];
				if (inet_ntop (AF_INET6, &sin6->sin6_addr, buffer, 256)){
					std::string s (buffer);
					if (!(withoutLocal && s == "::1"))
						dest->push_back (s);
				}
			}
		}
		current = current->ifa_next;
	}
	freeifaddrs (addrs);
	return true;
}

bool localIpv4Addresses (std::vector<sf::String> * dest, bool withoutLocal) {
	struct ifaddrs * addrs;
	int result = getifaddrs (&addrs);
	if (result) return false;
	struct ifaddrs * current = addrs;
	while (current){
		struct sockaddr * addr = current->ifa_addr;
		if (addr) {
			if (addr->sa_family == AF_INET){
				struct sockaddr_in *sin = (struct sockaddr_in*) addr;
				char buffer [256];
				if (inet_ntop (AF_INET, &sin->sin_addr, buffer, 256)){
					std::string s (buffer);
					if (!(withoutLocal && s.substr(0,4) == "127."))
						dest->push_back (s);
				}
			}
		}
		current = current->ifa_next;
	}
	freeifaddrs (addrs);
	return true;
}


#endif

}
}
