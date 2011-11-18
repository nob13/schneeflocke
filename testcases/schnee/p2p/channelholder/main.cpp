#include <schnee/test/test.h>
#include <schnee/test/initHelpers.h>
#include <schnee/test/LocalChannel.h>
#include <schnee/tools/Log.h>
#include <schnee/p2p/impl/ChannelHolder.h>
#include "MyMsg.h"
/*
 * @file
 * Test fundamental operations of the ChannelHolder.
 */
using namespace sf;

bool hasChannel (const ChannelHolder * holder, const HostId & target) {
	return holder->findBestChannel(target) > 0;
}

bool hasNoChannel (const ChannelHolder * holder, const HostId & target) {
	return holder->findBestChannel(target) == 0;
}

// Helper for waiting for datagrams
struct IncomingDatagramHelper : public DelegateBase {
	struct Package {
		Package () {}
		Package (const HostId & source, const String & cmd, const Deserialization & ds, const ByteArray & data) {
			this->source = source;
			this->cmd = cmd;
			this->ds = ds;
			this->data = data;
		}
		HostId source;
		String cmd;
		Deserialization ds;
		ByteArray data;

	};

	IncomingDatagramHelper () {
		SF_REGISTER_ME;
	}
	~IncomingDatagramHelper () {
		SF_UNREGISTER_ME;
	}

	/// connect this as incoming event
	void onIncoming (const HostId & source, const String & cmd, const Deserialization & ds, const ByteArray & data) {
		Package p (source, cmd, ds, data);
		inbox.push_back (p);
	}

	/// wait until there is data
	bool waitForData (int timeoutMs) {
		return test::waitUntilTrueMs(bind (&IncomingDatagramHelper::hasData, this), timeoutMs);
	}

	Package fetch () {
		Package p = inbox.back();
		inbox.pop_back();
		return p;
	}

	bool hasData () {
		return !inbox.empty();
	}

	std::vector<Package> inbox;
};

/// A simple test scenario with two peers A and B
struct Scenario {
	ChannelHolder holder1;
	ChannelHolder holder2;
	Scenario () {
		holder1.setHostId ("A");
		holder2.setHostId ("B");
	}

	/// there is connection between these two
	bool connectionsAvailable (int minLevel = 1) const {
		if (holder1.findBestChannel("B") == 0) return false;
		if (holder2.findBestChannel("A") == 0) return false;
		if (holder1.findBestChannel("B") < minLevel) return false;
		if (holder2.findBestChannel("A") < minLevel) return false;
		return true;
	}

	/// Add two local channels
	/// And give them to you
	void addChannels (test::LocalChannelPtr * outA, test::LocalChannelPtr * outB, int level = 10) {
		test::LocalChannelPtr a (new test::LocalChannel()), b (new test::LocalChannel());
		test::LocalChannel::bindChannels(*a, *b);
		*outA = a;
		*outB = b;
		holder1.add(a, "B", true, level);
		holder2.add(b, "A", false, level);
	}

	bool connectivityTest (int timeoutMs) {
		AsyncOpId id = holder1.findBestChannel("B");
		MyMsg msg;
		Error e = holder1.send(id, Datagram::fromCmd(msg), true);
		if (e) return false;

		IncomingDatagramHelper helper;
		holder2.incomingDatagram() = dMemFun (&helper, &IncomingDatagramHelper::onIncoming);

		bool suc = helper.waitForData(timeoutMs);
		if (!suc) return false;

		IncomingDatagramHelper::Package p = helper.fetch();
		if (p.cmd != MyMsg::getCmdName()){
			Log (LogError) << LOGID << "Wrong command name" << std::endl;
			return false;
		}
		MyMsg des;
		suc = des.deserialize(p.ds);
		if (!suc) {
			Log (LogError) << LOGID << "Deserialization error" << std::endl;
			return false;
		}

		if (des.code != msg.code) {
			Log (LogError) << LOGID << "Deserialization error" << std::endl;
		}
		return true;
	}

	void dump () {
		Log (LogInfo) << LOGID << "Connections of A " << toJSONEx (holder1.connections(), INDENT) << std::endl;
		Log (LogInfo) << LOGID << "Connections of B " << toJSONEx (holder2.connections(), INDENT) << std::endl;
	}
};

int testChannelAddAndClose () {
	Scenario scenario;
	test::LocalChannelPtr a,b;
	scenario.addChannels(&a, &b);
	tcheck1 (scenario.connectionsAvailable());

	// checking connectivity
	tcheck1 (scenario.connectivityTest(10000));
	// closing non-existant
	Error e = scenario.holder1.close (123456); // they are counted linear
	tcheck1 (e == error::NotFound);

	// ok, closing one...
	scenario.holder1.close (scenario.holder1.findBestChannel ("B"));

	bool suc = test::waitUntilTrueMs (bind (&hasNoChannel, &scenario.holder1, HostId ("B")), 1000);
	tcheck (suc, "Must close channel");

	suc = test::waitUntilTrueMs (bind (&hasNoChannel, &scenario.holder2, HostId ("A")), 1000);
	tcheck (suc, "Must close channel");

	// rebuild it, shall work without a problem
	scenario.addChannels(&a, &b);
	tcheck1 (scenario.connectionsAvailable());

	return 0;
}

int testChannelTimeout () {
	Scenario scenario;
	test::LocalChannelPtr a,b;
	scenario.addChannels (&a,&b);

	tcheck1 (scenario.connectivityTest(1000));
	scenario.holder1.setChannelTimeout(100);
	scenario.holder1.setChannelTimeoutCheckInterval(100);

	test::millisleep(500);
	// must be timeouted
	tcheck1 (!scenario.connectivityTest(500));
	tcheck1 (!scenario.connectionsAvailable());
	return 0;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);

	int ret = 0;
	testcase (testChannelAddAndClose());
	testcase (testChannelTimeout());

	return 0;
}
