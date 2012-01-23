#include "HttpContext.h"
#include <stdio.h>
#include "HttpRequest.h"
#include <schnee/schnee.h>
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
	HttpGetOperation * op = new HttpGetOperation (sf::regTimeOutMs(timeOutMs));
	op->setId(genFreeId());
	op->url = request.url();
	op->request = request.result();
	op->callback = callback;
	op->setState (HttpGetOperation::HG_WAITCONNECT);
	mConnectionManager.requestConnection(op->url, timeOutMs, abind(dMemFun(this, &HttpContext::onConnect), op->id()));
	addAsyncOp (op);
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
		mResult   = e;
		mResponse = response;
		mCalled = true;
		mCondition.notify_all();
	}
	std::pair<Error,HttpResponsePtr> waitAndReturnResult () {
		while (!mCalled) {
			mCondition.wait(schnee::mutex());
		}
		return std::make_pair(mResult, mResponse);
	}
private:
	Condition mCondition;
	HttpResponsePtr mResponse;
	Error mResult;
	bool mCalled;
};

std::pair<Error, HttpResponsePtr> HttpContext::syncRequest (const HttpRequest & r, int timeOutMs) {
	assert (schnee::mutex().try_lock() == false && "SchneeLock must be set and called from foreign thread!");
	Syncer s;
	request (r, timeOutMs, sf::memFun (&s, &Syncer::onResult));
	return s.waitAndReturnResult();
}


void HttpContext::onConnect (Error result, HttpConnectionPtr con, AsyncOpId id) {
	HttpGetOperation * op;
	getReadyAsyncOpInState (id, GetOperation, HttpGetOperation::HG_WAITCONNECT, &op);
	if (!op) return;
	do {
		if (result) break;
		op->con = con;
		op->con->channel->changed() = abind (dMemFun (this, &HttpContext::onChanged), id);
		op->setState(HttpGetOperation::HG_RECEIVE);

		op->response = HttpResponsePtr (new HttpResponse);
		op->response->authenticated = con->channel->info().authenticated;
		op->parser.setDestination (op->response);
		op->parser.inputBuffer().swap(op->con->inputBuffer);

		result = op->con->channel->write(op->request);
		Log (LogInfo) << LOGID << "(RemoveMe) sent" << *op->request << std::endl;
		op->request.reset();
		Log (LogInfo) << LOGID << " Sent request for " << op->url.toString() << " result=" << toString(result) << std::endl;
		if (result) break;
	} while (false);
	if (result) {
		finish (result, op);
		return;
	}
	addAsyncOp (op);
}

void HttpContext::onChanged (AsyncOpId id) {
	HttpGetOperation * op;
	getReadyAsyncOpInState (id, GetOperation,HttpGetOperation::HG_RECEIVE, &op);
	if (!op) return;
	if (op->finished) {
		// op is destroying
		addAsyncOp (op);
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
			finish (op->parser.result(), op);
			return;
		}
	} else {
		Error e = op->con->channel->error();
		if (e) {
			Log (LogWarning) << LOGID << "Got error " << toString (e) << " during data transfer on socket!" << std::endl;
			finish (e, op);
			return;
		}
	}
	addAsyncOp (op);
}

void HttpContext::finish (Error result, HttpGetOperation * op) {
	xcall (abind(op->callback, result, op->response));
	if (op->con)
		op->parser.inputBuffer().swap(op->con->inputBuffer);
	mConnectionManager.giveBack(result, op->con);
	delete op;
}

}

