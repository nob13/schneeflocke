#include <schnee/test/test.h>
#include <schnee/net/TLSChannel.h>
#include <schnee/net/TCPSocket.h>
#include <schnee/net/TCPServer.h>
#include <schnee/tools/Log.h>
#include <schnee/tools/MicroTime.h>
#include <schnee/tools/ResultCallbackHelper.h>

using namespace sf;

/*
 * Manual testcase for testing TLS server/client connectivity,
 * because there are still problems in that
 *
 * to be started with either --server or --client
 */

struct Credentials {
	x509::CertificatePtr cert;
	x509::PrivateKeyPtr key;
	Credentials (const String & name) {
		generate (name);
	}
	void generate (const String & name) {
		key  = x509::PrivateKeyPtr (new x509::PrivateKey());
		cert = x509::CertificatePtr (new x509::Certificate());
		double t0 = sf::microtime();
		key->generate(1024);
		cert->setKey(key.get());
		cert->setVersion (1);
		time_t t = time(NULL) - 7200;
		cert->setActivationTime(t);
		cert->setExpirationDays (10 * 365); // todo: reduce in future and check it
		cert->setSerial (1);
		cert->setCaStatus (true);
		cert->setCommonName(name.c_str());
		cert->sign(cert.get(), key.get()); // self sign
		double t1 = sf::microtime ();
		Log (LogProfile) << LOGID << "Key generation took " << (t1 - t0) * 1000.0 << "ms" << std::endl;
		
	}
};

ByteArrayPtr syncRead (Channel & c, int bytes, int timeOutMs) {
	Time  t (sf::futureInMs(timeOutMs));
	ByteArray buffer;
	while (true) {
		ByteArrayPtr data = c.read(bytes);
		if (!data || data->size() == 0) {
			if (c.error()){
				return ByteArrayPtr();
			}
			ResultCallbackHelper helper;
			c.changed() = helper.onReadyFunc();
			int restMs = (t - sf::currentTime()).total_milliseconds();
			if (restMs < 0) return ByteArrayPtr(); // timeout
			helper.wait(restMs);
		} else {
			buffer.append(*data);
			bytes -= (int) data->size();
			if (bytes == 0) {
				ByteArrayPtr result = createByteArrayPtr();
				result->swap (buffer);
				return result;
			}
		}

	}
}

int startServer (const String & name, int port) {
	Credentials cred (name);
	TCPServer server;
	ResultCallbackHelper handler;
	server.newConnection() = handler.onReadyFunc();
	bool suc = server.listen (port);
	tassert1 (suc);
	printf ("Server will listen on %d\n", port);
	while (true) {
		handler.wait(-1);
		shared_ptr<TCPSocket> sock;
		while ((sock = server.nextPendingConnection())){
			printf ("Accepted a connection on %s\n", sock->info().laddress.c_str());
			TLSChannel tls (sock);
			tls.setKey (cred.cert, cred.key);
			ResultCallbackHelper tlsHandler;
			tls.serverHandshake(TLSChannel::X509, tlsHandler.onResultFunc());
			Error e = tlsHandler.wait(1000);
			if (e) {
				Log (LogError) << LOGID << "Handshake failed " << toString (e) << std::endl;
				continue;
			}
			x509::CertificatePtr clientCert = tls.peerCertificate();
			e = tls.authenticate(clientCert.get(), "client");
			if (e) {
				Log (LogError) << LOGID << "Certificate check failed " << toString (e) << std::endl;
				continue;
			}
			ByteArrayPtr msg = syncRead (tls, 10, 1000);
			if (!msg) {
				Log (LogError) << LOGID << "Did not get message" << std::endl;
				continue;
			}
			ResultCallbackHelper writeCallback;
			tls.write(msg, writeCallback.onResultFunc());
			e = writeCallback.wait(1000);
			if (e) {
				Log (LogError) << LOGID << "Could not write back result" << std::endl;
			}
			printf ("Successfully handled request\n");
		}
		handler.reset();
	}


	return 0;
}

int startClient (const String& address, int port) {
	printf ("Client will conenct to %s:%d\n", address.c_str(), port);
	Credentials cred ("client");
	TCPSocketPtr socket (new TCPSocket);
	ResultCallbackHelper helper;
	Error e = socket->connectToHost(address, port, 10000, helper.onResultFunc());
	if (!e)
		e = helper.wait (10000);
	if (e) {
		Log (LogError) << LOGID << toString (e) << " on connect" << std::endl;
		return 1;
	}
	TLSChannel tls (socket);
	tls.setKey(cred.cert, cred.key);
	tls.disableAuthentication(); // will do it by hand
	tls.clientHandshake(TLSChannel::X509, address, helper.onResultFunc());
	e = helper.wait(10000);
	if (e) {
		Log (LogError) << LOGID << toString (e) << " on TLS handhshake" << std::endl;
		return 1;
	}
	x509::CertificatePtr serverCert = tls.peerCertificate();
	e = tls.authenticate(serverCert.get(), address);
	if (e) {
		Log (LogError) << LOGID << toString (e) << " error on manual certificate check" << std::endl;
		return 1;
	}

	ByteArrayPtr message = sf::createByteArrayPtr("HelloWorld");
	tls.write (message, helper.onResultFunc());
	e = helper.wait (10000);
	if (e) {
		Log (LogError) << LOGID << toString (e) << " could not send" << std::endl;
		return 1;
	}
	ByteArrayPtr r = syncRead (tls, 10, 1000);
	if (!r || *r != *message){
		Log (LogError) << LOGID << toString (e) << " did not get back what I sent" << std::endl;
		return 1;
	}
	printf ("ok\n");
	return 0;
}

int main (int argc, char * argv[]){
	schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;

	bool isServer (false);
	bool isClient (false);
	String address;
	int port = 1234;
	for (int i = 1; i< argc; i++) {
		String s (argv[i]);
		String t,u;
		if (i < argc - 1) t = argv[i + 1];
		if (i < argc - 2) u = argv[i + 2];
		if (s == "--server") {
			isServer = true;
			address = t;
			if (!u.empty() && u[0] != '-'){
				port    = atoi (u.c_str());
			}
		}
		if (s == "--client"){
			isClient = true;
			address = t;
			if (!u.empty() && u[0] != '-'){
				port    = atoi (u.c_str());
			}
		}
	}
	if (!address.empty() && port > 0){
		if (isServer) return startServer (address, port);
		if (isClient) return startClient (address, port);
	}
	printf ("Start with --server [Address] [Port] or --client [Address] [Port = 1234]\n");
	return 1;
}
