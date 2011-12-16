#pragma once
#include <schnee/net/http/HttpContext.h>
#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_print.hpp>
#include <schnee/net/Channel.h>

///@cond DEV

namespace sf {

/// Manages the transport of generic XML entities to a BOSH
/// capable server.
/// See http://xmpp.org/extensions/xep-0124.html
class BoshTransport : public Channel, public DelegateBase {
public:
	BoshTransport ();
	virtual ~BoshTransport ();

	typedef std::map<String, String> StringMap;

	/// Connects to the BOSH entity
	/// Already set: rid, wait, hold, xmlns etc.
	/// You can overwrite all values through additionalArgs
	/// Note: no security mechanisms implemented, use https!
	void connect (const Url & url, const StringMap & additionalArgs, int timeOutMs, const ResultCallback & callback);

	/// Send restart request (e.g. after Authentication)
	/// It will include the following XMPP specific attributes:
	/// xmpp:restart='true'
    /// xmlns:xmpp='urn:xmpp:xbosh'/>
	void restart ();

	// Implementation of Channel
	virtual sf::Error error () const { return mError; }
	virtual sf::String errorMessage () const { return mErrorMessage; }
	virtual State state () const { return mState;}
	virtual Error write (const ByteArrayPtr& data, const ResultCallback & callback = ResultCallback());
	virtual sf::ByteArrayPtr read (long maxSize = -1);
	virtual void close (const ResultCallback & callback = ResultCallback ());
	virtual ChannelInfo info () const;
	virtual const char * stackInfo () const;
	virtual sf::VoidDelegate & changed () { return mChanged; }


private:
	/// First reply on connect
	void onConnectReply (Error result, const HttpResponsePtr & response, const ResultCallback & callback);
	void failConnect (Error result, const ResultCallback & callback, const String & msg);

	/// Issues new requests
	void continueWorking ();

	/// Gets called periodically in order to get new data
	void startNextRequest (const StringMap & additionalArgs = StringMap (), const ResultCallback & callback = ResultCallback());
	void onRequestReply (Error result, const HttpResponsePtr & response, int64_t rid, const ResultCallback & originalCallback);
	/// Insert data which had to be hold until earlier data arrived
	void insertWaitingInputData ();
	void failRequest (Error result, int64_t rid, const String & msg, const ResultCallback & originalCallback);

	/// Packs data into a POST request and sends it
	void executeRequest (const ByteArrayPtr & data, int timeOutMs, const HttpContext::RequestCallback & callback);

	std::deque<ByteArrayPtr> mOutputBuffer; // Output Buffer
	ByteArray   mInputBuffer; // Input Buffer, in order

	typedef std::map<int64_t, ByteArrayPtr> RidDataMap;
	RidDataMap mOpenRids; // yet not answered regular requests
	RidDataMap mInWaitingRids; // Incoming data

	int mOpenRidCount; // = |mOpenRids|

	int64_t     mRid;	  ///< Last RID sent
	int64_t     mRidRecv; ///< Next RID to be received
	String      mSid;
	State       mState;

	Url         mUrl;
	int         mLongPollTimeoutMs;
	int         mReconnectWaitMs; // wait between immediately reconnecting
	HttpContext mContext;

	Error        mError;
	String       mErrorMessage;
	VoidDelegate mChanged;

	// Error correction
	int          mErrorCount;    //< HTTP errors (gets decreased on each successfull attempt)
	int          mSumErrorCount; //< Sum of all HTTP errors
	int          mSuccessCount;  //< HTTP success (doesn't mean core XML protocol was valid)
	int          mMaxErrorCount; //< Maximum HTTP errors
};
typedef shared_ptr<BoshTransport> BoshTransportPtr;


}

///@endcond DEV
