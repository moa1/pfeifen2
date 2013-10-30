// filters.h

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
} zerocrossingdetector_filter;

zerocrossingdetector_filter* zerocrossingdetector_filter_init(int samplerate);
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
