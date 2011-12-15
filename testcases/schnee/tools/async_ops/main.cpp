#include <schnee/schnee.h>
#include <schnee/Error.h>
#include <schnee/test/test.h>
#include <schnee/tools/ResultCallbackHelper.h>
#include <schnee/tools/async/DelegateBase.h>

#include <schnee/tools/async/AsyncOpBase.h>
#include <schnee/tools/async/ABindAround.h>
#include <assert.h>

using namespace sf;

/*
 * Tests asynchronous operations made with the class AsyncOpBase. It is necessary that
 * the operations can be
 * - canceled
 * - timeouted
 * - fullfilled.
 *
 * On termination of a asynchronous operation it must be possible to add new operations.
 *
 *
 */


typedef AsyncOpBase::OpId OpId;

/// One example user for AsyncOpBase
class AsyncExample : public AsyncOpBase {
public:

	AsyncExample () {
		SF_REGISTER_ME;
	}

	~AsyncExample () {
		SF_UNREGISTER_ME;
	}

	typedef function<void (OpId id, Error err)> ResultCallback1;
	typedef function<void (OpId id, Error err, int data)> ResultCallback2;

	struct Op1 : public AsyncOp {
		Op1 (Time _timeOut) : AsyncOp (1, _timeOut) {}
		ResultCallback1 cb;
		virtual void onCancel (Error reason) { cb (id(), reason); }
	};

	struct Op2 : public AsyncOp {
		Op2 (Time _timeOut) : AsyncOp (2, _timeOut) {}
		ResultCallback2 cb;
		virtual void onCancel (Error reason) { cb (id(), reason, 0x12345678); }
	};

	struct Op3 : public AsyncOp {
		Op3 (Time _timeOut) : AsyncOp (3, _timeOut) {}
		ResultCallback cb;
		virtual void onCancel (Error reason) { cb (reason); }
	};

	OpId asyncCall1 (int timeOutMs, const ResultCallback1& result, int key = 0) {
		Time timeOut = regTimeOutMs(timeOutMs);
		Op1 * op = new Op1 (timeOut);
		op->cb = result;
		op->setKey(key);
		return addAsyncOp (op);
	}

	OpId asyncCall2 (int timeOutMs, const ResultCallback2& result, int key = 0) {
		Time timeOut = regTimeOutMs(timeOutMs);
		Op2 * op = new Op2 (timeOut);
		op->cb = result;
		op->setKey(key);
		return addAsyncOp (op);
	}

	/// Tests some wired template, finishes immediately
	void asyncCall3 (int timeOutMs, const ResultCallback & callback) {
		Op3 * op = new Op3 (regTimeOutMs(timeOutMs));
		op->setId(genFreeId());
		op->cb = callback;

		xcall (aOpMemFun (op, &AsyncExample::onFinishOp3_locked));
		addAsyncOp (op);
	}

	void onFinishOp3_locked (Op3 * op) {
		op->cb(NoError);
		delete op;
	}

	void asyncCall3Param0 (int timeOutMs, const ResultCallback & callback) {
		Op3 * op = new Op3 (regTimeOutMs (timeOutMs));
		op->setId(genFreeId());
		op->cb = callback;
		xcall (abind (aOpMemFun (op, &AsyncExample::onFinishOp3Param0_locked), NoError));
		addAsyncOp (op);
	}

	void onFinishOp3Param0_locked (Op3 * op, Error e) {
		op->cb(e);
	}

	void finishCall (int64_t call) {
		AsyncOp * op;
		op = getReadyAsyncOp (call);
		if (op) {
			assert (op->type() == 1 || op->type() == 2);
			if (op->type() == 1) { Op1 * op1 = (Op1*) op; op1->cb (op->id(), NoError); }
			if (op->type() == 2) { Op2 * op2 = (Op2*) op; op2->cb (op->id(), NoError, 0x12345678); }
		}
		delete op;
	}

	void cancelCall (int key, Error e) {
		cancelAsyncOps (key, e);
	}

};

void mustTimeOutOrdered (OpId id, Error err) {
	// static OpId lastId = id;
	// assert (lastId >= id);			///< Must timeout in ascending order
	assert (err == error::TimeOut);
	if (err != error::TimeOut) exit (1);
	std::cout << " timeOut -- " << id << std::endl;
	// lastId = id;
}

void mustTimeOut (OpId, Error err) {
	assert (err == error::TimeOut);
	if (err != error::TimeOut) exit (1);
}


void mustFinish (OpId id, Error err, int data) {
	assert (err == NoError);
	std::cout << " -- finished " << id << std::endl;
	if (err) exit (2);
}

void mustBeOffline (OpId id, Error err) {
	tassert (err == error::TargetOffline, "Wrong error code");
}

void startTask (OpId id, Error err, int data, AsyncExample * example) {
	std::cout << " -- starting Task on being callback" << std::endl;
	example->asyncCall1 (10, &mustTimeOut);
}

void mayNotHappen (OpId id, Error err) {
	assert (false && "Do not call me");
	exit (2);
}

/// A callback handler which will disappear
struct DisappearingHandler : public DelegateBase {
	DisappearingHandler () {
		SF_REGISTER_ME;
	}
	~DisappearingHandler () {
		SF_UNREGISTER_ME;
	}
	void handle (OpId id, Error err) {
		assert (false && "I do not live anymore");
		exit (3);
	}
};

int main (int argc, char * argv[]) {
	schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	AsyncExample example;
	{
		std::cout << "Time out calculation" << std::endl;
		{
			AsyncExample::Op1 op (regTimeOutMs (-1));
			assert (op.lastingTimeMs() == -1); // infinite time
		}
		{
			AsyncExample::Op1 op (regTimeOutMs (10000));
			test::sleep_locked (1);
			int ms = op.lastingTimeMs();
			assert (ms > 8000 && ms < 9500); // around 9000
		}
		{
			AsyncExample::Op1 op (regTimeOutMs (1));
			test::sleep_locked (1);
			int ms = op.lastingTimeMs(); // exactly 0 time is out
			assert (ms == 0);
		}
		{
			AsyncExample::Op1 op (regTimeOutMs (10000));
			test::sleep_locked (1);
			int ms = op.lastingTimeMs(0.5f);
			assert (ms > 4000 && ms < 5000); // around 4500
		}
		{
			// opcb_locked function
			ResultCallbackHelper helper;
			example.asyncCall3(100, helper.onResultFunc());
			bool suc = helper.waitReadyAndNoError(100);
			assert (suc);
		}
		{
			// opcb_locked function2
			ResultCallbackHelper helper;
			example.asyncCall3Param0(100, helper.onResultFunc());
			bool suc = helper.waitReadyAndNoError(100);
			assert (suc);
		}

	}
	{
		std::cout << "Callback against non-existing instances" << std::endl;
		{
			DisappearingHandler handler;
			example.asyncCall1 (100, dMemFun (&handler, &DisappearingHandler::handle));
		}
		test::sleep_locked (1);
	}
	{
		std::cout << "Testing TimeOut (in backward order)" << std::endl;
		for (int i = 0; i < 1000; i++) {
			int timeOutInMs = 2000 - i;
			OpId id = example.asyncCall1 (timeOutInMs, mustTimeOutOrdered);
			std::cout << "Kicking id " << id << " timeOutInMs " << timeOutInMs << std::endl;
		}
		test::sleep_locked(3);						//< All must time out
		assert (example.waitingOps() == 0);		//< No call has to left
	}
	{
		std::cout << "cancel by hand" << std::endl;
		for (int i = 0; i < 1000; i++) {
			example.asyncCall1 (1000, mustBeOffline, 50);
		}
		example.cancelCall (50, error::TargetOffline);
		test::sleep_locked (1);
		assert (example.waitingOps() == 0);
	}
	{
		std::cout << "Testing finishing" << std::endl;
		// - finishing works
		OpId startId(0), endId(0);
		int iterations = 1000;
		for (int i = 0; i < iterations; i++) {
			OpId id = example.asyncCall2 (2000 - (i / 10), &mustFinish);
			if (i == 0) startId = id;
			if (i == iterations - 1) endId = id;
		}
		assert (endId - startId == iterations - 1);
		for (int i = 0; i < iterations; i++) {
			xcall(abind (dMemFun (&example, &AsyncExample::finishCall), startId + i));
			// xcall (bind (&AsyncExample::finishCall, &example, startId + i));
		}
		test::sleep_locked (3);
		assert (example.waitingOps() == 0);		//< No call has to left
	}
	{
		std::cout << "Testing new taks during timeOut" << std::endl;
		// - starting new tasks during timeOut works
		example.asyncCall1 (100, abind (&startTask, 0, &example));
		// example.asyncCall1 (100, bind (startTask, _1, _2, 0, &example));
		test::sleep_locked (1);
		assert (example.waitingOps() == 0);		//< No call has to left
	}
	{
		std::cout << "Testing new tasks during finish" << std::endl;
		// - starting new tasks during finishing
		// OpId id = example.asyncCall2 (1000, bind (startTask, _1, _2, _3, &example));
		OpId id = example.asyncCall2 (1000, abind (&startTask, &example));
		// xcall (bind (&AsyncExample::finishCall, &example, id));
		xcall ( abind (dMemFun (&example, &AsyncExample::finishCall), id));
		test::sleep_locked (1);
	}
	{
		std::cout << "Testing correct shut down" << std::endl;
		AsyncExample example2;
		for (int i = 0; i < 1000; i++) {
			example.asyncCall1 (2000 - (i / 10), mayNotHappen);
		}
		assert (example.waitingOps() == 1000);
	}

	return 0;
}
