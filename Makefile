
CC = gcc

all: pfeifen midi_in_decode

clean:
	rm -rf midi_in_decode pfeifen *.o

midi_in_decode: midi_in_decode.c
	gcc -g `pkg-config --libs --cflags jack` --std=c99 -o midi_in_decode midi_in_decode.c

pfeifen: pfeifen.c filters.c filters.h converter.c converter.h interface.h interface-gtk.c interface-gtk.glade io-jack.c io-jack.h
	gcc -g -lm `pkg-config --libs --cflags jack` `pkg-config --cflags --libs gtk+-3.0` --std=gnu99 -o pfeifen filters.c converter.c interface-gtk.c io-jack.c pfeifen.c

run-jack: pfeifen
# set --period to a high value so that fluidsynth has enough time to process.
	ps cax | grep jackd || jackd -d alsa --period 8192 >/dev/null 2>&1 &
	sleep 2
	ps cax | grep fluidsynth || fluidsynth -a jack -m jack -r 48000 -i -s /usr/share/sounds/sf2/FluidR3_GM.sf2 &
	sleep 2
	./pfeifen &
	sleep 1
	jack_connect fluidsynth:left system:playback_1
	jack_connect fluidsynth:right system:playback_2
	jack_connect system:capture_1 pfeifen:input
	jack_connect pfeifen:output fluidsynth:midi

un-alsa: pfeifen
#	fluidsynth -a alsa -m alsa_seq -r 48000 -d /usr/share/sounds/sf2/FluidR3_GM.sf2 &

unrun:
	killall jackd fluidsynth pfeifen 1>/dev/null 2>&1 || echo killed
	ps cax | grep -E '(jackd|fluidsynth|pfeifen)' 2>/dev/null || echo done

