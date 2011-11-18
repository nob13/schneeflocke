#include "UDTServer.h"
#include "UDTSocket.h"
#include <schnee/tools/Log.h>
#include <string.h>
#ifndef WIN32
#include <arpa/inet.h>
#endif

namespace sf {

UDTServer::UDTServer () {
	SF_REGISTER_ME;
	mSocket = UDT::socket(AF_INET, SOCK_STREAM, 0);
	
	// deactivating confusing reuse
	bool reuse = false;
	UDT::setsockopt(mSocket,0, UDT_REUSEADDR, &reuse, sizeof(reuse));

	// deactivating synchronized mode
	bool sync = false; 
	SF_UDT_CHECK (UDT::setsockopt (mSocket, 0, UDT_SNDSYN, &sync, sizeof(sync)));
	SF_UDT_CHECK (UDT::setsockopt (mSocket, 0, UDT_RCVSYN, &sync, sizeof(sync)));

	// accepted sockets earn some attributes from listening sockets, e.g. buffer size
	// thats why we have to set them here
	UDTSocket::setDefaultBufferSize(mSocket);
	
	UDTMainLoop::instance().add(this);
	
	mError = NoError;
}
UDTServer::~UDTServer (){
	SF_UNREGISTER_ME;
	UDTMainLoop::instance().remove (this);
	UDT::close (mSocket);
}

Error UDTServer::listen (int port) {
	LockGuard guard (mMutex);
	struct sockaddr_in addr;
	memset (&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	addr.sin_addr.s_addr = inet_addr ("0.0.0.0");
	
	int result = UDT::bind (mSocket, (const struct sockaddr*) &addr, sizeof (addr));
	if (result < 0) {
		UDT::ERRORINFO & info = UDT::getlasterror();
		Log (LogError) << LOGID << "Could not bind " << info.getErrorMessage() << std::endl;
		return error::ServerError;
	}
	
	result = UDT::listen(mSocket, 30);
	if (result < 0) {
		UDT::ERRORINFO & info = UDT::getlasterror();
		Log (LogError) << LOGID << "Error on listening: " << info.getErrorMessage() << std::endl;
		return error::ServerError;
	}
	return NoError;
}

int UDTServer::pendingConnections () const {
	LockGuard guard (mMutex);
	if (mError) return -1;
	return (int) mWaiting.size();
}

int UDTServer::port () const {
	LockGuard guard (mMutex);
	if (mError) return -1;
	struct sockaddr_in sock;
	int len = sizeof(sock);
	int result = UDT::getsockname(mSocket, (struct sockaddr*) & sock, &len);
	if (result == UDT::ERROR) {
		Log (LogWarning) << LOGID << "Could not get port due: " << UDT::getlasterror().getErrorMessage() << std::endl;
		return -1;
	}
	assert (len == sizeof(sock));
	if (len != sizeof(sock)) return -2; // strange
	return ntohs (sock.sin_port);
}


UDTSocket * UDTServer::nextConnection() {
	LockGuard guard (mMutex);
	if (mWaiting.empty()) return 0;
	UDTSOCKET s = mWaiting.front();
	mWaiting.pop_front();
	return new UDTSocket (s);
}

void UDTServer::onReadable  () {
	// there must be a new connection
	{
		LockGuard guard (mMutex);
		struct sockaddr_in address;
		int addrLen = sizeof(address);
		UDTSOCKET s = UDT::accept (mSocket, (struct sockaddr*) &address, &addrLen);
		if (s == UDT::INVALID_SOCK){
			UDT::ERRORINFO & info = UDT::getlasterror();
			Log (LogError) << LOGID << "Error on accepting " << info.getErrorMessage() << std::endl;
			return;
		}
		mWaiting.push_back (s);
		
		#ifndef WIN32 // My MSVC2008 doesn't have inet_ntop
		#ifndef NDEBUG
		char human[128];
		int humanPort = ntohs (address.sin_port);
		inet_ntop (AF_INET, &address.sin_addr, human, sizeof(human));
		Log (LogInfo) << LOGID << "Accepted connection from: " << human << ":" << humanPort << std::endl;
		#endif
		#endif
	}
	if (mNewConnection) mNewConnection();
}



}
