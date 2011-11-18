#include <schnee/test/test.h>
#include <schnee/tools/Path.h>
using namespace sf;

/**
 * @file
 * Test some methods of the Path structure
 */

int main (int argc, char * argv[]){
	// Directory like uris will be used in feature; test their functionality
	Path a ("hi/world");
	Path b ("hi");
	Path c ("hi/World");
	Path d ("hi/World/WhatAre/YouDoing/");
	Path e ("hi/World/WhatAre/YouDoing//");
	Path f ("//short");
	tassert (a.head() == "hi", "Uri test");
	tassert (a.subPath() == "world", "Uri test");
	tassert (b.head() == "hi", "Uri Test");
	tassert (b.subPath() == "", "Uri Test");
	tassert (c.head() == "hi", "Uri test");
	tassert (c.subPath() == "World", "Uri test");
	tassert (d.head() == "hi", "Uri Test");
	tassert (d.subPath() == "World/WhatAre/YouDoing/", "Uri Test");
	tassert (e.head() == "hi", "Uri test");
	tassert (e.subPath() == "World/WhatAre/YouDoing//", "Uri test");
	tassert (f.head() == "", "Uri Test");
	tassert (f.subPath() == "/short", "Uri Test");
	return 0;
}
