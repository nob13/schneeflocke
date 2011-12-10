#include <schnee/schnee.h>
#include <schnee/test/test.h>
#include <schnee/test/initHelpers.h>
#include <schnee/tools/async/MemFun.h>
#include <schnee/net/UDPEchoClient.h>
using namespace sf;

class TestHelper {
public:
	TestHelper () { mHasResult = false; }

	void callback (Error e) {
		mResult     = e;
		mHasResult  = true;
	}

	bool hasResult () {
		return mHasResult;
	}

	Error result () { return mResult; }

	bool waitForResult (int timeOutMs) {
		return test::waitUntilTrueMs(memFun(this, &TestHelper::hasResult), timeOutMs);
	}

private:

	Error mResult;
	bool  mHasResult;


};

/// Tries to get UDP address on non existing server, must time out
int testNonExistingServer () {
	TestHelper helper;
	UDPSocket socket;
	socket.bind();

	UDPEchoClient client(socket);
	client.result() = memFun (&helper, &TestHelper::callback);
	client.start ("127.0.0.1", 80, 1000); // Note: Port 80 won't respond
	bool hasResult = helper.waitForResult (2000);
	tcheck (hasResult, "Must callback for timeout");
	tcheck (helper.result() == error::TimeOut, "Must timeout");
	return 0;
}

// Test Existing server. Note: Must be started on port 1234
int testExistingServer () {
	TestHelper helper;
	UDPSocket socket;
	socket.bind();

	UDPEchoClient client(socket);
	client.result() = memFun (&helper, &TestHelper::callback);
	client.start ("127.0.0.1", 1234, 1000);
	bool hasResult = helper.waitForResult (2000);
	tassert (hasResult, "Must callback");
	if (helper.result() != NoError){
		fprintf (stderr, "Warning: Localhost echo server seems not to run!");
		return 0; // do not interpret as an error
	}
	tcheck1 (client.port()    == socket.port());
	tcheck1 (client.address() == "127.0.0.1");
	return 0;
}

int testSflxEchoServer () {
	TestHelper helper;
	UDPSocket socket;
	socket.bind();

	UDPEchoClient client(socket);
	client.result() = memFun (&helper, &TestHelper::callback);
	client.start ("62.48.92.13", 1234, 1000); // sflx.net
	bool hasResult = helper.waitForResult (2000);
	tassert (hasResult, "Must callback");
	if (helper.result() != NoError){
		fprintf (stderr, "Shall give no error, but gave: %s\n", toString (helper.result()));
		fprintf (stderr, "Maybe you don't have a udp access to the net?\n");
		fprintf (stderr, "Do not count this as an error, as UDP is not reliable");
		return 1;
	}
	printf ("Outside address: %s, outside port: %d\n", client.address().c_str(), client.port());
	return 0;
}

int main (int argc, char * argv[]){
	schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	testcase_start();
	testcase (testNonExistingServer());
	testcase (testExistingServer());
	testcase (testSflxEchoServer());
	testcase_end();
}
