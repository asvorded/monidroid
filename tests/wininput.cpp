#define BOOST_TEST_MODULE Windows Virtual input tests

#include <thread>

#include <Windows.h>
#include <boost/test/included/unit_test.hpp>

#include "native.h"

struct GlobalConfig {
    GlobalConfig() {
        boost::unit_test::unit_test_log.set_threshold_level(boost::unit_test::log_messages);
    }
};

BOOST_GLOBAL_FIXTURE(GlobalConfig);

BOOST_AUTO_TEST_CASE(WinInputMovingTest) {
	DWORD id = GetCurrentProcessId();
	DWORD session = -1;
	ProcessIdToSessionId(GetCurrentProcessId(), &session);
	BOOST_CHECK_EQUAL(session, WTSGetActiveConsoleSessionId());

	BOOST_TEST_MESSAGE("* Do not move the mouse! *");
	BOOST_TEST_MESSAGE("Start within 3 seconds...");

	std::this_thread::sleep_for(std::chrono::seconds(3));

	POINT start { }, end { };
	GetCursorPos(&start);
	for (int i = 0; i < 10; ++i) {
		monitorSendInput(Monitor(), 10, 10);
	}
	GetCursorPos(&end);

	BOOST_CHECK_NE(start.x, end.x);
	BOOST_CHECK_NE(start.y, end.y);
}