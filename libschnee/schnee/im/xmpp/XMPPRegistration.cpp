#include "XMPPRegistration.h"
#include "DirectXMPPConnection.h"
#include "BoshXMPPConnection.h"

#include <schnee/settings.h>

namespace sf {

XMPPRegistration::XMPPRegistration() {
	SF_REGISTER_ME;
	mStream  = shared_ptr<XMPPStream> (new XMPPStream ());
}

XMPPRegistration::~XMPPRegistration() {
	SF_UNREGISTER_ME;
}

struct RegisterGetIq : public xmpp::Iq {
	RegisterGetIq () { type = "get"; }
	String encodeInner() const {
		return "<query xmlns='jabber:iq:register'/>";
	}
};

/// Callback for register get iq.
typedef function<void (Error result, const String & instructions, const std::vector<String> & fields)> RegisterGetCallback;

static void registerGetIqHandler (Error e, const xmpp::Iq & iq, const XMLChunk & chunk, const RegisterGetCallback & originalCallback) {
	std::vector<String> fields;
	String instructions;
	if (chunk.hasChildWithName("error")){
		return originalCallback (error::NotSupported, instructions, fields);
	}
	const XMLChunk * query = chunk.getHasChild("query");
	if (!query) {
		return originalCallback (error::BadProtocol, instructions, fields);
	}

	const XMLChunk::ChunkVector & v = query->children();
	bool foundInstructions = false;
	bool foundRedirection  = false;
	bool registered        = false;
	bool xform             = false; // not supported
	for (XMLChunk::ChunkVector::const_iterator i = v.begin(); i != v.end(); i++) {
		Log (LogInfo) << LOGID << "Blob: " << *i << std::endl;
		if (i->name() == "registered"){
			registered = true;
			break;
		}
		if (i->name() == "x" && i->ns() == "jabber:x:oob"){
			foundRedirection = true;
			break;
		}
		if (i->name() == "instructions"){
			foundInstructions = true;
			instructions = i->text();
			continue;
		}
		if (i->name() == "x" && i->ns() == "'jabber:x:data"){
			xform = true;
			break;
		}
		fields.push_back(i->name());
	}
	if (foundRedirection) {
		Log (LogProfile) << LOGID << "Server uses redirection, not supported" << std::endl;
		return originalCallback (error::NotSupported, instructions, fields);
	}
	if (xform) {
		Log (LogProfile) << LOGID << "Server uses xform, not supported" << std::endl;
		return originalCallback (error::NotSupported, instructions, fields);
	}
	if (registered) {
		return originalCallback (error::ExistsAlready, instructions, fields);
	}
	originalCallback (NoError, instructions, fields);
}


static void insertTextTagIfContentNotEmpty(std::ostringstream & ss, const char * name, const String & content) {
	if (!content.empty()) ss << "<" << name << ">" << xml::encEntities(content) << "</" << name << ">";
}
#define XMLTAG(S) insertTextTagIfContentNotEmpty(ss,#S,S);

struct RegisterSetIq : public xmpp::Iq {
	RegisterSetIq () { type = "set"; }
	String username;
	String password;
	String email;
	String encodeInner() const {
		std::ostringstream ss;
		ss << "<query xmlns='jabber:iq:register'>";
		XMLTAG (username);
		XMLTAG (password);
		XMLTAG (email);
		ss << "</query>";
		return ss.str();
	}
};

static void registerSetIqHandler (Error e, const xmpp::Iq & iq, const XMLChunk & base, const ResultCallback & callback) {
	if (!callback) return;
	if (e) return callback (e);
	if (iq.type == "error") {
		Error e = error::Other;
		const XMLChunk * errorChield = base.getHasChild("error");
		const XMLChunk * textChild   = errorChield ? errorChield->getHasChild("text") : 0;
		if (errorChield) {
			Log (LogProfile) << LOGID << "Register failed " << std::endl;
			if (textChild){
				Log (LogProfile) << LOGID << "Cause: " << textChild->text();
			}
			if (errorChield->hasChildWithName("conflict")) e = error::ExistsAlready;
			if (errorChield->hasChildWithName("not-acceptable")) e = error::NotSupported;
		}
		return callback (e);
	}
	return callback (NoError);
}

Error XMPPRegistration::start (const String & server, const Credentials & credentials, int timeOutMs, const ResultCallback & callback) {
	mCredentials    = credentials;
	mResultCallback = callback;
	mProcessing = true;
	if (schnee::settings().forceBoshXmpp){
		mConnection = XMPPConnectionPtr (new BoshXMPPConnection());
	} else {
		mConnection = XMPPConnectionPtr (new DirectXMPPConnection());
	}
	XMPPConnection::XmppConnectDetails details;
	details.username = credentials.username;
	details.server = server;
	mConnection->setConnectionDetails(details);
	Error e = mConnection->pureConnect(mStream, timeOutMs * 0.7, dMemFun (this, &XMPPRegistration::onConnect));
	if (e) return e;
	mTimeoutHandle = sf::xcallTimed(dMemFun(this, &XMPPRegistration::onTimeout), sf::regTimeOutMs(timeOutMs));
	return NoError;
}

void XMPPRegistration::onConnect (Error result) {
	if (!mProcessing) return;
	if (result) return end (result);

	RegisterGetIq iq;
	Error e = mStream->requestIq (&iq, abind (&registerGetIqHandler, dMemFun(this, &XMPPRegistration::onRegisterGet)));
	if (e) end (e);
}

void XMPPRegistration::onRegisterGet (Error result, const String & instructions, const std::vector<String> & fields) {
	Log (LogProfile) << LOGID << "RegisterGet result: " << toString(result) << std::endl;
	Log (LogProfile) << LOGID << "Instructions: " << instructions << std::endl;
	Log (LogProfile) << LOGID << "Fields: " << toString (fields) << std::endl;
	if (result) return end (result);
	if (!mProcessing) return;

	RegisterSetIq iq;
	iq.username = mCredentials.username;
	iq.password = mCredentials.password;
	iq.email    = mCredentials.email;
	Error e = mStream->requestIq (&iq, abind (&registerSetIqHandler, dMemFun (this, &XMPPRegistration::onRegisterSet)));
	if (e) end (result);
}

void XMPPRegistration::onRegisterSet (Error result) {
	Log (LogProfile) << LOGID << "RegisterSet result: " << toString (result) << std::endl;
	if (result) end (result);
	end (NoError);
}

void XMPPRegistration::onTimeout () {
	if (!mProcessing) return;
	if (mResultCallback) xcall (abind (mResultCallback, error::TimeOut));
}

void XMPPRegistration::end (Error result) {
	if (!mProcessing) return;
	mProcessing = false;
	sf::cancelTimer (mTimeoutHandle);
	xcall (dMemFun (this, &XMPPRegistration::onShutdown));
	if (mResultCallback) xcall (abind (mResultCallback, result));
}

void XMPPRegistration::onShutdown () {
	if (mStream) mStream->close();
}


}
