#pragma once

// STANDARD LIBRARY
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <stdexcept>
#include <vector>

#ifdef CNM_Platform_Windows
#include <WinSock2.h>
#endif

#ifdef CNM_Platform_Linux || CNM_Platform_Mac
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#endif