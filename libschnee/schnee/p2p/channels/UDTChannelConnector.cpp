#include "UDTChannelConnector.h"
#include <schnee/net/Tools.h>

namespace sf {

UDTChannelConnector::UDTChannelConnector() {
	SF_REGISTER_ME;
	mRemoteTimeOutMs = 10000;
	mPunchRetryTimeMs = 1000;
	mMaxPunchTrys     = 5;
	mAuthentication   = 0;
}

UDTChannelConnector::~UDTChannelConnector() {
	SF_UNREGISTER_ME;
}

sf::Error UDTChannelConnector::createChannel (const HostId & target, const ResultCallback & callback, int timeOutMs) {
	CreateChannelOp * op = new CreateChannelOp (sf::regTimeOutMs(timeOutMs));
	op->callback = callback;
	op->setId(genFreeId ());
	op->target = target;
	op->connector = true;
	startConnecting (op);
	return NoError;
}

CommunicationComponent * UDTChannelConnector::protocol () {
	return this; // we are the protocol itsel.f
}

void UDTChannelConnector::setHostId (const sf::HostId & id) {
	mHostId = id;
}

void UDTChannelConnector::startConnecting (CreateChannelOp * op) {
	// Binding to own address
	op->udpSocket.bind();

	// setting local addresses
	std::vector<String> localAddresses;
	net::localIpv4Addresses(&localAddresses, true);
	for (std::vector<String>::const_iterator i = localAddresses.begin(); i != localAddresses.end(); i++){
		op->internAddresses.push_back (NetEndpoint (*i, op->udpSocket.port()));
	}

	Log (LogInfo) << LOGID << "Building channel from " << mHostId << " to " << op->target << " lasting time is " << op->lastingTimeMs() << "ms" << std::endl;
	Log (LogInfo) << LOGID << "Start echoing for UDP socket " << op->udpSocket.port() << std::endl;
	op->echoClient.result() = abind (dMemFun (this, &UDTChannelConnector::onEchoClientResult), op->id());
	// time for echoing must be < 0.5 as both entities must do some echoing.
	// and if echoing fails there is still some chance to get connected.
	op->echoClient.start(mEchoServer.address, mEchoServer.port, op->lastingTimeMs(0.3));
	// waiting for an answer...
	op->setState (CreateChannelOp::WaitOwnAddress);
	addAsyncOp (op);
}

void UDTChannelConnector::onEchoClientResult (Error result, AsyncOpId id) {
	CreateChannelOp * op;
	getReadyAsyncOp (id, CREATE_CHANNEL, &op);
	if (!op) return; // die.
	if (op->state() != CreateChannelOp::WaitOwnAddress) {
		Log (LogInfo) << LOGID << "Ignoring echo result " << toString(result) << ", as being in wrong state " << op->state() << " (addr was=" << op->echoClient.address() << ":" << op->echoClient.port() << ")" << std::endl;
		addAsyncOp (op);
		return;
	}
	Log (LogInfo) << LOGID << "Echoing result: " << toString (result) << " (lasting time=" << op->lastingTimeMs() << ")" << std::endl;

	if (result) {
		Log (LogWarning) << LOGID << "Could not find out remote address: " << toString (result) << std::endl;
		// still continuing; maybe it works without...
	} else {
		op->externAddress.address = op->echoClient.address();
		op->externAddress.port    = op->echoClient.port();
	}

	if (op->connector) {
		RequestUDTConnect request;
		request.id     = id;
		request.intern  = op->internAddresses;
		request.extern_ = op->externAddress;
		request.echoResult = result;

		mCommunicationDelegate->send(op->target, Datagram::fromCmd(request));
		op->setState (CreateChannelOp::WaitForeign);
	} else {
		// not connector
		RequestUDTConnectReply reply;
		reply.id      = op->remoteId;
		reply.localId = op->id();	// reversed, as reply.id has to fit the id of the RequestUDTConnect command.
		reply.intern  = op->internAddresses;
		reply.extern_ = op->externAddress;
		reply.echoResult = result;

		mCommunicationDelegate->send (op->target, Datagram::fromCmd(reply));
		op->setState (CreateChannelOp::Connecting);
		xcall (abind (dMemFun (this, &UDTChannelConnector::udpConnect), op->id()));
	}
	addAsyncOp (op);
}

void UDTChannelConnector::udpConnect (AsyncOpId id) {
	CreateChannelOp * op;
	getReadyAsyncOp (id, CREATE_CHANNEL, &op);
	if (!op) return;
	if (op->state() != CreateChannelOp::Connecting) {
		// ignoring; maybe a bit further now
		Log (LogInfo) << LOGID << "Canceling punching, already in further state=" << op->state() << std::endl;
		addAsyncOp (op);
		return;
	}
	if (op->sentAck) {
		Log (LogInfo) << LOGID << "Canceling punching, sent ack already" << std::endl;
		addAsyncOp (op);
		return; // already through procedure.
	}
	op->udpSocket.readyRead() = abind (dMemFun (this, &UDTChannelConnector::onUdpReadyRead), op->id());
	op->punchCount++;
	if (op->punchCount > mMaxPunchTrys) {
		// lets' fail
		Log (LogInfo) << LOGID << "Maximum punching count reached for " << op->target << std::endl;
		if (op->callback) {
			xcall (abind (op->callback, error::CouldNotConnectHost));
		}
		delete op;
		return;
	}
	// Try all available other side udp endpoints
	for (int i = 0; i < op->otherEndpointCount(); i++) {
		const NetEndpoint & p = op->otherEndpoint (i);
		if (!p.valid()) continue;
		PunchUDP punch;
		punch.id      = op->id();
		punch.otherId = op->remoteId;
		punch.local     = mHostId;
		punch.remote   = op->target;
		punch.num     = i;
		Log(LogInfo) << LOGID << "Sent punch to " << toJSON (p) << " (num=" << i << ") lastingTime=" << op->lastingTimeMs() << std::endl;
		op->udpSocket.sendTo (p.address, p.port, sf::createByteArrayPtr(toJSONCmd (punch)));
	}

	addAsyncOp (op);
	xcallTimed (abind (dMemFun (this, &UDTChannelConnector::udpConnect), id), futureInMs (mPunchRetryTimeMs));
}

void UDTChannelConnector::onUdpReadyRead (AsyncOpId id) {
	CreateChannelOp * op;
	getReadyAsyncOp (id, CREATE_CHANNEL, &op);
	if (!op) return;

	while (op->state() == CreateChannelOp::Connecting) {
		NetEndpoint sender;
		ByteArrayPtr data = op->udpSocket.recvFrom(&sender.address, &sender.port);
		if (!data) {
			addAsyncOp (op);
			return;
		}
		String cmd;
		Deserialization d (*data, cmd);
		if (d.error()) { addAsyncOp (op); return; } // net noise
		Log (LogInfo) << LOGID << mHostId << " recv " << cmd << " " << d << " from " << toJSON (sender)  << "(size=" << data->size() << ")" << std::endl;
		if (cmd == PunchUDP::getCmdName()) {
			PunchUDP punch;
			bool suc = punch.deserialize (d);
			if (suc)
				onUdpRecvPunch (sender, op, punch);
		} else
		if (cmd == PunchUDPReply::getCmdName()){
			PunchUDPReply reply;
			bool suc = reply.deserialize (d);
			if (suc)
				onUdpRecvPunchReply (sender, op, reply);
		} else
		if (cmd == AckUDP::getCmdName()) {
			AckUDP ack;
			bool suc = ack.deserialize (d);
			if (suc)
				onUdpRecvAck (sender, op, ack);
		} else
			Log (LogInfo) << LOGID << "Strange protocol, recv=" << *data << std::endl;
	}
	Log (LogInfo) << LOGID << "Ignoring ready read, changed state=" << op->state() << std::endl;
	addAsyncOp (op);
}

void UDTChannelConnector::onUdpRecvPunch (const NetEndpoint & from, CreateChannelOp * op, const PunchUDP & punch) {
	if (punch.remote != mHostId || punch.local != op->target) {
		Log (LogInfo) << LOGID << "Discarding noise from other clients " << toJSONCmd (punch) << std::endl;
		return;
	}
	PunchUDPReply reply;
	reply.remoteId = op->remoteId;
	reply.id      = op->id();
	reply.local     = mHostId;
	reply.remote   = op->target;
	reply.num     = punch.num;

	op->udpSocket.sendTo (from.address, from.port, createByteArrayPtr (toJSONCmd (reply)));
	Log (LogInfo) << LOGID << "Sent " << toJSONCmd(reply) << " to " << toJSON (from) << std::endl;
}

void UDTChannelConnector::onUdpRecvPunchReply (const NetEndpoint & from, CreateChannelOp * op, const PunchUDPReply & reply) {
	if (reply.remote != mHostId || reply.local  != op->target) {
		Log (LogInfo) << LOGID << "Discarding noise from other clients " << toJSONCmd (reply) << std::endl;
		return;
	}
	op->remoteWorkingAddress = from;
	if (op->sentAck) {
		Log (LogInfo) << LOGID << "Ignoring, sent already ack" << std::endl;
		return;
	}
	// he is receiving me, do send an ack.
	AckUDP ack;
	ack.id      = op->id();
	ack.remoteId = op->remoteId;
	ack.local     = mHostId;
	ack.remote   = op->target;
	ack.num     = reply.num;
	op->udpSocket.sendTo (from.address, from.port, createByteArrayPtr (toJSONCmd (ack)));
	Log (LogInfo) << LOGID << "Sent " << toJSONCmd (ack) << " to " << toJSON (from) << std::endl;
	op->sentAck = true;
}

void UDTChannelConnector::onUdpRecvAck (const NetEndpoint & from, CreateChannelOp * op, const AckUDP & ack) {
	if (ack.remote != mHostId || ack.local != op->target) {
		Log (LogInfo) << LOGID << "Discarding noise from other clients " << toJSONCmd (ack) << std::endl;
		addAsyncOp (op);
	}
	if (!op->sentAck) {
		AckUDP ack;
		ack.id      = op->id();
		ack.remoteId = op->remoteId;
		ack.local     = mHostId;
		ack.remote   = op->target;
		ack.num     = 0; // unknown
		op->udpSocket.sendTo (from.address, from.port, createByteArrayPtr (toJSONCmd (ack)));
		Log (LogInfo) << LOGID << "Sent " << toJSONCmd (ack) << " to " << toJSON (from) << std::endl;
		op->sentAck = true;
	}
	op->recvAck = true;
	// lets create UDT Channels...
	op->setState (CreateChannelOp::ConnectingUDT);
	op->udtSocket = UDTSocketPtr (new UDTSocket());
	op->udtSocket->rebind(&op->udpSocket);
	op->udtSocket->connectRendezvousAsync(from.address, from.port, aOpMemFun (op, &UDTChannelConnector::onUdtConnectResult));
}

void UDTChannelConnector::onUdtConnectResult (CreateChannelOp * op, Error result) {
	Log (LogInfo) << LOGID << mHostId << " UDT connect to " << op->target << " Result: " << toString (result) << std::endl;
	if (result) {
		Log (LogWarning) << LOGID << "Connecting " << op->target << "(" << op->id() << ") with UDT failed, cause: " << toString (result) << std::endl;
		if (op->callback)
			xcall (abind (op->callback, result));
		delete op;
		return;
	}
	op->setState (CreateChannelOp::TlsHandshaking);
	op->tlsChannel = TLSChannelPtr (new TLSChannel (op->udtSocket));
	Error e;

	TLSChannel::Mode mode = authenticationEnabled () ? TLSChannel::X509 : TLSChannel::DH;
	if (mode == TLSChannel::X509) {
		op->tlsChannel->setKey (mAuthentication->certificate(), mAuthentication->key());
		// we do that implicit
		op->tlsChannel->disableAuthentication();
	}

	if (op->connector) {
		e = op->tlsChannel->clientHandshake(mode, op->target, aOpMemFun (op, &UDTChannelConnector::onTlsHandshake));
	} else {
		e = op->tlsChannel->serverHandshake(mode, aOpMemFun (op, &UDTChannelConnector::onTlsHandshake));
	}
	if (e) {
		Log (LogWarning) << LOGID << "Warning TLS failed at the early beginning " << toString (e) << std::endl;
		if (op->callback){
			xcall (abind (op->callback, e));
		}
		delete op;
		return;
	}
	addAsyncOp (op);
}

void UDTChannelConnector::onTlsHandshake (CreateChannelOp * op, Error result) {
	Log (LogInfo) << LOGID << "TLS Handshake result to " << op->target << " Result: " << toString (result) << std::endl;
	if (result) {
		Log (LogWarning) << LOGID << "TLS Handshake to " << op->target << "(" << op->id() << ") failed, cause: " << toString (result) << std::endl;
		if (op->callback) {
			xcall (abind (op->callback, result));
		}
		sf::safeRemove(op->tlsChannel);
		delete op;
		return;
	}
	if (authenticationEnabled()){
		Authentication::CertInfo info = mAuthentication->get(op->target);
		if (info.type != Authentication::CT_PEER) {
			Log (LogProfile) << LOGID << "Could not do TLS authentication as no certificate is stored" << std::endl;
			// there is no sense in doing next operation; won't exchange certificates
			notifyAsync (op->callback, error::AuthError);
			sf::safeRemove(op->tlsChannel);
			delete op;
			return;
		}
		result = op->tlsChannel->authenticate(info.cert.get(), op->target);
		if (result) {
			Log (LogProfile) << LOGID << "Could not TLS authenticate " << op->target << std::endl;
			notifyAsync (op->callback, error::AuthError);
			sf::safeRemove (op->tlsChannel);
			delete op;
			return;
		}
	}
	op->setState (CreateChannelOp::Authenticating);
	op->authProtocol.finished() = aOpMemFun (op, &UDTChannelConnector::onAuthResult);
	op->authProtocol.init(op->tlsChannel, mHostId);
	if (op->connector){
		op->authProtocol.connect(op->target, op->lastingTimeMs());
	} else {
		op->authProtocol.passive(op->target, op->lastingTimeMs());
	}
	addAsyncOp (op);
}

void UDTChannelConnector::onAuthResult (CreateChannelOp * op, Error result) {
	if (result) {
		Log (LogInfo) << LOGID << "Authenticating UDT channel failed: " << toString (result) << std::endl;
		if (op->callback)
			xcall (abind (op->callback, result));
		delete op;
		return;
	}
	// Got it!
	Log (LogInfo) << LOGID << "(" << mHostId << ") Successfull created UDT channel to " << op->target << std::endl;
	xcall (abind (mChannelCreated, op->target, op->tlsChannel, op->connector));
	if (op->callback)
		xcall (abind (op->callback, NoError));
	delete op;
}

void UDTChannelConnector::onRpc (const HostId & sender, const RequestUDTConnect & request, const ByteArray & data) {
	CreateChannelOp * op = new CreateChannelOp (sf::regTimeOutMs (mRemoteTimeOutMs));
	op->setId (genFreeId());
	op->connector = false;
	op->remoteId   = request.id;
	op->remoteInternAddresses  = request.intern;
	if (request.extern_.valid())
		op->remoteExternAddresses.push_back (request.extern_);
	guessSomeExternAddresses (op);
	op->target = sender;
	startConnecting (op);
}

void UDTChannelConnector::onRpc (const HostId & sender, const RequestUDTConnectReply & reply, const ByteArray & data) {
	CreateChannelOp * op;
	getReadyAsyncOpInState (reply.id, CREATE_CHANNEL, CreateChannelOp::WaitForeign, &op);
	if (!op) return;
	op->remoteInternAddresses  = reply.intern;
	if (reply.extern_.valid())
		op->remoteExternAddresses.push_back (reply.extern_);
	guessSomeExternAddresses (op);
	op->remoteId              = reply.localId;
	op->setState (CreateChannelOp::Connecting);
	addAsyncOp (op);
	xcall (abind (dMemFun (this, &UDTChannelConnector::udpConnect), reply.id));
}

void UDTChannelConnector::guessSomeExternAddresses (CreateChannelOp * op) {
	if (op->remoteExternAddresses.empty()) return;
	NetEndpoint ep = op->remoteExternAddresses[0];
	if (!ep.valid()) return;
	op->remoteExternAddresses.push_back(NetEndpoint (ep.address, ep.port + 1)); // works often
	op->remoteExternAddresses.push_back(NetEndpoint (ep.address, ep.port + 2));
	op->remoteExternAddresses.push_back(NetEndpoint (ep.address, ep.port + 3));
	op->remoteExternAddresses.push_back(NetEndpoint (ep.address, ep.port - 1));
	op->remoteExternAddresses.push_back(NetEndpoint (ep.address, ep.port - 2));
}

}
