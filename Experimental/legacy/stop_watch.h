// Unimplemented historical stopwatch prototype. Not built.
class stop_watch {
public:
	stop_watch(void);

	void start();
	void lap();
	void end();

private:
	timer timer_;
};

