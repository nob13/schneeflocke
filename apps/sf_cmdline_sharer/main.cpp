#include <schnee/schnee.h>
#include <schnee/tools/ArgSplit.h>
#include <schnee/p2p/InterplexBeacon.h>
#include <schnee/p2p/DataSharingClient.h>
#include <schnee/p2p/DataSharingServer.h>
#include <schnee/tools/ResultCallbackHelper.h>

#include <flocke/filesharing/FileSharing.h>
#include <flocke/filesharing/FileGetting.h>
#include <flocke/sharedlists/SharedListServer.h>
#include <flocke/sharedlists/SharedListTracker.h>
#include <flocke/hardcodedLogin.h>
/*
 * Mini cmdline file sharer to test the demo application
 */

bool alwaysTrue () { return true; }

void onUpdatedTransfer (sf::AsyncOpId id, sf::TransferInfo::TransferUpdateType type, const sf::FileGetting::TransferInfo & t) {
	if (!t.error){
		float percent = (float) t.transferred / (float) t.size * 100.0f;
		std::cout << "Current Transfer: " << percent << "%" << std::endl;
	}
}

void onUpdatedListing   (const sf::Uri & uri, sf::Error error, const sf::DirectoryListing & listing) {
	if (error) {
		std::cerr << "Could not list directory " << uri << ": " << toString (error) << std::endl;
		return;
	}
	std::cout << "Directory: " << uri << " :" << sf::toJSONEx (listing, sf::INDENT) << std::endl;
}

void onLostTracking     (const sf::HostId & host, sf::Error cause){
	std::cout << "Lost tracking to " << host << " cause: " << toString (cause) << std::endl;
}
void onTrackingUpdate   (const sf::HostId & host, const sf::SharedList & list) {
	std::cout << "Updated sharing list (" << host << "): " << toJSON (list) << std::endl;
}


sf::Error operator+ (sf::Error a, sf::Error b) {
	if (a == b) return a;
	if (a && !b) return a;
	if (!a && b) return b;
	return sf::error::MultipleErrors;
}

struct Model {
	Model () {
		beacon = 0;
		sharingServer = 0;
		sharingClient = 0;
		sharedListServer = 0;
		sharedListTracker = 0;
		fileSharing = 0;
		fileGetting = 0;
	}

	~Model () {
		delete fileGetting;
		delete fileSharing;
		delete sharedListTracker;
		delete sharedListServer;
		// sharing client/server will be deleted automatically.
		delete beacon;
	}

	sf::Error init () {
		beacon = sf::InterplexBeacon::createIMInterplexBeacon();
		{
			// must be synchronous to GUI version
			std::vector<sf::String> features;
			features.push_back ("http://sflx.net/protocols/schnee");
			beacon->presences().setOwnFeature (sf::String("schneeflocke (console) ") + sf::schnee::version(), features);
		}
		sharingServer = sf::DataSharingServer::create ();
		sharingClient = sf::DataSharingClient::create ();
		beacon->components().addComponent(sharingServer);
		beacon->components().addComponent(sharingClient);

		// Deactivating permissions
		sharingServer->checkPermissions() = sf::memFun (this, &Model::onCheckPermissions);

		// App components
		sharedListServer   = new sf::SharedListServer (sharingServer);
		sharedListTracker = new sf::SharedListTracker (sharingClient);

		sharedListTracker->lostTracking()   = &onLostTracking;
		sharedListTracker->trackingUpdate() = &onTrackingUpdate;

		fileSharing = new sf::FileSharing (sharingServer, sharedListServer);
		fileGetting = new sf::FileGetting (sharingClient);
		fileGetting->updatedTransfer() = &onUpdatedTransfer;
		fileGetting->gotListing()      = &onUpdatedListing;
		sf::Error err = sharedListServer->init() + sharedListTracker->init() + fileSharing->init() + fileGetting->init();
		if (err) {
			std::cerr << "Could not init file sharing/getting: " << toString (err) << std::endl;
		}
		return err;
	}
	sf::Error connect (const sf::String & loginData) {
		sf::ResultCallbackHelper helper;
		sf::Error err = beacon->connect(loginData, "", helper.onResultFunc());
		if (err || (err = helper.wait())){
			std::cerr << "Could not connect: " << toString (err) << std::endl;
		}
		return err;
	}

	bool onCheckPermissions (const sf::HostId & host, const sf::Path & path) {
		if (!beacon) return false; // ?
		if (path == sharedListServer->path()) return true;
		sf::String shareName = path.head();
		sf::UserId user = beacon->presences().hostInfo (host).userId;
		sf::Error e = fileSharing->checkPermissions (user, shareName);
		return !e;
	}

	// Fundamental components
	sf::InterplexBeacon * beacon;
	sf::DataSharingServer * sharingServer;
	sf::DataSharingClient * sharingClient;

	// App components
	sf::SharedListServer * sharedListServer;
	sf::SharedListTracker * sharedListTracker;

	sf::FileSharing * fileSharing;
	sf::FileGetting * fileGetting;
};

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	if (argc<2){
		std::cerr << "Usage: sf_cmdline_sharer LOGINDATA" << std::endl;
		return 1;
	}
	std::string loginData = argv[1];
	loginData = sf::hardcodedLogin(loginData);

	Model model;
	sf::Error err = model.init();
	if (err) {
		std::cerr << "Could not init" << std::endl;
		return 1;
	}
	err = model.connect(loginData);
	if (err) {
		std::cerr << "Could not connect " << toString (err) << std::endl;
		return 1;
	}

	// command round
	sf::ArgumentList args;
	while (sf::nextCommand(&args)){
		if (args.size() == 0) continue; // hit enter
		const sf::String & cmd (args[0]);
		bool handled = false;
		if (cmd == "help") {
			const char * text =
					"help                                        - show this help\n"
					"share   [shareName] [fileName] [whom = all] - share a file / directory\n"
					"unshare [shareName]                         - unshare a file / directory\n"
					"shared                                      - list own shared files\n"
					"cons                                        - list open connections\n"
					"users                                       - list all users/hosts\n"
					"track [host]                                - track a host\n"
					"ls [uri]                                    - list a directory\n"
					"request [fulluri]                           - transfer some file\n"
					"requestDirectory [fullUri]                  - transfer some directory\n"
					"state                                       - query all info fields\n"
					"liftall                                     - lift all connections\n"
					"quit                                        - quit\n";
			std::cout << text << std::endl;
			handled = true;
		}
		if (cmd == "share" && args.size() >= 3) {
			sf::String shareName = args[1];
			sf::String fileName  = args[2];
			sf::String whoms = "all";
			if (args.size() >= 4){
				whoms = args[3];
			}
			bool forAll = (whoms == "all");
			sf::UserSet whom;
			if (!forAll) whom.insert (whoms);
			err = model.fileSharing->share (shareName, fileName, forAll, whom);
			if (err){
				std::cerr << "Error sharing " << shareName << " with file " << fileName << ": " << toString (err) << std::endl;
			} else {
				sf::FileSharing::FileShareInfo info;
				model.fileSharing->putInfo(shareName, &info);
				std::cout << "Shared " << fileName << " Path: " << info.path << " Type: " << toString (info.type) << " All: " << info.forAll << " Whom: " << sf::toString(info.whom) << std::endl;
			}
			handled = true;
		}
		if (cmd == "unshare" && args.size() >= 2) {
			sf::String shareName = args[1];
			err = model.fileSharing->unshare (shareName);
			if (err){
				std::cerr << "Error unsharing " << shareName << std::endl;
			}
			handled = true;
		}
		if (cmd == "shared" || cmd == "state"){
			sf::FileSharing::FileShareInfoVec vec = model.fileSharing->shared ();
			std::cout << "Shared:" << std::endl << sf::toJSONEx (vec, sf::INDENT) << std::endl;
			handled = true;
		}
		if (cmd == "cons" || cmd == "state") {
			sf::ConnectionManagement::ConnectionInfos cons = model.beacon->connections().connections();
			// sf::ConnectionManagement::ChannelInfoMap cons = model.beacon->connections().conDetails();
			std::cout << "Connections:" << std::endl << sf::toJSONEx (cons, sf::INDENT | sf::COMPRESS) << std::endl;
			handled = true;
		}
		if (cmd == "users" || cmd == "state") {
			sf::PresenceManagement::UserInfoMap users = model.beacon->presences().users();
			std::cout << "users: " << std::endl << sf::toJSONEx (users, sf::INDENT | sf::COMPRESS) << std::endl;
			handled = true;
		}
		if (cmd == "track" && args.size() >= 2){
			sf::Error e = model.sharedListTracker->trackShared (args[1]);
			if (e) {
				std::cerr << "Errror tracking " << args[1] << ": " << toString (e) << std::endl;
			}
			handled = true;
		}
		if (cmd == "ls" && args.size() >= 2){
			sf::Uri uri (args[1]);
			sf::Error e = model.fileGetting->listDirectory(uri);
			if (e){
				std::cerr << "Could not list directory: " << toString (e) << std::endl;
			}
			handled = true;
		}
		if (cmd == "request" && args.size() >= 2) {
			sf::Uri uri (args[1]);
			sf::Error e = model.fileGetting->request (uri);
			if (e){
				std::cerr << "Error requesting " << uri << ":" << toString (e) << std::endl;
			}
			handled = true;
		}
		if (cmd == "requestDirectory" && args.size() >= 2){
			sf::Uri uri (args[1]);
			sf::Error e = model.fileGetting->requestDirectory(uri);
			if (e) {
				std::cerr << "Error requesting " << uri << ":" << toString (e) << std::endl;
			}
			handled = true;
		}
		if (cmd == "state") {
			std::cout << "OwnId:   " << model.beacon->presences().hostId() << std::endl;
			typedef sf::SharedListTracker::SharedListMap SharedListMap;
			SharedListMap shared = model.sharedListTracker->sharedLists();
			std::cout << "Shared " << std::endl;
			for (SharedListMap::const_iterator i = shared.begin(); i != shared.end(); i++) {
				std::cout << "\t" << i->first << "\t" << sf::toJSON(i->second) << std::endl;
			}
			handled = true;
		}
		if (cmd == "liftall"){
			sf::PresenceManagement::HostInfoMap hosts = model.beacon->presences().hosts();
			for (sf::PresenceManagement::HostInfoMap::const_iterator i = hosts.begin(); i != hosts.end(); i++){
				model.beacon->connections().liftConnection (i->first);
			}
			handled = true;
		}
		if (cmd == "quit") {
			break;
		}
		if (!handled) {
			std::cerr << "Unknown command " << cmd << "/" << args.size() - 1 << std::endl;
		}
	}
	return 0;
}
