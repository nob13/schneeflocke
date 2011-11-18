#pragma once

#include <schnee/net/Channel.h>
#include <schnee/tools/async/DelegateBase.h>
namespace sf {
namespace test {

/// A helper class collecting info about different LocalChannels
class LocalChannelUsageCollector {
public:
	LocalChannelUsageCollector ();

	/// Add transferred data from to
	void addTransferred (const HostId & from, const HostId & to, long fullData, int hops);

	/// Adds data which is not yet consumed (for measuring not yet consumed data)
	void addPendingData (long data);

	/// Consume some data (for measuring not yet consumed data)
	void consumePendingData (long data);

	/// Clears all collected data
	void clear ();

	/// Prints a statistic
	void print (std::ostream & stream) const;

	/// Returns sum of all tranferred traffic (logical)
	long sum () const;

	/// Returns sum of all transferred traffic, multiplicated with hop count
	long physicalSum () const;

	/// Returns sum of data not yet consumed
	long pendingData () const;

private:
	typedef std::pair<HostId, HostId> Connection;
	typedef std::map<Connection, long> AmountMap;

	mutable Mutex mMutex;
	AmountMap mTransferred;
	long mSum;

	long mPhysicalSum;

	long mPendingData;
};
typedef shared_ptr<LocalChannelUsageCollector> LocalChannelUsageCollectorPtr;

/**
 * A local channel is a channel which delivers data within the same process.
 *
 * It is used for testing purposes.
 *
 * Bandwidth/Delay will just be inserted for statistics, they are not simulated itself.
 * however they are used indirectly by DataSharing for the creation of the Application Layer
 * Multicast network.
 */
class LocalChannel : public Channel, public DelegateBase {
public:

	LocalChannel (const HostId & id = "", LocalChannelUsageCollectorPtr collector = LocalChannelUsageCollectorPtr());
	virtual ~LocalChannel ();

	/// Binds two LocalChannels together, making them open
	static void bindChannels (LocalChannel & a, LocalChannel & b);

	///@name State
	///@{

	// Returns host id or "" if not set
	const HostId & hostId () const { return mHostId; }

	/// Sets a bandwidth for the channel (by Default -1)
	void setBandwidth (float bandwidth);

	/// Sets a delay for the channel (by Default -1)
	void setDelay (float delay);

	/// Marks the channel as being to a neighbor
	void setIsToNeighbor (bool v);

	/// Set the number of (network!) hops of this local channel, = 1 (default) means direct connection
	void setHops (int hops);

	///@}



	// Implementation of Channel
	virtual sf::Error error() const;
	virtual State state () const;

	virtual Error write (const ByteArrayPtr& data, const ResultCallback & callback = ResultCallback());

	virtual sf::ByteArrayPtr read (long maxSize);
	virtual void close (const ResultCallback & resultCallback = ResultCallback ());

	virtual const char * stackInfo () const { return "local";}

	virtual ChannelInfo info () const {
		ChannelInfo info;
		info.bandwidth  = mBandwidth;
		info.delay      = mDelay;
		info.toNeighbor = mIsToNeighbor;
		info.virtual_   = true;
		info.laddress   = toString (this->delegateKey());
		info.raddress   = mOther ? toString (mOther->delegateKey()) : String ("none");
		return info;
	}

	virtual sf::VoidDelegate & changed () { return mChanged; }

private:
	/// Function where to send datat
	typedef sf::function<void (ByteArrayPtr data)> TargetDelegate;

	/// Sets the other connected localchannel
	void setOther (LocalChannel * channel);


	/// Target function for the connected LocalChannel
	void incoming (ByteArrayPtr data);


	/// Own input buffer
	sf::ByteArray mInputBuffer;

	// Delegates
	sf::VoidDelegate mChanged;
	LocalChannel     * mOther;
	HostId             mHostId;
	LocalChannelUsageCollectorPtr mCollector;

	// Locking
	mutable sf::Mutex mMutex;
	mutable sf::Mutex mOtherMutex;	///< Mutex for writing into mOther
	
	// Information
	float mDelay;
	float mBandwidth;
	bool mIsToNeighbor;
	int   mHops;				///< Number of hops, =1 means direct connection
};

typedef shared_ptr<test::LocalChannel> LocalChannelPtr;

}
}

inline std::ostream & operator<< (std::ostream & stream, const sf::test::LocalChannelUsageCollector & collector) {
	collector.print(stream);
	return stream;
}
