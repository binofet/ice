#pragma once
#include "ice/core/expected.h"
#include "playground/custom_vector.h"
#include <bit>
#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace metagen_examples {
template <typename T> using result = ice::expected<T, std::string>;
template <typename T> struct vector_adapter {
	using value_type     = T;
	using container_type = std::vector<T>;
	using builder_type   = container_type;
	static std::size_t          size(const container_type &values) { return values.size(); }
	static result<builder_type> make(std::size_t count) {
		builder_type builder;
		builder.reserve(count);
		return builder;
	}
	static result<void> append(builder_type &builder, value_type value) {
		builder.push_back(std::move(value));
		return {};
	}
	static result<container_type> finish(builder_type builder) { return builder; }
	template <typename Visitor>
	static result<void> visit(const container_type &values, Visitor visitor) {
		for (const auto &value : values) {
			auto checked = visitor(value);
			if (!checked)
				return checked;
		}
		return {};
	}
};

// Contracts mention visitation/building, rather than requiring random access or native iterators.
template <typename Adapter>
concept sequence_adapter =
    requires(typename Adapter::container_type const &values,
             typename Adapter::builder_type &builder, typename Adapter::value_type value) {
	    { Adapter::size(values) } -> std::same_as<std::size_t>;
	    { Adapter::make(std::size_t{}) } -> std::same_as<result<typename Adapter::builder_type>>;
	    { Adapter::append(builder, std::move(value)) } -> std::same_as<result<void>>;
	    {
		    Adapter::finish(std::move(builder))
	    } -> std::same_as<result<typename Adapter::container_type>>;
	    {
		    Adapter::visit(values,
		                   [](const typename Adapter::value_type &) -> result<void> { return {}; })
	    } -> std::same_as<result<void>>;
    };

// Tiny demonstration encoding, not the portable graph format or a shipping ABI.
struct reader {
	std::span<const std::uint8_t> bytes;
	std::size_t                   position = 0;
	std::size_t                   items    = 65536;
	result<std::uint32_t>         u32() {
        if (bytes.size() - position < 4 || items == 0)
            return ice::unexpected(std::string("Truncated input/item budget"));
        --items;
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(bytes[position++]) << (i * 8);
        return value;
	}
};
inline void put(std::vector<std::uint8_t> &output, std::uint32_t value) {
	for (unsigned i = 0; i < 4; ++i)
		output.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 255));
}
struct int32_codec {
	using native_type = std::int32_t;
	static result<void> write(native_type value, std::vector<std::uint8_t> &output) {
		if (output.size() > 1024 * 1024 - 4)
			return ice::unexpected(std::string("Byte budget"));
		put(output, std::bit_cast<std::uint32_t>(value));
		return {};
	}
	static result<native_type> read(reader &source) {
		auto value = source.u32();
		if (!value)
			return ice::unexpected(value.error());
		return std::bit_cast<std::int32_t>(*value);
	}
};
template <sequence_adapter Adapter, typename ElementCodec> struct sequence_codec {
	using native_type = typename Adapter::container_type;
	static_assert(std::same_as<typename Adapter::value_type, typename ElementCodec::native_type>);
	static result<void> write(const native_type &values, std::vector<std::uint8_t> &output) {
		if (Adapter::size(values) > 4096 || output.size() > 1024 * 1024 - 4)
			return ice::unexpected(std::string("Container/byte budget"));
		put(output, static_cast<std::uint32_t>(Adapter::size(values)));
		return Adapter::visit(
		    values, [&](const auto &value) { return ElementCodec::write(value, output); });
	}
	static result<native_type> read(reader &source) {
		auto count = source.u32();
		if (!count || *count > 4096)
			return ice::unexpected(std::string("Container length/truncation"));
		auto builder = Adapter::make(*count);
		if (!builder)
			return ice::unexpected(builder.error());
		for (std::uint32_t i = 0; i < *count; ++i) {
			auto value = ElementCodec::read(source);
			if (!value)
				return ice::unexpected(value.error());
			auto appended = Adapter::append(*builder, std::move(*value));
			if (!appended)
				return ice::unexpected(appended.error());
		}
		return Adapter::finish(std::move(*builder));
	}
};
} // namespace metagen_examples

namespace playground {
template <typename T> struct vector_adapter {
	using value_type     = T;
	using container_type = custom_vector<T>;
	using builder_type   = container_type;
	static std::size_t size(const container_type &values) { return values.count(); }
	static metagen_examples::result<builder_type> make(std::size_t) { return builder_type{}; }
	static metagen_examples::result<void>         append(builder_type &builder, T value) {
        builder.add(std::move(value));
        return {};
	}
	static metagen_examples::result<container_type> finish(builder_type builder) { return builder; }
	template <typename Visitor>
	static metagen_examples::result<void> visit(const container_type &values, Visitor visitor) {
		return values.each(std::move(visitor));
	}
};
} // namespace playground
