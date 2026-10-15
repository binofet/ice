#include "ice/data/json.h"
#include "ice/core/utf8.h"
#include <charconv>
#include <cmath>
#include <type_traits>

namespace ice::data {
namespace {
constexpr std::size_t max_bytes = 1024 * 1024;
struct json_error {
	diagnostic problem;
};
struct reader {
	std::string_view  input;
	std::size_t       position = 0, nodes = 0;
	[[noreturn]] void fail(const char *message) const {
		throw json_error{{message, "byte " + std::to_string(position)}};
	}
	void whitespace() {
		while (position < input.size() && (input[position] == ' ' || input[position] == '\n' ||
		                                   input[position] == '\r' || input[position] == '\t'))
			++position;
	}
	bool take(char character) {
		if (position < input.size() && input[position] == character) {
			++position;
			return true;
		}
		return false;
	}
	unsigned hex4() {
		unsigned point = 0;
		for (unsigned i = 0; i < 4; ++i) {
			if (position == input.size())
				fail("Truncated Unicode escape");
			char     c     = input[position++];
			unsigned digit = c >= '0' && c <= '9'   ? unsigned(c - '0')
			                 : c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10)
			                 : c >= 'A' && c <= 'F' ? unsigned(c - 'A' + 10)
			                                        : 16;
			if (digit == 16)
				fail("Invalid Unicode escape");
			point = point * 16 + digit;
		}
		return point;
	}
	std::string string() {
		if (!take('"'))
			fail("Expected JSON string");
		std::string result;
		while (position < input.size()) {
			unsigned char c = static_cast<unsigned char>(input[position++]);
			if (c == '"')
				return result;
			if (c < 32)
				fail("Unescaped control character");
			if (c != '\\')
				result += char(c);
			else {
				if (position == input.size())
					fail("Truncated string escape");
				switch (input[position++]) {
				case '"':
					result += '"';
					break;
				case '\\':
					result += '\\';
					break;
				case '/':
					result += '/';
					break;
				case 'b':
					result += '\b';
					break;
				case 'f':
					result += '\f';
					break;
				case 'n':
					result += '\n';
					break;
				case 'r':
					result += '\r';
					break;
				case 't':
					result += '\t';
					break;
				case 'u': {
					unsigned point = hex4();
					if (point >= 0xD800 && point <= 0xDBFF) {
						if (!take('\\') || !take('u'))
							fail("Missing low Unicode surrogate");
						auto low = hex4();
						if (low < 0xDC00 || low > 0xDFFF)
							fail("Invalid low Unicode surrogate");
						point = 0x10000 + ((point - 0xD800) << 10) + low - 0xDC00;
					} else if (point >= 0xDC00 && point <= 0xDFFF)
						fail("Unpaired low Unicode surrogate");
					if (!point)
						fail("NUL is not portable text");
					if (point < 0x80)
						result += char(point);
					else {
						if (point >= 0x10000)
							result += char(0xF0 | (point >> 18));
						if (point >= 0x800)
							result +=
							    char((point >= 0x10000 ? 0x80 : 0xE0) | ((point >> 12) & 0x3F));
						result += char((point >= 0x800 ? 0x80 : 0xC0) | ((point >> 6) & 0x3F));
						result += char(0x80 | (point & 0x3F));
					}
					break;
				}
				default:
					fail("Unknown string escape");
				}
			}
			if (result.size() > 4096)
				fail("JSON string exceeds 4096 bytes");
		}
		fail("Unterminated JSON string");
	}
	value number() {
		auto begin    = position;
		bool negative = take('-');
		auto digit    = [&] {
            return position < input.size() && input[position] >= '0' && input[position] <= '9';
		};
		if (!take('0')) {
			if (!digit())
				fail("Expected number");
			while (digit())
				++position;
		}
		bool real = take('.');
		if (real) {
			if (!digit())
				fail("Missing fractional digits");
			while (digit())
				++position;
		}
		if (take('e') || take('E')) {
			real = true;
			if (!take('+'))
				take('-');
			if (!digit())
				fail("Missing exponent digits");
			while (digit())
				++position;
		}
		auto token   = input.substr(begin, position - begin);
		auto convert = [&](auto &destination) {
			auto result = std::from_chars(token.data(), token.data() + token.size(), destination);
			if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
				fail("JSON number outside its portable range");
		};
		if (real) {
			double result = 0;
			convert(result);
			if (!std::isfinite(result))
				fail("Nonfinite JSON number");
			return value{result};
		}
		if (negative) {
			std::int64_t result = 0;
			convert(result);
			return value{result};
		}
		std::uint64_t result = 0;
		convert(result);
		return value{result};
	}
	value read(unsigned depth = 0) {
		if (depth > 48 || ++nodes > 65536)
			fail("JSON depth/node budget exceeded");
		whitespace();
		if (take('[')) {
			value::sequence result;
			whitespace();
			if (take(']'))
				return value{std::move(result)};
			do {
				if (result.size() == 4096)
					fail("JSON container exceeds 4096 items");
				result.push_back(read(depth + 1));
				whitespace();
				if (take(']'))
					return value{std::move(result)};
			} while (take(','));
			fail("Expected comma or closing array bracket");
		}
		if (take('{')) {
			value::record result;
			whitespace();
			if (take('}'))
				return value{std::move(result)};
			do {
				if (result.size() == 4096 || ++nodes > 65536)
					fail("JSON object/node budget exceeded");
				whitespace();
				auto name = string();
				whitespace();
				if (!take(':'))
					fail("Expected colon after object key");
				if (!result.emplace(std::move(name), read(depth + 1)).second)
					fail("Duplicate JSON object key");
				whitespace();
				if (take('}'))
					return value{std::move(result)};
			} while (take(','));
			fail("Expected comma or closing object brace");
		}
		if (position < input.size() && input[position] == '"')
			return value{string()};
		for (auto keyword :
		     {std::string_view("null"), std::string_view("true"), std::string_view("false")}) {
			if (input.substr(position).starts_with(keyword)) {
				position += keyword.size();
				if (keyword == "null")
					return {};
				return value{keyword == "true"};
			}
		}
		return number();
	}
};
struct writer {
	std::string output;
	void        append(std::string_view text) {
        if (text.size() > max_bytes - output.size())
            throw json_error{{"Encoded JSON exceeds one MiB", {}}};
        output += text;
	}
	void string(std::string_view text) {
		append("\"");
		for (unsigned char c : text) {
			if (c == '"')
				append("\\\"");
			else if (c == '\\')
				append("\\\\");
			else if (c < 32) {
				constexpr char digits[]  = "0123456789abcdef";
				char           escaped[] = {'\\', 'u', '0', '0', digits[c >> 4], digits[c & 15]};
				append(std::string_view(escaped, 6));
			} else {
				char character = char(c);
				append(std::string_view(&character, 1));
			}
		}
		append("\"");
	}
	void write(const value &input) {
		std::visit(
		    [&](const auto &item) {
			    using type = std::decay_t<decltype(item)>;
			    if constexpr (std::is_same_v<type, std::monostate>)
				    append("null");
			    else if constexpr (std::is_same_v<type, bool>)
				    append(item ? "true" : "false");
			    else if constexpr (std::is_arithmetic_v<type>) {
				    char buffer[64];
				    auto result = std::to_chars(buffer, buffer + sizeof(buffer), item);
				    if (result.ec != std::errc{})
					    throw json_error{{"Cannot encode JSON number", {}}};
				    std::string_view token(buffer, std::size_t(result.ptr - buffer));
				    append(token);
				    // Keep floating-point representation, including -0.0, on a JSON round trip.
				    if constexpr (std::is_same_v<type, double>)
					    if (token.find_first_of(".eE") == token.npos)
						    append(".0");
			    } else if constexpr (std::is_same_v<type, std::string>)
				    string(item);
			    else if constexpr (std::is_same_v<type, value::sequence>) {
				    append("[");
				    bool first = true;
				    for (const auto &element : item) {
					    if (!first)
						    append(",");
					    first = false;
					    write(element);
				    }
				    append("]");
			    } else if constexpr (std::is_same_v<type, value::record>) {
				    append("{");
				    bool first = true;
				    for (const auto &[name, element] : item) {
					    if (!first)
						    append(",");
					    first = false;
					    string(name);
					    append(":");
					    write(element);
				    }
				    append("}");
			    }
		    },
		    input.data);
	}
};
} // namespace

ice::expected<value, diagnostic> decode_json(std::string_view input) {
	if (!ice::core::valid_utf8_text(input, max_bytes))
		return ice::unexpected(diagnostic{"Invalid UTF-8/NUL or JSON exceeds one MiB", {}});
	try {
		reader parser{input};
		auto   result = parser.read();
		parser.whitespace();
		if (parser.position != input.size())
			parser.fail("Trailing JSON input");
		auto checked = validate_tree(result);
		if (!checked)
			return ice::unexpected(checked.error());
		return result;
	} catch (const json_error &error) {
		return ice::unexpected(error.problem);
	}
}
ice::expected<std::string, diagnostic> encode_json(const value &input) {
	auto checked = validate_tree(input);
	if (!checked)
		return ice::unexpected(checked.error());
	try {
		writer encoder;
		encoder.write(input);
		return std::move(encoder.output);
	} catch (const json_error &error) {
		return ice::unexpected(error.problem);
	}
}
} // namespace ice::data
