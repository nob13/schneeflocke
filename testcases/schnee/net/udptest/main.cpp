#include <schnee/schnee.h>
#include <schnee/test/test.h>
#include <schnee/net/UDPSocket.h>
#include <schnee/tools/ResultCallbackHelper.h>
/**
 * @file Tests UDPSocket class
 */
using namespace sf;

int testSimpleSendAndReceive () {
	ResultCallbackHelper helperA;
	UDPSocket a;
	a.readyRead() = helperA.onReadyFunc();
	Error e = a.bind(1200);
	tassert (!e, "should find some port");
	tassert1 (a.port() > 0);

	ResultCallbackHelper helperB;
	UDPSocket b;
	b.readyRead() = helperB.onReadyFunc();
	e = b.bind(1201);
	tassert (!e, "should bind");
	tassert (b.port() > 0, "should find a port");

	// at least on localhost, UDP packets shall be reliable
	e = a.sendTo ("127.0.0.1", b.port(), sf::createByteArrayPtr("Hello"));
	tcheck (!e, "shall send");
	e = a.sendTo ("127.0.0.1", b.port(), sf::createByteArrayPtr("World"));
	tcheck (!e, "shall send");
	bool suc = helperB.waitUntilReady(2000);
	tcheck (suc, "b must receive the messages");
	String sender;
	int senderPort;
	ByteArrayPtr content = b.recvFrom(&sender, &senderPort);
	tassert1 (content && (*content == ByteArray("Hello") || *content == ByteArray ("World")));
	tassert1 (senderPort == a.port());

	suc = b.datagramsAvailable() > 0 || helperB.wait(2000);
	tcheck (suc, "b must receive both messages");
	ByteArrayPtr content2 = b.recvFrom (&sender, &senderPort);
	tcheck (content2 && (*content2 == ByteArray ("Hello") || *content2 == ByteArray ("World")), "data mismatch");
	tcheck (senderPort == a.port(), "wrong sender pot");
	tcheck (!(*content == *content2), "wrong content");
	return 0;
}

/// UDP Socket lacks DNS (and will so, see its documentation)
int testFailingDns () {
	UDPSocket a;
	Error e = a.bind (1202);
	tcheck (!e, "should bind");
	e = a.sendTo ("localhost", 4567, sf::createByteArrayPtr("Hello")); // DNS needed for localhost
	tcheck (e == error::InvalidArgument, "UDPSocket should not support DNS");
	return 0;
}

/// Binds to a random port
int freeBind () {
	UDPSocket a;
	Error e = a.bind(0); // search yourself a free port
	tcheck (!e, "should bind");
	printf ("A bound to: %d\n", a.port());
	return 0;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	int ret = 0;
	testcase (testSimpleSendAndReceive());
	testcase (testFailingDns ());
	testcase (freeBind());
	return ret;
}
