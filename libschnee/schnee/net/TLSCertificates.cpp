#include "TLSCertificates.h"

namespace sf {

TLSCertificates::TLSCertificates () {

}

TLSCertificates::~TLSCertificates () {

}

void TLSCertificates::add (const x509::CertificatePtr & cert) {
	mCerts.push_back (cert);
	updateGnuTlsCerts();
}

Error TLSCertificates::add (const String & certAsPemText) {
	x509::CertificatePtr cert (new x509::Certificate());
	int error = cert->textImport (certAsPemText);
	if (error) {
		return error::InvalidArgument;
	}
	add (cert);
	return NoError;
}

void TLSCertificates::clear () {
	mCerts.clear();
	updateGnuTlsCerts();
}

void TLSCertificates::updateGnuTlsCerts () {
	mGnuTlsCerts.clear();
	for (std::vector<x509::CertificatePtr>::const_iterator i = mCerts.begin(); i != mCerts.end(); i++) {
		mGnuTlsCerts.push_back (i->get()->data); // is ok, as gnutls_x509_crt_t is just a pointer
	}
}



}
