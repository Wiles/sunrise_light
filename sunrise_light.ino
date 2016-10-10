#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <Wire.h>
#include "RTClib.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

#define OLED_RESET 4
#define MODE 9
#define INC 10
#define DEC 11
#define DISARM 12
#define BUZZER 13

#define PIXELS 7

#define PWM_RANGE 6120l

#define ALARM_OFFSET 0 
#define OPTIONS_OFFSET 4
#define LIGHT_DURATION_OFFSET 5
#define SOUND_DURATION_OFFSET 6
#define OPTION_24_HOURS 1
 
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(24, PIXELS, NEO_GRB + NEO_KHZ800);
RTC_DS1307 rtc;

#define MODE_COUNT 11
const char setAlarmHour[] PROGMEM = "Set alarm hour";
const char setAlarmMinutes[] PROGMEM = "Set alarm minutes";
const char setHour[] PROGMEM = "Set hour";
const char setMinutes[] PROGMEM = "Set minutes";
const char setYear[] PROGMEM = "Set year";
const char setMonth[] PROGMEM = "Set month";
const char setDay[] PROGMEM = "Set day";
const char setTimeFormat[] PROGMEM = "Time format";
const char setLightDuration[] PROGMEM = "Set light duration";
const char setSoundDuration[] PROGMEM = "Set sound duration";
const char blank[] PROGMEM = "";

char buffer[20];

const char* const modes[] PROGMEM = {
  blank, // Display time
  setAlarmHour, 
  setAlarmMinutes, 
  setHour, 
  setMinutes, 
  setYear, 
  setMonth, 
  setDay,
  setTimeFormat,
  setLightDuration,
  setSoundDuration
};

TimeSpan alarm;

uint32_t lightDuration;
uint8_t soundDuration;

uint32_t pwm = 0;
uint8_t mode = 0;
boolean armed = true;
boolean modeInc = true;
boolean incInc = true;
boolean decInc = true;

uint8_t options;

String debug0;

void setup() {
  alarm  = TimeSpan(EEPROMReadLong(ALARM_OFFSET));
  options = EEPROM.read(OPTIONS_OFFSET);
  lightDuration = EEPROM.read(LIGHT_DURATION_OFFSET);
  if (lightDuration <= 0) {
    lightDuration = 1 * 60;
  } else if (lightDuration > 30) {
    lightDuration = 30 * 60;
  } else {
    lightDuration *= 60;
  }
  
  soundDuration = EEPROM.read(SOUND_DURATION_OFFSET);
  if (soundDuration > 60) {
    soundDuration = 60;
  }
  
  pinMode(MODE, INPUT_PULLUP);
  pinMode(INC, INPUT_PULLUP);
  pinMode(DEC, INPUT_PULLUP);
  pinMode(DISARM, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
  
  if (!rtc.begin()) {
    while (1);
  }

  if (! rtc.isrunning()) {
     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  strip.begin();
  strip.show();
}

DateTime now;

void loop() {
  now = rtc.now();

  modeInput();
  incrementInput();
  decrementInput();
  disarmInput();

  updateAlarm();
  
  updateStrip();
  
  updateDisplay();
}

void updateAlarm() {
  int32_t currTime = TimeSpan(0, now.hour(), now.minute(), now.second()).totalseconds();
  int32_t alarmTime = alarm.totalseconds();
  int32_t alarmStart = alarmTime - lightDuration;
  
  if (alarmStart < 0 && (currTime) > (86400 + alarmStart)) {
    currTime -= 3600l * 24;
  }
  
  if (currTime >= alarmStart && currTime <= alarmTime) {
    if (armed) {
      float alarmProgress = lightDuration - (alarmTime - currTime);
      pwm = easeIn(alarmProgress,0, PWM_RANGE, lightDuration);
    }
  } if(currTime >= alarmTime && currTime < (alarmTime + soundDuration)) {
    if (armed) {
        digitalWrite(BUZZER, ((currTime) % 2 == 0) ? HIGH : LOW);
    }
  }else {
    armed = true;
    digitalWrite(BUZZER, LOW);
  }

  if (pwm > PWM_RANGE) {
    pwm = PWM_RANGE;
  }
}

void updateStrip() {
  for (int i = 0; i < 24; ++i) {
    strip.setPixelColor(i, 0, 0, 0);
  }
  
  if (pwm != 0) {
    for (int i = 0; i < 24; ++i) {
      int v = pwm / 24;
      if ( i < pwm %24 ) {
        ++v;
      }
      strip.setPixelColor(i, v, v, v);
    }
  }
  strip.show();
}

String toTimeString(DateTime dateTime) {
  return toTimeString(dateTime.hour(), dateTime.minute(), dateTime.second());
}

String toTimeString(TimeSpan timeSpan) {
  return toTimeString(timeSpan.hours(), timeSpan.minutes(), timeSpan.seconds());
}

String toTimeString(int hours, int minutes, int seconds) {
  boolean pm = false;
  String iso = " ";

  if (options & OPTION_24_HOURS) {
    iso += "  ";
    if (hours < 10) {
      iso += " ";
    }
    iso += hours;
    if (seconds % 2 == 0) {
       iso += ":";
    } else {
       iso += " ";
    }
  
    if (minutes < 10) {
      iso += "0";
    }
  
    iso += minutes;
  } else {
    if (hours >= 12 ) {
      pm = true;
    }
    
    if (hours == 0) {
      hours = 12;
    } else if (hours > 12) {
      hours -= 12;
    }
    if (hours < 10) {
      iso += " ";
    }
    iso += hours;
    if (seconds % 2 == 0) {
       iso += ":";
    } else {
       iso += " ";
    }
  
    if (minutes < 10) {
      iso += "0";
    }
  
    iso += minutes;
  
    if (pm) {
      iso += " PM";
    } else {
      iso += " AM";
    }
  }


  return iso;
}

unsigned int easeIn(float t, float b, float c, float d) {
  t /= d;
  return c * t * t * t + b;
}

String toDateString(DateTime date) {
  String formattedDate = String();

  formattedDate += date.year();
  formattedDate += "-";
  
  if (date.month() < 10) {
    formattedDate += "0";
  }

  formattedDate += date.month();
  formattedDate += "-";

  if (date.day() < 10) {
    formattedDate += "0";
  }

  formattedDate += date.day();
  
  return formattedDate;
}

void updateDisplay() {
  String formattedTime = toTimeString(now.hour(), now.minute(), now.second());
  String formattedAlarm = toTimeString(alarm.hours(), alarm.minutes(), alarm.seconds());
  String date = toDateString(now);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.setTextSize(1);
  strcpy_P(buffer, (char*)pgm_read_word(&(modes[mode])));
  display.println(buffer);
  display.println(debug0);
  display.setTextSize(2);
  display.setCursor(0,25);
  if (mode == 1 || mode == 2) {
    display.println(formattedAlarm);
  } else if (mode == 8) {
    if (options & OPTION_24_HOURS) {
      display.println(" 24 hours");
    } else {
      display.println(" 12 hours");
    }
  } else if (mode == 9) {
    uint32_t min = lightDuration / 60;
    if (min == 0) {
      display.print(" DISABLED");
    } else {
      if (min < 10) {
        display.print(" ");
      }
      display.print(" ");
      display.print(min);
      display.print(" min");
    }
  } else if (mode == 10) {
    if (soundDuration == 0) {
      display.print(" DISABLED");
    } else {
      if (soundDuration < 10) {
        display.print(" ");
      }
      display.print(" ");
      display.print(soundDuration);
      display.print(" sec");
    }
  } else {
    display.println(formattedTime);
  }
  display.setTextSize(1);
  display.setCursor(67, 57);
  display.println(date);
  display.display();
}

void modeInput() {
  if (digitalRead(MODE) == LOW) {
    if (modeInc) {
      mode += modeInc;
      modeInc = 0;
      if (mode >= MODE_COUNT) {
        mode = 0;
      }
      modeInc = false;
    }
  } else {
    modeInc = true;
  }
}

void disarmInput() {
  if (digitalRead(DISARM) == LOW) {
    armed = false;
    pwm = 0;
    mode = 0;
  }
}

void incrementInput() {
  uint8_t min;
  if (digitalRead(INC) == LOW) {
    if (incInc) {
      switch(mode) {
        case 1:
          if (alarm.hours() == 23) {
            alarm = alarm - TimeSpan(0, 23, 0, 0);
          } else {
            alarm = alarm + TimeSpan(0, 1, 0, 0);
          }
          EEPROMWriteLong(ALARM_OFFSET, alarm.totalseconds());
        break;
        case 2:
          if (alarm.minutes() == 59) {
            alarm = alarm - TimeSpan(0, 0, 59, 0);
          } else {
            alarm = alarm + TimeSpan(0, 0, 1, 0);
          }
          EEPROMWriteLong(ALARM_OFFSET, alarm.totalseconds());
        break;
        case 3:
          now = now + TimeSpan(0, 1, 0, 0);
          rtc.adjust(now);
        break;
        case 4:
          if (now.minute() == 59) {
            now = now - TimeSpan(0, 0, 59, 0);
          } else {
            now = now + TimeSpan(0, 0, 1, 0);
          }
          rtc.adjust(now);
        break;
        case 5:
          if (now.year() < 2159) {
            now = DateTime(
              now.year() + 1,
              now.month(),
              now.day(),
              now.hour(),
              now.minute(),
              now.second()
            );
            rtc.adjust(now);
          }
        break;
        case 6:
          if (now.month() >= 12) {
            now = DateTime(
              now.year(),
              1,
              now.day(),
              now.hour(),
              now.minute(),
              now.second()
            );
          } else {
            now = DateTime(
              now.year(),
              now.month() + 1,
              now.day(),
              now.hour(),
              now.minute(),
              now.second()
            );
          }
          rtc.adjust(now);
        break;
        case 7:
          if (now.day() == daysInMonth(now.year(), now.month())) {
            now = DateTime(
              now.year(),
              now.month(),
              1,
              now.hour(),
              now.minute(),
              now.second()
            );
          } else {
            now = DateTime(
              now.year(),
              now.month(),
              now.day() + 1,
              now.hour(),
              now.minute(),
              now.second()
            );
          }
          rtc.adjust(now);
        break;
        case 8:
          options ^= OPTION_24_HOURS;
          EEPROMWriteLong(OPTIONS_OFFSET, options);
        break;
        case 9:
          min = lightDuration / 60;
          if (min == 60) {
            min = 0;
          } else {
            min++;
          }
          EEPROM.write(LIGHT_DURATION_OFFSET, min);
          lightDuration = min * 60;
        break;
        case 10:
          if (soundDuration == 59) {
            soundDuration = 0;
          } else {
            if (soundDuration == 0) {
              soundDuration = 1;
            } else {
              soundDuration += 2;
            }
          }
          EEPROM.write(SOUND_DURATION_OFFSET, soundDuration);
        break;
      }
    }
    incInc = false;
  } else {
    incInc = true;
  }
}

void decrementInput() {
  uint8_t min;
  if (digitalRead(DEC) == LOW) {
    if (decInc) {
      switch(mode) {
        case 1:
          if (alarm.hours() == 0) {
            alarm = alarm + TimeSpan(0, 23, 0, 0);
          } else {
            alarm = alarm - TimeSpan(0, 1, 0, 0);
          }
          EEPROMWriteLong(ALARM_OFFSET, alarm.totalseconds());
        break;
        case 2:
          if (alarm.minutes() == 0) {
            alarm = alarm + TimeSpan(0, 0, 59, 0);
          } else {
            alarm = alarm - TimeSpan(0, 0, 1, 0);
          }
          EEPROMWriteLong(ALARM_OFFSET, alarm.totalseconds());
        break;
        case 3:
          now = now - TimeSpan(0, 1, 0, 0);
          rtc.adjust(now);
        break;
        case 4:
          if (now.minute() == 0) {
            now = now + TimeSpan(0, 0, 59, 0);
            rtc.adjust(now);
          } else {
            now = now - TimeSpan(0, 0, 1, 0);
            rtc.adjust(now);
          }
        break;
        case 5:
        if (now.year() > 2000) {
          now = DateTime(
            now.year() - 1,
            now.month(),
            now.day(),
            now.hour(),
            now.minute(),
            now.second()
          );
          rtc.adjust(now);
        }
        break;
        case 6:
          if (now.month() <= 1) {
            now = DateTime(
              now.year(),
              12,
              now.day(),
              now.hour(),
              now.minute(),
              now.second()
            );
          } else {
            now = DateTime(
              now.year(),
              now.month() - 1,
              now.day(),
              now.hour(),
              now.minute(),
              now.second()
            );
          }
          rtc.adjust(now);
        break;
        case 7:
          if (now.day() == 1) {
            now = DateTime(
              now.year(),
              now.month(),
              daysInMonth(now.year(), now.month()),
              now.hour(),
              now.minute(),
              now.second()
            );
          } else {
            now = DateTime(
              now.year(),
              now.month(),
              now.day() - 1,
              now.hour(),
              now.minute(),
              now.second()
            );
          }
          rtc.adjust(now);
        break;
        case 8:
          options ^= OPTION_24_HOURS;
          EEPROMWriteLong(OPTIONS_OFFSET, options);
        break;
        case 9:
          min = lightDuration / 60;
          if (min == 0) {
            min = 60;
          } else {
            min--;
          }
          EEPROM.write(LIGHT_DURATION_OFFSET, min);
          lightDuration = min * 60;
        break;
        case 10:
          if (soundDuration == 0) {
            soundDuration = 59;
          } else {
            if (soundDuration == 1) {
              soundDuration = 0;
            } else {
              soundDuration -= 2;
            }
          }
          EEPROM.write(SOUND_DURATION_OFFSET, soundDuration);
        break;
      }
    }
    decInc = false;
  } else {
    decInc = true;
  }
}

/*
 * http://playground.arduino.cc/Code/EEPROMReadWriteLong
 */
void EEPROMWriteLong(int address, uint32_t value)
{
  //Decomposition from a long to 4 bytes by using bitshift.
  //One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);
  
  //Write the 4 bytes into the eeprom memory.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

uint32_t EEPROMReadLong(long address)
{
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);
  
  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

/*
 * http://stackoverflow.com/a/11595914/3293634
 */
boolean isLeapYear(int year) {
  if ((year & 3) == 0 && ((year % 25) != 0 || (year & 15) == 0)) {
    return true;
  }
  return false;
}

int daysInMonth(int year, int month) {
  switch(month){
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      return 31;
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
    case 2:
      if (isLeapYear(year)) {
        return 29;
      }
      return 30;
  }
}
      
