#include "UDPSocket.h"
#include "impl/UDPSocketPrivate.h"
namespace sf {

UDPSocket::UDPSocket () {
	d = new UDPSocketPrivate ();
}

UDPSocket::~UDPSocket() {
	if (d)
		d->requestDelete();
}

int UDPSocket::port () const {
	return d->port ();
}


Error UDPSocket::bind (int port, const String & address) {
	return d->bind(port, address);
}

Error UDPSocket::close () {
	return d->close();
}

Error UDPSocket::sendTo (const String & receiver, int receiverPort, const ByteArrayPtr & data) {
	return d->sendTo(receiver, receiverPort, data);
}

ByteArrayPtr UDPSocket::recvFrom (String * from, int * fromPort) {
	return d->recvFrom(from, fromPort);
}

bool UDPSocket::waitForReadyRead (int  timeOutMs) {
	return d->waitForReadyRead (timeOutMs);
}


int UDPSocket::datagramsAvailable () {
	return d->datagramsAvailable();
}

Error UDPSocket::error () const {
	return d->error();
}

VoidDelegate & UDPSocket::readyRead () {
	return d->readyRead();
}

VoidDelegate & UDPSocket::errored () {
	return d->errored();
}


}
