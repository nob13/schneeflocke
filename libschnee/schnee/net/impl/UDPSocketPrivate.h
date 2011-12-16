#pragma once
#include "../UDPSocket.h"
#include "IOBase.h"
#include "IOService.h"
#include <schnee/tools/async/MemFun.h>
#include <schnee/tools/async/Notify.h>
#include <schnee/tools/Log.h>
#include <schnee/schnee.h>

#include <boost/asio.hpp>
using boost::asio::ip::udp;
using namespace boost::asio;
using boost::system::error_code;

namespace sf {
class UDPSocketPrivate : public IOBase {
public:
	struct OutputElement {
		udp::endpoint dst;
		ByteArrayPtr  data;
		size_t size () { return data->size(); }
	};
	struct InputElement {
		udp::endpoint src;
		ByteArrayPtr data;
		size_t size() { return data->size(); }
	};

	UDPSocketPrivate () : IOBase (IOService::service()), mSocket (IOService::service()) {
		mOutputBufferSize = 0;
		mInputBufferSize  = 0;
		mTransferBufferSize = 20000;
		mTransferBuffer.data = sf::createByteArrayPtr();
		mTransferBuffer.data->resize(mTransferBufferSize);
		mReading = false;
		mWriting = false;
	}

	~UDPSocketPrivate() {

	}

	virtual void onDeleteItSelf () {
		boost::system::error_code ec; // we do not want any exceptions here..
		mSocket.cancel (ec);
		mSocket.close(ec);
		mOutputBuffer.clear ();
		mInputBuffer.clear ();
		mInputBufferSize = 0;
		mOutputBufferSize = 0;
		Log (LogInfo) << LOGID << "(RemoveMe) New output buffer size: " << mOutputBufferSize << std::endl;
		mReadyRead.clear ();
		mErrored.clear ();
		IOBase::onDeleteItSelf ();
	}


	int port () const {
		boost::system::error_code ec;
		udp::endpoint e = mSocket.local_endpoint(ec);
		if (ec) return -1;
		return e.port();
	}

	Error translate (udp::endpoint & dst, const String & address, int port) {
		dst.port(port);
		if (!address.empty()){
			ip::address addr;
			error_code ec;
			addr = ip::address::from_string (address, ec);
			dst.address(addr);
			if (ec) return error::InvalidArgument;
			assert (address == dst.address().to_string());
		}
		return NoError;
	}

	Error bind (int port, const String & address) {
		udp::endpoint ep;
		Error e = translate (ep, address, port);
		if (e) return e;
		error_code ec;
		if (!mSocket.is_open()){
			mSocket.open(udp::v4(), ec);
		}
		if (ec) {
			Log (LogWarning) << LOGID << "Could not open socket: " << ec.message () << std::endl;
			return error::ConnectionError;
		}
		mSocket.bind(ep, ec);
		if (ec == boost::asio::error::address_in_use) return error::ExistsAlready;
		if (ec) {
			Log (LogInfo) << LOGID << "Could not bind socket: " << ec.message() << std::endl;
			return error::Other;
		}
		startAsyncReading ();
		return NoError;
	}

	Error close () {
		error_code ec;
		mSocket.close(ec);
		if (ec) return error::Other;
		return NoError;
	}

	Error sendTo (const String & receiver, int receiverPort, const ByteArrayPtr & data) {
		OutputElement elem;
		Error e = translate (elem.dst, receiver, receiverPort);
		if (e) return e;
		elem.data = data;
		mOutputBuffer.push_back (elem);
		mOutputBufferSize += data->size();
		Log (LogInfo) << LOGID << "(RemoveMe) New output buffer size: " << mOutputBufferSize << std::endl;
		startAsyncWriting ();
		return NoError;
	}

	void startAsyncWriting () {
		if (mWriting) return; // alredy writing
		if (mOutputBuffer.empty()) {
			return;
		}
		const OutputElement & elem = mOutputBuffer.front();
		Log (LogInfo) << LOGID << "Will send " << elem.data->size() << " bytes to " << elem.dst.address().to_string() << ":" << elem.dst.port() << std::endl;
		mSocket.async_send_to(
				boost::asio::buffer (*elem.data),
				elem.dst,
				memFun (this, &UDPSocketPrivate::writeHandler)
		);
		mPendingOperations++;
		mWriting = true;
	}

	void writeHandler (const error_code & ec, std::size_t bytes_transferred) {
		SF_SCHNEE_LOCK;
		Log (LogInfo) << LOGID << "Sent " << bytes_transferred << " bytes" << std::endl;
		mPendingOperations--;
		mWriting = false;
		if (!ec) {
			assert (!mOutputBuffer.empty());
			assert (bytes_transferred == mOutputBuffer.front().size());
			mOutputBuffer.pop_front();
			mOutputBufferSize-= bytes_transferred;
			Log (LogInfo) << LOGID << "(RemoveMe) New output buffer size: " << mOutputBufferSize << std::endl;
			startAsyncWriting ();
		} else {
			setError (error::WriteError, ec.message());
			// Flushing output buffer
			// (on udp we will not found out what has failed)
			mOutputBuffer.clear();
			mOutputBufferSize = 0;
			Log (LogInfo) << LOGID << "(RemoveMe) New output buffer size: " << mOutputBufferSize << std::endl;
		}
		if (ec && mErrored) mErrored ();
	}

	ByteArrayPtr recvFrom (String * from = 0, int * fromPort = 0) {
		ByteArrayPtr result;
		startAsyncReading();
		if (mInputBuffer.empty()) {
			return ByteArrayPtr ();
		}
		InputElement elem = mInputBuffer.front();
		mInputBuffer.pop_front();
		mInputBufferSize-=elem.size();
		error_code ec;
		if (from)     *from     = elem.src.address().to_string(ec);
		if (fromPort) *fromPort = elem.src.port();
		result = elem.data;
		return result;
	}

	void startAsyncReading () {
		if (mInputBufferSize > maxInputBufferSize) {
			mReading = false;
			return; // no space
		}
		if (mReading) return; // already reading
		if (mError) return;   // in error state, won't read
		mSocket.async_receive_from(
				boost::asio::buffer (*mTransferBuffer.data),
				mTransferBuffer.src,
				memFun (this, &UDPSocketPrivate::readHandler)
		);
		mReading = true;
		mPendingOperations++;
	}

	void readHandler (const error_code & ec, std::size_t bytes_transferred) {
		SF_SCHNEE_LOCK;
		Log (LogInfo) << LOGID << "Read " << bytes_transferred << " bytes" << std::endl;
		mPendingOperations--;
		mReading = false;
#ifdef WIN32
		if (ec.value() == 10061){
			// No connection  could be made because the target machine actively refused it
			// just ignoring: this happens if we send udp packets to machines which do not
			// like them: http://permalink.gmane.org/gmane.comp.lib.boost.asio.user/1444
			Log (LogInfo) << LOGID << "Ignoring error " << ec.message () << std::endl;
			startAsyncReading ();
			return;
		}
#endif
		if (ec) {
			setError (error::ReadError, ec.message());
			Log (LogInfo) << LOGID << "Error on reading: " << ec.message () << std::endl;
		} else {
			// add to buffer
			mTransferBuffer.data->resize(bytes_transferred);
			mInputBuffer.push_back (mTransferBuffer);
			mInputBufferSize += bytes_transferred;

			// clear transfer buffer
			mTransferBuffer.src = udp::endpoint ();
			mTransferBuffer.data = createByteArrayPtr();
			mTransferBuffer.data->resize (mTransferBufferSize);

			startAsyncReading ();
		}
		if (ec) notify (mErrored);
		else notify (mReadyRead);
	}

	int datagramsAvailable () {
		return mInputBuffer.size();
	}

	VoidDelegate & readyRead () {
		return mReadyRead;
	}

	VoidDelegate & errored () {
		return mErrored;
	}

private:
	VoidDelegate mReadyRead;
	VoidDelegate mErrored;
	udp::socket mSocket;

	std::deque<OutputElement> mOutputBuffer;
	std::deque<InputElement> mInputBuffer;
	size_t mOutputBufferSize;
	size_t mInputBufferSize;

	InputElement mTransferBuffer;	///< buffer for async reading process

	bool   mReading; // currently reading
	bool   mWriting; // currently writing
	static const size_t maxInputBufferSize;
	size_t mTransferBufferSize;	///< Size of transfer buffer
};

const size_t UDPSocketPrivate::maxInputBufferSize = 4194304;

}
