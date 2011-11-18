#include "Log.h"
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <schnee/schnee.h>

///@cond DEV

namespace sf {

Logger * infoLog = 0;   ///< the log we use for everything but errors
Logger * warningLog = 0;	///< Log for warnings
Logger * profileLog = 0; ///< Log for profile stuff
Logger * errorLog = 0; ///< the log for errors

std::ofstream * fileOutStream = 0;
std::ostringstream emptyOutStream; 

StreamOutLog::StreamOutLog (Logger * logger) {
	mLogger = logger;
}

StreamOutLog::~StreamOutLog () {
	if (mLogger) {
		std::string s = mOutput.str();
		if (!s.empty()){
			mLogger->addLine(s.c_str());
		}
	}
}

StreamOutLog::StreamOutLog (const StreamOutLog & base) {
	mLogger = base.mLogger;
	base.mLogger = 0;
}

StreamOutLog & StreamOutLog::operator<< (const char * c) {
	if (mLogger) {
		mOutput << c;
	}
	return *this;
}

StreamOutLog& StreamOutLog::operator << ( std::ios_base& (*__pf) (std::ios_base&)) {
    if (mLogger){
    	mLogger->addLine (mOutput.str());
    	mOutput.str("");
    }
    return (*this);
}

StreamOutLog& StreamOutLog::operator << ( std::ostream& (*__pf) (std::ostream&)) {
    if (mLogger) {
    	mLogger->addLine (mOutput.str());
    	mOutput.str("");
    }
    return (*this);
}

void Logger::addLine (const char * line) {
	mMutex.lock ();
	mOut << mName << ":" << mSeqNum << " " << line << std::endl;
	mSeqNum++;
	mMutex.unlock ();
}

void Logger::addLine (const std::string& line) {
	mMutex.lock ();
	mOut << mName << ":" << mSeqNum << " " << line << std::endl;
	mSeqNum++;
	mMutex.unlock ();
}


StreamOutLog Logger::instance (int level) {
	if (!infoLog){
		std::cerr << "Call initLogging (called by schnee::init ()) first before you try to do logging!" << std::endl;
		exit (1);
	}
	if (level == LogError) return StreamOutLog (errorLog);
	if (level == LogWarning) return StreamOutLog (warningLog);
	if (level == LogProfile) return StreamOutLog (profileLog);
	return StreamOutLog (infoLog);
}


void initLogging (bool toStdErr, bool toFile, const std::string & filename) {
	if (toFile) {
		fileOutStream = new std::ofstream (filename.c_str());
		if (fileOutStream->is_open()){
			infoLog     = new Logger (*fileOutStream, false, "LogInfo");
			warningLog  = new Logger (*fileOutStream, false, "LogWarning");
			errorLog   = new Logger (*fileOutStream, false, "LogError");
			profileLog = new Logger (*fileOutStream, false, "LogProfile");
			return;
		} else {
			std::cerr << "Warning, could not open log file: " << filename << "; will do logging to std::cerr" << std::endl;
			toStdErr = true;
		}
	}
	if (toStdErr) {
		infoLog    = new Logger (std::cerr, false, "LogInfo");
		warningLog = new Logger (std::cerr, false, "LogWarning");
		profileLog = new Logger (std::cerr, false, "LogProfile");
	} else {
		infoLog    = new Logger (emptyOutStream, true, "LogInfo");
		warningLog = new Logger (emptyOutStream, true, "LogWarning");
		profileLog = new Logger (emptyOutStream, true, "LogProfile");
	}
	// Always log out errors
	errorLog = new Logger (std::cerr, false, "LogError");
	Log (LogProfile) << LOGID << "Log initialized. libschnee version " << schnee::version() << std::endl;
}

void deInitLogging () {
	delete infoLog;
	delete errorLog;
	delete profileLog;
	delete warningLog;
	delete fileOutStream;
	infoLog = 0;
}

}

///@endcond DEV
