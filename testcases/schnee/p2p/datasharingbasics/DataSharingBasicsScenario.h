#pragma once
#include <schnee/test/StandardScenario.h>
#include <schnee/test/test.h>
#include <schnee/test/PseudoRandom.h>
#include "OutstandingDataPromise.h"
#include "SubDataPromise.h"
#include "SyncCounter.h"

using namespace sf;

/// Test peer for simple protocol tests
/// Can only handle one action at once
struct BasicDataSharingPeer : public test::DataSharingScenario::DataSharingPeer, public DelegateBase {
	enum TransmissionState { None, Started, Cancelled, Finished, ERROR };
	
	BasicDataSharingPeer (InterplexBeacon * beacon) : DataSharingPeer (beacon) {
		SF_REGISTER_ME;
	}
	virtual ~BasicDataSharingPeer() {
		SF_UNREGISTER_ME;
	}
	
	void generateShareData () {
		const int size = 12345;
		sharedData.resize (size);
		test::pseudoRandomData(size,sharedData.c_array());
	}
	
	Uri uri () {
		return Uri (hostId(), "shareData");
	}
	
	void shareData () {
		Error e = server->share ("shareData", sf::createByteArrayPtr(sharedData));
		tassert1 (!e);
	}
	
	// Shares data which will never be delivered (for testing transmission cancel)
	void shareOutstandingData () {
		if (!outstandingDataPromise)
			outstandingDataPromise = OutstandingDataPromisePtr (new OutstandingDataPromise(sf::createByteArrayPtr(sharedData)));
		Error e = server->share (
				"shareData",
				DataSharingServer::createFixedPromise(outstandingDataPromise));
		tassert1 (!e);
	}
	
	// Shares data which is reachable through uri (shareData/subData) only.
	void shareSubData () {
		if (!subDataPromise){
			subDataPromise = SubDataPromisePtr (new SubDataPromise(sf::createByteArrayPtr(sharedData)));
		}
		Error e = server->share(
				"shareData",
				subDataPromise);
		tassert1 (!e);
	}
	
	void releaseOutstandingData () {
		tassert1 (outstandingDataPromise);
		outstandingDataPromise->release ();
	}
	
	void updateData () {
		Error e = server->update ("shareData", sf::createByteArrayPtr(sharedData));
		tassert1 (!e);
	}
	
	void unShareData () {
		Error e = server->unShare ("shareData");
		tassert1 (!e);
	}
	
	Error requestSync (const Uri & uri) {
		counter.mark();
		Error e = client->request (uri.host(), ds::Request(uri.path()), dMemFun (this, &BasicDataSharingPeer::onRequestReply), 100);
		if (e) return e;
		counter.wait();
		return lastRequestReply.err;
	}
	
	// Request a transmission (does not wait)
	Error requestTransmission (const Uri & uri) {
		ds::Request r;
		r.mark = ds::Request::Transmission;
		r.path  = uri.path();
		transmissionError = NoError;
		Error e = client->request(uri.host(), r, dMemFun (this, &BasicDataSharingPeer::onTransmissionReply), 100);
		return e;
	}
	
	Error requestTransmissionSync (const Uri & uri) {
		counter.mark ();
		requestTransmission (uri);
		counter.wait();
		return transmissionError;
	}
	
	Error cancelTransmission (const Uri & uri, AsyncOpId id) {
		Error e = client->cancelTransmission(uri.host(), id, uri.path());
		return e;
	}
	
	Error subscribeSync (const Uri & uri) {
		counter.mark();
		Error e = client->subscribe(uri.host(), ds::Subscribe(uri.path()),
				dMemFun (this, &BasicDataSharingPeer::onSubscribeReply),
				dMemFun (this, &BasicDataSharingPeer::onNotify),
				100);
		if (e) return e;
		counter.wait();
		return lastSubscribeReply.err;
	}
	
	Error cancelSubscription (const Uri & uri) {
		return client->cancelSubscription(uri);
	}
	
	static bool alwaysFalse () { return false; }

	void denyPermissions () {
		server->checkPermissions() = sf::bind (&alwaysFalse);
	}

	// Callbacks
	
	void onRequestReply (const HostId& sender, const ds::RequestReply & reply, const ByteArrayPtr & data){
		receivedData      = data;
		lastRequestReply = reply;
		counter.finish();
	}
	
	void onTransmissionReply (const HostId& sender, const ds::RequestReply & reply, const ByteArrayPtr & data){
		if (reply.err) {
			transmissionError = reply.err;
			transmissionState = ERROR;
			counter.finish ();
			return;
		}
		if (!receivedData) receivedData = sf::createByteArrayPtr();
		receivedData->append (*data);
		if (reply.mark == ds::RequestReply::TransmissionStart) {
			transmissionState = Started;
		}
		if (reply.mark == ds::RequestReply::TransmissionFinish){
			transmissionState = Finished;
			counter.finish ();
		}
		if (reply.mark == ds::RequestReply::TransmissionCancel){
			transmissionState = Cancelled;
			counter.finish();
		}
	}
	
	void onSubscribeReply (const HostId & sender, const ds::SubscribeReply & reply){
		lastSubscribeReply = reply;
		counter.finish ();
	}
	
	void onNotify (const HostId & sender, const ds::Notify & notify) {
		lastNotify = notify;
		counter.finish ();
	}
	
	ByteArray sharedData;
	OutstandingDataPromisePtr outstandingDataPromise;
	SubDataPromisePtr subDataPromise;
	ByteArrayPtr receivedData;
	
	ds::RequestReply lastRequestReply;
	ds::SubscribeReply lastSubscribeReply;
	Error transmissionError;
	TransmissionState transmissionState;
	
	ds::Notify lastNotify;
	
	SyncCounter counter;
};


struct DataSharingBasicsScenario : public test::DataSharingScenario {
	virtual ~DataSharingBasicsScenario() {}
	// override
	virtual Peer * createPeer (InterplexBeacon * beacon) { return new BasicDataSharingPeer (beacon); }
	virtual BasicDataSharingPeer * peer (int i) { assert (i < mNodeCount); return (BasicDataSharingPeer*) mPeers[i]; }
};

