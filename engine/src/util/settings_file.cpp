#include "ice/util/settings_file.h"
#include <map>
#include <sstream>
#include <string>
namespace ice::util {
static std::string trim(const std::string &s) {
	auto a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos)
		return {};
	auto b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}
settings_file::settings_file() : num_(0) {}
void        settings_file::clear_values() { num_ = 0; }
ice::result settings_file::set_file(const char *file) {
	if (!file)
		return IC_FAIL_NPE;
	file_ = file;
	return IC_OK;
}
ice::result settings_file::read() { return read_file(file_.c_str()); }
ice::result settings_file::write() { return write_file(file_.c_str()); }
ice::result settings_file::read_file(const char *file) {
	ice::io::file stream;
	if (ICEFAIL(stream.open(file, ICFMREAD_EXISTING)))
		return IC_FAIL_GEN;
	ice::uint64 size;
	if (ICEFAIL(stream.get_size(&size)) || size > 1024 * 1024)
		return IC_FAIL_GEN;
	std::string data((size_t)size, '\0');
	size_t      count;
	if (size && ICEFAIL(stream.read(&data[0], data.size(), &count)))
		return IC_FAIL_GEN;
	clear_values();
	parse_data(size ? &data[0] : nullptr, (int)size);
	return IC_OK;
}
void settings_file::parse_data(char *mem, int size) {
	if (!mem || size <= 0)
		return;
	std::istringstream stream(std::string(mem, size));
	std::string        line, prefix;
	while (std::getline(stream, line)) {
		line = trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';')
			continue;
		if (line.front() == '[' && line.back() == ']') {
			prefix = trim(line.substr(1, line.size() - 2));
			continue;
		}
		auto equals = line.find('=');
		if (equals == std::string::npos)
			continue;
		auto key   = trim(line.substr(0, equals));
		auto value = trim(line.substr(equals + 1));
		if (!prefix.empty())
			key = prefix + "." + key;
		add_pair(key.c_str(), value.c_str());
	}
}
ice::result settings_file::add_pair(const char *key, const char *value) {
	if (!key || !value || strlen(key) >= ICKEYLENGTH || strlen(value) >= ICVALUELENGTH)
		return IC_FAIL_GEN;
	for (int i = 0; i < num_; ++i)
		if (!strcmp(heap_[i].key, key)) {
			strcpy(heap_[i].value, value);
			return IC_OK;
		}
	if (num_ >= IC_SETTINGS_MAX)
		return IC_FAIL_GEN;
	strcpy(heap_[num_].key, key);
	strcpy(heap_[num_++].value, value);
	return IC_OK;
}
std::string settings_file::get_value(const char *key, const char *def_val, bool auto_add) {
	if (!key)
		return std::string();
	for (int i = 0; i < num_; ++i)
		if (!strcmp(heap_[i].key, key))
			return std::string(heap_[i].value);
	if (def_val) {
		if (auto_add)
			add_pair(key, def_val);
		return std::string(def_val);
	}
	return std::string();
}
ice::result settings_file::write_file(const char *file) {
	std::map<std::string, std::map<std::string, std::string>> sections;
	for (int i = 0; i < num_; ++i) {
		std::string key = heap_[i].key;
		auto        dot = key.find_last_of('.');
		sections[dot == std::string::npos ? "" : key.substr(0, dot)]
		        [dot == std::string::npos ? key : key.substr(dot + 1)] = heap_[i].value;
	}
	std::ostringstream output;
	for (auto &section : sections) {
		if (!section.first.empty())
			output << "[" << section.first << "]\n";
		for (auto &pair : section.second)
			output << pair.first << "=" << pair.second << "\n";
		output << "\n";
	}
	std::string   data = output.str();
	ice::io::file stream;
	if (ICEFAIL(stream.open(file, ICFMWRITE_NEW)))
		return IC_FAIL_GEN;
	size_t written;
	return stream.write((void *)data.data(), data.size(), &written);
}

} // namespace ice::util
