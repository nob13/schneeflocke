#include "FileSharingPromise.h"
#include <schnee/tools/Log.h>

#include <boost/filesystem/operations.hpp>
#include <stdio.h>

namespace fs = boost::filesystem;

namespace sf {

FileSharingPromise::FileSharingPromise (const String & name) {
	mName  = name;
	mFile  = 0;
	mError = NoError;
	mSize  = 0;
	updateFileSize ();
}

FileSharingPromise::~FileSharingPromise(){
	closeFile ();
}

sf::Error FileSharingPromise::read (const ds::Range & range, ByteArray & dst) {
	if (!mFile && !mError) openFile ();
	if (mError) return mError;
	if ((int64_t)mPosition != range.from) {
		fseek (mFile, range.from, SEEK_SET);
		mPosition = range.from;
	}
	size_t length = range.length();
	dst.resize (length);
	if (length == 0) {
		// otherwise win32 crashes on dst.c_array()
		return NoError;
	}
	size_t elem = fread (dst.c_array(), 1, length, mFile);
	if (elem != length){
		int err = errno;
		Log (LogWarning) << LOGID << "Could not read " << mName << ", error=" << strerror (err) << std::endl;
		mError = error::ReadError;
	}
	mPosition+=elem;
	return mError;
}

int64_t FileSharingPromise::size () const {
	return mSize;
}

void FileSharingPromise::openFile () {
	if (mError || mFile) return; // already opened / errored
	mFile = fopen (mName.c_str(), "rb");
	if (!mFile) { mError = error::ReadError; return; }
	mPosition = 0;
}

void FileSharingPromise::closeFile (){
	if (!mFile) return; // not opened
	fclose (mFile);
}

void FileSharingPromise::updateFileSize () {
	boost::filesystem::path p (mName);
	if (!fs::exists(p)) { 
		mError = error::NotFound; 
		return; 
	}
	if (!fs::is_regular(p)) { 
		mError = error::InvalidArgument; 
		return; 
	}
	mSize = fs::file_size(p);
}

}
