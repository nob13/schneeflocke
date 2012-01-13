#include <schnee/schnee.h>

#include <schnee/net/TLSChannel.h>
#include <schnee/net/TCPSocket.h>
#include <schnee/test/LocalChannel.h>
#include <schnee/tools/Log.h>
#include <schnee/tools/ResultCallbackHelper.h>

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

int createSelfSignedCertificate (x509::PrivateKey * key, x509::Certificate * cert, const char * name = 0) {
	int ret = 0;
	key->generate();
	std::string keyText;
	CHECK (key->textExport(&keyText));
	// printf ("Key: %s\n", keyText.c_str());

	CHECK (cert->setKey (key));
	CHECK (cert->setVersion (1));
	CHECK (cert->setActivationTime());
	CHECK (cert->setExpirationDays(365));
	CHECK (cert->setSerial(1));
	if (name) {
		CHECK (cert->setCommonName(name));
	}
	CHECK (cert->sign(cert, key)); // self sign

	std::string cacertText;
	CHECK (cert->textExport (&cacertText));
	std::string cacertDnText;
	CHECK (cert->dnTextExport(&cacertDnText));
	std::string sha256Fingerprint;
	CHECK (cert->fingerprintSha256(&sha256Fingerprint));
	// printf ("Cert Text:               %s\n", cacertText.c_str());
	printf ("Created self sigend certificate\n");
	printf ("  Cert DN Text:            %s\n", cacertDnText.c_str());
	printf ("  Cert SHA256 Fingerprint: %s\n", sha256Fingerprint.c_str());
	return ret;
}



int comTest (Channel& sa, Channel& sb) {
	Error e = sa.write (createByteArrayPtr("Hello World"));
	tcheck (!e, "Should send");

	test::millisleep_locked(100);
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
	test::millisleep_locked (100);
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

	test::millisleep_locked (100);
	return comTest (sa, sb);
}

int x509AuthTest () {
	x509::PrivateKeyPtr   serverKey = x509::PrivateKeyPtr (new x509::PrivateKey());
	x509::CertificatePtr  serverCert = x509::CertificatePtr (new x509::Certificate());
	createSelfSignedCertificate (serverKey.get(), serverCert.get(), "bummi");

	x509::PrivateKeyPtr   clientKey = x509::PrivateKeyPtr (new x509::PrivateKey());
	x509::CertificatePtr  clientCert = x509::CertificatePtr (new x509::Certificate());
	createSelfSignedCertificate (clientKey.get(), clientCert.get(), "bommel");

	LocalChannelPtr a = LocalChannelPtr (new test::LocalChannel());
	LocalChannelPtr b = LocalChannelPtr (new test::LocalChannel());
	test::LocalChannel::bindChannels(*a,*b);

	TLSChannel sa (a);
	TLSChannel sb (b);

	sa.setKey (clientCert, clientKey);
	sb.setKey (serverCert, serverKey);
	ResultCallbackHelper helperA;
	ResultCallbackHelper helperB;
	sa.clientHandshake(TLSChannel::X509, helperA.onResultFunc());
	sb.serverHandshake(TLSChannel::X509, helperB.onResultFunc());

	tcheck1 (helperA.wait() == NoError);
	tcheck1 (helperB.wait() == NoError);

	tcheck (sa.authenticate(serverCert.get(), "other") != NoError, "Should detect wrong hostname");
	tcheck (sa.authenticate(serverCert.get(), "bummi") == NoError, "Should accept self signed if provided");
	tcheck (sb.authenticate(clientCert.get(), "other") != NoError, "Should detect wrong hostname");
	tcheck (sb.authenticate(clientCert.get(), "bommel") == NoError, "Should accept self signed if provided");
	return 0;
}

int x509AuthTest2 () {
	TCPSocketPtr tcpSocket (new TCPSocket());
	ResultCallbackHelper helper;
	// google.de doesn't work; as it comes back with COMMON NAME google.com
	tcpSocket->connectToHost("www.google.com", 443, 30000, helper.onResultFunc());
	tcheck1 (helper.wait() == NoError);
	TLSChannel tls (tcpSocket);
	tls.clientHandshake (TLSChannel::X509, helper.onResultFunc());
	tcheck1 (helper.wait() == NoError);
	x509::CertificatePtr cert = tls.peerCertificate();
	String dn;
	tcheck1 (cert->dnTextExport(&dn) == 0);
	printf ("Google DN: %s\n", dn.c_str());
	String fp;
	tcheck1 (cert->fingerprintSha256(&fp) == 0);
	printf ("Fignerprint: %s\n", fp.c_str());
	tcheck1 (tls.authenticate(cert.get(), "www.google.com") == NoError);
	tcheck1 (tls.authenticate(cert.get(), "www.sflx.net") == error::AuthError);
	return 0;
}

static const char * sflxCaCertificate = "-----BEGIN CERTIFICATE-----"
		"MIIEMDCCAxigAwIBAgIJANPa8IW8g0knMA0GCSqGSIb3DQEBBQUAMG0xGTAXBgNV"
		"BAoTEHNmbHgubmV0IFByb2plY3QxCzAJBgNVBAsTAkNBMQ8wDQYDVQQHEwZCZXJs"
		"aW4xDzANBgNVBAgTBkJlcmxpbjELMAkGA1UEBhMCREUxFDASBgNVBAMTC3NmbHgu"
		"bmV0IENBMB4XDTEyMDExMzE3MTgwMFoXDTIyMDExMDE3MTgwMFowbTEZMBcGA1UE"
		"ChMQc2ZseC5uZXQgUHJvamVjdDELMAkGA1UECxMCQ0ExDzANBgNVBAcTBkJlcmxp"
		"bjEPMA0GA1UECBMGQmVybGluMQswCQYDVQQGEwJERTEUMBIGA1UEAxMLc2ZseC5u"
		"ZXQgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDED+J5bVXnlpvk"
		"RxE5szJ1oBJ1neXZj/yAbrXxJ3O1ZKNRepS70Vz8TCyb/XyBxtzkmUOyY4ny/hRK"
		"Ar3mgGKsrN3Qa02hQ014DahdyW/HdUjAuqannMPHuJvPqqfzJY0imCNXmy708aaf"
		"s6M29HKbN+ReUIa0ZNYq40i9Rmkk+uFvpwww/LCFRhrr6JoM+Tr+a46sKRsPevlz"
		"ZUZM+hL4ewwquFd/rvSJxVQxfeiK1/RqsYQFFWb7b0aa0Mx70SVUws9OOMJfzEMQ"
		"OEWxfRcJnjFo4EoBQdX+altEsBymmS/x1H14HkGUQeby2VlKS2CwEb8xIzbpVqej"
		"foUgOoU5AgMBAAGjgdIwgc8wDAYDVR0TBAUwAwEB/zAdBgNVHQ4EFgQUZsPKN1J5"
		"fFSp84Vs/aSEB+ra+hQwgZ8GA1UdIwSBlzCBlIAUZsPKN1J5fFSp84Vs/aSEB+ra"
		"+hShcaRvMG0xGTAXBgNVBAoTEHNmbHgubmV0IFByb2plY3QxCzAJBgNVBAsTAkNB"
		"MQ8wDQYDVQQHEwZCZXJsaW4xDzANBgNVBAgTBkJlcmxpbjELMAkGA1UEBhMCREUx"
		"FDASBgNVBAMTC3NmbHgubmV0IENBggkA09rwhbyDSScwDQYJKoZIhvcNAQEFBQAD"
		"ggEBAGe+uUg9jJiDv5xX0qWgkBgJDmR1exp8QlaoMVnFFirxxTjz6JFuPJa6R/PM"
		"C/quLO9rTCVR2BLUk3+wpr82ZYre+RiyWA6oDSzv0pJoVveTP1Mm/E/hjqr2ufKF"
		"oRpX8fUFW1Dwxrt9TnIYRp6v1cHRZzUeuoMoo69YDcv0v8rllpbRh1PS/HC5YYPU"
		"CLzz3tIIucaWXnSMARdbqGuixLXZeY1te9KbtIOIsptIlNU9Y3AzP3S/gwPj8LXL"
		"Og3d/1PerrJyg4pqx7gz2GqwHkeWMIU8T1DIzeGBSKH2TsWFA5dTLLSb2Ky8TjVq"
		"zMjqqHVo2tgYNlTvabyk9QQCAPI="
		"-----END CERTIFICATE-----";

/// Note: this test is not portable; as it requires installed certificates...
int x509AuthTest3 () {
	x509::CertificatePtr caCert = x509::CertificatePtr (new x509::Certificate ());
	int ret = caCert->textImport(sflxCaCertificate);
	tcheck1 (!ret);

	TCPSocketPtr tcpSocket (new TCPSocket());
	ResultCallbackHelper helper;
	tcpSocket->connectToHost("localhost", 443, 30000, helper.onResultFunc());
	tcheck1 (helper.wait() == NoError);
	TLSChannel tls (tcpSocket);
	tls.clientHandshake (TLSChannel::X509, helper.onResultFunc());
	tcheck1 (helper.wait() == NoError);

	tcheck (tls.authenticate(caCert.get(), "localhostbla") == error::AuthError, "Should detect wrong hostname");
	tcheck (tls.authenticate(caCert.get(), "localhost") == NoError, "Should accept right certificate");
	return 0;
}

// create private key and a certificate request
// Mostly copied from GnuTLS doc.
int createCertificateRequestTest () {
	int ret = 0;
	x509::PrivateKey key;
	key.generate();
	std::string keyText;
	key.textExport(&keyText);
	// printf ("My Private Key is: %s\n", keyText.c_str());

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
	// printf ("My Request is %s\n", requestText.c_str());
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
	// printf ("Cert Text: %s\n", certText.c_str());

	// Test this certificate:
	if (!cert.verify (&cacert)) {
		Log (LogError) << LOGID << "Did not trust self just signed certificate?" << std::endl;
		ret++;
	}
	return ret;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	testcase_start();
	testcase (diffieHellmanTest());
	testcase (x509Comtest());
	testcase (x509AuthTest());
	testcase (x509AuthTest2());
	testcase (x509AuthTest3());
	testcase (createCertificateRequestTest());
	testcase (signCertificateTest());
	testcase_end();
	return ret;
}
