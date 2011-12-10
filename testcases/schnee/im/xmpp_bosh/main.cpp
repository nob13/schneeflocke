#include <schnee/test/test.h>
#include <schnee/tools/ResultCallbackHelper.h>

#include <schnee/im/xmpp/XMPPStream.h>
#include <schnee/net/http/HttpContext.h>
#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_print.hpp>
#include <schnee/im/xmpp/bosh_impl/BoshNodeBuilder.h>
#include <schnee/im/xmpp/bosh_impl/BoshNodeParser.h>
#include <schnee/im/xmpp/bosh_impl/BoshTransport.h>
#include <schnee/im/xmpp/BoshXMPPConnection.h>
#include <schnee/tools/XMLChunk.h>

using namespace sf;

int testNodeBuildAndParse () {
	BoshNodeBuilder builder;
	builder.addAttribute("hello", "world");
	builder.addAttribute("and", "anotherone");
	builder.addContent (sf::createByteArrayPtr ("<bla>Hi dude</bla>"));
	builder.addContent (sf::createByteArrayPtr ("<bli></bli>"));
	String s = builder.toString();
	printf ("Serialized code: %s\n", s.c_str());

	BoshNodeParser parser;
	Error e = parser.parse(s);
	tcheck1(!e);

	tcheck1(parser.attribute("hello") == "world");
	tcheck1(parser.attribute("and") == "anotherone");
	String back = parser.content();
	XMLChunk chunk = xml::parseDocument(back.c_str(), back.length());
	tcheck1 (chunk.getChild("bla").text() == "Hi dude");
	tcheck1 (chunk.getHasChild("bli"));
	return 0;
}

int testManualConnect () {
	int timeoutMs = 1000000; // for debugging
	Url url = "http://localhost:5280/http-bind/"; // BOSH server
	String host = url.pureHost();
	String pass = "boom74_x";
	String user = "autotest1";

	// BOSHs connect
	ResultCallbackHelper helper;
	BoshTransportPtr transport (new BoshTransport());
	BoshTransport::StringMap addArgs;
	addArgs["to"] = host;
	addArgs["xmpp:version"] = "1.0"; // is important for ejabberd
	addArgs["xmlns:xmpp"] = "urn:xmpp:xbosh"; // is important for ejabberd
	transport->connect(url, addArgs, timeoutMs, helper.onResultFunc());
	tcheck (helper.waitReadyAndNoError(timeoutMs), "Should connect");

	// Feature receivement
	XMPPStream stream;
	stream.setInfo (user + "@" + host, host);
	Error e = stream.startInitAfterHandshake(transport);
	tcheck1(!e);
	stream.waitFeatures(helper.onResultFunc());
	tcheck (helper.waitReadyAndNoError(timeoutMs), "Should receive features");

	// No TLS/reconnecting, is already encrypted.
	// Try to login
	e = stream.authenticate(user,pass, helper.onResultFunc());
	tcheck (!e, "Should start authenticate");
	tcheck (helper.waitReadyAndNoError(timeoutMs), "Should authenticate");

	// Reuse
	transport->restart();
	e = stream.startInitAfterHandshake(transport);
	tcheck (!e, "Should reuse");

	// Waiting for features
	e = stream.waitFeatures(helper.onResultFunc());
	tcheck (!e, "Shall wait for features again");
	tcheck (helper.waitReadyAndNoError(timeoutMs), "Shall wait for a new feature set");

	// Bind resource name
	e = stream.bindResource("testcase", bind (helper.onResultFunc(), _1));
	tcheck (!e, "Should start resource bind");
	tcheck (helper.waitReadyAndNoError(timeoutMs), "Should bind resource");

	// Start session
	e = stream.startSession(helper.onResultFunc());
	tcheck (!e, "Should start session");
	tcheck (helper.waitReadyAndNoError(timeoutMs), "Should start session");

	// Ok thats it
	e = stream.close();
	tcheck (!e, "Should close again");
	return 0;
}

int testAutoConnect () {
	shared_ptr<XMPPStream> stream (new XMPPStream);
	BoshXMPPConnection con;
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

int testReuse () {
	// Connections shall be kept open...
	// Note: Prosody itself doesn't seem to do that
	// but NGINX does it.
	HttpContext httpContext;
	BoshNodeBuilder builder;
	builder.addAttribute("content", "text/xml; charset=utf-8");
	builder.addAttribute("from", "autotest1@localhost");
	builder.addAttribute("hold", "1");
	builder.addAttribute("to", "localhost");
	builder.addAttribute ("rid", "3000");
	builder.addAttribute ("xmlns","http://jabber.org/protocol/httpbind");

	HttpRequest req;
	req.start ("POST", "https://localhost/http-bind/");
	req.addHeader("Content-Type", "text/xml; charset=utf-8");
	req.addContent (sf::createByteArrayPtr(builder.toString()));
	req.end();
	std::pair<Error, HttpResponsePtr> res = httpContext.syncRequest(req, 60000);
	tcheck1(!res.first);
	test::millisleep_locked(1000);
	tcheck1 (httpContext.pendingConnections() == 1);
	return 0;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	testcase_start();
	testcase (testNodeBuildAndParse());
	testcase (testReuse ());
	testcase (testManualConnect());
	testcase (testAutoConnect());
	testcase_end();
	return ret;
}
