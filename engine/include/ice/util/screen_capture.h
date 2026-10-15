#pragma once
namespace ice::util {

/// Legacy capture interface for the current render surface; file-output operations require a live
/// context.
class screen_capture {
public:
	/// Construct inactive state; initialize explicitly before active use.
	screen_capture();
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	~screen_capture();

	/// Initialize against the supplied borrowed dependencies; inspect the result before using the
	/// subsystem.
	void init();

	/// Capture the active render surface to owned/output storage in the live current context.
	void capture_screen();

private:
};

} // namespace ice::util
