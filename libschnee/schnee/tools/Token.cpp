#include "Token.h"

namespace sf {

// found on StackOverflow
// http://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c
static void genRandomAlphaNumeric(char *s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
}

String genRandomToken80 () {
	char result [17];
	genRandomAlphaNumeric (result, 16);
	result[16] = 0;
	return result;
}

String genRandomToken128 () {
	char result [23];
	genRandomAlphaNumeric (result, 22);
	result[22] = 0;
	return result;
}

}
