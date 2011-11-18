#pragma once
#include <schnee/net/Channel.h>
#include "Datagram.h"

namespace sf {

/// Small state machine to read datagrams without a peek function in Channels
class DatagramReader {
public:
	enum State { DR_WAITLENGTHS, DR_WAITHEADER, DR_WAITCONTENT, DR_ERROR };
	DatagramReader ();

	State state () { return mState; }

	/// Tries to read a datagram from the given channel
	/// Returns NoError on a successfull read.
	Error read (const ChannelPtr& channel);

	/// The datagram which was read, when read returned NoError;
	const Datagram & datagram () const { return mDatagram; }

	/// clears internal states
	void clear ();
private:
	Error fillBytes (const ChannelPtr & channel, uint32_t count);

	Datagram  mDatagram; ///< the datagram to be filled.
	ByteArray mBuffer; //< small look buffer for unready reads..
	State     mState;
	uint32_t mNextHeaderLength;
	uint32_t mNextContentLength;
};


}
