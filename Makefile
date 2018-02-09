
CC = gcc

all: pfeifen pfeifen-debug midi_in_decode

clean:
	rm -rf midi_in_decode pfeifen pfeifen-debug *.o

midi_in_decode: midi_in_decode.c
	gcc -g `pkg-config --libs --cflags jack` --std=c99 -o midi_in_decode midi_in_decode.c

PFEIFEN_DEPS = pfeifen.c filters.c filters.h converter.c converter.h interface.h interface-none.c io-jack.c io-jack.h io-alsa.c io-alsa.h
PFEIFEN_CS = filters.c converter.c interface-none.c io-jack.c io-alsa.c pfeifen.c

pfeifen: $(PFEIFEN_DEPS)
	gcc -DPRINTD -g -lm `pkg-config --libs --cflags jack` `pkg-config --libs --cflags alsa` `pkg-config --cflags --libs gtk+-3.0` --std=gnu99 -o pfeifen $(PFEIFEN_CS)

pfeifen-debug: $(PFEIFEN_DEPS)
	gcc -DPRINTD -DDEBUG_CONVERTER -g -lm `pkg-config --libs --cflags jack` `pkg-config --libs --cflags alsa` `pkg-config --cflags --libs gtk+-3.0` --std=gnu99 -o pfeifen-debug $(PFEIFEN_CS)

.PHONY: run-jack

run-jack-start: pfeifen-debug
# set --period to a high value so that fluidsynth has enough time to process.
	ps cax | grep jackd || jackd -d alsa -T >/dev/null 2>&1 &
	sleep 2
	ps cax | grep fluidsynth || fluidsynth -a jack -m jack -r 48000 -i -s -j /usr/share/sounds/sf2/FluidR3_GM.sf2 &
	./pfeifen-debug --jack &
	sleep 2
	jack_connect system:capture_1 pfeifen:input
	jack_connect pfeifen:output fluidsynth:midi
	sleep $$((60*60*24*365)) || echo -n #one year should be enough

run-jack:
# this is taken from https://stackoverflow.com/a/32788564
	bash -c "trap 'trap - SIGINT SIGTERM ERR; echo cleaning up; $(MAKE) killall; exit 0' SIGINT SIGTERM ERR; $(MAKE) run-jack-start"

run-alsa: pfeifen-debug
	ps cax | grep fluidsynth || fluidsynth -a alsa -m alsa_seq -r 48000 -i -s /usr/share/sounds/sf2/FluidR3_GM.sf2 &
	sleep 2
	./pfeifen-debug --alsa

killall:
	ps cax | grep -E '(jackd|fluidsynth|pfeifen)' 2>/dev/null || echo -n
	killall -w jackd fluidsynth pfeifen pfeifen-debug 1>/dev/null 2>&1 || echo -n
	ps cax | grep -E '(jackd|fluidsynth|pfeifen)' 2>/dev/null || echo -n
