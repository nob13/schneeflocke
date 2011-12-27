#pragma once

#include <schnee/sftypes.h>
#include <schnee/tools/ByteArray.h>
#include <schnee/Error.h>
#include <sfserialization/autoreflect.h>

namespace sf {
/**
 * A channel represents a one-to-one Connection with Reading/Writing supported.
 *
 * It shall represent an interface for all of our connections (like XMPP, TCP etc.)
 *
 * Channels usually provide an internal buffer.
 * 
 */
class Channel {
public:
	Channel ();
	virtual ~Channel ();

	///@name State
	///@{

	/// Channel is in error state
	/// Valid errors are NoError, ChannelError, Eof, Closed
	virtual sf::Error error () const = 0;

	/// A human readable description of the error
	virtual sf::String errorMessage () const { return ""; }


	enum State { Unconnected, Connecting, Connected, Closing, Other };

	/// Returns the current state of the channel
	virtual State state () const = 0;

	///@}

	///@name IO
	///@{

	/// Writes data into the channel
	/// Asynchronous, calls back if data is sent
	/// Data may not be changed anymore
	virtual Error write (const ByteArrayPtr& data, const ResultCallback & callback = ResultCallback()) = 0;

	/// Read incoming data, up to maxSize
	/// maxsize < 0 --> all available is returned.
	virtual sf::ByteArrayPtr read (long maxSize = -1) = 0;

	/// Closes the Channel
	virtual void close (const ResultCallback & resultCallback = ResultCallback ()) = 0;

	///@}

	///@name Channel Information
	///@{

	/// Generic information about channels
	struct ChannelInfo {
		ChannelInfo () : bandwidth (-1), delay (-1), toNeighbor (false), virtual_ (false), authenticated (false), encrypted (false) {}
		float       bandwidth;	///< Bandwidth or < 0 if unknown
		float       delay;		///< Delay or < 0 if unknown
		bool        toNeighbor;	///< Channel is to a neighbor (very near)
		bool        virtual_;	///< Channel is virtual (e.g. IM Channel)
		bool        authenticated;	///< Channel is authenticated so far
		bool        encrypted;		///< Channel is encrypted
		std::string laddress;	///< Full local address (e.g. IP + Port), if available
		std::string raddress;	///< Full remote address (e.g. IP + Port), if available
		SF_AUTOREFLECT_SERIAL;
	};

	/// Provides information about the channel
	virtual ChannelInfo info () const { return ChannelInfo (); }

	/// Short name of the channel
	/// e.g. tls, tcp
	virtual const char * stackInfo () const = 0;

	typedef sf::shared_ptr<Channel> ChannelPtr;
	/// Returns the next channel in a stack architecture or 0
	virtual const ChannelPtr next () const { return ChannelPtr (); }
	///@}

	///@name Delegates
	///@{
	
	/// Something changed, there was an error or there is data to read.
	/// @note Will not be called if the change is just because of input buffer size change after read/readAll
	virtual sf::VoidDelegate & changed () = 0;
	
	///@}
private:
	// forbidden
	Channel (Channel & other);
	Channel & operator= (const Channel & other);
};

SF_AUTOREFLECT_ENUM (Channel::State);

typedef sf::shared_ptr<Channel> ChannelPtr;

}
