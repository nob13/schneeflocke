#pragma once

// Error codes in this GUI.
namespace err {
enum Code {
	NoError,
	CouldNotInit,
	CouldNotConnectService,
	CouldNotConnectRemoteHost,
	CouldNotInitFileSharing,
	CouldNotInitFileGetting,
	CouldNotAddShare,
	CouldNotRemoveShare,
	CouldNotEditShare,
	CouldNotTrackShared,
	CouldNotListDirectory,
	CouldNotRequestFile,
	CouldNotRequestDirectory,
	CouldNotAddUser,
	CouldNotSubscribeReplyUser,
	CouldNotRemoveUser,
	CouldNotRestoreShare,
	ServerSentError,
	RegistrationError
};
}
