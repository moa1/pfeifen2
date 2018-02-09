// converter.c

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "converter.h"


audio_midi_converter* converter = NULL;


float window_max(int windowlen, float* window) {
	float max = window[0];
	for (int i=1; i<windowlen; i++)
		if (window[i] > max)
			max = window[i];
	return max;
}

#ifdef DEBUG_CONVERTER
writer_filter* debug_input_raw;
writer_filter* debug_input;
writer_filter* debug_ampl;
writer_filter* debug_freq_raw;
writer_filter* debug_freq_filtered;
writer_filter* debug_note;
#endif

#ifdef PRINTD
#include <stdarg.h>
void printd(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
/*	void format_string(const char* fmt, va_list argptr, char* string);
	char string[1024];
	va_list args;
	va_start(args, fmt);
	format_string(fmt, args, string);
	va_end(args);
	fprintf(stdout, "%s", string);*/
}
#else
void printd(const char* fmt, ...) {
}
#endif

audio_midi_converter* audio_midi_converter_init(
	int (*midi_note_on) (void* info, int time, unsigned char pitch, unsigned char velocity),
	int (*midi_note_off) (void* info, int time, unsigned char pitch),
	int (*midi_pitchbend)(void* info, int time, short pitchbend),
	int (*midi_mainvolume)(void* info, int time, unsigned char volume),
	int (*midi_programchange)(void* info, int time, unsigned char program),
	float samplerate, float filter_min_freq, float filter_max_freq, float gain, float seconds_maxdelay, float notechange_mindelay, float out_freq_max_change, float ampl_noteon)
{

#ifdef DEBUG_CONVERTER
	debug_input_raw = writer_filter_init("/tmp/debug_input_raw.raw");
	debug_input = writer_filter_init("/tmp/debug_input.raw");
	debug_ampl = writer_filter_init("/tmp/debug_ampl.raw");
	debug_freq_raw = writer_filter_init("/tmp/debug_freq.raw");
	debug_freq_filtered = writer_filter_init("/tmp/debug_freq_filtered.raw");;
	debug_note = writer_filter_init("/tmp/debug_note.raw");;
#endif

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
	c->notechange_mindelay = (int)ceil(notechange_mindelay * samplerate);
	
	c->midi_program = 71;
	
	c->pitchbend_abs_range_in_half_notes = 4;
	
	c->note_on = 0;
	c->last_note_pitch = -1;
	c->last_note_pitchbend = -1;
	c->last_note_velocity = -1;
	c->last_note_volume = -1;
	c->last_note_duration = 0;
	
	c->midi_note_on = midi_note_on;
	c->midi_note_off = midi_note_off;
	c->midi_pitchbend = midi_pitchbend;
	c->midi_mainvolume = midi_mainvolume;
	c->midi_programchange = midi_programchange;
}

float loghnf(float x) {
	const float half_note_factor = powf(2.0, 1.0/12);
	const float loghnf = logf(half_note_factor);
	return logf(x) / loghnf;
}

float freq_to_hnfmp0(float freq) {
	// Wikipedia "Middle C": 261.6 Hz, In MIDI, it is note number 60, C-4.
	// Therefore, middle A is 440.0 Hz, MIDI note number 69, A-4.
	// What is MIDI note 0? 60/12=5, therefore it's C-(-1).
	// A-(-1) has 440.0Hz / 2/2/2/2/2 = 13.75 Hz.
	// C is 9 half notes deeper than A
	const float loghnf_440 = loghnf(440);
	float half_notes_from_middle_a = loghnf(freq) - loghnf_440;
	float half_notes_from_midi_pitch_0 = half_notes_from_middle_a + 69.0;
	return half_notes_from_midi_pitch_0;
}

int hnfmp0_to_midi_pitch(float hnfmp0) {
	int int_half_notes_from_midi_pitch_0 = (int)floorf(hnfmp0 + 0.5);
	return int_half_notes_from_midi_pitch_0;
}

// WARNING: this function may return values outside of [-8192,8191], so clamping must be done by the user.
int hnfmp0_and_pitch_to_midi_pitchbend(float hnfmp0, int midi_pitch, int pitchbend_abs_range_in_half_notes) {
	// the pitchbend goes from -8192 to 8191.
	// by default, -8192 means 2 half tones lower, 8191 means (almost) 2 half tones higher, and -4096 means 1 half tone lower.
	float pitchbend_in_half_notes = hnfmp0 - (float)midi_pitch;
	float float_midi_pitchbend = (pitchbend_in_half_notes / pitchbend_abs_range_in_half_notes * 2) * 8192;
	int midi_pitchbend = (int)floorf(float_midi_pitchbend + 0.5);
	return midi_pitchbend;
}

#include <unistd.h>

// process the audio data
void audio_midi_converter_process(audio_midi_converter* c, int buffer_size, float* buffer, void* midi_player_info) {
	
	float buffer_input_raw[buffer_size];
	float buffer_input[buffer_size];
	float buffer_ampl[buffer_size];
	float buffer_freq_raw[buffer_size];
	float buffer_freq_filtered[buffer_size];
	float buffer_note[buffer_size];

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
		
		float freq_raw = zerocrossingdetector_filter_next(c->zerocross_filter, input);

		float track_ampl = c->ampl_noteon / 2;
		if (c->ampl_last < track_ampl && amplmax >= track_ampl) {
			c->out_freq_low_filter_1->lasty = freq_raw;
			c->out_freq_low_filter_2->lasty = freq_raw;
		}
		float freq = lowpass_rc_filter_next(c->out_freq_low_filter_1, freq_raw);
		freq = lowpass_rc_filter_next(c->out_freq_low_filter_2, freq);
		
		if (c->ampl_last < c->ampl_noteon && amplmax >= c->ampl_noteon && c->note_on==0) {
			c->note_on = 1;
			float hnfmp0 = freq_to_hnfmp0(freq);
			int pitch = hnfmp0_to_midi_pitch(hnfmp0);
			int pitchbend = hnfmp0_and_pitch_to_midi_pitchbend(hnfmp0, pitch, c->pitchbend_abs_range_in_half_notes);
			
			c->last_note_pitch = pitch;
			c->last_note_pitchbend = pitchbend;
			c->last_note_velocity = 63; //TODO: ampl derivative is velocity
			c->last_note_volume = (int)(amplmax / 0.15 * 127);
			
			printd("NOTE ON. amplmax=%f freq=%f pitch:%i pitchbend=%i\n", amplmax, freq, pitch, pitchbend);
			
			//(c->midi_programchange)(midi_player_info, i, c->midi_program);
			(c->midi_note_on)(midi_player_info, i, c->last_note_pitch, c->last_note_velocity);
			(c->midi_pitchbend)(midi_player_info, i, c->last_note_pitchbend);
			//(c->midi_mainvolume)(midi_player_info, i, c->last_note_volume);
		} else if (c->ampl_last >= c->ampl_noteon && amplmax < c->ampl_noteon && c->note_on==1 && c->last_note_duration >= c->notechange_mindelay) {
			printd("NOTE OFF. amplmax=%f pitch:%i\n", amplmax, c->last_note_pitch);
			(c->midi_note_off)(midi_player_info, i, c->last_note_pitch);
			
			c->note_on = 0;
			c->last_note_pitch = -1;
			c->last_note_pitchbend = -1;
			c->last_note_velocity = -1;
			c->last_note_volume = -1;
			c->last_note_duration = 0;
		} else if (c->note_on == 1 && c->last_note_duration >= c->notechange_mindelay) {
			float hnfmp0 = freq_to_hnfmp0(freq);
			int pitchbend = hnfmp0_and_pitch_to_midi_pitchbend(hnfmp0, c->last_note_pitch, c->pitchbend_abs_range_in_half_notes);
			int volume = (int)(amplmax / 0.15 * 127);
			
			if (pitchbend >= -8192 && pitchbend <= 8191) {
				if (abs(pitchbend - c->last_note_pitchbend) > 200) {
					printd("Pitchbend change. freq=%f pitch:%i\n", freq, pitchbend);
					c->last_note_pitchbend = pitchbend;
					(c->midi_pitchbend)(midi_player_info, i, c->last_note_pitchbend);
					c->last_note_duration = 0;
				}
			} else {
				printd("NOTE CHANGE (NOTE OFF for now). pitch=%i\n", c->last_note_pitch);
				(c->midi_note_off)(midi_player_info, i, c->last_note_pitch);
				// allow note on in the next frame.
				amplmax = 0.0;
				c->note_on = 0;
				c->last_note_duration = 0;
			}
			if (abs(volume - c->last_note_volume) > 10) {
				printd("Volume change. amplmax=%f\n", amplmax);
				c->last_note_volume = volume;
				//(c->midi_mainvolume)(midi_player_info, i, c->last_note_volume);
				c->last_note_duration = 0;
			}
		}

		buffer_input_raw[i] = x;
		buffer_input[i] = input;
		buffer_ampl[i] = ampl;
		buffer_freq_raw[i] = freq_raw/10000.0;
		buffer_freq_filtered[i] = freq/10000.0;
		buffer_note[i] = c->note_on;

		c->ampl_last = amplmax;
		c->last_note_duration++;
	}

#ifdef DEBUG_CONVERTER
	write(debug_input_raw->handle, buffer_input_raw, sizeof(float)*buffer_size);
	write(debug_input->handle, buffer_input, sizeof(float)*buffer_size);
	write(debug_ampl->handle, buffer_ampl, sizeof(float)*buffer_size);
	write(debug_freq_raw->handle, buffer_freq_raw, sizeof(float)*buffer_size);
	write(debug_freq_filtered->handle, buffer_freq_filtered, sizeof(float)*buffer_size);
	write(debug_note->handle, buffer_note, sizeof(float)*buffer_size);
#endif
}

