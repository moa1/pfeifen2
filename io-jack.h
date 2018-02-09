#ifndef PFEIFEN2_IO_JACK_H
#define PFEIFEN2_IO_JACK_H

#include <jack/jack.h>
#include <jack/midiport.h>

extern int audio_processing;

extern jack_client_t *jack_client;
extern jack_nframes_t jack_sample_rate;
extern jack_nframes_t jack_buffer_size;
extern jack_port_t *jack_audio_in_port;
extern jack_port_t *jack_midi_out_port;

typedef struct jack_midi_event_info_t {
	void *port_buffer;
} jack_midi_event_info;

int jack_init();
int jack_process_callback(jack_nframes_t nframes, void *arg);
int jack_buffer_size_callback(jack_nframes_t nframes, void *arg);
int jack_sample_rate_callback(jack_nframes_t nframes, void *arg);
void jack_shutdown_callback(void* arg);
int jack_setup_and_activate();
int jack_close();
int jack_midi_note_on(void* info, int time, unsigned char pitch, unsigned char velocity);
int jack_midi_note_off(void* info, int time, unsigned char pitch);
int jack_midi_pitchbend(void* info, int time, short pitchbend);
int jack_midi_mainvolume(void* info, int time, unsigned char volume);
int jack_midi_programchange(void* info, int time, unsigned char program);

#endif//PFEIFEN2_IO_JACK_H