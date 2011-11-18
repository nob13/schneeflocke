#include "types.h"

std::string gDefaultServer = "sflx.net";

void splitUserServer (const sf::UserId & userId, sf::String * user, sf::String * server) {
	size_t nPart = userId.find ('@');
	if (nPart == userId.npos) {
		if (user) *user = userId;
		if (server) server->clear();
	} else {
		if (user)   *user   = userId.substr(0,nPart);
		if (server) *server = userId.substr(nPart + 1, userId.npos);
	}
}

QString formatUserName (const sf::UserId & userId, const sf::String & name) {
	if (!name.empty()) return qtString (name);
	sf::String user, server;
	splitUserServer (userId, &user, &server);
	if (server == gDefaultServer) {
		return qtString (user);
	} else {
		return qtString (userId);
	}
}

void setDefaultServer (const sf::String & defaultServer) {
	gDefaultServer = defaultServer;
}

