#define DUXCEL_REVIEW_INPUT_IMPLEMENTATION
#include "review_input.h"
static bool quarantined;
bool dx_review_input_blocked(void) { return quarantined; }
void dx_review_input_quarantine(bool blocked) { quarantined=blocked; }
