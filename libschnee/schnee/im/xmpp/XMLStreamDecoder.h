#pragma once
#include <schnee/sftypes.h>
#include <schnee/tools/XMLChunk.h>

namespace sf {

/// Implements FSM needed to decode XML streams like in XMPP.
class XMLStreamDecoder {
public:
	enum State { XS_Start, XS_ReadXmlBegin, XS_ReadOpener, XS_Closed, XS_Error };

	XMLStreamDecoder ();
	~XMLStreamDecoder ();

	///@name Controlling
	///@{

	/// Writes in data, during this it will call you back via stateChange + chunkRead.
	Error onWrite (const ByteArray & data);

	///@}

	///@name State
	///@{

	/// Access to the state
	State state () const { return mState; }

		/// The chunk which was received as a stream opener (main XML element, artifically closed)
	const XMLChunk& opener ();

	/// Error code
	Error error ();

	/// Description of an error
	const String& errorText ();

	/// Resets state
	void reset ();

	/// Directly initialize into XS_ReadOpener state
	void resetToPureStream ();

	///@}

	///@name Delegates
	///@{

	/// Get called on state changes
	VoidDelegate & stateChange ()  { return mStateChange; }

	typedef function<void (const XMLChunk & )> ChunkReadDelegate;

	/// A chunk was read
	ChunkReadDelegate & chunkRead () { return mChunkRead; }

	///@}

private:
	XMLStreamDecoder& operator=(const XMLStreamDecoder);
	XMLStreamDecoder (const XMLStreamDecoder &);

	void handleData ();
	State           mState;
	XMLChunk        mOpener;
	ByteArray       mInputBuffer;

	ChunkReadDelegate mChunkRead;
	VoidDelegate      mStateChange;
	Error             mError;
	String            mErrorText;
};

}
