// Checks what Clock understands by voice (src/apps/clock/voice_logic.h) on a computer, no board needed:
//   c++ -std=c++17 -O2 -I src/apps/clock tools/clocktest/voice_test.cpp -o /tmp/voice_test && /tmp/voice_test
#include <cstdio>
#include "voice_logic.h"

using namespace clockvoice;
static int failures;

static const char *kindName(Kind k) {
  switch (k) {
    case Kind::TIMER: return "timer";
    case Kind::TIMER_STOP: return "timer stop";
    case Kind::ALARM: return "alarm";
    case Kind::ALARMS_OFF: return "alarms off";
    case Kind::STOPWATCH_START: return "stopwatch start";
    case Kind::STOPWATCH_STOP: return "stopwatch stop";
    default: return "none";
  }
}

static void timer(const char *said, long seconds) {
  const Command c = parse(said, 600);
  const bool ok = c.kind == Kind::TIMER && c.seconds == seconds;
  printf("%s  \"%s\" -> %s %ld s\n", ok ? "ok  " : "FAIL", said, kindName(c.kind), c.seconds);
  failures += !ok;
}

// nowMin: minutes since midnight when it's said
static void alarm(const char *said, int nowMin, int h, int m, clocks::Repeat repeat = clocks::ONCE) {
  const Command c = parse(said, nowMin);
  const bool ok = c.kind == Kind::ALARM && c.hour == h && c.minute == m && c.repeat == repeat;
  printf("%s  \"%s\" at %02d:%02d -> %s %02d:%02d %s\n", ok ? "ok  " : "FAIL", said, nowMin / 60, nowMin % 60,
         kindName(c.kind), c.hour, c.minute, clocks::REPEAT_NAMES[c.repeat]);
  failures += !ok;
}

static void kind(const char *said, Kind k) {
  const Command c = parse(said, 600);
  const bool ok = c.kind == k;
  printf("%s  \"%s\" -> %s\n", ok ? "ok  " : "FAIL", said, kindName(c.kind));
  failures += !ok;
}

int main() {
  // timers
  timer("Set a timer for 8 minutes.", 480);
  timer("set a timer for eight minutes", 480);
  timer("Timer, 25 minutes.", 1500);
  timer("timer for twenty five minutes", 1500);
  timer("timer for twenty-five minutes", 1500);
  timer("Set a timer for 1 hour.", 3600);
  timer("set a timer for an hour and a half", 5400);
  timer("Set a timer for half an hour.", 1800);
  timer("timer for 1.5 hours", 5400);
  timer("timer for two and a half minutes", 150);
  timer("Set a timer for 90 seconds.", 90);
  timer("Timer for 1 hour 30 minutes", 5400);
  timer("set a timer for 2 hours and 15 minutes", 8100);
  timer("timer 5 mins", 300);
  timer("10 minutes", 600);
  timer("Start a 3 minute timer.", 180);
  timer("set a timer for a minute", 60);
  timer("Countdown for 45 seconds", 45);
  timer("timer for a quarter of an hour", 900);
  timer("timer for 30 hours", MAX_TIMER);

  // alarms: with am/pm, as said
  alarm("Set an alarm for 5 p.m.", 600, 17, 0);
  alarm("Set an alarm for 5:00pm.", 600, 17, 0);
  alarm("set an alarm for 5:00 PM", 600, 17, 0);
  alarm("Alarm at 6:30 a.m.", 1200, 6, 30);
  alarm("set an alarm for 12 am", 600, 0, 0);
  alarm("set an alarm for 12 pm", 600, 12, 0);
  alarm("Alarm at 17:45", 600, 17, 45);
  alarm("alarm at 07:15", 1200, 7, 15);
  // without am/pm: the next one to come round
  alarm("Wake me up at 7.", 1320, 7, 0);    // said at 22:00: 7 in the morning
  alarm("set an alarm for 5", 900, 17, 0);  // said at 15:00: 5 this afternoon
  alarm("set an alarm for 5", 180, 5, 0);   // said at 03:00: 5 this morning
  alarm("wake me at 7 in the morning", 600, 7, 0);
  alarm("alarm for 8 tonight", 600, 20, 0);
  alarm("set an alarm for seven thirty", 1320, 7, 30);
  alarm("Alarm at seven forty five.", 1320, 7, 45);
  alarm("set an alarm for 6 o'clock", 1320, 6, 0);
  alarm("alarm at half past six", 1320, 6, 30);
  alarm("alarm for quarter to seven", 1320, 6, 45);
  alarm("alarm at noon", 600, 12, 0);
  alarm("alarm for midnight", 600, 0, 0);
  alarm("Wake me up at 7 every weekday.", 1320, 7, 0, clocks::WEEKDAYS);
  alarm("set an alarm for 6:30 on weekdays", 1320, 6, 30, clocks::WEEKDAYS);
  alarm("alarm at 9 every day", 1320, 9, 0, clocks::DAILY);
  alarm("daily alarm at 8 am", 1320, 8, 0, clocks::DAILY);
  alarm("alarm at 10 on weekends", 1320, 10, 0, clocks::WEEKENDS);
  // from now
  alarm("set an alarm in 20 minutes", 600, 10, 20);
  alarm("alarm in 1 hour", 1410, 0, 30);  // past midnight

  // the rest
  kind("Stop the timer.", Kind::TIMER_STOP);
  kind("cancel the timer", Kind::TIMER_STOP);
  kind("Turn off my alarms.", Kind::ALARMS_OFF);
  kind("cancel the alarm", Kind::ALARMS_OFF);
  kind("Start the stopwatch.", Kind::STOPWATCH_START);
  kind("stop the stopwatch", Kind::STOPWATCH_STOP);
  kind("set an alarm", Kind::NONE);  // no time: nothing to set
  kind("What's the weather like?", Kind::NONE);
  kind("", Kind::NONE);
  kind("Thank you for watching.", Kind::NONE);  // Whisper's made-up words on silence

  printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
  return failures ? 1 : 0;
}
