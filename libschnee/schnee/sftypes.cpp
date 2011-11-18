#include "sftypes.h"

namespace sf {

const char* toString (OnlineState os) {
	if (os == OS_OFFLINE) return "OS_OFFLINE";
	if (os == OS_ONLINE)  return "OS_ONLINE";
	if (os == OS_UNKNOWN) return "OS_UNKNOWN";
	assert (false); // should not come here
	return "";
}

}
