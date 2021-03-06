//gcc -g `pkg-config --libs --cflags jack` --std=c99 -o midi_in_decode midi_in_decode.c

#include <jack/jack.h>
#include <jack/midiport.h>
#include <stdio.h>
#include <stdlib.h>

jack_client_t *jack_client = NULL;
jack_nframes_t jack_sample_rate = 0;
jack_nframes_t jack_buffer_size = 0;
jack_port_t *jack_audio_port = NULL;
jack_port_t *jack_midi_port = NULL;
jack_port_t *jack_midi_port_in = NULL;

int jack_init() {
	jack_status_t status;
	jack_client = jack_client_open("midi_in_decode", JackNullOption, &status);
	status = status & !JackServerStarted;
	printf("status without JackServerStarted:%i\n", status);
	// TODO: Interpret and output status errors (see jack/types.h).
	return status;
}

int jack_process_callback(jack_nframes_t nframes, void *arg) {
	//printf("jack_process_callback nframes:%i\n", nframes);
/*	if (jack_audio_port) {
		float *buf = jack_port_get_buffer(jack_audio_port, jack_buffer_size);
		printf("buf:%p\n", buf);
		for (int i=0; i<nframes; i++) {
			printf("%f ", buf[i]);
		}
		printf("\n");
	}
*/	if (jack_midi_port_in) {
		//printf("jack_process_callback nframes:%i port=jack_midi_port_in\n", nframes);
		void *buf = jack_port_get_buffer(jack_midi_port_in, jack_buffer_size);
		int n_events = jack_midi_get_event_count(buf);
		//printf("n_events:%i\n", n_events);
		for (int i=0; i<n_events; i++) {
			jack_midi_event_t event;
			jack_midi_event_get(&event, buf, i);
			printf("time:%3i size:%i ", event.time, event.size);
			for (int j=0; j<event.size; j++) {
				printf("buffer[%i]:0x%2x(=%3i) ",j,event.buffer[j],event.buffer[j]);
			}
			printf("\n");
		}
		//printf("\n");
	}
	return 0;
}

int jack_setup_and_activate() {
	// get jack ready
	int ret;
	ret = jack_set_process_callback(jack_client, &jack_process_callback, NULL);
	if (ret) {perror("jack_set_process_callback"); exit(1);}
	
	// now that the callback has been registered, we can activate.
	ret = jack_activate(jack_client);
	if (ret) {perror("jack_activate"); exit(1);}
	
	jack_sample_rate = jack_get_sample_rate(jack_client);
	jack_buffer_size = jack_get_buffer_size(jack_client);
	
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
	jack_init();
	if (jack_setup_and_activate()) {
		perror("jack_setup_and_activate");
		exit(1);
	}
	
	printf("jack_sample_rate:%i jack_buffer_size:%i\n", jack_sample_rate, jack_buffer_size);
	
	jack_midi_port_in = jack_port_register(jack_client, "midi input", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	if (jack_midi_port_in == NULL) {perror("jack_port_register midi in port\n"); exit(1);}

	jack_audio_port = jack_port_register(jack_client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	if (jack_audio_port == NULL) {perror("jack_port_register audio port\n"); exit(1);}

	jack_midi_port = jack_port_register(jack_client, "output", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if (jack_midi_port == NULL) {perror("jack_port_register midi port\n"); exit(1);}

	while(1) {
		jack_nframes_t nframes;
		nframes = jack_cycle_wait(jack_client);
		//printf("jack_cycle_wait nframes:%i\n", nframes);
	}
	
	int ret;
	ret = jack_port_unregister (jack_client, jack_audio_port);
	if (!ret) {perror("jack_port_unregister");exit(1);}
	ret = jack_port_unregister (jack_client, jack_midi_port);
	if (!ret) {perror("jack_port_unregister");exit(1);}
	
	jack_close();
}

