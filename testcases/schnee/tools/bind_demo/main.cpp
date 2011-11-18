#include <schnee/test/test.h>
#include <schnee/tools/async/MemFun.h>       // contains abind
#include <schnee/tools/async/DelegateBase.h> // contains xcall
#include <schnee/tools/async/ABindAround.h>

#include <schnee/schnee.h> // init stuff (needed for xcall)
#include <stdio.h>

typedef boost::function<void (int)> Callback; // boost function is the same like sf::function

// Synchronous worker function
void reallyDoingFunc (const Callback & callback) {
	callback (5);
}

/// Asynchronous API function
void asyncDemoFunc (const Callback & callback) {
	sf::xcall (sf::abind (&reallyDoingFunc, callback));
}

/// Self defined callback with specific context
void contextCallback (int result, const std::string & context) {
	printf ("Called with %d (context=%s)\n", result, context.c_str());
}


struct Watched : public sf::DelegateBase {
	Watched () {
		SF_REGISTER_ME;
	}
	~Watched () {
		SF_UNREGISTER_ME;
	}
	
	void func () {
		printf ("Watched::func called\n");
	}
};

// Abindaroud calling func
// calls f(5)
void boundCaller (const sf::function<void (int)> & f) {
	f(5);
}

// Abindaround destination func
void dstFunc (int x, int y, const sf::String & s) {
	printf ("Called with %d,%d and %s", x, y, s.c_str());
}

int main (int argc, char * argv[]) {
	sf::schnee::SchneeApp app (argc, argv);
	
	// Abind weg:
	asyncDemoFunc (sf::abind (&contextCallback, std::string ("Thats my context")));
	
	// Boost Weg
	asyncDemoFunc (boost::bind (&contextCallback, _1, std::string ("Thats my context using boost")));
	
	sf::test::millisleep (100);
	
	Watched * w = new Watched;
	boost::function<void ()> call = sf::dMemFun (w, &Watched::func);
	call (); // comes through
	delete w;
	call (); // doesn't comes through
	
	Watched * dummy = new Watched; // this would be found upon destruction of SchneeApp
	delete dummy; // but we do not want this testcase to crash
	
	sf::function<void (int, const sf::String & )> bound = sf::abindAround2<int, const sf::String&>(&boundCaller, &dstFunc);
	bound (18, sf::String("Hi"));

	return 0;
}
