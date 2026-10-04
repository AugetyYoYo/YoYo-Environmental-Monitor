#include "key.h"
#include "tick.h"

#define KEY_DEBOUNCE_MS 20

static unsigned char key_read(void) {
  if (KEY_MODE == 0)
    return KEY_MODE_K;
  if (KEY_UP == 0)
    return KEY_UP_K;
  if (KEY_DN == 0)
    return KEY_DN_K;
  return KEY_NONE;
}

void KEY_INIT(void) {
  /* 准双向模式（M1:M0 = 00），自带弱上拉，按键按下接地拉低 */
  P2M1 &= ~0x80;
  P2M0 &= ~0x80; /* P2.7 MODE */
  P4M1 &= ~0x40;
  P4M0 &= ~0x40; /* P4.6 UP   */
  P0M1 &= ~0x01;
  P0M0 &= ~0x01; /* P0.0 DN   */
}

unsigned char KEY_SCAN(void) {
  static unsigned char last = KEY_NONE;
  unsigned char now;

  now = key_read();

  if (now != last) {
    delay_ms(KEY_DEBOUNCE_MS);

    if (key_read() == now) { /* 20ms 后再确认 */
      last = now;
      return now;
    }
  }
  return KEY_NONE;
}
