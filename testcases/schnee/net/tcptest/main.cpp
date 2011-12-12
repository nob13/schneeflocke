#include <schnee/net/TCPSocket.h>
#include <schnee/schnee.h>
#include <schnee/test/test.h>
#include <schnee/tools/Log.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/test/timing.h>
#include <schnee/tools/ResultCallbackHelper.h>
#include <iostream>

/*
 * Tests some properties of the TCPSocket.
 * This is a manual test, as it needs some constraints like not having 987 running at localhost.
 */

void newData (sf::TCPSocket * mySocket){
	sf::ByteArrayPtr data = mySocket->read ();
	std::cout << "(recv)";
	for (sf::ByteArray::const_iterator i = data->begin(); i!= data->end(); i++){
		char c = *i;
		if (c == 0) break;
		std::cout << c;
	}
	std::cout << std::endl;
}

/// Port 987 doesn't exist, should fail
int denyingTest (){
	sf::TCPSocket mySocket;
	std::cout << "Connecting..." << std::endl;
	sf::ResultCallbackHelper helper;
	mySocket.connectToHost ("localhost", 987, 10000, helper.onResultFunc());
	helper.wait();
	mySocket.readyRead() = sf::bind (newData, &mySocket);
	if (mySocket.isConnected()){
		std::cout << "We are connected, bad" << std::endl;
		return 1;
	} else {
		std::cout << "Problem connecting, good" << std::endl;
	}
	return 0;
}

int acceptingTest (){
	sf::TCPSocket mySocket;
	std::cout << "Connecting..." << std::endl;
	sf::ResultCallbackHelper helper;
	mySocket.connectToHost ("sflx.net", 80, 10000, helper.onResultFunc());
	helper.wait();
	mySocket.readyRead() = sf::bind (newData, &mySocket);
	if (mySocket.isConnected()){
		std::cout << "We are connected, good" << std::endl;
	} else {
		std::cout << "Problem connecting, bad" << std::endl;
		return 1;
	}
	sf::ByteArrayPtr data = sf::createByteArrayPtr("<?xml version='1.0' encoding='utf-8'?><stream:stream xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' to='localhost' version='1.0'>");
	mySocket.write (data, helper.onResultFunc());
	sf::Error e = helper.wait();
	if (e){
		std::cout << "No callback or wrong code during sending!" << std::endl;
		return 1;
	}
	return 0;
}

void deleteHandler (sf::TCPSocket * mySocket){
	// first read it all (to eventually start a new reading process)
	sf::ByteArrayPtr all = mySocket->read ();
	std::cout << "Will now delete socket" << std::endl;
	delete mySocket;
}

void deleteOnFail (sf::Error e, sf::TCPSocket * mySocket) {
	if (e) {
		deleteHandler (mySocket);
	}
}

// Deleting a socket in its connection handler
int deleteBehaviour1 () {
	sf::TCPSocket * mySocket = new sf::TCPSocket ();;
	mySocket->connectToHost ("you'don't now address", 5298, 10000, sf::bind (deleteHandler, mySocket));
	sf::test::sleep_locked (1); // should not crash
	return 0;
}

int deleteBehaviour2 () {
	sf::TCPSocket * mySocket = new sf::TCPSocket ();;
	mySocket->connectToHost ("localhost", 5222, 10000, sf::bind (deleteOnFail, _1, mySocket));
	mySocket->readyRead() = sf::bind (deleteHandler, mySocket);
	sf::test::sleep_locked(1);
	mySocket->write (sf::createByteArrayPtr("<?xml version='1.0' encoding='utf-8'?><stream:stream xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' to='localhost' version='1.0'>"));
	sf::test::sleep_locked (1);
	return 0;
}

int deleteBehaviour3 () {
	sf::TCPSocket * mySocket = new sf::TCPSocket ();;
	mySocket->connectToHost ("sflx.net", 80, 10000, sf::bind (deleteOnFail, _1, mySocket));
	mySocket->readyRead() = sf::bind (deleteHandler, mySocket);
	mySocket->disconnected() = sf::bind (deleteHandler, mySocket);
	sf::test::sleep_locked (1);
	if (!mySocket->isConnected()){
		std::cerr << LOGID << "Socket should connect" << std::endl;
		return 1;
	}
	mySocket->write (sf::createByteArrayPtr("HiDude")); // peer will close it....
	sf::test::sleep_locked(1);
	return 0;
}


int main (int argc, char* argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	testcase_start();
	testcase (denyingTest ());
	testcase (acceptingTest ());
	testcase (deleteBehaviour1());
	testcase (deleteBehaviour2());
	testcase (deleteBehaviour3());
	testcase_end();
	return 0;
}
