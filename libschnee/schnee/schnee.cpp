#include "net/impl/IOService.h"
#include "tools/async/impl/DelegateRegister.h"
#include "net/impl/UDTMainLoop.h"
#include "net/TLSCertificates.h"
#include "settings.h"

#include <gcrypt.h>
#include <gnutls/gnutls.h>

#ifndef WIN32
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#else
// Source: https://build.opensuse.org/package/view_file?file=libsoup-2.28.1-gcrypt.patch&package=mingw64-libsoup&project=windows%3Amingw%3Awin64&srcmd5=2a77d044b5e1e3e626a675c89008d772
static int gcry_win32_mutex_init (void **priv)
{
   int err = 0;
   CRITICAL_SECTION *lock = (CRITICAL_SECTION*)malloc (sizeof (CRITICAL_SECTION));

   if (!lock)
       err = ENOMEM;
   if (!err) {
       InitializeCriticalSection (lock);
       *priv = lock;
   }
   return err;
}

static int gcry_win32_mutex_destroy (void **lock)
{
   DeleteCriticalSection ((CRITICAL_SECTION*)*lock);
   free (*lock);
   return 0;
}

static int gcry_win32_mutex_lock (void **lock)
{
   EnterCriticalSection ((CRITICAL_SECTION*)*lock);
   return 0;
}

static int gcry_win32_mutex_unlock (void **lock)
{
   LeaveCriticalSection ((CRITICAL_SECTION*)*lock);
   return 0;
}


static struct gcry_thread_cbs gcry_threads_win32 = {       \
   (GCRY_THREAD_OPTION_USER | (GCRY_THREAD_OPTION_VERSION << 8)),     \
   NULL, gcry_win32_mutex_init, gcry_win32_mutex_destroy, \
   gcry_win32_mutex_lock, gcry_win32_mutex_unlock,    \
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };


#endif

void global_InitGnuTls () {
#ifndef WIN32
	// multithreading
	gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
#else
	gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_win32);
#endif
	// debug level
	gnutls_global_set_log_level (1);
	// non-blocking random
	gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0);
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
