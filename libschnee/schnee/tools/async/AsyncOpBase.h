#pragma once
#include <schnee/sftypes.h>
#include <deque>
#include <schnee/tools/Log.h>
#include "DelegateBase.h"
#include <schnee/tools/async/ABindAround.h>
#include <schnee/Error.h>

namespace sf {

/// A Base class for objects who want to provide asynchronous operations and timeouts.
class AsyncOpBase : public DelegateBase {
public:
	typedef AsyncOpId OpId;

	AsyncOpBase ();
	~AsyncOpBase ();

	/// Returns number of currently awaited async operations
	size_t waitingOps () { return mCount; }
protected:

	/// Base class for all asynchronous Operations working inside a AsyncOpBase
	/// Note: modification should be done inside the AsyncOpBase implementation
	/// by getting access via getReady() and adding again via add()
	class AsyncOp {
	public:
		AsyncOp (int type, sf::Time _timeOut) :  mState(0), mId(0), mKey(0), mType (type), mTimeOut (_timeOut) {
		}
		virtual ~AsyncOp () {}

		///@name Member Access
		///@{

		/// Returns operation id
		OpId id () const { return mId; }

		/// Sets the id. May only be done if none is set yet
		void setId (OpId id) {
			assert (mId == 0 && "Resetting the id?");
			mId = id;
		}

		/// Changes the time out.
		/// You should only do this, if the AsyncOp is not currently added to an AsyncOpBase.
		void setTimeOut (const Time & t) {
			mTimeOut = t;
		}
		const Time & getTimeOut () const {
			return mTimeOut;
		}

		/// User defined type id
		int type () const { return mType; }
		
		/// AsyncOps may return a state
		int state () const { return mState; }
		
		/// Sets the state
		void setState (int state) { mState = state; }
		
		/// User defined key (or 0 if none has been defined)
		/// Multiple operations may share the same key
		int key () const {
			return mKey;
		}

		/// Sets key
		void setKey (int key) { mKey = key; }


		/// Calculates the lasting time for this operation times out
		/// Returns time in ms or -1 if operation has infinite time
		/// Factor: with a factor != 1.0f you can modify the time (to get 2/3 of lasting time
		/// use lastingTimeMs (0.66))
		int lastingTimeMs (float factor = 1.0f) {
			if (mTimeOut == sf::posInfTime()) return -1;
			int x = (mTimeOut - sf::currentTime()).total_milliseconds();
			if (x < 0) return 0;
			return x * factor;
		}

		///@}


		///@name Events
		///@{

		/// Handler for errors (e.g. TimeOut)
		virtual void onCancel (sf::Error reason) = 0;

		///@}

	protected:
		int mState;				///< User defined state of the operation
		
	private:
		OpId mId;				///< Id of the operation (0 is no id yet)
		int mKey;
		int mType;
		sf::Time mTimeOut;
		friend class AsyncOpBase;
	};
	
	/// Adds a timeout; also sets id,type,timeOut, returns it's id
	/// if opt id is 0, one will be generated
	OpId addAsyncOp (AsyncOp * op);

	/// Generate a new free id
	OpId genFreeId ();


	/// Returns an async op and deletes it from the waiting list
	/// (Do it if the async op returned)
	/// Returns 0 if not found or timer was already through
	AsyncOp * getReadyAsyncOp (OpId id);

	/// Cancel Operations for a given key
	void cancelAsyncOps (int key, Error err);

	/// Returns an async op (via param ret) and delete it from the waiting list.
	/// Converts it automatically to the given Type/type id
	/// Ret is non null on success. Type mismatches will be logged
	template <class T> void getReadyAsyncOp (OpId id, int type, T ** ret);
	
	/// Returns an async op (stored in ret) in a desired state
	/// If op is in wrong state, it will be deleted and an error message is printed
	template <class T> void getReadyAsyncOpInState (OpId id, int type, int desiredState, T ** ret);

	/// Executes given functor(AsyncOp) for each stored asynchronous function
	template <class Functor> void forEachAsyncOp (Functor & func);
	

	/**
	 * Generates a callback useful for asynchronous operations, which when executed
	 * does DelegateBase-check, and checks whether the Op is from the right type and right state
	 * and when all this is true, the function will be called with AsyncOp (which you have to add() before)
	 *
	 * This way you can create nice asynchronous operation cascades, used in many networking algorithms.
	 *
	 * @param op     - The operation - must already have it's state when the callback strikes.
	 * @param dstFun - The function with signature YourType::*fun (Op * op). The function will be called
	 * DelegateBase-protected and with the Op type and the right op state (may not change afterwards).
	 *
	 */
	template <class ThisType, class OpType> VoidCallback aOpMemFun (OpType * op, void (ThisType::*dstFun) (OpType *));

	// Like aOpMemFun, but with one argument which gets bound until dstFun
	template <class ThisType, class OpType, class Param0> function<void (Param0)> aOpMemFun (OpType * op, void (ThisType::*dstFun) (OpType *, Param0 result));
	// Like aOpMemFun, but with two arguments who get bound until dstFun
	template <class ThisType, class OpType, class Param0, class Param1> function<void (Param0, Param1)> aOpMemFun (OpType * op, void (ThisType::*dstFun) (OpType *, Param0 result0, Param1 result1));

	// Like aOpMemFun, but with tree arguments who get bound until dstFun
	template <class ThisType, class OpType, class Param0, class Param1, class Param2> function<void (Param0, Param1, Param2)> aOpMemFun (OpType * op, void (ThisType::*dstFun) (OpType *, Param0 result0, Param1 result1, Param2 result2));

private:
	/// Executes given function (appending op), when op with given id is found, has the right type and the right state
	/// @experimental
	template <class OpType> void executeOpCheck (const function<void(OpType*)> & functor, OpId id, int requiredOpType, int requiredOpState);
	
	// The Timer for the first has striked
	void onTimer ();

	struct TimeOutComparator {
		bool operator() (const AsyncOp * a, const AsyncOp * b) const {
			if (a->mTimeOut == b->mTimeOut) { return a < b; }
			return a->mTimeOut < b->mTimeOut;
		}
	};
	typedef std::set<AsyncOp*, TimeOutComparator> AsyncOpSet;
	typedef std::map<OpId, AsyncOp*> AsyncOpMap;


	AsyncOpSet mAsyncOps;						///< Waiting list for the timer
	AsyncOpMap mAsyncOpMap;						///< id to AsyncOp map
	OpId mNextId;
	size_t mCount;
	TimedCallHandle mTimerHandle;
	Time mNextTimeOut;
	bool mCancelling;	///< Currently cancelling the timer
};


// Implementation of templates (boring)

template <class T> void AsyncOpBase::getReadyAsyncOp (OpId id, int type, T ** ret) {
	*ret = 0;
	AsyncOp * op = getReadyAsyncOp (id);
	if (!op) { *ret = 0; return; }					 // not found, still ok, maybe timeouted
	if (op->type() == type) { *ret = static_cast<T*> (op); return;} // success
	sf::Log (LogWarning) << LOGID << "Type mismatch, requested type=" << type << " found type=" << op->type() << std::endl;
	delete op;
}


template <class T> void AsyncOpBase::getReadyAsyncOpInState (OpId id, int type, int desiredState, T ** ret) {
	getReadyAsyncOp (id, type, ret);
	if (!*ret) return;
	if ((*ret)->state() != desiredState){
		sf::Log (LogError) << LOGID << "AsyncOp " << id << " is in wrong state (desired=" << desiredState << " found=" << (*ret)->state() << ")" << std::endl;
		delete *ret;
		*ret = 0;
		return;
	}
}

template <class Functor> void AsyncOpBase::forEachAsyncOp (Functor & func) {
	   for (std::map<OpId, AsyncOp*>::const_iterator i = mAsyncOpMap.begin(); i != mAsyncOpMap.end(); i++){
			   const AsyncOp * op = i->second;
			   func (op);
	   }
}

template <class ThisType, class OpType> VoidCallback AsyncOpBase::aOpMemFun (OpType * op, void (ThisType::*dstFun) (OpType *)){
	OpId id = op->id();
	assert (id != 0 && "This template works only if the operation has an associated id, use AsyncOp::setId(genFreeId())");
	ThisType * me = static_cast<ThisType*> (this);

	return abind (
			dMemFun (this, &AsyncOpBase::executeOpCheck<OpType>),
			memFun (me, dstFun), id, op->type(), op->state());
}

template <class ThisType, class OpType, class Param0> function<void (Param0)> AsyncOpBase::aOpMemFun (OpType * op, void (ThisType::*dstFun) (OpType *, Param0 result)){
	OpId id = op->id();
	assert (id != 0 && "This template works only if the operation has an associated id, use AsyncOp::setId(genFreeId())");
	ThisType * me = static_cast<ThisType*> (this);

	return abindAround1 <Param0> (abind (dMemFun (this, &AsyncOpBase::executeOpCheck<OpType>), id, op->type(), op->state()),  memFun (me, dstFun));
}

template <class ThisType, class OpType, class Param0, class Param1> function<void (Param0, Param1)> AsyncOpBase::aOpMemFun (OpType * op, void (ThisType::*dstFun) (OpType *, Param0 result0, Param1 result1)) {
	OpId id = op->id();
	assert (id != 0 && "This template works only if the operation has an associated id, use AsyncOp::setId(genFreeId())");
	ThisType * me = static_cast<ThisType*> (this);

	return abindAround2 <Param0, Param1> (abind (dMemFun (this, &AsyncOpBase::executeOpCheck<OpType>), id, op->type(), op->state()), memFun (me, dstFun));
}

template <class ThisType, class OpType, class Param0, class Param1, class Param2> function<void (Param0, Param1, Param2)> AsyncOpBase::aOpMemFun (OpType * op, void (ThisType::*dstFun) (OpType *, Param0 result0, Param1 result1, Param2 result2)) {
	OpId id = op->id();
	assert (id != 0 && "This template works only if the operation has an associated id, use AsyncOp::setId(genFreeId())");
	ThisType * me = static_cast<ThisType*> (this);

	return abindAround3 <Param0, Param1, Param2> (abind (dMemFun (this, &AsyncOpBase::executeOpCheck<OpType>), id, op->type(), op->state()), memFun (me, dstFun));
}

template <class OpType> void AsyncOpBase::executeOpCheck (const function<void (OpType*)>& functor, OpId id, int requiredOpType, int requiredOpState){
	OpType * op;
	this->getReadyAsyncOpInState (id, requiredOpType, requiredOpState, &op);
	if (!op) return;
	return functor(op);
}


}


