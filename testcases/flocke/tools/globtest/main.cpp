#include <schnee/schnee.h>
#include <schnee/test/test.h>
#include <flocke/filesharing/file_io/Globber.h>

#include <schnee/tools/async/DelegateBase.h>
#include <schnee/tools/FileTools.h>
#include <schnee/tools/Serialization.h>
#include <stdio.h>

using namespace sf;

/// Helper struct to place synchronized globbing calls.
struct SyncGlobProcess : public DelegateBase{
	SyncGlobProcess (Globber & g, const String & dir, int timeOutMs = -1) : mGlobber (g) {
		SF_REGISTER_ME;
		mReady = false;
		Error e = g.glob(dir, dMemFun (this, &SyncGlobProcess::onGlobResult), timeOutMs);
		if (e){
			mReady = true;
			mResult = e;
		}
	}
	~SyncGlobProcess () {
		SF_UNREGISTER_ME;
	}
	
	void wait () {
		while (!mReady){
			mCondition.wait (schnee::mutex());
		}
	}
	
	void onGlobResult (Error result, const RecursiveDirectoryListingPtr & listing) {
		mReady   = true;
		mListing = listing;
		mResult  = result;
		mCondition.notify_all ();
	}
	
	Error result () const {
		return mResult;
	}
	
	const RecursiveDirectoryListingPtr& listing () const {
		return mListing;
	}
	
private:
	Error mResult;
	bool mReady;
	Globber & mGlobber;
	Mutex mMutex;
	Condition mCondition;
	RecursiveDirectoryListingPtr mListing;
};

sf::Error globSync (Globber & g, const String & dir, RecursiveDirectoryListingPtr * result, int timeOutMs = -1){
	SyncGlobProcess proc (g, dir, timeOutMs);
	proc.wait();
	*result = proc.listing();
	return proc.result();
}

void mayNotCall (Error result, const RecursiveDirectoryListingPtr & listing) {
	printf ("mayNotCall called with result %s, listing. %s\n", toString (result), 
			listing ?  toJSONEx(*listing, INDENT).c_str() : "(no listing)");
	tassert (false, "Shall not come here");
}

/// This tests the user specific const_iterator and the sizeSum
void checkIteratorAndSize (RecursiveDirectoryListingPtr listing) {
	int64_t ownSum = 0;
	RecursiveDirectoryListing::const_iterator end = listing->end();
	for (RecursiveDirectoryListing::const_iterator i = listing->begin(); i != end; ++i){
		if (i->type == DirectoryListing::File)
			printf ("  Single file:      %s --> %s\n", i.path().c_str(), toJSON (*i).c_str());
		else
			printf ("  Single Directory: %s --> %s\n", i.path().c_str(), i->name.c_str());
		if (i->type == DirectoryListing::File)
			ownSum+=i->size;
	}
	int64_t calcSum = listing->sizeSum();
	printf (" sizeSum: %ld ownSum: %ld\n", calcSum, ownSum);
	tassert (ownSum == calcSum, "Shall be equal as both is the sum of all file sizes");
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	if (!sf::fileExists("testbed")){
		fprintf (stderr, "testbed folder doesn't exist, test is failing\n");
		return 1;
	}
	{
		printf ("Small Folder\n");
		sf::Globber globber;
		RecursiveDirectoryListingPtr listing;
		sf::Error e = globSync (globber, "testbed/dir1", &listing);
		tassert (!e, "Shall not fail");
		printf ("Listing: %s\n", toJSONEx (*listing, sf::INDENT).c_str());
		checkIteratorAndSize (listing);
	}
	{
		printf ("Symlink Loop\n");
		sf::Globber globber;
		RecursiveDirectoryListingPtr listing;
		sf::Error e = globSync (globber, "testbed/globber", &listing);
		tassert (!e, "Shall not fail");
		printf ("Listing: %s\n", toJSONEx (*listing, sf::INDENT).c_str());
		checkIteratorAndSize (listing);
	}
	{
		printf ("Proper Timeout\n");
		sf::Globber globber;
		RecursiveDirectoryListingPtr listing;
		sf::Error e = globSync (globber, "/usr", &listing, 5);
		tassert (e == error::TimeOut, "Shall time out");
	}
	{
		printf ("Async ending behaviour\n");
		// asynchronous ending behaviour
		sf::Globber globber;
		globber.glob ("/usr", &mayNotCall, 1000000);
		globber.glob ("/usr", &mayNotCall, 1000000);
		test::millisleep_locked (1);
	}
	{
		printf ("Really big folder\n");
		sf::Globber globber;
		RecursiveDirectoryListingPtr listing;
		sf::Error e = globSync (globber, "/usr/local", &listing, 100000); // 100sec
		tassert (!e, "Shall not fail");
		checkIteratorAndSize(listing);
	}	
	
	return 0;
}
