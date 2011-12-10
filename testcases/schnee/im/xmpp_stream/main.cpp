#include <schnee/test/test.h>
#include <schnee/test/LocalChannel.h>
#include <schnee/tools/ResultCallbackHelper.h>

#include <schnee/im/xmpp/XMPPStream.h>
using namespace sf;

struct Scenario {
	Scenario () {
		channel1 = test::LocalChannelPtr (new test::LocalChannel);
		channel2 = test::LocalChannelPtr (new test::LocalChannel);
		test::LocalChannel::bindChannels (*channel1, *channel2);
	}

	Error connect () {
		ResultCallbackHelper initHelper1;
		ResultCallbackHelper initHelper2;

		Error e = stream1.startInit(channel1, initHelper1.onResultFunc());
		if (e) return error::ConnectionError;

		e = stream2.respondInit(channel2, initHelper2.onResultFunc());
		if (e) return error::ConnectionError;

		bool suc = (initHelper1.waitUntilReady(100));
		if (!suc) return error::ConnectionError;
		suc = initHelper2.waitUntilReady(100);
		if (!suc) return error::ConnectionError;
		if (initHelper1.result()) return error::ConnectionError;
		if (initHelper2.result()) return error::ConnectionError;
		return NoError;
	}

	test::LocalChannelPtr channel1;
	test::LocalChannelPtr channel2;
	XMPPStream stream1;
	XMPPStream stream2;
};

int crossConnectivity () {
	Scenario scenario;
	scenario.stream1.setInfo("A");
	scenario.stream2.setInfo("B");
	Error e = scenario.connect();
	tcheck1(!e);

	ResultCallbackHelper closeHandler;
	scenario.stream2.closed () = closeHandler.onReadyFunc();
	scenario.stream1.close();

	tcheck (closeHandler.waitUntilReady(100), "Should get a close message");
	return 0;
}

int channelError () {
	Scenario scenario;
	Error e = scenario.connect();
	tcheck1(!e);

	ResultCallbackHelper channelErrorHelper;
	scenario.stream2.asyncError() = channelErrorHelper.onReadyFunc();
	scenario.channel1->close();
	tcheck (channelErrorHelper.waitUntilReady(100), "Should get a channel error message");
	return 0;
}

// A full connection testcase can be found in xmpp_connection
int main (int argc, char * argv[]){
	schnee::SchneeApp app (argc, argv);
	int ret = 0;
	testcase (crossConnectivity());
	testcase (channelError());
	return ret;
}
