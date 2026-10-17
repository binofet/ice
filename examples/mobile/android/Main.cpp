#include "../common/Demo.h"
#include "Overlay.h"
#include "ice/input/companion.h"
#include "ice/net/companion_link.h"
#include "ice/platform/host.h"
#include <algorithm>
#include <android/log.h>
#include <android/native_window.h>
#include <android/sensor.h>
#include <android_native_app_glue.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <time.h>
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "ICEHost", __VA_ARGS__)
static double networkNow() {
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}
struct AndroidProbe : ice::platform::host_client {
	ice::platform::host           host;
	android_app                  *app;
	ice::platform::surface_id     surface = 0;
	ice::platform::device_id      touch = 0, accel = 0, gyro = 0;
	ASensorManager               *manager       = nullptr;
	ASensorEventQueue            *sensors       = nullptr;
	const ASensor                *accelerometer = nullptr;
	const ASensor                *gyroscope     = nullptr;
	bool                          resumed = false, visible = false, focused = false;
	unsigned                      frames = 0, inputs = 0;
	double                        elapsed = 0, sensorClockOffset = 0;
	ice::input::companion_session companion;
	ice::net::companion_link      link;
	bool                          pairingPanel = false, toolsVisible = false;
	int                           pairingField = 0;
#ifndef NDEBUG
	bool networkSmoke = false, networkSmokeSent = false;
#endif
	std::string address, portText, codeText;
	void        LoadConnection() {
        FILE *f = std::fopen(
            (std::string(app->activity->internalDataPath) + "/connection.txt").c_str(), "r");
        if (f) {
            char     ip[32] = {};
            unsigned port = 0, code = 0;
            if (std::fscanf(f, "%31s %u %u", ip, &port, &code) == 3) {
                address  = ip;
                portText = std::to_string(port);
                codeText = std::to_string(code);
            }
            std::fclose(f);
        }
	}
	bool PairingControl(const ice::platform::device_input &e) {
		if (e.phase != ice::platform::input_phase::begin)
			return true;
		if (e.y >= .11f && e.y < .32f) {
			pairingField = std::min(2, (int)((e.y - .11f) / .07f));
			return true;
		}
		if (e.y >= .36f && e.y < .76f) {
			int         row = (e.y - .36f) / .1f, col = std::min(2, (int)(e.x * 3));
			const char *keys = "123456789.0-";
			char        key  = keys[row * 3 + col];
			auto &text = pairingField == 0 ? address : pairingField == 1 ? portText : codeText;
			if (key == '-') {
				if (!text.empty())
					text.pop_back();
			} else if (text.size() < (pairingField == 0   ? 15u
			                          : pairingField == 1 ? 5u
			                                              : 6u) &&
			           (key != '.' || pairingField == 0))
				text += key;
		}
		if (e.y >= .80f && e.y < .88f) {
			unsigned port = std::strtoul(portText.c_str(), nullptr, 10),
			         code = std::strtoul(codeText.c_str(), nullptr, 10);
			if (port > 0 && port <= 65535 && codeText.size() == 6) {
				link.connect(address.c_str(), (unsigned short)port, code, networkNow());
				pairingPanel = false;
				message      = "REPLAY STAYS LOCAL. LIVE SHOTS GO TO DESKTOP.";
			} else
				message = "ENTER PORT AND SIX DIGIT CODE.";
		}
		if (e.y >= .90f)
			pairingPanel = false;
		return true;
	}
	std::string takePath, message = "DRAG UP TO ROLL. TAP PAIR TO CONNECT.";
	void        SaveTake() {
        if (companion.take().events.empty())
            return;
        message = companion.take().save(takePath) ? "TAKE SAVED. TAP REPLAY TO CHECK IT."
		                                                 : "COULD NOT SAVE TAKE.";
	}
	void FinishRecording() {
		if (companion.recording()) {
			companion.stop_recording();
			SaveTake();
		}
	}
	bool Control(const ice::platform::device_input &e) {
		if (e.kind != ice::platform::input_kind::pointer)
			return false;
		if (pairingPanel)
			return PairingControl(e);
		if (e.phase != ice::platform::input_phase::begin)
			return false;
		if (e.y >= .025f && e.y < .075f && e.x > .78f) {
			companion.suspend();
			if (link.status() == ice::net::companion_link::state::connected ||
			    link.status() == ice::net::companion_link::state::pairing)
				link.close();
			else {
				LoadConnection();
				pairingPanel = true;
			}
			return true;
		}
		if (e.y >= .025f && e.y < .075f && e.x > .60f && e.x <= .78f) {
			toolsVisible = !toolsVisible;
			return true;
		}
		if (!toolsVisible)
			return false;
		if (e.y >= .14f && e.y < .21f) {
			if (!companion.replaying()) {
				FinishRecording();
				if (e.x < .5f)
					companion.set_mode(ice::input::shot_mode::touch);
				else if (accel)
					companion.set_mode(ice::input::shot_mode::motion);
				else
					message = "MOTION NEEDS DEVICE SENSORS.";
			}
			return true;
		}
		if (e.y >= .23f && e.y < .30f) {
			if (companion.replaying())
				return true;
			if (e.x < .333f) {
				if (companion.recording())
					FinishRecording();
				else {
					companion.start_recording();
					message = "RECORDING RAW INPUT.";
				}
			} else if (e.x < .666f) {
				FinishRecording();
				if (!companion.start_replay())
					message = "RECORD A TAKE FIRST.";
			} else {
				FinishRecording();
				auto result = companion.calibrate();
				LOG("companion calibration=%d", (int)result);
				message = result == ice::input::calibration_result::ready
				              ? "CALIBRATED. HOLD SWING RELEASE."
				          : result == ice::input::calibration_result::moving
				              ? "HOLD STILL AND TRY AGAIN."
				              : "WAIT FOR SENSOR SAMPLES.";
			}
			return true;
		}
		return false;
	}
	explicit AndroidProbe(android_app *a) : host(*this), app(a) {
#ifndef NDEBUG
		// One-shot native transport fixture, supplied explicitly through adb run-as.
		// Never enabled by normal pairing, recordings, or replay.
		std::string fixture = std::string(app->activity->internalDataPath) + "/network-smoke.txt";
		FILE       *test    = std::fopen(fixture.c_str(), "r");
		if (test) {
			char     ip[32] = {};
			unsigned port = 0, code = 0;
			if (std::fscanf(test, "%31s %u %u", ip, &port, &code) == 3 && port > 0 && port <= 65535)
				networkSmoke = link.connect(ip, (unsigned short)port, code, networkNow());
			std::fclose(test);
			std::remove(fixture.c_str());
		}
#endif
		takePath = std::string(app->activity->internalDataPath) + "/last-take.iceinput";
		if (companion.take().load(takePath))
			message = "PREVIOUS TAKE LOADED. TAP REPLAY.";
		else {
			companion.take() = icCompanionDemo();
			message          = "DEMO TAKE LOADED. REPLAY OR RECORD YOUR OWN.";
		}
	}
	void UpdateState() {
		host.set_state(resumed && focused ? ice::platform::app_state::active
		               : visible          ? ice::platform::app_state::inactive
		                                  : ice::platform::app_state::background);
	}
	void on_app_state(ice::platform::app_state state) override {
		LOG("state=%d", (int)state);
		if (state == ice::platform::app_state::active) {
			// Android sensors include deep sleep; MotionEvent/steady_clock do not.
			// Re-correlate after every resume before comparing gesture sample times.
			timespec boot{}, monotonic{};
			clock_gettime(CLOCK_MONOTONIC, &monotonic);
			clock_gettime(CLOCK_BOOTTIME, &boot);
			sensorClockOffset =
			    (boot.tv_sec - monotonic.tv_sec) + (boot.tv_nsec - monotonic.tv_nsec) * 1e-9;
		}
		if (state != ice::platform::app_state::active) {
			FinishRecording();
			companion.deactivate();
			link.close();
		}
		if (!sensors)
			return;
		for (auto sensor : {accelerometer, gyroscope})
			if (sensor) {
				if (state == ice::platform::app_state::active) {
					ASensorEventQueue_enableSensor(sensors, sensor);
					ASensorEventQueue_setEventRate(sensors, sensor, 16667);
				} else
					ASensorEventQueue_disableSensor(sensors, sensor);
			}
	}
	void on_surface_added(const ice::platform::surface &s) override {
		LOG("surface added=%llu %dx%d", (unsigned long long)s.id, s.width, s.height);
	}
	void on_surface_changed(const ice::platform::surface &s) override {
		LOG("surface resized=%llu %dx%d", (unsigned long long)s.id, s.width, s.height);
	}
	void on_surface_removed(const ice::platform::surface &s) override {
		LOG("surface removed=%llu", (unsigned long long)s.id);
	}
	void on_input(const ice::platform::device_input &event) override {
		++inputs;
		if (Control(event))
			return;
		if (event.kind == ice::platform::input_kind::pointer &&
		    event.phase == ice::platform::input_phase::begin &&
		    event.y < (toolsVisible ? .46f : .23f))
			return;
		bool recording = companion.recording();
		auto before    = companion.shots().size();
		companion.on_input(event);
		if (recording && !companion.recording())
			SaveTake();
		if (companion.shots().size() > before) {
			const auto &shot = companion.shots().back();
			link.send_shot(shot, networkNow());
			LOG("companion shot aim=%.3f power=%.3f source=%d", shot.aim, shot.power,
			    (int)shot.source);
		}
	}
	void on_update(double dt) override {
		link.poll(networkNow());
#ifndef NDEBUG
		if (networkSmoke && !networkSmokeSent &&
		    link.status() == ice::net::companion_link::state::connected) {
			ice::input::shot_intent fixture;
			fixture.aim      = .25f;
			fixture.power    = .5f;
			networkSmokeSent = link.send_shot(fixture, networkNow());
			LOG("SYNTHETIC native transport fixture sent=%d", networkSmokeSent);
		}
#endif
		elapsed += dt;
		bool playing = companion.replaying();
		companion.advance_replay(dt);
		if (playing && !companion.replaying())
			LOG("companion replay matched=%d shots=%zu", companion.replay_matched(),
			    companion.shots().size());
	}
	void on_render(const ice::platform::surface &s) override {
		auto                 window = static_cast<ANativeWindow *>(s.native_handle);
		ANativeWindow_Buffer buffer;
		if (ANativeWindow_lock(window, &buffer, nullptr) < 0)
			return;
		CompanionOverlay ui{static_cast<std::uint32_t *>(buffer.bits), buffer.width, buffer.height,
		                    buffer.stride,
		                    std::max(2, std::min(buffer.width / 420, buffer.height / 350))};
		ui.rect(0, 0, buffer.width, buffer.height, 0xff24170d);
		if (pairingPanel) {
			ui.text(24, buffer.height * .035f, "CONNECT TO DESKTOP / F2 IN SKEEBALL");
			std::string fields[] = {"IP: " + address, "PORT: " + portText, "CODE: " + codeText};
			for (int i = 0; i < 3; ++i)
				ui.button(.02f, .11f + i * .07f, .96f, .06f, fields[i].c_str(), pairingField == i);
			const char *keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "DEL"};
			for (int i = 0; i < 12; ++i)
				ui.button((i % 3) / 3.f, .36f + (i / 3) * .1f, 1.f / 3, .09f, keys[i]);
			ui.button(.02f, .80f, .96f, .08f, "CONNECT");
			ui.button(.02f, .90f, .96f, .07f, "CANCEL");
			ANativeWindow_unlockAndPost(window);
			return;
		}
		ui.text(24, buffer.height * .035f, "SKEEBALL");
		ui.button(.60f, .025f, .18f, .05f, toolsVisible ? "DONE" : "TOOLS");
		ui.button(.79f, .025f, .21f, .05f,
		          link.status() == ice::net::companion_link::state::connected ? "UNPAIR" : "PAIR");
		ui.text(24, buffer.height * .085f,
		        companion.replaying()        ? "REPLAYING"
		        : companion.recording()      ? "RECORDING"
		        : companion.replay_matched() ? "REPLAY MATCHED"
		                                     : link.message().c_str(),
		        0xffd9e56b);
		bool motion = companion.gesture().config().mode == ice::input::shot_mode::motion;
		if (toolsVisible) {
			ui.button(.02f, .14f, .48f, .07f, "TOUCH DRAG", !motion);
			ui.button(.5f, .14f, .48f, .07f, "MOTION SWING", motion);
			ui.button(.02f, .23f, .32f, .07f, companion.recording() ? "STOP / SAVE" : "RECORD");
			ui.button(.34f, .23f, .32f, .07f, "REPLAY");
			ui.button(.66f, .23f, .32f, .07f, "CALIBRATE");
			ui.text(24, buffer.height * .335f, message.c_str());
		}
		float padTop   = toolsVisible ? .49f : .24f;
		float strength = companion.gesture().power();
		ui.rect(24, buffer.height * (toolsVisible ? .40f : .20f), buffer.width - 48, 12,
		        0xff57412f);
		ui.rect(24, buffer.height * (toolsVisible ? .40f : .20f), (buffer.width - 48) * strength,
		        12, 0xff9fe457);
		char line[100];
		if (companion.shots().empty())
			std::snprintf(line, sizeof(line), "DRAG UP AND RELEASE TO ROLL");
		else {
			const auto &shot = companion.shots().back();
			std::snprintf(line, sizeof(line), "POWER %.0f%%  AIM %+.0f%%", shot.power * 100,
			              shot.aim * 100);
		}
		ui.text(24, buffer.height * (toolsVisible ? .43f : .16f), line);
		ui.rect(24, buffer.height * padTop, buffer.width - 48, buffer.height * (.94f - padTop),
		        0xff322217);
		const char *hint =
		    motion ? (companion.gesture().config().calibrated ? "HOLD - SWING - RELEASE"
		                                                      : "HOLD STILL AND CALIBRATE FIRST")
		           : "DRAG UP - RELEASE TO ROLL";
		ui.text(40, buffer.height * (padTop + .03f), hint);
		float aim = companion.gesture().aim();
		if (!companion.gesture().armed() && !companion.shots().empty()) {
			aim      = companion.shots().back().aim;
			strength = companion.shots().back().power;
		}
		int startX = buffer.width / 2, startY = buffer.height * .90f,
		    endX = startX + aim * buffer.width * .38f,
		    endY = startY - strength * buffer.height * .3f;
		for (int i = 0; i < 40; ++i) {
			float t = i / 39.f;
			ui.rect(startX + (endX - startX) * t - 2, startY + (endY - startY) * t - 2, 4, 4,
			        0xffb6d86c);
		}
		ui.rect(endX - 12, endY - 12, 24, 24, 0xffffffffu);
		ANativeWindow_unlockAndPost(window);
		++frames;
		if (frames == 1 || frames % 300 == 0)
			LOG("companion frame=%u input=%u shots=%zu", frames, inputs, companion.shots().size());
	}
	void Attach() {
		if (!app->window)
			return;
		if (surface)
			host.remove_surface(surface);
		ANativeWindow_setBuffersGeometry(app->window, 0, 0, WINDOW_FORMAT_RGBA_8888);
		ice::platform::surface s;
		s.native_handle = app->window;
		s.width         = ANativeWindow_getWidth(app->window);
		s.height        = ANativeWindow_getHeight(app->window);
		surface         = host.add_surface(s);
	}
	void motion() {
		ASensorEvent sample;
		while (sensors && ASensorEventQueue_getEvents(sensors, &sample, 1) > 0) {
			ice::platform::device_input event;
			event.seconds = sample.timestamp * 1e-9 - sensorClockOffset;
			if (sample.type == ASENSOR_TYPE_ACCELEROMETER) {
				event.device = accel;
				event.kind   = ice::platform::input_kind::acceleration;
			} else if (sample.type == ASENSOR_TYPE_GYROSCOPE) {
				event.device = gyro;
				event.kind   = ice::platform::input_kind::angular_velocity;
			} else
				continue;
			event.x = sample.vector.x;
			event.y = sample.vector.y;
			event.z = sample.vector.z;
			host.post_input(event);
		}
	}
};
static void command(android_app *app, int32_t cmd) {
	auto &probe = *static_cast<AndroidProbe *>(app->userData);
	switch (cmd) {
	case APP_CMD_START:
		probe.visible = true;
		probe.UpdateState();
		break;
	case APP_CMD_RESUME:
		probe.resumed = true;
		probe.UpdateState();
		break;
	case APP_CMD_PAUSE:
		probe.resumed = false;
		probe.UpdateState();
		break;
	case APP_CMD_STOP:
		probe.visible = false;
		probe.UpdateState();
		break;
	case APP_CMD_GAINED_FOCUS:
		probe.focused = true;
		probe.UpdateState();
		break;
	case APP_CMD_LOST_FOCUS:
		probe.focused = false;
		probe.UpdateState();
		break;
	case APP_CMD_INIT_WINDOW:
		probe.Attach();
		break;
	case APP_CMD_TERM_WINDOW:
		probe.host.remove_surface(probe.surface);
		probe.surface = 0;
		break;
	default:
		break;
	}
}
static int32_t input(android_app *app, AInputEvent *native) {
	if (AInputEvent_getType(native) != AINPUT_EVENT_TYPE_MOTION)
		return 0;
	auto &probe = *static_cast<AndroidProbe *>(app->userData);
	if (!probe.surface || !app->window)
		return 0;
	int    action   = AMotionEvent_getAction(native);
	int    type     = action & AMOTION_EVENT_ACTION_MASK;
	size_t count    = AMotionEvent_getPointerCount(native),
	       selected = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
	                  AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
	for (size_t i = 0; i < count; ++i) {
		if (type != AMOTION_EVENT_ACTION_MOVE && type != AMOTION_EVENT_ACTION_CANCEL &&
		    i != selected)
			continue;
		ice::platform::device_input event;
		event.device  = probe.touch;
		event.surface = probe.surface;
		event.contact = AMotionEvent_getPointerId(native, i);
		event.seconds = AMotionEvent_getEventTime(native) * 1e-9;
		if (type == AMOTION_EVENT_ACTION_DOWN || type == AMOTION_EVENT_ACTION_POINTER_DOWN)
			event.phase = ice::platform::input_phase::begin;
		else if (type == AMOTION_EVENT_ACTION_UP || type == AMOTION_EVENT_ACTION_POINTER_UP)
			event.phase = ice::platform::input_phase::end;
		else if (type == AMOTION_EVENT_ACTION_MOVE)
			event.phase = ice::platform::input_phase::change;
		else if (type == AMOTION_EVENT_ACTION_CANCEL)
			event.phase = ice::platform::input_phase::cancel;
		else
			continue;
		event.x = AMotionEvent_getX(native, i) / std::max(1, ANativeWindow_getWidth(app->window));
		event.y = AMotionEvent_getY(native, i) / std::max(1, ANativeWindow_getHeight(app->window));
		probe.host.post_input(event);
	}
	return 1;
}
extern "C" void android_main(android_app *app) {
	AndroidProbe probe(app);
	app->userData     = &probe;
	app->onAppCmd     = command;
	app->onInputEvent = input;
	probe.touch =
	    probe.host.add_device(ice::platform::input_capability(ice::platform::input_kind::pointer));
	probe.manager = ASensorManager_getInstance();
	if (probe.manager) {
		probe.accelerometer =
		    ASensorManager_getDefaultSensor(probe.manager, ASENSOR_TYPE_ACCELEROMETER);
		probe.gyroscope = ASensorManager_getDefaultSensor(probe.manager, ASENSOR_TYPE_GYROSCOPE);
		probe.sensors = ASensorManager_createEventQueue(probe.manager, app->looper, LOOPER_ID_USER,
		                                                nullptr, nullptr);
		if (probe.sensors) {
			if (probe.accelerometer)
				probe.accel = probe.host.add_device(
				    ice::platform::input_capability(ice::platform::input_kind::acceleration));
			if (probe.gyroscope)
				probe.gyro = probe.host.add_device(
				    ice::platform::input_capability(ice::platform::input_kind::angular_velocity));
		}
	}
	while (!app->destroyRequested) {
		int                  events;
		android_poll_source *source_ = nullptr;
		int                  ident =
		    ALooper_pollOnce(probe.host.state() == ice::platform::app_state::active ? 16 : -1,
		                     nullptr, &events, (void **)&source_);
		if (ident >= 0 && source_)
			source_->process(app, source_);
		probe.motion();
		if (probe.surface && app->window)
			probe.host.resize_surface(probe.surface, ANativeWindow_getWidth(app->window),
			                          ANativeWindow_getHeight(app->window), 1);
		double now =
		    std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
		        .count();
		probe.host.frame(now);
	}
	probe.host.stop();
	if (probe.sensors)
		ASensorManager_destroyEventQueue(probe.manager, probe.sensors);
}
