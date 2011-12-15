#pragma once

namespace sf {
namespace test {
/// sleep for some seconds (workaround for win32, unix has it)
/// note: depreciated as you must manually unlock libschnee before
void _sleep (unsigned int seconds);

/// unlocks libschnee and waits
void sleep_locked (unsigned int seconds);

/// sleep for some seconds (unix: with usleep, win32: with Sleep(..))
/// Note: depreciated as you must manually unlock libschnee before
void _millisleep (unsigned int ms);

/// Unlocks schnee lock and waits
void millisleep_locked (unsigned int ms);

}
}
