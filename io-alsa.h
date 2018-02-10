#ifndef PFEIFEN2_IO_ALSA_H
#define PFEIFEN2_IO_ALSA_H

#include <alsa/asoundlib.h>

int alsa_init(const char* device_name, int bufsize);
int alsa_setup(int *sample_rate);
int alsa_start(int syn_client1, int syn_port1, audio_midi_converter* converter);
int alsa_process_callback();
int alsa_close();
int alsa_midi_note_on(void* info, int time, unsigned char pitch, unsigned char velocity);
int alsa_midi_note_off(void* info, int time, unsigned char pitch);
int alsa_midi_pitchbend(void* info, int time, short pitchbend);
int alsa_midi_mainvolume(void* info, int time, unsigned char volume);
int alsa_midi_programchange(void* info, int time, unsigned char program);

#endif//PFEIFEN2_IO_ALSA_H
