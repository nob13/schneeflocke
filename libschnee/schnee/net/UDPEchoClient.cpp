#include "UDPEchoClient.h"
#include <stdio.h>
#include <schnee/tools/ArgSplit.h>
#include <schnee/tools/Log.h>
#include <schnee/tools/Token.h>

namespace sf {

UDPEchoClient::UDPEchoClient (UDPSocket & socket) : mSocket (socket) {
	SF_REGISTER_ME;
	mState = NOSTATE;
}
UDPEchoClient::~UDPEchoClient () {
	SF_UNREGISTER_ME;
	mSocket.readyRead().clear();
}

void UDPEchoClient::start (String echoServer, int echoPort, int timeOutMs) {
	mToken = sf::genRandomToken80 ();
	mSocket.readyRead() = dMemFun (this, &UDPEchoClient::onReadyRead);
	mTimeoutHandle = xcallTimed (dMemFun (this, &UDPEchoClient::onTimeOut), regTimeOutMs(timeOutMs));
	char request [256];
	snprintf (request, 256, "condata %s", mToken.c_str());
	Error e = mSocket.sendTo(echoServer, echoPort, sf::createByteArrayPtr (request));
	if (e) {
		xcall (abind (mResultDelegate, e));
		return;
	}
	mState = WAIT;
}

void UDPEchoClient::onReadyRead () {
	{
		LockGuard guard (mMutex);
		if (mState != WAIT) return; // ignoring; timeout.
		// Check protocol
		String from;
		int fromPort;
		ByteArrayPtr data = mSocket.recvFrom(&from,&fromPort);
		if (!data) {
			Log (LogWarning) << LOGID << "Could not read any data tough readyRead() signal" << std::endl;
			return; // ?
		}
		String toParse (data->const_c_array(), data->size());
		ArgumentList list;
		sf::argSplit (toParse, &list);
		// Format: TOKEN IP-Address Port
		if (list.size() != 4 || list[0] != "condataReply") {
			Log (LogInfo) << LOGID << "Invalid protocol in answer " << *data << ", token=" << list.size() <<  std::endl;
			return;   // invalid protocol
		}
		if (list[1] != mToken){
			Log (LogInfo) << LOGID << "Token mismatch in answer " << *data << std::endl;
			return; // invalid token
		}
		mState = READY;
		mAddress = list[2];
		mPort = atoi (list[3].c_str());
		Log (LogInfo) << LOGID << "Successfully decoded echo answer: " << mAddress << ":" << mPort << " coming from " << from << ":" << fromPort << std::endl;
		sf::cancelTimer(mTimeoutHandle);
	}
	mResultDelegate (NoError);
}

void UDPEchoClient::onTimeOut () {
	LockGuard guard (mMutex);
	if (mState != WAIT) {
		mTimeoutHandle = TimedCallHandle();
		return;
	}
	mSocket.readyRead().clear ();
	mResultDelegate (error::TimeOut);
	mState = TIMEOUT;
	mTimeoutHandle = TimedCallHandle();
}


}
