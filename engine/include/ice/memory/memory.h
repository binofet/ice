#pragma once

#include "ice/globals.h"

#include "ice/memory/dl_memory_pool.h"
#include "ice/memory/tlsf_memory_pool.h"
namespace ice::memory {

#define kB(x) (1024 * x)
#define mB(x) (1024 * 1024 * x)

/// Process-level legacy allocator registry. Initialize before obtaining pools and shut down after
/// releasing all pool allocations.
class memory {
public:
	/// Initialize against the supplied borrowed dependencies; inspect the result before using the
	/// subsystem.
	static ice::result init();
	/// Release process allocator state after all dependent objects/blocks have been destroyed.
	static ice::result shutdown();

	/// Borrow the process default pool between memory init and shutdown.
	static memory_pool_i *get_default_pool();

	/// Borrow the configured script allocator between memory init and shutdown.
	static memory_pool_i *get_script_pool() { return script_pool_; }
	/// Borrow the configured legacy string allocator between memory init and shutdown.
	static memory_pool_i *get_string_pool() { return string_pool_; }
	/// Borrow the configured TLSF allocator between memory init and shutdown.
	static memory_pool_i *get_tlsf_pool() { return tlsf_pool_; }

private:
	static tlsf_memory_pool *tlsf_pool_;

	static dl_memory_pool *string_pool_;
	static dl_memory_pool *script_pool_;
};

} // namespace ice::memory
