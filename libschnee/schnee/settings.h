#include <schnee/sftypes.h>

/**
 * @file
 *
 * libschnee wide settings. This setting are very simple and can be read
 * from each component. They are set via command line parameters or auto
 * detection.
 */
namespace sf {
namespace schnee {

///@name libschnee wide settings
///@{

struct Settings {
	Settings ();
	bool   enableLog;		///< Logging enabled
	bool   enableFileLog;   ///< Logging to file enabled
	String logFile;			///< File where to log (if logging to file enabled)

	bool   noLineNoise;		///< No lineNoise to use (--noLineNoise)
	String echoServer;		///< Echo Server to use (--echoServer [name] [port])
	int    echoServerPort;	///< Echo Server port
	bool   disableTcp; 		///< Disable TCP connections (--disableTcp)
	bool   disableUdt;		///< Disalbe UDT connections (--disableUdt)

	bool   forceBoshXmpp;	///< Force BOSH connection when connecting via XMPP (--forceBoshXmpp)
};

/// Gives (const!) you access to global settings
const Settings & settings ();

/// Explicitly set echo server
void setEchoServer (const String & server);

/// Explicitly set echo server port
void setEchoServerPort (int port);

/// Explicitly set forcing bosh mode.
void setForceBoshXmpp (bool v);

///@}

}
}
