#include "ice/input/companion.h"
#include <iomanip>
#include <iostream>
#include <locale>
int main(int argc, char **argv) {
	if (argc != 2) {
		std::cerr << "Usage: ice_take_replay recording.iceinput\n";
		return 2;
	}
	ice::input::companion_session session;
	if (!session.take().load(argv[1]) || !session.start_replay()) {
		std::cerr << "Invalid or empty companion take\n";
		return 2;
	}
	session.advance_replay(301);
	std::cout.imbue(std::locale::classic());
	std::cout << std::setprecision(17);
	std::cout << "{\"matched\":" << (session.replay_matched() ? "true" : "false") << ",\"shots\":[";
	for (std::size_t i = 0; i < session.shots().size(); ++i) {
		const auto &shot = session.shots()[i];
		if (i)
			std::cout << ',';
		std::cout << "{\"source\":\""
		          << (shot.source == ice::input::shot_mode::touch ? "touch" : "motion")
		          << "\",\"aim\":" << shot.aim << ",\"power\":" << shot.power
		          << ",\"seconds\":" << shot.seconds << '}';
	}
	std::cout << "]}\n";
	return session.replay_matched() ? 0 : 1;
}
