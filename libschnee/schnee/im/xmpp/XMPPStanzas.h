#pragma once

/* \file XMPPStanzas.h
 * 
 * Collection of different XMPP stanzasa altogether with debug output and parsing routine
 * 
 * They are in their own namespace, because their names like Message collide with a other classes.
 * 
 * Used by XMPPDispatcher
 * 
 */ 

// @cond DEV

#include <schnee/tools/XMLChunk.h>
#include <schnee/tools/Log.h>
#include <schnee/im/IMClient.h>
#include <schnee/sftypes.h>
#include <sfserialization/autoreflect.h>

#include <vector>
namespace sf {
namespace xmpp {

/// Decodes an IMClient-Stanza; returns true on success
bool decodeStanza (IMClient::Stanza & stanza, const XMLChunk & elem);

/** A message we received about the presence of someone
 * 
 * Presence Messages look like this:
 * <presence from='enob2@shodan/shodan' to='enob1@shodan/schneeflocke' xml:lang='en'>
 *	<show>chat</show>
 *  <status>bla</status>
 *  <priority>5</priority>
 *  <c xmlns='http://jabber.org/protocol/caps' node='http://psi-im.org/caps' ver='0.12.1' ext='cs ep-notify html'/>
 * </presence>
 * 
 */
struct PresenceInfo : public IMClient::Stanza {
	PresenceInfo () : priority (0), state (IMClient::PS_UNKNOWN) {}
	bool decode (const XMLChunk&);
	virtual String encode() const;

	int     priority; 			    ///< priority of the client (0 is either 0 or not set)
	IMClient::PresenceState state;  ///< Which kind of presence
	String status;					///< Status message of the presence
	String type;					///< Presence type
	String errorType;				///< Error type (if set)
	String errorText;				///< Error message (if set)
	SF_AUTOREFLECT_SERIAL;
};


/// An iq stanza
struct Iq : public IMClient::Stanza {
	virtual ~Iq () {}
	String id;	/// id of the Iq query
	String type;
	bool decode (const XMLChunk &);
	virtual String encode () const;
	// Encode the inner part between <iq> and </iq>
	virtual String encodeInner () const { return "";}
	SF_AUTOREFLECT_SERIAL;
};

struct ErrorIq : public Iq {
	ErrorIq () {
		type = "error";
	}
	virtual ~ErrorIq () {}

	String errorCondition; ///< Must be full qualified XML or empty
	String errorType;      ///< Must be a word

	// overload
	virtual String encodeInner () const;
};

/// A roster Iq.
struct RosterIq : public Iq {
	struct Item {
		Item () : subscription (IMClient::SS_NONE), remove (false), waitForSubscription (false) {}
		String jid;
		String name;
		IMClient::SubscriptionState subscription;
		bool remove; 				// item is to be removed
		bool waitForSubscription;	// waiting for peer to accept subscription
		SF_AUTOREFLECT_SERIAL;
	};
	std::vector <Item> items;
	bool decode (const XMLChunk &);
	SF_AUTOREFLECT_SERIAL;
};

/// Iq as used in disco info.
struct DiscoInfoIq : public Iq {
	std::vector<String> features;
	String clientType;
	String clientName;
	bool decode (const XMLChunk &);
	virtual String encode () const;
	SF_AUTOREFLECT_SERIAL;
};

/**
 * A data tag like defined in http://xmpp.org/extensions/xep-0231.html
 */
struct DataBlock {
	int      age;  		///< How long shall it be cached (non-defined = -1)
	String  cid;  		///< Content id (recommended: algo+hash@bob.xmpp.org, e.g. sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org
	String  type; 		///< MIME type of data
	ByteArray data;	///< The encoded data (was Base64 but got already decoded)
	SF_AUTOREFLECT_SERIAL;
};
typedef shared_ptr<DataBlock> DataBlockPtr;

/**
 * The message stanza, base for all IM
 * 
 * Types of messages are defined in RFC 3921 (http://tools.ietf.org/html/rfc3921#page-4)
 * 
 * * Small amounts of data are supported - see XEP-00231 (http://xmpp.org/extensions/xep-0231.html)
 * 
 * TODO: 
 * * Support for chatstate (XEP-0085) http://xmpp.org/extensions/xep-0085.html
 * * Support for formated message (XEP-0071)
 */
struct Message : public IMClient::Message {
	Message () {}
	Message (const IMClient::Message & msg) { IMClient::Message::operator= (msg); }
	bool decode (const XMLChunk &);
	void encode (String & target) const;
	
	String thread;		///< machine readable for tracking conversation, must be unique through conversation
	DataBlockPtr data; ///< Messages may contain data (if not isNull())
	SF_AUTOREFLECT_SERIAL;
};

void serialize (Serialization & s, const DataBlockPtr & block);

}
}

/// @endcond DEV

