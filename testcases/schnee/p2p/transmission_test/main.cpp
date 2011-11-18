#include <schnee/schnee.h>
#include <schnee/test/initHelpers.h>
#include <schnee/test/StandardScenario.h>
#include <schnee/test/test.h>
#include <schnee/test/PseudoRandom.h>
#include <schnee/p2p/DataSharingClient.h>
#include <schnee/p2p/DataSharingServer.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/tools/Log.h>

using namespace sf;

/*
 * Tests the Transmission functionality in DataSharingClient/DataSharingServer
 *
 * Planned
 * Test1: Simple tranmsission between one server and one client
 * Test2: Transmissions between one server and multiple clients
 * Test3: Multipel Trasnmissions between one server and one client (must use some kind of line sharing)
 * Test3: Dying server
 * Test4: Dying client
 */

/// A share promise with unknown size
/// (Used for testing only)
class UnknownSizedSharePromise : public sf::ByteArrayPromise {
public:
	UnknownSizedSharePromise (const sf::ByteArrayPtr & data) : ByteArrayPromise (data){
		mRealSize = data->size();
	}
	virtual sf::Error read (const ds::Range & range, ByteArray & dst) {
		if (range.to > mRealSize){
			sf::Error e = ByteArrayPromise::read (ds::Range (range.from, mRealSize), dst);
			if (e)  return e;
			return error::Eof;
		} else {
			return ByteArrayPromise::read (range, dst);
		}
	}
	
	virtual sf::Error read (ByteArray & dst) {
		return error::NotSupported;
	}
	
	virtual int64_t size () const { return -1; } // unknown size
private:
	int64_t mRealSize;
};

/// A asynchronous share promise
/// It gives out data only 1 second after initalization
/// (Used for testing only)
class AsyncSharePromise : public sf::ByteArrayPromise {
public:
	AsyncSharePromise (const sf::ByteArrayPtr & data) : sf::ByteArrayPromise (data){
		mStartTime = currentTime();
		mRealSize = data->size();
	}
	bool ready() const {
		if ((currentTime() - mStartTime).total_milliseconds() > 1000) return true;
		return false;
	}
	virtual int64_t size () const { 
		if (ready()) return ByteArrayPromise::size();
		else return -1;
	}

	virtual sf::Error read (const ds::Range & range, ByteArray & dst) {
		if (!ready()){
			tassert1 (false && "Should not come here, we are not ready");
			return error::MultipleErrors;
		}

		// inserting EoF
		if (range.to > mRealSize){
			sf::Error e = ByteArrayPromise::read (ds::Range (range.from, mRealSize), dst);
			if (e)  return e;
			return error::Eof;
		} else {
			return ByteArrayPromise::read (range, dst);
		}
	}
	
	virtual sf::Error read (ByteArray & dst) {
		if (!ready()){
			tassert1 (false && "Should not come here, we are not ready");
			return error::MultipleErrors;
		}
		return error::NotSupported;
	}
	
private:
	Time mStartTime;
	bool mHasData;
	int64_t mRealSize;
};

struct Scenario : public test::DataSharingScenario, public DelegateBase {
	ByteArrayPtr data;
	Uri uri;

	Scenario (){
		SF_REGISTER_ME;
	}

	virtual ~Scenario () {
		SF_UNREGISTER_ME;
	}

	static bool alwaysTrue () { return true; }

	void shareHostFile (bool unknownSize = false, bool asyncPromise = false){
		if (!data) {
			size_t size = 2048 * 1024;
			data = sf::createByteArrayPtr();
			data->resize (size); // 2MB
			test::pseudoRandomData(size, data->c_array());
			uri = Uri (peer(0)->hostId(), "data0");
			Log (LogInfo) << LOGID << "Created sharing data with " << size << " bytes" << std::endl;
		}
		typedef DataSharingServer::SharingPromisePtr SharingPromisePtr;
		SharingPromisePtr promise;
		
		if (asyncPromise){
			promise = DataSharingServer::createFixedPromise(DataPromisePtr(new AsyncSharePromise (data)));
		} else {
			if (unknownSize)
				promise = DataSharingServer::createFixedPromise (DataPromisePtr (new UnknownSizedSharePromise (data)));
			else
				promise = DataSharingServer::createPromise(data);
		}
		
		tassert (peer(0)->server->share (uri.path(), promise) == NoError, "Must have no problem to share data");
	}

	struct ClientTracker {
		ClientTracker () : lastError (NoError), finished (false),unknownSized(false) {}
		ds::Range range;
		ByteArray received;
		sf::Error lastError;
		bool finished;
		bool unknownSized;
		Mutex mutex;
		Condition condition;
		int64_t finalReceivedLength;	///< Transmitted length at the end of a transmission
	};
	typedef shared_ptr<ClientTracker> ClientTrackerPtr;
	typedef std::list<ClientTrackerPtr> TrackerList;
	TrackerList trackers;

	void startTransmission (int user){
		ClientTrackerPtr tracker (new ClientTracker());
		ds::Request r;
		r.path  = uri.path();
		r.mark = ds::Request::Transmission;
		tracker->range = r.range.isDefault() ? ds::Range (0, data->size()) : r.range;
		trackers.push_back (tracker);
		tassert (peer(user)->client->request (
					uri.host(),
					r,
					abind (dMemFun (this, &Scenario::onRequestReply), tracker)
				) == NoError, "Must have no problem to start transmission");

	}

	/// Test whether all current operations are working (or have an error)
	bool allFinished () {
		for (TrackerList::iterator i = trackers.begin(); i != trackers.end(); i++){
			ClientTrackerPtr t (*i);
			LockGuard guard (t->mutex);
			if (!t->finished && !t->lastError) return false;
		}
		return true;
	}

	/// Wait until all transmissions arrived with success
	bool waitAllFinished (int timeMs) {
		return test::waitUntilTrueMs (bind (&Scenario::allFinished, this), timeMs);
	}

	/// Checks whether all transmissions finished AND where successfull
	bool allSuccessfull () {
		for (TrackerList::iterator i = trackers.begin(); i != trackers.end(); i++){
			ClientTrackerPtr t (*i);
			LockGuard guard (t->mutex);
			if (!t->finished && t->lastError) return false;
			if (t->received != *data) return false;
			if (t->finalReceivedLength != (int64_t) data->size()) return false;
		}
		return true;
	}

	void onRequestReply (const HostId & sender, const ds::RequestReply& reply, const sf::ByteArrayPtr& data, ClientTrackerPtr tracker){
		LockGuard guard (tracker->mutex);
		if (reply.err) {
			tracker->lastError = reply.err;
		} else {
			switch (reply.mark) {
			case ds::RequestReply::NoMark:
					Log (LogError) << LOGID << "Strange, receiving no mark " << toJSON (reply) << std::endl;
					break;
				case ds::RequestReply::TransmissionCancel:
					Log (LogError) << LOGID << "Strange, receiving transmission cancel" << std::endl;
					break;
				case ds::RequestReply::TransmissionStart:
				case ds::RequestReply::Transmission:
				case ds::RequestReply::TransmissionFinish:
					if (reply.mark == ds::RequestReply::TransmissionStart){
						if (reply.range == ds::Range (0,-1))
							tracker->unknownSized = true;
						else
							tassert (reply.range == tracker->range, "Range should fit");
					}
					tracker->received.append (*data);
					break;
			}
			if (reply.mark == ds::RequestReply::TransmissionFinish || reply.mark == ds::RequestReply::TransmissionCancel) tracker->finished = true;
			if (reply.mark == ds::RequestReply::TransmissionFinish) tracker->finalReceivedLength = reply.range.to;
		}
		tracker->condition.notify_all ();
	}

};

int main (int argc, char * argv[]){
	schnee::SchneeApp app (argc, argv);
	{
		// Test if scenario shutdowns correctly
		Scenario scenario;
		tassert (scenario.initConnectAndLift(2) == NoError, "Maynotfail");
	}

	{
		printf ("Scenario1\n");
		Scenario scenario;
		tassert (scenario.initConnectAndLift (2) == NoError, "Scenario must start");
		scenario.shareHostFile();
		scenario.startTransmission (1);
		tassert (scenario.waitAllFinished(5000), "Transmission should be done in 5s");
		tassert (scenario.allSuccessfull(), "All transactions shall be successfull");
	}
	{
		printf ("Scenario 1 with XMPP and TCP\n");
		// Scenario1
		Scenario scenario;
		sf::Error result = scenario.initConnectAndLift (2, false, false);
		tassert (result == NoError, "Scenario must start");
		scenario.shareHostFile();
		scenario.startTransmission (1);
		tassert (scenario.waitAllFinished(5000), "Transmission should be done in 5s");
		tassert (scenario.allSuccessfull(), "All transactions shall be successfull");
	}
	{
		printf ("UnknownSize Scenario\n");
		Scenario scenario;
		sf::Error result = scenario.initConnectAndLift (2);
		tassert (result == NoError, "Scenario must start");
		scenario.shareHostFile(true);
		scenario.startTransmission (1);
		tassert (scenario.waitAllFinished(5000), "Transmission should be done in 5s");
		tassert (scenario.allSuccessfull(), "All transactions shall be successfull");
	}
	{
		printf ("Async Scenario\n");
		Scenario scenario;
		sf::Error result = scenario.initConnectAndLift (2);
		tassert (result == NoError, "Scenario must start");
		scenario.shareHostFile(true, true);
		scenario.startTransmission (1);
		tassert (scenario.waitAllFinished(5000), "Transmission should be done in 5s");
		tassert (scenario.allSuccessfull(), "All transactions shall be successfull");
	}
	{
		printf ("One server multiple clients\n");
		Scenario scenario;
		tassert (scenario.initConnectAndLift (8) == NoError, "Scenario must start");
		scenario.shareHostFile();
		for (int i = 1; i < 7; i++)
			scenario.startTransmission (i);
		tassert (scenario.waitAllFinished(5000), "Transmission should be done in 5s");
		tassert (scenario.allSuccessfull(), "All transactions shall be successfull");
	}
	{
		printf ("Multiple Transmissions...\n");
		Scenario scenario;
		tassert (scenario.initConnectAndLift (2) == NoError, "Scenario must start");
		scenario.shareHostFile();
		for (int i = 0; i < 7; i++)
			scenario.startTransmission (1);
		tassert (scenario.waitAllFinished(5000), "Transmission should be done in 5s");
		tassert (scenario.allSuccessfull(), "All transactions shall be successfull");
	}
	return 0;
}
