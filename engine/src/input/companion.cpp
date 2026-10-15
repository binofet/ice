#include "ice/input/companion.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
namespace ice::input {
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
} // namespace ice::input
#include <windows.h>
namespace ice::input {
#endif
namespace {
const std::size_t max_events = 18000;
float             clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
bool              valid(const ice::platform::device_input &e) {
    bool held =
        e.kind == ice::platform::input_kind::pointer || e.kind == ice::platform::input_kind::key;
    bool phase = held ? e.phase != ice::platform::input_phase::sample
	                               : e.phase == ice::platform::input_phase::sample;
    return phase && e.device && std::isfinite(e.seconds) && e.seconds >= 0 && std::isfinite(e.x) &&
           std::isfinite(e.y) && std::isfinite(e.z) && std::isfinite(e.w) &&
           (unsigned)e.kind <= (unsigned)ice::platform::input_kind::attitude &&
           (unsigned)e.phase <= (unsigned)ice::platform::input_phase::sample;
}
bool valid(const gesture_config &c) {
	float g = c.gravity_x * c.gravity_x + c.gravity_y * c.gravity_y + c.gravity_z * c.gravity_z;
	return (unsigned)c.mode <= 1 && std::isfinite(g) && (!c.calibrated || (g >= 64 && g <= 121));
}
bool same(const shot_intent &a, const shot_intent &b) {
	return a.source == b.source && std::abs(a.aim - b.aim) < 1e-5 &&
	       std::abs(a.power - b.power) < 1e-5 && std::abs(a.seconds - b.seconds) < 1e-8;
}
} // namespace
void shot_gesture::reset(const gesture_config &config) {
	config_ = valid(config) ? config : gesture_config();
	samples_.clear();
	gyro_ = 0;
	cancel();
}
void shot_gesture::cancel() {
	armed_ = false;
	aim_ = touch_power_ = filtered_ = peak_ = 0;
	last_motion_ = peak_time_ = last_pointer_ = -1;
}
calibration_result shot_gesture::calibrate() {
	if (samples_.size() < 20 || samples_.back().seconds - samples_.front().seconds < .25)
		return calibration_result::need_samples;
	double x = 0, y = 0, z = 0;
	for (const auto &e : samples_) {
		x += e.x;
		y += e.y;
		z += e.z;
	}
	x /= samples_.size();
	y /= samples_.size();
	z /= samples_.size();
	double variance = 0;
	for (const auto &e : samples_)
		variance += (e.x - x) * (e.x - x) + (e.y - y) * (e.y - y) + (e.z - z) * (e.z - z);
	double gravity = x * x + y * y + z * z;
	if (variance / samples_.size() > .16 || gravity < 64 || gravity > 121 || gyro_ > .4f)
		return calibration_result::moving;
	cancel();
	config_.gravity_x  = x;
	config_.gravity_y  = y;
	config_.gravity_z  = z;
	config_.calibrated = true;
	return calibration_result::ready;
}
float shot_gesture::power() const {
	return config_.mode == shot_mode::touch ? touch_power_ : clamp((peak_ - 1.0f) / 14.0f, 0, 1);
}
bool shot_gesture::consume(const ice::platform::device_input &event, shot_intent &shot) {
	if (!valid(event))
		return false;
	if (event.kind == ice::platform::input_kind::angular_velocity &&
	    event.phase == ice::platform::input_phase::sample) {
		gyro_ = std::hypot(event.x, std::hypot(event.y, event.z));
		return false;
	}
	if (event.kind == ice::platform::input_kind::acceleration &&
	    event.phase == ice::platform::input_phase::sample) {
		if (!armed_) {
			if (!samples_.empty() && event.seconds < samples_.back().seconds)
				return false;
			samples_.push_back(event);
			while (samples_.size() > 90 || event.seconds - samples_.front().seconds > .75)
				samples_.pop_front();
		} else if (config_.mode == shot_mode::motion && event.seconds >= contact_.seconds) {
			if (event.seconds - contact_.seconds > 3) {
				cancel();
				return false;
			}
			if (last_motion_ >= 0 && event.seconds <= last_motion_)
				return false;
			float dt = last_motion_ < 0 ? 1.f / 60 : clamp(event.seconds - last_motion_, 0, .1f);
			float x = event.x - config_.gravity_x, y = event.y - config_.gravity_y,
			      z         = event.z - config_.gravity_z;
			float magnitude = clamp(std::hypot(x, std::hypot(y, z)), 0, 100);
			filtered_ += (magnitude - filtered_) * (1 - std::exp(-dt / .035f));
			peak_ *= std::exp(-dt * 2);
			if (filtered_ >= peak_) {
				peak_      = filtered_;
				peak_time_ = event.seconds;
			}
			last_motion_ = event.seconds;
		}
		return false;
	}
	if (event.kind != ice::platform::input_kind::pointer)
		return false;
	if (event.phase == ice::platform::input_phase::begin) {
		if (armed_ || (config_.mode == shot_mode::motion && !config_.calibrated))
			return false;
		cancel();
		armed_        = true;
		contact_      = event;
		start_y_      = event.y;
		last_pointer_ = event.seconds;
	}
	if (!armed_ || event.device != contact_.device || event.surface != contact_.surface ||
	    event.contact != contact_.contact || event.seconds < last_pointer_)
		return false;
	last_pointer_ = event.seconds;
	if (event.phase == ice::platform::input_phase::cancel) {
		cancel();
		return false;
	}
	aim_         = clamp((event.x - .5f) * 2, -1, 1);
	touch_power_ = clamp((start_y_ - event.y) * 1.8f, 0, 1);
	if (event.phase != ice::platform::input_phase::end)
		return false;
	double duration = event.seconds - contact_.seconds;
	float  power    = this->power();
	bool   fresh    = config_.mode == shot_mode::touch
	                      ? duration <= 8
	                      : duration <= 3 && last_motion_ >= contact_.seconds &&
                           event.seconds - last_motion_ <= .25 && event.seconds - peak_time_ <= .6;
	shot.aim        = aim_;
	shot.power      = power;
	shot.seconds    = event.seconds;
	shot.source     = config_.mode;
	cancel();
	return fresh && power >= .08f;
}
void companion_session::consume(const ice::platform::device_input &event, bool record) {
	shot_intent shot;
	if (gesture_.consume(event, shot)) {
		shots_.push_back(shot);
		if (record)
			take_.expected.push_back(shot);
	}
}
void companion_session::on_input(const ice::platform::device_input &event) {
	if (replaying_ || !valid(event))
		return;
	bool record = recording_;
	if (record) {
		if (take_.events.size() == max_events ||
		    (!take_.events.empty() &&
		     std::abs(event.seconds - take_.events.front().seconds) > 300)) {
			recording_ = false;
			record     = false;
		} else
			take_.events.push_back(event);
	}
	consume(event, record);
}
void companion_session::set_mode(shot_mode mode) {
	suspend();
	auto config = gesture_.config();
	config.mode = mode;
	gesture_.reset(config);
}
calibration_result companion_session::calibrate() {
	suspend();
	return gesture_.calibrate();
}
void companion_session::start_recording() {
	suspend();
	take_        = companion_take();
	take_.config = gesture_.config();
	gesture_.reset(take_.config);
	shots_.clear();
	recording_      = true;
	replay_matched_ = false;
}
bool companion_session::start_replay() {
	suspend();
	if (take_.events.empty())
		return false;
	gesture_.reset(take_.config);
	shots_.clear();
	replay_index_ = 0;
	replay_time_ = deadline_ = 0;
	replaying_               = true;
	replay_matched_          = false;
	return true;
}
void companion_session::advance_replay(double seconds) {
	if (!replaying_ || !std::isfinite(seconds) || seconds < 0)
		return;
	replay_time_ += seconds;
	while (replay_index_ < take_.events.size() && deadline_ <= replay_time_) {
		consume(take_.events[replay_index_++], false);
		if (replay_index_ < take_.events.size())
			deadline_ = std::max(deadline_, take_.events[replay_index_].seconds -
			                                    take_.events.front().seconds);
	}
	if (replay_index_ == take_.events.size()) {
		replaying_      = false;
		replay_matched_ = shots_.size() == take_.expected.size();
		for (std::size_t i = 0; replay_matched_ && i < shots_.size(); ++i)
			replay_matched_ = same(shots_[i], take_.expected[i]);
		gesture_.cancel();
	}
}
void companion_session::deactivate() {
	suspend();
	auto config       = gesture_.config();
	config.calibrated = false;
	gesture_.reset(config);
}
void companion_session::suspend() {
	recording_ = false;
	replaying_ = false;
	gesture_.cancel();
}
bool companion_take::save(const std::string &path) const {
	if (!valid(config) || events.size() > max_events || expected.size() > events.size())
		return false;
	for (const auto &e : events)
		if (!valid(e) || std::abs(e.seconds - events.front().seconds) > 300)
			return false;
	for (const auto &s : expected)
		if ((unsigned)s.source > 1 || !std::isfinite(s.aim) || !std::isfinite(s.power) ||
		    !std::isfinite(s.seconds) || s.seconds < 0 || std::abs(s.aim) > 1 || s.power < .08f ||
		    s.power > 1)
			return false;
	// The app supplies a writable sandbox path. Commit complete takes by rename.
	std::string   temporary = path + ".tmp";
	std::ofstream file(temporary, std::ios::trunc);
	file.imbue(std::locale::classic());
	file << std::setprecision(std::numeric_limits<double>::max_digits10);
	file << "ICE_COMPANION 1\n"
	     << (int)config.mode << ' ' << config.calibrated << ' ' << config.gravity_x << ' '
	     << config.gravity_y << ' ' << config.gravity_z << '\n';
	file << events.size() << ' ' << expected.size() << '\n';
	for (const auto &e : events)
		file << e.device << ' ' << e.surface << ' ' << e.contact << ' ' << e.seconds << ' '
		     << (int)e.kind << ' ' << (int)e.phase << ' ' << e.x << ' ' << e.y << ' ' << e.z << ' '
		     << e.w << '\n';
	for (const auto &s : expected)
		file << (int)s.source << ' ' << s.aim << ' ' << s.power << ' ' << s.seconds << '\n';
	file.flush();
	bool good = file.good();
	file.close();
	good = good && !file.fail();
#ifdef _WIN32
	if (good && MoveFileExA(temporary.c_str(), path.c_str(),
	                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		return true;
#else
	if (good && std::rename(temporary.c_str(), path.c_str()) == 0)
		return true;
#endif
	std::remove(temporary.c_str());
	return false;
}
bool companion_take::load(const std::string &path) {
	std::ifstream file(path);
	file.imbue(std::locale::classic());
	if (!file)
		return false;
	file.seekg(0, std::ios::end);
	auto bytes = file.tellg();
	if (bytes < 0 || bytes > 8 * 1024 * 1024)
		return false;
	file.seekg(0);
	std::string    magic;
	int            version, mode, calibrated;
	std::size_t    count, shots;
	companion_take candidate;
	if (!(file >> magic >> version) || magic != "ICE_COMPANION" || version != 1)
		return false;
	if (!(file >> mode >> calibrated >> candidate.config.gravity_x >> candidate.config.gravity_y >>
	      candidate.config.gravity_z) ||
	    mode < 0 || mode > 1 || calibrated < 0 || calibrated > 1)
		return false;
	candidate.config.mode       = (shot_mode)mode;
	candidate.config.calibrated = calibrated != 0;
	if (!valid(candidate.config) || !(file >> count >> shots) || count > max_events ||
	    shots > count)
		return false;
	for (std::size_t i = 0; i < count; ++i) {
		ice::platform::device_input e;
		int                         kind, phase;
		if (!(file >> e.device >> e.surface >> e.contact >> e.seconds >> kind >> phase >> e.x >>
		      e.y >> e.z >> e.w) ||
		    kind < 0 || kind > 4 || phase < 0 || phase > 4)
			return false;
		e.kind  = (ice::platform::input_kind)kind;
		e.phase = (ice::platform::input_phase)phase;
		if (!valid(e))
			return false;
		candidate.events.push_back(e);
	}
	for (std::size_t i = 0; i < shots; ++i) {
		shot_intent s;
		int         source;
		if (!(file >> source >> s.aim >> s.power >> s.seconds) || source < 0 || source > 1 ||
		    !std::isfinite(s.aim) || !std::isfinite(s.power) || !std::isfinite(s.seconds) ||
		    s.seconds < 0 || std::abs(s.aim) > 1 || s.power < .08f || s.power > 1)
			return false;
		s.source = (shot_mode)source;
		candidate.expected.push_back(s);
	}
	std::string extra;
	if (file >> extra)
		return false;
	// A take is a bounded diagnostic session, not an unbounded wait on hostile timestamps.
	if (!candidate.events.empty())
		for (const auto &e : candidate.events)
			if (std::abs(e.seconds - candidate.events.front().seconds) > 300)
				return false;
	*this = std::move(candidate);
	return true;
}

} // namespace ice::input
