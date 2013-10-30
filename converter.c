// converter.c

#include <stdlib.h>
#include <stdio.h>
#include "converter.h"

audio_midi_converter* audio_midi_converter_init(float samplerate, float low_freq, float high_freq, float gain) {
	audio_midi_converter* c = malloc(sizeof(audio_midi_converter));
	
	c->samplerate = samplerate;
	
	c->in_gain_filter = automaticgain_filter_init(gain, samplerate);
	c->in_high_filter_1 = highpass_rc_filter_init(low_freq, samplerate);
	c->in_high_filter_2 = highpass_rc_filter_init(low_freq, samplerate);
	c->in_low_filter_1 = lowpass_rc_filter_init(high_freq, samplerate);
	c->in_low_filter_2 = lowpass_rc_filter_init(high_freq, samplerate);
	
	c->zerocross_filter = zerocrossingdetector_filter_init(samplerate);
	
	c->ampl_low_filter_1 = lowpass_rc_filter_init(high_freq, samplerate);
	c->ampl_low_filter_2 = lowpass_rc_filter_init(high_freq, samplerate);
	c->ampl_ampldetect_filter = amplitudedetector_filter_init();
	
	c->last_note_on = 0;
	c->last_note_note = -1;
	c->last_note_pitch = -1;
	c->last_note_velocity = -1;
	c->last_note_volume = -1;
}

// process the audio data
void audio_midi_converter_process(audio_midi_converter* c, int buffer_size, float* buffer) {
	for (int i=0; i<buffer_size; i++) {
		float x = buffer[i];
		
		float y_input = automaticgain_filter_next(c->in_gain_filter, x);
		y_input = highpass_rc_filter_next(c->in_high_filter_1, y_input);
		y_input = highpass_rc_filter_next(c->in_high_filter_2, y_input);
		y_input = lowpass_rc_filter_next(c->in_low_filter_2, y_input);
		y_input = lowpass_rc_filter_next(c->in_low_filter_2, y_input);
		
		float freq = zerocrossingdetector_filter_next(c->zerocross_filter, y_input);
		
		float y_ampl = lowpass_rc_filter_next(c->ampl_low_filter_1, x);
		y_ampl = lowpass_rc_filter_next(c->ampl_low_filter_2, y_ampl);
		float ampl = amplitudedetector_filter_next(c->ampl_ampldetect_filter, y_ampl);
		
		if (ampl > 0.1) {
			if (freq != -1.0) {
				printf("ampl=%f freq=%f\n", ampl, freq);
			}
		}
	}
	
}

