#include "TCPServer.h"

#include "impl/TCPServerPrivate.h"

namespace sf {

TCPServer::TCPServer (){
	d = new TCPServerPrivate ();
}

TCPServer::~TCPServer (){
	d->requestDelete();
}

void TCPServer::close (){
	d->close();
}

bool TCPServer::listen (int port){
	return (d->listen (port));
}

bool TCPServer::isListening () const {
	return d->isListening();
}

int TCPServer::serverPort () const {
	return d->serverPort ();
}

String TCPServer::serverAddress () const {
	return d->serverAddress ();
}

void TCPServer::setMaxPendingConnections (int num){
	d->mMaxPendingConnections = num;
}

int TCPServer::maxPendingConnections () const{
	return d->mMaxPendingConnections;
}

shared_ptr<TCPSocket> TCPServer::nextPendingConnection (){
	return d->nextPendingConnection ();
}

bool TCPServer::hasPendingConnections() const{
	return d->hasPendingConnections ();
}

VoidDelegate & TCPServer::newConnection (){
	return d->mNewConnectionDelegate;
}


}
