#include "timing.h"

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
void sleep (unsigned int seconds) { ::sleep(seconds); }
#else
void sleep (unsigned int seconds) { Sleep (seconds * 1000); }
#endif

void millisleep (unsigned int ms) {
#ifdef WIN32
	::Sleep (ms);
#else
	::usleep (ms * 1000);
#endif
}


}
}
