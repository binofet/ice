#include "ice/audio/recording_store.h"
#include "ice/core/sha256.h"
#include "ice/data/json.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <sddl.h>
#else
#include <sys/stat.h>
#endif
#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Recording check %d: %s\n", __LINE__, #condition);                \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)
namespace {
namespace fs = std::filesystem;
struct cleanup {
	fs::path path;
	~cleanup() {
		std::error_code ignored;
		fs::remove_all(path, ignored);
	}
};
std::string read(const fs::path &path) {
	std::ifstream input(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void write(const fs::path &path, const std::string &bytes) {
#ifdef _WIN32
	PSECURITY_DESCRIPTOR descriptor = nullptr;
	if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;GA;;;OW)", SDDL_REVISION_1,
	                                                          &descriptor, nullptr))
		throw std::runtime_error("Test ACL creation failed");
	SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
	HANDLE output = CreateFileW(path.c_str(), GENERIC_WRITE, 0, &security, CREATE_ALWAYS,
	                            FILE_ATTRIBUTE_NORMAL, nullptr);
	LocalFree(descriptor);
	if (output == INVALID_HANDLE_VALUE)
		throw std::runtime_error("Test file creation failed");
	DWORD      count = 0;
	const bool okay  = WriteFile(output, bytes.data(), DWORD(bytes.size()), &count, nullptr) &&
	                  count == bytes.size();
	CloseHandle(output);
	if (!okay)
		throw std::runtime_error("Test file write failed");
#else
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(bytes.data(), std::streamsize(bytes.size()));
	output.close();
	chmod(path.c_str(), 0600);
#endif
}
} // namespace
int main() {
	using namespace ice::audio;
	const auto nonce  = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto parent = fs::temp_directory_path() / ("ice-recordings-" + std::to_string(nonce));
	CHECK(fs::create_directory(parent));
	cleanup    remove_parent{parent};
	const auto path    = parent / "recordings";
	auto       capture = capture_buffer::create(48000, 10);
	CHECK(capture);
	const std::array<std::int16_t, 8> pcm{-32768, 32767, -123, 42, 0, 1, -1, 8192};
	CHECK(capture->append(pcm) == pcm.size());
	const recording_timing timing{123000000, 124000000, 2, "release"};
	std::string            first, wav, metadata;
	{
		auto store = recording_store::open(path, {1024 * 1024, 2});
		CHECK(store);
		CHECK(!recording_store::open(path)); // Retained single-writer lock.
		auto id = store->reserve_id();
		CHECK(id);
		first        = *id;
		auto receipt = store->commit(first, *capture, timing);
		CHECK(receipt);
		CHECK(!fs::exists(path / first / "commit.pending"));
		CHECK(fs::hard_link_count(path / first / "committed") == 1);
		CHECK(receipt->recording_id == first && receipt->sample_rate == 48000 &&
		      receipt->frames == 8);
		CHECK(receipt->started_monotonic_ns == timing.started_monotonic_ns &&
		      receipt->ended_monotonic_ns == timing.ended_monotonic_ns && receipt->xruns == 2);
		CHECK(!receipt->acknowledged && receipt->wav_path == first + "/capture.wav");
		wav      = read(path / receipt->wav_path);
		metadata = read(path / receipt->metadata_path);
		CHECK(wav.size() == 60 && receipt->wav_sha256 == ice::core::sha256(wav));
		auto decoded = ice::data::decode_json(metadata);
		CHECK(decoded);
		const auto &record = std::get<ice::data::value::record>(decoded->data);
		CHECK(std::get<std::string>(record.at("source").data) == "aaudio_microphone" &&
		      std::get<std::string>(record.at("consent").data) == "user_push_to_talk_X" &&
		      !std::get<bool>(record.at("automatic_sharing").data));
		CHECK(!store->commit(first, *capture, timing) && read(path / receipt->wav_path) == wav);
		auto second = store->reserve_id();
		CHECK(second && *second != first);
		CHECK(!store->reserve_id()); // Count quota includes uncommitted reservations.
		CHECK(store->cancel_reservation(*second));
		auto third = store->reserve_id();
		CHECK(third && *third != *second);
		CHECK(!store->cancel_reservation(first));
		CHECK(!store->commit(*third, *capture, {5, 4, 0, "reversed"}));
		CHECK(!store->commit(*third, *capture, {1, 2, -1, "invalid"}));
		CHECK(!store->commit("../../foreign", *capture, timing));
		// A failed exclusive write cannot overwrite preexisting bytes or publish a receipt.
		write(path / *third / "capture.wav", "occupied");
		CHECK(!store->commit(*third, *capture, timing));
		CHECK(read(path / *third / "capture.wav") == "occupied");
		auto listed = store->list();
		CHECK(listed && listed->recordings.size() == 1 &&
		      listed->incomplete_ids == std::vector<std::string>{*third});
		CHECK(store->remove(*third));
		// Simulate a crash after WAV/metadata writes, before publication. No implicit deletion.
		auto interrupted = store->reserve_id();
		CHECK(interrupted);
		write(path / *interrupted / "capture.wav", wav);
		write(path / *interrupted / "receipt.json", metadata);
#ifndef _WIN32
		struct stat info {};
		CHECK(stat((path / first / "capture.wav").c_str(), &info) == 0 &&
		      (info.st_mode & 077) == 0);
#endif
	}
	{
		auto store = recording_store::open(path);
		CHECK(store);
		auto listed = store->list();
		CHECK(listed && listed->recordings.size() == 1 && listed->incomplete_ids.size() == 1);
		CHECK(read(path / first / "capture.wav") == wav &&
		      read(path / first / "receipt.json") == metadata);
		CHECK(store->remove(listed->incomplete_ids.front()));
		// A pending marker from a crash is not mistaken for an uncommitted clip or overwritten.
#ifndef _WIN32
		fs::create_hard_link(path / first / "committed", path / first / "commit.pending");
		CHECK(store->list() && store->list()->recordings.size() == 1);
#endif
		write(path / first / "ack.pending", ice::core::sha256(metadata));
		CHECK(store->acknowledge(first) && store->acknowledge(first));
		listed = store->list();
		CHECK(listed && listed->recordings.front().acknowledged);
		CHECK(fs::exists(path / first / "capture.wav")); // Ack is not deletion or sharing.
		// Alter only PCM data: an unchanged receipt cannot authenticate changed bytes.
		auto damaged = wav;
		damaged.back() ^= 1;
		write(path / first / "capture.wav", damaged);
		CHECK(!store->list());
		write(path / first / "capture.wav", wav);
		CHECK(store->list());
		// Recompute the marker after changing metadata, so WAV-shape validation is independent
		// of the JSON digest check rather than masked by it.
		auto decoded = ice::data::decode_json(metadata);
		CHECK(decoded);
		std::get<ice::data::value::record>(decoded->data).at("sample_rate") =
		    ice::data::value{std::uint64_t(96000)};
		auto changed = ice::data::encode_json(*decoded);
		CHECK(changed);
		write(path / first / "receipt.json", *changed);
		write(path / first / "committed", ice::core::sha256(*changed));
		CHECK(!store->list());
		write(path / first / "receipt.json", metadata);
		write(path / first / "committed", ice::core::sha256(metadata));
		CHECK(store->list());
		// Removal intent is durable first. Restart/discovery must never resurrect the clip.
		write(path / first / "removing", "not a removal receipt");
		CHECK(!store->list());
		write(path / first / "removing", "explicit removal\n");
		listed = store->list();
		CHECK(listed && listed->recordings.empty() && listed->incomplete_ids.size() == 1);
	}
	{
		auto store = recording_store::open(path, {1, 1});
		CHECK(store && store->remove(first));
		CHECK(store->list() && store->list()->recordings.empty() &&
		      store->list()->incomplete_ids.empty());
		auto next = store->reserve_id();
		CHECK(next && *next != first);
		CHECK(store->cancel_reservation(*next));
	}
	{
		auto small = recording_store::open(parent / "quota", {100, 10000});
		CHECK(small);
		auto id = small->reserve_id();
		CHECK(id && !small->commit(*id, *capture, timing));
		CHECK(small->list() && small->list()->bytes_used == 0 && small->list()->recordings.empty());
		CHECK(small->cancel_reservation(*id));
	}
	{
		// Restart at an exact byte quota: an existing pending acknowledgment needs no
		// additional file bytes and must remain finishable without deleting any audio.
		const auto    ack_path = parent / "ack-quota";
		std::string   id;
		std::uint64_t bytes = 0;
		{
			auto store = recording_store::open(ack_path);
			CHECK(store);
			auto reserved = store->reserve_id();
			CHECK(reserved);
			id         = *reserved;
			auto saved = store->commit(id, *capture, timing);
			CHECK(saved);
			write(ack_path / id / "ack.pending",
			      ice::core::sha256(read(ack_path / saved->metadata_path)));
			auto inventory = store->list();
			CHECK(inventory);
			bytes = inventory->bytes_used;
		}
		auto store = recording_store::open(ack_path, {bytes, 10000});
		CHECK(store);
		CHECK(store->acknowledge(id));
		auto inventory = store->list();
		CHECK(inventory && inventory->recordings.front().acknowledged &&
		      inventory->bytes_used == bytes);
	}
	CHECK(!recording_store::open(parent / "invalid", {0, 10000}));
#ifndef _WIN32
	{
		auto store = recording_store::open(parent / "links");
		CHECK(store);
		auto id = store->reserve_id();
		CHECK(id);
		fs::create_symlink(path / ".writer.lock", parent / "links" / *id / "capture.wav");
		CHECK(!store->commit(*id, *capture, timing) && !store->list());
		CHECK(!store->remove(*id)); // Never follows or deletes a foreign link.
		fs::remove(parent / "links" / *id / "capture.wav");
		CHECK(store->cancel_reservation(*id));
	}
	fs::create_directory_symlink(path, parent / "alias");
	CHECK(!recording_store::open(parent / "alias"));
#endif
	std::puts("Recording store passed: durable recovery, exclusive publication, quotas, "
	          "ack/removal, WAV/metadata integrity");
}
