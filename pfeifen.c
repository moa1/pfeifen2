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
connect VMPK using a2jmidid to gmidimonitor and pfeifen:
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

#include <jack/jack.h>
#include <jack/midiport.h>
#include <stdio.h>
#include <stdlib.h>
#include "converter.h"

jack_client_t *jack_client = NULL;
jack_nframes_t jack_sample_rate = 0;
jack_nframes_t jack_buffer_size = 0;
jack_port_t *jack_audio_port = NULL;
jack_port_t *jack_midi_port = NULL;
jack_port_t *jack_midi_port_in = NULL;

audio_midi_converter* converter = NULL;

int jack_init() {
	jack_status_t status;
	jack_client = jack_client_open("pfeifen", JackNullOption, &status);
	status = status & !JackServerStarted;
	printf("status without JackServerStarted:%i\n", status);
	// TODO: Interpret and output status errors (see jack/types.h).
	return status;
}

int jack_process_callback(jack_nframes_t nframes, void *arg) {
	//printf("jack_process_callback nframes:%i\n", nframes);
	if (jack_audio_port) {
		float *buf = jack_port_get_buffer(jack_audio_port, jack_buffer_size);
		audio_midi_converter_process(converter, nframes, buf);
	}
	return 0;
}

int jack_buffer_size_callback(jack_nframes_t nframes, void *arg) {
	printf("jack_buffer_size_callback\n");
	jack_buffer_size = nframes;
	return 0;
}

int jack_sample_rate_callback(jack_nframes_t nframes, void *arg) {
	printf("jack_sample_rate_callback\n");
	jack_sample_rate = nframes;
	return 0;
}

int jack_setup_and_activate() {
	// get jack ready
	int ret;
	ret = jack_set_process_callback(jack_client, &jack_process_callback, NULL);
	if (ret) {perror("jack_set_process_callback"); exit(1);}
	
	ret = jack_set_buffer_size_callback(jack_client, &jack_buffer_size_callback, NULL);
	if (ret) {perror("jack_set_buffer_size_callback"); exit(1);}

	ret = jack_set_sample_rate_callback (jack_client, &jack_sample_rate_callback, NULL);
	if (ret) {perror("jack_set_sample_rate_callback"); exit(1);}

	/*
	do we need a latency callback?
	jack.h: "More complex clients that take an input signal,"
	int jack_set_latency_callback (jack_client_t *client,
                               JackLatencyCallback latency_callback,
                               void *)
	*/

	// before activating, set variables we need to know.
	jack_sample_rate = jack_get_sample_rate(jack_client);
	jack_buffer_size = jack_get_buffer_size(jack_client);
	
	// now that all the callbacks have been registered, we can activate.
	ret = jack_activate(jack_client);
	if (ret) {perror("jack_activate"); exit(1);}
	
	return 0;
}


int jack_close() {
	int ret;
	ret = jack_deactivate(jack_client);
	if (ret == 0) {
		ret = jack_client_close(jack_client);
	}
	return ret;
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

	converter = audio_midi_converter_init(jack_sample_rate, 80.0, 2500.0, 4800.0);
	
	jack_midi_port_in = jack_port_register(jack_client, "midi input", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	if (jack_midi_port_in == NULL) {perror("jack_port_register midi in port\n"); exit(1);}

	jack_audio_port = jack_port_register(jack_client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	if (jack_audio_port == NULL) {perror("jack_port_register audio port\n"); exit(1);}

	jack_midi_port = jack_port_register(jack_client, "output", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if (jack_midi_port == NULL) {perror("jack_port_register midi port\n"); exit(1);}

	while(1) {
		jack_nframes_t nframes;
		nframes = jack_cycle_wait(jack_client);
		printf("jack_cycle_wait nframes:%i\n", nframes);
	}
	
	int ret;
	ret = jack_port_unregister (jack_client, jack_audio_port);
	if (!ret) {perror("jack_port_unregister");exit(1);}
	ret = jack_port_unregister (jack_client, jack_midi_port);
	if (!ret) {perror("jack_port_unregister");exit(1);}
	
	// converter_free(converter);
	
	jack_close();
}

