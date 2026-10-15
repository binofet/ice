
#include "ice/memory/memory.h"

#include "ice/memory/system_memory_pool.h"
namespace ice::memory {

// dlMemoryPool icMemory::_defaultPool;

dl_memory_pool   *memory::string_pool_;
dl_memory_pool   *memory::script_pool_;
tlsf_memory_pool *memory::tlsf_pool_;

ice::result memory::init() {
	//	_defaultPool.Create(1024*1024*256);

	string_pool_ = new dl_memory_pool(mB(2));

	tlsf_pool_ = new tlsf_memory_pool();
	tlsf_pool_->create(mB(2));

	script_pool_ = new dl_memory_pool(mB(2));

	return IC_OK;
}

ice::result memory::shutdown() {
	//_defaultPool.Destroy();
	delete string_pool_;
	delete tlsf_pool_;
	delete script_pool_;
	return IC_OK;
}

memory_pool_i *memory::get_default_pool() {
	static dl_memory_pool default_pool(mB(128));

	return &default_pool;
}
/*
void* operator new (size_t num_bytes)
{
    return icMemory::getDefaultPool()->malloc(num_bytes);
}

void operator delete(void* p)
{
    icMemory::getDefaultPool()->free(p);
}

void* operator new [](size_t num_bytes)
{
    return icMemory::getDefaultPool()->malloc(num_bytes);
}

void operator delete [](void* p)
{
    icMemory::getDefaultPool()->free(p);
}

*/
} // namespace ice::memory
