#pragma once

/**
 * @file Includes various Test Functions shared by the testcases
 */


#include "timing.h"
#include <stdlib.h>
#include <stdio.h>
#include <schnee/schnee.h>


/// An assertion command which doesn't get optimized away by NDEBUG
#define tassert(COND,MSG) if (!(COND)) { fprintf (stderr, "assert %s failed: %s (%s:%d)\n", #COND, MSG, __FILE__, __LINE__); ::abort(); }
/// like tassert, but without a message
#define tassert1(COND) if (!(COND)) {\
		fprintf (stderr, "assert %s failed: (%s:%d)\n", #COND, __FILE__, __LINE__);\
		::abort(); \
	}

/// Checks if a condition is not true, and returns 1 in that case.
#define tcheck(COND,MSG) if (!(COND)) {\
		fprintf (stderr, "%s failed: %s (%s:%d)\n", #COND, MSG, __FILE__, __LINE__); \
		return 1; \
	}

/// Like tcheck, without msg
#define tcheck1(COND) if (!(COND)) {\
		fprintf (stderr, "%s failed (%s:%d) \n", #COND, __FILE__, __LINE__);\
		return 1; \
	}

#define testcase_start() int ret = 0; int testcase_count = 0;
#define testcase_end() fprintf (stderr, "%d/%d of tests failed\n", ret, testcase_count); return ret;

/// Main simple runner. If a function returns not null it adds one to the variable ret.
#define testcase(FUNC) \
		{ \
			printf ("%s starting...\n", #FUNC); \
			int r = FUNC; \
			if (r) {\
				fprintf (stderr, "%s failed\n", #FUNC); \
				ret++;\
			} \
			else { \
				printf ("%s succeded!\n", #FUNC);\
			}\
			testcase_count++;\
		};
