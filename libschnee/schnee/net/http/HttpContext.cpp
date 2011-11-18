#include "HttpContext.h"
#include <stdio.h>
#include "HttpRequest.h"

namespace sf {

HttpContext::HttpContext () {
	SF_REGISTER_ME; 
}

HttpContext::~HttpContext() {
	SF_UNREGISTER_ME; 
}

void HttpContext::get (const Url & url, int timeOutMs, const RequestCallback & callback) {
	HttpRequest r;
	r.start("GET", url, HTTP_11);
	r.end();
	request (r, timeOutMs, callback);
}

void HttpContext::request (const HttpRequest & request, int timeOutMs, const RequestCallback & callback){
	LockGuard guard (mMutex);
	HttpGetOperation * op = new HttpGetOperation (sf::regTimeOutMs(timeOutMs));
	op->setId(genFreeId_locked());
	op->url = request.url();
	op->request = request.result();
	op->callback = callback;
	op->setState (HttpGetOperation::HG_WAITCONNECT);
	mConnectionManager.requestConnection(op->url, timeOutMs, abind(dMemFun(this, &HttpContext::onConnect), op->id()));
	add_locked (op);
}

/// Helper class for synchronizing HttpRequest calls
/// TODO: Merge with ResultCallbackHelper
class Syncer {
public:
	Syncer () {
		mResult = NoError;
		mCalled = false;
	}
	void onResult (Error e, const HttpResponsePtr & response) {
		{
			LockGuard guard (mMutex);
			mResult   = e;
			mResponse = response;
			mCalled = true;
		}
		mCondition.notify_all();
	}
	std::pair<Error,HttpResponsePtr> waitAndReturnResult () {
		LockGuard guard (mMutex);
		while (!mCalled) {
			mCondition.wait(mMutex);
		}
		return std::make_pair(mResult, mResponse);
	}
private:
	Condition mCondition;
	Mutex mMutex;
	HttpResponsePtr mResponse;
	Error mResult;
	bool mCalled;
};

std::pair<Error, HttpResponsePtr> HttpContext::syncRequest (const HttpRequest & r, int timeOutMs) {
	Syncer s;
	request (r, timeOutMs, sf::memFun (&s, &Syncer::onResult));
	return s.waitAndReturnResult();
}


void HttpContext::onConnect (Error result, HttpConnectionPtr con, AsyncOpId id) {
	LockGuard guard (mMutex);
	HttpGetOperation * op;
	getReadyInState_locked (id, GetOperation, HttpGetOperation::HG_WAITCONNECT, &op);
	if (!op) return;
	do {
		if (result) break;
		op->con = con;
		op->con->channel->changed() = abind (dMemFun (this, &HttpContext::onChanged), id);
		op->setState(HttpGetOperation::HG_RECEIVE);

		op->response = HttpResponsePtr (new HttpResponse);
		op->parser.setDestination (op->response);
		op->parser.inputBuffer().swap(op->con->inputBuffer);

		result = op->con->channel->write(op->request);
		Log (LogInfo) << LOGID << "(RemoveMe) sent" << *op->request << std::endl;
		op->request.reset();
		Log (LogInfo) << LOGID << " Sent request for " << op->url.toString() << " result=" << toString(result) << std::endl;
		if (result) break;
	} while (false);
	if (result) {
		finish_locked (result, op);
		return;
	}
	add_locked (op);
}

void HttpContext::onChanged (AsyncOpId id) {
	LockGuard guard (mMutex);
	HttpGetOperation * op;
	getReadyInState_locked (id, GetOperation,HttpGetOperation::HG_RECEIVE, &op);
	if (!op) return;
	if (op->finished) {
		// op is destroying
		add_locked (op);
		return;
	}
	ByteArrayPtr all = op->con->channel->read();
	if (!all->empty()){
		Log (LogInfo) << LOGID << "Recv\n" << *all << std::endl;
		op->parser.push(*all);
		if (op->parser.ready()) {
			Log (LogInfo) << LOGID << "Result of parser: " << toString (op->parser.result()) << std::endl;
			if (op->response->data)
				Log (LogInfo) << LOGID << "Bytes Read: " << op->response->data->size() << std::endl;
			finish_locked (op->parser.result(), op);
			return;
		}
	} else {
		Error e = op->con->channel->error();
		if (e) {
			Log (LogWarning) << LOGID << "Got error " << toString (e) << " during data transfer on socket!" << std::endl;
			finish_locked (e, op);
			return;
		}
	}
	add_locked (op);
}

void HttpContext::finish_locked (Error result, HttpGetOperation * op) {
	xcall (abind(op->callback, result, op->response));
	if (op->con)
		op->parser.inputBuffer().swap(op->con->inputBuffer);
	mConnectionManager.giveBack(result, op->con);
	delete op;
}

}

