#include <stdio.h>
#include "converter.h"
#include "io-jack.h"


jack_client_t *jack_client = NULL;
jack_nframes_t jack_sample_rate = 0;
jack_nframes_t jack_buffer_size = 0;
jack_port_t *jack_audio_in_port = NULL;
jack_port_t *jack_midi_out_port = NULL;


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
	if (jack_audio_in_port && jack_midi_out_port) {
		audio_processing = 1;

		jack_default_audio_sample_t *audio_buf = jack_port_get_buffer(jack_audio_in_port, nframes);
		unsigned char *midi_out_buf = jack_port_get_buffer(jack_midi_out_port, nframes);
		jack_midi_clear_buffer(midi_out_buf);
		jack_midi_event_info info;
		info.port_buffer = midi_out_buf;
		audio_midi_converter_process(converter, nframes, audio_buf, &info);

		audio_processing = 0;
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

void jack_shutdown_callback(void* arg) {
	printf("jack_shutdown_callback\n");
	exit(1);
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
	
	jack_on_shutdown(jack_client, jack_shutdown_callback, NULL);
	
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

int jack_midi_note_on(void* info, int time, unsigned char pitch, unsigned char velocity) {
	jack_midi_event_info* i = (jack_midi_event_info*) info;
	
//	printf("jack_midi_note_on time=%i pitch=%i velocity=%i\n", time, pitch, velocity);
	
	jack_midi_data_t buffer[3];
	buffer[0] = 0x90;
	buffer[1] = pitch;
	buffer[2] = velocity;

	int ret = jack_midi_event_write(i->port_buffer, time, buffer, 3);
	return ret;
}

int jack_midi_note_off(void* info, int time, unsigned char pitch) {
	jack_midi_event_info* i = (jack_midi_event_info*) info;
	
//	printf("jack_midi_note_off time=%i pitch=%i\n", time, pitch);

	jack_midi_data_t buffer[3];
	buffer[0] = 0x80;
	buffer[1] = pitch;
	buffer[2] = 0;

	return jack_midi_event_write(i->port_buffer, time, buffer, 3);
}

int jack_midi_pitchbend(void* info, int time, short pitchbend) {
	jack_midi_event_info* i = (jack_midi_event_info*) info;
	
	// pitchbend is from -8192 to 8191.
	if (pitchbend < -8192 || pitchbend > 8191) {
		printf("pitchbend out of range:%i\n", pitchbend);
		exit(1);
	}
	jack_midi_data_t buffer[3];
	buffer[0] = 0xe0;
	buffer[1] = (pitchbend + 8192) & 0x7f;
	buffer[2] = (pitchbend + 8192) >> 7;

	return jack_midi_event_write(i->port_buffer, time, buffer, 3);
}

int jack_midi_mainvolume(void* info, int time, unsigned char volume) {
	jack_midi_event_info* i = (jack_midi_event_info*) info;
	
	jack_midi_data_t buffer[3];
	buffer[0] = 0xb0;
	buffer[1] = 7;
	buffer[2] = volume;

	return jack_midi_event_write(i->port_buffer, time, buffer, 3);
}

int jack_midi_programchange(void* info, int time, unsigned char program) {
	jack_midi_event_info* i = (jack_midi_event_info*) info;
	
	jack_midi_data_t buffer[2];
	buffer[0] = 0xc0;
	buffer[1] = program;

	return jack_midi_event_write(i->port_buffer, time, buffer, 2);
}
