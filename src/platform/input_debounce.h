#ifndef INPUT_DEBOUNCE_H
#define INPUT_DEBOUNCE_H

typedef struct {
    int stable_required;
    int stable_count;
    int last_raw_pressed;
    int debounced_pressed;
    int armed;
    int held_samples;
    int long_emitted;
} input_debounce_t;

typedef enum {
    INPUT_DEBOUNCE_NONE = 0,
    INPUT_DEBOUNCE_SHORT_PRESS,
    INPUT_DEBOUNCE_LONG_PRESS
} input_debounce_event_t;

void input_debounce_init(input_debounce_t *debounce, int stable_required);
int input_debounce_update(input_debounce_t *debounce, int raw_pressed);
input_debounce_event_t input_debounce_update_hold(input_debounce_t *debounce, int raw_pressed, int long_press_samples);

#endif
