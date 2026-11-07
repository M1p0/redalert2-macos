/* Native host's optional right-button map panning. */
#pragma once
void ra2_mapdrag_set(int enabled);
int ra2_mapdrag_enabled(void);
int ra2_mapdrag_input(unsigned message, int x, int y);
void ra2_mapdrag_position(int *x, int *y);
