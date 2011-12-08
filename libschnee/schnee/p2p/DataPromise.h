#pragma once
#include <schnee/sftypes.h>
#include "DataSharingElements.h"

namespace sf {

namespace ds {

/// Information about a working transmission
struct TransmissionInfo {
	TransmissionInfo () {
		mark = RequestReply::NoMark;
		transferred = 0;
		error = NoError;
		speed = 0;
	}
	typedef RequestReply::Mark TransmissionMark;
	TransmissionMark mark;
	int64_t transferred;
	Error   error;
	HostId  destination;
	float   speed;
	Path    path;
	SF_AUTOREFLECT_SERIAL;
};

}

/// A drop in replacement for a (slightly decorated) ByteArrayPtr
/// serves all data, and can even provide it asynchronally
/// DataSharingServer uses it as the source of all data
struct DataPromise {
	virtual ~DataPromise() {}
	/// The promise is ready to give out data
	/// (This allows the creation of asynchronous promises
	/// Note: Asynchronous promises are only supported on Request-Transmission-Operations
	/// In order to implement timeOuts just be ready and deliver a timeOut upon read() operations
	virtual bool ready () const = 0;
	
	/// Read something from the promise
	/// Unknown-sized transmissions may return Eof
	virtual sf::Error read (const ds::Range & range, ByteArray & dst) = 0;
	
	/// Read all into a Byte Array
	/// Unknown-sized transmissions may return Eof
	virtual sf::Error read (ByteArray & dst) {
		return read (ds::Range (0, size()), dst);
	}
	
	/// Writes in some data (e.g. on pushing)
	virtual sf::Error write (const ds::Range & range, const ByteArrayPtr & data) {
		return error::NotSupported;
	}

	/// Size of the promise (-1 is UNKNOWN)
	/// Asynchronous operations must return a size of -1 as long the size is not known
	virtual int64_t size () const = 0;
	
	/// (optional) a data description, which will be delivered upon requests
	virtual ds::DataDescription dataDescription () const {
		return ds::DataDescription();
	}

	/// An error state (if existing)
	virtual Error error () const { return NoError; }

	/// Continuous information update about a served transmission
	/// Note: DataSharing updates this in it's locked mode
	/// Do not call DataSharingServer upon it.
	virtual void onTransmissionUpdate_locked (AsyncOpId id, const ds::TransmissionInfo & info) {}
};

typedef sf::shared_ptr<DataPromise> DataPromisePtr;

/// DataSharingPromise implementation for ByteArrayPtr
class ByteArrayPromise : public DataPromise {
public:
	ByteArrayPromise (const sf::ByteArrayPtr & data, const ds::DataDescription & desc = ds::DataDescription()) : mData (data), mDesc(desc) {}
	virtual bool ready () const { return true; }
	virtual sf::Error read (const ds::Range & range, ByteArray & dst) {
		dst.assign (mData->begin() + range.from, mData->begin() + range.to);
		return NoError;
	}
	virtual int64_t size () const {
		return mData->size();
	}
	virtual ds::DataDescription dataDescription () const {
		return mDesc;
	}

private:
	ByteArrayPtr mData;
	ds::DataDescription mDesc;
};

inline DataPromisePtr createDataPromise (const sf::ByteArrayPtr & data, const ds::DataDescription & desc = ds::DataDescription()){
	if (!data) return DataPromisePtr();
	return DataPromisePtr (new ByteArrayPromise (data, desc));
}

}
