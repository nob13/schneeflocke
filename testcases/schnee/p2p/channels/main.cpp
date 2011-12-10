#include <schnee/schnee.h>
#include <schnee/test/LocalChannel.h>
#include <schnee/p2p/channels/TCPChannelConnector.h>
#include <schnee/p2p/channels/IMDispatcher.h>
#include <schnee/p2p/Datagram.h>
#include <schnee/p2p/DatagramReader.h>

#include <schnee/p2p/com/PingProtocol.h>
#include <schnee/p2p/CommunicationMultiplex.h>


#include <schnee/net/Tools.h>
#include <schnee/Error.h>
#include <schnee/tools/Log.h>
#include <schnee/tools/Deserialization.h>
#include <schnee/tools/Serialization.h>
#include <schnee/tools/async/MemFun.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/test/test.h>
#include <schnee/tools/ResultCallbackHelper.h>

#include <boost/weak_ptr.hpp>
using namespace sf;

typedef shared_ptr<TCPSocket> TCPSocketPtr;
typedef shared_ptr<test::LocalChannel> LocalChannelPtr;
typedef boost::weak_ptr<Channel> ChannelWPtr;

#ifdef WIN32
#include <Windows.h>
void sleep (int seconds) { Sleep (seconds * 1000); }
#else 
#include <unistd.h>
#endif

/*
 * Test the creation of TCP Channels with by using indirect channels (simulated, XMPP). Note that this testcase was created
 * in a early stage of the library and may use a slightly different style.
 */


/// One peer in the 1:1 sczenario
struct Peer : public CommunicationDelegate, CommunicationMultiplex, DelegateBase {
	Peer (ChannelPtr  channel, const String & id, const String & otherId) {
		SF_REGISTER_ME;
		setCommunicationDelegate (this);
		mId = id;
		mOtherId = otherId;
		
		mSendPing     = 0;
		mReceivedPong = 0;
		
		bool gotOwnAddresses = net::localAddresses (&mAddresses, false);
		assert (gotOwnAddresses);
		Log (LogInfo) << LOGID << "Own addresses " << toString (mAddresses) << std::endl;
		
		mInitialChannel = channel;
		mInitialChannel->changed() = abind (dMemFun (this, &Peer::onChannelChange), ChannelWPtr (channel), String("initial"));

		mTcpConnector.channelCreated()            = dMemFun (this, &Peer::tcpChannelCreated);

		addComponent (mTcpConnector.protocol());

		mPingProtocol = new PingProtocol ();
		mPingProtocol->pongReceived() = dMemFun (this, &Peer::onPongReceived);
		addComponent (mPingProtocol); // deletion will be done by CommunicationMultiplex
	}
	
	virtual ~Peer () {
		SF_UNREGISTER_ME;
		delComponent (mTcpConnector.protocol());
		mTcpChannel     = ChannelPtr ();
		mInitialChannel = ChannelPtr ();
	}

	void start () {
		mTcpConnector.setHostId (mId);
		mTcpConnector.start();
	}

	void onChannelChange (const ChannelWPtr& channel, const String & channelName) {
		Error e = NoError;
		while (!e) {
			e = mDatagramReader.read(ChannelPtr(channel));
			if (!e) onReceived (mDatagramReader.datagram(), channelName);
			else if (e != error::NotEnough) {
				onError (e, channelName);
			}
		}
	}

	void onReceived (const Datagram & datagram, const String & channelName) {
		String cmd;
		const ByteArray & header = *datagram.header ();
		Deserialization ds (header, cmd);
		if (ds.error()){
			Log (LogError) << LOGID << "Error deserializing the header " << header << std::endl;
			assert (false);
			return;
		}

		dispatch (mOtherId, cmd, ds, *datagram.content());
	}

	void onPongReceived (const HostId & sender, float time, int id){
		Log (LogInfo) << LOGID << "Ping (id=" << id << ") time was " << time * 1000000.0f << "μs" << std::endl;
		mReceivedPong++; return;
	}

	void onError (Error err, const String & channel){
		if (channel == "tcp"){
			if (err == error::Closed || err == error::Eof) {
				mTcpChannel = ChannelPtr ();
				return;
			}
		}
		assert (false);
	}

	void ping () {
		mPingProtocol->sendPing (mOtherId);
	}
	
	int openPings () const {
		return mPingProtocol->openPingCount ();
	}

	void connectOther () {
		Error err = mTcpConnector.createChannel (mOtherId, bind(&Peer::tcpChannelResult, this, _1), 1000);
		assert (err == NoError);
	}
	
	void tcpChannelCreated (const String & otherId, ChannelPtr channel, bool requested){
		assert (!mTcpChannel);
		mTcpChannel = channel;
		mTcpChannel->changed() = abind (dMemFun (this, &Peer::onChannelChange), ChannelWPtr (channel), String ("tcp"));
		onChannelChange (channel, "tcp");
	}

	void tcpChannelResult (Error e) {
		assert (!e);
	}

	// Implementation of CommunicationDelegate
	// dispatch (..) is already implemented by ProtocollCommDelegate
	Error send (const HostId & id, const Datagram & datagram, const ResultCallback & callback) {
		assert (!callback && "Not supported here");
		if (mTcpChannel) {
			Log (LogInfo) << LOGID << "Will send " << datagram.header() << " via channel tcp" << std::endl;
			return datagram.sendTo (mTcpChannel);
		}
		else {
			Log (LogInfo) << LOGID << "Will send " << datagram.header() << " via channel initial" << std::endl;
			return datagram.sendTo (mInitialChannel);
		}
	}
	Error send (const HostSet & set, const Datagram & datagram) {
		// to get it compiling
		tassert (false, "Not Implemented");
		return error::NotSupported;
	}
	
	int channelLevel (const HostId & receiver) {
		return 1; // not supported
	}

	
	HostId hostId() const { return mId; }
	
	HostId mId;
	String mOtherId;

	ChannelPtr mInitialChannel;
	
	std::vector<String> mAddresses;
	ChannelPtr mTcpChannel;
	TCPChannelConnector mTcpConnector;
	
	int mSendPing, mReceivedPong;
	PingProtocol * mPingProtocol;

	DatagramReader mDatagramReader;
};

/**
 * Sends pings between the two peers and tries to build a TCP Connection (which again is tested with Pings).
 * They must have an initial Channel between them.
 */
bool testRun (Peer & p1, Peer & p2){
	p1.start ();
	p2.start ();

	// At first some ping tests
	for (int i = 0; i < 10; i++){
		p1.ping();
	}
	for (int i = 10; i < 20; i++){
		p2.ping();
	}
	// wait some time in which channels communicate (async)
	sleep (2);
	int p1open = p1.openPings ();
	int p2open = p2.openPings ();
	assert (p1open == 0 && p2open == 0);
	if (p1open != 0|| p2open != 0) return false;

	// Now let the two connect each other
	p1.connectOther ();

	// wait until connected
	{
		int max = 50;
		int i = 0;
		for (; i < max; i++){
			if (p1.mTcpChannel) break;
			sleep (1);
		}
		if (i == max) { assert (!"Timeout during connect"); }
	}

	for (int i = 0; i < 10; i++){
		p2.ping();
		p1.ping ();
	}
	// wait some time in which channels communicate (async)
	sleep (2);
	p1open = p1.openPings ();
	p2open = p2.openPings ();

	assert (p1open == 0);
	assert (p2open == 0);
	if (p1open != 0) return false;
	if (p2open != 0) return false;
	return true;
}

/// Handles construction of channels
struct ChannelBuildHelper {
	ChannelBuildHelper (IMDispatcher & dispatcher) : createdNumCalls (0) {
		dispatcher.channelCreated() = bind (&ChannelBuildHelper::channelCreated, this, _1, _2);
	}
	
	String createdChannelId;
	ChannelPtr createdChannel;
	int createdNumCalls;
	Mutex mutex;
	Condition condition;
	void channelCreated (const String & id, ChannelPtr channel) {
		mutex.lock();
		createdNumCalls++;
		createdChannelId = id;
		createdChannel = channel;
		mutex.unlock();
		condition.notify_all();
	}
	
	ChannelPtr waitForChannelCreated (int timeOutMs){
		LockGuard lock (mutex);
		Time timeOut = futureInMs (timeOutMs);
		while (!createdChannel){
			if (!condition.timed_wait (mutex, timeOut)) break;
		}
		return createdChannel;
	}
};


int main (int argc, char * argv[]){
	schnee::SchneeApp app (argc, argv);
	std::vector<String> addresses;
	bool ret = net::localAddresses (&addresses, false);
	assert (ret);
	Log (LogInfo) << LOGID << "Own address detector: " << toString (addresses) << std::endl;

	bool doXmppTest = false;
	for (int i = 1; i < argc; i++){
		if (String (argv[i]) == "--doXmppTest") doXmppTest = true;
	}
	
	/*
	 * Ziel: Zwei Leute erzeugen aus einem nicht-weiter definierten Channel einen TCP-Channel.
	 */

	/*
	 * Dazu:
	 * - Zwei Leute erzeugen
	 * - Lokalen Channel zwischen beiden herstellen, eine State-Maschine zwischen beiden herstellen, beide haben TCPServer
	 */
	if (!doXmppTest){
		String aId ("ClientA");
		String bId ("ClientB");
		LocalChannelPtr c1 = LocalChannelPtr( new test::LocalChannel ());
		LocalChannelPtr c2 = LocalChannelPtr( new test::LocalChannel ());
		test::LocalChannel::bindChannels (*c1, *c2);
		Peer p1 (c1, aId, bId);
		Peer p2 (c2, bId, aId);
		if (!testRun(p1,p2)) return 1;
	}
	// Und jetzt das ganze nochmal über XMPP
	if (doXmppTest){
		String a = "autotest1@localhost/channeltest";
		String b = "autotest2@localhost/channeltest";
		String apw = "boom74_x";
		String bpw = "gotcha13_y";
		
		IMDispatcher dispatcher1;
		ResultCallbackHelper helper;
		Error e = dispatcher1.connect ("xmpp://" + a, apw, helper.onResultFunc());
		assert (!e); if (e) return 1;
		e = helper.wait();
		assert (!e); if (e) return 1;

		IMDispatcher dispatcher2;
		dispatcher2.connect ("xmpp://" + b, bpw, helper.onResultFunc());
		assert (!e); if (e) return 1;
		e = helper.wait();
		assert (!e); if (e) return 1;

		ChannelBuildHelper helper1 (dispatcher1);
		ChannelBuildHelper helper2 (dispatcher2);
		
		e = dispatcher1.createChannel (b, ResultCallback());
		assert (!e);
		helper1.waitForChannelCreated (2000);
		helper2.waitForChannelCreated (2000);
		assert (helper1.createdChannel);
		assert (helper2.createdChannel);
		assert (helper1.createdNumCalls == 1);
		assert (helper2.createdNumCalls == 1);
		if (!helper1.createdChannel || !helper2.createdChannel) return 1;
		if (helper1.createdNumCalls != 1 || helper2.createdNumCalls != 1) return 1;
		
		Peer p1 (helper1.createdChannel, a, b);
		Peer p2 (helper2.createdChannel, b, a);
		if (!testRun (p1, p2)) return 1;
	} else {
		std::cout << "Skipping XMPP Test, add --doXmppTest to the parameters in order to activate it" << std::endl;
	}

	// Let the system fall together, hope nothing crashes..
	return 0;
}

