#include <stdio.h>
#include <stdlib.h>

#include "interface.h"


interface* interface_init(
	int (*parameters_change)(void* info, float ampl_noteon, float notechange_mindelay, float pitchbend_abs_range_in_half_notes, int midi_program),
	void* parameters_change_data,
	float ampl_noteon, float notechange_mindelay, float pitchbend_abs_range_in_half_notes, int midi_program
	)
{
	interface* i;
	
	if ((i = malloc(sizeof(interface))) == NULL) {
		fprintf(stderr, "malloc\n");
		return NULL;
	}

	i->parameters_change = parameters_change;
	i->parameters_change_data = parameters_change_data;
	i->ampl_noteon = ampl_noteon;
	i->notechange_mindelay = notechange_mindelay;
	i->pitchbend_abs_range_in_half_notes = pitchbend_abs_range_in_half_notes;
	i->midi_program = midi_program;

	return i;
}

int interface_process() {
	return 0;
}
