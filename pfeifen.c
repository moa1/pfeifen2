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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "converter.h"
#include "interface.h"
#include "io-jack.h"

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

int main() {
	if (jack_init()) {
		perror("jack_init");
		exit(1);
	}
	if (jack_setup_and_activate()) {
		perror("jack_setup_and_activate");
		exit(1);
	}

	printf("jack_sample_rate:%i jack_buffer_size:%i\n", jack_sample_rate, jack_buffer_size);

	int (*midi_note_on) (void* info, int time, unsigned char pitch, unsigned char velocity) = &jack_midi_note_on;
	int (*midi_note_off) (void* info, int time, unsigned char pitch) = &jack_midi_note_off;
	int (*midi_pitchbend) (void* info, int time, short pitchbend) = &jack_midi_pitchbend;
	int (*midi_mainvolume) (void* info, int time, unsigned char volume) = &jack_midi_mainvolume;
	int (*midi_programchange) (void* info, int time, unsigned char program) = &jack_midi_programchange;

	converter = audio_midi_converter_init(midi_note_on, midi_note_off, midi_pitchbend, midi_mainvolume, midi_programchange, jack_sample_rate, 450.0, 2500.0, 4800.0, 0.005, 0.05, 50.0, 0.01);

	interface* interf = interface_init(parameters_change, converter, 0.01, 0.05, 1, 71);
	if (interf == NULL) {
		perror("Could not initialize interface!");
		exit(1);
	}

	jack_audio_in_port = jack_port_register(jack_client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	if (jack_audio_in_port == NULL) {perror("jack_port_register audio port\n"); exit(1);}

	jack_midi_out_port = jack_port_register(jack_client, "output", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if (jack_midi_out_port == NULL) {perror("jack_port_register midi port\n"); exit(1);}

/*	while(1) {
		struct timespec t;
		t.tv_sec = 0;
		t.tv_nsec = (int) (0.01 * 1000000000);
		nanosleep(&t, NULL);

		if (interface_process() == 0) {
			break;
		}
	}
*/
	interface_process();
	
	int ret;
	ret = jack_port_unregister(jack_client, jack_audio_in_port);
	if (!ret) {perror("jack_port_unregister error");exit(1);}
	ret = jack_port_unregister(jack_client, jack_midi_out_port);
	if (!ret) {perror("jack_port_unregister error");exit(1);}
	
	// interface_free(interf);
	// converter_free(converter);
	
	jack_close();
}

