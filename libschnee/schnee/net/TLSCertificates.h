#pragma once
#include <schnee/sftypes.h>
#include <schnee/tools/Singleton.h>
#include "x509.h"

namespace sf {

/// Holds generelly trusted TLS/SSL Root Certificates
class TLSCertificates : public Singleton<TLSCertificates> {
public:
	TLSCertificates ();
	~TLSCertificates ();

	/// Add a certificate
	void add (const x509::CertificatePtr & cert);

	/// Add a certificate as PEM text
	Error add (const String & certAsPemText);

	/// Clears all certificates
	void clear ();

	/// Returns certificate count
	size_t certificateCount () const { return mCerts.size(); }

	/// Returns GnuTLS certificate list (hacky)
	/// For use with verify method
	const gnutls_x509_crt_t * gnuTlsCertificates () const { return & (*mGnuTlsCerts.begin()); }
private:

	void updateGnuTlsCerts ();
	std::vector<x509::CertificatePtr> mCerts;
	std::vector<gnutls_x509_crt_t> mGnuTlsCerts; // reordered certificate list for verify-function
};

}
