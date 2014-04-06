
CC = gcc

all: pfeifen midi_in_decode

clean:
	rm midi_in_decode
	rm pfeifen
	rm *.o    

midi_in_decode: midi_in_decode.c
	gcc -g `pkg-config --libs --cflags jack` --std=c99 -o midi_in_decode midi_in_decode.c

pfeifen: pfeifen.c filters.c filters.h converter.c converter.h interface.h interface-gtk.c
	gcc -g -lm `pkg-config --libs --cflags jack` `pkg-config --cflags --libs gtk+-3.0` --std=gnu99 -o pfeifen filters.c converter.c interface-gtk.c pfeifen.c

