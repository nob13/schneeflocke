#pragma once

#include <schnee/net/Channel.h>
#include <schnee/im/IMClient.h>

namespace sf {

class IMDispatcher;

/**
 * Channel implementation for use with Instant Messaging Systems
 */
class IMChannel : public Channel {
public:
	IMChannel (IMDispatcher * dispatcher, const sf::String & id, OnlineState state);
	virtual ~IMChannel ();
	
	/// Returns current online state of im Channel
	OnlineState state () { return mState; }

	// Implementation of Channel
	virtual sf::Error error () const;
	virtual sf::String errorMessage () const;
	virtual State state () const;

	virtual Error write (const ByteArrayPtr& data, const ResultCallback & callback = ResultCallback());
	virtual sf::ByteArrayPtr read (long maxSize);
	virtual void close (const ResultCallback &  resultCallback = ResultCallback());
	virtual ChannelInfo info () const {
		ChannelInfo info;
		info.virtual_ = true;
		return info;
	}
	virtual const char * stackInfo () const { return "im";}
	virtual sf::VoidDelegate & changed () { return mChanged; }

private:
	// API for IMDispatcher

	/// When the lights go out
	void invalidate ();
	/// Received a message
	void pushMessage (const sf::IMClient::Message & m);
	/// Sets the online state
	void setState    (OnlineState state);
	
	/// Sets connecting State
	

	// IM Stuff
	friend class IMDispatcher;
	IMDispatcher * mDispatcher;				///< Our boss
	sf::String mId;							///< IM id
	OnlineState mState;						///< Is the contact online

	// Input queue
	sf::ByteArray mInputBuffer;

	/// Protecting state and input buffer
	mutable sf::Mutex     mMutex;

	// Delegates
	sf::VoidDelegate mChanged;


};

}
