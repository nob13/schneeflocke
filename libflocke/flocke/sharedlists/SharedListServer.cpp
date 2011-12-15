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
	uninit_locked();

	Error err = mServer->share (gSharedName, createByteArrayPtr(toJSONEx (mList, INDENT)));
	if (err) return err;

	mInitialized = true;
	return NoError;
}

Error SharedListServer::uninit () {
	return uninit_locked ();
}

Error SharedListServer::add (const String & shareName, const SharedElement & element) {
	if (mList.count(shareName) > 0) return error::ExistsAlready;
	mList[shareName] = element;
	return update_locked ();	
}

Error SharedListServer::remove (const String & shareName) {
	SharedList::iterator i = mList.find (shareName);
	if (i == mList.end()) return error::NotFound;
	mList.erase (i);
	return update_locked ();	
}

Error SharedListServer::clear () {
	mList.clear ();
	return update_locked ();
	
}

SharedList SharedListServer::list () const {
	return mList;
}

Path SharedListServer::path () const {
	return gSharedName;
}

Error SharedListServer::uninit_locked () {
	if (!mInitialized) return NoError;
	mList.clear ();
	mInitialized = false;
	return mServer->unShare("shared");	
}

Error SharedListServer::update_locked () {
	Error err = mServer->update ("shared", createByteArrayPtr(toJSONEx (mList, INDENT)));
	return err;
}

}
