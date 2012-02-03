#include "net/impl/IOService.h"
#include "tools/async/impl/DelegateRegister.h"
#include "net/impl/UDTMainLoop.h"
#include "net/TLSCertificates.h"
#include "settings.h"

#ifndef WIN32
#include <gcrypt.h>
#endif
#include <gnutls/gnutls.h>

#ifndef WIN32
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif

void global_InitGnuTls () {
#ifndef WIN32
	// Technically we don't need multithreading support anymore
	// Because all TLS Operations are working inside one mutex now.
	// Win32 doesn't have it anymore because it's new GnuTLS library
	// is not based on libcrypt anymore.

	// multithreading
	gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
#endif
	// debug level
	gnutls_global_set_log_level (1);
#ifndef WIN32
	// non-blocking random
	gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0);
#endif
	gnutls_global_init ();
}

void global_uninitGnuTls () {
	gnutls_global_deinit();
}


#include "schnee.h"
#include "sftypes.h"
#include "schnee/tools/Log.h"

#include <stdlib.h>

namespace sf {
namespace schnee {

static bool gInitialized = false;

// Defined in settings.cpp
void parseArguments (int argc, const char * argv[]);

/// Defined in settings.cpp
void setInitialCertificates ();


bool init (int argc, const char * argv []) {
	if (isInitialized()) return true;
	parseArguments (argc, argv);

	const Settings & settings = schnee::settings();
	initLogging (settings.enableLog, settings.enableFileLog, settings.logFile);
	global_InitGnuTls ();
	TLSCertificates::initInstance();
	schnee::setInitialCertificates ();
	IOService::initInstance ();
	IOService::instance().start();
	Log (LogInfo) << LOGID << "Started IOService, thread " << IOService::threadId(IOService::service()) << std::endl;

	DelegateRegister::initInstance();
	UDTMainLoop::initInstance();

	gInitialized = true;
	return true;
}

void deinit () {
	UDTMainLoop::destroyInstance();
	DelegateRegister::instance().finish();
	IOService::instance().stop();
	DelegateRegister::destroyInstance();
	IOService::destroyInstance ();
	TLSCertificates::destroyInstance();
	global_uninitGnuTls ();
	gInitialized = false;
	deInitLogging ();
}

bool isInitialized () {
	return gInitialized;
}

const char * version () {
	return SF_VERSION; // set by cmake script

}

Mutex & mutex () {
	static Mutex mutex;
	return mutex;
}


}

}
