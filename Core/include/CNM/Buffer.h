#pragma once

#include <memory>
#include <atomic>
#include <cstdint>

namespace Carnival {
	// Serialized ring buffer :
	// Drop is fine, write index is auth, if read > write, that's capacity, return false if cant write
	// if write > read, capacity = size - write.
	// packet boundaries, recrod say size and type, it should be complete,
	// write flag will be checked, if true, read will add all to the packet.
	// packet send function cannot wrap, data must be contiguous. 
	// if write flag is false, and read index reaches write index, reset both to 0
	// sound a lot like locks.



	// Replication Buffer :
	// One big array, size = configNum(1024) * sizeof(ReplicationRecord)
	// MPSC, Atomic Write Index. Non-Atomic read index. writes and reads will be done in phases.
	// Writer Reserves a spot, by moving index forward. if tmp index > capacity, return false 
	// or throw error. once write phase ends, read phase begins from first index. serialization is done
	// once read phase ends, both indices are set to 0. buffer is reset.
}