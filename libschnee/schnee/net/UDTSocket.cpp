#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#endif

#include "UDTSocket.h"
#include <schnee/tools/async/DelegateBase.h>
#include <sys/types.h>
#include "impl/UDTMainLoop.h"
#include <schnee/tools/Log.h>
#ifndef WIN32
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include <boost/thread.hpp>
namespace sf {

const int UDTSocket::gInputTransferSize  = 20000;
const int UDTSocket::gMaxInputBufferSize = 1024 * 1024 * 10; // 10mb 

UDTSocket::UDTSocket () {
	SF_REGISTER_ME;
	init (UDT::socket(AF_INET, SOCK_STREAM, 0));
}

UDTSocket::UDTSocket (UDTSOCKET s) {
	SF_REGISTER_ME;
	init (s);
}

UDTSocket::~UDTSocket () {
	SF_UNREGISTER_ME;
	if (mSocket > 0) UDTMainLoop::instance().remove(this);
	UDT::close (mSocket);
}

Error UDTSocket::connect (const String & host, int port) {
	LockGuard guard (mMutex);
	struct sockaddr_in dst;
	memset (&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_addr.s_addr = inet_addr (host.c_str());
	if (dst.sin_addr.s_addr == INADDR_NONE) { // won't broadcast here
		return error::InvalidArgument;
	}
	dst.sin_port = htons (port);

	mConnecting = true;
	int result = UDT::connect(mSocket, (const struct sockaddr*) &dst, sizeof(dst));
	mConnecting = false;

	if (result) {
		UDT::ERRORINFO & info = UDT::getlasterror();
		Log (LogInfo) << LOGID << "Could not connect: " << info.getErrorMessage() << std::endl;
		return error::CouldNotConnectHost;
	}
	return NoError;
}

Error UDTSocket::connectRendezvous (const String & host, int port) {
	{
		LockGuard guard (mMutex);
		bool yes = true;
		UDT::setsockopt (mSocket, 0, UDT_RENDEZVOUS, &yes, sizeof(yes));
	}
	Error e = connect (host, port);
	{
		LockGuard guard (mMutex);
		bool no = false;
		UDT::setsockopt (mSocket, 0, UDT_RENDEZVOUS, &no, sizeof(no));
	}
	return e;
}

void UDTSocket::connectRendezvousAsync (const String & host, int port, const function <void (Error)> & callback) {
	LockGuard guard (mMutex);
	boost::thread t (abind (dMemFun (this, &UDTSocket::connectInOtherThread), host, port, true, callback));
}

Error UDTSocket::bind (int port) {
	LockGuard guard (mMutex);
	struct sockaddr_in dst;
	memset (&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_addr.s_addr = 0; // all interfaces
	if (dst.sin_addr.s_addr == INADDR_NONE) { // won't broadcast here
		return error::InvalidArgument;
	}
	dst.sin_port = htons (port);
	int result = UDT::bind (mSocket, (const struct sockaddr*) &dst, sizeof (dst));
	if (result) {
		Log (LogInfo) << LOGID << "Bind failed: " << UDT::getlasterror().getErrorMessage() << std::endl;
		return error::ConnectionError;
	}
	return NoError;
}

Error UDTSocket::rebind (UDPSocket * socket) {
	int port = socket->port();
	if (port <= 0){
		Log (LogWarning) << LOGID << "Socket was not bound!" << std::endl;
		return error::ConnectionError;
	}
	socket->close ();
	return bind (port);
}


int UDTSocket::port () const {
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

bool UDTSocket::isConnected () const {
	LockGuard guard (mMutex);
	return isConnected_locked ();
}

ByteArrayPtr UDTSocket::peek (long maxSize) {
	LockGuard guard (mMutex);
	if (maxSize < 0) {
		return sf::createByteArrayPtr(mInputBuffer);
	} else {
		size_t size = mInputBuffer.size();
		if ((size_t) maxSize < size) {
			return ByteArrayPtr (new ByteArray (mInputBuffer.const_c_array(), maxSize));
		} else {
			// them all
			return sf::createByteArrayPtr (mInputBuffer);
		}
	}
}

long UDTSocket::bytesAvailable() const {
	LockGuard guard (mMutex);
	return (long) mInputBuffer.size();
}

Error UDTSocket::write (const ByteArrayPtr& data, const ResultCallback & callback) {
	{
		LockGuard guard (mMutex);
		if (data->size() > 1024 * 1024 * 16) return error::TooMuch; // 16mb
		mOutputBuffer.push_back (OutputElement (data, callback));
		mOutputBufferSize += data->size();

		if (mWriting) return NoError;
		mWriting = true;
	}
	// Will continue on next write event
	UDTMainLoop::instance().addWrite(this);
	return NoError;
}

ByteArrayPtr UDTSocket::read (long maxSize) {
	LockGuard guard (mMutex);
	if (mInputBuffer.empty()){
		return ByteArrayPtr ();
	}
	if (maxSize < 0 || maxSize > (long) mInputBuffer.size()){
		ByteArrayPtr result = sf::createByteArrayPtr();
		result->swap(mInputBuffer);
		return result;
	}
	// cut a part
	ByteArrayPtr result = ByteArrayPtr (new ByteArray (mInputBuffer.const_c_array(), maxSize));
	mInputBuffer.l_truncate (maxSize);
	return result;
}

void UDTSocket::close (const ResultCallback & resultCallback) {
	UDTMainLoop::instance().remove(this); // MainLoop may not be inside lock
	LockGuard guard (mMutex);
	int result = UDT::close(mSocket);
	if (result < 0){
		Log (LogError) << LOGID << "Could not close socket: " << UDT::getlasterror().getErrorMessage() << std::endl;
		// will leaking?
	}
	mSocket = -1;
	mWriting = false;
	notifyAsync (resultCallback, NoError);
}

static std::string sockIp (const struct sockaddr_in & sock) {
	char ip [64];
	// WIN32 doesn't support inet_ntop
	// inet_ntop (AF_INET, &sock.sin_addr, ip, 64)
	if (!getnameinfo ((sockaddr*)&sock, sizeof(sock), ip, sizeof (ip), NULL, 0, NI_NUMERICHOST)){
		return ip;
	}
	return std::string (); // error
}

Channel::ChannelInfo UDTSocket::info () const {
	LockGuard  guard (mMutex);
	ChannelInfo info;
	struct sockaddr_in sock;
	memset (&sock, 0, sizeof(sock));
	int socklen = sizeof (sock);
	int result = UDT::getsockname(mSocket, (struct sockaddr*) & sock, &socklen);
	if (!result && socklen == sizeof(sock)) {
		std::string lIp = sockIp (sock);
		if (!lIp.empty()) {
			info.laddress = lIp + ":" + toString (ntohs (sock.sin_port));
		}
	}
	memset (&sock, 0, sizeof(sock));
	socklen = sizeof (sock);
	result = UDT::getpeername(mSocket, (struct sockaddr*) & sock, &socklen);
	if (!result && socklen == sizeof(sock)) {
		std::string rIp = sockIp (sock);
		if (!rIp.empty()){
			info.raddress = rIp + ":" + toString (ntohs(sock.sin_port));
		}
	}
	return info;
}

/*static*/ void UDTSocket::setDefaultBufferSize (UDTSOCKET s) {
	// small buffer size (instead of 10MB default) decreases raw throroughput
	// but increases reactivity much.
	// It bases upon a typical fast internet link: 2MB/s with RTT 20ms (=40kb needed)
	int bufSize = 524288;
	UDT::setsockopt (s, 0, UDT_SNDBUF, &bufSize, sizeof (bufSize));
	UDT::setsockopt (s, 0, UDT_RCVBUF, &bufSize, sizeof (bufSize));
}

void UDTSocket::onReadable  () {
	int receivedSum = 0;
	bool error = false;
	{
		LockGuard guard (mMutex);
		while (true) {
			if (mInputBuffer.size() + (size_t) gInputTransferSize > (size_t) gMaxInputBufferSize) {
				Log (LogWarning) << LOGID << "Overflowing in UDTSocket" << std::endl;
				// overflow, user shall consume!
				break;
			}
			mInputBuffer.reserve (mInputBuffer.size() + 2 * gInputTransferSize);
			char transferBuf [gInputTransferSize];
			int received = UDT::recv (mSocket, transferBuf, gInputTransferSize, 0);
			if (received == UDT::ERROR) {
				UDT::ERRORINFO & errInfo = UDT::getlasterror();
				if (errInfo.getErrorCode() == UDT::ERRORINFO::EASYNCRCV) break; // ignoring, no data available
				Log (LogInfo) << LOGID << "Could not read on socket: " << errInfo.getErrorMessage() << std::endl;
				mError = error::ReadError;
				error = true;
				UDT::close(mSocket);
				break;
			}
			if (received > 0){
				mInputBuffer.append(transferBuf, received);
				receivedSum+=received;
			} else
				break;
		}
	}
	// Log (LogInfo) << LOGID << "Received " << receivedSum << " bytes" << std::endl;
	if ((error || receivedSum > 0) && mChanged) mChanged();
}

bool UDTSocket::onWriteable () {
	Error e = NoError;
	bool continueWriting = false;
	{
		LockGuard guard (mMutex);
		if (mWriting){
			e = continueWriting_locked ();
			if (e) {
				UDT::close(mSocket);
			}
		}
		if (mWriting && !e){
			continueWriting = true;
		}
	}
	if (e && mChanged) mChanged();
	return continueWriting;
}

bool UDTSocket::isConnected_locked () const {
	return UDT::recv(mSocket, 0, 0, 0) == 0;
}


void UDTSocket::connectInOtherThread (const String & address, int port, bool rendezvous, const function <void (Error)> & callback) {
	{
		LockGuard guard (mMutex);
		if (rendezvous) {
			bool yes = true;
			UDT::setsockopt (mSocket, 0, UDT_RENDEZVOUS, &yes, sizeof(yes));
		}
	}
	Error result = connect (address, port);
	if (callback) callback (result);
	{
		LockGuard guard (mMutex);
		if (rendezvous) {
			bool no = false;
			UDT::setsockopt (mSocket, 0, UDT_RENDEZVOUS, &no, sizeof(no));
		}
	}
}

void UDTSocket::init (UDTSOCKET s) {
	mSocket = s;
	mOutputBufferSize = 0;
	mWriting    = false;
	mConnecting = false;
	mError = NoError;

	assert (s > 0);
	if (s <= 0) {
		mError = error::NotInitialized;
		return;
	}
	// Making socket completely asynchronous
	bool sync = false;
	UDT::setsockopt (mSocket, 0, UDT_SNDSYN, &sync, sizeof(sync));
	UDT::setsockopt (mSocket, 0, UDT_RCVSYN, &sync, sizeof(sync));
	
	setDefaultBufferSize (mSocket);

	UDTMainLoop::instance().add(this);
}

Error UDTSocket::continueWriting_locked () {
	if (mOutputBufferSize == 0) {
		mWriting = false;
		return NoError;
	}
	mWriting = true;
	OutputElement next = mOutputBuffer.front();
	bool notifySuccess = false;
	int size = (int) next.data->size();
	int result = UDT::send (mSocket, next.data->const_c_array(), size, 0);

	if (result < 0) {
		Log (LogWarning) << LOGID << "Could not send " << size << "bytes: " << UDT::getlasterror().getErrorMessage() << std::endl;
		mError   = error::WriteError;
		mWriting = false;
		return error::WriteError;
	}
	// Log (LogInfo) << LOGID << "Sent " << result << "/" << size << " bytes" << std::endl;
	if (result == 0){
		return NoError; // ?
	}
	if (result < size) {
		// truncate it
		next.data->l_truncate (result);
	} else {
		// fully sent
		notifySuccess = true;
		mOutputBuffer.pop_front();
	}
	mOutputBufferSize -= result;
	if (notifySuccess && next.callback) {
		mMutex.unlock();
		next.callback(NoError);
		mMutex.lock();
	}
	return NoError;
}



}
