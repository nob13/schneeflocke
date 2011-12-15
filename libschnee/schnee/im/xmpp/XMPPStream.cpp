#include "XMPPStream.h"
#include <schnee/tools/Log.h>
#include <schnee/tools/Token.h>
#include <schnee/tools/Base64.h>

namespace sf {

XMPPStream::XMPPStream() {
	SF_REGISTER_ME;
	mState     = XMS_Null;
	mCurrentOp = XMO_Null;
	mReceivedFeatures = false;
	mSkipInit = false;;

	// XmlStreamDecoder is not asynchronous, doesn't need dMemFun
	// Calls directly into locked state.
	mXmlStreamDecoder.stateChange() = memFun (this, &XMPPStream::onXmlStreamStateChange_locked);
	mXmlStreamDecoder.chunkRead()   = memFun (this, &XMPPStream::onXmlChunkRead_locked);
}

XMPPStream::~XMPPStream() {
	SF_UNREGISTER_ME;
	mOpenIqs.clear();
	if (mChannel)
		mChannel->changed().clear ();
}

XMPPStream::State XMPPStream::state() const {
	return mState;
}

void XMPPStream::setInfo (const String & own, const String & dst) {
	mOwnJid = own;
	mDstJid = dst;
}

String XMPPStream::ownJid () const {
	return mOwnJid;
}

String XMPPStream::ownFullJid () const {
	return mFullJid;
}

String XMPPStream::dstJid () const {
	return mDstJid;
}

XMLChunk XMPPStream::features () const {
	return mFeatures;
}

Error XMPPStream::startInit (ChannelPtr channel, const ResultCallback& callback) {
	mSkipInit = false;
	Error e = init_locked (channel, false);
	if (e) return e;
	e = startOp_locked (XMO_StartInitialize, callback);
	if (e) return e;
	mState = XMS_StartInitializing;
	sendStreamInit_locked();
	return NoError;
}

Error XMPPStream::respondInit (ChannelPtr channel, const ResultCallback & callback) {
	Error e = init_locked (channel, false);
	if (e) return e;
	e = startOp_locked (XMO_RespondInitialize, callback);
	if (e) return e;
	mState = XMS_RespondInitializing;
	/// wait for opponent...
	return NoError;
}

Error XMPPStream::startInitAfterHandshake (ChannelPtr channel) {
	Error e = init_locked (channel, true);
	if (e) return e;
	mState = XMS_Initialized;
	return NoError;
}

Error XMPPStream::close () {
	Error e = NoError;
	if (!mSkipInit){
		e = send_locked ("</stream:stream>");
	}
	if (mChannel) {
		mChannel->changed().clear ();
		mChannel->close(abind (dMemFun (this, &XMPPStream::throwAwayOnClose), mChannel));
		mChannel = ChannelPtr ();
	}
	return e;
}

Error XMPPStream::waitFeatures (const ResultCallback & callback) {
	if (mReceivedFeatures) {
		// we have it already.
		if (callback) xcall (abind (callback, NoError));
		return NoError;
	}
	Error e = startOp_locked (XMO_WaitFeatures, callback);
	if (e) return e;
	return NoError;
}

static bool hasMechanism (const String & name, const XMLChunk & features) {
	XMLChunk mechanisms = features.getChild ("mechanisms");
	if (mechanisms.error()) return false;
	String nameUppered (name);
	boost::algorithm::to_upper(nameUppered);
	typedef std::vector<const XMLChunk*> Vec;
	Vec elements = mechanisms.getChildren("mechanism");
	for (Vec::const_iterator i = elements.begin(); i != elements.end(); i++) {
		const XMLChunk * chunk (*i);
		String text = chunk->text();
		boost::algorithm::to_upper (text);
		if (text == nameUppered) return true;
	}
	return false;
}

/// Constructs the secret as the PLAIN mechanism wants it.
static String plainSecret (const String & username, const String & password) {
	// construction of the Base64 secret = \0username\0password
	ByteArray secret;
	secret.push_back ('\0');
	secret.insert (secret.end(), username.begin(), username.end());
	secret.push_back ('\0');
	secret.insert (secret.end(), password.begin(), password.end());

	assert (secret.size() == username.size() + password.size() + 2);

	return Base64::encodeFromArray (secret);
}

Error XMPPStream::authenticate (const String & username, const String & password, const ResultCallback & result) {
	if (!mReceivedFeatures){
		Log (LogWarning) << LOGID << "Cannot authenticate, no stream features" << std::endl;
		return error::WrongState;
	}
	if (!hasMechanism ("PLAIN", mFeatures)){
		Log (LogWarning) << LOGID << "Authentication mechanism PLAIN not supported!" << std::endl;
		return error::NotSupported;
	}
	Error e = startOp_locked (XMO_Authenticate, result);
	if (e) return e;

	String secret = plainSecret (username, password);
	String auth = "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='PLAIN'>" + secret + "</auth>";
	e = send_locked (auth);
	if (e) return e;
	mNextChunkHandler = memFun (this, &XMPPStream::onAuthReply_locked);
	return NoError;
}

Error XMPPStream::requestTls (const ResultCallback & callback) {
	if (!mReceivedFeatures) {
		Log (LogWarning) << LOGID << "Cannot request TLS, no stream features" << std::endl;
		return error::WrongState;
	}
	if (!mFeatures.hasChildWithName("starttls")){
		Log (LogWarning) << LOGID << "Cannot request TLS, not supported by stream features" << std::endl;
		return error::NotSupported;
	}
	Error e = startOp_locked (XMO_RequestTls, callback);
	if (e) return e;
	String tlsRequest ("<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
	e = send_locked (tlsRequest);
	if (e) return e;
	mNextChunkHandler = memFun (this, &XMPPStream::onStartTlsReply_locked);
	return NoError;
}

Error XMPPStream::requestIq (xmpp::Iq * iq, const IqResultCallback & callback) {
	iq->id = generateIqId_locked ();
	String req = iq->encode();
	Error e = send_locked (req);
	if (e) return e;
	mOpenIqs[iq->id] = callback;
	return NoError;
}

void XMPPStream::uncouple () {
	mChannel->changed().clear ();
	xcall (abind (dMemFun (this, &XMPPStream::throwAwayChannel), mChannel));
	mChannel = ChannelPtr ();
}

struct BindIq : public xmpp::Iq {
	BindIq () { type =  "set"; }
	String resource;
	String encodeInner() const { return "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'><resource>" + resource + "</resource></bind>"; }
};

Error XMPPStream::bindResource  (const String & name, const BindCallback & callback) {
	BindIq iq;
	iq.resource = name;
	return requestIq (&iq, abind (dMemFun (this, &XMPPStream::onResourceBind), callback));
}

static void sessionIqResultHandler (Error e, const xmpp::Iq & iq, const XMLChunk & chunk, const ResultCallback & originalCallback) {
	if (originalCallback) originalCallback(e);
}

struct SessionIq : public xmpp::Iq {
	SessionIq () { type = "set"; }
	String encodeInner() const { return "<session xmlns='urn:ietf:params:xml:ns:xmpp-session'/>"; }
};

Error XMPPStream::startSession (const ResultCallback & callback) {
	SessionIq iq;
	// hack
	typedef function<void (Error, const xmpp::Iq &, const XMLChunk &, const ResultCallback &)> InternalHandler;
	return requestIq (&iq, abind (&sessionIqResultHandler, callback));
}

Error XMPPStream::sendPresence (const xmpp::PresenceInfo & p) {
	String dst = p.encode();
	return send_locked (dst);
}

Error XMPPStream::sendMessage (const xmpp::Message & m) {
	String dst;
	m.encode(dst);
	return send_locked (dst);
}

Error XMPPStream::sendPlainIq (const xmpp::Iq & iq) {
	String dst;
	dst = iq.encode();
	return send_locked (dst);
}

Error XMPPStream::init_locked (ChannelPtr channel, bool skipInit) {
	if (mChannel) mChannel->changed().clear();
	mError = NoError;
	mSkipInit = skipInit;
	mState = XMS_Null;
	if (mSkipInit)
		mXmlStreamDecoder.resetToPureStream();
	else
		mXmlStreamDecoder.reset();
	mReceivedFeatures = false;
	mFeatures = XMLChunk ();
	mChannel = channel;
	mChannel->changed() = dMemFun (this, &XMPPStream::onChannelChange);
	xcall (dMemFun (this, &XMPPStream::onChannelChange));
	return NoError;
}

void XMPPStream::onXmlStreamStateChange_locked () {
	XMLStreamDecoder::State s = mXmlStreamDecoder.state();
	if (s == XMLStreamDecoder::XS_ReadXmlBegin) return; // ignore
	if (s == XMLStreamDecoder::XS_ReadOpener) {
		if (mState == XMS_StartInitializing) {
			if (!decodeStreamInit_locked()) {
				finishOp_locked (XMO_StartInitialize, mError);
				return;
			}
			mState = XMS_Initialized;
			finishOp_locked (XMO_StartInitialize, NoError);
			return;
		} else
		if (mState == XMS_RespondInitializing) {
			if (!decodeStreamInit_locked()){
				finishOp_locked (XMO_RespondInitialize, mError);
				return;
			}
			sendStreamInit_locked();
			mState = XMS_Initialized;
			finishOp_locked (XMO_RespondInitialize, NoError);
		} else {
			onChannelError_locked (error::BadProtocol);
		}
	}
	if (s == XMLStreamDecoder::XS_Closed) {
		mState = XMS_End;
		if (mClosed)
			xcall (mClosed);
	}
}

void XMPPStream::onXmlChunkRead_locked (const XMLChunk & chunk) {
	if (mNextChunkHandler){
		mNextChunkHandler (chunk);
		mNextChunkHandler.clear();
		return;
	}

	if (chunk.name() == "stream:features") {
		if (mReceivedFeatures) {
			Log (LogWarning) << LOGID << "Double receive features!" << std::endl;
		}
		mFeatures         = chunk;
		mReceivedFeatures = true;
		finishOp_locked (XMO_WaitFeatures, NoError);
		return;
	} else
	if (chunk.name() == "iq") {
		xmpp::Iq iq;
		bool suc = iq.decode(chunk);
		if (!suc) {
			Log (LogWarning) << LOGID << "Could not decode " << chunk << std::endl;
			return;
		}
		String id = chunk.getAttribute("id");
		if (id.empty()){
			Log (LogWarning) << LOGID << "Received empty id in iq" << std::endl;
		}

		OpenIqMap::iterator i = mOpenIqs.find (id);
		if (i != mOpenIqs.end()){
			IqResultCallback cb = i->second;
			mOpenIqs.erase(i); // make this behaviour selectable?
			if (cb){
				notifyAsync (cb, NoError, iq, chunk);
			} else {
				Log (LogInfo) << LOGID << "Ignoring iq reply as there is no callback set (id=" << iq.id << ")" << std::endl;
			}
		} else {
			if (mIncomingIq) {
				notifyAsync (mIncomingIq, iq, chunk);
			} else {
				Log (LogWarning) << LOGID << "No iq handler set!" << std::endl;
			}
		}
	} else
	if (chunk.name() == "message") {
		xmpp::Message m;
		bool suc = m.decode(chunk);
		if (!suc) {
			Log (LogWarning) << LOGID << "Could not decode message " << chunk << std::endl;
			return;
		}
		notifyAsync (mIncomingMessage, m, chunk);
	} else
	if (chunk.name() == "presence") {
		xmpp::PresenceInfo p;
		bool suc = p.decode(chunk);
		if (!suc) {
			Log (LogWarning) << LOGID << "Could not decode presence " << chunk << std::endl;
			return;
		}
		notifyAsync (mIncomingPresence, p, chunk);
	} else
	if (chunk.name() == "stream:error"){
		String text = chunk.getChild("text").text();
		if (mIncomingStreamError){
			notifyAsync (mIncomingStreamError, text, chunk);
		} else {
			Log (LogWarning) << LOGID << "Discarding incoming stream error, no delegate " << text << std::endl;
		}
	} else {
		Log (LogWarning) << LOGID << "Unknown chunk type " << chunk << std::endl;
	}
}

void XMPPStream::onChannelError_locked (Error e) {
	mError = e;
	if (mCurrentCallback) {
		xcall (abind (mCurrentCallback,e));
	}
	if (mAsyncError) {
		xcall (mAsyncError);
	}
	mCurrentOp = XMO_Null;
	mState = XMS_Error;
}

void XMPPStream::onChannelChange   () {
	ByteArrayPtr data = mChannel->read();
	if (data && data->size() > 0){
#ifndef NDEBUG
		Log (LogInfo) << LOGID << "recv " << *data << std::endl;
#endif
		mXmlStreamDecoder.onWrite(*data);
	}
	// mXmlStreamDecoder can remove the channel by its own callbacks!
	if (mChannel && mChannel->error()) {
		onChannelError_locked (mChannel->error());
	}
}

void XMPPStream::onAuthReply_locked (const XMLChunk & chunk) {
	if (chunk.name() == "success"){
		finishOp_locked (XMO_Authenticate, NoError);
		return;
	}
	finishOp_locked (XMO_Authenticate, error::AuthError);
}

void XMPPStream::onStartTlsReply_locked (const XMLChunk & chunk) {
	if (chunk.name() == "proceed"){
		finishOp_locked (XMO_RequestTls, NoError);
		return;
	}
	Log (LogWarning) << LOGID << "Could not request TLS: " << chunk << std::endl;
	finishOp_locked (XMO_RequestTls, error::NotSupported);
}

void XMPPStream::onResourceBind (Error e, const xmpp::Iq & iq, const XMLChunk & chunk, const XMPPStream::BindCallback & originalCallback) {
	String jid = chunk.getChild ("bind").getChild("jid").text();
	if (jid.empty() && !e){
		Log (LogWarning) << LOGID << "Could not bind, got no JID" << std::endl;
	}
	if (!e) {
		Log (LogInfo) << LOGID << "Bound resource to " << mFullJid << std::endl;
		mFullJid = jid;
	}
	if (originalCallback){
		if (e) {
			return originalCallback (e, "");
		}
		originalCallback (NoError, jid);
	}
}


bool XMPPStream::decodeStreamInit_locked () {
	const XMLChunk & opener = mXmlStreamDecoder.opener();
	if (opener.name() != "stream:stream")  {
		mState = XMS_Error;
		mError = error::BadProtocol;
		return false;
	}
	if (mOwnJid.empty()) mOwnJid = opener.getAttribute("to");
	if (mDstJid.empty()) mDstJid = opener.getAttribute("from");
	return true;
}

void XMPPStream::sendStreamInit_locked () {
	String fromPart = mOwnJid.empty() ? "" : "from='" + mOwnJid + "' ";
	String toPart   = mDstJid.empty() ? "" : "to='" + mDstJid + "' ";
	String init = String ("<?xml version='1.0' encoding='utf-8'?><stream:stream xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' " + fromPart + toPart + " version='1.0'>");
	send_locked (init);
}

Error XMPPStream::send_locked (const String & content) {
	if (!mChannel) return error::NotInitialized;
#ifndef NDEBUG
	Log (LogInfo) << LOGID << "send " << content << std::endl;
#endif
	return mChannel->write (sf::createByteArrayPtr(content));
}

Error XMPPStream::startOp_locked (CurrentOp op, const ResultCallback & callback) {
	if (mCurrentOp) {
		Log (LogWarning) << LOGID << "Cannot start op " << toString (op) << ", pending op " << toString (mCurrentOp) << std::endl;
		return error::ExistsAlready;
	}
	mCurrentOp = op;
	mCurrentCallback = callback;
	return NoError;
}

void XMPPStream::finishOp_locked (CurrentOp op, Error result) {
	if (mCurrentOp == op) {
		if (mCurrentCallback) xcall (abind (mCurrentCallback, result));
		mCurrentCallback.clear();
		mCurrentOp = XMO_Null;
	}
}

void XMPPStream::throwAwayChannel (ChannelPtr channel) {
	// intentionally nothing
}

String XMPPStream::generateIqId_locked () {
	String id = sf::genRandomToken80();
	while (mOpenIqs.count(id) > 0) id = sf::genRandomToken80();
	return id;
}





}
