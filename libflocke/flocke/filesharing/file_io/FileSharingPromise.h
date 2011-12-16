#pragma once
#include <schnee/p2p/DataPromise.h>
#include <schnee/tools/async/DelegateBase.h>

namespace sf {

/// DataSharingPromise which will be given to the DataSharingServer;
/// Note: multiple reads are not thread safe, but destruction is.
/// The file size is calculated upon construction, however the file is opened
/// lazy.
class FileSharingPromise : public DataPromise {
public:
	FileSharingPromise (const String & name);
	~FileSharingPromise ();

	// Implementation of DataSharingPromise
	virtual bool ready () const { return true; }
	virtual sf::Error read (const ds::Range & range, ByteArray & dst);
	virtual int64_t size () const;
	virtual Error error () const { return mError; }

	typedef function <void (AsyncOpId id, const ds::TransmissionInfo &)> TransmissionUpdateDelegate;
	TransmissionUpdateDelegate& transmissionUpdated () { return mTransmissionUpdated; }

	/// @override
	virtual void onTransmissionUpdate (AsyncOpId id, const ds::TransmissionInfo & info) {
		// uncouple, because DataSharingServer is locked
		notifyAsync (mTransmissionUpdated, id, info);
	}

	const String& fileName () {
		return mName;
	}

private:
	// (lazy) open the file
	void openFile ();
	// (lazy) close the file
	void closeFile ();
	// update file size
	void updateFileSize();
	String mName;
	size_t mSize;
	size_t mPosition;
	FILE * mFile;
	Error  mError;

	TransmissionUpdateDelegate mTransmissionUpdated;
};
typedef shared_ptr<FileSharingPromise> FileSharingPromisePtr;

}
