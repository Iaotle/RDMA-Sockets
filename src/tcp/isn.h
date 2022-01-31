#ifndef ANPNETSTACK_ISN_H
#define ANPNETSTACK_ISN_H

#include <stdint.h>

// Start the ISN clock
// Return:
//	0 in case of success
//	-EALREADY if the clock was already started
//	otherwise the return code of the pthread call
int isn_start_clock();
uint32_t isn_get();

#endif // ANPNETSTACK_ISN_H
