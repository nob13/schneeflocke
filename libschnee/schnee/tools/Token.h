#include <schnee/sftypes.h>

namespace sf {

/// Generates a random token (16 characters, at least 5 bit, min 80 bit)
String genRandomToken80 ();

/// Generates a random token (22 characters, ~ 128bit).
String genRandomToken128 ();
}
