#include "UDTChannelConnector.h"
#include <schnee/net/Tools.h>

namespace sf {

UDTChannelConnector::UDTChannelConnector() {
	SF_REGISTER_ME;
	mRemoteTimeOutMs = 10000;
	mPunchRetryTimeMs = 1000;
	mMaxPunchTrys     = 5;
}

UDTChannelConnector::~UDTChannelConnector() {
	SF_UNREGISTER_ME;
}

sf::Error UDTChannelConnector::createChannel (const HostId & target, const ResultCallback & callback, int timeOutMs) {
	LockGuard guard (mMutex);
	CreateChannelOp * op = new CreateChannelOp (sf::regTimeOutMs(timeOutMs));
	op->callback = callback;
	op->setId(genFreeId_locked ());
	op->target = target;
	op->connector = true;
	startConnecting_locked (op);
	return NoError;
}

CommunicationComponent * UDTChannelConnector::protocol () {
	return this; // we are the protocol itsel.f
}

void UDTChannelConnector::setHostId (const sf::HostId & id) {
	LockGuard guard (mMutex);
	mHostId = id;
}

void UDTChannelConnector::startConnecting_locked (CreateChannelOp * op) {
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
	add_locked (op);
}

void UDTChannelConnector::onEchoClientResult (Error result, AsyncOpId id) {
	LockGuard guard (mMutex);
	CreateChannelOp * op;
	getReady_locked (id, CREATE_CHANNEL, &op);
	if (!op) return; // die.
	if (op->state() != CreateChannelOp::WaitOwnAddress) {
		Log (LogInfo) << LOGID << "Ignoring echo result " << toString(result) << ", as being in wrong state " << op->state() << " (addr was=" << op->echoClient.address() << ":" << op->echoClient.port() << ")" << std::endl;
		add_locked (op);
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
	add_locked (op);
}

void UDTChannelConnector::udpConnect (AsyncOpId id) {
	LockGuard guard (mMutex);
	CreateChannelOp * op;
	getReady_locked (id, CREATE_CHANNEL, &op);
	if (!op) return;
	if (op->state() != CreateChannelOp::Connecting) {
		// ignoring; maybe a bit further now
		Log (LogInfo) << LOGID << "Canceling punching, already in further state=" << op->state() << std::endl;
		add_locked (op);
		return;
	}
	if (op->sentAck) {
		Log (LogInfo) << LOGID << "Canceling punching, sent ack already" << std::endl;
		add_locked (op);
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

	add_locked (op);
	xcallTimed (abind (dMemFun (this, &UDTChannelConnector::udpConnect), id), futureInMs (mPunchRetryTimeMs));
}

void UDTChannelConnector::onUdpReadyRead (AsyncOpId id) {
	LockGuard guard (mMutex);
	CreateChannelOp * op;
	getReady_locked (id, CREATE_CHANNEL, &op);
	if (!op) return;

	while (op->state() == CreateChannelOp::Connecting) {
		NetEndpoint sender;
		ByteArrayPtr data = op->udpSocket.recvFrom(&sender.address, &sender.port);
		if (!data) {
			add_locked (op);
			return;
		}
		String cmd;
		Deserialization d (*data, cmd);
		if (d.error()) { add_locked (op); return; } // net noise
		Log (LogInfo) << LOGID << mHostId << " recv " << cmd << " " << d << " from " << toJSON (sender)  << "(size=" << data->size() << ")" << std::endl;
		if (cmd == PunchUDP::getCmdName()) {
			PunchUDP punch;
			bool suc = punch.deserialize (d);
			if (suc)
				onUdpRecvPunch_locked (sender, op, punch);
		} else
		if (cmd == PunchUDPReply::getCmdName()){
			PunchUDPReply reply;
			bool suc = reply.deserialize (d);
			if (suc)
				onUdpRecvPunchReply_locked (sender, op, reply);
		} else
		if (cmd == AckUDP::getCmdName()) {
			AckUDP ack;
			bool suc = ack.deserialize (d);
			if (suc)
				onUdpRecvAck_locked (sender, op, ack);
		} else
			Log (LogInfo) << LOGID << "Strange protocol, recv=" << *data << std::endl;
	}
	Log (LogInfo) << LOGID << "Ignoring ready read, changed state=" << op->state() << std::endl;
	add_locked (op);
}

void UDTChannelConnector::onUdpRecvPunch_locked (const NetEndpoint & from, CreateChannelOp * op, const PunchUDP & punch) {
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

void UDTChannelConnector::onUdpRecvPunchReply_locked (const NetEndpoint & from, CreateChannelOp * op, const PunchUDPReply & reply) {
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

void UDTChannelConnector::onUdpRecvAck_locked (const NetEndpoint & from, CreateChannelOp * op, const AckUDP & ack) {
	if (ack.remote != mHostId || ack.local != op->target) {
		Log (LogInfo) << LOGID << "Discarding noise from other clients " << toJSONCmd (ack) << std::endl;
		add_locked (op);
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
	op->udtSocket->connectRendezvousAsync(from.address, from.port, aOpMemFun (op, &UDTChannelConnector::onUdtConnectResult_locked));
}

void UDTChannelConnector::onUdtConnectResult_locked (CreateChannelOp * op, Error result) {
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
	if (op->connector) {
		e = op->tlsChannel->clientHandshake(TLSChannel::DH, aOpMemFun (op, &UDTChannelConnector::onTlsHandshake_locked));
	} else {
		e = op->tlsChannel->serverHandshake(TLSChannel::DH, aOpMemFun (op, &UDTChannelConnector::onTlsHandshake_locked));
	}
	if (e) {
		Log (LogWarning) << LOGID << "Warning TLS failed at the early beginning " << toString (e) << std::endl;
		if (op->callback){
			xcall (abind (op->callback, e));
		}
		delete op;
		return;
	}
	add_locked (op);
}

void UDTChannelConnector::onTlsHandshake_locked (CreateChannelOp * op, Error result) {
	Log (LogInfo) << LOGID << "TLS Handshake result to " << op->target << " Result: " << toString (result) << std::endl;
	if (result) {
		Log (LogWarning) << LOGID << "TLS Handshake to " << op->target << "(" << op->id() << ") failed, cause: " << toString (result) << std::endl;
		if (op->callback) {
			xcall (abind (op->callback, result));
		}
		delete op;
		return;
	}
	op->setState (CreateChannelOp::Authenticating);
	op->authProtocol.finished() = aOpMemFun (op, &UDTChannelConnector::onAuthResult_locked);
	op->authProtocol.init(op->tlsChannel, mHostId);
	if (op->connector){
		op->authProtocol.connect(op->target, op->lastingTimeMs());
	} else {
		op->authProtocol.passive(op->target, op->lastingTimeMs());
	}
	add_locked (op);
}

void UDTChannelConnector::onAuthResult_locked (CreateChannelOp * op, Error result) {
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
	LockGuard guard (mMutex);
	CreateChannelOp * op = new CreateChannelOp (sf::regTimeOutMs (mRemoteTimeOutMs));
	op->setId (genFreeId_locked());
	op->connector = false;
	op->remoteId   = request.id;
	op->remoteInternAddresses  = request.intern;
	if (request.extern_.valid())
		op->remoteExternAddresses.push_back (request.extern_);
	guessSomeExternAddresses_locked (op);
	op->target = sender;
	startConnecting_locked (op);
}

void UDTChannelConnector::onRpc (const HostId & sender, const RequestUDTConnectReply & reply, const ByteArray & data) {
	LockGuard guard (mMutex);
	CreateChannelOp * op;
	getReadyInState_locked (reply.id, CREATE_CHANNEL, CreateChannelOp::WaitForeign, &op);
	if (!op) return;
	op->remoteInternAddresses  = reply.intern;
	if (reply.extern_.valid())
		op->remoteExternAddresses.push_back (reply.extern_);
	guessSomeExternAddresses_locked (op);
	op->remoteId              = reply.localId;
	op->setState (CreateChannelOp::Connecting);
	add_locked (op);
	xcall (abind (dMemFun (this, &UDTChannelConnector::udpConnect), reply.id));
}

void UDTChannelConnector::guessSomeExternAddresses_locked (CreateChannelOp * op) {
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
