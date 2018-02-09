// converter.h

#ifndef PFEIFEN2_CONVERTER_H
#define PFEIFEN2_CONVERTER_H

#include "filters.h"

typedef struct audio_midi_converter_t {
	float samplerate;
	
	highpass_rc_filter* ampl_high_filter_1;
	highpass_rc_filter* ampl_high_filter_2;
	amplitudedetector_filter* ampl_ampldetect_filter;
	
	windowfunction_filter* amplitudedetector_max_filter;
	
	automaticgain_filter* in_gain_filter;
	highpass_rc_filter* in_high_filter_1;
	highpass_rc_filter* in_high_filter_2;
	lowpass_rc_filter* in_low_filter_1;
	lowpass_rc_filter* in_low_filter_2;
	
	zerocrossingdetector_filter* zerocross_filter;

	lowpass_rc_filter* out_freq_low_filter_1;	
	lowpass_rc_filter* out_freq_low_filter_2;	
	
	float ampl_last;
	float ampl_noteon;
	int notechange_mindelay;
	
	int midi_program;
	
	int pitchbend_abs_range_in_half_notes;	//how many half notes between lowest and highest pitchbend?

	char note_on;
	int last_note_pitch;
	int last_note_pitchbend;
	int last_note_velocity;
	int last_note_volume;
	int last_note_duration;	//ticks since last command
	
	int (*midi_note_on) (void* info, int time, unsigned char pitch, unsigned char velocity);
	int (*midi_note_off) (void* info, int time, unsigned char pitch);
	int (*midi_pitchbend) (void* info, int time, short pitchbend);
	int (*midi_mainvolume) (void* info, int time, unsigned char volume);
	int (*midi_programchange) (void* info, int time, unsigned char program);
} audio_midi_converter;

audio_midi_converter* audio_midi_converter_init(
	int (*midi_note_on) (void* info, int time, unsigned char pitch, unsigned char velocity),
	int (*midi_note_off) (void* info, int time, unsigned char pitch),
	int (*midi_pitchbend)(void* info, int time, short pitchbend),
	int (*midi_mainvolume)(void* info, int time, unsigned char volume),
	int (*midi_programchange)(void* info, int time, unsigned char program),
	float samplerate, float filter_min_freq, float filter_max_freq, float gain, float seconds_maxdelay, float notechange_mindelay, float out_freq_max_change, float ampl_noteon);

extern audio_midi_converter* converter;

void audio_midi_converter_process(audio_midi_converter* c, int buffer_size, float* buffer, void* midi_player_info);

#endif //PFEIFEN2_CONVERTER_H
