#ifndef DUXCEL_REVIEW_INPUT_H
#define DUXCEL_REVIEW_INPUT_H
/* Forced only into the isolated review configurations, after raylib's normal
   declarations. Quarantine the single capture render without feeding input to
   ordinary controllers; the composer later owns normal input itself. */
#include <raylib.h>
#include <stdbool.h>
bool dx_review_input_blocked(void);
void dx_review_input_quarantine(bool blocked);
#ifndef DUXCEL_REVIEW_INPUT_IMPLEMENTATION
#define IsKeyPressed(k) (!dx_review_input_blocked() && IsKeyPressed(k))
#define IsKeyPressedRepeat(k) (!dx_review_input_blocked() && IsKeyPressedRepeat(k))
#define IsKeyDown(k) (!dx_review_input_blocked() && IsKeyDown(k))
#define IsKeyReleased(k) (!dx_review_input_blocked() && IsKeyReleased(k))
#define GetKeyPressed() (dx_review_input_blocked() ? 0 : GetKeyPressed())
#define GetCharPressed() (dx_review_input_blocked() ? 0 : GetCharPressed())
#define IsMouseButtonPressed(b) (!dx_review_input_blocked() && IsMouseButtonPressed(b))
#define IsMouseButtonReleased(b) (!dx_review_input_blocked() && IsMouseButtonReleased(b))
#define IsMouseButtonDown(b) (!dx_review_input_blocked() && IsMouseButtonDown(b))
#define GetMouseWheelMove() (dx_review_input_blocked() ? 0.0f : GetMouseWheelMove())
#define GetMouseWheelMoveV() (dx_review_input_blocked() ? (Vector2){0,0} : GetMouseWheelMoveV())
#endif
#endif
