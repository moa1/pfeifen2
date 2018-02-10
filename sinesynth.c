/* miniFMsynth 1.1 by Matthias Nagorni    */
/* This program uses callback-based audio */
/* playback as proposed by Paul Davis on  */
/* the linux-audio-dev mailinglist.       */

#define _ISO_C99_SOURCE
#include <stdio.h>   
#include <stdlib.h>
#include <alloca.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <inttypes.h>

#define M_PI 3.14159265358979

#define BUFSIZE 512

snd_seq_t *seq_handle;
snd_pcm_t *playback_handle;
short *buf;
int rate;
int64_t tick;
int note;
float pitch, velocity;

snd_seq_t *open_seq() {

    snd_seq_t *seq_handle;

	tick = 0;
    if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "Error opening ALSA sequencer.\n");
        exit(1);
    }
    snd_seq_set_client_name(seq_handle, "sinesynth");
    if (snd_seq_create_simple_port(seq_handle, "sinesynth",
        SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION) < 0) {
        fprintf(stderr, "Error creating sequencer port.\n");
        exit(1);
    }
    return(seq_handle);
}

snd_pcm_t *open_pcm(char *pcm_name) {
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
            
    if (snd_pcm_open (&playback_handle, pcm_name, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf (stderr, "cannot open audio device %s\n", pcm_name);
        exit (1);
    }
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(playback_handle, hw_params);
    snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0);
	fprintf(stderr, "sample rate=%i\n", rate);
    snd_pcm_hw_params_set_channels(playback_handle, hw_params, 2);
    snd_pcm_hw_params_set_periods(playback_handle, hw_params, 2, 0);
    snd_pcm_hw_params_set_period_size(playback_handle, hw_params, BUFSIZE, 0);
    snd_pcm_hw_params(playback_handle, hw_params);
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(playback_handle, sw_params);
    snd_pcm_sw_params_set_avail_min(playback_handle, sw_params, BUFSIZE);
    snd_pcm_sw_params(playback_handle, sw_params);
    return(playback_handle);
}

int midi_callback() {

    snd_seq_event_t *ev;
    int l1;
	int e;
  
    do {
        while ((e = snd_seq_event_input(seq_handle, &ev)) < 0) {
			fprintf(stderr, "snd_seq_event_input error: %s\n", snd_strerror(e));
		}
        switch (ev->type) {
            case SND_SEQ_EVENT_PITCHBEND:
                pitch = (double)ev->data.control.value / 8192.0;
				printf("pitchbend pitch=%f\n", pitch);
                break;
            case SND_SEQ_EVENT_CONTROLLER:
				/*if (ev->data.control.param == 1) {
                    modulation = (double)ev->data.control.value / 10.0;
                }
				*/
                break;
            case SND_SEQ_EVENT_NOTEON:
				note = ev->data.note.note;
				velocity = ev->data.note.velocity / 127.0;
                break;        
            case SND_SEQ_EVENT_NOTEOFF:
				velocity = 0.0;
                break;        
        }
        snd_seq_free_event(ev);
    } while (snd_seq_event_input_pending(seq_handle, 0) > 0);
    return (0);
}

int playback_callback (snd_pcm_sframes_t nframes) {
	if (velocity > 0) {
		printf("tick=%i note=%i velocity=%f pitch=%f\n", tick, note, velocity, pitch);
	}
	for (int i = 0; i < nframes; i++) {
		int n = note;
		float f = exp(((float)n - 69) / 12 * log(2)) * 440;
		int t = tick + i;
		float a = ((1 << 15) - 1) * velocity;
		float x = sin(((float)t) / rate * f * 2 * M_PI) * a;
		//printf("n=%i t=%i f=%f x=%f\n", n , t, f, x);
		buf[i * 2] = x;
		buf[i * 2 + 1] = x;
	}
	tick += nframes;
    return snd_pcm_writei(playback_handle, buf, nframes); 
}
      
int main (int argc, char *argv[]) {

    int pcm_nfds, seq_nfds, l1;
    struct pollfd *pfds;
    
    if (argc != 2) {
        fprintf(stderr, "sinesynth DEVICE\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "DEVICE  PCM device\n");
        exit(1);
    }
    buf = (short *) malloc (2 * sizeof (short) * BUFSIZE);
    rate = 48000;
	//rate = 10000; //on eckplatz2, this doesn't use recognizably less CPU than rate = 48000
    playback_handle = open_pcm(argv[1]);
    seq_handle = open_seq();
    seq_nfds = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
    pcm_nfds = snd_pcm_poll_descriptors_count(playback_handle);
    pfds = (struct pollfd *)alloca(sizeof(struct pollfd) * (seq_nfds + pcm_nfds));
    snd_seq_poll_descriptors(seq_handle, pfds, seq_nfds, POLLIN);
    snd_pcm_poll_descriptors(playback_handle, pfds+seq_nfds, pcm_nfds);

    while (1) {
        if (poll (pfds, seq_nfds + pcm_nfds, 1000) > 0) {
            for (l1 = 0; l1 < seq_nfds; l1++) {
				if (pfds[l1].revents > 0) {
					midi_callback();
				}
            }
            for (l1 = seq_nfds; l1 < seq_nfds + pcm_nfds; l1++) {    
                if (pfds[l1].revents > 0) { 
                    if (playback_callback(BUFSIZE) < BUFSIZE) {
                        fprintf (stderr, "xrun !\n");
                        snd_pcm_prepare(playback_handle);
                    }
                }
            }
        }
    }
    snd_pcm_close (playback_handle);
    snd_seq_close (seq_handle);
    free(buf);
    return (0);
}
