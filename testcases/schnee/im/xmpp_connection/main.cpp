#include <schnee/test/test.h>
#include <schnee/im/xmpp/DirectXMPPConnection.h>
#include <schnee/im/xmpp/XMPPRegistration.h>
#include <schnee/tools/ResultCallbackHelper.h>
#include <schnee/net/TCPSocket.h>
#include <schnee/net/TLSChannel.h>

#include <schnee/im/xmpp/XMPPStream.h>
using namespace sf;

int manualConnect () {
	TCPSocketPtr socket =  TCPSocketPtr (new TCPSocket());
	ResultCallbackHelper connectHandler;
	Error e = socket->connectToHost("localhost", 5222, 30000, connectHandler.onResultFunc());
	if (e) {
		fprintf (stderr, "Cannot do test, no XMPP server, e = %s", toString (e));
		return 1;
	}
	tcheck1 (connectHandler.waitReadyAndNoError(50000));
	XMPPStream stream;
	stream.setInfo("autotest1@localhost", "localhost"); // dst is important
	ResultCallbackHelper initHandler;
	e = stream.startInit(socket, initHandler.onResultFunc());
	tcheck1(!e);
	tcheck1 (initHandler.waitReadyAndNoError(100));


	ResultCallbackHelper featureHandler;
	e = stream.waitFeatures(featureHandler.onResultFunc());
	tcheck1 (!e);
	tcheck1 (featureHandler.waitReadyAndNoError(100));

	/// TLS Stuff
	ResultCallbackHelper tlsHandler;
	e = stream.requestTls(tlsHandler.onResultFunc());
	tcheck1 (!e);
	tcheck1 (tlsHandler.waitReadyAndNoError (100));
	stream.uncouple();
	TLSChannelPtr tlsChannel = TLSChannelPtr (new TLSChannel (socket));
	ResultCallbackHelper handshakeHandler;
	e = tlsChannel->clientHandshake(TLSChannel::X509, handshakeHandler.onResultFunc());
	tcheck1 (!e);
	tcheck1 (handshakeHandler.waitReadyAndNoError(100));

	/// Reuse
	e = stream.startInit(tlsChannel, initHandler.onResultFunc());
	tcheck1 (!e);
	tcheck1 (initHandler.waitReadyAndNoError(100));

	// features
	e = stream.waitFeatures(featureHandler.onResultFunc());
	tcheck1 (!e && featureHandler.waitReadyAndNoError(100));

	// login
	ResultCallbackHelper authHandler;
	e = stream.authenticate("autotest1", "boom74_x", authHandler.onResultFunc());
	tcheck1 (!e && authHandler.waitReadyAndNoError(100));

	// Reuse (AGAIN, after login)
	e = stream.startInit(tlsChannel, initHandler.onResultFunc());
	tcheck1(!e && initHandler.waitReadyAndNoError(100));

	// wait features
	e = stream.waitFeatures(featureHandler.onResultFunc());
	tcheck1 (!e && featureHandler.waitReadyAndNoError(100));

	// bind username
	ResultCallbackHelper bindHandler;
	e = stream.bindResource("testcase", sf::bind (bindHandler.onResultFunc(), _1));
	tcheck1 (!e && bindHandler.waitReadyAndNoError(100));

	// start session
	ResultCallbackHelper sessionHandler;
	e = stream.startSession(sessionHandler.onResultFunc());
	tcheck1 (!e && sessionHandler.waitReadyAndNoError(100));

	// close the stream
	e = stream.close();
	tcheck1 (!e);


	return 0;
}

/// Connect using XMPPConnection2
int autoConnect () {
	shared_ptr<XMPPStream> stream (new XMPPStream);
	DirectXMPPConnection con;
	Error e = con.setConnectionString("xmpp://autotest1:boom74_x@localhost/autoConnect");
	tcheck1 (!e);

	ResultCallbackHelper conResultHandler;
	e = con.connect(stream, 2500, conResultHandler.onResultFunc());
	tcheck1 (!e && conResultHandler.waitReadyAndNoError(5000));

	tcheck1 (stream->ownFullJid() == "autotest1@localhost/autoConnect");

	e = stream->close();
	tcheck1 (!e);
	return 0;
}

int registerMyself () {
	XMPPRegistration registration;
	XMPPRegistration::Credentials c;
	c.username = "teddytester";
	c.email = "teddy@sflx.net";
	c.password = "tedman";
	ResultCallbackHelper conResultHandler;
	Error e = registration.start("localhost", c, 60000, conResultHandler.onResultFunc());
	tcheck1 (!e);
	tcheck1 (conResultHandler.waitUntilReady(5000));
	tcheck1 (conResultHandler.result() == NoError || conResultHandler.result() == error::ExistsAlready);
	return 0;
}


int main (int argc, char * argv[]) {
	schnee::SchneeApp app (argc, argv);
	testcase_start();
	testcase (manualConnect());
	testcase (autoConnect());
	testcase (registerMyself());
	testcase_end();
	return ret;
}

