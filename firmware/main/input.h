#pragma once

namespace tokmagotchi {

// Start a task that samples the scroll wheel (rotary encoder + push) and the
// touchscreen, dispatching events to display / audio / camera modules.
//
// Hardware notes:
//   - Scroll wheel = two GPIO quadrature lines + one push button line.
//   - Touchscreen = I2C controller (consult OSHW schematic for address + int pin).
// GPIO assignments are centralised here; see input.cpp for the placeholder
// values that must be replaced against the board schematic.
void input_start();

} // namespace tokmagotchi
