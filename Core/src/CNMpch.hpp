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
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <utility>

#ifdef CL_Platform_Windows
#include <WinSock2.h>
#endif

#if defined(CL_Platform_Linux) || defined(CL_Platform_Mac)
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#endif

// CNM
#include <CNM/utils.h>
#include <CNM/cnm_core.h>
#include <CNM/WireFormat.h>
#include <CNM/Buffer.h>
#include <CNM/Replication.h>

#include <ECS/ECS.h>
#include <CNM/Socket.h>