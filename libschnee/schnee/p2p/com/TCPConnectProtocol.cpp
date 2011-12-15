#include "TCPConnectProtocol.h"

namespace sf {

TCPConnectProtocol::TCPConnectProtocol () {
	SF_REGISTER_ME;
}

TCPConnectProtocol::~TCPConnectProtocol () {
	SF_UNREGISTER_ME;
}


void TCPConnectProtocol::setOwnDetails(const ConnectDetailsPtr & details){
	if (!details) {
		mOwnDetails = ConnectDetailsPtr (new ConnectDetails());
		return;
	}
	mOwnDetails = details;
}

Error TCPConnectProtocol::requestDetails (const HostId & target, const RequestConnectDetailsCallback & callback, int timeOutMs){
	OpId id = genFreeId ();
	RequestConnectDetails req;
	req.id = id;
	Error e = mCommunicationDelegate->send (target, Datagram::fromCmd(req));
	if (e) return e;
	RequestOp * op = new RequestOp (regTimeOutMs(timeOutMs));
	op->cb = callback;
	op->setId(id);
	addAsyncOp (op);
	return NoError;
}

void TCPConnectProtocol::onRpc (const HostId & sender, const RequestConnectDetails & r, const sf::ByteArray & content) {
	ConnectDetailsReply reply;
	reply.id   = r.id;
	reply.error     = mOwnDetails->error;
	reply.port      = mOwnDetails->port;
	reply.addresses = mOwnDetails->addresses;
	mCommunicationDelegate->send (sender, Datagram::fromCmd(reply));
}

void TCPConnectProtocol::onRpc (const HostId & sender, const ConnectDetailsReply & reply, const ByteArray & content) {
	AsyncOp * op = getReadyAsyncOp (reply.id);
	if (!op) {
		Log (LogWarning) << LOGID << "Did not found op " << reply.id  << " (maybe timeouted)" << std::endl;
		return;
	}
	RequestOp * rop = static_cast <RequestOp *> (op);
	ConnectDetailsPtr ptrDetails = ConnectDetailsPtr (new ConnectDetails(reply));
	if (rop->cb){
		rop->cb (reply.error, reply);
	}
	delete rop;
}

}
