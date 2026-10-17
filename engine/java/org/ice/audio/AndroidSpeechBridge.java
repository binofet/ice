package org.ice.audio;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.speech.tts.Voice;
import java.io.File;
import java.nio.file.Files;
import java.util.Set;
import java.util.TreeMap;

/** Optional installed-TTS PCM bridge. No speak(), data-install request, or output device. */
public final class AndroidSpeechBridge {
	private final Context context;
	private final String  requestedEngine;
	private final long    owner;
	private final Handler ui                      = new Handler(Looper.getMainLooper());
	private final TreeMap<String, Voice> eligible = new TreeMap<String, Voice>();
	private TextToSpeech                 tts;
	private volatile int                 state; // 0 pending, 1 ready, 2 unavailable, 3 closed.
	private volatile String              error         = "";
	private volatile String[]            voiceSnapshot = new String[0];
	private File                         scratchDirectory;
	private File                         scratch;
	private long                         generation;
	private String[]                     segments;
	private int                          segment;
	private boolean                      terminalSeen;

	public AndroidSpeechBridge(Context suppliedContext, String engine, long callbackOwner) {
		context         = suppliedContext.getApplicationContext();
		requestedEngine = engine;
		owner           = callbackOwner;
		ui.post(new Runnable() {
			@Override
			public void run() {
				initialize();
			}
		});
	}
	private synchronized void initialize() {
		if (state == 3 || closeRequested)
			return;
		try {
			scratchDirectory = new File(context.getCacheDir(), "ice-speech-pcm");
			if (Files.isSymbolicLink(scratchDirectory.toPath()) ||
			    (!scratchDirectory.isDirectory() && !scratchDirectory.mkdir())) {
				unavailable("Private speech scratch directory unavailable");
				return;
			}
			// onInit can fire inside the constructor on failure. Queue handling until tts exists.
			tts = new TextToSpeech(context, new TextToSpeech.OnInitListener() {
				@Override
				public void onInit(final int status) {
					ui.post(new Runnable() {
						@Override
						public void run() {
							initialized(status);
						}
					});
				}
			}, requestedEngine.isEmpty() ? null : requestedEngine);
		} catch (RuntimeException | OutOfMemoryError failure) {
			unavailable("Android speech initialization failed");
		}
	}
	private synchronized void initialized(int status) {
		if (state == 3 || closeRequested)
			return;
		try {
			if (status != TextToSpeech.SUCCESS || tts == null) {
				unavailable("No initialized installed Android TTS engine");
				return;
			}
			Set<Voice> voices = tts.getVoices();
			if (voices == null) {
				unavailable("Android TTS voice discovery failed");
				return;
			}
			for (Voice voice : voices) {
				Set<String> features = voice.getFeatures();
				// setVoice(notInstalled) may trigger downloads: filter BEFORE selecting anything.
				if (!voice.isNetworkConnectionRequired() && features != null &&
				    !features.contains(TextToSpeech.Engine.KEY_FEATURE_NOT_INSTALLED)) {
					if (eligible.size() == 65536) {
						unavailable("Android voice inventory exhausted");
						return;
					}
					eligible.put(voice.getName(), voice);
				}
			}
			if (eligible.isEmpty()) {
				unavailable("No advertised installed offline Android voices");
				return;
			}
			if (tts.setOnUtteranceProgressListener(listener) != TextToSpeech.SUCCESS) {
				unavailable("Android speech callback registration failed");
				return;
			}
			String[] snapshot = new String[eligible.size() * 3];
			int      at       = 0;
			for (Voice voice : eligible.values()) {
				snapshot[at++] = voice.getName();
				snapshot[at++] = voice.getName();
				snapshot[at++] = voice.getLocale().toLanguageTag();
			}
			voiceSnapshot = snapshot;
			state         = 1;
		} catch (RuntimeException | OutOfMemoryError failure) {
			unavailable("Android speech discovery failed");
		}
	}
	private void unavailable(String message) {
		error = message;
		state = 2;
		if (tts != null) {
			try {
				tts.shutdown();
			} catch (RuntimeException ignored) {
			}
			tts = null;
		}
	}
	public int      status() { return state; }
	public String   failure() { return error; }
	public int      maximumCharacters() { return TextToSpeech.getMaxSpeechInputLength(); }
	public String[] voices() { return voiceSnapshot; }
	private static final class Pending {
		final long generation;
		final String[] text;
		final String   voice;
		final float    rate;
		Pending(long token, String[] parts, String selected, float speed) {
			generation = token;
			text       = parts;
			voice      = selected;
			rate       = speed;
		}
	}
	private final Object     commandLock = new Object();
	private Pending          pending;
	private boolean          commandsPosted, cancelRequested;
	private volatile boolean closeRequested;
	private long             stopGeneration;
	// commandLock protects only bounded mailbox state, never an external TTS/Binder call.
	private void scheduleCommands() {
		if (commandsPosted)
			return;
		commandsPosted = true;
		if (!ui.post(new Runnable() {
			    @Override
			    public void run() {
				    drainCommands();
			    }
		    })) {
			commandsPosted = false;
			if (pending != null)
				nativeFailure(owner, pending.generation, -1, "Android UI service unavailable");
			pending = null;
		}
	}
	private void drainCommands() {
		final boolean close, cancel;
		final long    stop;
		final Pending next;
		synchronized (commandLock) {
			commandsPosted  = false;
			close           = closeRequested;
			cancel          = cancelRequested;
			stop            = stopGeneration;
			next            = pending;
			cancelRequested = false;
			stopGeneration  = 0;
			pending         = null;
		}
		synchronized (this) {
			if (close) {
				state = 3;
				cancelOnUi();
				if (tts != null) {
					try {
						tts.shutdown();
					} catch (RuntimeException ignored) {
					}
					tts = null;
				}
				eligible.clear();
				return;
			}
			if (cancel || (stop != 0 && stop == generation))
				cancelOnUi();
			if (next != null)
				startOnUi(next.generation, next.text, next.voice, next.rate);
		}
	}
	public void start(long token, String[] text, String voice, float rate) {
		synchronized (commandLock) {
			if (closeRequested) {
				nativeFailure(owner, token, -1, "Android speech is closed");
				return;
			}
			pending = new Pending(token, text, voice, rate);
			scheduleCommands();
		}
	}
	private synchronized void startOnUi(long token, String[] text, String voiceName, float rate) {
		if (state != 1 || tts == null) {
			nativeFailure(owner, token, -1, "Android speech service unavailable");
			return;
		}
		try {
			cancelOnUi();
			Voice frozen = eligible.get(voiceName);
			if (frozen == null || text == null || text.length == 0 || token == 0) {
				nativeFailure(owner, token, -1,
				              "Android speech voice/request is no longer available");
				return;
			}
			// setVoice can load/download data. Revalidate CURRENT service metadata first.
			Set<Voice> current = tts.getVoices();
			if (current == null || current.size() > 65536) {
				nativeFailure(owner, token, -1, "Android speech voice inventory changed");
				return;
			}
			Voice voice = null;
			for (Voice candidate : current) {
				if (candidate != null && voiceName.equals(candidate.getName())) {
					Set<String> features = candidate.getFeatures();
					if (voice != null || !frozen.equals(candidate) ||
					    candidate.isNetworkConnectionRequired() || features == null ||
					    features.contains(TextToSpeech.Engine.KEY_FEATURE_NOT_INSTALLED)) {
						nativeFailure(owner, token, -1, "Android speech voice metadata changed");
						return;
					}
					voice = candidate;
				}
			}
			if (voice == null) {
				nativeFailure(owner, token, -1, "Android speech voice is no longer installed");
				return;
			}
			if (tts.setVoice(voice) != TextToSpeech.SUCCESS || tts.getVoice() == null ||
			    !voice.equals(tts.getVoice()) || tts.setSpeechRate(rate) != TextToSpeech.SUCCESS) {
				nativeFailure(owner, token, -1, "Android speech rejected exact voice/rate");
				return;
			}
			generation = token;
			segments   = text;
			segment    = 0;
			queueSegment();
		} catch (RuntimeException | OutOfMemoryError failure) {
			nativeFailure(owner, token, -1, "Android speech request failed");
			cancelOnUi();
		}
	}
	private String utterance() {
		return Long.toString(owner) + ":" + Long.toString(generation) + ":" + segment;
	}
	private boolean matches(String id) {
		return state == 1 && generation != 0 && !terminalSeen && utterance().equals(id);
	}
	private void queueSegment() {
		terminalSeen = false;
		try {
			cleanupScratch();
			scratch = File.createTempFile("ice-tts-", ".wav", scratchDirectory);
			if (!scratch.setReadable(false, false) || !scratch.setWritable(false, false) ||
			    !scratch.setReadable(true, true) || !scratch.setWritable(true, true)) {
				nativeFailure(owner, generation, -1, "Private speech scratch permissions failed");
				cancelOnUi();
				return;
			}
			// File route works at API29. A WAV synthesis destination must remain seekable.
			int queued =
			    tts.synthesizeToFile(segments[segment], new Bundle(), scratch, utterance());
			if (queued != TextToSpeech.SUCCESS) {
				nativeFailure(owner, generation, queued, "Android speech queue rejected");
				cancelOnUi();
			}
		} catch (Exception failure) {
			nativeFailure(owner, generation, -1, "Private speech scratch/queue failed");
			cancelOnUi();
		}
	}
	private void stopLater(long token) {
		synchronized (commandLock) {
			stopGeneration = token;
			scheduleCommands();
		}
	}
	private final UtteranceProgressListener listener = new UtteranceProgressListener() {
		@Override
		public void onStart(String id) {}
		@Override
		public void onBeginSynthesis(String id, int rate, int format, int channels) {
			synchronized (AndroidSpeechBridge.this) {
				if (matches(id) && !nativeBegin(owner, generation, rate, format, channels))
					stopLater(generation);
			}
		}
		@Override
		public void onAudioAvailable(String id, byte[] audio) {
			synchronized (AndroidSpeechBridge.this) {
				if (matches(id) && !nativeAudio(owner, generation, audio))
					stopLater(generation);
			}
		}
		@Override
		public void onDone(String id) {
			synchronized (AndroidSpeechBridge.this) {
				if (!matches(id))
					return;
				terminalSeen           = true;
				final long    token    = generation;
				final boolean last     = segment + 1 == segments.length;
				final boolean accepted = nativeDone(owner, token, last);
				ui.post(new Runnable() {
					@Override
					public void run() {
						synchronized (AndroidSpeechBridge.this) {
							if (generation != token)
								return;
							cleanupScratch();
							if (!accepted || last) {
								generation = 0;
								segments   = null;
							} else {
								++segment;
								queueSegment();
							}
						}
					}
				});
			}
		}
		@Override
		@SuppressWarnings("deprecation") // Abstract legacy callback must still be implemented.
		public void onError(String id) {
			onError(id, TextToSpeech.ERROR_SYNTHESIS);
		}
		@Override
		public void onError(String id, int code) {
			synchronized (AndroidSpeechBridge.this) {
				if (matches(id)) {
					terminalSeen = true;
					nativeFailure(owner, generation, code, "Android speech synthesis failed");
					stopLater(generation);
				}
			}
		}
		@Override
		public void onStop(String id, boolean interrupted) {
			synchronized (AndroidSpeechBridge.this) {
				if (matches(id)) {
					terminalSeen = true;
					nativeFailure(owner, generation, -2, "Android speech stopped");
					stopLater(generation);
				}
			}
		}
	};
	private void cleanupScratch() {
		if (scratch != null) {
			try {
				scratch.delete();
			} catch (RuntimeException ignored) {
			}
			scratch = null;
		}
	}
	private void cancelOnUi() {
		generation   = 0;
		segments     = null;
		terminalSeen = true;
		if (tts != null)
			try {
				tts.stop();
			} catch (RuntimeException ignored) {
			}
		cleanupScratch();
	}
	public void cancel() {
		synchronized (commandLock) {
			pending         = null;
			cancelRequested = true;
			scheduleCommands();
		}
	}
	public void close() {
		synchronized (commandLock) {
			pending        = null;
			closeRequested = true;
			scheduleCommands();
		}
	}
	private static native boolean nativeBegin(long owner, long generation, int rate, int format,
	                                          int channels);
	private static native boolean nativeAudio(long owner, long generation, byte[] audio);
	private static native boolean nativeDone(long owner, long generation, boolean last);
	private static native void nativeFailure(long owner, long generation, int code, String message);
}
