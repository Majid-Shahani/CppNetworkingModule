#pragma once

namespace Carnival {
	// Serialized ring buffer :
	// Drop is fine, write index is auth, if read > write, that's capacity, return false if cant write
	// if write > read, capacity = size - read.
	// packet boundaries, recrod say size and type, it should be complete,
	// write flag will be checked, if true, read will add all to the packet.
	// packet send function cannot wrap, data must be contiguous. 
	// if write flag is false, and read index reaches write index, reset both to 0
	// sound a lot like locks.
}