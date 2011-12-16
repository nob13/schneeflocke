#pragma once

#include <gnutls/x509.h>
#include <schnee/sftypes.h>
/**
 * @file
 * Wraps (from us used) used X509 Functionality from GnuTLS
 * in C++ Classes.
 */

namespace sf {

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
	int setDN (const char * oid, const char * value, int len = -1) {
		if (len < 0) len = strlen (value);
		return gnutls_x509_crt_set_dn_by_oid (data, oid, 0, value, len);
	}

	// necessary
	int setVersion (int version = 1) {
		return gnutls_x509_crt_set_version (data, version);
	}

	int setExpirationTime (time_t t) {
		return gnutls_x509_crt_set_expiration_time (data ,t);
	}

	int setExpirationDays (int days) {
		return setExpirationTime (::time(NULL) + 3600 * 24 * days);
	}

	// necessary.
	int setActivationTime (time_t t = ::time(NULL)) {
		return gnutls_x509_crt_set_activation_time (data, t);
	}

	// necessary
	int setSerial (int serial) {
		return gnutls_x509_crt_set_serial (data, &serial, sizeof(serial));
	}

	int setCountryName (const char * name) {
		return setDN (GNUTLS_OID_X520_COUNTRY_NAME, name);
	}

	int setOrganizationName (const char * name) {
		return setDN (GNUTLS_OID_X520_ORGANIZATION_NAME, name);
	}

	int setOrganisationUnitName (const char * name) {
		return setDN (GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME, name);
	}

	int setStateName (const char * name) {
		return setDN (GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, name);
	}

	int setLocalityName (const char * name) {
		return setDN (GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, name);
	}

	int setCommonName (const char * name) {
		return setDN (GNUTLS_OID_X520_COMMON_NAME, name);
	}
	int setName (const char * name){
		return setDN (GNUTLS_OID_X520_NAME, name);
	}

	// necessary?
	int setUid (const char * name) {
		return setDN (GNUTLS_OID_LDAP_UID, name);
	}

	int setKey (PrivateKey * key) {
		return gnutls_x509_crt_set_key (data, key->data);
	}

	int sign (Certificate * issuer, PrivateKey * issuerKey) {
		return gnutls_x509_crt_sign (data, issuer->data, issuerKey->data);
	}

	int setRequest (CertificateRequest * req) {
		return gnutls_x509_crt_set_crq (data, req->data);
	}

	int textExport (std::string * dst) {
		char buffer [10 * 1024];
		size_t bufferSize = sizeof (buffer);
		int r = gnutls_x509_crt_export (data, GNUTLS_X509_FMT_PEM, buffer, &bufferSize);
		if (!r) *dst = buffer;
		return r;
	}

	int dnTextExport (std::string * dst) {
		char buffer [10 * 1024];
		size_t bufferSize = sizeof (buffer);
		int r = gnutls_x509_crt_get_dn (data, buffer, &bufferSize);
		if (!r) *dst = buffer;
		return r;
	}

	int binaryImport (const gnutls_datum_t* buffer) {
		return gnutls_x509_crt_import (data, buffer, GNUTLS_X509_FMT_DER);
	}

	bool checkHostname (const std::string & hostname) {
		return gnutls_x509_crt_check_hostname (data, hostname.c_str()) != 0;
	}

	bool verify (const Certificate * trusted);

	gnutls_x509_crt_t data;
};

typedef shared_ptr<Certificate> CertificatePtr;
typedef shared_ptr<PrivateKey>  PrivateKeyPtr;

}
}
