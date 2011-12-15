#pragma once

#include <schnee/sftypes.h>
#include "RecursiveDirectoryListing.h"

#include <schnee/tools/async/AsyncOpBase.h>

#include "../tools/WorkThread.h"

namespace sf {

/**
 * A globber "globs" throug a directory and so collects all contained directores / files
 * recursivly in a RecursiveDirectoryListing structure. As this can take a time its meant
 * to be executed in its own thread.
 * 
 * Search is using breadth first search
 * 
 * Globber could be faster when not always using strings (but boost::file_system iterators etc.)
 */
class Globber : public AsyncOpBase {
public:
	Globber();
	~Globber();
	
	typedef function<void (Error result, const RecursiveDirectoryListingPtr & listing)> GlobCallback;
	
	/**
	 * Starts a globbing process for a given directory, returning all data and success state in the callback
	 * @param timeOutMs - the given timeOut. if < 0 it will use the defaultTimeOutMs (NOT infinity)
	 * @return if an error is returned, the glob function won't callback you!
	 */
	Error glob (const String & directory, const GlobCallback & callback, int timeOutMs = -1);
	
	/// Returns default time out for glob operations
	int defaultTimeOutMs () const { return mDefaultTimeOutMs; }
	
	/// Sets default timeout for glob operations
	void setDefaultTimeOutMs (int v)  { mDefaultTimeOutMs = v; } 
private:
	struct GlobOp;
	typedef RecursiveDirectoryListing::RecursiveEntry RecursiveEntry;
	typedef RecursiveDirectoryListing::RecursiveEntryVec RecursiveEntryVec;
	typedef RecursiveEntryVec::iterator EntryIterator;
	typedef std::pair<EntryIterator, String> WorkPoint;		///< The current node to expand and its path
	typedef std::deque<WorkPoint> WorkQueue;
	
	/// Continues globbing (for around 1ms)
	void continueGlobbing (AsyncOpId id);
	
	/// Expand one working point
	/// Adds new work to the workqueue
	/// if an error is returned the operation will be already deleted
	Error expand (GlobOp * op, const String& directory, RecursiveEntryVec & target);
	
	/// Add more globbing work (pushed continueGlobbing to the workThread)
	void addGlobbingWork_locked (AsyncOpId id);
	void onReady (AsyncOpId id);
	
	enum GlobOperations { GLOB = 1 };
	
	/// A globbing operation
	struct GlobOp : public AsyncOp {
		GlobOp (const sf::Time & time) : AsyncOp (GLOB, time) {
			begin = true;
			listing = RecursiveDirectoryListingPtr (new RecursiveDirectoryListing);
		}
		
		// imp of AsyncOp
		virtual void onCancel (sf::Error reason) {
			if (callback) callback (reason, RecursiveDirectoryListingPtr());
		}
		
		bool begin;
		RecursiveDirectoryListingPtr listing;	//< Result listing
		WorkQueue workQueue;

		GlobCallback   callback;
		String         directory;
	};
	WorkThread mWorkThread;
	int        mDefaultTimeOutMs;
};

typedef shared_ptr<Globber> GlobberPtr;

}
