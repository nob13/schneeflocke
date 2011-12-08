#pragma once
#include "CommunicationComponent.h"
#include "DataSharingElements.h"
#include "DataPromise.h"

namespace sf {

/**
 * All Server functionalities of the DataSharing component.
 *
 * You can share, update, and unshare data blocks. You can
 * also receive pushed shares.
 *
 * Note: In order to receive data/share info or pushing data
 * you will have to use the DataSharingClient.
 *
 * Note: Share-Uris are not allowed to have full path names; these are reserved
 * for sub data (see DataSharing::DataSharingPromise::subData)
 *
 */
struct DataSharingServer : public CommunicationComponent {

	/// The base class where users can drop in data into the data sharing
	struct SharingPromise {
		virtual ~SharingPromise() {}
		
		/// Asks for the shared data on a given sub path
		virtual DataPromisePtr data (const Path & subPath, const String & user) const = 0;
		
		///@name Meta information
		///@{
		
		/// A general data description
		virtual ds::DataDescription dataDescription () const { return ds::DataDescription(); }
		
		/// A general size (-1 if not defined, ask the data for the size)
		virtual int64_t size() const { return -1; }
		
		/// An error state (if existing)
		virtual Error error () const { return NoError; }

		///@}
	};
	typedef shared_ptr<SharingPromise> SharingPromisePtr;
	
	/// A sharing promise which provides fixed data (and does not contain any sub paths)
	class FixedSharingPromise : public SharingPromise {
	public:
		FixedSharingPromise (const DataPromisePtr& data) : mData (data) {}
		
		/// Always returns same data
		DataPromisePtr data (const Path & subPath, const String & user) const { return mData; }
		
		virtual ds::DataDescription desc () const { return mData->dataDescription(); }
		virtual int64_t size() const { return mData->size(); }
		virtual Error error () const { return mData->error(); }
		
	private:
		
		DataPromisePtr mData;
	};
	
	static SharingPromisePtr createFixedPromise (const DataPromisePtr & data) {
		return SharingPromisePtr (new FixedSharingPromise (data));
	}
	
	static SharingPromisePtr createPromise (const ByteArrayPtr & data) {
		return SharingPromisePtr (new FixedSharingPromise (sf::createDataPromise(data)));
	}

	/// Creates a DataSharing Implementation
	static DataSharingServer * create ();

	/// Stops all datasharing, cancels subscriptions, clears shared files etc.
	virtual Error shutdown () = 0;


	///@name Sharing Data
	///@{

	/// Shares a bit of data, ByteArrayPtr variant
	virtual Error share (const Path & path, const ByteArrayPtr & data) {
		return share (path, createPromise (data));
	}
	/// Shares a bit of data
	/// Data may be 0  (if your stream did not yet started or we do not have a version yet)
	virtual Error share (const Path & path, const SharingPromisePtr & data = SharingPromisePtr ()) = 0;

	/// Updates a bit of data, subscribed used will get a notification.
	virtual Error update  (const Path & path, const SharingPromisePtr & data) = 0;

	/// Updates a bit of data, ByteArrayPtr variant
	virtual Error update (const Path & path, const ByteArrayPtr & data) {
		return update (path, createPromise (data));
	}

	/// Unshares a bit of data. If you just want to cancel a subscription use cancelSubscription.
	virtual Error unShare (const Path & path) = 0;

	/// Cancel an ongoing transfer
	virtual Error cancelTransfer (AsyncOpId id) = 0;

	///@}

	///@name State & Diagnostics
	///@{

	/// A Simplified information about shared data
	struct SharedDataDesc {
		int     currentRevision;			///< The current revision
		HostSet subscribers;				///< Set of subscribers
		void serialize (Serialization & s) const;
	};
	/// A simple map containing information about shared data
	typedef std::map<Path,SharedDataDesc> SharedDataDescMap;

	/// Dump information about shared data (slow)
	virtual SharedDataDescMap shared () const = 0;

	/// Dump information about a specific shared file
	virtual SharedDataDesc  shared (const Path & path, bool * found = 0) const = 0;

	///@}

	///@name Delegates
	///@{

	typedef sf::function<bool (const HostId & user, const Path & path)> PermissionDelegate;

	/// A delegate which will ask you for permissions whether a user may receive specific data
	virtual PermissionDelegate & checkPermissions () = 0;

	///@}

};

void serialize (sf::Serialization & s, const sf::DataSharingServer::SharedDataDescMap & map);

}
