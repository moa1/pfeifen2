#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>

#include "interface.h"

#define INTERFACE_FILENAME "interface-gtk.glade"

typedef struct interface_widgets_t {
	GtkWidget *drawingarea_waveform;
} interface_widgets;

void init_values(interface* i, float ampl_noteon, float notechange_mindelay, float pitchbend_abs_range_in_half_notes, int midi_program) {
	i->ampl_noteon = ampl_noteon * 100.0;
	i->notechange_mindelay = notechange_mindelay * 1000.0;
	i->pitchbend_abs_range_in_half_notes = pitchbend_abs_range_in_half_notes;
	i->midi_program = midi_program;
}

void set_values(interface* i) {
	i->parameters_change(i->parameters_change_data, i->ampl_noteon/100.0, i->notechange_mindelay/1000.0, i->pitchbend_abs_range_in_half_notes, i->midi_program);
}

void spinbutton_ampl_noteon_value_changed(GtkSpinButton *spinbutton, gpointer user_data) {
	interface* i = (interface*) user_data;
	
	i->ampl_noteon = gtk_spin_button_get_value(spinbutton);
	set_values(i);
}

void spinbutton_notechange_mindelay_value_changed(GtkSpinButton *spinbutton, gpointer user_data) {
	interface* i = (interface*) user_data;
	
	i->notechange_mindelay = gtk_spin_button_get_value(spinbutton);
	set_values(i);
}

void spinbutton_midi_pitchbend_range_value_changed(GtkSpinButton* spinbutton, gpointer user_data) {
	interface* i = (interface*) user_data;
	
	i->pitchbend_abs_range_in_half_notes = gtk_spin_button_get_value(spinbutton);
	set_values(i);
}

// TODO: connect and handle combobox_midi_program "changed" signal.

interface* interface_init(
	int (*parameters_change)(void* info, float ampl_noteon, float notechange_mindelay, float pitchbend_abs_range_in_half_notes, int midi_program),
	void* parameters_change_data,
	float ampl_noteon, float notechange_mindelay, float pitchbend_abs_range_in_half_notes, int midi_program
	)
{
	interface* i = malloc(sizeof(interface));
	if (i == NULL) {
		fprintf(stderr, "Not enough memory!\n");
		return NULL;
	}
	
	i->parameters_change = parameters_change;
	i->parameters_change_data = parameters_change_data;
	i->interface_data = malloc(sizeof(interface_widgets));
	if (i->interface_data == NULL) {
		fprintf(stderr, "Not enough memory!\n");
		free(i);
		return NULL;
	}
	interface_widgets* iw = (interface_widgets*)i->interface_data;
	
	init_values(i, ampl_noteon, notechange_mindelay, pitchbend_abs_range_in_half_notes, midi_program);
	
	gtk_init(NULL, NULL);
	
	GtkBuilder* builder;
	
	/* use GtkBuilder to build our interface from the XML file */
	builder = gtk_builder_new();
	if (gtk_builder_add_from_file(builder, INTERFACE_FILENAME, NULL) == 0)
	{
		fprintf(stderr, "Cannot load GTK interface %s\n", INTERFACE_FILENAME);
		free(i->interface_data);
		free(i);
		return NULL;
	}
	
	/* get the widgets which will be referenced in callbacks */
	iw->drawingarea_waveform = (GtkWidget*)gtk_builder_get_object(builder, "drawingarea_waveform");
	
	/* connect signals, passing the interface struct as user data */
	//gtk_builder_connect_signals(builder, interface);

	// set the interface elements to values float ampl_noteon, float notechange_mindelay, float pitchbend_abs_range_in_half_notes, int midi_program

	//SEE gtk-intros/example4-builder.c on how to connect signals

	GtkWidget* w;

	w = (GtkWidget*)gtk_builder_get_object(builder, "spinbutton_ampl_noteon");
	gtk_spin_button_set_value((GtkSpinButton*)w, i->ampl_noteon);
	g_signal_connect(w, "value-changed", G_CALLBACK(spinbutton_ampl_noteon_value_changed), i);

	w = (GtkWidget*)gtk_builder_get_object(builder, "spinbutton_notechange_mindelay");
	gtk_spin_button_set_value((GtkSpinButton*)w, i->notechange_mindelay);
	g_signal_connect(w, "value-changed", G_CALLBACK(spinbutton_notechange_mindelay_value_changed), i);
	
	w = (GtkWidget*)gtk_builder_get_object(builder, "spinbutton_midi_pitchbend_range");
	gtk_spin_button_set_value((GtkSpinButton*)w, i->pitchbend_abs_range_in_half_notes);
	g_signal_connect(w, "value-changed", G_CALLBACK(spinbutton_midi_pitchbend_range_value_changed), i);

	// connect quit
	GtkWidget* window;
	window = (GtkWidget*)gtk_builder_get_object(builder, "window");
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	g_object_unref(builder); // free GtkBuilder

	gtk_widget_show(window);
	
	return i;
}

/*
int interface_process() {
	while (gtk_events_pending()) {
		if (gtk_main_iteration()) {
			return 0;
		}
	}
	return 1;
}
*/
int interface_process() {
	gtk_main();
}

