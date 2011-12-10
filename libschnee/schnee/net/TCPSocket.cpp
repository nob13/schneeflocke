#include "TCPSocket.h"
#include "impl/TCPSocketPrivate.h"
#include <schnee/tools/Log.h>
using boost::asio::ip::tcp;

namespace sf {

TCPSocket::TCPSocket() {
	if (!IOService::hasInstance()){
		Log (LogError) << LOGID << "Cannot create an TCPSocket without having a TCPSocket. Ending the program, it would crash anyway." << std::endl;
		std::exit(1);
	}
	
	d = new TCPSocketPrivateImpl();
}

TCPSocket::TCPSocket (TCPSocketPrivate * init){
	// someone else does initialization
	d = init;
}

TCPSocket::~TCPSocket() {
	d->requestDelete ();
}

Error TCPSocket::connectToHost(const String & host, int port, int timeOut, const ResultCallback & callback){
	return d->connectToHost (host, port, timeOut, callback);
}

bool TCPSocket::isConnected() const {
	return d->isConnected ();
}

void TCPSocket::disconnectFromHost (){
	return d->disconnectFromHost ();
}

bool TCPSocket::keepAlive () const {
	return d->keepAlive ();
}

bool TCPSocket::setKeepAlive (bool v) {
	return d->setKeepAlive (v);
}

Error TCPSocket::error() const {
	return d->error();
}

String TCPSocket::errorMessage() const {
	return d->errorMessage ();
}

Channel::State TCPSocket::state () const {
	return d->state ();
}

Error TCPSocket::write (const ByteArrayPtr& data, const ResultCallback & callback) {
	return d->write (data, callback);
}

ByteArrayPtr TCPSocket::read(long maxSize){
	return d->read (maxSize);
}

void TCPSocket::close (const ResultCallback & callback) {
	return d->close (callback);
}

Channel::ChannelInfo TCPSocket::info () const {
	return d->info();
}

sf::VoidDelegate & TCPSocket::changed () {
	return d->changed();
}

bool TCPSocket::atEnd() const {
	return d->atEnd();
}

ByteArrayPtr TCPSocket::peek (long maxSize) {
	return d->peek(maxSize);
}


long TCPSocket::bytesAvailable () const {
	return d->bytesAvailable ();
}

VoidDelegate& TCPSocket::readyRead () {
	return d->readyRead();
}

VoidDelegate& TCPSocket::disconnected () {
	return d->mDisconnectedDelegate;
}

}
