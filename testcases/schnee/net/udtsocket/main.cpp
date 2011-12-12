#include <schnee/sftypes.h>
#include <schnee/schnee.h>
#include <schnee/tools/Singleton.h>
#include <schnee/test/timing.h>
#include <schnee/test/test.h>
#include <schnee/test/PseudoRandom.h>
#include <schnee/tools/ResultCallbackHelper.h>
#include <schnee/test/initHelpers.h>

#include <schnee/tools/async/MemFun.h>
#include <schnee/tools/MicroTime.h>
#include <schnee/tools/Serialization.h>
#include <schnee/net/UDPSocket.h>

#include <schnee/net/impl/UDTMainLoop.h>
#include <schnee/net/UDTSocket.h>
#include <schnee/net/UDTServer.h>

using namespace sf;

// test automatic running/stopping of UDT main loop
int testUDTMainLoopRunning () {
	{
		UDTSocket a;
		test::millisleep_locked (100);
		tassert (UDTMainLoop::instance().isRunning(), "should run");
	}
	tassert (!UDTMainLoop::instance().isRunning(), "should not run");
	{
		UDTSocket a;
		UDTSocket b;
		test::millisleep_locked (100);
		tassert (UDTMainLoop::instance().isRunning(), "should run");
		UDTServer s;
	}
	tassert (!UDTMainLoop::instance().isRunning(), "should not run");
	return 0;
}


// check predicate if there is $amount data pending on a socket
bool hasData (const UDTSocket * socket, int amount) {
	return socket->bytesAvailable() >= amount;
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

// Test connectivity between two sockets (socket1 --> socket2)
int testConnectivity (UDTSocket * socket1, UDTSocket * socket2) {
	// Socket --> Socket2
	WriteCallbackHandler callbackTest1, callbackTest2;
	socket1->write (sf::createByteArrayPtr ("Hello"), dMemFun (&callbackTest1, &WriteCallbackHandler::onReady)); // 5 characters
	socket1->write (sf::createByteArrayPtr ("World"), dMemFun (&callbackTest2, &WriteCallbackHandler::onReady)); // 5 characters

	bool suc = test::waitUntilTrueMs (sf::bind (hasData, socket2, 10), 100);
	if (!suc) {
		fprintf (stderr, "Socket %p did not get any data!", &socket2);
		tassert1 (!false);
	}
	tassert1 (callbackTest1.gotCallback && !callbackTest1.result);
	tassert1 (callbackTest2.gotCallback && !callbackTest2.result);
	ByteArrayPtr data = socket2->read();
	tassert (data && *data == ByteArray ("HelloWorld"), "Data must be correct");
	return 0;
}

// Test >50MB Traffic between two sockets, at least filling 5 mb into the buffer
int testHeavyTraffic (UDTSocket * socket1, UDTSocket * socket2){
	// test data size
	const size_t amount = 1024 * 5500; // > 5mb
	const size_t halfAmount = amount / 2;
	const int rounds = 5;
	tassert (halfAmount * 2 == amount, "bad testcase");

	// creating test data
	static bool initialized = false;
	static ByteArray data (amount, 0); // > 5mb
	if (!initialized){
		printf ("Creating %d bytes test data\n", (int) amount);
		test::pseudoRandomData(amount,data.c_array());
		initialized = true;
	}

	double t0 = sf::microtime();
	for (int i = 0; i < rounds; i++) {
		printf ("TestHeavyTraffic - Round: %d\n", i);
		double ts0 = sf::microtime ();

		// sending
		socket1->write (sf::createByteArrayPtr(data));

		// first half
		tassert (test::waitUntilTrueMs (sf::bind(hasData, socket2, halfAmount), 10000), "Should get data");
		ByteArrayPtr part1 = socket2->read(halfAmount);

		// second half
		tassert (test::waitUntilTrueMs (sf::bind(hasData, socket2, halfAmount), 10000), "Should get data");
		ByteArrayPtr part2 = socket2->read(halfAmount);

		// checking correctness
		tassert (part1 && part2, "Shall receive all data");
		ByteArray combined;
		combined = *part1;
		combined.append(*part2);
		tassert (combined == data, "Shall receive the same as send");

		double ts1 = sf::microtime ();
		double rate = amount / (ts1 - ts0) / 1024 / 1024; // MiB/s
		printf ("   Round  rate: %f MiB/s\n", rate);
	}
	double t1 = sf::microtime ();
	size_t totalTraffic = rounds * amount;
	double rate = totalTraffic / (t1 - t0) / 1024 / 1024; // MiB/s
	printf ("TestHeavyTraffic: Rate: %f MiB/s\n", rate);

	return 0;
}

// Do some stress test with these sockets
int testSockets (UDTSocket * socket1, UDTSocket * socket2) {
	printf ("Testing socket connectivity between\n  %s and \n  %s\n",
			toJSONEx (socket1->info(), sf::COMPRESS | sf::COMPACT).c_str(),
			toJSONEx (socket2->info(), sf::COMPRESS | sf::COMPACT).c_str());
	testcase_start();
	testcase (testConnectivity (socket1, socket2));
	testcase (testConnectivity (socket2, socket1));

	testcase (testHeavyTraffic (socket1, socket2));
	testcase (testHeavyTraffic (socket2, socket1));
	testcase_end();
}

/// creates an UDT server and connects using a socket to it
int testUDTServer () {
	UDTServer server;
	Error e = server.listen (0);
	tassert (!e, "Should find a port");
	int port = server.port ();
	printf ("Listening on %d\n", port);

	UDTSocket socket;
	tassert (socket.connect ("localhost", port) == error::InvalidArgument, "Should not accept DNS names");
	e = socket.connect ("127.0.0.1", port); // localhost won't work!
	tassert (!e, "Should connect");

	test::millisleep_locked (100);
	tassert (server.pendingConnections() == 1, "Should have one pending connection");

	UDTSocket * socket2 = server.nextConnection();
	tassert (socket2->error() == NoError, "Incoming socket shall be ok");
	tassert (socket.error() == NoError, "Outgoing socket shall be ok");

	tassert (testSockets (&socket, socket2) == 0, "Sockets shall support this test");

	socket2->close();
	test::millisleep_locked (100);
	tassert1 (!socket.isConnected());
	delete socket2;

	return 0;
}

/// Test UDT Rendezvous mode
int testRendezvous () {
	UDTSocket a;
	UDTSocket b;
	Error e = a.bind();
	tassert (!e, "socket shall bind");
	e = b.bind();
	tassert (!e, "socket shall bind");
	printf ("A bound to %d, B bound to %d\n", a.port(), b.port());
	xcall (abind (memFun (&b, &UDTSocket::connectRendezvous), String ("127.0.0.1"), a.port())); // b connecting in IOService thread back
	schnee::mutex().unlock();
	e = a.connectRendezvous("127.0.0.1", b.port());
	schnee::mutex().lock();
	tassert (!e, "Shall connect");

	test::millisleep_locked (100); // giving IOService thread enough time to connectRendezvous
	tassert (testSockets (&a, &b) == 0, "Sockets shall support this test");
	return 0;
}

typedef ResultCallbackHelper AsyncConnectHelper;

/// Test async rendezvous
int testAsyncRendezvous () {
	UDTSocket a;
	UDTSocket b;
	Error e = a.bind();
	tassert1 (!e);
	e = b.bind();
	tassert1 (!e);
	AsyncConnectHelper helperA;
	AsyncConnectHelper helperB;
	a.connectRendezvousAsync("127.0.0.1", b.port(), helperA.onResultFunc());
	b.connectRendezvousAsync("127.0.0.1", a.port(), helperB.onResultFunc());
	bool suc = helperA.waitUntilReady (2000);
	tassert (suc, "Must connect");
	suc = helperB.waitUntilReady(2000);
	tassert (suc, "Must connect");
	tassert1 (testConnectivity (&a, &b) == 0);
	return 0;
}

// Test reusing using UDT Sockets as a replacement of UDP sockets
int testReusage () {
	// Init two UDP Sockets
	UDPSocket base1;
	UDPSocket base2;
	Error e = base1.bind();
	tcheck (!e, "Shall bind");
	e = base2.bind();
	tcheck (!e, "Shall bind");

	int aPort = base1.port ();
	int bPort = base2.port ();



	// Sending some data between them
	e = base1.sendTo ("127.0.0.1", bPort, sf::createByteArrayPtr("Hello World"));
	tcheck (!e, "Shall send");

	e = base2.sendTo ("127.0.0.1", aPort, sf::createByteArrayPtr ("You too!"));
	tcheck (!e, "Shall send");

	tassert1 (test::waitUntilTrueMs(sf::bind (&UDPSocket::datagramsAvailable, base1), 2000));
	tassert1 (test::waitUntilTrueMs(sf::bind (&UDPSocket::datagramsAvailable, base2), 2000));

	// Checking data
	String from;
	int port = -1;
	ByteArrayPtr data;
	data = base2.recvFrom (&from, &port);
	tcheck (data && *data == ByteArray ("Hello World"), "Received wrong data");
	data = base1.recvFrom (&from, &port);
	tcheck (data && *data == ByteArray ("You too!"), "Received wrong data");

	// Converting to UDT Sockets.
	UDTSocket a;
	UDTSocket b;
	e = a.rebind(&base1);
	tcheck (!e, "Shall have no problem to rebind");
	e = b.rebind (&base2);
	tcheck (!e, "Shall have no problem to rebind");

	tassert1 (a.port() == aPort);
	tassert1 (b.port() == bPort);

	printf ("Ports now A: %d B: %d\n", a.port(), b.port());

	// Let's connect them...
	xcall (abind (memFun (&b, &UDTSocket::connectRendezvous), String ("127.0.0.1"), a.port()));
	e = a.connectRendezvous("127.0.0.1", b.port());
	tcheck (!e, "Shall be no problem to connect now");

	test::millisleep_locked (100); // giving IOService thread enough time to connectRendezvous

	tassert (testSockets (&a, &b) == 0, "supports shall work");
	return 0;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	// Sync rendezvous doesn't work anymore since libschnee
	// is globally locked.
	testcase_start();
	testcase (testUDTMainLoopRunning());
 	testcase (testUDTServer());
	// testcase (testRendezvous());
	testcase (testAsyncRendezvous());
	// testcase (testReusage());
	testcase_end();
}
