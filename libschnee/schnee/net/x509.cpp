#include "x509.h"
#include "TLSCertificates.h"
#include <schnee/tools/Log.h>

namespace sf {
namespace x509 {

static void dumpVerifyErrorCode (int v) {
	if (v & GNUTLS_CERT_SIGNER_NOT_FOUND){
		Log (LogInfo )<< LOGID << "Signer not found" << std::endl;
	}
	if (v & GNUTLS_CERT_SIGNER_NOT_CA) {
		Log (LogInfo) << LOGID << "Signer is not a CA" << std::endl;
	}
	if (v & GNUTLS_CERT_INSECURE_ALGORITHM) {
		Log (LogInfo) << LOGID << "Insecure algorithm" << std::endl;
	}
}

bool Certificate::verify (const Certificate * trusted) const {
	unsigned int v = 0;
	int r = gnutls_x509_crt_verify (data, &trusted->data, 1, 0, &v);
	if (r) {
		Log (LogError) << LOGID << "Verification error: " << gnutls_strerror_name(r) << std::endl;
		return false;
	} else {
		if (v & GNUTLS_CERT_INVALID){
			dumpVerifyErrorCode (v);
			Log (LogInfo) << LOGID << "Certificate not trusted, " << v << std::endl;
			return false;
		} else {

			return true;
		}
	}
}

bool Certificate::verify (const TLSCertificates & certificates) const {
	unsigned int v = 0;
	int caListLength = (int) certificates.certificateCount();
	const gnutls_x509_crt_t * certs = certificates.gnuTlsCertificates();
	int r = gnutls_x509_crt_verify (data, certs, caListLength, 0, &v);
	if (r) {
		Log (LogError) << LOGID << "Verification error: " << gnutls_strerror_name(r) << std::endl;
		return false;
	} else {
		if (v & GNUTLS_CERT_INVALID){
			dumpVerifyErrorCode(v);
			Log (LogInfo) << LOGID << "Certificate not trusted, " << v << std::endl;
			return false;
		} else {

			return true;
		}
	}
}


}
}
