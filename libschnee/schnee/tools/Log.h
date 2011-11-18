#pragma once

#include <iostream>
#include <schnee/sftypes.h>

namespace sf {

/**
 * @file
 * A Simple log system.
 *
 * Usage:
 * @verbatim
 * Log (LogInfo) << LOGID << "Log your stuff here" << std::endl;
 * @endverbatim
 *
 * Instead of LogInfo you can also use the other logging levels like LogWarning, LogProfile and LogError.
 * 
 * 
 */

/**
 * The different log levels the log supports
 */
enum LogLevel {
	LogError = 1,
	LogWarning = 2,
	LogProfile = 4,
	LogInfo = 8
};

/// @cond DEV

class Logger;

/// A simple streaming compatible log class which gets returned by Logger::instance
/// It is thread safe, as there will be only one instance per class
class StreamOutLog {
public:
	StreamOutLog (Logger * logger);
	// this constructor invalidates the old instance (base.mLogger will be set to 0)
	StreamOutLog (const StreamOutLog & base);
	~StreamOutLog ();
	
	
	/// Prints out something
	template<class T> StreamOutLog & operator<< (const T & element) {
		if (mLogger){
			mOutput << element;
		}
		return *this;
	}
	
	/// specialization
	StreamOutLog & operator<< (const char * c);
	
	// for std::endl support
	StreamOutLog& operator << ( std::ios_base& (*__pf) (std::ios_base&));

	/// for std::endl support
	StreamOutLog& operator << ( std::ostream& (*__pf) (std::ostream&));
	
private:
	mutable Logger * mLogger;
	std::stringstream mOutput;
};


class Logger {
public:
	explicit Logger (std::ostream & stream, bool discard, const char * name) : mOut (stream), mDiscard (discard), mName (name), mSeqNum (0) {}

	/// Adds a line to the logger
	void addLine (const char * line);
	void addLine (const std::string& line);
	
	static StreamOutLog instance (int level);
	
private:
	std::ostream & mOut;
	bool  mDiscard;
	const char * mName;
	int   mSeqNum;
	Mutex mMutex;
};

/// A log which discards everything
/// we hope that the compiler eliminates it completely
class EmptyLog {
public:
	EmptyLog () {}
	template <class T> EmptyLog& operator<< (const T & element) {
		return *this;
	}
	
	/// returns the Logger for the specific level
	static EmptyLog instance (int level) { return EmptyLog (); }
	static EmptyLog instanceLine (int level) { return EmptyLog (); }
	
	/// does nothing
	EmptyLog & seqNum () { return *this; }
	
	// for std::endl support
	EmptyLog& operator << ( std::ios_base& (*__pf) (std::ios_base&)) { return *this; }

	/// for std::endl support
	EmptyLog& operator << ( std::ostream& (*__pf) (std::ostream&)) { return *this; }
	
};

/// @name Logging Init / Deinitalization

/// @{
/**
 * Inits the logging system 
 * @param toStdErr - put logging to std::cerr
 * @param toFile   - put logging to a file with given filename
 * @param filename - the file to use for logging
 * Note, currentlay toStdErr and toFile may not be supported simultaneously. 
 * In this case the log will be done to file 
 */
void initLogging (bool toStdCerr = true, bool toFile = false, const std::string & filename = "");
/// deinitializes the log
void deInitLogging ();

/// @}

#ifdef _DEBUG
typedef Logger Logger_LogInfo;
typedef Logger Logger_LogWarning;
typedef Logger Logger_LogProfile;
typedef Logger Logger_LogError;
#else
typedef EmptyLog Logger_LogInfo;
typedef Logger Logger_LogWarning;
typedef Logger Logger_LogProfile;
typedef Logger Logger_LogError;
#endif

/// returns only filename of __FILE__ filename
static inline const char * basename (const char * x){
	int i = 0;
	int l = 0;
	while (x[i] != '\0'){
		if (x[i] == '/') l = i + 1;
		i++;
	}
	return x + l;
}



///@endcond DEV

/**
Generates a log output.
Use <code> Log (level) << "You're stuff to log" << std::endl; </code>.
Where level can be anything of LogLevel.
*/
#define Log(x) Logger_ ## x ::instance (sf::x)
/// Generates a log position (filename and line number) stamp
#define LOGID sf::basename (__FILE__) << "(" << __LINE__ << "): "
}

