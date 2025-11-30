#include "lib_mp3.hpp"

MP3Player::MP3Player(uint8_t uartNum, int rxPin, int txPin, unsigned long baud)
    : player_(),
      serial_(nullptr),
      uartNum_(uartNum),
      rxPin_(rxPin),
      txPin_(txPin),
      baud_(baud),
      playing_(false),
      paused_(false),
      lastTrackIndex_(0) {
  // Map uartNum to HardwareSerial reference for ESP32
  switch (uartNum_) {
    case 0:
      serial_ = &Serial0;  // Serial (UART0) on ESP32
      break;
    case 1:
      serial_ = &Serial1;
      break;
    case 2:
      serial_ = &Serial2;
      break;
    default:
      serial_ = &Serial1;
      break;
  }

  // Start serial with specified pins/baud. ESP32 allows begin with pins.
  if (serial_) {
    serial_->begin(baud_, SERIAL_8N1, rxPin_, txPin_);
  }
}

bool MP3Player::begin(bool isACK, bool doReset) {
  if (!serial_) return false;
  // The DFRobot library expects a Stream reference
  bool ok = player_.begin(*serial_, isACK, doReset);
  if (ok) {
    // keep internal state consistent (device started but not playing yet)
    playing_        = false;
    paused_         = false;
    lastTrackIndex_ = 0;
  }
  return ok;
}

void MP3Player::setVolume(uint8_t vol) {
  player_.volume(vol);
}

void MP3Player::play(uint16_t index) {
  if (index == 0) return;
  player_.play(index);
  lastTrackIndex_ = index;
  playing_        = true;
  paused_         = false;
}

void MP3Player::togglePlayPause() {
  if (playing_) {
    if (paused_) {
      // resume
      player_.start();
      paused_ = false;
    } else {
      // pause
      player_.pause();
      paused_ = true;
    }
  } else {
    // currently stopped: play last track or default to 1
    uint16_t idx = lastTrackIndex_ ? lastTrackIndex_ : 1;
    player_.play(idx);
    lastTrackIndex_ = idx;
    playing_        = true;
    paused_         = false;
  }
}

void MP3Player::stopPlayback() {
  player_.stop();
  playing_ = false;
  paused_  = false;
}