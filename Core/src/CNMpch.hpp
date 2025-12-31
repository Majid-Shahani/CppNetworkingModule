#pragma once

// STANDARD LIBRARY
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_map>

#ifdef CL_Platform_Windows
#include <WinSock2.h>
#endif

#if defined(CL_Platform_Linux) || defined(CL_Platform_Mac)
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#endif