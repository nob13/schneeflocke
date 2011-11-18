#include <schnee/net/TCPSocket.h>
#include <schnee/schnee.h>
#include <schnee/tools/Log.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/test/timing.h>
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
	mySocket.connectToHost ("localhost", 987, 10000);
	mySocket.waitForConnected();
	mySocket.readyRead() = sf::bind (newData, &mySocket);
	if (mySocket.isConnected()){
		std::cout << "We are connected, bad" << std::endl;
		return 1;
	} else {
		std::cout << "Problem connecting, good" << std::endl;
	}
	return 0;
}

struct WriteCallbackHandler : public sf::DelegateBase {
	WriteCallbackHandler () : gotCallback (false), result (sf::NoError) {
		SF_REGISTER_ME;
	}
	~WriteCallbackHandler () {
		SF_UNREGISTER_ME;
	}
	void onReady (sf::Error r) {
		gotCallback = true;
		result = r;
	}
	bool gotCallback;
	sf::Error result;
};

int acceptingTest (){
	sf::TCPSocket mySocket;
	std::cout << "Connecting..." << std::endl;
	mySocket.connectToHost ("sflx.net", 80, 10000);
	mySocket.waitForConnected();
	mySocket.readyRead() = sf::bind (newData, &mySocket);
	if (mySocket.isConnected()){
		std::cout << "We are connected, good" << std::endl;
	} else {
		std::cout << "Problem connecting, bad" << std::endl;
		return 1;
	}
	WriteCallbackHandler handler;
	sf::ByteArrayPtr data = sf::createByteArrayPtr("<?xml version='1.0' encoding='utf-8'?><stream:stream xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' to='localhost' version='1.0'>");
	mySocket.write (data, sf::dMemFun (&handler, &WriteCallbackHandler::onReady));
	mySocket.waitForAsyncWrite();
	if (!handler.gotCallback || handler.result){
		std::cout << "No callback or wrong code during sending!" << std::endl;
		return 1;
	}
	sf::test::sleep (1);
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
	sf::test::sleep (1); // should not crash
	return 0;
}

int deleteBehaviour2 () {
	sf::TCPSocket * mySocket = new sf::TCPSocket ();;
	mySocket->connectToHost ("localhost", 5222, 10000, sf::bind (deleteOnFail, _1, mySocket));
	mySocket->readyRead() = sf::bind (deleteHandler, mySocket);
	bool notFail = mySocket->waitForConnected();
	if (notFail == false){
		std::cerr << LOGID << "Socket should connect" << std::endl;
		return 1;
	}
	mySocket->write (sf::createByteArrayPtr("<?xml version='1.0' encoding='utf-8'?><stream:stream xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' to='localhost' version='1.0'>"));
	mySocket->waitForAsyncWrite();
	return 0;
}

int deleteBehaviour3 () {
	sf::TCPSocket * mySocket = new sf::TCPSocket ();;
	mySocket->connectToHost ("sflx.net", 80, 10000, sf::bind (deleteOnFail, _1, mySocket));
	mySocket->readyRead() = sf::bind (deleteHandler, mySocket);
	mySocket->disconnected() = sf::bind (deleteHandler, mySocket);
	bool notFail = mySocket->waitForConnected();
	if (notFail == false){
		std::cerr << LOGID << "Socket should connect" << std::endl;
		return 1;
	}
	mySocket->write (sf::createByteArrayPtr("HiDude")); // peer will close it....
	mySocket->waitForAsyncWrite();
	return 0;
}


int main (int argc, char* argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	
	int result = 0;
	result += denyingTest ();
	result += acceptingTest ();
	result += deleteBehaviour1();
	result += deleteBehaviour2();
	result += deleteBehaviour3();
	sf::test::sleep (1);
	return 0;
}
