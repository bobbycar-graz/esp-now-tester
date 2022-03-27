#pragma once

// 3rdparty lib includes
#include <espchrono.h>

enum MemoryDebug {Off,Normal,Fast};
extern MemoryDebug memoryDebug;
extern espchrono::millis_clock::time_point lastMemoryDebug;

void init_debugconsole();
void update_debugconsole();
