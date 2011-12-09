#include "Globber.h"
#include <schnee/tools/FileTools.h>
#include <boost/foreach.hpp>
#include <schnee/schnee.h>

#define foreach BOOST_FOREACH

namespace sf {

Globber::Globber () {
	SF_REGISTER_ME;
	mWorkThread.start("Globber WorkThread");
	mDefaultTimeOutMs = 30000; // 30s
}

Globber::~Globber () {
	SF_UNREGISTER_ME;
	mWorkThread.stop ();
}

Error Globber::glob (const String & directory, const GlobCallback & callback, int timeOutMs) {
	LockGuard guard (mMutex);
	if (!isDirectory (directory)){
		Log (LogWarning) << LOGID << directory << " is not a directory." << std::endl;
		return error::InvalidArgument;
	}
	if (timeOutMs < 0) timeOutMs = mDefaultTimeOutMs;
	AsyncOpId id = genFreeId_locked ();
	GlobOp * op = new GlobOp (sf::regTimeOutMs(timeOutMs));
	op->setId(id);
	op->callback  = callback;
	op->directory = directory;
	add_locked (op);
	addGlobbingWork_locked (id);
	return NoError;
}

void Globber::continueGlobbing (AsyncOpId id) {
	LockGuard guard (mMutex);
	GlobOp * op;
	getReady_locked (id, GLOB, &op);
	if (!op) {
		// probably timeouted
		return;
	}
	// op cannot time out within that time frame now
	Time stop  = sf::futureInMs (50);

	if (op->begin) {
		Error e = expand_locked (op, op->directory, op->listing->entries);
		if (e) return;
		
		op->begin = false;
	}
	
	while (!op->workQueue.empty()){
		if(sf::currentTime() > stop) {
			add_locked (op);
			addGlobbingWork_locked (id);
			return;
		}

		WorkPoint wp = op->workQueue.front();
		op->workQueue.pop_front();
		
		Error e = expand_locked (op, wp.second, wp.first->entries);
		if (e) return;
	}
	{
		SF_SCHNEE_LOCK;
		op->callback (NoError, op->listing);
		delete op;
	}
	return;	
}

Error Globber::expand_locked (GlobOp * op, const String& directory, RecursiveEntryVec & target) {
	Log (LogInfo) << LOGID << "Expanding " << directory << " for op " << op->id() << std::endl;
	DirectoryListing list;
	Error e = sf::listDirectory (directory, &list);
	if (e) {
		op->callback (e, op->listing);
		delete op;
		return e;
	}
	foreach (DirectoryListing::Entry & e, list.entries) {
		target.push_back (RecursiveEntry(e));
	}
	for (EntryIterator i = target.begin(); i != target.end(); i++){
		if (i->type == DirectoryListing::Directory){
			String name = directory + sf::gDirectoryDelimiter + i->name;;
			if (sf::isSymlink (name)) continue; // do not dereference symlinks
			WorkPoint p;
			p.first  = i;
			p.second = name; 
			op->workQueue.push_back (p);
		}
	}
	return NoError;
}



void Globber::addGlobbingWork_locked (AsyncOpId id) {
	Error e = mWorkThread.add (abind (dMemFun (this, &Globber::continueGlobbing), id));
	assert (!e && "Workthread doesn't run?!");
}



}
