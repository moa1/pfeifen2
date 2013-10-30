// converter.h

#include "filters.h"

typedef struct audio_midi_converter_t {
	float samplerate;
	
	automaticgain_filter* in_gain_filter;
	highpass_rc_filter* in_high_filter_1;
	highpass_rc_filter* in_high_filter_2;
	lowpass_rc_filter* in_low_filter_1;
	lowpass_rc_filter* in_low_filter_2;
	
	zerocrossingdetector_filter* zerocross_filter;
	
	lowpass_rc_filter* ampl_low_filter_1;
	lowpass_rc_filter* ampl_low_filter_2;
	amplitudedetector_filter* ampl_ampldetect_filter;

	char last_note_on;
	int last_note_note;
	int last_note_pitch;
	int last_note_velocity;
	int last_note_volume;
} audio_midi_converter;

audio_midi_converter* audio_midi_converter_init(float samplerate, float low_freq, float high_freq, float gain);
void audio_midi_converter_process(audio_midi_converter* c, int buffer_size, float* buffer);

