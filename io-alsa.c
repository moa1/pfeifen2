#include <stdio.h>
#include <stdint.h>
#include "converter.h"
#include "io-alsa.h"

snd_pcm_t *pcm_handle;
snd_seq_t *seq_handle;
int seq_port;
int seq_channel = 0;
int seq_program = 0;
int syn_client;
int syn_port;
int queue_id; //TODO: use a queue to set the time stamp of MIDI events correctly. Currently they are set to 0 via `snd_seq_ev_clear`.
int device_sample_rate;
int target_sample_rate;
audio_midi_converter* alsa_converter;

#define ALSA_CHANNELS 2

unsigned int input_bufsize;
int16_t *bufarea_s16;
float *bufarea_float;

int alsa_init(const char* device_name, int bufsize) {
	int e;

	// check available devices using 'aplay -L'.
	if ((e = snd_pcm_open(&pcm_handle, device_name, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf(stderr, "Cannot open capture audio device %s\n%s\n", device_name, snd_strerror(e));
	    return 1;
	}

	{
		int set_bits = 0;
		int temp = bufsize;
		while(temp > 0) {
			if ((temp & 1) == 1) set_bits++;
			temp = temp >> 1;
		}
		if (set_bits != 1) {
			fprintf(stderr, "input_bufsize must be a power of 2, but is %i\n", bufsize);
			return 1;
		}
	}
	input_bufsize = bufsize;
	
	if ((bufarea_s16 = malloc(sizeof(int16_t) * bufsize * ALSA_CHANNELS)) == NULL) {
		fprintf(stderr, "malloc: cannot allocate buffer\n");
		return 1;
	}
	memset(bufarea_s16, 0, sizeof(int16_t) * bufsize * ALSA_CHANNELS);
	if ((bufarea_float = malloc(sizeof(float) * bufsize * 1)) == NULL) {
		fprintf(stderr, "malloc: cannot allocate buffer\n");
		return 1;
	}
	memset(bufarea_float, 0, sizeof(float) * bufsize * 1);

	return 0;
}

int alsa_setup_input(int *sample_rate) {
	int e;
	snd_pcm_hw_params_t *hw_params;

	if ((e = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		fprintf(stderr, "snd_pcm_hw_params_malloc error\n%s\n", snd_strerror(e));
		return 1;
	}

	if ((e = snd_pcm_hw_params_any(pcm_handle, hw_params)) < 0) {
		fprintf(stderr, "snd_pcm_hw_params_any error\n%s\n", snd_strerror(e));
		return 1;
	}

	if ((e = snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_access error\n%s\n", snd_strerror(e));
		return 1;
	}

	if ((e = snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16)) < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_format error: cannot set sample format\n%s\n", snd_strerror(e));
		return 1;
	}

	if ((e = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, sample_rate, 0)) < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_rate_near error: cannot set sampling rate\n%s\n", snd_strerror(e));
		fprintf(stderr, "returned sample_rate = %i\n", *sample_rate);
		return 1;
	}
	device_sample_rate = *sample_rate;
	while(*sample_rate >= 8000) {
		*sample_rate /= 2;
	}
	*sample_rate *= 2;
	target_sample_rate = *sample_rate;

	if ((e = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, ALSA_CHANNELS)) < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_channels error\n%s\n", snd_strerror(e));
		return 1;
	}

	if ((e = snd_pcm_hw_params(pcm_handle, hw_params)) < 0) {
		fprintf(stderr, "snd_pcm_hw_params error\n%s\n", snd_strerror(e));
		return 1;
	}

	snd_pcm_hw_params_free(hw_params);

	if ((e = snd_pcm_prepare(pcm_handle)) < 0) {
		fprintf(stderr, "snd_pcm_prepare error\n%s\n", snd_strerror(e));
		return 1;
	}

/*	if (snd_pcm_state(pcm_handle) != SND_PCM_STATE_RUNNING) {
		fprintf(stderr, "snd_pcm_state() != SND_PCM_STATE_RUNNING\n");
		return 1;
	}*/

	return 0;
}

int alsa_setup_output() {
	int e;
	if ((e = snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_OUTPUT, 0)) < 0) {
		fprintf(stderr, "snd_seq_open error:\n%s\n", snd_strerror(e));
		return 1;
	}

	if ((e = snd_seq_set_client_name(seq_handle, "pfeifen2")) < 0) {
		fprintf(stderr, "snd_seq_set_client_name error:\n%s\n", snd_strerror(e));
		return 1;
	}

/*	if ((queue_id = snd_seq_alloc_queue(seq_handle)) < 0) {
		fprintf(stderr, "snd_seq_alloc_queue error:\n%s\n", snd_strerror(e));
		return 1;
	}
	int seq_len = 64;
	snd_seq_set_client_pool_output(seq_handle, (seq_len<<1) + 4);*/
	
	return 0;
}

int alsa_setup(int *sample_rate) {
	if (alsa_setup_input(sample_rate)) {
		return 1;
	}
	if (alsa_setup_output()) {
		return 1;
	}
	return 0;
}

void alsa_send_event(snd_seq_event_t *ev) {
	int e;
	snd_seq_ev_set_direct(ev);
	snd_seq_ev_set_source(ev, seq_port);
	snd_seq_ev_set_dest(ev, syn_client, syn_port);
	//snd_seq_ev_set_subs(ev); //what's the difference to snd_seq_set_dest?
	snd_seq_event_output(seq_handle, ev);
	if ((e = snd_seq_drain_output(seq_handle)) < 0) {
		fprintf(stderr, "snd_seq_drain_output error\n%s\n", snd_strerror(e));
		exit(1);
	}
}	

int alsa_start(int syn_client1, int syn_port1, audio_midi_converter* converter) {
	alsa_converter = converter;

	int e;
	
	if (syn_client1 >= 0) {
		syn_client = syn_client1; //check available clients using 'aconnect -l'
		syn_port = syn_port1;
	} else {
		syn_client = SND_SEQ_ADDRESS_SUBSCRIBERS; //do not connect
		syn_port = syn_port1;
	}
	
	unsigned int caps = SND_SEQ_PORT_CAP_READ;
	if (syn_client == SND_SEQ_ADDRESS_SUBSCRIBERS)
		caps |= SND_SEQ_PORT_CAP_SUBS_READ;
	if ((seq_port = snd_seq_create_simple_port(seq_handle, "instrument", caps, SND_SEQ_PORT_TYPE_MIDI_GENERIC|SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
		fprintf(stderr, "snd_seq_create_simple_port error:\n%s\n", snd_strerror(e));
		return 1;
	}
	
	if (syn_client != SND_SEQ_ADDRESS_SUBSCRIBERS) {
		if ((e = snd_seq_connect_to(seq_handle, seq_port, syn_client, syn_port)) < 0) {
			fprintf(stderr, "snd_seq_connect_to error:\n%s\n", snd_strerror(e));
			return 1;
		}
	}

	// set MIDI instrument
	int bank = 0;
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_controller(&ev, 0, 0, bank);
	alsa_send_event(&ev);

	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_pgmchange(&ev, seq_channel, seq_program);
	alsa_send_event(&ev);

	return 0;
}

int alsa_close() {
	int e;
	if ((e = snd_seq_close(seq_handle)) < 0) {
		fprintf(stderr, "snd_seq_close error\n%s\n", snd_strerror(e));
		return 1;
	}
	
	if ((e = snd_pcm_close(pcm_handle)) < 0) {
		fprintf(stderr, "snd_pcm_close error\n%s\n", snd_strerror(e));
		return 1;
	}

	free(bufarea_s16);
	free(bufarea_float);

	return 0;
}

int alsa_process_callback() {
	int e = 0;
	while(e < input_bufsize) {
		while ((e = snd_pcm_readi(pcm_handle, &bufarea_s16[e], input_bufsize - e)) < 0) {
			switch (e) {
				case -EPIPE:
					e = snd_pcm_prepare(pcm_handle);
					break;
				default:
					fprintf(stderr, "snd_pcm_readi error\n%s\n", snd_strerror(e));
					return 1;
			}
		}
		if (e < input_bufsize) {
			fprintf(stderr, "snd_pcm_readi returned only %i of %i frames", e, input_bufsize);
		}
	}
	for (int i=0;i<input_bufsize;i++) {
		// reduce audio data to one channel of floats
		bufarea_float[i] = ((float)bufarea_s16[i*2]) / (1<<15);
	}
	// TODO: `bufarea_float` is sampled at sample rate `device_sample_rate`. Build the average of two adjacent floats in `bufarea_float`, until the sample_rate of the resulting array is equal to `target_sample_rate`. To do this, we will have to check that the `input_bufsize` does not become smaller than 1. This will decrease CPU usage, because the filters in `audio_midi_converter_process` will have to be run on fewer samples.
	audio_midi_converter_process(alsa_converter, input_bufsize, bufarea_float, NULL);

	return 0;
}

int alsa_midi_note_on(void* info, int time, unsigned char pitch, unsigned char velocity) {
	//printf("noteon time=%i pitch=%i velocity=%i\n", time, pitch, velocity);
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_noteon(&ev, 0x90, pitch, velocity);
	alsa_send_event(&ev);
	return 0;
}

int alsa_midi_note_off(void* info, int time, unsigned char pitch) {
	//printf("noteoff time=%i pitch=%i\n", time, pitch);
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_noteoff(&ev, 0x90, pitch, 0);
	alsa_send_event(&ev);
	return 0;
}

int alsa_midi_pitchbend(void* info, int time, short pitchbend) {
	//printf("pitchbend time=%i pitchbend=%i\n", time, pitchbend);
	// pitchbend is from -8192 to 8191.
	if (pitchbend < -8192 || pitchbend > 8191) {
		printf("pitchbend out of range:%i\n", pitchbend);
		exit(1);
	}
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_pitchbend(&ev, 0, pitchbend);
	alsa_send_event(&ev);
	return 0;
}

// article about the 4 MIDI volume settings: https://midi-tutor.proboards.com/thread/12/9-controlling-midi-volume
int alsa_midi_mainvolume(void* info, int time, unsigned char volume) {
/*	alsa_midi_event_info* i = (alsa_midi_event_info*) info;
	
	alsa_midi_data_t buffer[3];
	buffer[0] = 0xb0;
	buffer[1] = 7;
	buffer[2] = volume;

	return alsa_midi_event_write(i->port_buffer, time, buffer, 3);*/
}

int alsa_midi_programchange(void* info, int time, unsigned char program) {
/*	alsa_midi_event_info* i = (alsa_midi_event_info*) info;
	
	alsa_midi_data_t buffer[2];
	buffer[0] = 0xc0;
	buffer[1] = program;

	return alsa_midi_event_write(i->port_buffer, time, buffer, 2);*/
}

