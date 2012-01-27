#pragma once

#include <gnutls/x509.h>
#include <schnee/sftypes.h>
/**
 * @file
 * Wraps (from us used) used X509 Functionality from GnuTLS
 * in C++ Classes.
 */

namespace sf {
class TLSCertificates;

namespace x509 {
struct PrivateKey {
	PrivateKey () { gnutls_x509_privkey_init(&data); }
	~PrivateKey () { gnutls_x509_privkey_deinit (data); }

	/// Generate RSA key
	int generate (int length = 1024){
		return gnutls_x509_privkey_generate (data, GNUTLS_PK_RSA, length, 0);
	}

	int textExport (std::string * dst) {
		char buffer [10 * 1024];
		size_t bufferSize = sizeof(buffer);
		int r = gnutls_x509_privkey_export (data, GNUTLS_X509_FMT_PEM, buffer, &bufferSize);
		if (!r) *dst = buffer;
		return r;
	}

	gnutls_x509_privkey_t data;
};

struct CertificateRequest {
	CertificateRequest () {gnutls_x509_crq_init (&data);}
	~CertificateRequest () {gnutls_x509_crq_deinit(data);}

	/// Set some distinguished name (not DER encoded)
	int setDN (const char * oid, const char * value, int len = -1) {
		if (len < 0) len = strlen (value);
		return gnutls_x509_crq_set_dn_by_oid (data, oid, 0, value, len);
	}

	int setCountryName (const char * name) {
		return setDN (GNUTLS_OID_X520_COUNTRY_NAME, name);
	}

	int setCommonName (const char * name) {
		return setDN (GNUTLS_OID_X520_COMMON_NAME, name);
	}

	int setVersion (int version = 1) {
		return gnutls_x509_crq_set_version (data, version);
	}
	int setChallengePassword (const char * password) {
		return gnutls_x509_crq_set_challenge_password (data, password);
	}
	int setKey (PrivateKey * key) {
		return gnutls_x509_crq_set_key (data, key->data);
	}
	int sign (PrivateKey * key) {
		return gnutls_x509_crq_sign (data, key->data);
	}
	int textExport (std::string * dst) {
		char buffer [10 * 1024];
		size_t bufferSize = sizeof (buffer);
		int r = gnutls_x509_crq_export (data, GNUTLS_X509_FMT_PEM, buffer, &bufferSize);
		if (!r) *dst = buffer;
		return r;
	}

	gnutls_x509_crq_t data;
};

struct Certificate {
	Certificate () {gnutls_x509_crt_init (&data);}
	~Certificate() {gnutls_x509_crt_deinit (data); }

	/// Set some distinguished name (not DER encoded)
	/// 0 = success
	int setDN (const char * oid, const char * value, int len = -1) {
		if (len < 0) len = strlen (value);
		return gnutls_x509_crt_set_dn_by_oid (data, oid, 0, value, len);
	}

	/// 0 = success
	int getDN (const char * oid, std::string * target) const {
		char buffer[1024];
		memset (buffer, sizeof(buffer), 0);
		size_t size = sizeof(buffer) - 1;
		int r = gnutls_x509_crt_get_dn_by_oid (data, oid, 0, 0, buffer, &size);
		if (r) return r;
		target->assign (buffer, buffer + size);
		return 0;
	}

	// necessary
	/// 0 = success
	int setVersion (int version = 1) {
		return gnutls_x509_crt_set_version (data, version);
	}

	/// 0 = success
	int setExpirationTime (time_t t) {
		return gnutls_x509_crt_set_expiration_time (data ,t);
	}

	/// 0 = success
	int setExpirationDays (int days) {
		return setExpirationTime (::time(NULL) + 3600 * 24 * days);
	}

	// necessary.
	/// 0 = success
	int setActivationTime (time_t t = ::time(NULL)) {
		return gnutls_x509_crt_set_activation_time (data, t);
	}

	// necessary
	/// 0 = success
	int setSerial (int serial) {
		return gnutls_x509_crt_set_serial (data, &serial, sizeof(serial));
	}

	/// 0 = success
	int setCountryName (const char * name) {
		return setDN (GNUTLS_OID_X520_COUNTRY_NAME, name);
	}

	/// 0 = success
	int setOrganizationName (const char * name) {
		return setDN (GNUTLS_OID_X520_ORGANIZATION_NAME, name);
	}

	/// 0 = success
	int setOrganisationUnitName (const char * name) {
		return setDN (GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME, name);
	}

	/// 0 = success
	int setStateName (const char * name) {
		return setDN (GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, name);
	}

	/// 0 = success
	int setLocalityName (const char * name) {
		return setDN (GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, name);
	}

	/// 0 = success
	int setCommonName (const char * name) {
		return setDN (GNUTLS_OID_X520_COMMON_NAME, name);
	}
	/// 0 = success
	int getCommonName (std::string * name) {
		return getDN (GNUTLS_OID_X520_COMMON_NAME, name);
	}

	/// 0 = success
	int setName (const char * name){
		return setDN (GNUTLS_OID_X520_NAME, name);
	}

	/// 0 = success
	int setCaStatus (bool status) {
		return gnutls_x509_crt_set_ca_status (data, status ? 1 : 0);
	}

	/// 0 = success
	int getCaStatus (bool * v) const {
		unsigned int status = 0;
		int result = gnutls_x509_crt_get_ca_status (data, &status);
		*v = status != 0;
		return result;
	}

	// necessary?
	/// 0 = success
	int setUid (const char * name) {
		return setDN (GNUTLS_OID_LDAP_UID, name);
	}

	/// 0 = success
	int setKey (PrivateKey * key) {
		return gnutls_x509_crt_set_key (data, key->data);
	}

	/// 0 = success
	int sign (Certificate * issuer, PrivateKey * issuerKey) {
		return gnutls_x509_crt_sign (data, issuer->data, issuerKey->data);
	}

	/// 0 = success
	int setRequest (CertificateRequest * req) {
		return gnutls_x509_crt_set_crq (data, req->data);
	}

	/// 0 = success
	int textExport (std::string * dst) const {
		char buffer [10 * 1024];
		size_t bufferSize = sizeof (buffer);
		int r = gnutls_x509_crt_export (data, GNUTLS_X509_FMT_PEM, buffer, &bufferSize);
		if (!r) *dst = buffer;
		return r;
	}
	/// 0 = success
	int textImport (const std::string & src) const {
		gnutls_datum_t datum;
		datum.data = (unsigned char*) src.c_str();
		datum.size = src.length();
		int r = gnutls_x509_crt_import (data, &datum, GNUTLS_X509_FMT_PEM);
		return r;
	}

	/// 0 = success
	int dnTextExport (std::string * dst) const {
		char buffer [10 * 1024];
		size_t bufferSize = sizeof (buffer);
		int r = gnutls_x509_crt_get_dn (data, buffer, &bufferSize);
		if (!r) *dst = buffer;
		return r;
	}

	/// Non-0-Ptr = success
	ByteArrayPtr fingerprintSha256Binary () const {
		ByteArrayPtr buffer = createByteArrayPtr();
		buffer->resize(64);
		size_t bufferSize = buffer->size();
		int r = gnutls_x509_crt_get_fingerprint (data, GNUTLS_DIG_SHA256, buffer->c_array(), &bufferSize);
		if (r) return ByteArrayPtr();
		else {
			buffer->resize(bufferSize);
		}
		return buffer;
	}

	int fingerprintSha256 (std::string * dst) {
		ByteArrayPtr sha256 = fingerprintSha256Binary();
		if (!sha256) return 1;
		char buffer [129];
		::memset (buffer, 0, sizeof(buffer));
		size_t bufferSize = sizeof (buffer) - 1;
		gnutls_datum d;
		d.data = (unsigned char*) sha256->c_array();
		d.size = sha256->size();
		int r = gnutls_hex_encode (&d, buffer, &bufferSize);
		if (!r) *dst = buffer;
		return r;
	}

	/// 0 = success
	int binaryImport (const gnutls_datum_t* buffer) {
		return gnutls_x509_crt_import (data, buffer, GNUTLS_X509_FMT_DER);
	}

	/// true = success
	bool checkHostname (const std::string & hostname) const {
		return gnutls_x509_crt_check_hostname (data, hostname.c_str()) != 0;
	}

	/// true = success
	bool verify (const Certificate * trusted) const;

	/// verifies against given thrust list of certificates
	bool verify (const TLSCertificates & certificates) const;


	gnutls_x509_crt_t data;
};

typedef shared_ptr<Certificate> CertificatePtr;
typedef shared_ptr<PrivateKey>  PrivateKeyPtr;

}
}
