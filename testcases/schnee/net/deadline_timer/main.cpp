#include <schnee/schnee.h>
#include <schnee/test/timing.h>
#include <schnee/net/impl/DeadlineTimer.h>
#include <schnee/net/impl/IOService.h>

#include <boost/asio.hpp>

sf::DeadlineTimer * timer;

sf::Mutex waitBlock;

/*
 * TestsDeadlineTimer
 * - Must be deletable in its own callback
 * - Must be deletable from another thread
 */

void handler (){
	delete timer;
	waitBlock.unlock ();
}

bool called = false;
void handler2 (boost::system::error_code ec){
	std::cout << "Was called with " << ec.message() << std::endl;
	called = true;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);

	// What happens if we delete a boost asio timer?
	boost::asio::deadline_timer * t = new boost::asio::deadline_timer (sf::IOService::service(), sf::futureInMs (100));
	t->async_wait (handler2);
	delete t;
	sf::test::sleep (1);
	std::cout << "Deleting immediately lead to a call: " << called << std::endl;

	/*
	 * Tests wether a timer can be deleted in is own callback, this is
	 * a source of headache for us.
	 */
	timer = new sf::DeadlineTimer ();
	timer->timeOut() = handler;
	waitBlock.lock();
	timer->start (sf::futureInMs (50));

	waitBlock.lock (); // waits until handler.
	waitBlock.unlock (); // otherwise boost gets angry.

	// now we delete from a different thread
	sf::DeadlineTimer * timer2 = new sf::DeadlineTimer ();
	timer2->timeOut () = handler;
	timer2->start (sf::futureInMs (10));
	waitBlock.lock();
	delete timer2;
	waitBlock.unlock();


	return 0;
}
