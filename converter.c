// converter.c

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "converter.h"

float window_max(int windowlen, float* window) {
	float max = window[0];
	for (int i=1; i<windowlen; i++)
		if (window[i] > max)
			max = window[i];
	return max;
}

audio_midi_converter* audio_midi_converter_init(
	int (*midi_note_on) (void* info, int time, unsigned char pitch, unsigned char velocity),
	int (*midi_note_off) (void* info, int time, unsigned char pitch),
	int (*midi_pitchbend)(void* info, int time, short pitchbend),
	int (*midi_mainvolume)(void* info, int time, unsigned char volume),
	float samplerate, float filter_min_freq, float filter_max_freq, float gain, float seconds_maxdelay, float out_freq_max_change, float ampl_noteon)
{
	audio_midi_converter* c = malloc(sizeof(audio_midi_converter));
	
	c->samplerate = samplerate;
	
	c->ampl_high_filter_1 = highpass_rc_filter_init(filter_min_freq, samplerate);
	c->ampl_high_filter_2 = highpass_rc_filter_init(filter_min_freq, samplerate);
	c->ampl_ampldetect_filter = amplitudedetector_filter_init();

	c->amplitudedetector_max_filter = windowfunction_filter_init(seconds_maxdelay, samplerate, window_max);
	
	c->in_gain_filter = automaticgain_filter_init(gain, samplerate);
	c->in_high_filter_1 = highpass_rc_filter_init(filter_min_freq, samplerate);
	c->in_high_filter_2 = highpass_rc_filter_init(filter_min_freq, samplerate);
	c->in_low_filter_1 = lowpass_rc_filter_init(filter_max_freq, samplerate);
	c->in_low_filter_2 = lowpass_rc_filter_init(filter_max_freq, samplerate);
	
	c->zerocross_filter = zerocrossingdetector_filter_init(samplerate);

	c->out_freq_low_filter_1 = lowpass_rc_filter_init(out_freq_max_change, samplerate);
	c->out_freq_low_filter_2 = lowpass_rc_filter_init(out_freq_max_change, samplerate);
	
	c->ampl_last = 0.0;
	c->ampl_noteon = ampl_noteon;
	
	c->pitchbend_abs_range_in_half_notes = 4;
	
	c->note_on = 0;
	c->last_note_pitch = -1;
	c->last_note_pitchbend = -1;
	c->last_note_velocity = -1;
	c->last_note_volume = -1;
	
	c->midi_note_on = midi_note_on;
	c->midi_note_off = midi_note_off;
	c->midi_pitchbend = midi_pitchbend;
	c->midi_mainvolume = midi_mainvolume;
}

float loghnf(float x) {
	const float half_note_factor = powf(2.0, 1.0/12);
	const float loghnf = logf(half_note_factor);
	return logf(x) / loghnf;
}

void freq_to_midi_pitch(float freq, int* midi_pitch, int* midi_pitchbend, int pitchbend_abs_range_in_half_notes) {
	// Wikipedia "Middle C": 261.6 Hz, In MIDI, it is note number 60, C-4.
	// Therefore, middle A is 440.0 Hz, MIDI note number 69, A-4.
	const float loghnf_440 = loghnf(440);
	float float_half_notes_from_middle_a = loghnf(freq) - loghnf_440;
	int half_notes_from_middle_a = (int)floorf(float_half_notes_from_middle_a + 0.5);
	*midi_pitch = half_notes_from_middle_a + 69;
	
	// the pitchbend goes from -8192 to 8191.
	// by default, -8192 means 2 half tones lower, 8191 means 2 half tones higher, and -4096 means 1 half tone lower.
	float pitchbend_in_half_notes = float_half_notes_from_middle_a - (float)half_notes_from_middle_a;
	float float_midi_pitchbend = (pitchbend_in_half_notes / pitchbend_abs_range_in_half_notes * 2) * 8192;
	*midi_pitchbend = (int)floorf(float_midi_pitchbend + 0.5);
	if (*midi_pitchbend > 8191)
		*midi_pitchbend = 8191;
	else if (*midi_pitchbend < -8192)
		*midi_pitchbend = -8192;
}

// process the audio data
void audio_midi_converter_process(audio_midi_converter* c, int buffer_size, float* buffer, void* midi_player_info) {
	for (int i=0; i<buffer_size; i++) {
		float x = buffer[i];
		
		float ampl = highpass_rc_filter_next(c->ampl_high_filter_1, x);
		ampl = highpass_rc_filter_next(c->ampl_high_filter_2, ampl);
		ampl = amplitudedetector_filter_next(c->ampl_ampldetect_filter, ampl);
		
		float amplmax = windowfunction_filter_next(c->amplitudedetector_max_filter, ampl);
		
		float input = automaticgain_filter_next(c->in_gain_filter, x);
		input = highpass_rc_filter_next(c->in_high_filter_1, input);
		input = highpass_rc_filter_next(c->in_high_filter_2, input);
		input = lowpass_rc_filter_next(c->in_low_filter_2, input);
		input = lowpass_rc_filter_next(c->in_low_filter_2, input);
				
		float freq = zerocrossingdetector_filter_next(c->zerocross_filter, input);
		
		float track_ampl = c->ampl_noteon / 2;
		if (c->ampl_last < track_ampl && amplmax >= track_ampl) {
			c->out_freq_low_filter_1->lasty = freq;
			c->out_freq_low_filter_2->lasty = freq;
		}
		freq = lowpass_rc_filter_next(c->out_freq_low_filter_1, freq);
		freq = lowpass_rc_filter_next(c->out_freq_low_filter_2, freq);
		
		if (c->ampl_last < c->ampl_noteon && amplmax >= c->ampl_noteon) {
			c->note_on = 1;
			int last_note_pitch;
			int last_note_pitchbend;
			freq_to_midi_pitch(freq, &last_note_pitch, &last_note_pitchbend, c->pitchbend_abs_range_in_half_notes);
			c->last_note_pitch = last_note_pitch;
			c->last_note_pitchbend = last_note_pitchbend;
			c->last_note_velocity = 63; //TODO: ampl derivative is velocity
			c->last_note_volume = 63;
			
			(c->midi_note_on)(midi_player_info, i, c->last_note_pitch, c->last_note_velocity);
			(c->midi_pitchbend)(midi_player_info, i, c->last_note_pitchbend);
			(c->midi_mainvolume)(midi_player_info, i, c->last_note_volume);
		} else if (c->ampl_last >= c->ampl_noteon && amplmax < c->ampl_noteon) {
			(c->midi_note_off)(midi_player_info, i, c->last_note_pitch);
		}
		
		c->ampl_last = amplmax;
	}
	
}

