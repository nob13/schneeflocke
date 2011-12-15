#include <schnee/test/test.h>
#include <schnee/p2p/DataSharingServer.h>
#include <schnee/p2p/DataPromise.h>
#include <schnee/net/Channel.h>
#include <schnee/tools/async/Notify.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/test/StandardScenario.h>

using namespace sf;

/// A test channel working on an outgoing and ingoing buffer
class BufferChannel : public Channel {
public:

	/// Eat something what was written
	ByteArrayPtr consume (long maxSize = -1) {
		if (mIngoingBuffer.empty()) return ByteArrayPtr();
		ByteArrayPtr result = createByteArrayPtr();
		if (maxSize < 0 || maxSize >= (int64_t) mIngoingBuffer.size()){
			result->swap (mIngoingBuffer);
			return result;
		}
		result->assign (mIngoingBuffer.begin(), mIngoingBuffer.begin() + maxSize);
		mIngoingBuffer.l_truncate (maxSize);
		notifyAsync (mChanged);
		return result;
	}

	/// Feed something to be send
	Error feed (const ByteArrayPtr & data) {
		mOutgoingBuffer.append(*data);
		notifyAsync (mChanged);
		return NoError;
	}

	// Implementation of Channel
	virtual sf::Error error () const { return NoError; }
	virtual State state () const { return Connected; }
	virtual Error write (const ByteArrayPtr& data, const ResultCallback & callback = ResultCallback()){
		mIngoingBuffer.append (*data);
		notifyAsync (callback, NoError);
		notifyAsync (mChanged);
		return NoError;
	}
	// Hack: return ByteArrayPtr () if there is nothing to read
	virtual sf::ByteArrayPtr read (long maxSize = -1) {
		if (mOutgoingBuffer.empty()){
			return sf::ByteArrayPtr ();
		}
		sf::ByteArrayPtr result = createByteArrayPtr ();
		if (maxSize == 0)
			return result;
		if (maxSize < 0 || maxSize >= (int64_t) mOutgoingBuffer.size()) {
			result->swap(mOutgoingBuffer);
			return result;
		}
		result->assign (mOutgoingBuffer.begin(), mOutgoingBuffer.begin() + maxSize);
		mOutgoingBuffer.l_truncate(maxSize);
		return result;
	}
	virtual void close (const ResultCallback & resultCallback = ResultCallback ()) {
		notifyAsync (resultCallback, error::NotSupported);
	}
	virtual const char * stackInfo () const {
		return "buffer";
	}
	virtual sf::VoidDelegate & changed () {
		return mChanged;
	}
private:
	VoidDelegate mChanged;
	/// Stuff to be read
	ByteArray mOutgoingBuffer;
	/// Stuff to be written
	ByteArray mIngoingBuffer;
};
typedef shared_ptr<BufferChannel> BufferChannelPtr;

/// An async source
class ChannelDataSource : public DataPromise, public DelegateBase {
public:
	ChannelDataSource (ChannelPtr channel) {
		SF_REGISTER_ME
		mChannel = channel;
		mReady = false;
		mPosition = 0;
		mWritePosition = 0;
		mChannel->changed() = dMemFun (this, &ChannelDataSource::onChanged);
	}
	~ChannelDataSource () {
		SF_UNREGISTER_ME;
	}
	virtual bool ready () const {
		LockGuard guard (mMutex);
		return mReady;
	}
	virtual sf::Error read (const ds::Range & range, ByteArray & dst) {
		LockGuard guard (mMutex);
		ds::Range realRange = ds::Range (range.from - mPosition, range.to - mPosition);

		if (realRange.from != 0)
			return error::NotSupported; // no seeking
		ByteArrayPtr data = mChannel->read (range.to);
		if (data) {
			dst.swap (*data);
		}
		if (!data || data->empty()){
			mReady = false;
		}
		mPosition+=dst.size();
		return mChannel->error();
	}
	virtual sf::Error read (ByteArray & dst) {
		return error::NotSupported;
	}
	sf::Error write (const ds::Range & range, const ByteArrayPtr & data) {
		LockGuard guard (mMutex);
		ds::Range realRange = ds::Range (range.from - mWritePosition, range.to - mWritePosition);
		if (realRange.from != 0) {
			// no seeking
			return error::NotSupported;
		}
		mWritePosition+=data->size();
		return mChannel->write (data);
	}
	virtual int64_t size() const {
		// unknown
		return -1;
	}
private:
	void onChanged () {
		LockGuard guard (mMutex);
		mReady = (bool) (mChannel->read(0));
	}
	mutable Mutex mMutex;
	bool mReady;
	ChannelPtr mChannel;
	int64_t mPosition;
	int64_t mWritePosition;
};

void writeGateWay (const HostId & sender, const ds::RequestReply & reply, const sf::ByteArrayPtr & data, BufferChannelPtr gateWay){
	if (data)
		gateWay->write (data);
}

int testConnectivity () {
	/*
	 * B requests A's Data
	 * Must get it delivered as soon it is ready.
	 */
	test::DataSharingScenario scenario;
	scenario.initConnectAndLift (2,false,true);

	BufferChannelPtr a (new BufferChannel());
	BufferChannelPtr b (new BufferChannel());
	scenario.peer(0)->server->share ("bla", DataSharingServer::createFixedPromise (DataPromisePtr (new ChannelDataSource (a))));
	ds::Request r;
	r.mark = ds::Request::Transmission;
	r.path = "bla";
	scenario.peer(1)->client->request (scenario.peer(0)->hostId(), r, abind (&writeGateWay, b));

	ByteArray testData1 ("Hello World");
	ByteArray testData2 ("How are you?");
	ByteArray testData = testData1; testData.append (testData2);

	test::millisleep_locked (100);
	a->feed (sf::createByteArrayPtr (testData1));
	test::millisleep_locked (100);
	a->feed (sf::createByteArrayPtr (testData2));
	test::millisleep_locked (100);

	ByteArrayPtr p = b->consume();
	tcheck1 (p && *p == testData);

	return 0;
}

int main (int argc, char * argv[]) {
	schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	testcase_start();
	testcase (testConnectivity());
	testcase_end();
}
