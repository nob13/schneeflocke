#include <schnee/schnee.h>
#include <schnee/tools/Singleton.h>
#include <assert.h>
#include <stdio.h>

#include <schnee/tools/MicroTime.h>
#include <schnee/test/test.h>

#include <schnee/tools/async/DelegateBase.h>
#include <schnee/tools/async/impl/DelegateRegister.h> // testcases may do include them

/*
 * Tests some improved delegate model for working
 *
 * It is a bit slower, but protects delegates from being called on non-existing objects.
 */


struct TestObject : public sf::DelegateBase {
	TestObject () {
		SF_REGISTER_ME_WN("TestObject");
	}
	~TestObject () {
		SF_UNREGISTER_ME;
	}
	
	void aSimpleFunction (int arg) {
		printf ("Function got called, with arg=%d\n", arg);
	}

	int oneFunction (int arg) {
		// should not be optimizable...
		printf ("");
		return arg+1;
	}

	void mayNotCall (int arg) {
		assert (false && "Should not come here");
		std::abort();
	}

	void multipleCall (int arg) {
		sf::DelegateRegister::slock (delegateKey());
		aSimpleFunction (arg);
		sf::DelegateRegister::sunlock (delegateKey());
	}

	void withConstRef (const sf::String & ref){
		std::cout << "Hello " << ref << std::endl;
	}

	void withDoubleConstRef (const sf::String & a, const sf::String & b){
		std::cout << a << " " << b << std::endl;
	}

};

bool timeWasCalled = false;
sf::Time callTime;
void callIt (){
	timeWasCalled = true;
	callTime = sf::currentTime();
}

typedef sf::function <void (int arg)> Delegate;

int main (int argc, char * argv[]) {
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	{
		// Timed Xcall
		sf::Time startTime = sf::currentTime();
		sf::test::millisleep_locked (250);
		sf::xcallTimed(callIt, sf::futureInMs (100));
		sf::test::millisleep_locked (250);
		tassert (timeWasCalled, "sf::xcallTimed seems not to work");
		std::cout << "Time delta: " << (callTime - startTime).total_milliseconds() << std::endl;
	}
	{
		// Several tests
		TestObject obj;
		// Direct memcall
		sf::DMemFun1<void, TestObject, int> call (&obj, &TestObject::aSimpleFunction, obj.delegateKey());
		call (5);
		// MemCall as Delegate
		Delegate d = call;
		d (5);
		// Comparison, regular bind
		d = sf::bind (&TestObject::aSimpleFunction, &obj, _1);
		d (5);
		// Construction via memFun
		d = dMemFun (&obj, &TestObject::aSimpleFunction);
		d (5);
	}
	{
		// Removed objects
		Delegate d;
		{
			TestObject obj2;
			d = sf::dMemFun (&obj2, &TestObject::mayNotCall);
		}
		// obj2 doesn't live anymore, shall shall not come through
		d (5);
	}
	{
		// Multi call depth
		Delegate d;
		TestObject obj;
		d = sf::dMemFun (&obj, &TestObject::multipleCall);
		d (5);
	}
	{
		// const ref
		TestObject obj;
		sf::function<void (const sf::String &)> d = sf::dMemFun (&obj, &TestObject::withConstRef);
		d ("World");
	}
	{
		// Double const ref
		TestObject obj;
		sf::function<void (const sf::String &)> d = abind (dMemFun (&obj, &TestObject::withDoubleConstRef), sf::String("World"));
		d ("Hello");
		sf::function<void ()> d2 = abind (dMemFun (&obj, &TestObject::withDoubleConstRef), sf::String ("Hello"), sf::String ("World"));
		d2 ();
	}
	{
		// XCall1
		TestObject obj;
		xcall (sf::abind (dMemFun (&obj, &TestObject::withDoubleConstRef), sf::String ("Comes"), sf::String ("Through")));
		// Should come through
		sf::test::millisleep_locked(1);
	}
	{
		// XCall2, shall not come through
		{
			sf::function<void ()> d;
			sf::function<void ()> d2;
			{
				TestObject obj;
				d = sf::abind (dMemFun (&obj, &TestObject::withDoubleConstRef), sf::String ("NOT"), sf::String ("READABLE"));
				d2 = sf::abind (dMemFun(&obj, &TestObject::mayNotCall), 5);
			}
			sf::xcall (d);
			sf::xcall (d2);
		}
	}
	{
		// Call speed
		TestObject obj;
		int callNum = 1000000;
		double  a = sf::microtime();
		for (int i = 0; i < callNum; i++){
			obj.oneFunction (i);
		}
		double b = sf::microtime ();
		Delegate del = sf::bind (&TestObject::oneFunction, &obj,_1);
		for (int i = 0; i < callNum; i++){
			del (i);
		}
		double c = sf::microtime();
		del = dMemFun (&obj, &TestObject::oneFunction);
		for (int i = 0; i < callNum; i++){
			del (i);
		}
		double d = sf::microtime();

		std::cout << "Direct Call:   " << (b - a) / (double)(callNum) * 1000000.0 << "µs/Call" << std::endl;
		std::cout << "sf::bind Call: " << (c - b) / (double)(callNum) * 1000000.0 << "µs/Call" << std::endl;
		std::cout << "memFun Call:   " << (d - c) / (double)(callNum) * 1000000.0 << "µs/Call" << std::endl;
	}
	// DelegateRegister::destroyInstance();
	return 0;
}
