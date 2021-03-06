// filters.h

#ifndef PFEIFEN2_FILTERS_H
#define PFEIFEN2_FILTERS_H

typedef struct lowpass_rc_filter_t {
	float a;
	float b;
	float lasty;
} lowpass_rc_filter;

lowpass_rc_filter* lowpass_rc_filter_init(float cutoff, float samplerate);
float lowpass_rc_filter_next(lowpass_rc_filter* f, float x);


typedef struct highpass_rc_filter_t {
	float a;
	float lasty;
	float lastx;
} highpass_rc_filter;

highpass_rc_filter* highpass_rc_filter_init(float cutoff, float samplerate);
float highpass_rc_filter_next(highpass_rc_filter* f, float x);


typedef struct automaticgain_filter_t {
	float a;
	float gain;
} automaticgain_filter;

automaticgain_filter* automaticgain_filter_init(float gain, float samplerate);
float automaticgain_filter_next(automaticgain_filter* f, float x);


typedef struct zerocrossingdetector_filter_t {
	float samplerate;
	float lastx;
	int lastupcrossing;
	float lastfreq;
} zerocrossingdetector_filter;

zerocrossingdetector_filter* zerocrossingdetector_filter_init(float samplerate);
float zerocrossingdetector_filter_next(zerocrossingdetector_filter* f, float x);


typedef struct amplitudedetector_filter_t {
	float minx;
	float maxx;
	char dir;
	float lastx;
	float ampl;
} amplitudedetector_filter;

amplitudedetector_filter* amplitudedetector_filter_init();
float amplitudedetector_filter_next(amplitudedetector_filter* f, float x);


typedef struct windowfunction_filter_t {
	int maxwindowlen;
	int windowlen;
	float* window;
	float (*function)(int, float *);
} windowfunction_filter;

windowfunction_filter* windowfunction_filter_init(float seconds, float samplerate, float (*function)(int, float*));
float windowfunction_filter_next(windowfunction_filter* f, float x);


typedef struct writer_filter_t {
	int handle;
} writer_filter;
writer_filter* writer_filter_init(const char* filename);
float writer_filter_next(writer_filter* f, float x);

#endif//PFEIFEN2_FILTERS_H
