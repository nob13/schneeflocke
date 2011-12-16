#include "SharedListServer.h"

namespace sf {

const char * gSharedName = "shared";

SharedListServer::SharedListServer (DataSharingServer * server) {
	mServer = server;
	mInitialized = false;
}

SharedListServer::~SharedListServer () {
	uninit ();
}

Error SharedListServer::init () {
	uninit();

	Error err = mServer->share (gSharedName, createByteArrayPtr(toJSONEx (mList, INDENT)));
	if (err) return err;

	mInitialized = true;
	return NoError;
}

Error SharedListServer::uninit () {
	if (!mInitialized) return NoError;
	mList.clear ();
	mInitialized = false;
	return mServer->unShare("shared");
}

Error SharedListServer::add (const String & shareName, const SharedElement & element) {
	if (mList.count(shareName) > 0) return error::ExistsAlready;
	mList[shareName] = element;
	return update ();	
}

Error SharedListServer::remove (const String & shareName) {
	SharedList::iterator i = mList.find (shareName);
	if (i == mList.end()) return error::NotFound;
	mList.erase (i);
	return update ();	
}

Error SharedListServer::clear () {
	mList.clear ();
	return update ();
	
}

SharedList SharedListServer::list () const {
	return mList;
}

Path SharedListServer::path () const {
	return gSharedName;
}

Error SharedListServer::update () {
	Error err = mServer->update ("shared", createByteArrayPtr(toJSONEx (mList, INDENT)));
	return err;
}

}
