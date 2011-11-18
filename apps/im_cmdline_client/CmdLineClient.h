#ifndef CMDLINECLIENT_H_
#define CMDLINECLIENT_H_

#include <schnee/im/IMClient.h>
#include <schnee/tools/async/DelegateBase.h>

/**
 * Waiting loop asking for commands
 *
 * Events should be directed, as QThread itself doesn't live in its own thread
 */
class CmdLineClient : public sf::DelegateBase {
public:
	/// Initializes a XMPP connection
	CmdLineClient (const sf::String & connectionString);
	~CmdLineClient ();
	
	void setAuthContacts (bool v = true) { mAuthContacts = v; }

	/// if it ends, the program ends
	int start ();

private:

	sf::IMClient * mImClient;
	sf::String mConnectionString;
	bool mAuthContacts; ///< Automatically authenticate contacts

	void connectionStateChanged(sf::IMClient::ConnectionState state);
	void messageReceived (const sf::IMClient::Message & msg);
	void contactRosterChanged ();
	void subscriptionRequest (const sf::UserId & user);
	void streamErrorReceived (const sf::String & text);
	void onReceivedFeatureSet (sf::Error result, const sf::IMClient::FeatureInfo & info, const sf::HostId & source);
};



#endif /* CMDLINECLIENT_H_ */
