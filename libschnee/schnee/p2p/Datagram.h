#pragma once

#include <schnee/sftypes.h>
#include <schnee/Error.h>
#include <schnee/net/Channel.h>

namespace sf {

/** A single packet to be send between different hosts
 * It consists of a header and a content package
 * And can be encoded into a single data packet
 */
class Datagram {
public:
	Datagram (const ByteArrayPtr & header = ByteArrayPtr (), const ByteArrayPtr & content = ByteArrayPtr ()) : mHeader (header), mContent (content){
	}

	/// Returns header part
	const ByteArrayPtr & header () const { return mHeader; }

	/// Returns data part
	const ByteArrayPtr & content() const { return mContent; }

	/// Initializes a datagram from a RPC command structure
	template <class Cmd> static Datagram fromCmd (const Cmd & cmd, const ByteArrayPtr & content = ByteArrayPtr()) {
		return Datagram (sf::createByteArrayPtr(toJSONCmd (cmd)), content);
	}

	/// Encodes into one ByteArrayPtr
	/// May only fail if data size is much to high
	/// Then it returns 0
	ByteArrayPtr encode () const;

	/// Decodes a bytearray into a datagram
	/// If bytes is not null it will be set to the number of bytes consumed
	Error decodeFrom (const ByteArray & source, long * bytes = 0);

	/// Returns size the Datagram will have when it is encoded
	long encodedSize () const {
		long size = 8; // prefix
		if (mHeader)  size += (long) mHeader->size();
		if (mContent) size += (long) mContent->size();
		return size;
	}

	/// Returns size of the content of the datagram.
	long contentSize () const { return (long) (mContent ? mContent->size() : 0); }

	/// Sends one datagram to a channel
	sf::Error sendTo (ChannelPtr channel) const;

	friend class DatagramReader;
private:
	ByteArrayPtr mHeader;
	ByteArrayPtr mContent;
};



}
