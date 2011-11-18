#include <schnee/schnee.h>
#include <schnee/net/TCPServer.h>
#include <schnee/tools/Log.h>

#include <boost/thread.hpp>
#include <vector>

/*
 * Tests the stability of an echoing TCPServer
 * Connects hundreds of threads which sends something and check whether the server repeats it.
 *
 * Note: this testcase causes problems in windows which seems not to support so many threads.
 */

std::vector<boost::thread*> threads;
int serverPort;

void connectThread (){
	sf::Log (LogInfo) << LOGID << "New connection Thread" << std::endl;
	sf::TCPSocket sock;
	sock.connectToHost("localhost", serverPort);
	bool result = sock.waitForConnected();
	assert (result);
	sf::Time start = sf::currentTime ();
	sf::ByteArray data = "HelloWorld quite a lot of data";
	int comb = 10000;
	int dataCount = comb * data.size();
	for (int i = 0; i < comb; i++){
		sock.write(sf::createByteArrayPtr(data));
		sock.waitForAsyncWrite();
	}
	int recv = 0;
	while (recv < dataCount){
		bool ret = sock.waitForReadyRead();
		if (!ret){
			assert (!sock.error());
			ret = sock.waitForReadyRead (-1); // no timeOut
			if (!ret) std::cerr << LOGID << "Err: " << sock.errorMessage () << std::endl;
			assert (ret);
		}
		sf::ByteArrayPtr rdata = sock.read ();
		recv+= rdata->size();
	}
	assert (recv == dataCount);
	sf::Time end = sf::currentTime();
	std::cout << LOGID << "Sending and receiving of " << dataCount << " took " << (end - start).total_milliseconds() << "ms" << std::endl;
	sock.close ();
}

void answerThread (sf::shared_ptr<sf::TCPSocket> sock){
	std::cout << LOGID << "New Answer Thread " << std::endl;
	while (!sock->atEnd ()){
		bool ret = sock->waitForReadyRead ();
		if (ret){
			sf::ByteArrayPtr data = sock->read ();
			sock->write (data);
			sock->waitForAsyncWrite();
		}
	}
}

void onNewConnection (sf::TCPServer * server){
	sf::shared_ptr<sf::TCPSocket> sock = server->nextPendingConnection();
	boost::thread thread ((sf::bind (answerThread, sock)));
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	
	sf::TCPServer server;
	
	server.listen ();
	
	if (!server.isListening()){
		std::cerr << "Could not listen" << std::endl;
		return 1;
	}
	serverPort = server.serverPort();
	
	std::cout << "Listening on port " << server.serverPort () << std::endl;
	std::cout << "Will send back everything we get" << std::endl;
	
	server.newConnection() = sf::bind (onNewConnection, &server);
	
	const int numThreads = 100;
	
	for (int i = 0; i < numThreads; i++){
		boost::thread * thread = new boost::thread (connectThread);
		threads.push_back (thread);
	}
	
	
	
	// waiting for them all..
	for (std::vector<boost::thread*>::iterator i = threads.begin(); i != threads.end(); i++){
		(*i)->join();
	}
	
	return 0;
	
}
