#pragma once
#include <schnee/sftypes.h>

namespace sf {

inline String hardcodedLogin (int index) {
	switch (index){
		case 0:
			return "xmpp://autotest1:boom74_x@localhost/SF";
		case 1:
			return "xmpp://autotest2:gotcha13_y@localhost/SF";
		case 2:
			return "xmpp://autotest3:evoruk14_z@localhost/SF";
		case 3:
			return "xmpp://autotest4:habbel15_a@localhost/SF";
		default:
			assert (!"More than supported");
			return "";
	}
}

/// Translates a hardcoded login into its real representation
/// Note: used for testcases on localhost only.
inline String hardcodedLogin (const String & s){
	if (s == "x1") return hardcodedLogin (0);
	if (s == "x2") return hardcodedLogin (1);
	if (s == "x3") return hardcodedLogin (2);
	if (s == "x4") return hardcodedLogin (3);
	return s;
}

}
