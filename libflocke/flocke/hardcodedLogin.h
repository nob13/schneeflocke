#pragma once
#include <schnee/sftypes.h>

namespace sf {

/// Translates a hardcoded login into its real representation
/// Note: used for testcases on localhost only.
inline String hardcodedLogin (const String & s){
	if (s == "x1") return "xmpp://autotest1:boom74_x@localhost/SF";
	if (s == "x2") return "xmpp://autotest2:gotcha13_y@localhost/SF";
	if (s == "x3") return "xmpp://autotest3:evoruk14_z@localhost/SF";
	if (s == "x4") return "xmpp://autotest4:habbel15_a@localhost/SF";
	if (s == "s1") return "slxmpp://Alice";
	if (s == "s2") return "slxmpp://Bob";
	return s;
}

}
