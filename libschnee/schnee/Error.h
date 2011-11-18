#pragma once
#include <string>
#include<sfserialization/autoreflect.h>

/**
 * @file
 * Everywhere used error code.
 */

namespace sf {

	// This trick shall put all error codes into namespace error
	namespace error {

	/// A Generic Error code for various subroutines
	enum Error {
		NoError = 0,		///< No Error hapened
		NoPerm,				///< No Permission
		NotFound,			///< Not found
		RevisionNotFound,	///< The particular revision is not found (maybe updated now)

		InvalidArgument,	///< Some argument is invalid
		ChannelError,		///< Some error in data channel
		Closed,				///< Channel closed, input closed, something closed
		Eof,				///< End-Of-File

		ConnectionError,	///< General error during connection
		ConnectionReset,	///< (Peer) reset connection
		LiftError,			///< Could not lift a connection
		HostOffline,		///< Own host is offline
		TargetOffline,		///< Target host is offline

		NotEnough,			///< Not enough (e.g. data)
		TooMuch,			///< Too much (e.g. data)
		
		ExistsAlready,		///< Something exists already
		
		ServerError,		///< Could not start server (or similar)
		
		TimeOut,			///< There was an timeout
		
		NotSupported,		///< Some feature is not supported
		
		NotInitialized,		///< Component was not corretly initialized
		TryAgain,			///< Connection is on doing, try it again - maybe with a smaller data amount

		InvalidUri,			///< An invalid uri value was given (specialization of InvalidArgument)
		BadProtocol,		///< Opponent spoke a strange protocol
		OldProtocol,		///< Your protocol is too old (preparation)
		NewProtocol,		///< Your protocol is too new (preparation)

		CouldNotConnectHost,	///< Could not connect to the host
		AuthError,				///< Could not authenticate
		SignInError,			///< Sign in error (did not came through Sign-In-Process)
		TlsError,				///< TLS-Error (could not secure connection)

		NoZeroConf,				///< No ZeroConf-Service available
		ZeroConfError,			///< Error with ZeroConf

		BufferFull,				///< (output) Buffer is full
		MultipleErrors,			///< There where multiple errors

		Loop,				///< Loop detected / Would lead into a loop
		BadDeserialization,	///< Error during deserialization

		ReadError,			///< Error while reading something
		WriteError,			///< Error writing something
		WrongState,			///< Some state went wrong
		Canceled,			///< Some (async) op was canceled
		Other				///< Other (unknown) error
	};
	
	SF_AUTOREFLECT_ENUM (Error);

	}

	
	/// The generic error code used everywhere in libschnee.
	typedef error::Error Error;
	using error::NoError;

	/*
	/// Translates error code into its string representation
	const char* toString (Error err) {
		return error::toString(err);
	}
	/// Translates string representation into the error code
	/// @return true on success
	bool fromString (const std::string & s, Error & err) {
		return error::fromString (s, err);
	}
	*/
	
}
