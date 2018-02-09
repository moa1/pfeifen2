
CC = gcc

all: pfeifen midi_in_decode

clean:
	rm -rf midi_in_decode pfeifen *.o

midi_in_decode: midi_in_decode.c
	gcc -g `pkg-config --libs --cflags jack` --std=c99 -o midi_in_decode midi_in_decode.c

pfeifen: pfeifen.c filters.c filters.h converter.c converter.h interface.h interface-gtk.c io-jack.c io-jack.h
	gcc -g -lm `pkg-config --libs --cflags jack` `pkg-config --cflags --libs gtk+-3.0` --std=gnu99 -o pfeifen filters.c converter.c interface-gtk.c io-jack.c pfeifen.c

run-jack: pfeifen
	jackd -d alsa &
	fluidsynth -a jack -m jack -r 48000 -d /usr/share/sounds/sf2/FluidR3_GM.sf2 &
#	fluidsynth -a alsa -m alsa_seq -r 48000 -d /usr/share/sounds/sf2/FluidR3_GM.sf2 &
	pfeifen &
	jack_connect pfeifen:output fluidsynth:midi
	jack_connect fluidsynth:left system:playback_1
	jack_connect fluidsynth:right system:playback_2
	jack_connect system:capture_1 pfeifen:input

