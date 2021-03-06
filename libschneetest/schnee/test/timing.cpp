#include "timing.h"
#include <schnee/schnee.h>

#ifndef WIN32
#include <sys/time.h>
#include <unistd.h>
#else
#include <schnee/winsupport.h>
#include <Windows.h>
#endif

namespace sf {
namespace test {


#ifndef WIN32
void _sleep (unsigned int seconds) { ::sleep(seconds); }
#else
void _sleep (unsigned int seconds) { Sleep (seconds * 1000); }
#endif

void sleep_locked (unsigned int seconds) {
	schnee::mutex().unlock();
	_sleep (seconds);
	schnee::mutex().lock();
}


void _millisleep (unsigned int ms) {
#ifdef WIN32
	::Sleep (ms);
#else
	::usleep (ms * 1000);
#endif
}

void millisleep_locked (unsigned int ms) {
	schnee::mutex().unlock();
	_millisleep(ms);
	schnee::mutex().lock();
}



}
}
