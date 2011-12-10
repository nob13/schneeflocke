#include <schnee/schnee.h>

#include <schnee/net/TLSChannel.h>
#include <schnee/test/LocalChannel.h>
#include <schnee/tools/Log.h>

#include <schnee/test/test.h>
#include <gnutls/gnutls.h>
#include <gcrypt.h>

using namespace sf;
typedef test::LocalChannelPtr LocalChannelPtr;

/*
 * Testcase for TLS secured channel development.
 *
 * Objectives:
 * - Anonym                       vs Anoynm with Encryption
 * - Certificated vs Anonym       with Encryption
 * - Certificated vs Certificated with Encryption
 *
 * - Certificate creation.
 * - Getting certificate of Anonymous Peers
 */

#define CHECK(X) { int r = X; if (r) { \
	Log (LogWarning) << LOGID << #X << "failed: " << gnutls_strerror_name (r) << std::endl; ret++; }\
}

int createSelfSignedCertificate (x509::PrivateKey * key, x509::Certificate * cert) {
	int ret = 0;
	key->generate();
	std::string keyText;
	CHECK (key->textExport(&keyText));
	printf ("Key: %s\n", keyText.c_str());

	CHECK (cert->setKey (key));
	CHECK (cert->setVersion (1));
	CHECK (cert->setActivationTime());
	CHECK (cert->setExpirationDays(365));
	CHECK (cert->setSerial(1));
	CHECK (cert->sign(cert, key)); // self sign

	std::string cacertText;
	CHECK (cert->textExport (&cacertText));
	printf ("Cert Text: %s\n", cacertText.c_str());
	return ret;
}



int comTest (Channel& sa, Channel& sb) {
	Error e = sa.write (createByteArrayPtr("Hello World"));
	tcheck (!e, "Should send");

	test::millisleep(100);
	ByteArrayPtr data = sb.read();
	tcheck (data, "shall receive");
	tcheck (*data == ByteArray ("Hello World"), "right");
	return 0;
}

int diffieHellmanTest () {
	LocalChannelPtr a = LocalChannelPtr (new test::LocalChannel());
	LocalChannelPtr b = LocalChannelPtr (new test::LocalChannel());
	test::LocalChannel::bindChannels(*a,*b);

	TLSChannel sa (a);
	TLSChannel sb (b);

	sa.clientHandshake(TLSChannel::DH);
	sb.serverHandshake(TLSChannel::DH);
	test::millisleep (100);
	return comTest (sa, sb);
}

int x509Comtest () {
	int ret = 0;
	x509::PrivateKeyPtr  key  = x509::PrivateKeyPtr  (new x509::PrivateKey());
	x509::CertificatePtr cert = x509::CertificatePtr (new x509::Certificate());
	ret += createSelfSignedCertificate (key.get(), cert.get());

	LocalChannelPtr a = LocalChannelPtr (new test::LocalChannel());
	LocalChannelPtr b = LocalChannelPtr (new test::LocalChannel());
	test::LocalChannel::bindChannels(*a,*b);

	TLSChannel sa (a);
	TLSChannel sb (b);

	sb.setKey (cert, key);
	sa.clientHandshake(TLSChannel::X509);
	sb.serverHandshake(TLSChannel::X509);

	test::millisleep (100);
	return comTest (sa, sb);
}

// create private key and a certificate request
// Mostly copied from GnuTLS doc.
int createCertificateRequestTest () {
	int ret = 0;
	x509::PrivateKey key;
	key.generate();
	std::string keyText;
	key.textExport(&keyText);
	printf ("My Private Key is: %s\n", keyText.c_str());

	// Generating requests
	x509::CertificateRequest request;
	CHECK (request.setCountryName("de"));
	CHECK (request.setCommonName("sflx developer"));
	CHECK (request.setVersion(1));
	CHECK (request.setChallengePassword("something to remember"));
	CHECK (request.setKey (&key));
	CHECK (request.sign(&key));
	std::string requestText;
	CHECK (request.textExport(&requestText));
	printf ("My Request is %s\n", requestText.c_str());
	return ret;
}

/// Create a private key, and let it sign by another generated cert
int signCertificateTest () {
	int ret = 0;
	x509::PrivateKey  cakey;
	x509::Certificate cacert;
	ret += createSelfSignedCertificate (&cakey, &cacert);

	// Use it to sign an private key.
	x509::PrivateKey key;
	key.generate();
	x509::Certificate cert;
	CHECK (cert.setKey(&key));
	CHECK (cert.setVersion(1));
	CHECK (cert.setActivationTime());
	CHECK (cert.setExpirationDays(365));
	CHECK (cert.setSerial(2));
	CHECK (cert.sign(&cacert, &cakey));
	std::string certText;
	CHECK (cert.textExport(&certText));
	printf ("Cert Text: %s\n", certText.c_str());

	// Test this certificate:
	if (!cert.verify (&cacert)) {
		Log (LogError) << LOGID << "Did not trust self just signed certificate?" << std::endl;
		ret++;
	}
	return ret;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);

	testcase_start();
	testcase (diffieHellmanTest());
	testcase (x509Comtest());
	testcase (createCertificateRequestTest());
	testcase (signCertificateTest());
	testcase_end();
	return ret;
}
