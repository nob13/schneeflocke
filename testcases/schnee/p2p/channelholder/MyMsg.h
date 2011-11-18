#pragma once
#include <sfserialization/autoreflect.h>

/// Test message for the testcase
struct MyMsg {
	MyMsg() : code (1337) {}
	int code;
	SF_AUTOREFLECT_SDC;
};
