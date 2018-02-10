// gcc -c --std=gnu99 -o filters.o filters.c
// filters.c

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "filters.h"

#define M_PI 3.14159265358979

// An in-place low-pass filter with cutoff frequency CUTOFF for the samples SAMPLES sampled at sample rate SAMPLERATE.
lowpass_rc_filter* lowpass_rc_filter_init(float cutoff, float samplerate) {
	lowpass_rc_filter* f = malloc(sizeof(lowpass_rc_filter));
	if (f == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	float rc = 1.0/(cutoff*2*M_PI);
	float dt = 1.0/samplerate;
    f->a = dt / (rc + dt);
    f->b = 1.0 - f->a;
    f->lasty = 0.0;
    return f;
}

inline float lowpass_rc_filter_next(lowpass_rc_filter* f, float x) {
	float y = f->a * x + f->b * f->lasty;
	f->lasty = y;
	return y;
}

//Wikipedia "high-pass filter": y_n = alpha ( y_{n-1} + x_{n} - x_{n-1}) :where alpha = frac{RC}{RC + Delta t}
highpass_rc_filter* highpass_rc_filter_init(float cutoff, float samplerate) {
	highpass_rc_filter* f = malloc(sizeof(highpass_rc_filter));
	if (f == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	float rc = 1.0/(cutoff*2*M_PI);
	float dt = 1.0/samplerate;
	f->a = rc / (rc + dt);
	f->lasty = 0.0;
	f->lastx = 0.0;
	return f;
}

inline float highpass_rc_filter_next(highpass_rc_filter* f, float x) {
	float y = f->a * (f->lasty + x - f->lastx);
	f->lastx = x;
	f->lasty = y;
	return y;
}

automaticgain_filter* automaticgain_filter_init(float gain, float samplerate) {
	automaticgain_filter* f = malloc(sizeof(automaticgain_filter));
	if (f == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	f->a = 0.0;
	f->gain = gain/samplerate;
	return f;
}

float automaticgain_filter_next(automaticgain_filter* f, float x) {
	f->a += f->gain;
	float y = x * f->a;
	if (y > 0.8) {
		f->a = 0.8 / x;
		if (y > 1.0)
			y = 1.0;
	} else if (y < -0.8) {
		f->a = -0.8 / x;
		if (y < -1.0)
			y = -1.0;
	}
	return y;
}

zerocrossingdetector_filter* zerocrossingdetector_filter_init(float samplerate) {
	zerocrossingdetector_filter* f = malloc(sizeof(zerocrossingdetector_filter));
	if (f == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	f->samplerate = samplerate;
	f->lastx = 0.0;
	f->lastupcrossing = 1;
	f->lastfreq = samplerate;
	return f;
}

float zerocrossingdetector_filter_next(zerocrossingdetector_filter* f, float x) {
	float y = f->lastfreq;
//	printf("zcd_filter_next f:%p f->lastx:%f x:%f f->lastupcrossing:%i\n", f, f->lastx, x, f->lastupcrossing);
	if (f->lastx < 0.0 && x >= 0.0) {
		// TODO: subsample precision by taking into account lastx and x.
		y = f->samplerate / f->lastupcrossing;
		f->lastupcrossing = 0;
		f->lastfreq = y;
	}
	f->lastx = x;
	f->lastupcrossing += 1;
	return y;
}

amplitudedetector_filter* amplitudedetector_filter_init() {
	amplitudedetector_filter* f = malloc(sizeof(amplitudedetector_filter));
	if (f == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	f->minx = 0.0;
	f->maxx = 0.0;
	f->dir = 1;
	f->lastx = 0.0;
	f->ampl = 0.0;
	return f;
}

float amplitudedetector_filter_next(amplitudedetector_filter* f, float x) {
	float xdiff = x - f->lastx;
	if (f->dir == 1 && xdiff >= 0) {
		f->maxx = x;
	} else if (f->dir == 1 && xdiff < 0) {
		f->maxx = f->lastx;
		f->ampl = (f->maxx - f->minx) / 2.0;
		f->dir = -1;
	} else if (f->dir == -1 && xdiff <= 0) {
		f->minx = x;
	} else if (f->dir == -1 && xdiff > 0) {
		f->minx = f->lastx;
		f->ampl = (f->maxx - f->minx) / 2.0;
		f->dir = 1;
	}
	f->lastx = x;
	return f->ampl;
	//return the maximum of the last 0.05 (0.01?) seconds of f->ampl.
}

windowfunction_filter* windowfunction_filter_init(float seconds, float samplerate, float (*function)(int, float*)) {
	windowfunction_filter* f = malloc(sizeof(windowfunction_filter));
	if (f == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	f->maxwindowlen = (int)ceil(seconds * samplerate);
	f->windowlen = 0;
	f->window = (float*)malloc(sizeof(float)*f->maxwindowlen);
	f->function = function;
	return f;
}

float windowfunction_filter_next(windowfunction_filter* f, float x) {
	if (f->windowlen >= f->maxwindowlen) {
		memmove(&f->window[0], &f->window[1], sizeof(float)*(f->windowlen-1));
		f->windowlen--;
	}
	f->window[f->windowlen] = x;
	f->windowlen++;
	float y = (f->function)(f->windowlen, f->window);
	return y;
}

writer_filter* writer_filter_init(const char* filename) {
	writer_filter* f = malloc(sizeof(writer_filter));
	if (f == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	
	int handle = open(filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
	if (handle == -1) {
		fprintf(stderr,"filename=%s\n",filename);
		perror("cannot open file");
		exit(1);
	}
	
	f-> handle = handle;
}
float writer_filter_next(writer_filter* f, float x) {
	write(f->handle, &x, sizeof(float));
}

