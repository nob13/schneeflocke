#include <stdarg.h>
#include <stdio.h>

#include <schnee/schnee.h>
#include <schnee/im/IMClient.h> // there are some long serializeable types

#include <iostream>
#include <schnee/tools/Log.h>
#include <schnee/tools/Serialization.h>


/*
 * Tests the Log System and explores some syntax changes for a new one. The new one
 * never got ready.
 */

/*
 * Trying a C-like log
 */

void newLog (const char * filename, int linenumber, const char * text, ...){
	static int logNum = -1;
	logNum++;

	const char * bn = sf::basename (filename);
	char buffer [512];
	int c = snprintf (buffer, 512, "%d %s(%d)", logNum, bn, linenumber);
	va_list args;
	va_start (args, text);
	vsnprintf (buffer + c, 512 - c, text, args);
	buffer[511] = 0;
	va_end (args);
	printf ("%s", buffer);
}

#define LOGID2 __FILE__,__LINE__

void testFunc1 (const sf::IMClient::Message & m){
	newLog (LOGID2, "(LogTest)This is a very cool Log alltogether with everything for us, %s\n", sf::toJSON(m).c_str());
}

void testFunc2 (const sf::IMClient::Message & m){
	std::cout << LOGID << "(LogTest)This is a very cool Log alltogether with everything for us, " << sf::toJSON(m) << std::endl;
}

void testFunc3 (const sf::IMClient::Message & m){
	newLog (LOGID2, "(LogTest)Build with newLog %s, %d\n", sf::toJSON(m).c_str(), 5);
}

void testFunc4 (const sf::IMClient::Message & m){
	std::cout << LOGID << "(LogTest)Build with old log " << sf::toJSON(m) << ", " << 5 << std::endl;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;

	sf::IMClient::Message message;
	message.from ="nob@me";
	message.to   ="houston@away";
	message.subject = "a long subject";
	message.type = sf::IMClient::MT_CHAT;
	message.body = "this is a content of a very long \n message \t with \"tabs\"";


	std::cout << "Log Test" << std::endl;
	// New Log
	testFunc1 (message);
	testFunc3 (message);

	// Old Log
	testFunc2 (message);
	testFunc4 (message);

	return 0;
}
