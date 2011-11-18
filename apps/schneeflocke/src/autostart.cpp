#ifdef WIN32
#include <windows.h>
#include <winreg.h>
#include <tchar.h>
#endif

#include "autostart.h"

namespace autostart {

#ifndef WIN32

// no support for UNIX yet

bool isAvailable () { return false; }

bool isInstalled (const std::string & name) { return false; }

bool install (const std::string & name, const std::string & path, const std::string & parameters) { return false; }

bool uninstall (const std::string & name) { return false; }

#else

// converts regular string to Wide String
// found here http://social.msdn.microsoft.com/Forums/en-US/Vsexpressvc/thread/0f749fd8-8a43-4580-b54b-fbf964d68375/
static std::wstring s2ws(const std::string& s) {
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0); 
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

// win32String --> generates a WIN32 string pointer from C++ string
#ifdef UNICODE
#define win32String(S) s2ws(S).c_str()	
#else
#define win32String(S) S.c_str()
#endif


static LPCTSTR runSubKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
 

bool isAvailable () {
	return true;
}

/// RIIA handle for registry
class RegHandle {
public:
	RegHandle (HKEY key, LPCTSTR subKey) {
		LONG err = ::RegOpenKey (key, subKey, &mHandle);
		mSuccess = (err == ERROR_SUCCESS);
	}
	~RegHandle () {
		if (mSuccess) {
			RegCloseKey (mHandle);
		}
	}

	bool success () const { return mSuccess; }
	HKEY handle () const { return mHandle; }
private:

	bool mSuccess;
	HKEY mHandle;
};


bool isInstalled (const std::string & name) { 
	RegHandle rh (HKEY_CURRENT_USER, runSubKey);
	if (!rh.success()) return false;
	LONG err = ::RegQueryValueEx (rh.handle(), win32String(name), NULL, NULL, NULL, NULL);
	return err == ERROR_SUCCESS ? true : false;
}


bool install (const std::string & name, const std::string & path, const std::string & parameters) { 
	RegHandle rh (HKEY_CURRENT_USER, runSubKey);
	if (!rh.success()) return false;
	
	std::string fullPath = std::string ("\"") + path + "\"";
	if (!parameters.empty()){
		if (parameters[0] != ' ') fullPath += ' ';
		fullPath += parameters;
	}

	// backslashes must be escaped according to documentation
	// but that seems not true!
	// also registry-autostart seems not to like slashes instead of backslashes.
	std::string escaped = fullPath;
	for (std::string::iterator i = escaped.begin(); i != escaped.end(); i++) {
		if (*i == '/') *i = '\\';
	}
	
	LONG err = ::RegSetValueEx (rh.handle(), win32String(name), NULL, REG_SZ, (const BYTE *) (win32String (escaped)), strlen (win32String(escaped)) + 1);
	return err == ERROR_SUCCESS ? true : false;
}

bool uninstall (const std::string & name) { 
	RegHandle rh (HKEY_CURRENT_USER, runSubKey);
	if (!rh.success()) return false;

	LONG err = ::RegDeleteValue (rh.handle(), win32String (name));
	return err == ERROR_SUCCESS ? true : false;
}


#endif

}
