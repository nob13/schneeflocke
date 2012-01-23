#pragma once
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/im/xmpp/XMLStreamDecoder.h>
#include <schnee/im/xmpp/XMPPStanzas.h>
#include <schnee/net/Channel.h>

namespace sf {

/// Decodes an XMPP stream and implements XMPP's state machine
class XMPPStream : public DelegateBase {
public:
	XMPPStream ();
	~XMPPStream ();

	enum State {
		XMS_Null,
		XMS_StartInitializing,
		XMS_RespondInitializing,
		XMS_Initialized,
		XMS_End,
		XMS_Error
	};

	enum CurrentOp {
		XMO_Null,
		XMO_StartInitialize,
		XMO_RespondInitialize,
		XMO_WaitFeatures,
		XMO_Authenticate,
		XMO_RequestTls
	};

	/// Access to current state
	State state () const;

	/// Sets info about own / other identity. If not set,
	/// The XMPPStream tries to negotiate them
	void setInfo (const String & own, const String & dst = "");

	/// Returns set/negotiated own jid
	String ownJid () const;

	/// Returns own full jid (after resource binding)
	String ownFullJid () const;

	/// returns set/negotiated destination jid;
	String dstJid () const;

	/// Features of the stream
	XMLChunk features () const;

	/// Returns channel info
	Channel::ChannelInfo channelInfo() const;

	///@name Connecting Sequence
	///@{

	/// Begins connecting someone
	Error startInit (ChannelPtr channel, const ResultCallback& callback = ResultCallback());

	/// Responds to a connection attempt
	Error respondInit (ChannelPtr channel, const ResultCallback & callback = ResultCallback());

	/// Associate to connection, but with handshake already done (like BOSH)
	Error startInitAfterHandshake (ChannelPtr channel);

	/// Closes the stream
	Error close ();

	/// Wait for stream features
	Error waitFeatures (const ResultCallback & callback);

	/// Authenticates to the other side (need features)
	Error authenticate (const String & username, const String & password, const ResultCallback & callback);

	/// Requests establishment of TLS to the other site
	Error requestTls (const ResultCallback & callback);

	typedef function <void (Error result, const xmpp::Iq & iq, const XMLChunk & base)> IqResultCallback;

	/// Sends an Iq (automatically sets the id!)
	/// Calls you back on response
	Error requestIq (xmpp::Iq * iq, const IqResultCallback & callback = IqResultCallback());

	/// Uncouples from transport channel callbacks
	void uncouple ();

	/// @}

	///@name Convenience Iqs
	///@{
	/// Binds a resource

	typedef function<void (Error result, const String & fullJid)> BindCallback;
	Error bindResource  (const String & name, const BindCallback & callback);

	/// Binds a name
	Error startSession (const ResultCallback & callback);

	///@}

	///@name Sending
	///@{

	/// Send a Presence Stanza
	Error sendPresence (const xmpp::PresenceInfo & p);

	/// Send a message
	Error sendMessage (const xmpp::Message & m);

	/// Send a plain iq stanza (e.g. a result)
	/// No tracking will be started
	Error sendPlainIq (const xmpp::Iq & iq);

	///@}

	///@name Delegates
	///@{

	typedef function<void (const xmpp::Message & m,      const XMLChunk & base)> MessageDelegate;
	typedef function<void (const xmpp::PresenceInfo & p, const XMLChunk & base)> PresenceDelegate;
	typedef function<void (const xmpp::Iq & iq,          const XMLChunk & base)> IqDelegate;
	typedef function<void (const String & text, const XMLChunk & base)> StreamErrorDelegate;

	/// A message flow in
	MessageDelegate  & incomingMessage ()  { return mIncomingMessage; }

	/// A presence flow in
	PresenceDelegate & incomingPresence () { return mIncomingPresence; }

	/// An - not bound - iq flow in
	IqDelegate &       incomingIq () { return mIncomingIq; }

	/// Received an stream error
	StreamErrorDelegate& incomingStreamError () { return mIncomingStreamError; }

	/// There is an error
	VoidDelegate & asyncError () { return mAsyncError; }

	/// Stream closed
	VoidDelegate & closed () { return mClosed; }
	///@}
private:
	/// Common parts of init
	Error init (ChannelPtr channel, bool skipInit);

	void onXmlStreamStateChange ();
	void onXmlChunkRead (const XMLChunk & chunk);
	void onChannelError (Error e);
	void onChannelChange   ();
	void onAuthReply     (const XMLChunk & chunk);
	void onStartTlsReply (const XMLChunk & chunk);
	void onResourceBind (Error e, const xmpp::Iq & iq, const XMLChunk & chunk, const XMPPStream::BindCallback & originalCallback);

	/// Decodes stream opener, returns true if successfull
	/// On Error the error state will be set and the init callback called.
	bool decodeStreamInit ();
	void sendStreamInit ();
	Error send (const String & content);

	/// An iq timeouted
	void onTimeoutIq (const String & id);

	/// Starts a new async op
	Error startOp (CurrentOp op, const ResultCallback & callback);
	/// Finish current operation (if its op), sets mCurrentOp to null, calls back
	void finishOp (CurrentOp op, Error result);

	/// some channels (TLSChannel) do not like to be kicked while delegating
	/// so we do it asyncrhonous with this function
	/// TODO: Fix TLSChannel?
	void throwAwayOnClose (Error result, ChannelPtr channel) { return throwAwayChannel (channel); }
	void throwAwayChannel (ChannelPtr channel);

	ChannelPtr mChannel;
	String mOwnJid;		 		///< Own XMPP's id
	String mFullJid;			///< Own full jid
	String mDstJid;		 		///< Destinations Id
	Error  mError;		 		///< Error code
	bool   mReceivedFeatures; 	///< Received Features yet.
	bool   mSkipInit;			///< Skip init receiving (BOSH)
	XMLChunk mFeatures;			///< Received Features

	VoidDelegate          mAsyncError;    ///< Got some error
	VoidDelegate          mClosed;
	typedef function<void (XMLChunk)> ChunkHandler;
	ChunkHandler          mNextChunkHandler; ///< Can change handling of chunks

	MessageDelegate  	mIncomingMessage;
	PresenceDelegate 	mIncomingPresence;
	IqDelegate       	mIncomingIq;
	StreamErrorDelegate mIncomingStreamError;


	State     mState;
	CurrentOp mCurrentOp;
	ResultCallback   mCurrentCallback;			///< Callback for current operation
	XMLStreamDecoder mXmlStreamDecoder;

	String generateIqId ();

	typedef std::map<String, IqResultCallback> OpenIqMap;
	OpenIqMap mOpenIqs;
};

SF_AUTOREFLECT_ENUM (XMPPStream::CurrentOp);
SF_AUTOREFLECT_ENUM (XMPPStream::State);

}
