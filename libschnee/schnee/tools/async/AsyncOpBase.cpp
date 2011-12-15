#include "AsyncOpBase.h"
#include <schnee/tools/Log.h>

namespace sf {

AsyncOpBase::AsyncOpBase () {
	mCount = 0;
	mNextId = 1;
}

AsyncOpBase::~AsyncOpBase () {
	sf::cancelTimer(mTimerHandle);
	for (AsyncOpSet::iterator i = mAsyncOps.begin(); i != mAsyncOps.end(); i++){
		delete *i;
	}
}

AsyncOpBase::OpId AsyncOpBase::addAsyncOp (AsyncOp * op) {
	sf::cancelTimer (mTimerHandle);

	if (op->id() == 0) {
		op->mId = mNextId++;
	}

	mAsyncOps.insert (op);
	mAsyncOpMap[op->id()] = op;
	mCount++;
	OpId id = op->id();
	mTimerHandle = xcallTimed (dMemFun (this, &AsyncOpBase::onTimer), (*mAsyncOps.begin())->mTimeOut);
	return id;
}

AsyncOpBase::OpId AsyncOpBase::genFreeId () {
	AsyncOpBase::OpId id = mNextId++;
	return id;
}


AsyncOpBase::AsyncOp * AsyncOpBase::getReadyAsyncOp (OpId id) {
	AsyncOp * result = 0;
	AsyncOpMap::iterator i = mAsyncOpMap.find (id);
	if (i != mAsyncOpMap.end()){
		result = i->second;
		mAsyncOps.erase (i->second);
		mAsyncOpMap.erase (id);
		mCount--;
	} else {
#ifndef NDEBUG
		sf::Log (LogInfo) << LOGID << "Did not found id" << id << std::endl << "Valid ones: " << std::endl;
		for (AsyncOpMap::const_iterator i = mAsyncOpMap.begin(); i != mAsyncOpMap.end(); i++) {
			sf::Log (LogInfo) << i->first << "(" << i->second->id() << "," << i->second->type() << ")";
		}
		sf::Log (LogInfo) << std::endl;
#endif
	}
	return result;
}

void AsyncOpBase::cancelAsyncOps (int key, Error err) {
	AsyncOpMap::iterator i = mAsyncOpMap.begin();
	while (i != mAsyncOpMap.end()){
		AsyncOp * op = i->second;
		if (op->key() == key) {
			mAsyncOps.erase (op);
			mAsyncOpMap.erase (i++);
			mCount--;
			sf::Log (LogInfo) << LOGID << "Op " << op << " key=" << key << ", type=" << op->type() << " canceled with " << toString (err) << std::endl;
			op->onCancel (err);
			delete op;
			i = mAsyncOpMap.begin(); // i could be changed - we were not locked
		} else
			i++;
	}
}

void AsyncOpBase::onTimer () {
	mTimerHandle = TimedCallHandle ();
	sf::Time ct = sf::currentTime ();
	AsyncOpSet::iterator i = mAsyncOps.begin();
	while (i != mAsyncOps.end() && ct >= (*i)->mTimeOut) {
		AsyncOp * op = *i;
		mAsyncOpMap.erase (op->id());
		mAsyncOps.erase (i);
		mCount--;
		sf::Log (LogInfo) << LOGID << "Op " << op << " (id= " << op->id() << " type=" << op->type() << " timeouted)" << std::endl;
		op->onCancel (error::TimeOut);
		delete op;
		i = mAsyncOps.begin();
	}
	if (!mAsyncOps.empty()){
		mTimerHandle = xcallTimed (dMemFun (this, &AsyncOpBase::onTimer), (*mAsyncOps.begin())->mTimeOut);
	}
}

}
