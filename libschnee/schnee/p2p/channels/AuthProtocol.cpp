#include "AuthProtocol.h"

#include <schnee/net/impl/IOService.h>
#include <schnee/tools/Log.h>
#include <schnee/tools/Serialization.h>
#include <schnee/tools/Deserialization.h>
#include <schnee/p2p/Datagram.h>
#include <schnee/schnee.h>

#include <assert.h>

namespace sf {

AuthProtocol::AuthProtocol (ChannelPtr channel, const HostId & me) {
	SF_REGISTER_ME;
	init (channel, me);
}

AuthProtocol::AuthProtocol () {
	SF_REGISTER_ME;
}

void AuthProtocol::init (ChannelPtr channel, const sf::HostId & me) {
	LockGuard guard (mMutex);
	sf::cancelTimer(mTimer);
	mMe        = me;
	mChannel   = channel;
	mTimer     = TimedCallHandle();
	mWasActive = false;
}


AuthProtocol::~AuthProtocol () {
	SF_UNREGISTER_ME;
}

void AuthProtocol::connect (const HostId & other, int timeOutInMs) {
	sf::LockGuard guard (mMutex);
	cancelTimer (mTimer);

	CreateChannel cmd;
	cmd.from    = mMe;
	cmd.to      = other;
	cmd.version = schnee::version();
	
	mOther = other;
	mChannel->changed () = dMemFun (this, &AuthProtocol::onChannelChange);
	Error err = Datagram::fromCmd(cmd).sendTo (mChannel);
	if (err) {
		mState = AUTH_ERROR;
		xcall (abind (dMemFun (this, &AuthProtocol::onError), error::ConnectionError));
		return;
	}
	mState = SENT_CREATE_CHANNEL;
	mTimer = xcallTimed (dMemFun (this, &AuthProtocol::onTimeOut), sf::regTimeOutMs (timeOutInMs));
	mWasActive = true;
}

void AuthProtocol::passive (const HostId & other, int timeOutInMs) {
	sf::LockGuard guard (mMutex);
	mOther = other;
	mChannel->changed () = dMemFun (this, &AuthProtocol::onChannelChange);
	xcall (dMemFun (this, &AuthProtocol::onChannelChange)); // could already received some data
	sf::cancelTimer (mTimer);
	mState = WAIT_FOR_CREATE_CHANNEL;
	mTimer = xcallTimed (dMemFun (this, &AuthProtocol::onTimeOut), sf::regTimeOutMs (timeOutInMs));
	mWasActive = false;
}

void AuthProtocol::onChannelChange () {
	// TODO: Locking cleanup!
	sf::Error err;
	sf::ByteArrayPtr header;
	{
		LockGuard  guard (mMutex);
		if (mState == AUTH_ERROR || mState == TIMEOUT || mState == FINISHED) return;
		// err = datagram.receiveFrom(mChannel);
		err = mDatagramReader.read(mChannel);
		header = mDatagramReader.datagram().header();
	}
	if (err == error::NotEnough) return;
	if (err) {
		onError (error::BadProtocol);
		return;
	}
	ChannelFail fail;
	String cmd;
	sf::Deserialization deserialization (*header, cmd);
	bool isFail = (cmd == ChannelFail::getCmdName()) && fail.deserialize(deserialization);

	switch (mState) {
		case SENT_CREATE_CHANNEL:
			// is this my answer?
			if (checkParams (deserialization, cmd, "createChannelAccept")){
				ChannelAccept accept;
				accept.from = mMe;
				accept.to   = mOther;
				err = Datagram::fromCmd(accept).sendTo (mChannel);
				if (err) {
					onError (error::ConnectionError);
				}else 
					onFinish ();
			} else {
				if (!isFail) {
					// wrong command, or decoding etc.
					ChannelFail fail;
					fail.error = error::BadProtocol;
					Datagram::fromCmd(fail).sendTo(mChannel);
					onError (error::BadProtocol);
				} else {
					onError (fail.error);
				}
			}
		break;
		case WAIT_FOR_CREATE_CHANNEL:
			if (checkParams (deserialization, cmd, "createChannel", mOther.empty())){
				CreateChannelAccept accept;
				accept.from    = mMe;
				accept.to      = mOther;
				accept.version = schnee::version();
				String answerCmd = toJSONCmd (accept); 
				
				err = Datagram::fromCmd(accept).sendTo (mChannel);
				if (err) {
					onError (error::ConnectionError);
				} else {
					mMutex.lock ();
					mState = SENT_CREATE_CHANNEL_ACCEPT;
					mMutex.unlock ();
				}
			} else {
				if (!isFail) {
					// wrong command, or decoding etc.
					ChannelFail fail;
					fail.error = error::BadProtocol;
					Datagram::fromCmd(fail).sendTo(mChannel);
					onError (error::BadProtocol);
				} else {
					onError (fail.error);
				}
			}
		break;
		case SENT_CREATE_CHANNEL_ACCEPT:
			if (checkParams (deserialization, cmd, "channelAccept")){
				// Ok, finished
				onFinish ();
			} else { 
				if (!isFail) {
					// wrong command, or decoding etc.
					ChannelFail fail;
					fail.error = error::BadProtocol;
					Datagram::fromCmd (fail).sendTo(mChannel);
					onError (error::BadProtocol);
				} else {
					onError (fail.error);
				}
			}
		break;
		default:
			assert (!"Should not come here");
			break;
	}
}

void AuthProtocol::onTimeOut () {
	{
		sf::LockGuard guard (mMutex);
		if (mState == FINISHED) return;
		mChannel->changed() = sf::VoidDelegate ();
		mState = TIMEOUT;
		mTimer = TimedCallHandle();
	}
	xcall (abind (mFinished, error::TimeOut));
}

void AuthProtocol::onError  (Error err) {
	LockGuard guard (mMutex);
	mState = AUTH_ERROR;
	mChannel->changed() = sf::VoidDelegate ();

	Error cerr = mChannel->error();
	if (cerr) err = cerr;
	if (!err) err = error::ConnectionError;

	cancelTimer (mTimer);
	if (!mFinished){
		sf::Log (LogError) << LOGID << "No finished handler" << std::endl;
	} else {
		xcall (abind (mFinished, err));
	}
}

void AuthProtocol::onFinish (){
	LockGuard guard (mMutex);
	mState = FINISHED;
	cancelTimer (mTimer);
	mChannel->changed() = sf::VoidDelegate ();
	xcall (abind (mFinished,sf::NoError));
}

bool AuthProtocol::checkParams (const sf::Deserialization & ds, const String & cmd, const String & desiredCommand, bool setOther){
	if (ds.error()) return false;
	if (cmd != desiredCommand) return false;
	bool decode;
	String from, to;
	decode = ds ("from", from);
	decode = decode && ds ("to", to);
	if (!decode) return false;
	if (setOther) {
		mOther = from;
		return to == mMe;
	} else {
		return (from == mOther && to == mMe);
	}
}

}
