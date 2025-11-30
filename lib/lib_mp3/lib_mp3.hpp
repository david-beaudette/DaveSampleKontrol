#ifndef LIB_MP3_HPP
#define LIB_MP3_HPP

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

/*
 * lib_mp3 - multi-instance wrapper around DFRobotDFPlayerMini for ESP32-S3
 *
 * Usage:
 *   MP3Player hw1(1, RX1, TX1);       // instance using UART1
 *   if (!hw1.begin()) { ... }
 *   hw1.setVolume(10);
 *   hw1.play(1);
 *   hw1.togglePlayPause();
 *   hw1.stopPlayback();
 */

class MP3Player {
 public:
  // uartNum: 0..2 (ESP32 UART indices). rxPin/txPin: hardware pins for that
  // UART.
  MP3Player(uint8_t uartNum, int rxPin, int txPin, unsigned long baud = 9600);
  // initialize DFPlayer (returns true on success)
  bool begin(bool isACK = true, bool doReset = true);
  void setVolume(uint8_t vol);  // 0..30
  void play(uint16_t index);    // play track index

  // new control helpers:
  // toggle between play and pause. If currently stopped, will resume last
  // played track (or play track 1 if none).
  void togglePlayPause();

  // stop playback and reset playing state
  void stopPlayback();

 private:
  DFRobotDFPlayerMini player_;
  HardwareSerial*     serial_;
  uint8_t             uartNum_;
  int                 rxPin_;
  int                 txPin_;
  unsigned long       baud_;

  // internal state tracking for toggle behavior
  bool     playing_;
  bool     paused_;
  uint16_t lastTrackIndex_;
};

#endif  // LIB_MP3_HPP