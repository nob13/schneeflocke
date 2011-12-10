#include <schnee/test/test.h>
#include <schnee/tools/ResultCallbackHelper.h>
#include <schnee/net/http/HttpContext.h>

using namespace sf;

void callback (Error e, const HttpResponsePtr & response, const ResultCallback & callback, HttpResponsePtr * target){
	*target = response;
	callback (e);
}

HttpResponsePtr testWebsite (const std::string & url) {
	HttpContext context;
	ResultCallbackHelper helper;
	HttpResponsePtr result;
	context.get(Url (url), 10000, abind (&callback, helper.onResultFunc(), &result));
	bool suc = helper.waitReadyAndNoError(1000);
	if (!suc) {
		fprintf (stderr, "Should load %s\n", url.c_str());
		return HttpResponsePtr();
	}
	return result;
}

int testHttp () {
	HttpResponsePtr c = testWebsite ("http://sflx.net/");
	return !c ? 1 : 0;
}

int testHttps () {
	HttpResponsePtr c = testWebsite ("https://sflx.net/");
	return !c ? 1 : 0;
}

int testChunked () {
	HttpResponsePtr c = testWebsite ("http://www.heise.de/");
	return !c ? 1 : 0;
}

int test302 () {
	HttpResponsePtr c = testWebsite ("http://www.facebook.com/");
	if (!c) return 1;
	tcheck (c->resultCode == 302, "Resultcode mismatch");
	return 0;
}

int testReuse () {
	HttpContext context;
	ResultCallbackHelper helper;
	HttpResponsePtr result;
	context.get("http://sflx.net/index.html", 100000, abind (&callback, helper.onResultFunc(), &result));
	bool suc = helper.waitReadyAndNoError(100000);
	tcheck1 (suc);
	helper.reset();
	context.get ("http://sflx.net/sflx.css", 100000, abind (&callback, helper.onResultFunc(), &result));
	suc = helper.waitReadyAndNoError(100000);
	tcheck1 (suc);
	tcheck (context.createdConnections() == 1, "May only have 1 created connection if reuse is working");
	tcheck (context.pendingConnections() <= 1, "At maximum 1 connection may pend now");
	return 0;
}

int testReuseSsl () {
	HttpContext context;
	ResultCallbackHelper helper;
	HttpResponsePtr result;
	context.get("https://sflx.net/index.html", 100000, abind (&callback, helper.onResultFunc(), &result));
	bool suc = helper.waitReadyAndNoError(100000);
	tcheck1 (suc);
	helper.reset();
	context.get ("https://sflx.net/sflx.css", 100000, abind (&callback, helper.onResultFunc(), &result));
	suc = helper.waitReadyAndNoError(100000);
	tcheck1 (suc);
	tcheck (context.createdConnections() == 1, "May only have 1 created connection if reuse is working");
	tcheck (context.pendingConnections() <= 1, "At maximum 1 connection may pend now");
	return 0;
}


int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	testcase_start();
	
	// Custom call
	Url url;
	bool foundOne = false;
	for (int i = 1; i < argc; i++) {
		String s ( argv[i]);
		if (s.size() >= 3 && s.substr(0,2) != "--"){
			url = s;
			foundOne = true;
		}
	}
	if (!foundOne) {
		// regular testcase
		testcase (testHttp());
		testcase (testHttps());
		testcase (testChunked());
		testcase (test302());
		testcase (testReuse());
		testcase (testReuseSsl());
	} else {
		printf ("Doing custom call to: %s\n", url.toString().c_str());
		ResultCallbackHelper helper;
		HttpContext context;
		context.get(url, 10000, bind (helper.onResultFunc(), _1));
		bool suc = helper.waitUntilReady(10000);
		tcheck1 (suc);
		printf ("Result Code: %s\n", toString (helper.result()));
	}
	testcase_end();
}

