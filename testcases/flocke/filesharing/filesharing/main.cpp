#include <schnee/schnee.h>
#include <schnee/test/PseudoRandom.h>
#include <schnee/test/test.h>
#include <schnee/test/initHelpers.h>
#include <schnee/test/StandardScenario.h>
#include <schnee/tools/Log.h>
#include <flocke/filesharing/FileSharing.h>
#include <flocke/filesharing/FileGetting.h>
#include <flocke/sharedlists/SharedListServer.h>
#include <flocke/sharedlists/SharedListTracker.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include <boost/filesystem/operations.hpp>
namespace fs = boost::filesystem;

using namespace sf;

/*
 * Test Description
 * - Generate a scenario with full FileSharing/FileGetting, DataSharing/DataGetting/InterplexBeacon instances
 *
 * - Let #1 share a file
 * - Transfer file, check integrity
 * - Transfer unknown file, check error
 * - Transfer directory content - check content
 * - Transfer files in directory - check content
 * - Transfer unknown files in directory - check error message
 * - Try to escape from sandbox, check success (with files and within directory contents)
 *
 *
 * --> Note: the scenario shall be in libflocke or libflocketest or similar.
 */

/// A file sharing scenario
class FileSharingScenario : public test::DataSharingScenario {
public:
	virtual ~FileSharingScenario () {}
	struct FileSharingPeer : public DataSharingPeer, public sf::DelegateBase {
		sf::FileSharing * sharing;
		sf::FileGetting * getting;
		sf::SharedListServer  * listServer;
		sf::SharedListTracker * listTracker;

		FileSharingPeer (InterplexBeacon * beacon) : DataSharingPeer (beacon) {
			SF_REGISTER_ME;
			listServer  = new SharedListServer (server);
			listTracker = new SharedListTracker (client);
			
			Error e = listServer->init();
			tassert1 (!e);
			e = listTracker->init();
			tassert1 (!e);
			
			sharing = new FileSharing(server, listServer);
			getting = new FileGetting(client);

			e = sharing->init();
			tassert (!e, "FileSharing Should init without problems");

			e = getting->init();
			tassert (!e, "FileGetting should init without problems");
		}

		~FileSharingPeer () {
			SF_UNREGISTER_ME;
			delete sharing;
			delete getting;
			delete listServer;
			delete listTracker;
		}

		/// Executes suc if result is NoError, notSuc otherwise
		void afterSuccessDo (sf::Error result, const sf::VoidDelegate& suc, const sf::VoidDelegate & notSuc) {
			if (!result) suc();
			else {
				if (notSuc) notSuc();
			}
		}
		
		void reportError (Error error, const sf::String & msg) {
			sf::Log (LogError) << LOGID << "Error " << toString (error) << " happened (" << msg << ")" << std::endl;
			tassert (false, "Should not come here");
		}
		
		sf::Error trackShared (const sf::HostId & other) {
			return listTracker->trackShared(other);
		}
		
		sf::Error autoLiftTrackShared (const sf::HostId & other) {
			if (beacon->connections().channelLevel(other) < 10){
				sf::VoidDelegate suc    = abind (sf::dMemFun (this, &FileSharingPeer::trackShared), other);
				sf::VoidDelegate notSuc = abind (sf::dMemFun (this, &FileSharingPeer::reportError), error::LiftError, sf::String ("Could not build connection in order to get files"));
				sf::ResultCallback cb = sf::abind (memFun (this, &FileSharingPeer::afterSuccessDo), suc, notSuc);
				return beacon->connections().liftConnection(other, cb, 1000);
			}
			return trackShared(other);
		}
	};
	FileSharingPeer * peer (int i) { return (FileSharingPeer*) mPeers[i]; }
	FileSharingPeer * server () { return (FileSharingPeer*) mServer; }

	virtual Peer * createPeer (InterplexBeacon * beacon) { return new FileSharingPeer (beacon); }
};

void checkRequestReply (const HostId & sender, const ds::RequestReply & reply, const sf::ByteArrayPtr & data){
	if (data)
		Log (LogInfo) << LOGID << "Got data " << *data << std::endl;
	tassert (reply.err == NoError, "Must not fail");
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;

#ifndef WIN32
	{
		char cwdbuf[4096];
		char * x = getcwd (cwdbuf, 4096);
		printf ("Current working directory: %s\n", x ? x : "<unknown>");
	}
#endif
	
	typedef sf::FileGetting::TransferInfoMap TransferInfoMap;
	typedef sf::FileGetting::TransferInfo TransferInfo;
	
	// Testdata has to lie in testbed
	// it is generated via python script
	if (!fs::exists ("testbed")){
		std::cerr << "Test folder does not exist" << std::endl;
		return 1;
	}

	{
		// Test 1.1. Sharing files
		FileSharingScenario s;
		tassert (!s.initConnectAndLift(3), "Must init and connect");

		sf::HostId shost = s.server()->hostId();

		sf::Error err = s.server()->sharing->share("A.file", "testbed/a");
		tassert1 (!err);
		err = s.server()->sharing->share ("emptyfile", "testbed/emptyfile");
		tassert1 (!err);
		err = s.server()->sharing->share ("B.dir", "testbed/dir1");
		tassert1 (!err);

		{
			FileSharing * fs = s.server()->sharing;
			FileSharing::FileShareInfo info;
			Error e = fs->putInfo("A.file",&info);
			tassert1 (!e && info.shareName == "A.file" && info.type == DirectoryListing::File);
			e = fs->putInfo("B.dir", &info);
			tassert1 (!e && info.shareName == "B.dir" && info.type == DirectoryListing::Directory);
		}

		err = s.peer(0)->listTracker->trackShared(shost);
		tassert (!err, "Should be no problem to track");
		test::millisleep_locked (500);

		sf::SharedListTracker::SharedListMap slm = s.peer(0)->listTracker->sharedLists();
		tassert1 (slm.count(shost) > 0);

		sf::SharedList list = slm[shost];
		tassert1 (list.count ("A.file"));
		tassert1 (list.count ("B.dir"));
		tassert (list["A.file"].desc.user == "file", "wrong user data");
		tassert (list["B.dir"].desc.user == "dir", "wrong user data");

		
		s.peer(0)->getting->setDestinationDirectory("testbed/save");

		// 1.2. Copying test
		{
			sf::Path path = list["A.file"].path;
			AsyncOpId opId;
			Error err = s.peer(0)->getting->request (Uri (shost, path), &opId);
			if (err) {
				fprintf (stderr, "Error on getting file=%s\n", toString (err));
			}
			tassert (!err, "Should find file");
			test::millisleep_locked (500);
			TransferInfoMap transfers = s.peer(0)->getting->transfers();
			tassert (transfers.count (opId) > 0, "Must have transfer in its map");
			tassert1 (transfers[opId].state == TransferInfo::FINISHED);
		}
		// 1.2.1. Copying an empty file (regression)
		{
			sf::Path path = list["emptyfile"].path;
			AsyncOpId opId;
			Error err = s.peer(0)->getting->request (Uri (shost, path), &opId);
			tassert (!err, "Should find file");
			test::millisleep_locked (500);
			TransferInfoMap transfers = s.peer(0)->getting->transfers();
			tassert (transfers.count (opId) > 0, "Must have transfer in its map");
			tassert1 (transfers[opId].state == TransferInfo::FINISHED);
			tassert (transfers[opId].transferred == 0, "File must be empty");
		}
		
		// 1.3. Copying the same twice; request shall be successful (will add an number to the name)
		{
			AsyncOpId opId;
			Error err = s.peer(0)->getting->request (Uri (shost, list["A.file"].path), &opId);
			tassert (!err, "should be no problem to start transfer twice");
			test::millisleep_locked (500);
			TransferInfoMap transfers = s.peer(0)->getting->transfers();
			tassert (transfers.count (opId) > 0, "Must have Transfer in its map");
			tassert1 (transfers[opId].state == TransferInfo::FINISHED);
		}

		// 1.4. Copying a sub file of a directory
		{
			AsyncOpId opId;
			Error err = s.peer(0)->getting->request (Uri (shost, list["B.dir"].path + Path("a")), &opId);
			tassert (!err, "should be no problem to start transfer");
			test::millisleep_locked (500);
			TransferInfoMap transfers = s.peer(0)->getting->transfers();
			tassert (transfers.count(opId) > 0, "Must have transfer in its map");
			tassert1 (transfers[opId].state == TransferInfo::FINISHED);
		}
		
		// 1.5 Requesting a glob from a directory (via DataSharingClient, one abstraction level beyond)
		{
			ds::Request r; 
			r.path  = list["B.dir"].path;
			r.user = "glob"; 
			r.mark = ds::Request::Transmission;
			err = s.peer(0)->client->request (shost, r, &checkRequestReply);
			tassert1 (!err);
			test::millisleep_locked (500);
		}
		// 1.6 Transfer the whole directory
		{
			AsyncOpId opId;
			Error err = s.peer(0)->getting->requestDirectory (Uri (shost, list["B.dir"].path), &opId);
			tassert (!err, "should be no problem to start directory transfer");
			test::millisleep_locked (5000);
			TransferInfoMap transfers = s.peer(0)->getting->transfers();
			tassert (transfers.count(opId) > 0, "must have transfer in its map");
			tassert1 (transfers[opId].state == TransferInfo::FINISHED);
		}
		// 1.7 Transfer a whole subdirectory
		{
			AsyncOpId opId;
			Error err = s.peer(0)->getting->requestDirectory (Uri (shost, list["B.dir"].path + Path ("dir2")), &opId);
			tassert (!err, "should be no problem to start directory transfer");
			test::millisleep_locked (5000);
			TransferInfoMap transfers = s.peer(0)->getting->transfers();
			tassert (transfers.count(opId) > 0, "must have transfer in its map");
			tassert1 (transfers[opId].state == TransferInfo::FINISHED);
		}
		
	}
	{
		// Test Right Management
		FileSharingScenario scenario;
		scenario.initConnectAndLift(2,false,true);
		FileSharing * sharing = scenario.peer(0)->sharing;
		Error e = sharing->share("A.file", "testbed/a", true, UserSet());
		tassert1 (!e);
		e = sharing->share("B.file", "testbed/b", false,UserSet());
		tassert1 (!e);
		UserSet set;
		set.insert ("blub");
		e = sharing->share("C.file", "testbed/dir1/c", false,set);
		tassert1 (!e);
		// free for all
		e = sharing->checkPermissions("blub", "A.file");
		tassert1 (e == NoError);
		// free for nobody
		e = sharing->checkPermissions("blub", "B.file");
		tassert1 (e == error::NoPerm);
		// free for users (also this user)
		e = sharing->checkPermissions("blub", "C.file");
		if (e != NoError) {
			fprintf (stderr, "Current Sharing Table: %s\n", sf::toJSONEx(sharing->shared(), sf::INDENT | sf::COMPACT).c_str());
		}
		tassert1 (e == NoError);
		// not foudn
		e = sharing->checkPermissions("blub", "D.file");
		tassert1 (e == error::NotFound);
	}


	return 0;
}
