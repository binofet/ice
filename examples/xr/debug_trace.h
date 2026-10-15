#pragma once

#include <array>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>

namespace ice::examples::xr {

// Two app-private 32 KiB files retain diagnostics across debugger/radio disconnects.
// Only explicit probe records are written; there is no system-log or identifier capture.
class debug_trace {
public:
	~debug_trace() {
		if (file_)
			std::fclose(file_);
	}
	debug_trace()                               = default;
	debug_trace(const debug_trace &)            = delete;
	debug_trace &operator=(const debug_trace &) = delete;
	bool         initialize(const std::string &directory) {
        std::lock_guard lock(mutex_);
        if (file_)
            std::fclose(file_);
        path_ = directory + "/ice-xr-trace.log";
        file_ = std::fopen(path_.c_str(), "ab+");
        if (!file_)
            return false;
        if (std::fseek(file_, 0, SEEK_END) != 0) {
            std::fclose(file_);
            file_ = nullptr;
            return false;
        }
        const auto end = std::ftell(file_);
        if (end < 0) {
            std::fclose(file_);
            file_ = nullptr;
            return false;
        }
        bytes_ = static_cast<std::size_t>(end);
        return true;
	}
	bool write(char level, const char *message) {
		std::lock_guard lock(mutex_);
		if (!file_)
			return false;
		const auto seconds =
		    std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
		        .count();
		std::array<char, 1152> record{};
		const auto formatted = std::snprintf(record.data(), record.size(), "%.6f %c %.1023s\n",
		                                     seconds, level, message);
		if (formatted < 0 || static_cast<std::size_t>(formatted) >= record.size())
			return false;
		const auto size = static_cast<std::size_t>(formatted);
		if (bytes_ + size > file_budget_) {
			std::fclose(file_);
			const auto previous = path_ + ".previous";
			std::remove(previous.c_str());
			std::rename(path_.c_str(), previous.c_str());
			file_  = std::fopen(path_.c_str(), "wb");
			bytes_ = 0;
			if (!file_)
				return false;
		}
		const auto written = std::fwrite(record.data(), 1, size, file_);
		bytes_ += written;
		const bool flushed = std::fflush(file_) == 0;
		return written == size && flushed;
	}

private:
	static constexpr std::size_t file_budget_ = 32 * 1024;
	std::mutex                   mutex_;
	std::string                  path_;
	std::FILE                   *file_  = nullptr;
	std::size_t                  bytes_ = 0;
};

} // namespace ice::examples::xr
