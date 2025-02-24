// NonBlockingTimer.h
#ifndef NON_BLOCKING_TIMER_H
#define NON_BLOCKING_TIMER_H

#include <Arduino.h>

class NonBlockingTimer
{
public:
  // Constructor accepts the period in microseconds.
  NonBlockingTimer(unsigned long period_micros)
      : period(period_micros), last_time(micros()) {}

  // Returns true if the timer is "ringing" (the period has elapsed).
  // Automatically resets the timer when the period has elapsed.
  bool is_ringing()
  {
    unsigned long currentTime = micros();
    if (currentTime - last_time >= period)
    {
      last_time = currentTime;
      return true;
    }
    return false;
  }

  // Manually resets the timer.
  void reset()
  {
    last_time = micros();
  }

private:
  unsigned long period;
  unsigned long last_time;
};

#endif // NON_BLOCKING_TIMER_H