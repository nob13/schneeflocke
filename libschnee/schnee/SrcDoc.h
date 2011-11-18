#pragma once

// This file has some info for Doxygen

/**
 * @mainpage Index
 * 
 * @brief This is the doxygen generated HTML documentation of libschnee, a P2P communication library.
 * @endbrief
 *
 * Starting points:
 * - It is important to initialize/deinitialize the library. You can use sf::schnee::init and sf::schnee::deinit 
 *     or allocate one sf::schnee::SchneeApp object.
 * - Create communication with other peers by using an InterplexBeacon object, you can create it with sf::InterplexBeacon::create.
 * - Manage sharing of data and streams by creating an sf::DataSharing object, you can create it with sf::DataSharing::create. sf::DataSharing is a CommunicationComponent.
 *   If you want to use it you will have to add it to the Beacon with sf::InterplexBeacon::addComponent.
 * - If you just want to send simple messages, use the sf::Messaging CommunicationComponent.
 * - If you just want to try out the IM connection, use sf::IMClient and forget about InterplexBeacon.
 * - If you wan to receive thread safe delegate notifications, make sure that your destination object derives from sf::DelegateBase and 
 *   that it calls #SF_REGISTER_ME during construction and #SF_UNREGISTER_ME during deconstruction. Then it is possible to create callback objects
 *   with sf::dMemFun.
 *
 * Debugging:
 * - All libschnee applications do provide a log. You can enable it with passing @verbatim --enableLog @endverbatim or @verbatim --enableFileLog @endverbatim
 * - If you want to use the log in your own code, include "Log.h" and use it like:
     @verbatim sf::Log (LogWarning) << LOGID << "My log goes here" << std::endl; @endverbatim
     Other Logging Levels like LogInfo or LogError are also possible (sf::LogLevel).
 *
 */

/// This namespace holds all classes inside libschnee. The 'sf' stands for schneeflocke, german for snow flake.
namespace sf {

}
