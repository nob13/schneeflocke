#include "MicroTime.h"
#ifndef WIN32
#include <sys/time.h>
#include <unistd.h>
#else
#include <schnee/winsupport.h>
#include <Windows.h>
#endif

namespace sf {

double microtime (){
#ifdef WIN32
	FILETIME time;
	GetSystemTimeAsFileTime (&time);
	int64_t full = 0;
	full |= time.dwHighDateTime;
	full <<= 32;
	full |= time.dwLowDateTime;
	// is in 100nano-seconds intervals...
	static int64_t first = full;
	int64_t use = (full - first);
	return (use / 10000000.0);
#else
	struct timeval t;
	gettimeofday (&t, 0);
	return t.tv_sec + t.tv_usec / 1000000.0;
#endif
}

}
