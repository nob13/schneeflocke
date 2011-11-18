#include "x509.h"
#include <schnee/tools/Log.h>

namespace sf {
namespace x509 {

bool Certificate::verify (Certificate * trusted){
	unsigned int v = 0;
	int r = gnutls_x509_crt_verify (data, &trusted->data, 1, 0, &v);
	if (r) {
		Log (LogError) << LOGID << "Verification error: " << gnutls_strerror_name(r) << std::endl;
		return false;
	} else {
		if (v) {
			Log (LogInfo) << LOGID << "Verification gave result: " << v << std::endl;
			return false;
		}
		return true;
	}
}

}
}
