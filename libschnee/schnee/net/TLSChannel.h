#pragma once

#include <schnee/net/Channel.h>
#include <gnutls/gnutls.h>
#include <schnee/tools/async/DelegateBase.h>
#include "x509.h"

namespace sf {

/// Adds encryption functionality to an existing channel (Decorator)
class TLSChannel : public Channel, public DelegateBase {
public:

	TLSChannel (ChannelPtr next);
	virtual ~TLSChannel ();

	enum Mode { DH, X509 };

	/// Set key/cert for X509 Encryption (necessary for servers!)
	void setKey  (const x509::CertificatePtr & cert, const x509::PrivateKeyPtr& key);

	Error clientHandshake (Mode mode, const ResultCallback & callback = ResultCallback());
	Error serverHandshake (Mode mode, const ResultCallback & callback = ResultCallback());

	/// Simple x509 authentication; validating the certificate
	Error authenticate (const x509::Certificate * trusted, const String & hostName);

	/// Returns the main peer certificate
	x509::CertificatePtr peerCertificate () const;

	// Implementation of Channel
	virtual sf::Error error () const;
	virtual sf::String errorMessage () const;
	virtual State state () const;
	virtual Error write (const ByteArrayPtr& data, const ResultCallback & callback = ResultCallback());
	virtual sf::ByteArrayPtr read (long maxSize = -1);
	virtual void close (const ResultCallback & callback);
	virtual ChannelInfo info () const;
	virtual const char * stackInfo () const { return "tls";}
	const ChannelPtr next () const { return mNext; }
	virtual sf::VoidDelegate & changed ();

	// Base class for all encryption data (for cleanup)
	struct EncryptionData {
		typedef gnutls_session_t Session;
		virtual ~EncryptionData() {}
		/// Applys settings to a GnuTLS session
		virtual int apply (Session s) = 0;
	};
	typedef shared_ptr <EncryptionData> EncryptionDataPtr;

private:
	/// Start handshake, once encryption data is set
	Error startHandshake (Mode m, const ResultCallback & callback, bool server, const char * type);

	void freeTlsData ();
	/// Set Transport (Functions) to GnuTLS
	void setTransport();
	/// Continue a handshaking process
	void continueHandshake ();
	/// Continue reading process
	void continueReading ();

	void onChanged ();
	VoidDelegate mChanged;
	ChannelPtr mNext;

	gnutls_session_t mSession; //< TLS session
	EncryptionDataPtr mEncryptionData;
	x509::CertificatePtr mCert;
	x509::PrivateKeyPtr  mKey;

	bool            mSecured;	///< Did handhskaing
	bool            mHandshaking;
	bool            mServer;	///< Acts as a server during handshake
	bool            mAuthenticated;
	Mode            mMode;
	ResultCallback  mHandshakeCallback;
	ResultCallback  mCurrentWriteCallback;
	Error           mHandshakeError;

	// Adapters for GnuTLS
	static ssize_t c_push     (gnutls_transport_ptr instance, const void * data, size_t size);
	static ssize_t c_pull     (gnutls_transport_ptr instance, void * data, size_t size);
	const char *    functionalityName () { return mServer ? "Server" : "Client"; }
};

typedef shared_ptr<TLSChannel> TLSChannelPtr;

}
