#pragma once

namespace sf {
namespace test {
// sleep for some seconds (workaround for win32, unix has it)
void sleep (unsigned int seconds);

/// unlocks libschnee and waits
void sleep_locked (unsigned int seconds);

/// sleep for some seconds (unix: with usleep, win32: with Sleep(..))
void millisleep (unsigned int ms);

/// Unlocks schnee lock and waits
void millisleep_locked (unsigned int ms);

}
}
