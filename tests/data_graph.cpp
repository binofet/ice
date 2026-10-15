#include "graph.generated.h"
#include "ice/data/document.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>
#include <type_traits>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::cerr << "Native graph failed " << __LINE__ << ": " << #expression << '\n';        \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
using namespace ice::data;
const auto            &fingerprint = metagen_examples::graph_fingerprint;
const value::record   &record(const value &input) { return std::get<value::record>(input.data); }
value::record         &record(value &input) { return std::get<value::record>(input.data); }
const value::sequence &sequence(const value &input) {
	return std::get<value::sequence>(input.data);
}
value::sequence       &sequence(value &input) { return std::get<value::sequence>(input.data); }
const std::string     &string(const value &input) { return std::get<std::string>(input.data); }
std::vector<std::byte> bytes(const std::filesystem::path &path) {
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		throw std::runtime_error("Missing fixture: " + path.string());
	std::string content((std::istreambuf_iterator<char>(stream)), {});
	return {reinterpret_cast<const std::byte *>(content.data()),
	        reinterpret_cast<const std::byte *>(content.data() + content.size())};
}
value load(const std::filesystem::path &path) {
	auto result = decode_binary(bytes(path), fingerprint);
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}
value reference_view(const object_reference &reference) {
	return value{
	    value::record{{"$resolved", value{value::record{{"document", value{reference.document}},
	                                                    {"identity", value{reference.identity}},
	                                                    {"type_id", value{reference.type}}}}}}};
}
value value_view(const value &input) {
	if (auto reference = std::get_if<object_reference>(&input.data))
		return reference_view(*reference);
	if (auto items = std::get_if<value::sequence>(&input.data)) {
		value::sequence output;
		for (const auto &item : *items)
			output.push_back(value_view(item));
		return value{std::move(output)};
	}
	if (auto fields = std::get_if<value::record>(&input.data)) {
		value::record output;
		for (const auto &[name, item] : *fields)
			output.emplace(name, value_view(item));
		return value{std::move(output)};
	}
	return input;
}
value graph_view(const graph_data &input) {
	value::sequence objects;
	for (const auto &[address, object] : input.objects)
		objects.push_back(value{value::record{{"document", value{address.first}},
		                                      {"identity", value{address.second}},
		                                      {"value", value_view(object)}}});
	return value{value::record{{"root", reference_view(input.root)},
	                           {"objects", value{std::move(objects)}}}};
}
int check_value_growth() {
	static_assert(std::is_nothrow_move_constructible_v<value>);
	static_assert(std::is_nothrow_move_assignable_v<value>);
	static_assert(std::is_nothrow_move_constructible_v<value::storage>);
	static_assert(std::is_nothrow_move_assignable_v<value::storage>);
	CHECK(std::holds_alternative<std::monostate>(value{}.data));
	value::sequence values;
	for (std::uint64_t index = 0; index < 128; ++index) {
		auto &fields = values.emplace_back().data.emplace<value::record>();
		fields["id"].data.emplace<std::uint64_t>(index);
		fields["label"].data.emplace<std::string>("player");
		auto &nested = fields["nested"].data.emplace<value::record>();
		nested["enabled"].data.emplace<bool>(true);
	}
	// Growing recursive vector storage must invoke each active alternative's move.
	// Byte relocation leaves map nodes pointing to sentinels in the old allocation.
	auto copied = values;
	copied.reserve(copied.capacity() + 1);
	copied.erase(copied.begin() + 17);
	copied.insert(copied.begin() + 17, values[17]);
	for (const auto &item : copied) {
		unsigned fields = 0, nested_fields = 0;
		for (const auto &[name, field] : record(item)) {
			++fields;
			if (name == "label")
				CHECK(string(field) == "player");
			if (name == "nested")
				for (const auto &[nested_name, nested] : record(field)) {
					++nested_fields;
					CHECK(nested_name == "enabled" && std::get<bool>(nested.data));
				}
		}
		CHECK(fields == 3 && nested_fields == 1);
	}
	record(copied[0])["label"] = value{std::string("independent")};
	CHECK(string(record(values[0]).at("label")) == "player");
	value moved(std::move(copied[0]));
	value assigned{false};
	assigned = std::move(moved);
	CHECK(string(record(assigned).at("label")) == "independent");
	assigned = value{};
	CHECK(std::holds_alternative<std::monostate>(assigned.data));
	value::sequence empty_records;
	for (unsigned index = 0; index < 32; ++index)
		empty_records.emplace_back().data.emplace<value::record>();
	empty_records.reserve(empty_records.capacity() + 1);
	for (const auto &item : empty_records)
		CHECK(record(item).begin() == record(item).end());
	return 0;
}
} // namespace

int main(int count, char **arguments) {
	try {
		CHECK(count == 2);
		CHECK(check_value_growth() == 0);
		std::filesystem::path fixtures(arguments[1]);
		auto                  corpus   = load(fixtures / "graph.corpus.mdb");
		unsigned              accepted = 0, rejected = 0;
		for (const auto &entry : sequence(corpus)) {
			const auto &row           = record(entry);
			const auto &name          = string(row.at("name"));
			bool        should_accept = std::get<bool>(row.at("accept").data);
			auto        definitions   = catalog::read(row.at("schema"));
			if (!definitions) {
				if (should_accept)
					std::cerr << name << ": " << definitions.error().message << '\n';
				CHECK(!should_accept);
				++rejected;
				continue;
			}
			graph_store     store(std::move(*definitions));
			document_reader reader =
			    [&](const std::string &uri) -> ice::expected<value, diagnostic> {
				const auto &external = record(row.at("external"));
				auto        found    = external.find(uri);
				if (found == external.end())
					return ice::unexpected(diagnostic{"Missing fixture URI", uri});
				return found->second;
			};
			auto candidate = store.prepare("memory://main/party.json", row.at("document"), reader);
			if (!candidate && should_accept)
				std::cerr << name << ": " << candidate.error().message << " at "
				          << candidate.error().path << '\n';
			if (candidate && !should_accept)
				std::cerr << "Unexpectedly accepted: " << name << '\n';
			CHECK(bool(candidate) == should_accept);
			CHECK(!store.snapshot());
			if (!candidate) {
				++rejected;
				continue;
			}
			++accepted;
			auto actual   = encode_binary(graph_view(*candidate->data()), fingerprint);
			auto expected = encode_binary(row.at("view"), fingerprint);
			if (actual != expected)
				std::cerr << "Effective graph mismatch: " << name << '\n';
			CHECK(actual && expected && *actual == *expected);
			CHECK(store.activate(*candidate, 0) == std::uint64_t{1});
			CHECK(store.snapshot()->data->authored.at("memory://main/party.json") ==
			      row.at("document"));
		}
		CHECK(accepted + rejected == metagen_examples::graph_case_count);
		std::cout << accepted << " accepted and " << rejected
		          << " rejected shared Python/C++ graph cases agree\n";

		auto source      = load(fixtures / "graph.party.mdb");
		auto external    = load(fixtures / "graph.guests.mdb");
		auto definitions = catalog::read(load(fixtures / "graph.catalog.mdb"));
		CHECK(definitions);
		graph_store     store(*definitions), foreign(*definitions);
		unsigned        reads  = 0;
		document_reader reader = [&](const std::string &uri) -> ice::expected<value, diagnostic> {
			++reads;
			if (uri != "assets://people/players.json?revision=1")
				return ice::unexpected(diagnostic{"Unexpected URI", uri});
			return external;
		};
		auto first = store.prepare("memory://main/party.json", source, reader);
		CHECK(first && reads == 1);
		CHECK(!foreign.activate(*first, 0));
		CHECK(!store.activate(*first, 1));
		CHECK(store.activate(*first, 0) == std::uint64_t{1});
		auto       retained = store.snapshot();
		const auto root     = retained->data->root;
		auto       handle   = store.handle(root);
		CHECK(handle && handle->owner == retained->owner && store.inspect(*handle));
		auto detached = store.inspect(*handle);
		CHECK(detached);
		record(*detached)["players"] = value{value::sequence{}};
		CHECK(sequence(record(*store.inspect(*handle)).at("players")).size() == 2);
		auto  changed = source;
		auto &objects = sequence(record(changed).at("objects"));
		record(objects[2])["players"] =
		    value{value::record{{"$replace", value{value::sequence{}}}}};
		auto second = store.prepare("memory://main/party.json", changed, reader);
		CHECK(second && store.snapshot() == retained);
		auto invalid = changed;
		record(sequence(record(invalid).at("objects"))[1])["host"] =
		    value{value::record{{"$ref", value{std::string("missing")}}}};
		auto failed = store.prepare("memory://main/party.json", invalid, reader);
		CHECK(!failed && !failed.error().path.empty() && store.snapshot() == retained);
		CHECK(store.activate(*second, 1) == std::uint64_t{2});
		CHECK(!store.inspect(*handle));
		CHECK(sequence(
		          record(retained->data->objects.at({root.document, root.identity})).at("players"))
		          .size() == 2);
		auto second_handle = store.handle(root);
		CHECK(second_handle &&
		      sequence(record(*store.inspect(*second_handle)).at("players")).empty());
		CHECK(!store.undo(1));
		CHECK(store.undo(2) == std::uint64_t{3});
		CHECK(!store.inspect(*handle) && !store.inspect(*second_handle));
		CHECK(sequence(record(*store.inspect(*store.handle(root))).at("players")).size() == 2);
		CHECK(!store.undo(3));
		auto other = foreign.prepare("memory://main/party.json", source, reader);
		CHECK(other && foreign.activate(*other, 0) == std::uint64_t{1} &&
		      !foreign.inspect(*store.handle(root)));
		for (std::uint64_t revision = 3; revision < 15; ++revision)
			CHECK(store.activate(*second, revision) == revision + 1);
		for (std::uint64_t revision = 15; revision < 23; ++revision)
			CHECK(store.undo(revision) == revision + 1);
		CHECK(!store.undo(23));
		CHECK(!store.prepare("relative/path", source, reader));
		CHECK(!store.prepare("memory://main/party.json#selector", source, reader));
		auto moved = std::move(*second);
		CHECK(!second->data() && !store.activate(*second, 23));
		CHECK(store.activate(moved, 23) == std::uint64_t{24});

		// Authored persistence is independent of active revisions/materialized inheritance.
		const auto serialized = bytes(fixtures / "graph.party.mdb");
		CHECK(encode_binary(source, fingerprint) == serialized);
		for (std::size_t prefix = 0; prefix < serialized.size(); ++prefix)
			CHECK(!decode_binary(std::span(serialized).first(prefix), fingerprint));
		auto trailing = serialized;
		trailing.push_back(std::byte{0});
		CHECK(!decode_binary(trailing, fingerprint));
		auto wrong = fingerprint;
		wrong[0] ^= std::byte{1};
		CHECK(!decode_binary(serialized, wrong));
		CHECK(!encode_binary(value{root}, fingerprint));
		CHECK(!encode_binary(value{std::numeric_limits<double>::quiet_NaN()}, fingerprint));
		CHECK(!encode_binary(value{std::string("\xED\xA0\x80")}, fingerprint));
		value boundaries{value::sequence{value{std::int64_t(INT64_MIN)}, value{UINT64_MAX},
		                                 value{-0.0}, value{std::string("☃ 😎\n")}}};
		auto  boundary_bytes = encode_binary(boundaries, fingerprint);
		CHECK(boundary_bytes && decode_binary(*boundary_bytes, fingerprint) == boundaries);
		std::mt19937 random(9123);
		for (unsigned trial = 0; trial < 1000; ++trial) {
			std::vector<std::byte> garbage(36 + random() % 128);
			std::copy(serialized.begin(), serialized.begin() + 36, garbage.begin());
			for (std::size_t i = 36; i < garbage.size(); ++i)
				garbage[i] = std::byte(random() & 255);
			if (auto decoded = decode_binary(garbage, fingerprint)) {
				auto encoded = encode_binary(*decoded, fingerprint);
				CHECK(encoded && *encoded == garbage);
			}
		}
		std::cout << "Native graph prepare/activate/undo, stale/foreign handles, owned snapshots "
		             "and MDB1 bounds passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "Unexpected native graph exception: " << error.what() << '\n';
		return 1;
	}
}
