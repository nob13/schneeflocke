#include "Authentication.h"
#include <schnee/tools/Log.h>
#include <schnee/tools/MicroTime.h>
namespace sf {

String Authentication::CertInfo::fingerprint () const {
	if (!cert) return "";
	String s;
	cert->fingerprintSha256(&s);
	return s;
}


Authentication::Authentication () {
	mEnabled = false;
	mKeySet  = false;
}

void Authentication::setIdentity (const String & identity) {
	mIdentity = identity;
}

void Authentication::enable (bool v) {
	if (v && !mKeySet) {
		mKey         = x509::PrivateKeyPtr (new x509::PrivateKey());
		mCertificate = x509::CertificatePtr (new x509::Certificate());
		double t0 = sf::microtime();
		Log (LogProfile) << LOGID << "Generating key as no key was set" << std::endl;
		if (mIdentity.empty()){
			Log (LogError) << LOGID << "No identity set!" << std::endl;
		}
		mKey->generate(1024);
		mCertificate->setKey(mKey.get());
		mCertificate->setVersion (1);
		mCertificate->setActivationTime();
		mCertificate->setExpirationDays (10 * 365); // todo: reduce in future and check it
		mCertificate->setSerial (1);
		mCertificate->setCommonName(mIdentity.c_str());
		mCertificate->sign(mCertificate.get(), mKey.get()); // self sign
		mCertificate->fingerprintSha256(&mCertFingerprint);
		mKeySet = true;
		double t1 = sf::microtime ();
		Log (LogProfile) << LOGID << "Key generation took " << (t1 - t0) / 1000.0 << "ms" << std::endl;
	}
	mEnabled = v;
}

void Authentication::update (const String & identity, const CertInfo& info) {
	if (!info.cert || info.type == CT_ERROR) {
		mCerts.erase(identity);
	} else {
		mCerts[identity] = info;
	}
}

Authentication::CertInfo Authentication::get (const String & identity) {
	CertMap::const_iterator i = mCerts.find(identity);
	if (i == mCerts.end()) {
		CertInfo fail;
		fail.type = CT_ERROR;
		return fail;
	}
	return i->second;
}

const String& Authentication::certFingerprint () const {
	return mCertFingerprint;
}

}
