#pragma once

#include <schnee/sftypes.h>
#include <schnee/tools/ByteArray.h>
#include <schnee/Error.h>
#include <schnee/tools/Serialization.h>
#include <schnee/tools/Deserialization.h>
#include <sfserialization/autoreflect.h>
/**
 * @file
 * Protocol Elements for the DataSharing Protocol.
 */

namespace sf {

/// A short Namespace in which all Protocol Elements for DataSharing are living
namespace ds {

/// A description of data
/// Will be transported using notify mechanisms
struct DataDescription {
	DataDescription () {}
	sf::String mime;	 ///< Mime type (default=application/octet-stream)
	sf::String storage;	 ///< Storage format of data (e.g. text or binary or xml) (default: binary)
	sf::String user;	 ///< A user specific type

	bool operator== (const DataDescription & d) const { return (mime == d.mime && storage == d.storage && user == d.user); }

	SF_AUTOREFLECT_SDC;
};

/// A Update Notification element
struct Notify {
	Notify () : revision(0), size(0), mark (NoMark) {}
	Path path;
	int revision;
	int64_t size;

	enum Mark { // other possible could be End for end-of-stream or similar
		NoMark = 0,			// Regular Notification
		SubscriptionCancel  // subscription is canceled
	};
	
	Mark mark;

	SF_AUTOREFLECT_SDC;
};
SF_AUTOREFLECT_ENUM (Notify::Mark);

/// A Date range (from ... to bytes)
struct Range {
	Range (int64_t _from = 0, int64_t _to = 0) : from (_from), to (_to) {}
	int64_t from;
	int64_t to;
	// bool isDefault () const { return from == 0 && to == 0; }
	bool isValid   () const { return from >= 0 && to >= 0 && to >= from; }
	int64_t length () const { return to - from; }
	bool inRange (const Range & r) const { return isValid() && from >= r.from && to <= r.to; }
	
	static int64_t clipTo (int64_t x, const Range & r) {
		if (x < r.from) return r.from;
		if (x > r.to) return r.to;
		return x;
	}
	
	Range clipTo (const Range & r) const {
		return Range (clipTo (from, r), clipTo (to, r));
		
	}
	bool operator== (const Range & r) const { return from == r.from && to == r.to; }
	
	SF_AUTOREFLECT_SD;
};

/// A base class for a command which carries a unique id
struct GenericCommand {
	GenericCommand () : id (0) {}
	mutable AsyncOpId id;	///< May only be set by DataSharing
	
	SF_AUTOREFLECT_SDC;
};


/// A base class for a reply for a returning command, carrying the same id and an error code
struct GenericReply {
	GenericReply () : err (NoError), id (0) {}
	Path    path;	///< Path of request
	Error 	err;	///< Result of request
	AsyncOpId id;	///< Id of request (currently not used), 0 means no id
	
	SF_AUTOREFLECT_SDC;
};

/// A request for direct transfer
struct Request : public GenericCommand {
	Request (const Path & _path = Path(), int _revision = 0, const Range & _range = Range()) : path (_path), revision(_revision), range(_range), mark (NoMark) {}
	Path path;
	String user;	///< User specific subtype (default = "")
	int revision;
	Range range;
	enum Mark { NoMark = 0, Transmission, TransmissionCancel };
	Mark mark;
	SF_AUTOREFLECT_SDC;
};
SF_AUTOREFLECT_ENUM (Request::Mark);

/// A reply to a request command
struct RequestReply : public GenericReply {
	RequestReply () : revision (0), mark(NoMark) {}
	DataDescription desc;	///< Desc must be set if NoError
	int revision;
	Range range;
	enum Mark { NoMark = 0, TransmissionStart, Transmission, TransmissionFinish, TransmissionCancel };
	Mark mark;
	SF_AUTOREFLECT_SDC;
};
SF_AUTOREFLECT_ENUM(RequestReply::Mark);

/// A Subscription request
struct  Subscribe : public GenericCommand {
	Subscribe (const Path & _path = Path()) : path( _path), mark (NoMark) {}
	Path path;

	enum Mark {
		NoMark = 0,			// Regular subscription
		SubscriptionCancel  // Cancel a subscription
	};
	Mark mark;

	SF_AUTOREFLECT_SDC;
};
SF_AUTOREFLECT_ENUM (Subscribe::Mark);

/// a reply to a subscribe command
struct SubscribeReply : public GenericReply {
	DataDescription desc;	///< Description of the uri
	SF_AUTOREFLECT_SDC;
};

/// A push command
struct Push : public GenericCommand {
	Push (const Path & _path = Path(), int _revision = 0, const Range & _range = Range()) : path (_path), revision(_revision), range (_range) {}
	Path path;
	int revision;
	Range range;
	SF_AUTOREFLECT_SDC;
};

/// A reply message to push{}
/// Note: you have to generate it during a onPushed method
///       it generates NotSupported per default (e.g. if you do not give a callback)
//        change it to NoError to show the client, that pushing was successfull
struct PushReply : public GenericReply {
	PushReply (Error e = error::NotSupported) {
		err = e;
	}
	SF_AUTOREFLECT_SDC;
};

}

}
