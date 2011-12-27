#pragma once
#include <schnee/net/x509.h>

namespace sf {

/// Stores authentication information
/// and controls authentication subsystem
/// Stores:
/// - Certificates for other peers (CT_PEER)
/// - Certificates for XMPP hosts (CT_HUB)
/// - Own certificate and private key
/// TODO: Not yet ready
class Authentication {
public:
	enum CertType {
		CT_HUB,
		CT_PEER,
		CT_ERROR
	};

	struct CertInfo {
		CertInfo () : type (CT_ERROR) {}
		CertType type;
		x509::CertificatePtr cert;
		String fingerprint () const;
	};

	Authentication ();

	/// Sets own identity name
	/// Must be called before enablation
	/// Must fit to own key
	void setIdentity (const String & identity);

	/// Checks authentication enabled state
	bool isEnabled () const { return mEnabled; }

	/// Sets authentication to enabled
	/// If no key was assigned so far
	/// one will be generated here
	void enable (bool v = true);

	/// Access to private key
	const x509::PrivateKeyPtr & key () const { return mKey; }
	/// Access to own certificate
	const x509::CertificatePtr & certificate () const { return mCertificate; }

	/// Store some certificate for later use
	void update (const String & identity, const CertInfo& info);

	/// Returns some certificate (certificate is CT_ERROR if not found)
	CertInfo get (const String & identity);

	/// Fingerprint of own cert
	const String& certFingerprint () const;

private:
	x509::PrivateKeyPtr mKey;
	x509::CertificatePtr mCertificate;

	typedef std::map<std::string, CertInfo> CertMap;
	CertMap mCerts; ///< Stores all certificates

	bool mEnabled;
	bool mKeySet;
	String mIdentity;
	String mCertFingerprint;
};

SF_AUTOREFLECT_ENUM (Authentication::CertType);

}
