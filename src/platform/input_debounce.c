#include "platform/input_debounce.h"

void input_debounce_init(input_debounce_t *debounce, int stable_required) {
    if (debounce == 0) {
        return;
    }

    debounce->stable_required = stable_required > 1 ? stable_required : 1;
    debounce->stable_count = 0;
    debounce->last_raw_pressed = 0;
    debounce->debounced_pressed = 0;
    debounce->armed = 1;
    debounce->held_samples = 0;
    debounce->long_emitted = 0;
}

int input_debounce_update(input_debounce_t *debounce, int raw_pressed) {
    int pressed = raw_pressed ? 1 : 0;
    if (debounce == 0) {
        return 0;
    }

    if (pressed == debounce->last_raw_pressed) {
        if (debounce->stable_count < debounce->stable_required) {
            debounce->stable_count++;
        }
    } else {
        debounce->last_raw_pressed = pressed;
        debounce->stable_count = 1;
    }

    if (debounce->stable_count < debounce->stable_required) {
        return 0;
    }

    if (debounce->debounced_pressed != pressed) {
        debounce->debounced_pressed = pressed;
        if (!pressed) {
            debounce->armed = 1;
            return 0;
        }
        if (debounce->armed) {
            debounce->armed = 0;
            return 1;
        }
    }

    return 0;
}

input_debounce_event_t input_debounce_update_hold(input_debounce_t *debounce, int raw_pressed, int long_press_samples) {
    int pressed = raw_pressed ? 1 : 0;
    int required;
    if (debounce == 0) {
        return INPUT_DEBOUNCE_NONE;
    }

    required = long_press_samples > debounce->stable_required ? long_press_samples : debounce->stable_required;
    if (pressed == debounce->last_raw_pressed) {
        if (debounce->stable_count < debounce->stable_required) {
            debounce->stable_count++;
        }
    } else {
        debounce->last_raw_pressed = pressed;
        debounce->stable_count = 1;
    }

    if (debounce->stable_count < debounce->stable_required) {
        return INPUT_DEBOUNCE_NONE;
    }

    if (debounce->debounced_pressed != pressed) {
        debounce->debounced_pressed = pressed;
        if (pressed) {
            debounce->held_samples = debounce->stable_count;
            debounce->long_emitted = 0;
        } else {
            int emit_short = debounce->armed && !debounce->long_emitted;
            debounce->held_samples = 0;
            debounce->long_emitted = 0;
            debounce->armed = 1;
            return emit_short ? INPUT_DEBOUNCE_SHORT_PRESS : INPUT_DEBOUNCE_NONE;
        }
    } else if (pressed) {
        debounce->held_samples++;
    }

    if (pressed && debounce->armed && !debounce->long_emitted && debounce->held_samples >= required) {
        debounce->long_emitted = 1;
        debounce->armed = 0;
        return INPUT_DEBOUNCE_LONG_PRESS;
    }

    return INPUT_DEBOUNCE_NONE;
}
