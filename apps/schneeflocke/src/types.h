#pragma once

#include <schnee/sftypes.h>
#include <QString>

// General type conversion things

/// Converts QString to sf::String
inline sf::String sfString (const QString & qs) { return std::string (qs.toUtf8().data()); }
/// Converts sf::String to QString
inline QString qtString (const sf::String & qs) { return QString::fromUtf8 (qs.c_str());  }

/// splits user part and server part from userId.
/// TODO: Perhaps userid should be its own class.
void splitUserServer (const sf::UserId & userId, sf::String * user, sf::String * server);

/// Formats the displayed name for a given UserID / Presence Name
/// If user is on default server the servername will be omittted.
/// TODO Need a better place
QString formatUserName (const sf::UserId & userId, const sf::String & name);

/// Sets default server for formatUserName
void setDefaultServer (const sf::String & defaultServer);
