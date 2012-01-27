#include "TLSChannel.h"
#include "TLSCertificates.h"
#include <schnee/tools/Log.h>
#include <gnutls/gnutls.h>
#include <gcrypt.h>

#ifdef WIN32
#include "errno.h"
#define EBADFD EBADF
#endif


namespace sf {

#define CHECK(X) { int r = X; if (r) {\
	Log (LogError) << LOGID << #X << " failed, err = " << gnutls_strerror_name(r) << std::endl;\
	return error::TlsError;\
} }

struct ServerDiffieHellman : public TLSChannel::EncryptionData {
	ServerDiffieHellman () {
		gnutls_anon_allocate_server_credentials (&credentials);
		gnutls_dh_params_init (&params);
		gnutls_dh_params_generate2 (params, 1024);
		gnutls_anon_set_server_dh_params (credentials, params);

	}
	~ServerDiffieHellman () {
		gnutls_anon_free_server_credentials (credentials);
		gnutls_dh_params_deinit (params);
	}
	virtual int apply (Session s) {
		return gnutls_credentials_set (s, GNUTLS_CRD_ANON, credentials);
	}

	gnutls_dh_params_t params;
	gnutls_anon_server_credentials_t credentials;
};

struct ClientDiffieHellman : public TLSChannel::EncryptionData {
	ClientDiffieHellman() {
		gnutls_anon_allocate_client_credentials (&credentials);
	}
	~ClientDiffieHellman() {
		gnutls_anon_free_client_credentials (credentials);
	}
	virtual int apply (Session s) {
		return gnutls_credentials_set (s, GNUTLS_CRD_ANON, credentials);
	}

	gnutls_anon_client_credentials_t credentials;
};

struct X509EncryptionData : public TLSChannel::EncryptionData {
	X509EncryptionData() {
		gnutls_certificate_allocate_credentials (&credentials);
	}
	~X509EncryptionData(){
		gnutls_certificate_free_credentials (credentials);
	}

	static int cert_callback (gnutls_session_t session,
		       const gnutls_datum_t * req_ca_rdn,
		       int nreqs,
		       const gnutls_pk_algorithm_t * sign_algos, int sign_algos_length,
		       gnutls_retr_st * st) {
		TLSChannel * instance = static_cast<TLSChannel*> (gnutls_session_get_ptr (session));
		st->type = gnutls_certificate_type_get (session);
		st->ncerts = 0;
		if (st->type == GNUTLS_CRT_X509){
#if GNUTLS_VERSION_NUMBER >= 0x020a00 // gnutls_sign_algorithm_get_requested is available on >= 2.10
			{
				// Check if server accepts signature algorithm
				int ret = gnutls_x509_crt_get_signature_algorithm (instance->cert()->data);
				if (ret < 0) {
					Log (LogError) << LOGID << "Invalid signature algorithm" << std::endl;
					return -1; // invalid cert signature algorithm
				}
				gnutls_sign_algorithm_t certAlgorithm = static_cast<gnutls_sign_algorithm_t> (ret);
				bool foundOne = false;
				for (int i = 0;; i++) {
					gnutls_sign_algorithm_t requestedAlgorithm;
					int ret = gnutls_sign_algorithm_get_requested (session, i, &requestedAlgorithm);
					if (ret == 0 && requestedAlgorithm == certAlgorithm) { foundOne = true; break; }
					if (i == 0 && ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) { foundOne = true; break; } // don't care
					if (ret < 0) break;
				}
				if (!foundOne) {
					Log (LogWarning) << LOGID << "Do not have a signature algorithm for the server!" << std::endl;
					return -1;
				}
			}
#endif

			// Putting in our key / cert.
			st->cert.x509   = &(instance->cert()->data);
			st->ncerts = 1;
			st->key.x509    = instance->key()->data;
			st->deinit_all = 0;
			return 0;
		}
		Log (LogWarning) << LOGID << "Wrong cert type requested?!" << std::endl;
		return -1; // error
	}

	virtual int apply (Session s) {
		return gnutls_credentials_set (s, GNUTLS_CRD_CERTIFICATE, credentials);
	}

	Error setKey (const x509::CertificatePtr& cert, const x509::PrivateKeyPtr& key){
		this->cert = cert;
		this->key  = key;
		CHECK (gnutls_certificate_set_x509_key (credentials, &cert->data, 1, key->data));

		// install callback for client sending server a certificate even if it doesn't match his CA's
		// see http://comments.gmane.org/gmane.network.gnutls.general/145
		gnutls_certificate_client_set_retrieve_function (credentials, &X509EncryptionData::cert_callback);
		return NoError;
	}

	x509::CertificatePtr cert;
	x509::PrivateKeyPtr  key;
	gnutls_certificate_credentials_t credentials;
};

TLSChannel::TLSChannel (ChannelPtr next) {
	SF_REGISTER_ME;
	mNext            = next;
	mNext->changed() = dMemFun (this, &TLSChannel::onChanged);
	mHandshaking     = false;
	mSecured         = false;
	mServer          = false;
	mAuthenticated   = false;
	mHandshakeError  = NoError;
	mDisableAuthentication = false;

	mSession = 0;
}

TLSChannel::~TLSChannel () {
	SF_UNREGISTER_ME;
	mNext.reset();
	freeTlsData();
}

void TLSChannel::setKey  (const x509::CertificatePtr & cert, const x509::PrivateKeyPtr& key) {
	mCert = cert;
	mKey  = key;
}

Error TLSChannel::clientHandshake (Mode mode, const String & hostname, const ResultCallback & callback) {
	if (mSecured) return error::WrongState;
	const char * prios = 0;
	switch (mode) {
		case DH:
			mEncryptionData = EncryptionDataPtr (new ClientDiffieHellman());
			prios = "PERFORMANCE:+ANON-DH";
			break;
		case X509:{
			X509EncryptionData * data = new X509EncryptionData;
			if (mKey && mCert)
				data->setKey (mCert,mKey);
			mEncryptionData = EncryptionDataPtr (data);
			mHostname = hostname;
			prios = "PERFORMANCE";
			break;
		}
		default:
			assert (!"Shall not come here");
		break;
	}
	return startHandshake (mode, callback, false, prios);
}

Error TLSChannel::serverHandshake (Mode mode, const ResultCallback & callback) {
	if (mSecured) return error::WrongState;
	const char * prios = 0;
	switch (mode) {
		case DH:
			mEncryptionData = EncryptionDataPtr (new ServerDiffieHellman());
			prios = "PERFORMANCE:+ANON-DH";
			break;
		case X509:{
			if (!mKey || !mCert) {
				Log (LogError) << LOGID << " Server X509 needs key/cert!" << std::endl;
				return error::InvalidArgument;
			}
			X509EncryptionData * data = new X509EncryptionData;
			data->setKey(mCert, mKey);
			mEncryptionData = EncryptionDataPtr (data);
			prios = "PERFORMANCE";
			break;
		}
		default:
			assert (!"Shall not come here");
		break;
	}
	return startHandshake (mode, callback, true, prios);
}

void TLSChannel::disableAuthentication() {
	mDisableAuthentication = true;
}

Error TLSChannel::authenticate (const x509::Certificate*  trusted, const String & hostName) {
	x509::CertificatePtr peerCert = peerCertificate();
	if (!peerCert) return error::AuthError;
	String dn;
	if (!peerCert->dnTextExport(&dn)){
		Log (LogInfo) << LOGID << "Peer DN: " << dn << std::endl;
	}
	if (!peerCert->checkHostname(hostName)){
		return error::AuthError;
	}
	if (!peerCert->verify(trusted)){
		return error::AuthError;
	}
	mAuthenticated = true;
	return NoError;
}

Error TLSChannel::authenticate (const String & hostName) {
	x509::CertificatePtr peerCert = peerCertificate();
	if (!peerCert) return error::AuthError;
	String dn;
	if (!peerCert->dnTextExport(&dn)){
		Log (LogInfo) << LOGID << "Peer DN: " << dn << std::endl;
	}
	if (!peerCert->checkHostname(hostName)){
		return error::AuthError;
	}
	if (!peerCert->verify(TLSCertificates::instance())){
		return error::AuthError;
	}
	mAuthenticated = true;
	return NoError;
}

x509::CertificatePtr TLSChannel::peerCertificate () const {
	if (!mSecured) return x509::CertificatePtr();
	if (mMode != X509) return x509::CertificatePtr();
	if (gnutls_certificate_type_get(mSession) != GNUTLS_CRT_X509){
		assert (!"?");
		return x509::CertificatePtr ();
	}
	const gnutls_datum_t * cert_list;
	unsigned int cert_list_size;
	cert_list = gnutls_certificate_get_peers (mSession, &cert_list_size);
	if (!cert_list) {
		Log (LogWarning) << LOGID << "No peer certificate or error" << std::endl;
		return x509::CertificatePtr();
	}
	x509::CertificatePtr result (new x509::Certificate());
	if (result->binaryImport (&cert_list[0])) {
		Log (LogWarning) << LOGID << "Could not decode peer certificate" << std::endl;
		return x509::CertificatePtr();
	}
	return result;
}

sf::Error TLSChannel::error () const {
	if (mHandshakeError) return mHandshakeError;
	return mNext->error();
}

sf::String TLSChannel::errorMessage () const {
	return mNext->errorMessage();
}

Channel::State TLSChannel::state () const {
	return mNext->state();
}

Error TLSChannel::write (const ByteArrayPtr& data, const ResultCallback & callback) {
	if (!mSecured) {
		return error::NotInitialized;
	}
	ByteArrayPtr d (data);
	ssize_t size = 0;

	/* TLS just accepts up to 16384 bytes per record.
	 * At the same time we want a callback if all data has been sent (for flow control)
	 * So we sent the first chunks without these callback, and put it on the last
	 * chunk.
	 */
	while (d->size() > 16384) {
		size = gnutls_record_send (mSession, data->const_c_array(), 16384);
		if (size == 0) {
			Log (LogWarning) << LOGID << "Bad, 0 sent" << std::endl;
		}
		if (size < 0) {
			Log (LogInfo) << LOGID << "Write error " << strerror (errno) << std::endl;
			return error::WriteError;
		}
		if (size > ((ssize_t) d->size())) {
			assert (!"May not happen");
			return error::WriteError;
		}
		d->l_truncate((size_t) size);
	}
	// Sending last chunk:
	mCurrentWriteCallback = callback;
	size = gnutls_record_send (mSession, d->const_c_array(), d->size());
	mCurrentWriteCallback = ResultCallback ();
	if (size < 1) {
		Log (LogInfo) << LOGID << "TLS write failed: " << strerror (errno) << std::endl;
		return error::WriteError;
	}
	if (size > (ssize_t) data->size()){
		Log (LogError) << LOGID << "Something serious is wrong" << std::endl;
		return error::WriteError;
	}
	if (size < (ssize_t) data->size()) {
		Log (LogInfo) << LOGID << "Could not send it all, rest size " << d->size() << " got " << size << std::endl;
		return error::WriteError;
	}
	return NoError;
}

sf::ByteArrayPtr TLSChannel::read (long maxSize) {
	const size_t bufSize = 65536;
	char buffer [bufSize];
#ifdef WIN32
	size_t len = maxSize < 0 ? bufSize : min (bufSize, (size_t)maxSize);
#else
	size_t len = maxSize < 0 ? bufSize : std::min (bufSize, (size_t)maxSize);
#endif

	ByteArrayPtr result = createByteArrayPtr();
	while (len > 0) {
		ssize_t recv = gnutls_record_recv (mSession, buffer, len);
		// Log (LogInfo) << LOGID << "Read " << recv << " cleartext bytes of " << len << " bytes requested" << std::endl;
		if (recv == 0) break; // Nothing more to read.
		if (recv < 0) {
			if (recv != GNUTLS_E_AGAIN)
				Log (LogWarning) << LOGID << "Could not read from TLS channel due error " << gnutls_strerror_name (recv) << std::endl;
			break;
		}
		len -= recv;
		result->append(buffer, recv);
	}
	return result;
}

void TLSChannel::close (const ResultCallback & callback) {
	if (mNext) mNext->close(callback);
	else
		notifyAsync (callback, NoError);
}

Channel::ChannelInfo TLSChannel::info () const {
	ChannelInfo info = mNext->info();
	if (!info.encrypted) {
		info.encrypted = mSecured;
	}
	if (!info.authenticated) {
		info.authenticated = mAuthenticated;
	}
	return info;
}

sf::VoidDelegate & TLSChannel::changed () {
	return mChanged;
}

Error TLSChannel::startHandshake (Mode m, const ResultCallback & callback, bool server, const char * type) {
	freeTlsData ();
	mHandshakeCallback = callback;
	CHECK (gnutls_init (&mSession, server ? GNUTLS_SERVER : GNUTLS_CLIENT));
	CHECK (gnutls_priority_set_direct (mSession, type, NULL));
	gnutls_session_set_ptr (mSession, this);
	CHECK (mEncryptionData->apply(mSession));
	if (m == X509 && server){
		// request certificate from the client
		// otherwise he would never send it
		gnutls_certificate_server_set_request (mSession, GNUTLS_CERT_REQUEST);
	}
    setTransport ();

    mHandshaking = true;
    mServer = server;
    mMode   = m;
    xcall (dMemFun (this, &TLSChannel::continueHandshake));
    return NoError;
}


void TLSChannel::freeTlsData () {
	if (mSession) {
		gnutls_deinit (mSession);
		mSession = 0;
	}
}

void TLSChannel::setTransport() {
	gnutls_transport_set_push_function (mSession, &TLSChannel::c_push);
	// Note: new GnuTLS variants have a vec_push method
	// But not yet available on development machine
	// gnutls_transport_set_vec_push_function (mSession, &TLSChannel::c_vec_push);
	gnutls_transport_set_pull_function  (mSession, &TLSChannel::c_pull);
	gnutls_transport_set_ptr (mSession, this);
}

void TLSChannel::continueHandshake () {
	Error result = NoError;
	if (!mHandshaking) {
		return;
	}
	int r = gnutls_handshake (mSession);
	if (!r) {
		mSecured     = true;
		if (!mDisableAuthentication && mMode == X509 && !mServer) {
			result = authenticate (mHostname);
		} else {
			result = NoError;
		}
	} else if (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED){
		// Async, do something else
		return;
	} else {
		// fail
		Log (LogWarning) << LOGID << functionalityName() << "Handshaking failed due " << gnutls_strerror_name (r) << std::endl;
		result = error::TlsError;
		mHandshakeError = result;
	}
	mHandshaking = false;
	Log (LogInfo) << LOGID << "Handshaking result: " << toString (result) << std::endl;
	if (mHandshakeCallback) mHandshakeCallback (result);
}

void TLSChannel::onChanged() {
	{
		if (mHandshaking) {
			xcall (dMemFun (this, &TLSChannel::continueHandshake));
		}
	}
	if (mChanged) mChanged ();
}

/*static*/ ssize_t TLSChannel::c_push   (gnutls_transport_ptr instance, const void * data, size_t size) {
	TLSChannel * _this = static_cast<TLSChannel*> (instance);
	if (!_this->mNext) {
		gnutls_transport_set_errno (_this->mSession, EBADFD);
		return -1;
	}
	Error e = _this->mNext->write(ByteArrayPtr (new ByteArray ((const char*)data, size)), _this->mCurrentWriteCallback);
	if (e) {
		Log (LogInfo) << LOGID << "Returning BADFD in sent" << std::endl;
		gnutls_transport_set_errno (_this->mSession, EBADFD);
		return -1;
	}
	return size;
}

/*static*/ ssize_t TLSChannel::c_pull   (gnutls_transport_ptr instance, void * data, size_t size) {
	TLSChannel * _this = static_cast<TLSChannel*> (instance);
	if (!_this->mNext){
		gnutls_transport_set_errno (_this->mSession, EBADFD);
		return -1;
	}
	ByteArrayPtr block = _this->mNext->read(size);
	if (!block || block->size() == 0) {
		// Check for EOF
		if (_this->mNext->error() == error::Eof){
			Log (LogInfo) << LOGID << "Forwarding Eof, nothing to read" << std::endl;
			return 0;
		}
		// Real error
		if (_this->mNext->error()) {
			Log (LogInfo) << LOGID << "Forwarding error" << toString(_this->mNext->error()) << std::endl;
			gnutls_transport_set_errno (_this->mSession, EBADFD);
			return -1;
		}
		// Nothing to read...
		// Log (LogInfo) << LOGID << "Forwarding EAGAIN" << std::endl;
		gnutls_transport_set_errno (_this->mSession, EAGAIN); // or EWOULDBLOCk
		return -1;
	}
	memcpy (data, block->const_c_array(), block->size());
	return block->size();
}

}
