/* Idee: In converter.c eine limitierte Fourier-Transformation machen, indem man die Frequenz, Phase und Amplitude der lautesten Sinusschwingung mittels Zero-Crossing-Zählung bestimmt, und dann vom Originalsignal eine syntetisierte Sinusschwingung mit der bestimmten Frequenze, Phase und Amplitude abzieht. Vom Differenzsignal kann man dann wieder die lauteste Sinusschwingung bestimmen, also die global gesehen zweitlauteste Sinusschwingung. Die abwechselnden Schritte Sinusschwingungsbestimmung mittels Zero-Crossing-Zählung und Differenzsignalberechnung kann man so lange wiederholen, bis die Amplitude des Signals zu klein wird, als dass sie gehört werden kann. */

/* Idee: Die Interpretation des Pfeifenklangs abhängig machen von der Taste, die gerade gedrückt ist. Zum Beispiel könnte man bei keiner gedrückten Taste die pitchbend-funktion ausschalten, d.h. den analysierten Ton auf einen Halbton genau quantisieren, und bei gedrückter Taste die pitchbend-Funktion einschalten. Ebenso mit Lautstärke-Interpretation (bei einer anderen gedrückten Taste). Die Tasten sollten gleichzeitig drückbar sein. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "converter.h"
#include "interface.h"
#include "io-jack.h"
#include "io-alsa.h"

int audio_processing = 0;

int parameters_change(void* info, float ampl_noteon, float notechange_mindelay, float pitchbend_abs_range_in_half_notes, int midi_program) {
	// TODO: add a mutex that prevents audio_midi_converter_process from being called while we change converter parameters. And add waiting until audio_midi_converter_process is finished if it's running at the beginning of parameters_change.
	// TODO: replace this with a mutex or lock or whatever.
	while(audio_processing == 1) {};

	audio_midi_converter* converter = (audio_midi_converter*) info;
	converter->ampl_noteon = ampl_noteon;
	converter->notechange_mindelay = notechange_mindelay;
	converter->pitchbend_abs_range_in_half_notes = pitchbend_abs_range_in_half_notes;
	converter->midi_program = midi_program;
}

typedef enum sound_io_enum {
	sound_io_alsa = 0,
	sound_io_jack,
} sound_io;

int main(int argc, char** argv) {
	const sound_io default_sound_io = sound_io_alsa;
	const char* default_device_name = "hw"; 
	const int default_bufsize = 1024;
	const int default_sample_rate = 12000;
	const int default_alsa_syn_client = -1;
	const int default_alsa_syn_port = 0;
	const float default_filter_min_freq = 450.0;
	const float default_filter_max_freq = 2500.0;
	const float default_gain = 4800.0;
	const float default_seconds_maxdelay = 0.005;
	const float default_notechange_mindelay = 0.05;
	const float default_out_freq_max_change = 50.0;
	const float default_ampl_noteon = 0.01;

	sound_io sound_io = default_sound_io;
	const char* device_name = default_device_name;
	int bufsize = default_bufsize;
	int sample_rate = default_sample_rate;
	int alsa_syn_client = 128;
	int alsa_syn_port = 0;
	float filter_min_freq = default_filter_min_freq;
	float filter_max_freq = default_filter_max_freq;
	float gain = default_gain;
	float seconds_maxdelay = default_seconds_maxdelay;
	float notechange_mindelay = default_notechange_mindelay;
	float out_freq_max_change = default_out_freq_max_change;
	float ampl_noteon = default_ampl_noteon;

	for (int i = 1; i < argc; i++) {
		void show_error(const char* argname, const char* argtype) {
			fprintf(stderr, "%s [OPTIONS]\n", argv[0]);
			fprintf(stderr, "Options:\n");
			fprintf(stderr, "--alsa  Use ALSA for sound input and MIDI output (default)\n");
			fprintf(stderr, "--jack  Use JACK for sound input and MIDI output\n");
			fprintf(stderr, "--device  ALSA capture device name (default: %s)\n", default_device_name);
			fprintf(stderr, "--bufsize  ALSA buffer size (the larger, the longer the latency, default: %i)\n", default_bufsize);
			fprintf(stderr, "--rate  ALSA capture device sample rate (default: %i)\n", default_sample_rate);
			fprintf(stderr, "--client  ALSA synthesizer client number, -1 means no subscription (default: %i)\n", default_alsa_syn_client);
			fprintf(stderr, "--client-port  ALSA synthesizer client port (default: %i)\n", default_alsa_syn_port);
			fprintf(stderr, "--min-freq  highpass filter frequency (default: %.3f)\n", default_filter_min_freq);
			fprintf(stderr, "--max-freq  lowpass filter frequency (default: %.3f)\n", default_filter_max_freq);
			fprintf(stderr, "--gain  amplification gain (default: %.3f)\n", default_gain);
			fprintf(stderr, "--max-delay  ?maximum delay when a note is played? (default: %.3f)\n", default_seconds_maxdelay);
			fprintf(stderr, "--min-delay  minimal note length (default: %.3f)\n", default_notechange_mindelay);
			fprintf(stderr, "--max-change  maximal detected played notes per second (default: %.3f)\n", default_out_freq_max_change);
			fprintf(stderr, "--ampl  amplitude threshold required for a note to be played (default: %.3f)\n", default_ampl_noteon);
			if (argname) {
				fprintf(stderr, "\n");
				fprintf(stderr, "%s needs a %s as argument\n", argname, argtype);
			}
			exit(1);
		}
		int read_positive_int(const char* argname) {
			if (++i >= argc) show_error(argname, "positive int");
			char *endptr;
			int n = strtol(argv[i], &endptr, 10);
			if (*endptr != '\0') show_error(argname, "positive int");
			printf("Setting %s to %i\n", &argname[2], n);
			return n;
		}
		float read_positive_float(const char* argname) {
			if (++i >= argc) show_error(argname, "positive float");
			char *endptr;
			float f = strtof(argv[i], &endptr);
			if (*endptr != '\0') show_error(argname, "positive float");
			printf("Setting %s to %.3f\n", &argname[2], i);
			return f;
		}

		char* a = argv[i];
		if (strcmp(a, "--help") == 0) {
			show_error(NULL, 0);
		} else if(strcmp(a, "--alsa") == 0) {
			sound_io = sound_io_alsa;
		} else if (strcmp(a, "--jack") == 0) {
			sound_io = sound_io_jack;
		} else if (strcmp(a, "--device") == 0) { //only used for ALSA
			i++;
			if (i >= argc) show_error("--device", "string");
			device_name = argv[i];
		} else if (strcmp(a, "--bufsize") == 0) { //only used for ALSA
			bufsize = read_positive_int(a);
		} else if (strcmp(a, "--rate") == 0) { //only used for ALSA
			sample_rate = read_positive_int(a);
		} else if (strcmp(a, "--client") == 0) { //only used for ALSA
			alsa_syn_client = read_positive_int(a);
		} else if (strcmp(a, "--client-port") == 0) { //only used for ALSA
			alsa_syn_port = read_positive_int(a);
		} else if (strcmp(a, "--min-freq") == 0) {
			filter_min_freq = read_positive_float(a);
		} else if (strcmp(a, "--max-freq") == 0) {
			filter_max_freq = read_positive_float(a);
		} else if (strcmp(a, "--gain") == 0) {
			gain = read_positive_float(a);
		} else if (strcmp(a, "--max-delay") == 0) {
			seconds_maxdelay = read_positive_float(a);
		} else if (strcmp(a, "--min-delay") == 0) {
		    notechange_mindelay = read_positive_float(a);
		} else if (strcmp(a, "--max-change") == 0) {
		    notechange_mindelay = read_positive_float(a);
		} else if (strcmp(a, "--ampl") == 0) {
		    ampl_noteon = read_positive_float(a);
		} else {
			fprintf(stderr, "unknown parameter %s\n", a);
			return 1;
		}
	}

	int (*io_init)(const char* device_name, int bufsize);
	int (*io_setup)(int *sample_rate);
	int (*io_start)();
	int (*io_close)();
	int (*midi_note_on)(void* info, int time, unsigned char pitch, unsigned char velocity);
	int (*midi_note_off)(void* info, int time, unsigned char pitch);
	int (*midi_pitchbend)(void* info, int time, short pitchbend);
	int (*midi_mainvolume)(void* info, int time, unsigned char volume);
	int (*midi_programchange)(void* info, int time, unsigned char program);
	int (*io_process_callback)();

	if (sound_io == sound_io_alsa) {
		io_init = &alsa_init;
		io_setup = &alsa_setup;
		io_start = &alsa_start;
		io_close = &alsa_close;
		midi_note_on = &alsa_midi_note_on;
		midi_note_off = &alsa_midi_note_off;
		midi_pitchbend = &alsa_midi_pitchbend;
		midi_mainvolume = &alsa_midi_mainvolume;
		midi_programchange = &alsa_midi_programchange;
		io_process_callback = &alsa_process_callback;
		printf("Using ALSA, client=%i:%i\n", alsa_syn_client, alsa_syn_port);
	} else if (sound_io == sound_io_jack) {
		io_init = &jack_init;
		io_setup = &jack_setup;
		io_start = &jack_start;
		io_close = &jack_close;
		midi_note_on = &jack_midi_note_on;
		midi_note_off = &jack_midi_note_off;
		midi_pitchbend = &jack_midi_pitchbend;
		midi_mainvolume = &jack_midi_mainvolume;
		midi_programchange = &jack_midi_programchange;
		io_process_callback = &jack_process_callback_nothing;
		printf("Using JACK\n");
	}

	// end of parameter parsing and initializing, start of real program
	
	if ((io_init)(device_name, bufsize)) {
		fprintf(stderr, "io_init failed\n");
		return 1;
	}
	if ((io_setup)(&sample_rate)) {
		fprintf(stderr, "io_setup error\n");
		return 1;
	}
	printf("sample_rate = %i\n", sample_rate);

	audio_midi_converter* converter = audio_midi_converter_init(midi_note_on, midi_note_off, midi_pitchbend, midi_mainvolume, midi_programchange, (float)sample_rate, filter_min_freq, filter_max_freq, gain, seconds_maxdelay, notechange_mindelay, out_freq_max_change, ampl_noteon);
	if (converter == NULL) {
		fprintf(stderr, "audio_midi_converter_init error\n");
		return 1;
	}

	if ((*io_start)(alsa_syn_client, alsa_syn_port, converter)) {
		fprintf(stderr, "io_start error\n");
		return 1;
	}
	
	interface* interf = interface_init(parameters_change, converter, 0.01, 0.05, 1, 71);
	if (interf == NULL) {
		perror("Could not initialize interface!");
		return 1;
	}

	while(1) {
		if ((*io_process_callback)()) {
			fprintf(stderr, "io_process_callback error\n");
			return 1;
		}

		if (interface_process()) {
			fprintf(stderr, "interface_process error\n");
			return 1;
		}
	}

	// TODO: free
	// interface_free(interf);
	// converter_free(converter);
	
	(*io_close)();

	return 0;
}


/*
jack-keyboard press individual keys:
time:125 size:3 buffer[0]:144 buffer[1]:90 buffer[2]:74 	note90 at velocity 74
time:120 size:3 buffer[0]:128 buffer[1]:90 buffer[2]:74 	NoteOff note90
time:13 size:3 buffer[0]:144 buffer[1]:89 buffer[2]:74 		F-? at velocity 74
time:11 size:3 buffer[0]:128 buffer[1]:89 buffer[2]:74 		NoteOff F-?
time:55 size:3 buffer[0]:144 buffer[1]:89 buffer[2]:1 		F-? at velocity 1
time:39 size:3 buffer[0]:128 buffer[1]:89 buffer[2]:1 
time:48 size:3 buffer[0]:144 buffer[1]:60 buffer[2]:127		C-5 at velocity 127
time:32 size:3 buffer[0]:128 buffer[1]:60 buffer[2]:0 		NoteOff C-5
time:32 size:3 buffer[0]:144 buffer[1]:61 buffer[2]:127 	C#5 at velocity 127
time:16 size:3 buffer[0]:128 buffer[1]:61 buffer[2]:0 		NoteOff C#5
jack-keyboard increase Program from 0 to 1:					gmidimonitor says:
time:75 size:3 buffer[0]:176 buffer[1]:32 buffer[2]:0 		CC Bank selection (LSB) (32), value 0
time:75 size:3 buffer[0]:176 buffer[1]:0 buffer[2]:0 		CC Bank selection (0), value 0
time:76 size:2 buffer[0]:192 buffer[1]:1 					Program change, 1 (Bright Acoustic Piano)
jack-keyboard increase Bank from 0 to 1:
time:83 size:3 buffer[0]:176 buffer[1]:32 buffer[2]:1 		CC Bank selection (LSB) (32), value 1
time:84 size:3 buffer[0]:176 buffer[1]:0 buffer[2]:0 		CC Bank selection (0), value 0
time:84 size:2 buffer[0]:192 buffer[1]:0 					Program change, 0 (Accoustic Grand Piano)
connect VMPK using a2jmidid to gmidimonitor and midi_in_decode:
VMPK put Bender slider to full right:						gmidimonitor
time:45 size:3 buffer[0]:224 buffer[1]:127 buffer[2]:127 	Pitchwheel, 8191
time:81 size:3 buffer[0]:224 buffer[1]:0 buffer[2]:64 		Pitchwheel, 0
VMPK put Bender slider slightly to the left:
time:92 size:3 buffer[0]:224 buffer[1]:36 buffer[2]:63 		Pitchwheel, -92
time:18 size:3 buffer[0]:224 buffer[1]:0 buffer[2]:64 		Pitchwheel, 0
VMPK set Control-7 Volume to approx. middle
time:45 size:3 buffer[0]:176 buffer[1]:7 buffer[2]:63 		CC Main volume (7), value 63
time:69 size:3 buffer[0]:176 buffer[1]:7 buffer[2]:65 		CC Main volume (7), value 65
time:25 size:3 buffer[0]:176 buffer[1]:7 buffer[2]:67 		CC Main volume (7), value 67

whistling should emit the following MIDI Commands:
CC Main volume according to whistled volume.
Note On at predefined (constant?) velocity.
while whisling goes on:
	set CC Main volume according to whistling volume
	set Pitchwheel according to whistling pitch.
Note Off when whistling stops.
*/
