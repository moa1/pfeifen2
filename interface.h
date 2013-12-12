
typedef struct interface_t {
	int (*parameters_change)(void* data, float ampl_noteon, float notechange_mindelay, float pitchbend_abs_range_in_half_notes, int midi_program);
	void* parameters_change_data;
	
	float ampl_noteon;
	float notechange_mindelay;
	float pitchbend_abs_range_in_half_notes;
	int midi_program;	
	
	void *interface_data;
} interface;

interface* interface_init(
	int (*parameters_change)(void* info, float ampl_noteon, float notechange_mindelay, float pitchbend_abs_range_in_half_notes, int midi_program),
	void* parameters_change_data,
	float ampl_noteon, float notechange_mindelay, float pitchbend_abs_range_in_half_notes, int midi_program
	);
int interface_process();

