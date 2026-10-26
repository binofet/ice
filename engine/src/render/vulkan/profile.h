#pragma once
#include "ice/core/expected.h"
#include "ice/render/types.h"
#include <algorithm>
#include <charconv>
#include <map>
#include <span>
#include <string_view>

namespace ice::render::vulkan::detail {
inline auto reject(const char *message) { return ice::unexpected(diagnostic{message}); }
inline constexpr std::size_t push_bytes = 80;
struct program {
	std::vector<std::uint32_t> vertex, fragment;
	bool                       pushes = false;
};
// This is bounded profile reflection, not a SPIR-V validator or compiler sandbox.
// The caller supplies trusted, validated Vulkan1.1 modules from an offline compiler.
inline ice::expected<bool, diagnostic>
reflect(std::span<const std::uint32_t> words, std::uint32_t stage, std::string_view entry,
        vertex_layout layout = vertex_layout::position_normal) {
	if (words.size() < 5 || words.size() > 32768 || words[0] != 0x07230203 ||
	    words[1] < 0x00010000 || words[1] > 0x00010300 || (words[1] & 255) || !words[3] ||
	    words[3] > 65536 || words[4] || entry.empty() || entry.size() > 128)
		return reject("Invalid Vulkan1.1 SPIR-V header or entry");
	std::map<std::uint32_t, std::vector<std::uint32_t>>              types;
	std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> offsets, strides;
	std::map<std::pair<std::uint32_t, std::uint32_t>, bool>          columns;
	std::vector<std::uint32_t>                                       pushes;
	std::vector<std::pair<std::uint32_t, std::uint32_t>>             inputs;
	std::map<std::uint32_t, std::uint32_t>                           locations, builtins;
	std::map<std::uint32_t, bool>                                    blocks;
	unsigned                                                         entries = 0;
	for (std::size_t at = 5; at < words.size();) {
		const auto count = words[at] >> 16, op = words[at] & 0xffff;
		if (!count || count > words.size() - at)
			return reject("Malformed SPIR-V instruction");
		auto p = words.subspan(at, count);
		if (op == 17 && (count != 2 || p[1] != 1))
			return reject("Vulkan first slice supports Shader capability only");
		if (op == 10)
			return reject("SPIR-V extensions are outside the Vulkan first profile");
		if (op == 15) {
			if (count < 4 || p[1] != stage)
				return reject("Wrong SPIR-V execution model");
			std::string name;
			bool        terminated = false;
			for (std::size_t i = 3; i < p.size() && !terminated; ++i)
				for (unsigned shift = 0; shift < 32; shift += 8) {
					const char c = char((p[i] >> shift) & 255);
					if (!c) {
						terminated = true;
						break;
					}
					name += c;
				}
			if (!terminated || name != entry || ++entries != 1)
				return reject("SPIR-V requires one exact stage entry");
		}
		if ((op == 22 || op == 23 || op == 24 || op == 30 || op == 32) && count >= 2) {
			if (!p[1] || p[1] >= words[3] || types.contains(p[1]))
				return reject("Invalid or duplicate reflected SPIR-V type");
			types.emplace(p[1], std::vector<std::uint32_t>(p.begin(), p.end()));
		}
		if (op == 72 && count >= 4) {
			const auto key = std::pair{p[1], p[2]};
			if (p[3] == 35 && count == 5)
				offsets[key] = p[4];
			if (p[3] == 7 && count == 5)
				strides[key] = p[4];
			if (p[3] == 4 || p[3] == 5)
				columns[key] = p[3] == 5;
		}
		if (op == 71 && count >= 3) {
			if (p[2] == 30 && count == 4)
				locations[p[1]] = p[3];
			if (p[2] == 11 && count == 4)
				builtins[p[1]] = p[3];
			if (p[2] == 2)
				blocks[p[1]] = true;
		}
		if (op == 59) {
			if (count < 4)
				return reject("Malformed SPIR-V variable");
			// No descriptors, storage buffers, workgroup or physical-address resources.
			if (p[3] != 1 && p[3] != 3 && p[3] != 6 && p[3] != 7 && p[3] != 9)
				return reject("SPIR-V descriptor/storage resources are unsupported");
			if (p[3] == 9)
				pushes.push_back(p[1]);
			if (p[3] == 1 && stage == 0)
				inputs.emplace_back(p[2], p[1]);
		}
		at += count;
	}
	if (entries != 1 || pushes.size() > 1)
		return reject("Missing entry or multiple push blocks");
	auto type = [&](std::uint32_t id) -> const std::vector<std::uint32_t> & {
		static const std::vector<std::uint32_t> absent;
		auto                                    it = types.find(id);
		return it == types.end() ? absent : it->second;
	};
	unsigned input_mask = 0;
	for (const auto &[id, pointer_id] : inputs) {
		if (builtins.contains(id)) {
			if (builtins[id] != 42 && builtins[id] != 43)
				return reject("Unsupported vertex built-in");
			continue;
		}
		if (layout != vertex_layout::position_normal || !locations.contains(id) ||
		    locations[id] > 1 || (input_mask & (1u << locations[id])))
			return reject("Unsupported vertex input location");
		input_mask |= 1u << locations[id];
		const auto &pointer = type(pointer_id);
		if (pointer.size() != 4 || (pointer[0] & 0xffff) != 32 || pointer[2] != 1)
			return reject("Invalid vertex input pointer");
		const auto &vector = type(pointer[3]);
		if (vector.size() != 4 || (vector[0] & 0xffff) != 23 || vector[3] != 3)
			return reject("Vertex inputs require float3");
		const auto &scalar = type(vector[2]);
		if (scalar.size() != 3 || (scalar[0] & 0xffff) != 22 || scalar[2] != 32)
			return reject("Vertex inputs require float32");
	}
	if (pushes.empty())
		return false;
	const auto &pointer = type(pushes[0]);
	if (pointer.size() != 4 || (pointer[0] & 0xffff) != 32 || pointer[2] != 9)
		return reject("Invalid push pointer");
	const auto &block = type(pointer[3]);
	if (block.size() != 4 || (block[0] & 0xffff) != 30 || !blocks[pointer[3]])
		return reject("Push block must contain mat4 transform and vec4 color");
	const auto &matrix = type(block[2]);
	const auto &color  = type(block[3]);
	if (matrix.size() != 4 || (matrix[0] & 0xffff) != 24 || matrix[3] != 4 || color.size() != 4 ||
	    (color[0] & 0xffff) != 23 || color[3] != 4 || matrix[2] != block[3])
		return reject("Push block requires float32 mat4 and vec4");
	const auto &scalar = type(color[2]);
	if (scalar.size() != 3 || (scalar[0] & 0xffff) != 22 || scalar[2] != 32 ||
	    !offsets.contains({pointer[3], 0}) || offsets[{pointer[3], 0}] != 0 ||
	    !offsets.contains({pointer[3], 1}) || offsets[{pointer[3], 1}] != 64 ||
	    strides[{pointer[3], 0}] != 16 || !columns[{pointer[3], 0}])
		return reject("Unsupported push block offsets, matrix layout or scalar type");
	return true;
}
inline ice::expected<program, diagnostic>
parse(std::string_view source, std::string_view vertex_entry, std::string_view fragment_entry,
      vertex_layout layout = vertex_layout::position_normal) {
	if (source.size() > 1024 * 1024)
		return reject("Vulkan shader packet exceeds 1MiB");
	auto token = [&]() {
		const auto start = source.find_first_not_of(" \r\n\t");
		if (start == source.npos) {
			source = {};
			return std::string_view{};
		}
		source.remove_prefix(start);
		const auto end    = source.find_first_of(" \r\n\t");
		auto       result = source.substr(0, end);
		source.remove_prefix(result.size());
		return result;
	};
	if (token() != "ice-vulkan-spirv-v1")
		return reject("Expected ice-vulkan-spirv-v1 shader packet");
	program result;
	for (auto stage : {0u, 4u}) {
		if (token() != (stage == 0 ? "vertex" : "fragment"))
			return reject("Missing shader stage");
		const auto  count_text = token();
		std::size_t count      = 0;
		if (count_text.empty())
			return reject("Missing SPIR-V stage word count");
		auto converted =
		    std::from_chars(count_text.data(), count_text.data() + count_text.size(), count);
		if (converted.ec != std::errc{} || converted.ptr != count_text.end() || count < 5 ||
		    count > 32768)
			return reject("Invalid SPIR-V stage word count");
		auto &words = stage == 0 ? result.vertex : result.fragment;
		words.reserve(count);
		for (std::size_t i = 0; i < count; ++i) {
			const auto    hex  = token();
			std::uint32_t word = 0;
			if (hex.size() != 8)
				return reject("Expected eight-digit SPIR-V hex word");
			auto parsed = std::from_chars(hex.data(), hex.data() + hex.size(), word, 16);
			if (parsed.ec != std::errc{} || parsed.ptr != hex.end())
				return reject("Invalid SPIR-V hex word");
			words.push_back(word);
		}
		auto reflected = reflect(words, stage, stage == 0 ? vertex_entry : fragment_entry, layout);
		if (!reflected)
			return ice::unexpected(reflected.error());
		result.pushes = result.pushes || *reflected;
	}
	if (!token().empty())
		return reject("Trailing Vulkan shader packet data");
	return result;
}
inline bool supported(const pipeline_options &p) {
	return (p.vertices == vertex_layout::procedural ||
	        p.vertices == vertex_layout::position_normal) &&
	       p.target_format == color_format::rgba8_unorm && p.sampled_bindings == 0 &&
	       p.depth_sampled_bindings == 0 && (!p.depth_write || p.depth_test);
}
struct extent {
	std::uint32_t width, height;
};
inline extent choose_extent(extent requested, extent current, extent minimum, extent maximum) {
	if (!requested.width || !requested.height)
		return {0, 0};
	if (current.width != UINT32_MAX)
		return current;
	return {std::clamp(requested.width, minimum.width, maximum.width),
	        std::clamp(requested.height, minimum.height, maximum.height)};
}
} // namespace ice::render::vulkan::detail
