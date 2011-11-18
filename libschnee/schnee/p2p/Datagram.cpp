#include "Datagram.h"
#include <schnee/tools/Log.h>

// For htonl() and ntohl()
#ifdef WIN32
#include "WinSock.h"
#else
extern "C" {
#include <arpa/inet.h>
}
#endif


namespace sf {

ByteArrayPtr Datagram::encode () const {
	// simple encoding headerLength, contentLength, header, content
	// both with 4 bytes
	size_t headerLength  = mHeader ? mHeader->size() : 0;
	if (headerLength > 2147483647) {
		Log (LogError) << LOGID << "Header to long!" << std::endl;
		assert (false);
		return ByteArrayPtr();
	}
	size_t contentLength = mContent ? mContent->size() : 0;
	if (contentLength > 2147483647) {
		Log (LogError) << LOGID << "Content to long" << std::endl;
		assert (false);
		return ByteArrayPtr();
	}
	uint32_t hln = htonl ((uint32_t)headerLength);
	uint32_t cln = htonl ((uint32_t)contentLength);

	ByteArrayPtr dest = createByteArrayPtr ();
	dest->reserve(8 + headerLength + contentLength);
	dest->append((const char*) &hln, 4);
	dest->append((const char*) &cln, 4);
	if (mHeader)
		dest->append(*mHeader);
	if (mContent)
		dest->append(*mContent);
	assert (dest->size() == 8 + headerLength + contentLength);
	return dest;
}

Error Datagram::decodeFrom (const ByteArray & source, long * bytes) {
	if (source.size() < 8) return sf::error::NotEnough;
	uint32_t headerLength = ntohl (*((const uint32_t*) (source.const_c_array())));
	uint32_t contentLength = ntohl (*((const uint32_t*) (source.const_c_array()+4)));
	if (headerLength > 2147483647 || contentLength > 2147483647){
		Log (LogError) << LOGID << "Header/Content length to big" << std::endl;
		return error::InvalidArgument;
	}

	if (source.size () >= headerLength + contentLength + 8){
		// we can decode it all
		mHeader = sf::createByteArrayPtr();
		mHeader->assign (source.begin() + 8, source.begin() + 8 + headerLength);
		mContent = sf::createByteArrayPtr();
		mContent->assign (source.begin() + 8 + headerLength, source.begin() + 8 + headerLength + contentLength);
	} else {
		return error::NotEnough;
	}
	if (bytes) *bytes = headerLength + contentLength + 8;
	return NoError;
}

sf::Error Datagram::sendTo (ChannelPtr channel) const {
	if (channel->error()) return channel->error();

	sf::ByteArrayPtr chunk = encode ();
	if (!chunk) return error::TooMuch;

	Error err = channel->write (chunk);
	return err;
}

}
