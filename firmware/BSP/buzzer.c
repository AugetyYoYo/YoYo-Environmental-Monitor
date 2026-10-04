#include "buzzer.h"
#include "Config.h"
#include "intrins.h"

/* 方波频率（Hz） */
#define BUZZER_FREQ 2700

/* 占空比（%）：50 = 最响；10~20 就挺轻了 */
#define BUZZER_DUTY 20

/* 一个周期的空循环总次\xCA\375（音调不对就调\xB3\375\xCA\375） */
#define PERIOD_LOOP ((unsigned int)(MAIN_Fosc / BUZZER_FREQ / 10UL))

void BUZZER_INIT(void) {
  P3M1 &= ~0x10;
  P3M0 |= 0x10; /* P3.4 推挽输出 */
  BUZZER_OFF();
}

void BUZZER_ON(void) { BUZZER = 1; }

void BUZZER_OFF(void) { BUZZER = 0; }

void BUZZER_BEEP(unsigned int ms) {
  unsigned long total;
  unsigned long i;
  unsigned int on_cnt;
  unsigned int off_cnt;
  unsigned int k;

  total = (unsigned long)ms * BUZZER_FREQ / 1000UL;

  on_cnt = (unsigned int)((unsigned long)PERIOD_LOOP * BUZZER_DUTY / 100UL);
  if (on_cnt == 0) {
    on_cnt = 1;
  }
  off_cnt = PERIOD_LOOP - on_cnt;

  for (i = 0; i < total; i++) {
    BUZZER = 1;
    for (k = 0; k < on_cnt; k++) {
      _nop_();
    }
    BUZZER = 0;
    for (k = 0; k < off_cnt; k++) {
      _nop_();
    }
  }
}
