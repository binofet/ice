#include "ice/audio/recording_store.h"
#include "ice/core/sha256.h"
#include "ice/core/utf8.h"
#include "ice/data/json.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <aclapi.h>
#include <sddl.h>
#else
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/fs.h>
#include <sys/syscall.h>
#endif
#endif

namespace ice::audio {
namespace {
using value                         = ice::data::value;
constexpr std::uint64_t maximum_wav = 44u + 192000u * 10u * 2u;
[[noreturn]] void       fail(const char *message) { throw std::runtime_error(message); }
bool                    identifier(std::string_view id) {
    if (id.size() < 34 || id.size() > 53 || id[32] != '-')
        return false;
    for (std::size_t i = 0; i < 32; ++i)
        if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f')))
            return false;
    if (id[33] == '0')
        return false;
    std::uint64_t counter = 0;
    auto          parsed = std::from_chars(id.data() + 33, id.data() + id.size(), counter);
    return parsed.ec == std::errc{} && parsed.ptr == id.data() + id.size() && counter;
}
bool owned_name(std::string_view name) {
	return name == "capture.wav" || name == "receipt.json" || name == "commit.pending" ||
	       name == "committed" || name == "ack.pending" || name == "acknowledged" ||
	       name == "remove.pending" || name == "removing";
}
struct file {
	bool marker = false;
#ifdef _WIN32
	HANDLE handle = INVALID_HANDLE_VALUE;
	bool   valid() const { return handle != INVALID_HANDLE_VALUE; }
	void   close() noexcept {
        if (valid())
            CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
	}
#else
	int  handle = -1;
	bool valid() const { return handle >= 0; }
	void close() noexcept {
		if (valid())
			::close(handle);
		handle = -1;
	}
#endif
	file()                        = default;
	file(const file &)            = delete;
	file &operator=(const file &) = delete;
	file(file &&other) noexcept : marker(other.marker), handle(other.handle) {
#ifdef _WIN32
		other.handle = INVALID_HANDLE_VALUE;
#else
		other.handle = -1;
#endif
	}
	file &operator=(file &&other) noexcept {
		if (this != &other) {
			close();
			marker = other.marker;
			handle = std::exchange(other.handle,
#ifdef _WIN32
			                       INVALID_HANDLE_VALUE
#else
			                       -1
#endif
			);
		}
		return *this;
	}
	~file() { close(); }
	std::uint64_t size() const {
#ifdef _WIN32
		BY_HANDLE_FILE_INFORMATION info{};
		if (!GetFileInformationByHandle(handle, &info) ||
		    (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
		    info.nNumberOfLinks != 1)
			fail("Recording file is not a private regular file");
		return (std::uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
#else
		struct stat info {};
		if (fstat(handle, &info) || !S_ISREG(info.st_mode) || info.st_uid != geteuid() ||
		    (info.st_mode & 077) || info.st_nlink > (marker ? 2u : 1u) || info.st_size < 0)
			fail("Recording file is not a private regular file");
		return std::uint64_t(info.st_size);
#endif
	}
	void sync() const {
#ifdef _WIN32
		if (!FlushFileBuffers(handle))
			fail("Recording file flush failed");
#else
		int status;
		do {
			status = fsync(handle);
		} while (status && errno == EINTR);
		if (status)
			fail("Recording file flush failed");
#endif
	}
	void write(std::string_view bytes) const {
		while (!bytes.empty()) {
#ifdef _WIN32
			DWORD count = 0;
			if (!WriteFile(handle, bytes.data(), DWORD(bytes.size()), &count, nullptr) || !count)
#else
			auto count = ::write(handle, bytes.data(), bytes.size());
			if (count < 0 && errno == EINTR)
				continue;
			if (count <= 0)
#endif
				fail("Recording file write failed");
			bytes.remove_prefix(std::size_t(count));
		}
	}
	std::string read(std::uint64_t limit) const {
		const auto length = size();
		if (length > limit)
			fail("Recording file exceeds its format bound");
		std::string result(std::size_t(length), '\0');
		std::size_t offset = 0;
		while (offset < result.size()) {
#ifdef _WIN32
			DWORD count = 0;
			if (!ReadFile(handle, result.data() + offset, DWORD(result.size() - offset), &count,
			              nullptr) ||
			    !count)
#else
			auto count = ::read(handle, result.data() + offset, result.size() - offset);
			if (count < 0 && errno == EINTR)
				continue;
			if (count <= 0)
#endif
				fail("Recording file read failed");
			offset += std::size_t(count);
		}
		return result;
	}
};
#ifdef _WIN32
struct private_security {
	PSECURITY_DESCRIPTOR descriptor = nullptr;
	SECURITY_ATTRIBUTES  attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE};
	private_security() {
		if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
		        L"D:P(A;;GA;;;OW)", SDDL_REVISION_1, &descriptor, nullptr))
			fail("Cannot create recording owner ACL");
		attributes.lpSecurityDescriptor = descriptor;
	}
	~private_security() {
		if (descriptor)
			LocalFree(descriptor);
	}
	private_security(const private_security &)            = delete;
	private_security &operator=(const private_security &) = delete;
};
void check_private(HANDLE handle) {
	PSECURITY_DESCRIPTOR descriptor = nullptr;
	PSID                 owner      = nullptr;
	PACL                 acl        = nullptr;
	if (GetSecurityInfo(handle, SE_FILE_OBJECT,
	                    OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr,
	                    &acl, nullptr, &descriptor) != ERROR_SUCCESS)
		fail("Cannot inspect recording owner ACL");
	file  token;
	bool  okay  = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.handle) != 0;
	DWORD count = 0;
	if (okay)
		GetTokenInformation(token.handle, TokenUser, nullptr, 0, &count);
	std::vector<std::byte> storage(count);
	okay = okay && count &&
	       GetTokenInformation(token.handle, TokenUser, storage.data(), count, &count);
	if (okay)
		okay = EqualSid(owner, reinterpret_cast<TOKEN_USER *>(storage.data())->User.Sid) && acl;
	PSID rights = nullptr;
	okay        = okay && ConvertStringSidToSidW(L"S-1-3-4", &rights);
	if (okay)
		for (DWORD i = 0; i < acl->AceCount; ++i) {
			void *raw = nullptr;
			if (!GetAce(acl, i, &raw) ||
			    static_cast<ACE_HEADER *>(raw)->AceType != ACCESS_ALLOWED_ACE_TYPE) {
				okay = false;
				break;
			}
			auto *ace = static_cast<ACCESS_ALLOWED_ACE *>(raw);
			if (!EqualSid(&ace->SidStart, owner) && !EqualSid(&ace->SidStart, rights)) {
				okay = false;
				break;
			}
		}
	if (rights)
		LocalFree(rights);
	LocalFree(descriptor);
	if (!okay)
		fail("Recording storage must grant access only to its current owner");
}
#endif
struct directory {
	std::filesystem::path path;
	file                  owner;
	void                  verify() const {
#ifdef _WIN32
		BY_HANDLE_FILE_INFORMATION info{};
		if (!GetFileInformationByHandle(owner.handle, &info) ||
		    !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
		    (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
			fail("Invalid recording directory");
		check_private(owner.handle);
#else
		struct stat info {};
		if (fstat(owner.handle, &info) || !S_ISDIR(info.st_mode) || info.st_uid != geteuid() ||
		    (info.st_mode & 077))
			fail("Recording directory must be private to its owner");
#endif
	}
	static directory root(const std::filesystem::path &path) {
		directory result;
		result.path = path;
#ifdef _WIN32
		private_security security;
		if (!CreateDirectoryW(path.c_str(), &security.attributes) &&
		    GetLastError() != ERROR_ALREADY_EXISTS)
			fail("Cannot create recording directory");
		result.owner.handle = CreateFileW(
		    path.c_str(), GENERIC_READ | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
		    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
#else
		if (mkdir(path.c_str(), 0700) && errno != EEXIST)
			fail("Cannot create recording directory");
		result.owner.handle = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
#endif
		if (!result.owner.valid())
			fail("Cannot open recording directory");
		result.verify();
#ifndef _WIN32
		// The first store's directory entry belongs to its trusted parent. Flushing only
		// the new directory would not make that parent entry durable across power loss.
		file       parent;
		const auto parent_path =
		    path.parent_path().empty() ? std::filesystem::path(".") : path.parent_path();
		parent.handle = ::open(parent_path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (!parent.valid())
			fail("Cannot retain recording store parent for durability");
		parent.sync();
#endif
		return result;
	}
	directory child(const std::string &name, bool create = false) const {
		directory result;
		result.path = path / name;
#ifdef _WIN32
		private_security security;
		if (create && !CreateDirectoryW(result.path.c_str(), &security.attributes))
			fail("Recording identity already exists or cannot be created");
		result.owner.handle =
		    CreateFileW(result.path.c_str(), GENERIC_READ | READ_CONTROL,
		                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
		                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
#else
		if (create && mkdirat(owner.handle, name.c_str(), 0700))
			fail("Recording identity already exists or cannot be created");
		result.owner.handle =
		    openat(owner.handle, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
#endif
		if (!result.owner.valid())
			fail("Cannot open recording identity");
		result.verify();
		return result;
	}
	file open_file(const std::string &name, bool create = false) const {
		file result;
		result.marker = name != "capture.wav" && name != "receipt.json";
#ifdef _WIN32
		private_security security;
		result.handle = CreateFileW(
		    (path / name).c_str(), (create ? GENERIC_WRITE : GENERIC_READ) | READ_CONTROL,
		    FILE_SHARE_READ, create ? &security.attributes : nullptr,
		    create ? CREATE_NEW : OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
#else
		result.handle = openat(
		    owner.handle, name.c_str(),
		    O_NOFOLLOW | O_CLOEXEC | (create ? O_WRONLY | O_CREAT | O_EXCL : O_RDONLY), 0600);
#endif
		if (!result.valid())
			fail("Recording file cannot be opened exclusively or read");
		(void)result.size();
#ifdef _WIN32
		check_private(result.handle);
#endif
		return result;
	}
	std::vector<std::string> names(std::size_t maximum) const {
		std::vector<std::string> result;
#ifdef _WIN32
		WIN32_FIND_DATAW item{};
		const HANDLE     search = FindFirstFileW((path / L"*").c_str(), &item);
		if (search == INVALID_HANDLE_VALUE)
			fail("Recording directory scan failed");
		try {
			do {
				const std::filesystem::path name(item.cFileName);
				if (name == L"." || name == L"..")
					continue;
				if (result.size() == maximum)
					fail("Recording directory entry budget exceeded");
				result.push_back(name.string());
			} while (FindNextFileW(search, &item));
			if (GetLastError() != ERROR_NO_MORE_FILES)
				fail("Recording directory scan failed");
		} catch (...) {
			FindClose(search);
			throw;
		}
		FindClose(search);
#else
		// dup shares a directory cursor; reopen '.' to obtain an independent scan each time.
		const int descriptor = openat(owner.handle, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		DIR      *entries    = descriptor < 0 ? nullptr : fdopendir(descriptor);
		if (!entries) {
			if (descriptor >= 0)
				::close(descriptor);
			fail("Recording directory scan failed");
		}
		try {
			for (;;) {
				errno      = 0;
				auto *item = readdir(entries);
				if (!item) {
					if (errno)
						fail("Recording directory scan failed");
					break;
				}
				std::string name(item->d_name);
				if (name == "." || name == "..")
					continue;
				if (result.size() == maximum)
					fail("Recording directory entry budget exceeded");
				result.push_back(std::move(name));
			}
		} catch (...) {
			closedir(entries);
			throw;
		}
		closedir(entries);
#endif
		std::ranges::sort(result);
		return result;
	}
	void sync() const {
#ifndef _WIN32
		owner.sync();
#endif
		// Windows publication uses flushed files and MoveFileEx WRITE_THROUGH. Win32 has
		// no portable directory-fsync equivalent; filesystem power-loss guarantees vary.
	}
	void write_new(const std::string &name, std::string_view bytes) const {
		auto output = open_file(name, true);
		output.write(bytes);
		output.sync();
	}
	void publish(const std::string &pending, const std::string &final) const {
#ifdef _WIN32
		if (!MoveFileExW((path / pending).c_str(), (path / final).c_str(), MOVEFILE_WRITE_THROUGH))
			fail("Recording marker publication failed");
#elif defined(__linux__)
		// Android app SELinux policy may forbid hard links even to our own private files.
		// Use the kernel's atomic exclusive rename directly: libc exposes renameat2 only
		// from Android30, but the supported Android29 kernels provide this syscall.
		// No check-then-rename fallback may weaken the no-replace publication contract.
		if (syscall(SYS_renameat2, owner.handle, pending.c_str(), owner.handle, final.c_str(),
		            RENAME_NOREPLACE) != 0) {
			const int code = errno;
			throw std::runtime_error("Recording atomic marker rename failed; errno=" +
			                         std::to_string(code));
		}
#else
		if (linkat(owner.handle, pending.c_str(), owner.handle, final.c_str(), 0))
			fail("Recording marker publication failed");
		if (unlinkat(owner.handle, pending.c_str(), 0))
			fail("Recording marker staging retirement failed");
#endif
		sync();
	}
	void unlink(const std::string &name, bool folder = false) const {
#ifdef _WIN32
		const auto target = path / name;
		if (!(folder ? RemoveDirectoryW(target.c_str()) : DeleteFileW(target.c_str())))
			fail("Explicit recording removal failed");
#else
		if (unlinkat(owner.handle, name.c_str(), folder ? AT_REMOVEDIR : 0))
			fail("Explicit recording removal failed");
#endif
	}
};
std::uint64_t number(const value::record &record, std::string_view name) {
	auto field = record.find(name);
	if (field == record.end())
		fail("Recording metadata field missing");
	auto *result = std::get_if<std::uint64_t>(&field->second.data);
	if (!result)
		fail("Recording metadata field has wrong type");
	return *result;
}
const std::string &text(const value::record &record, std::string_view name) {
	auto field = record.find(name);
	if (field == record.end())
		fail("Recording metadata field missing");
	auto *result = std::get_if<std::string>(&field->second.data);
	if (!result)
		fail("Recording metadata field has wrong type");
	return *result;
}
std::string   hash(std::string_view bytes) { return ice::core::sha256(bytes); }
std::uint32_t le32(std::string_view bytes, std::size_t offset) {
	std::uint32_t result = 0;
	for (unsigned n = 0; n < 4; ++n)
		result |= std::uint32_t(static_cast<unsigned char>(bytes[offset + n])) << (n * 8);
	return result;
}
recording_receipt validate(const directory &folder, const std::string &id,
                           const std::vector<std::string> &names) {
	auto metadata = folder.open_file("receipt.json").read(8192);
	if (folder.open_file("committed").read(64) != hash(metadata))
		fail("Recording commit marker does not match its metadata");
	auto decoded = ice::data::decode_json(metadata);
	if (!decoded)
		fail("Invalid recording JSON receipt");
	auto *record = std::get_if<value::record>(&decoded->data);
	if (!record || record->size() != 19 || number(*record, "version") != 2 ||
	    text(*record, "recording_id") != id || text(*record, "source") != "aaudio_microphone" ||
	    text(*record, "consent") != "user_push_to_talk_X" ||
	    text(*record, "format") != "pcm_s16le" || number(*record, "channels") != 1 ||
	    text(*record, "clock") != "std::chrono::steady_clock" ||
	    !ice::core::valid_utf8_text(text(*record, "stop_reason"), 128))
		fail("Unsupported recording receipt metadata");
	auto sharing = record->find("automatic_sharing");
	if (sharing == record->end() || !std::holds_alternative<bool>(sharing->second.data) ||
	    std::get<bool>(sharing->second.data))
		fail("Recording receipt cannot imply automatic sharing");
	recording_receipt receipt;
	receipt.recording_id  = id;
	receipt.wav_path      = id + "/capture.wav";
	receipt.metadata_path = id + "/receipt.json";
	if (text(*record, "wav_path") != receipt.wav_path ||
	    text(*record, "metadata_path") != receipt.metadata_path)
		fail("Recording paths do not match their immutable identity");
	const auto rate = number(*record, "sample_rate"), frames = number(*record, "frames"),
	           xruns = number(*record, "xruns");
	if (rate < 8000 || rate > 192000 || !frames || frames > rate * 10 ||
	    xruns > std::uint64_t(std::numeric_limits<std::int32_t>::max()))
		fail("Invalid recording PCM dimensions or xrun metadata");
	receipt.sample_rate          = std::uint32_t(rate);
	receipt.frames               = frames;
	receipt.xruns                = std::int32_t(xruns);
	receipt.dropped_frames       = number(*record, "dropped_frames");
	receipt.started_monotonic_ns = number(*record, "started_monotonic_ns");
	receipt.ended_monotonic_ns   = number(*record, "ended_monotonic_ns");
	if (receipt.ended_monotonic_ns < receipt.started_monotonic_ns)
		fail("Recording clock interval is reversed");
	auto wav = folder.open_file("capture.wav").read(maximum_wav);
	if (wav.size() != 44 + frames * 2 || number(*record, "wav_bytes") != wav.size() ||
	    wav.substr(0, 4) != "RIFF" || le32(wav, 4) != wav.size() - 8 ||
	    wav.substr(8, 8) != "WAVEfmt " || le32(wav, 16) != 16 || le32(wav, 20) != 0x10001 ||
	    le32(wav, 24) != rate || le32(wav, 28) != rate * 2 || le32(wav, 32) != 0x100002 ||
	    wav.substr(36, 4) != "data" || le32(wav, 40) != frames * 2)
		fail("Recording WAV does not match PCM receipt metadata");
	receipt.wav_sha256 = hash(wav);
	if (receipt.wav_sha256 != text(*record, "wav_sha256"))
		fail("Recording WAV content hash mismatch");
	receipt.acknowledged = std::ranges::find(names, "acknowledged") != names.end();
	if (receipt.acknowledged && folder.open_file("acknowledged").read(64) != hash(metadata))
		fail("Recording acknowledgment does not match its receipt");
	return receipt;
}
std::string random_session() {
	std::random_device random;
	std::string        result;
	constexpr char     hex[] = "0123456789abcdef";
	for (unsigned n = 0; n < 4; ++n) {
		auto word = std::uint32_t(random());
		for (int shift = 28; shift >= 0; shift -= 4)
			result.push_back(hex[(word >> shift) & 15]);
	}
	return result;
}
} // namespace
struct recording_store::state {
	directory             root;
	file                  lock;
	recording_limits      limits;
	std::string           session;
	std::uint64_t         counter = 0;
	std::set<std::string> reservations;
	recording_inventory   scan(bool contents) const {
        recording_inventory result;
        auto                names = root.names(1000001);
        for (const auto &id : names) {
            if (id == ".writer.lock")
                continue;
            if (!identifier(id))
                fail("Unexpected entry in recording store");
            auto folder = root.child(id);
            auto files  = folder.names(8);
            for (const auto &name : files) {
                if (!owned_name(name))
                    fail("Unexpected file in recording identity");
                const auto bytes = folder.open_file(name).size();
                if (bytes > maximum_wav)
                    fail("Recording byte quota exceeded");
                result.bytes_used += bytes;
            }
            const bool removing = std::ranges::find(files, "removing") != files.end();
            if (removing && folder.open_file("removing").read(32) != "explicit removal\n")
                fail("Invalid explicit recording removal marker");
            if (std::ranges::find(files, "committed") != files.end() && !removing) {
                if (contents)
                    result.recordings.push_back(validate(folder, id, files));
                else
                    result.recordings.emplace_back();
            } else
                result.incomplete_ids.push_back(id);
        }
        return result;
	}
};
recording_store::recording_store(std::unique_ptr<state> state) : state_(std::move(state)) {}
recording_store::~recording_store()                                           = default;
recording_store::recording_store(recording_store &&other) noexcept            = default;
recording_store &recording_store::operator=(recording_store &&other) noexcept = default;
ice::expected<recording_store, diagnostic> recording_store::open(const std::filesystem::path &path,
                                                                 recording_limits limits) {
	try {
		if (!limits.max_recordings || limits.max_recordings > 1000000 || !limits.max_bytes ||
		    limits.max_bytes > (1ull << 40))
			fail("Invalid recording storage quota");
		auto state    = std::make_unique<recording_store::state>();
		state->root   = directory::root(path);
		state->limits = limits;
#ifdef _WIN32
		private_security security;
		state->lock.handle =
		    CreateFileW((path / L".writer.lock").c_str(),
		                GENERIC_READ | GENERIC_WRITE | READ_CONTROL, FILE_SHARE_READ,
		                &security.attributes, OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (!state->lock.valid())
			fail("Recording store already has a writer or lock is unavailable");
		check_private(state->lock.handle);
#else
		state->lock.handle = openat(state->root.owner.handle, ".writer.lock",
		                            O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
		if (!state->lock.valid() || flock(state->lock.handle, LOCK_EX | LOCK_NB))
			fail("Recording store already has a writer or lock is unavailable");
#endif
		if (state->lock.size() != 0)
			fail("Recording writer lock contains foreign data");
		(void)state->scan(true);
		state->session = random_session();
		return recording_store(std::move(state));
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
}
ice::expected<std::string, diagnostic> recording_store::reserve_id() {
	try {
		if (!state_ || state_->counter == std::numeric_limits<std::uint64_t>::max())
			fail("Recording owner is unavailable or identity counter exhausted");
		auto inventory = state_->scan(false);
		if (inventory.recordings.size() + inventory.incomplete_ids.size() >=
		        state_->limits.max_recordings ||
		    inventory.bytes_used >= state_->limits.max_bytes)
			fail("Recording quota exhausted; explicitly acknowledge/remove clips before another "
			     "take");
		auto id = state_->session + "-" + std::to_string(++state_->counter);
		state_->reservations.insert(id);
		try {
			(void)state_->root.child(id, true);
			state_->root.sync();
		} catch (...) {
			state_->reservations.erase(id);
			throw;
		}
		return id;
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
}
ice::expected<void, diagnostic> recording_store::cancel_reservation(const std::string &id) {
	try {
		if (!state_ || !identifier(id) || !state_->reservations.contains(id))
			fail("Recording reservation is not owned by this writer");
		{
			auto folder = state_->root.child(id);
			if (!folder.names(1).empty())
				fail("Cannot cancel nonempty recording; explicit removal is required");
		}
		state_->root.unlink(id, true);
		state_->root.sync();
		state_->reservations.erase(id);
		return {};
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
}
ice::expected<recording_receipt, diagnostic>
recording_store::commit(const std::string &id, const capture_buffer &capture,
                        const recording_timing &timing) {
	try {
		if (!state_ || !identifier(id) || !state_->reservations.contains(id))
			fail("Recording commit requires this writer's reserved identity");
		if (capture.samples().empty() || timing.ended_monotonic_ns < timing.started_monotonic_ns ||
		    timing.xruns < 0 || !ice::core::valid_utf8_text(timing.stop_reason, 128))
			fail("Invalid recording samples or timing metadata");
		auto encoded = encode_wav(capture);
		if (!encoded)
			return ice::unexpected(encoded.error());
		std::string_view  wav(reinterpret_cast<const char *>(encoded->data()), encoded->size());
		const auto        digest = hash(wav);
		recording_receipt receipt{id,
		                          id + "/capture.wav",
		                          id + "/receipt.json",
		                          digest,
		                          capture.sample_rate(),
		                          capture.samples().size(),
		                          capture.dropped_frames(),
		                          timing.started_monotonic_ns,
		                          timing.ended_monotonic_ns,
		                          timing.xruns,
		                          false};
		value::record     record{{"version", value{std::uint64_t(2)}},
		                         {"recording_id", value{id}},
		                         {"source", value{std::string("aaudio_microphone")}},
		                         {"consent", value{std::string("user_push_to_talk_X")}},
		                         {"automatic_sharing", value{false}},
		                         {"format", value{std::string("pcm_s16le")}},
		                         {"channels", value{std::uint64_t(1)}},
		                         {"sample_rate", value{std::uint64_t(receipt.sample_rate)}},
		                         {"frames", value{receipt.frames}},
		                         {"dropped_frames", value{receipt.dropped_frames}},
		                         {"xruns", value{std::uint64_t(receipt.xruns)}},
		                         {"started_monotonic_ns", value{receipt.started_monotonic_ns}},
		                         {"ended_monotonic_ns", value{receipt.ended_monotonic_ns}},
		                         {"clock", value{std::string("std::chrono::steady_clock")}},
		                         {"stop_reason", value{timing.stop_reason}},
		                         {"wav_path", value{receipt.wav_path}},
		                         {"metadata_path", value{receipt.metadata_path}},
		                         {"wav_sha256", value{digest}},
		                         {"wav_bytes", value{std::uint64_t(wav.size())}}};
		auto              metadata = ice::data::encode_json(value{std::move(record)});
		if (!metadata)
			fail("Recording JSON encoding failed");
		auto       inventory = state_->scan(false);
		const auto needed    = std::uint64_t(wav.size() + metadata->size() + 64);
		if (inventory.bytes_used >= state_->limits.max_bytes ||
		    needed > state_->limits.max_bytes - inventory.bytes_used)
			fail("Recording byte quota exhausted; take not committed");
		auto folder = state_->root.child(id);
		if (!folder.names(1).empty())
			fail("Recording identity already contains data; it cannot be overwritten");
		folder.write_new("capture.wav", wav);
		folder.write_new("receipt.json", *metadata);
		folder.sync();
		folder.write_new("commit.pending", hash(*metadata));
		folder.publish("commit.pending", "committed");
		state_->reservations.erase(id);
		return receipt;
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
}
ice::expected<recording_inventory, diagnostic> recording_store::list() const {
	try {
		if (!state_)
			fail("Recording owner is unavailable");
		return state_->scan(true);
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
}
ice::expected<void, diagnostic> recording_store::acknowledge(const std::string &id) {
	try {
		if (!state_ || !identifier(id))
			fail("Invalid recording identity");
		auto folder = state_->root.child(id);
		auto names  = folder.names(8);
		if (std::ranges::find(names, "removing") != names.end())
			fail("Recording is being explicitly removed");
		auto receipt = validate(folder, id, names);
		if (receipt.acknowledged) {
			folder.sync(); // Reestablish a possibly interrupted prior publication barrier.
			return {};
		}
		const auto metadata = folder.open_file("receipt.json").read(8192);
		const bool pending  = std::ranges::find(names, "ack.pending") != names.end();
		if (pending) {
			if (folder.open_file("ack.pending").read(64) != hash(metadata))
				fail("Interrupted acknowledgment does not match receipt");
		} else {
			auto inventory = state_->scan(false);
			if (inventory.bytes_used >= state_->limits.max_bytes ||
			    64 > state_->limits.max_bytes - inventory.bytes_used)
				fail("Recording acknowledgment exceeds byte quota");
			folder.write_new("ack.pending", hash(metadata));
		}
		folder.publish("ack.pending", "acknowledged");
		return {};
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
}
ice::expected<void, diagnostic> recording_store::remove(const std::string &id) {
	try {
		if (!state_ || !identifier(id))
			fail("Invalid recording identity");
		{
			auto folder = state_->root.child(id);
			auto names  = folder.names(8);
			for (const auto &name : names) {
				if (!owned_name(name))
					fail("Unexpected file prevents explicit recording removal");
				(void)folder.open_file(name).size();
			}
			if (std::ranges::find(names, "removing") == names.end()) {
				if (std::ranges::find(names, "remove.pending") != names.end())
					folder.unlink("remove.pending");
				folder.write_new("remove.pending", "explicit removal\n");
				folder.publish("remove.pending", "removing");
			}
			if (folder.open_file("removing").read(32) != "explicit removal\n")
				fail("Invalid explicit recording removal marker");
			// A visible retry marker may precede the crashed writer's directory fsync.
			// Reestablish that barrier before the first destructive unlink.
			folder.sync();
			for (const auto &name : folder.names(8))
				if (name != "removing")
					folder.unlink(name);
			folder.sync();
			folder.unlink("removing");
			folder.sync();
		}
		state_->root.unlink(id, true);
		state_->root.sync();
		state_->reservations.erase(id);
		return {};
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
}
} // namespace ice::audio
