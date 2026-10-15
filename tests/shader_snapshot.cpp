#include "ice/render/shader_snapshot.h"
#include <cstdio>
#include <memory>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Shader snapshot check %d: %s\n", __LINE__, #condition);          \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
class provider final : public ice::io::virtual_scheme_handler {
public:
	std::string                   text   = "abc", requested;
	unsigned                      reads  = 0;
	ice::io::resource_read_result result = ice::io::resource_read_result::success;
	ice::io::resource_read_result read(const ice::io::resource_uri &uri,
	                                   std::vector<unsigned char>  &bytes,
	                                   std::string                 &error) override {
		++reads;
		requested = uri.string();
		bytes.assign(text.begin(), text.end());
		error = "provider failure";
		return result;
	}
};
} // namespace
int main() {
	using ice::render::shader_snapshot;
	ice::io::resource_resolver resolver;
	auto                       source = std::make_shared<provider>();
	resolver.register_handler("memory", source);
	auto first = shader_snapshot::read(resolver, "MeMoRy://source/./shader?quality=1#entry");
	CHECK(first && source->reads == 1 && first->source() == "abc");
	CHECK(first->uri() == "memory://source/shader?quality=1#entry");
	CHECK(source->requested == "memory://source/shader?quality=1");
	CHECK(first->sha256() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	source->text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	auto second  = shader_snapshot::read(resolver, "memory://source/shader");
	CHECK(second && source->reads == 2);
	CHECK(second->sha256() == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
	CHECK(first->source() == "abc" && first->sha256() != second->sha256());
	source->text.assign(1000000, 'a');
	auto million = shader_snapshot::read(resolver, "memory://source/shader");
	CHECK(million &&
	      million->sha256() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
	source->text.assign(1024 * 1024, 'x');
	CHECK(shader_snapshot::read(resolver, "memory://source/shader"));
	source->text.push_back('x');
	CHECK(!shader_snapshot::read(resolver, "memory://source/shader"));
	for (const auto &bad : {std::string{}, std::string("a\0b", 3), std::string("\xED\xA0\x80")}) {
		source->text = bad;
		CHECK(!shader_snapshot::read(resolver, "memory://source/shader"));
	}
	source->text   = "valid";
	source->result = ice::io::resource_read_result::error;
	CHECK(!shader_snapshot::read(resolver, "memory://source/shader"));
	const auto before = source->reads;
	CHECK(!shader_snapshot::read(resolver, std::string(4097, 'x')));
	CHECK(source->reads == before);
	std::puts("Shader snapshots passed: one read, immutable bytes, canonical URI/query, SHA-256 "
	          "vectors and source/URI budgets");
}
