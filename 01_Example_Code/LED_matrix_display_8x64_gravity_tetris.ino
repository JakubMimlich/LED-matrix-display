/*
  ESP8266/ESP32 + 9x 74HC595
  8x64 LED matrix
  Original shiftOut() drive
  Flexible fixed-size font support

  Pinout:
    DATA_PIN  = 23
    CLOCK_PIN = 18
    LATCH_PIN = 19

  Register mapping confirmed earlier:
    shift byte [0] = row register
    shift byte [1] = columns 56..63
    shift byte [2] = columns 48..55
    shift byte [3] = columns 40..47
    shift byte [4] = columns 32..39
    shift byte [5] = columns 24..31
    shift byte [6] = columns 16..23
    shift byte [7] = columns 8..15
    shift byte [8] = columns 0..7

  Column bit order inside each 8-column block is mirrored:
    x=0 -> bit0
    x=7 -> bit7
*/

#include <Arduino.h>
#include <string.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <time.h>
#include <sys/time.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <espnow.h>
  using MatrixWebServer = ESP8266WebServer;
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <esp_now.h>
  #if defined(__has_include)
    #if __has_include(<esp_arduino_version.h>)
      #include <esp_arduino_version.h>
    #endif
  #endif
  using MatrixWebServer = WebServer;
#else
  #error "This sketch requires an ESP8266 or ESP32 board."
#endif

#include "web_ui.h"

// ============================================================
// Pinout
// ============================================================
#if defined(ESP8266)
const int DATA_PIN  = D7;
const int CLOCK_PIN = D5;
const int LATCH_PIN = D6;
#else
const int DATA_PIN  = 23;
const int CLOCK_PIN = 18;
const int LATCH_PIN = 19;
#endif

// ============================================================
// Matrix configuration
// ============================================================
static const uint8_t MATRIX_WIDTH  = 64;
static const uint8_t MATRIX_HEIGHT = 8;
static const uint8_t COLUMN_BYTES  = 8;
static const uint8_t SHIFT_BYTES   = 9;

// ============================================================
// Logic configuration
// ============================================================
static const bool COLUMN_ACTIVE_HIGH  = true;
static const bool ROW_ACTIVE_HIGH     = true;
static const bool COLUMN_BIT_REVERSED = true;

// Refresh timing
static const uint32_t ROW_HOLD_US = 1000;


static const uint8_t COLUMN_BIT_MAP[8][8] = {
  {6,1,2,7,4,5,3,0},
  {6,1,2,7,4,5,3,0},
  {6,1,2,7,4,5,3,0},
  {6,1,2,7,4,5,3,0},
  {6,1,2,7,4,5,3,0},
  {6,1,2,7,4,5,3,0},
  {6,1,2,7,4,5,3,0},
  {6,1,2,7,4,5,3,0}
};

// ============================================================
// Fixed-size font format
// ============================================================
struct FixedFont
{
  const uint8_t* data;   // flattened [charCount][width]
  uint8_t width;         // bytes per character
  uint8_t height;        // pixel rows, max 8 in this implementation
  char firstChar;        // first ASCII code
  char lastChar;         // last ASCII code
};

// ============================================================
// Example font: 6x8, ASCII 32..90
// 59 characters, 6 bytes per character
// Each byte = one vertical column
// bit0 = top row, bit7 = bottom row
// ============================================================
static const uint8_t font6x8[59][6] = {
  {0x00,0x00,0x00,0x00,0x00,0x00}, // 32 ' '
  {0x00,0x00,0x5F,0x5F,0x00,0x00}, // 33 '!'
  {0x00,0x07,0x00,0x07,0x00,0x00}, // 34 '"'
  {0x14,0x7F,0x14,0x7F,0x14,0x00}, // 35 '#'
  {0x24,0x2A,0x7F,0x2A,0x12,0x00}, // 36 '$'
  {0x23,0x13,0x08,0x64,0x62,0x00}, // 37 '%'
  {0x36,0x49,0x55,0x22,0x50,0x00}, // 38 '&'
  {0x00,0x05,0x03,0x00,0x00,0x00}, // 39 '\''
  {0x00,0x1C,0x22,0x41,0x00,0x00}, // 40 '('
  {0x00,0x41,0x22,0x1C,0x00,0x00}, // 41 ')'
  {0x14,0x08,0x3E,0x08,0x14,0x00}, // 42 '*'
  {0x08,0x08,0x3E,0x08,0x08,0x00}, // 43 '+'
  {0x00,0x50,0x30,0x00,0x00,0x00}, // 44 ','
  {0x08,0x08,0x08,0x08,0x08,0x00}, // 45 '-'
  {0x00,0x60,0x60,0x00,0x00,0x00}, // 46 '.'
  {0x20,0x10,0x08,0x04,0x02,0x00}, // 47 '/'

  {0x3E,0x51,0x49,0x45,0x3E,0x00}, // 48 '0'
  {0x00,0x42,0x7F,0x40,0x00,0x00}, // 49 '1'
  {0x42,0x61,0x51,0x49,0x46,0x00}, // 50 '2'
  {0x21,0x41,0x45,0x4B,0x31,0x00}, // 51 '3'
  {0x18,0x14,0x12,0x7F,0x10,0x00}, // 52 '4'
  {0x27,0x45,0x45,0x45,0x39,0x00}, // 53 '5'
  {0x3C,0x4A,0x49,0x49,0x30,0x00}, // 54 '6'
  {0x01,0x71,0x09,0x05,0x03,0x00}, // 55 '7'
  {0x36,0x49,0x49,0x49,0x36,0x00}, // 56 '8'
  {0x06,0x49,0x49,0x29,0x1E,0x00}, // 57 '9'

  {0x00,0x36,0x36,0x00,0x00,0x00}, // 58 ':'
  {0x00,0x56,0x36,0x00,0x00,0x00}, // 59 ';'
  {0x08,0x14,0x22,0x41,0x00,0x00}, // 60 '<'
  {0x14,0x14,0x14,0x14,0x14,0x00}, // 61 '='
  {0x00,0x41,0x22,0x14,0x08,0x00}, // 62 '>'
  {0x02,0x01,0x51,0x09,0x06,0x00}, // 63 '?'
  {0x32,0x49,0x79,0x41,0x3E,0x00}, // 64 '@'

  {0x7E,0x11,0x11,0x11,0x7E,0x00}, // 65 'A'
  {0x7F,0x49,0x49,0x49,0x36,0x00}, // 66 'B'
  {0x3E,0x41,0x41,0x41,0x22,0x00}, // 67 'C'
  {0x7F,0x41,0x41,0x22,0x1C,0x00}, // 68 'D'
  {0x7F,0x49,0x49,0x49,0x41,0x00}, // 69 'E'
  {0x7F,0x09,0x09,0x09,0x01,0x00}, // 70 'F'
  {0x3E,0x41,0x49,0x49,0x7A,0x00}, // 71 'G'
  {0x7F,0x08,0x08,0x08,0x7F,0x00}, // 72 'H'
  {0x00,0x41,0x7F,0x41,0x00,0x00}, // 73 'I'
  {0x20,0x40,0x41,0x3F,0x01,0x00}, // 74 'J'
  {0x7F,0x08,0x14,0x22,0x41,0x00}, // 75 'K'
  {0x7F,0x40,0x40,0x40,0x40,0x00}, // 76 'L'
  {0x7F,0x02,0x0C,0x02,0x7F,0x00}, // 77 'M'
  {0x7F,0x04,0x08,0x10,0x7F,0x00}, // 78 'N'
  {0x3E,0x41,0x41,0x41,0x3E,0x00}, // 79 'O'
  {0x7F,0x09,0x09,0x09,0x06,0x00}, // 80 'P'
  {0x3E,0x41,0x51,0x21,0x5E,0x00}, // 81 'Q'
  {0x7F,0x09,0x19,0x29,0x46,0x00}, // 82 'R'
  {0x46,0x49,0x49,0x49,0x31,0x00}, // 83 'S'
  {0x01,0x01,0x7F,0x01,0x01,0x00}, // 84 'T'
  {0x3F,0x40,0x40,0x40,0x3F,0x00}, // 85 'U'
  {0x1F,0x20,0x40,0x20,0x1F,0x00}, // 86 'V'
  {0x7F,0x20,0x18,0x20,0x7F,0x00}, // 87 'W'
  {0x63,0x14,0x08,0x14,0x63,0x00}, // 88 'X'
  {0x03,0x04,0x78,0x04,0x03,0x00}, // 89 'Y'
  {0x61,0x51,0x49,0x45,0x43,0x00}  // 90 'Z'
};

static const FixedFont FONT_6X8 = {
  &font6x8[0][0],
  6,
  8,
  32,
  90
};

// ============================================================
// Matrix driver
// ============================================================
class LedMatrix8x64
{
public:
  void begin()
  {
    pinMode(DATA_PIN, OUTPUT);
    pinMode(CLOCK_PIN, OUTPUT);
    pinMode(LATCH_PIN, OUTPUT);

    digitalWrite(DATA_PIN, LOW);
    digitalWrite(CLOCK_PIN, LOW);
    digitalWrite(LATCH_PIN, LOW);

    clear();
    allOffNow();
  }

  void update()
  {
    const uint32_t now = micros();
    if ((uint32_t)(now - _lastRefreshUs) >= ROW_HOLD_US)
    {
      _lastRefreshUs = now;
      refreshOneRow();
    }
  }

  void clear()
  {
    memset(_framebuffer, 0x00, sizeof(_framebuffer));
  }

  void fill()
  {
    memset(_framebuffer, 0xFF, sizeof(_framebuffer));
  }

  void setRotation180(bool enabled)
  {
    _rotation180 = enabled;
  }

  bool rotation180() const
  {
    return _rotation180;
  }

  void setPixel(int16_t x, int16_t y, bool state = true)
{
  if (x < 0 || y < 0) return;
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return;

  if (_rotation180)
  {
    x = MATRIX_WIDTH - 1 - x;
    y = MATRIX_HEIGHT - 1 - y;
  }

  const uint8_t blockIndex = x / 8;     // 0..7
  const uint8_t localCol   = x % 8;     // 0..7 inside block
  const uint8_t bitIndex   = COLUMN_BIT_MAP[blockIndex][localCol];

  if (state)
    _framebuffer[y][blockIndex] |= (1U << bitIndex);
  else
    _framebuffer[y][blockIndex] &= ~(1U << bitIndex);
}

  bool getPixel(int16_t x, int16_t y) const
{
  if (x < 0 || y < 0) return false;
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return false;

  if (_rotation180)
  {
    x = MATRIX_WIDTH - 1 - x;
    y = MATRIX_HEIGHT - 1 - y;
  }

  const uint8_t blockIndex = x / 8;
  const uint8_t localCol   = x % 8;
  const uint8_t bitIndex   = COLUMN_BIT_MAP[blockIndex][localCol];

  return (_framebuffer[y][blockIndex] & (1U << bitIndex)) != 0;
}

  void drawHorizontalLine(uint8_t y, bool state = true)
  {
    if (y >= MATRIX_HEIGHT) return;
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) setPixel(x, y, state);
  }

  void drawVerticalLine(uint8_t x, bool state = true)
  {
    if (x >= MATRIX_WIDTH) return;
    for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) setPixel(x, y, state);
  }

  void drawRect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool state = true)
  {
    if (x0 > x1) { uint8_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { uint8_t t = y0; y0 = y1; y1 = t; }

    for (uint8_t x = x0; x <= x1 && x < MATRIX_WIDTH; x++)
    {
      if (y0 < MATRIX_HEIGHT) setPixel(x, y0, state);
      if (y1 < MATRIX_HEIGHT) setPixel(x, y1, state);
    }

    for (uint8_t y = y0; y <= y1 && y < MATRIX_HEIGHT; y++)
    {
      if (x0 < MATRIX_WIDTH) setPixel(x0, y, state);
      if (x1 < MATRIX_WIDTH) setPixel(x1, y, state);
    }
  }

  void allOffNow()
  {
    uint8_t data[SHIFT_BYTES];
    buildBlankBuffer(data);
    sendBuffer(data);
  }

private:
  uint8_t  _framebuffer[MATRIX_HEIGHT][COLUMN_BYTES];
  bool     _rotation180 = false;
  uint8_t  _currentRow = 0;
  uint32_t _lastRefreshUs = 0;

void outRowsOffOnly(uint8_t out[SHIFT_BYTES])
{
  // preserve currently displayed column bytes from framebuffer of next row is not necessary.
  // Use all zeros for columns if active high, ones if active low.
  for (uint8_t i = 1; i < SHIFT_BYTES; i++)
  {
    out[i] = COLUMN_ACTIVE_HIGH ? 0x00 : 0xFF;
  }

  out[0] = ROW_ACTIVE_HIGH ? 0x00 : 0xFF;
}
void refreshOneRow()
{
  uint8_t data[SHIFT_BYTES];

  // all rows OFF first
  outRowsOffOnly(data);
  sendBuffer(data);

  // now load columns + selected row together
  buildRowBuffer(_currentRow, data);
  sendBuffer(data);

  _currentRow++;
  if (_currentRow >= MATRIX_HEIGHT) _currentRow = 0;
}
  void buildBlankBuffer(uint8_t out[SHIFT_BYTES])
{
  // all columns off
  for (uint8_t i = 1; i < SHIFT_BYTES; i++)
  {
    out[i] = COLUMN_ACTIVE_HIGH ? 0x00 : 0xFF;
  }

  // all rows off
  out[0] = ROW_ACTIVE_HIGH ? 0x00 : 0xFF;
}

  void buildRowBuffer(uint8_t row, uint8_t out[SHIFT_BYTES])
  {
    const uint8_t rowMask = (1U << row);
    out[0] = ROW_ACTIVE_HIGH ? rowMask : (uint8_t)~rowMask;

    for (uint8_t byteIndex = 0; byteIndex < COLUMN_BYTES; byteIndex++)
    {
      const uint8_t colData = _framebuffer[row][byteIndex];
      out[8 - byteIndex] = COLUMN_ACTIVE_HIGH ? colData : (uint8_t)~colData;
    }
  }

  void sendBuffer(const uint8_t data[SHIFT_BYTES])
  {
    digitalWrite(LATCH_PIN, LOW);

    for (uint8_t i = 0; i < SHIFT_BYTES; i++)
    {
      shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, data[i]);
    }

    digitalWrite(LATCH_PIN, HIGH);
  }
};

// ============================================================
// Flexible fixed-size text renderer
// ============================================================
class TextRenderer
{
public:
  explicit TextRenderer(LedMatrix8x64& matrix) : _matrix(matrix) {}

  void drawChar(int16_t x, int16_t y, char c, const FixedFont& font)
  {
    if (c < font.firstChar || c > font.lastChar) return;
    if (font.height > 8) return;

    const uint16_t charIndex = (uint16_t)(c - font.firstChar);
    const uint16_t base = charIndex * font.width;

    for (uint8_t col = 0; col < font.width; col++)
    {
      const uint8_t line = font.data[base + col];

      for (uint8_t row = 0; row < font.height; row++)
      {
        const bool pixel = (line & (1U << row)) != 0;
        _matrix.setPixel(x + col, y + row, pixel);
      }
    }
  }

  void drawText(int16_t x,
                int16_t y,
                const char* text,
                const FixedFont& font,
                uint8_t spacing = 1)
  {
    int16_t cursorX = x;

    while (*text)
    {
      drawChar(cursorX, y, *text, font);
      cursorX += font.width + spacing;
      text++;
    }
  }

  int16_t getTextWidth(const char* text,
                       const FixedFont& font,
                       uint8_t spacing = 1) const
  {
    const size_t len = strlen(text);
    if (len == 0) return 0;
    return (int16_t)(len * font.width + (len - 1) * spacing);
  }

private:
  LedMatrix8x64& _matrix;
};

// ============================================================
// Flexible text scroller
// ============================================================
class TextScroller
{
public:
  TextScroller(LedMatrix8x64& matrix, TextRenderer& renderer)
    : _matrix(matrix), _renderer(renderer),
      _font(nullptr), _offsetX(MATRIX_WIDTH), _y(0),
      _speedMs(60), _spacing(1), _lastStepMs(0)
  {
    strncpy(_text, "HELLO", sizeof(_text) - 1);
    _text[sizeof(_text) - 1] = '\0';
  }

  void setFont(const FixedFont& font)
  {
    _font = &font;
  }

  void setText(const char* text)
  {
    strncpy(_text, text, sizeof(_text) - 1);
    _text[sizeof(_text) - 1] = '\0';
    reset();
  }

  void setSpeed(uint16_t speedMs)
  {
    _speedMs = speedMs;
  }

  void setSpacing(uint8_t spacing)
  {
    _spacing = spacing;
  }

  void setY(int16_t y)
  {
    _y = y;
  }

  void reset()
  {
    _offsetX = MATRIX_WIDTH;
    _lastStepMs = 0;
  }

  // Returns true after one complete pass across the display.
  bool update()
  {
    if (_font == nullptr) return false;

    const uint32_t now = millis();
    if ((uint32_t)(now - _lastStepMs) < _speedMs) return false;
    _lastStepMs = now;

    _matrix.clear();
    _renderer.drawText(_offsetX, _y, _text, *_font, _spacing);
    _offsetX--;

    const int16_t textWidth = _renderer.getTextWidth(_text, *_font, _spacing);
    if (_offsetX < -textWidth)
    {
      _offsetX = MATRIX_WIDTH;
      return true;
    }
    return false;
  }

private:
  LedMatrix8x64& _matrix;
  TextRenderer& _renderer;
  const FixedFont* _font;

  char _text[128];
  int16_t _offsetX;
  int16_t _y;
  uint16_t _speedMs;
  uint8_t _spacing;
  uint32_t _lastStepMs;
};

// ============================================================
// Application + STA/AP web control
// ============================================================
LedMatrix8x64 matrix;
TextRenderer text(matrix);
TextScroller scroller(matrix, text);
TextScroller staIpScroller(matrix, text);
MatrixWebServer webServer(80);
DNSServer dnsServer;

static const uint16_t EEPROM_SIZE = 512;
static const uint16_t DNS_PORT = 53;
static const uint32_t CONFIG_MAGIC = 0x4C4D3634UL; // "LM64"
static const uint16_t CONFIG_VERSION = 6;

struct LegacyConfigV1
{
  uint32_t magic;
  uint16_t version;
  char staSsid[33];
  char staPassword[65];
  char apSsid[33];
  char apPassword[65];
};

struct LegacyConfigV2
{
  uint32_t magic;
  uint16_t version;
  char staSsid[33];
  char staPassword[65];
  char apSsid[33];
  char apPassword[65];
  char ntpServer[64];
  int16_t timezoneOffsetMinutes;
};

struct LegacyConfigV3
{
  uint32_t magic;
  uint16_t version;
  char staSsid[33];
  char staPassword[65];
  char apSsid[33];
  char apPassword[65];
  char ntpServer[64];
  int16_t timezoneOffsetMinutes;
  uint8_t lastMode;
  char lastDisplayText[128];
  uint16_t lastDisplaySpeedMs;
  uint32_t timerDurationSeconds;
};

struct LegacyConfigV4
{
  uint32_t magic;
  uint16_t version;
  char staSsid[33];
  char staPassword[65];
  char apSsid[33];
  char apPassword[65];
  char ntpServer[64];
  int16_t timezoneOffsetMinutes;
  uint8_t lastMode;
  char lastDisplayText[128];
  uint16_t lastDisplaySpeedMs;
  uint32_t timerDurationSeconds;
  uint8_t paintBitmap[MATRIX_HEIGHT * COLUMN_BYTES];
};

struct LegacyConfigV5
{
  uint32_t magic;
  uint16_t version;
  char staSsid[33];
  char staPassword[65];
  char apSsid[33];
  char apPassword[65];
  char ntpServer[64];
  int16_t timezoneOffsetMinutes;
  uint8_t lastMode;
  char lastDisplayText[128];
  uint16_t lastDisplaySpeedMs;
  uint32_t timerDurationSeconds;
  uint8_t paintBitmap[MATRIX_HEIGHT * COLUMN_BYTES];
  uint8_t rotation180;
};

struct StoredConfig
{
  uint32_t magic;
  uint16_t version;
  char staSsid[33];
  char staPassword[65];
  char apSsid[33];
  char apPassword[65];
  char ntpServer[64];
  int16_t timezoneOffsetMinutes;
  uint8_t lastMode;
  char lastDisplayText[128];
  uint16_t lastDisplaySpeedMs;
  uint32_t timerDurationSeconds;
  uint8_t paintBitmap[MATRIX_HEIGHT * COLUMN_BYTES];
  uint8_t rotation180;
  uint8_t timerPresentation;
};

static_assert(sizeof(StoredConfig) <= EEPROM_SIZE, "StoredConfig exceeds EEPROM allocation");
StoredConfig config;

enum AppMode
{
  MODE_TEST_PATTERN = 0,
  MODE_STATIC_TEXT = 1,
  MODE_SCROLL_TEXT = 2,
  MODE_SPACE_GAME = 3,
  MODE_TREX_GAME = 4,
  MODE_RESERVED_5 = 5, // Reserved so older EEPROM mode numbers stay compatible.
  MODE_TIME = 6,
  MODE_TIMER = 7,
  MODE_STOPWATCH = 8,
  MODE_GRAVITY_GAME = 9, // Reuses display mode number 9 for EEPROM compatibility.
  MODE_TETRIS_GAME = 10,
  MODE_PAINT = 11,
  MODE_CALENDAR = 12,
  MODE_SCOREBOARD = 13,
  MODE_RACING_GAME = 14,
  MODE_ANIMATION = 15
};

enum SpaceGameState
{
  SPACE_READY,
  SPACE_RUNNING,
  SPACE_PAUSED,
  SPACE_GAME_OVER
};

struct SpaceObstacle
{
  int16_t x;
  uint8_t y;
  uint8_t type;
  bool active;
};

enum TrexGameState
{
  TREX_READY,
  TREX_RUNNING,
  TREX_PAUSED,
  TREX_GAME_OVER
};

enum ChronoState
{
  CHRONO_READY,
  CHRONO_RUNNING,
  CHRONO_PAUSED,
  CHRONO_FINISHED
};

enum TimerPresentation : uint8_t
{
  TIMER_PRESENTATION_TIME = 0,
  TIMER_PRESENTATION_BAR = 1
};

struct TrexObstacle
{
  int16_t x;
  uint8_t height;
  bool active;
};

enum GravityGameState
{
  GRAVITY_READY,
  GRAVITY_RUNNING,
  GRAVITY_PAUSED,
  GRAVITY_GAME_OVER
};

enum GravityObstacleType : uint8_t
{
  GRAVITY_WALL = 0,
  GRAVITY_PLATFORM = 1,
  GRAVITY_STAIRS = 2
};

struct GravityObstacle
{
  int16_t x;
  uint8_t height;
  uint8_t width;
  uint8_t type;
  bool fromTop;
  bool active;
};

enum TetrisGameState
{
  TETRIS_READY,
  TETRIS_RUNNING,
  TETRIS_PAUSED,
  TETRIS_GAME_OVER
};

struct TetrisPiece
{
  uint8_t type;
  uint8_t rotation;
  int8_t x;
  int8_t y;
};

enum RacingGameState
{
  RACING_READY,
  RACING_RUNNING,
  RACING_PAUSED,
  RACING_GAME_OVER
};

struct RacingObstacle
{
  int16_t x;
  uint8_t y;
  uint8_t height;
  bool active;
};

enum AnimationState
{
  ANIMATION_PLAYING,
  ANIMATION_PAUSED
};


enum TextPresentation : uint8_t
{
  TEXT_PRESENTATION_STATIC = 1,
  TEXT_PRESENTATION_RUNNING = 2
};

struct DisplayMessage
{
  char value[128];
  TextPresentation presentation;
  uint16_t speedMs;
  uint16_t holdMs;
  uint8_t repeatCount;
};

static const uint8_t DISPLAY_QUEUE_CAPACITY = 6;

// ESP-NOW packet: packed and kept well below the ESP-NOW payload limit.
static const uint32_t ESPNOW_DISPLAY_MAGIC = 0x4C4D4553UL; // "LMES"
static const uint8_t ESPNOW_DISPLAY_VERSION = 1;
static const uint8_t ESPNOW_FLAG_STACK = 0x01;

enum EspNowDisplayCommand : uint8_t
{
  ESPNOW_SHOW_STATIC = 1,
  ESPNOW_SHOW_RUNNING = 2,
  ESPNOW_SHOW_TIME = 3,
  ESPNOW_CLEAR_STACK = 4
};

struct __attribute__((packed)) EspNowDisplayPacket
{
  uint32_t magic;
  uint8_t version;
  uint8_t command;
  uint8_t flags;
  uint8_t repeatCount;
  uint16_t speedMs;
  uint16_t durationMs;
  char text[96];
};

static const uint8_t MAX_OBSTACLES = 10;
static const uint8_t SHIP_X = 3;
static const uint8_t MAX_TREX_OBSTACLES = 6;
static const uint8_t TREX_X = 4;
static const uint8_t TREX_GROUND_Y = 7;
static const uint8_t TREX_JUMP_STEPS = 8;
static const uint8_t MAX_GRAVITY_OBSTACLES = 8;
static const uint8_t GRAVITY_PLAYER_X = 5;
static const uint8_t GRAVITY_TOP_Y = 1;
static const uint8_t GRAVITY_BOTTOM_Y = 6;

static const uint8_t TETRIS_WIDTH = MATRIX_WIDTH;
static const uint8_t TETRIS_HEIGHT = MATRIX_HEIGHT;
static const uint8_t MAX_RACING_OBSTACLES = 8;
static const uint8_t RACING_PLAYER_X = 4;

AppMode mode = MODE_SCROLL_TEXT;
char displayText[128] = "Hello world";
uint16_t displaySpeedMs = 60;

SpaceObstacle obstacles[MAX_OBSTACLES];
SpaceGameState spaceGameState = SPACE_READY;
uint8_t shipY = 4;
uint32_t spaceScore = 0;
uint16_t spaceStartStepMs = 120;
uint16_t spaceStepMs = 120;
uint16_t stepsUntilSpawn = 10;
uint32_t lastSpaceStepMs = 0;

TrexObstacle trexObstacles[MAX_TREX_OBSTACLES];
TrexGameState trexGameState = TREX_READY;
uint32_t trexScore = 0;
uint16_t trexStartStepMs = 110;
uint16_t trexStepMs = 110;
uint16_t trexStepsUntilSpawn = 14;
uint32_t lastTrexStepMs = 0;
uint8_t trexJumpPhase = 0;
bool trexJumping = false;

GravityObstacle gravityObstacles[MAX_GRAVITY_OBSTACLES];
GravityGameState gravityGameState = GRAVITY_READY;
uint32_t gravityScore = 0;
uint16_t gravityStartStepMs = 90;
uint16_t gravityStepMs = 90;
uint16_t gravityStepsUntilSpawn = 14;
uint32_t lastGravityStepMs = 0;
uint8_t gravityPlayerY = GRAVITY_BOTTOM_Y;
bool gravityPullsTop = false;

uint8_t tetrisBoard[TETRIS_WIDTH];
TetrisPiece tetrisPiece = {0, 0, 60, 2};
TetrisGameState tetrisGameState = TETRIS_READY;
uint32_t tetrisScore = 0;
uint16_t tetrisLines = 0;
uint16_t tetrisStepMs = 450;
uint32_t lastTetrisStepMs = 0;

RacingObstacle racingObstacles[MAX_RACING_OBSTACLES];
RacingGameState racingGameState = RACING_READY;
uint8_t racingPlayerY = 4;
uint32_t racingScore = 0;
uint16_t racingStartStepMs = 120;
uint16_t racingStepMs = 120;
uint16_t racingStepsUntilSpawn = 10;
uint32_t lastRacingStepMs = 0;

uint8_t scoreboardLeftScore = 0;
uint8_t scoreboardRightScore = 0;

AnimationState animationState = ANIMATION_PLAYING;
uint8_t animationFrame = 0;
uint16_t animationSpeedMs = 220;
uint32_t lastAnimationFrameMs = 0;

uint8_t paintBitmap[MATRIX_HEIGHT][COLUMN_BYTES];
uint8_t paintCursorX = 0;
uint8_t paintCursorY = 0;
bool paintCursorVisible = true;
uint32_t lastPaintCursorBlinkMs = 0;
bool paintSavePending = false;
uint32_t paintSaveDueMs = 0;
static const uint32_t PAINT_CURSOR_BLINK_MS = 350;
static const uint32_t PAINT_SAVE_DELAY_MS = 1200;


char clockText[9] = "--:--:--";
char calendarText[9] = "--.--.--";
time_t lastRenderedClockSecond = 0;
uint32_t lastRenderedCalendarKey = 0xFFFFFFFFUL;
bool ntpConfiguredForConnection = false;
bool timeWasSetManually = false;
uint32_t lastNtpSyncRequestMs = 0;
static const uint32_t NTP_RESYNC_INTERVAL_MS = 12UL * 60UL * 60UL * 1000UL;

ChronoState timerState = CHRONO_READY;
uint32_t timerDurationSeconds = 300;
uint32_t timerRemainingMs = 300000;
uint32_t timerLastUpdateMs = 0;
uint32_t timerLastRenderedSecond = 0xFFFFFFFFUL;
TimerPresentation timerPresentation = TIMER_PRESENTATION_TIME;
bool timerBlinkVisible = true;
uint32_t timerLastBlinkMs = 0;
static const uint32_t TIMER_FINISHED_BLINK_MS = 450;
char timerText[9];

ChronoState stopwatchState = CHRONO_READY;
uint32_t stopwatchElapsedMs = 0;
uint32_t stopwatchStartedMs = 0;
uint32_t stopwatchLastRenderedSecond = 0xFFFFFFFFUL;
char stopwatchText[9];

DisplayMessage displayQueue[DISPLAY_QUEUE_CAPACITY];
uint8_t displayQueueHead = 0;
uint8_t displayQueueTail = 0;
uint8_t displayQueueCount = 0;
bool queuedMessageActive = false;
DisplayMessage activeQueuedMessage;
AppMode queuedResumeMode = MODE_SCROLL_TEXT;
uint32_t queuedMessageStartedMs = 0;
uint8_t queuedScrollPasses = 0;

bool espNowReady = false;
volatile bool espNowPacketPending = false;
EspNowDisplayPacket espNowPendingPacket;
uint8_t espNowPendingSender[6] = {0};
volatile uint32_t espNowDroppedPackets = 0;
uint32_t espNowReceivedPackets = 0;
char espNowLastSender[18] = "—";

bool restartPending = false;
uint32_t restartAtMs = 0;
uint32_t lastReconnectAttemptMs = 0;

// When nobody is connected through either interface, show the reachable IP
// address for a maximum of 20 seconds. AP clients are detected by the Wi-Fi
// driver. STA clients are considered present while the web UI keeps polling
// the HTTP server.
static const uint32_t WEB_CLIENT_ACTIVE_MS = 5000;
static const uint32_t IP_ANNOUNCEMENT_TIMEOUT_MS = 20000;
bool staWasConnected = false;
bool staIpAnnouncementActive = false;
uint32_t staIpAnnouncementStartedMs = 0;
uint32_t lastWebClientSeenMs = 0;
bool webClientSeen = false;
bool ipAnnouncementTimedOut = false;
char staIpAnnouncementText[48] = "AP IP";

void noteWebClient();

void copyString(char* destination, size_t capacity, const String& value)
{
  if (capacity == 0) return;
  size_t length = value.length();
  if (length >= capacity) length = capacity - 1;
  memcpy(destination, value.c_str(), length);
  destination[length] = '\0';
}

String defaultApSsid()
{
#if defined(ESP8266)
  const uint32_t id = ESP.getChipId();
#else
  const uint32_t id = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFUL);
#endif
  char suffix[9];
  snprintf(suffix, sizeof(suffix), "%06lX", (unsigned long)(id & 0xFFFFFFUL));
  return String("LED-Matrix-") + suffix;
}

void setDefaultConfig()
{
  memset(&config, 0, sizeof(config));
  config.magic = CONFIG_MAGIC;
  config.version = CONFIG_VERSION;
  copyString(config.apSsid, sizeof(config.apSsid), defaultApSsid());
  copyString(config.apPassword, sizeof(config.apPassword), "ledmatrix");
  copyString(config.ntpServer, sizeof(config.ntpServer), "pool.ntp.org");
  config.timezoneOffsetMinutes = 0;
  config.lastMode = (uint8_t)MODE_SCROLL_TEXT;
  copyString(config.lastDisplayText, sizeof(config.lastDisplayText), "Hello world");
  config.lastDisplaySpeedMs = 60;
  config.timerDurationSeconds = 300;
  memset(config.paintBitmap, 0, sizeof(config.paintBitmap));
  config.rotation180 = 0;
  config.timerPresentation = (uint8_t)TIMER_PRESENTATION_TIME;
}

void saveConfig()
{
  EEPROM.put(0, config);
  EEPROM.commit();
}

void loadConfig()
{
  EEPROM.begin(EEPROM_SIZE);

  uint32_t storedMagic = 0;
  uint16_t storedVersion = 0;
  EEPROM.get(0, storedMagic);
  EEPROM.get(sizeof(storedMagic), storedVersion);


  if (storedMagic == CONFIG_MAGIC && storedVersion == 5)
  {
    LegacyConfigV5 legacy;
    EEPROM.get(0, legacy);
    legacy.staSsid[sizeof(legacy.staSsid) - 1] = '\0';
    legacy.staPassword[sizeof(legacy.staPassword) - 1] = '\0';
    legacy.apSsid[sizeof(legacy.apSsid) - 1] = '\0';
    legacy.apPassword[sizeof(legacy.apPassword) - 1] = '\0';
    legacy.ntpServer[sizeof(legacy.ntpServer) - 1] = '\0';
    legacy.lastDisplayText[sizeof(legacy.lastDisplayText) - 1] = '\0';
    setDefaultConfig();
    copyString(config.staSsid, sizeof(config.staSsid), String(legacy.staSsid));
    copyString(config.staPassword, sizeof(config.staPassword), String(legacy.staPassword));
    copyString(config.apSsid, sizeof(config.apSsid), String(legacy.apSsid));
    copyString(config.apPassword, sizeof(config.apPassword), String(legacy.apPassword));
    copyString(config.ntpServer, sizeof(config.ntpServer), String(legacy.ntpServer));
    config.timezoneOffsetMinutes = legacy.timezoneOffsetMinutes;
    config.lastMode = legacy.lastMode;
    copyString(config.lastDisplayText, sizeof(config.lastDisplayText), String(legacy.lastDisplayText));
    config.lastDisplaySpeedMs = legacy.lastDisplaySpeedMs;
    config.timerDurationSeconds = legacy.timerDurationSeconds;
    memcpy(config.paintBitmap, legacy.paintBitmap, sizeof(config.paintBitmap));
    config.rotation180 = legacy.rotation180 ? 1 : 0;
    config.timerPresentation = (uint8_t)TIMER_PRESENTATION_TIME;
    saveConfig();
    return;
  }

  if (storedMagic == CONFIG_MAGIC && storedVersion == 4)
  {
    LegacyConfigV4 legacy;
    EEPROM.get(0, legacy);
    legacy.staSsid[sizeof(legacy.staSsid) - 1] = '\0';
    legacy.staPassword[sizeof(legacy.staPassword) - 1] = '\0';
    legacy.apSsid[sizeof(legacy.apSsid) - 1] = '\0';
    legacy.apPassword[sizeof(legacy.apPassword) - 1] = '\0';
    legacy.ntpServer[sizeof(legacy.ntpServer) - 1] = '\0';
    legacy.lastDisplayText[sizeof(legacy.lastDisplayText) - 1] = '\0';
    setDefaultConfig();
    copyString(config.staSsid, sizeof(config.staSsid), String(legacy.staSsid));
    copyString(config.staPassword, sizeof(config.staPassword), String(legacy.staPassword));
    copyString(config.apSsid, sizeof(config.apSsid), String(legacy.apSsid));
    copyString(config.apPassword, sizeof(config.apPassword), String(legacy.apPassword));
    copyString(config.ntpServer, sizeof(config.ntpServer), String(legacy.ntpServer));
    config.timezoneOffsetMinutes = legacy.timezoneOffsetMinutes;
    config.lastMode = legacy.lastMode;
    copyString(config.lastDisplayText, sizeof(config.lastDisplayText), String(legacy.lastDisplayText));
    config.lastDisplaySpeedMs = legacy.lastDisplaySpeedMs;
    config.timerDurationSeconds = legacy.timerDurationSeconds;
    memcpy(config.paintBitmap, legacy.paintBitmap, sizeof(config.paintBitmap));
    config.rotation180 = 0;
    saveConfig();
    return;
  }

  if (storedMagic == CONFIG_MAGIC && storedVersion == 3)
  {
    LegacyConfigV3 legacy;
    EEPROM.get(0, legacy);
    legacy.staSsid[sizeof(legacy.staSsid) - 1] = '\0';
    legacy.staPassword[sizeof(legacy.staPassword) - 1] = '\0';
    legacy.apSsid[sizeof(legacy.apSsid) - 1] = '\0';
    legacy.apPassword[sizeof(legacy.apPassword) - 1] = '\0';
    legacy.ntpServer[sizeof(legacy.ntpServer) - 1] = '\0';
    legacy.lastDisplayText[sizeof(legacy.lastDisplayText) - 1] = '\0';
    setDefaultConfig();
    copyString(config.staSsid, sizeof(config.staSsid), String(legacy.staSsid));
    copyString(config.staPassword, sizeof(config.staPassword), String(legacy.staPassword));
    copyString(config.apSsid, sizeof(config.apSsid), String(legacy.apSsid));
    copyString(config.apPassword, sizeof(config.apPassword), String(legacy.apPassword));
    copyString(config.ntpServer, sizeof(config.ntpServer), String(legacy.ntpServer));
    config.timezoneOffsetMinutes = legacy.timezoneOffsetMinutes;
    config.lastMode = legacy.lastMode;
    copyString(config.lastDisplayText, sizeof(config.lastDisplayText), String(legacy.lastDisplayText));
    config.lastDisplaySpeedMs = legacy.lastDisplaySpeedMs;
    config.timerDurationSeconds = legacy.timerDurationSeconds;
    saveConfig();
    return;
  }

  if (storedMagic == CONFIG_MAGIC && storedVersion == 2)
  {
    LegacyConfigV2 legacy;
    EEPROM.get(0, legacy);
    legacy.staSsid[sizeof(legacy.staSsid) - 1] = '\0';
    legacy.staPassword[sizeof(legacy.staPassword) - 1] = '\0';
    legacy.apSsid[sizeof(legacy.apSsid) - 1] = '\0';
    legacy.apPassword[sizeof(legacy.apPassword) - 1] = '\0';
    legacy.ntpServer[sizeof(legacy.ntpServer) - 1] = '\0';
    setDefaultConfig();
    copyString(config.staSsid, sizeof(config.staSsid), String(legacy.staSsid));
    copyString(config.staPassword, sizeof(config.staPassword), String(legacy.staPassword));
    copyString(config.apSsid, sizeof(config.apSsid), String(legacy.apSsid));
    copyString(config.apPassword, sizeof(config.apPassword), String(legacy.apPassword));
    copyString(config.ntpServer, sizeof(config.ntpServer), String(legacy.ntpServer));
    config.timezoneOffsetMinutes = legacy.timezoneOffsetMinutes;
    saveConfig();
    return;
  }

  if (storedMagic == CONFIG_MAGIC && storedVersion == 1)
  {
    LegacyConfigV1 legacy;
    EEPROM.get(0, legacy);
    legacy.staSsid[sizeof(legacy.staSsid) - 1] = '\0';
    legacy.staPassword[sizeof(legacy.staPassword) - 1] = '\0';
    legacy.apSsid[sizeof(legacy.apSsid) - 1] = '\0';
    legacy.apPassword[sizeof(legacy.apPassword) - 1] = '\0';
    setDefaultConfig();
    copyString(config.staSsid, sizeof(config.staSsid), String(legacy.staSsid));
    copyString(config.staPassword, sizeof(config.staPassword), String(legacy.staPassword));
    copyString(config.apSsid, sizeof(config.apSsid), String(legacy.apSsid));
    copyString(config.apPassword, sizeof(config.apPassword), String(legacy.apPassword));
    saveConfig();
    return;
  }

  EEPROM.get(0, config);
  config.staSsid[sizeof(config.staSsid) - 1] = '\0';
  config.staPassword[sizeof(config.staPassword) - 1] = '\0';
  config.apSsid[sizeof(config.apSsid) - 1] = '\0';
  config.apPassword[sizeof(config.apPassword) - 1] = '\0';
  config.ntpServer[sizeof(config.ntpServer) - 1] = '\0';
  config.lastDisplayText[sizeof(config.lastDisplayText) - 1] = '\0';

  const bool invalid =
    config.magic != CONFIG_MAGIC ||
    config.version != CONFIG_VERSION ||
    config.apSsid[0] == '\0';

  if (invalid)
  {
    setDefaultConfig();
    saveConfig();
    return;
  }

  if (config.ntpServer[0] == '\0')
    copyString(config.ntpServer, sizeof(config.ntpServer), "pool.ntp.org");

  if (config.timezoneOffsetMinutes < -720 || config.timezoneOffsetMinutes > 840)
    config.timezoneOffsetMinutes = 0;

  if (config.lastMode > (uint8_t)MODE_ANIMATION ||
      config.lastMode == (uint8_t)MODE_RESERVED_5)
    config.lastMode = (uint8_t)MODE_SCROLL_TEXT;
  if (config.lastDisplayText[0] == '\0')
    copyString(config.lastDisplayText, sizeof(config.lastDisplayText), "Hello world");
  if (config.lastDisplaySpeedMs < 15 || config.lastDisplaySpeedMs > 500)
    config.lastDisplaySpeedMs = 60;
  if (config.timerDurationSeconds < 1 || config.timerDurationSeconds > 359999)
    config.timerDurationSeconds = 300;
  config.rotation180 = config.rotation180 ? 1 : 0;
  if (config.timerPresentation > (uint8_t)TIMER_PRESENTATION_BAR)
    config.timerPresentation = (uint8_t)TIMER_PRESENTATION_TIME;
}

bool isRestorableMode(uint8_t value)
{
  return value <= (uint8_t)MODE_ANIMATION &&
    value != (uint8_t)MODE_TEST_PATTERN &&
    value != (uint8_t)MODE_RESERVED_5;
}

void updateStoredDisplayState()
{
  config.lastMode = isRestorableMode((uint8_t)mode)
    ? (uint8_t)mode
    : (uint8_t)MODE_SCROLL_TEXT;
  copyString(config.lastDisplayText, sizeof(config.lastDisplayText), String(displayText));
  config.lastDisplaySpeedMs = constrain(displaySpeedMs, (uint16_t)15, (uint16_t)500);
  config.timerDurationSeconds = constrain(timerDurationSeconds, (uint32_t)1, (uint32_t)359999);
  config.timerPresentation = (uint8_t)timerPresentation;
  memcpy(config.paintBitmap, paintBitmap, sizeof(config.paintBitmap));
}

void saveDisplayState()
{
  const uint8_t storedMode = isRestorableMode((uint8_t)mode)
    ? (uint8_t)mode
    : (uint8_t)MODE_SCROLL_TEXT;
  const uint16_t storedSpeed = constrain(displaySpeedMs, (uint16_t)15, (uint16_t)500);
  const uint32_t storedTimer = constrain(timerDurationSeconds, (uint32_t)1, (uint32_t)359999);
  const bool changed =
    config.lastMode != storedMode ||
    strcmp(config.lastDisplayText, displayText) != 0 ||
    config.lastDisplaySpeedMs != storedSpeed ||
    config.timerDurationSeconds != storedTimer ||
    config.timerPresentation != (uint8_t)timerPresentation ||
    memcmp(config.paintBitmap, paintBitmap, sizeof(config.paintBitmap)) != 0;
  if (!changed)
  {
    paintSavePending = false;
    return;
  }

  updateStoredDisplayState();
  saveConfig();
  paintSavePending = false;
}

String jsonEscape(const String& value)
{
  String result;
  result.reserve(value.length() + 8);

  for (size_t i = 0; i < value.length(); i++)
  {
    const char c = value[i];
    switch (c)
    {
      case '"': result += F("\\\""); break;
      case '\\': result += F("\\\\"); break;
      case '\n': result += F("\\n"); break;
      case '\r': result += F("\\r"); break;
      case '\t': result += F("\\t"); break;
      default:
        if ((uint8_t)c >= 0x20) result += c;
        break;
    }
  }
  return result;
}

void sendJson(int statusCode, const String& body)
{
  noteWebClient();
  webServer.sendHeader(F("Cache-Control"), F("no-store"));
  webServer.send(statusCode, F("application/json"), body);
}

void sendWebUi()
{
  noteWebClient();
  webServer.sendHeader(F("Cache-Control"), F("no-store"));
  webServer.send_P(200, "text/html; charset=utf-8", WEB_UI_HTML);
}

void sanitizeText(const String& source, char* destination, size_t capacity)
{
  if (capacity == 0) return;
  size_t output = 0;
  for (size_t i = 0; i < source.length() && output < capacity - 1; i++)
  {
    char c = source[i];
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c < 32 || c > 90) c = '?';
    destination[output++] = c;
  }

  if (output == 0) destination[output++] = ' ';
  destination[output] = '\0';
}

void sanitizeDisplayText(const String& source)
{
  sanitizeText(source, displayText, sizeof(displayText));
}

void loadTestPattern()
{
  matrix.clear();
  matrix.drawRect(0, 0, 63, 7, true);
  matrix.drawHorizontalLine(3, true);
  matrix.drawVerticalLine(31, true);

  for (uint8_t i = 0; i < 8; i++) matrix.setPixel(i, i, true);
}

void renderStaticText(const char* value)
{
  matrix.clear();
  text.drawText(0, 0, value, FONT_6X8, 1);
}

void loadStaticText()
{
  renderStaticText(displayText);
}

void configureRunningText(const char* value, uint16_t speedMs)
{
  scroller.setFont(FONT_6X8);
  scroller.setText(value);
  scroller.setSpacing(1);
  scroller.setY(0);
  scroller.setSpeed(speedMs);
  scroller.reset();
}

void applyDisplaySettings();

void clearDisplayStackStorage()
{
  displayQueueHead = 0;
  displayQueueTail = 0;
  displayQueueCount = 0;
}

uint8_t getDisplayStackDepth()
{
  return displayQueueCount + (queuedMessageActive ? 1 : 0);
}

void cancelDisplayStack(bool restorePreviousMode)
{
  const bool wasActive = queuedMessageActive;
  clearDisplayStackStorage();
  queuedMessageActive = false;
  queuedScrollPasses = 0;

  if (restorePreviousMode && wasActive)
  {
    mode = queuedResumeMode;
    applyDisplaySettings();
  }
}

void showTextStatic(const char* value)
{
  cancelDisplayStack(false);
  sanitizeDisplayText(String(value));
  mode = MODE_STATIC_TEXT;
  renderStaticText(displayText);
  saveDisplayState();
}

void showTextRunning(const char* value, uint16_t speedMs = 60)
{
  cancelDisplayStack(false);
  sanitizeDisplayText(String(value));
  if (speedMs < 15) speedMs = 15;
  if (speedMs > 500) speedMs = 500;
  displaySpeedMs = speedMs;
  mode = MODE_SCROLL_TEXT;
  configureRunningText(displayText, displaySpeedMs);
  saveDisplayState();
}

void showText(const char* value, TextPresentation presentation, uint16_t speedMs = 60)
{
  if (presentation == TEXT_PRESENTATION_STATIC) showTextStatic(value);
  else showTextRunning(value, speedMs);
}

void activateQueuedMessage(const DisplayMessage& message)
{
  activeQueuedMessage = message;
  queuedMessageActive = true;
  queuedMessageStartedMs = millis();
  queuedScrollPasses = 0;

  if (message.presentation == TEXT_PRESENTATION_STATIC)
  {
    mode = MODE_STATIC_TEXT;
    renderStaticText(message.value);
  }
  else
  {
    mode = MODE_SCROLL_TEXT;
    configureRunningText(message.value, message.speedMs);
  }
}

void startNextStackedMessage()
{
  if (displayQueueCount == 0)
  {
    queuedMessageActive = false;
    mode = queuedResumeMode;
    applyDisplaySettings();
    return;
  }

  const DisplayMessage next = displayQueue[displayQueueHead];
  displayQueueHead = (uint8_t)((displayQueueHead + 1) % DISPLAY_QUEUE_CAPACITY);
  displayQueueCount--;
  activateQueuedMessage(next);
}

bool stackTextMessage(const char* value, TextPresentation presentation,
                      uint16_t speedMs = 60, uint16_t holdMs = 3000,
                      uint8_t repeatCount = 1)
{
  if (displayQueueCount >= DISPLAY_QUEUE_CAPACITY) return false;

  DisplayMessage& message = displayQueue[displayQueueTail];
  sanitizeText(String(value), message.value, sizeof(message.value));
  message.presentation = presentation;
  message.speedMs = constrain(speedMs, (uint16_t)15, (uint16_t)500);
  message.holdMs = constrain(holdMs, (uint16_t)250, (uint16_t)60000);
  message.repeatCount = constrain(repeatCount, (uint8_t)1, (uint8_t)10);

  displayQueueTail = (uint8_t)((displayQueueTail + 1) % DISPLAY_QUEUE_CAPACITY);
  displayQueueCount++;

  if (!queuedMessageActive)
  {
    queuedResumeMode = mode;
    startNextStackedMessage();
  }
  return true;
}

bool stackTextStatic(const char* value, uint16_t holdMs = 3000)
{
  return stackTextMessage(value, TEXT_PRESENTATION_STATIC, 60, holdMs, 1);
}

bool stackTextRunning(const char* value, uint16_t speedMs = 60, uint8_t repeatCount = 1)
{
  return stackTextMessage(value, TEXT_PRESENTATION_RUNNING, speedMs, 3000, repeatCount);
}

bool updateDisplayStack()
{
  if (!queuedMessageActive) return false;

  if (activeQueuedMessage.presentation == TEXT_PRESENTATION_STATIC)
  {
    if ((uint32_t)(millis() - queuedMessageStartedMs) >= activeQueuedMessage.holdMs)
      startNextStackedMessage();
  }
  else if (scroller.update())
  {
    queuedScrollPasses++;
    if (queuedScrollPasses >= activeQueuedMessage.repeatCount)
      startNextStackedMessage();
  }
  return true;
}

bool isClockValid()
{
  return time(nullptr) >= 1700000000;
}

void updateClockText()
{
  if (!isClockValid())
  {
    memcpy(clockText, "--:--:--", sizeof(clockText));
    return;
  }

  const time_t utcNow = time(nullptr);
  const time_t localNow = utcNow + (time_t)config.timezoneOffsetMinutes * 60;
  struct tm timeInfo;
  gmtime_r(&localNow, &timeInfo);
  snprintf(clockText, sizeof(clockText), "%02d:%02d:%02d",
           timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
}

void renderClock()
{
  updateClockText();
  matrix.clear();
  const int16_t width = text.getTextWidth(clockText, FONT_6X8, 1);
  text.drawText((MATRIX_WIDTH - width) / 2, 0, clockText, FONT_6X8, 1);
  lastRenderedClockSecond = time(nullptr);
}

void updateClockDisplay()
{
  if (mode != MODE_TIME) return;
  const time_t currentSecond = time(nullptr);
  if (currentSecond != lastRenderedClockSecond) renderClock();
}

uint32_t localCalendarKey()
{
  if (!isClockValid()) return 0xFFFFFFFFUL;
  const time_t localNow = time(nullptr) + (time_t)config.timezoneOffsetMinutes * 60;
  struct tm timeInfo;
  gmtime_r(&localNow, &timeInfo);
  return ((uint32_t)(timeInfo.tm_year + 1900) << 9) |
         (uint32_t)(timeInfo.tm_yday & 0x1FF);
}

void updateCalendarText()
{
  if (!isClockValid())
  {
    memcpy(calendarText, "--.--.--", sizeof(calendarText));
    return;
  }

  const time_t localNow = time(nullptr) + (time_t)config.timezoneOffsetMinutes * 60;
  struct tm timeInfo;
  gmtime_r(&localNow, &timeInfo);
  snprintf(calendarText, sizeof(calendarText), "%02d.%02d.%02d",
           timeInfo.tm_mday,
           timeInfo.tm_mon + 1,
           (timeInfo.tm_year + 1900) % 100);
}

void renderCalendar()
{
  updateCalendarText();
  matrix.clear();
  const int16_t width = text.getTextWidth(calendarText, FONT_6X8, 1);
  text.drawText((MATRIX_WIDTH - width) / 2, 0, calendarText, FONT_6X8, 1);
  lastRenderedCalendarKey = localCalendarKey();
}

void updateCalendarDisplay()
{
  if (mode != MODE_CALENDAR) return;
  const uint32_t key = localCalendarKey();
  if (key != lastRenderedCalendarKey) renderCalendar();
}


void formatDuration(uint32_t totalSeconds, char* output, size_t capacity)
{
  const uint32_t maximum = 99UL * 3600UL + 59UL * 60UL + 59UL;
  if (totalSeconds > maximum) totalSeconds = maximum;
  const uint8_t hours = (uint8_t)(totalSeconds / 3600UL);
  const uint8_t minutes = (uint8_t)((totalSeconds / 60UL) % 60UL);
  const uint8_t seconds = (uint8_t)(totalSeconds % 60UL);
  snprintf(output, capacity, "%02u:%02u:%02u",
           (unsigned)hours, (unsigned)minutes, (unsigned)seconds);
}

const char* chronoStateName(ChronoState state)
{
  switch (state)
  {
    case CHRONO_RUNNING: return "running";
    case CHRONO_PAUSED: return "paused";
    case CHRONO_FINISHED: return "finished";
    default: return "ready";
  }
}

void resetTimerBlink()
{
  timerBlinkVisible = true;
  timerLastBlinkMs = millis();
}

void advanceTimer()
{
  if (timerState != CHRONO_RUNNING) return;
  const uint32_t now = millis();
  const uint32_t elapsed = (uint32_t)(now - timerLastUpdateMs);
  timerLastUpdateMs = now;

  if (elapsed >= timerRemainingMs)
  {
    timerRemainingMs = 0;
    timerState = CHRONO_FINISHED;
    timerLastRenderedSecond = 0xFFFFFFFFUL;
    resetTimerBlink();
  }
  else
  {
    timerRemainingMs -= elapsed;
  }
}

uint32_t timerRemainingSeconds()
{
  advanceTimer();
  if (timerRemainingMs == 0) return 0;
  return (timerRemainingMs + 999UL) / 1000UL;
}

uint8_t timerProgressPercent()
{
  advanceTimer();
  if (timerDurationSeconds == 0) return 0;
  const uint32_t totalMs = timerDurationSeconds * 1000UL;
  if (timerRemainingMs >= totalMs) return 100;
  return (uint8_t)(((uint64_t)timerRemainingMs * 100ULL + totalMs / 2UL) / totalMs);
}

void renderTimerBar()
{
  matrix.clear();
  matrix.drawHorizontalLine(0, true);
  matrix.drawHorizontalLine(MATRIX_HEIGHT - 1, true);
  matrix.setPixel(0, 1, true);
  matrix.setPixel(0, MATRIX_HEIGHT - 2, true);
  matrix.setPixel(MATRIX_WIDTH - 1, 1, true);
  matrix.setPixel(MATRIX_WIDTH - 1, MATRIX_HEIGHT - 2, true);

  const uint8_t percent = timerProgressPercent();
  const uint8_t innerWidth = MATRIX_WIDTH - 2;
  const uint8_t filledColumns = (uint8_t)(((uint16_t)innerWidth * percent + 99U) / 100U);
  for (uint8_t x = 1; x <= filledColumns && x < MATRIX_WIDTH - 1; x++)
    for (uint8_t y = 1; y < MATRIX_HEIGHT - 1; y++)
      matrix.setPixel(x, y, true);
}

void renderTimer()
{
  const uint32_t seconds = timerRemainingSeconds();
  formatDuration(seconds, timerText, sizeof(timerText));
  matrix.clear();

  if (timerState != CHRONO_FINISHED || timerBlinkVisible)
  {
    if (timerPresentation == TIMER_PRESENTATION_BAR)
      renderTimerBar();
    else
    {
      const int16_t width = text.getTextWidth(timerText, FONT_6X8, 1);
      text.drawText((MATRIX_WIDTH - width) / 2, 0, timerText, FONT_6X8, 1);
    }
  }

  timerLastRenderedSecond = seconds;
}

void updateTimerDisplay()
{
  if (mode != MODE_TIMER) return;
  const uint32_t seconds = timerRemainingSeconds();
  const uint32_t now = millis();

  if (timerState == CHRONO_FINISHED)
  {
    if ((uint32_t)(now - timerLastBlinkMs) >= TIMER_FINISHED_BLINK_MS)
    {
      timerLastBlinkMs = now;
      timerBlinkVisible = !timerBlinkVisible;
      renderTimer();
    }
    return;
  }

  if (!timerBlinkVisible) timerBlinkVisible = true;
  if (seconds != timerLastRenderedSecond) renderTimer();
}

uint32_t stopwatchElapsedMilliseconds()
{
  if (stopwatchState != CHRONO_RUNNING) return stopwatchElapsedMs;
  return stopwatchElapsedMs + (uint32_t)(millis() - stopwatchStartedMs);
}

uint32_t stopwatchElapsedSeconds()
{
  const uint32_t maximum = 99UL * 3600UL + 59UL * 60UL + 59UL;
  uint32_t seconds = stopwatchElapsedMilliseconds() / 1000UL;
  if (seconds > maximum) seconds = maximum;
  return seconds;
}

void renderStopwatch()
{
  const uint32_t seconds = stopwatchElapsedSeconds();
  formatDuration(seconds, stopwatchText, sizeof(stopwatchText));
  matrix.clear();
  const int16_t width = text.getTextWidth(stopwatchText, FONT_6X8, 1);
  text.drawText((MATRIX_WIDTH - width) / 2, 0, stopwatchText, FONT_6X8, 1);
  stopwatchLastRenderedSecond = seconds;
}

void updateStopwatchDisplay()
{
  if (mode != MODE_STOPWATCH) return;
  const uint32_t seconds = stopwatchElapsedSeconds();
  if (seconds != stopwatchLastRenderedSecond) renderStopwatch();
}

bool shipPixelAt(int16_t x, int16_t y)
{
  return
    (x == SHIP_X - 1 && y == shipY) ||
    (x == SHIP_X && y >= (int16_t)shipY - 1 && y <= (int16_t)shipY + 1) ||
    (x == SHIP_X + 1 && y == shipY);
}

bool obstaclePixelAt(const SpaceObstacle& obstacle, int16_t x, int16_t y)
{
  if (!obstacle.active) return false;

  if (obstacle.type == 0)
  {
    return x >= obstacle.x && x <= obstacle.x + 1 &&
           y >= obstacle.y && y <= obstacle.y + 1;
  }

  return
    (x == obstacle.x + 1 && y == obstacle.y) ||
    (x >= obstacle.x && x <= obstacle.x + 2 && y == obstacle.y + 1) ||
    (x == obstacle.x + 1 && y == obstacle.y + 2);
}

bool obstacleHitsShip(const SpaceObstacle& obstacle)
{
  if (!obstacle.active) return false;
  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++)
  {
    for (uint8_t x = SHIP_X - 1; x <= SHIP_X + 1; x++)
    {
      if (shipPixelAt(x, y) && obstaclePixelAt(obstacle, x, y)) return true;
    }
  }
  return false;
}

const char* spaceStateName()
{
  switch (spaceGameState)
  {
    case SPACE_RUNNING: return "running";
    case SPACE_PAUSED: return "paused";
    case SPACE_GAME_OVER: return "gameover";
    default: return "ready";
  }
}

void renderSpaceGame()
{
  matrix.clear();

  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++)
  {
    for (uint8_t x = SHIP_X - 1; x <= SHIP_X + 1; x++)
      if (shipPixelAt(x, y)) matrix.setPixel(x, y, true);
  }

  for (uint8_t i = 0; i < MAX_OBSTACLES; i++)
  {
    if (!obstacles[i].active) continue;
    for (uint8_t y = 0; y < MATRIX_HEIGHT; y++)
    {
      for (int16_t x = obstacles[i].x; x <= obstacles[i].x + 2; x++)
        if (obstaclePixelAt(obstacles[i], x, y)) matrix.setPixel(x, y, true);
    }
  }

  if (spaceGameState == SPACE_GAME_OVER)
  {
    matrix.setPixel(0, 0, true);
    matrix.setPixel(1, 1, true);
    matrix.setPixel(0, 2, true);
    matrix.setPixel(MATRIX_WIDTH - 1, 5, true);
    matrix.setPixel(MATRIX_WIDTH - 2, 6, true);
    matrix.setPixel(MATRIX_WIDTH - 1, 7, true);
  }
}

void clearObstacles()
{
  for (uint8_t i = 0; i < MAX_OBSTACLES; i++) obstacles[i].active = false;
}

void resetSpaceGame()
{
  mode = MODE_SPACE_GAME;
  shipY = MATRIX_HEIGHT / 2;
  spaceScore = 0;
  spaceStepMs = spaceStartStepMs;
  spaceGameState = SPACE_READY;
  stepsUntilSpawn = 8;
  lastSpaceStepMs = millis();
  clearObstacles();
  renderSpaceGame();
}

void spawnObstacle()
{
  for (uint8_t i = 0; i < MAX_OBSTACLES; i++)
  {
    if (obstacles[i].active) continue;
    obstacles[i].type = (uint8_t)random(2);
    obstacles[i].x = (obstacles[i].type == 0) ? MATRIX_WIDTH - 2 : MATRIX_WIDTH - 3;
    obstacles[i].y = (obstacles[i].type == 0)
      ? (uint8_t)random(MATRIX_HEIGHT - 1)
      : (uint8_t)random(MATRIX_HEIGHT - 2);
    obstacles[i].active = true;
    return;
  }
}

void moveShip(int8_t delta)
{
  if (mode != MODE_SPACE_GAME || spaceGameState == SPACE_GAME_OVER) return;

  int16_t requestedY = (int16_t)shipY + delta;
  if (requestedY < 1) requestedY = 1;
  if (requestedY > MATRIX_HEIGHT - 2) requestedY = MATRIX_HEIGHT - 2;
  shipY = (uint8_t)requestedY;

  for (uint8_t i = 0; i < MAX_OBSTACLES; i++)
  {
    if (obstacleHitsShip(obstacles[i]))
    {
      spaceGameState = SPACE_GAME_OVER;
      break;
    }
  }
  renderSpaceGame();
}

void updateSpaceGame()
{
  if (mode != MODE_SPACE_GAME || spaceGameState != SPACE_RUNNING) return;

  const uint32_t now = millis();
  if ((uint32_t)(now - lastSpaceStepMs) < spaceStepMs) return;
  lastSpaceStepMs = now;

  for (uint8_t i = 0; i < MAX_OBSTACLES; i++)
  {
    if (!obstacles[i].active) continue;
    obstacles[i].x--;

    const int16_t obstacleRight = obstacles[i].x + (obstacles[i].type == 0 ? 1 : 2);
    if (obstacleRight < 0)
    {
      obstacles[i].active = false;
      spaceScore++;
      spaceStepMs = max((uint16_t)35, (uint16_t)(spaceStepMs > 3 ? spaceStepMs - 3 : 35));
      continue;
    }

    if (obstacleHitsShip(obstacles[i]))
    {
      spaceGameState = SPACE_GAME_OVER;
      renderSpaceGame();
      return;
    }
  }

  if (stepsUntilSpawn > 0) stepsUntilSpawn--;
  if (stepsUntilSpawn == 0)
  {
    spawnObstacle();
    const uint16_t difficultyReduction = (spaceScore < 20) ? (uint16_t)(spaceScore / 4) : 5;
    stepsUntilSpawn = (uint16_t)random(9 - difficultyReduction, 16 - difficultyReduction);
  }

  renderSpaceGame();
}


static const uint8_t TREX_JUMP_HEIGHTS[TREX_JUMP_STEPS] = {1, 2, 4, 5, 4, 2, 1, 0};

const char* trexStateName()
{
  switch (trexGameState)
  {
    case TREX_RUNNING: return "running";
    case TREX_PAUSED: return "paused";
    case TREX_GAME_OVER: return "gameover";
    default: return "ready";
  }
}

uint8_t currentTrexJumpHeight()
{
  if (!trexJumping || trexJumpPhase >= TREX_JUMP_STEPS) return 0;
  return TREX_JUMP_HEIGHTS[trexJumpPhase];
}

int16_t currentTrexY()
{
  return (int16_t)TREX_GROUND_Y - 1 - currentTrexJumpHeight();
}

bool trexPixelAt(int16_t x, int16_t y)
{
  return x == TREX_X && y == currentTrexY();
}

bool trexObstaclePixelAt(const TrexObstacle& obstacle, int16_t x, int16_t y)
{
  if (!obstacle.active || x != obstacle.x) return false;
  const uint8_t height = constrain(obstacle.height, (uint8_t)1, (uint8_t)5);
  const int16_t top = (int16_t)TREX_GROUND_Y - height;
  return y >= top && y < TREX_GROUND_Y;
}

int16_t trexObstacleRight(const TrexObstacle& obstacle)
{
  return obstacle.x;
}

bool trexObstacleHitsDino(const TrexObstacle& obstacle)
{
  return obstacle.active &&
         obstacle.x == TREX_X &&
         trexObstaclePixelAt(obstacle, TREX_X, currentTrexY());
}

void clearTrexObstacles()
{
  for (uint8_t i = 0; i < MAX_TREX_OBSTACLES; i++) trexObstacles[i].active = false;
}

void renderTrexGame()
{
  matrix.clear();

  // Continuous ground line across the full 64-pixel display.
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++)
    matrix.setPixel(x, TREX_GROUND_Y, true);

  // The player is intentionally represented by one LED.
  matrix.setPixel(TREX_X, currentTrexY(), true);

  // Each enemy is a one-pixel-wide vertical column with variable height.
  for (uint8_t i = 0; i < MAX_TREX_OBSTACLES; i++)
  {
    if (!trexObstacles[i].active) continue;
    const uint8_t height = constrain(trexObstacles[i].height, (uint8_t)1, (uint8_t)5);
    for (uint8_t step = 1; step <= height; step++)
      matrix.setPixel(trexObstacles[i].x, TREX_GROUND_Y - step, true);
  }

  if (trexGameState == TREX_GAME_OVER)
  {
    matrix.setPixel(0, 0, true);
    matrix.setPixel(1, 1, true);
    matrix.setPixel(2, 0, true);
    matrix.setPixel(MATRIX_WIDTH - 3, 0, true);
    matrix.setPixel(MATRIX_WIDTH - 2, 1, true);
    matrix.setPixel(MATRIX_WIDTH - 1, 0, true);
  }
}

void resetTrexGame()
{
  mode = MODE_TREX_GAME;
  trexScore = 0;
  trexStepMs = trexStartStepMs;
  trexGameState = TREX_READY;
  trexStepsUntilSpawn = 12;
  lastTrexStepMs = millis();
  trexJumpPhase = 0;
  trexJumping = false;
  clearTrexObstacles();
  renderTrexGame();
}

void spawnTrexObstacle()
{
  for (uint8_t i = 0; i < MAX_TREX_OBSTACLES; i++)
  {
    if (trexObstacles[i].active) continue;
    trexObstacles[i].height = (uint8_t)random(1, 6);
    trexObstacles[i].x = MATRIX_WIDTH - 1;
    trexObstacles[i].active = true;
    return;
  }
}

void jumpTrex()
{
  if (mode != MODE_TREX_GAME || trexGameState != TREX_RUNNING || trexJumping) return;
  trexJumping = true;
  trexJumpPhase = 0;
  renderTrexGame();
}

void updateTrexGame()
{
  if (mode != MODE_TREX_GAME || trexGameState != TREX_RUNNING) return;

  const uint32_t now = millis();
  if ((uint32_t)(now - lastTrexStepMs) < trexStepMs) return;
  lastTrexStepMs = now;

  if (trexJumping)
  {
    trexJumpPhase++;
    if (trexJumpPhase >= TREX_JUMP_STEPS)
    {
      trexJumpPhase = 0;
      trexJumping = false;
    }
  }

  for (uint8_t i = 0; i < MAX_TREX_OBSTACLES; i++)
  {
    if (!trexObstacles[i].active) continue;
    trexObstacles[i].x--;

    if (trexObstacleRight(trexObstacles[i]) < 0)
    {
      trexObstacles[i].active = false;
      trexScore++;
      trexStepMs = max((uint16_t)35, (uint16_t)(trexStepMs > 3 ? trexStepMs - 3 : 35));
      continue;
    }

    if (trexObstacleHitsDino(trexObstacles[i]))
    {
      trexGameState = TREX_GAME_OVER;
      renderTrexGame();
      return;
    }
  }

  if (trexStepsUntilSpawn > 0) trexStepsUntilSpawn--;
  if (trexStepsUntilSpawn == 0)
  {
    spawnTrexObstacle();
    const uint16_t reduction = (trexScore < 24) ? (uint16_t)(trexScore / 6) : 4;
    trexStepsUntilSpawn = (uint16_t)random(11 - reduction, 19 - reduction);
  }

  renderTrexGame();
}



const char* gravityStateName()
{
  switch (gravityGameState)
  {
    case GRAVITY_RUNNING: return "running";
    case GRAVITY_PAUSED: return "paused";
    case GRAVITY_GAME_OVER: return "gameover";
    default: return "ready";
  }
}

const char* gravitySurfaceName()
{
  return gravityPullsTop ? "top" : "bottom";
}

uint8_t gravityObstacleDepthAt(const GravityObstacle& obstacle, uint8_t localX)
{
  if (obstacle.type == GRAVITY_WALL)
    return constrain(obstacle.height, (uint8_t)1, (uint8_t)5);

  if (obstacle.type == GRAVITY_PLATFORM)
    return constrain(obstacle.height, (uint8_t)2, (uint8_t)5);

  const uint8_t width = max((uint8_t)1, obstacle.width);
  const uint8_t height = constrain(obstacle.height, (uint8_t)2, (uint8_t)5);
  if (width <= 1) return height;

  // Two horizontal pixels per stair where possible. The complete step is
  // solid from its route surface, so every visible stair pixel is hazardous.
  const uint16_t scaled = (uint16_t)localX * (height - 1);
  return (uint8_t)(1 + scaled / (width - 1));
}

bool gravityObstaclePixelAt(const GravityObstacle& obstacle, int16_t x, int16_t y)
{
  if (!obstacle.active || x < obstacle.x ||
      x >= obstacle.x + obstacle.width || y <= 0 || y >= MATRIX_HEIGHT - 1)
    return false;

  const uint8_t localX = (uint8_t)(x - obstacle.x);
  const uint8_t depthFromSurface = obstacle.fromTop
    ? (uint8_t)y
    : (uint8_t)((MATRIX_HEIGHT - 1) - y);
  const uint8_t objectDepth = gravityObstacleDepthAt(obstacle, localX);

  if (obstacle.type == GRAVITY_PLATFORM)
    return depthFromSurface == objectDepth;

  return depthFromSurface >= 1 && depthFromSurface <= objectDepth;
}

int16_t gravityObstacleRight(const GravityObstacle& obstacle)
{
  return obstacle.x + obstacle.width - 1;
}

bool gravityPlayerHitsAnyObstacle()
{
  for (uint8_t i = 0; i < MAX_GRAVITY_OBSTACLES; i++)
  {
    if (gravityObstaclePixelAt(gravityObstacles[i],
                               GRAVITY_PLAYER_X,
                               gravityPlayerY)) return true;
  }
  return false;
}

void renderGravityGame();

bool endGravityGameIfColliding()
{
  if (!gravityPlayerHitsAnyObstacle()) return false;
  gravityGameState = GRAVITY_GAME_OVER;
  renderGravityGame();
  return true;
}

void clearGravityObstacles()
{
  for (uint8_t i = 0; i < MAX_GRAVITY_OBSTACLES; i++)
    gravityObstacles[i].active = false;
}

void renderGravityGame()
{
  matrix.clear();
  matrix.drawHorizontalLine(0, true);
  matrix.drawHorizontalLine(MATRIX_HEIGHT - 1, true);

  for (uint8_t i = 0; i < MAX_GRAVITY_OBSTACLES; i++)
  {
    if (!gravityObstacles[i].active) continue;
    for (int16_t x = gravityObstacles[i].x;
         x <= gravityObstacleRight(gravityObstacles[i]); x++)
    {
      for (uint8_t y = 1; y < MATRIX_HEIGHT - 1; y++)
      {
        if (gravityObstaclePixelAt(gravityObstacles[i], x, y))
          matrix.setPixel(x, y, true);
      }
    }
  }

  matrix.setPixel(GRAVITY_PLAYER_X, gravityPlayerY, true);

  if (gravityGameState == GRAVITY_GAME_OVER)
  {
    matrix.setPixel(0, 3, true);
    matrix.setPixel(1, 4, true);
    matrix.setPixel(2, 3, true);
    matrix.setPixel(MATRIX_WIDTH - 3, 3, true);
    matrix.setPixel(MATRIX_WIDTH - 2, 4, true);
    matrix.setPixel(MATRIX_WIDTH - 1, 3, true);
  }
}

void resetGravityGame()
{
  mode = MODE_GRAVITY_GAME;
  gravityScore = 0;
  gravityStepMs = gravityStartStepMs;
  gravityGameState = GRAVITY_READY;
  gravityStepsUntilSpawn = 10;
  lastGravityStepMs = millis();
  gravityPlayerY = GRAVITY_BOTTOM_Y;
  gravityPullsTop = false;
  clearGravityObstacles();
  renderGravityGame();
}

void spawnGravityObstacle()
{
  for (uint8_t i = 0; i < MAX_GRAVITY_OBSTACLES; i++)
  {
    if (gravityObstacles[i].active) continue;

    GravityObstacle& obstacle = gravityObstacles[i];
    obstacle.type = (uint8_t)random(3);
    obstacle.fromTop = random(2) != 0;

    if (obstacle.type == GRAVITY_WALL)
    {
      obstacle.height = (uint8_t)random(1, 6);
      obstacle.width = (uint8_t)random(1, 3);
    }
    else if (obstacle.type == GRAVITY_PLATFORM)
    {
      obstacle.height = (uint8_t)random(2, 6);
      obstacle.width = (uint8_t)random(5, 11);
    }
    else
    {
      obstacle.height = (uint8_t)random(2, 6);
      obstacle.width = (uint8_t)(obstacle.height * 2);
    }

    obstacle.x = MATRIX_WIDTH;
    obstacle.active = true;
    return;
  }
}

void flipGravityGuy()
{
  if (mode != MODE_GRAVITY_GAME || gravityGameState != GRAVITY_RUNNING) return;

  // A flip never makes the player immune to an object it is already touching.
  if (endGravityGameIfColliding()) return;
  gravityPullsTop = !gravityPullsTop;
  if (endGravityGameIfColliding()) return;
  renderGravityGame();
}

void updateGravityGame()
{
  if (mode != MODE_GRAVITY_GAME || gravityGameState != GRAVITY_RUNNING) return;

  const uint32_t now = millis();
  if ((uint32_t)(now - lastGravityStepMs) < gravityStepMs) return;
  lastGravityStepMs = now;

  if (endGravityGameIfColliding()) return;

  const uint8_t targetY = gravityPullsTop ? GRAVITY_TOP_Y : GRAVITY_BOTTOM_Y;
  if (gravityPlayerY < targetY) gravityPlayerY++;
  else if (gravityPlayerY > targetY) gravityPlayerY--;

  if (endGravityGameIfColliding()) return;

  for (uint8_t i = 0; i < MAX_GRAVITY_OBSTACLES; i++)
  {
    if (!gravityObstacles[i].active) continue;
    gravityObstacles[i].x--;

    // Collision is checked immediately after every object's movement. This
    // prevents a one-frame pass-through at the player's fixed X coordinate.
    if (gravityObstaclePixelAt(gravityObstacles[i],
                               GRAVITY_PLAYER_X,
                               gravityPlayerY))
    {
      gravityGameState = GRAVITY_GAME_OVER;
      renderGravityGame();
      return;
    }

    if (gravityObstacleRight(gravityObstacles[i]) < 0)
    {
      gravityObstacles[i].active = false;
      gravityScore++;
      gravityStepMs = max((uint16_t)30, (uint16_t)(gravityStepMs > 2 ? gravityStepMs - 2 : 30));
    }
  }

  if (gravityStepsUntilSpawn > 0) gravityStepsUntilSpawn--;
  if (gravityStepsUntilSpawn == 0)
  {
    spawnGravityObstacle();
    const uint16_t reduction = (gravityScore < 30)
      ? (uint16_t)(gravityScore / 8)
      : 3;
    gravityStepsUntilSpawn = (uint16_t)random(10 - reduction, 18 - reduction);
  }

  renderGravityGame();
}



const char* racingStateName()
{
  switch (racingGameState)
  {
    case RACING_RUNNING: return "running";
    case RACING_PAUSED: return "paused";
    case RACING_GAME_OVER: return "gameover";
    default: return "ready";
  }
}

void clearRacingObstacles()
{
  for (uint8_t i = 0; i < MAX_RACING_OBSTACLES; i++) racingObstacles[i].active = false;
}

bool racingObstacleHitsPlayer(const RacingObstacle& obstacle)
{
  if (!obstacle.active) return false;
  if (obstacle.x != RACING_PLAYER_X && obstacle.x != RACING_PLAYER_X + 1) return false;
  return racingPlayerY >= obstacle.y && racingPlayerY < obstacle.y + obstacle.height;
}

void renderRacingGame()
{
  matrix.clear();
  matrix.drawHorizontalLine(0, true);
  matrix.drawHorizontalLine(MATRIX_HEIGHT - 1, true);

  matrix.setPixel(RACING_PLAYER_X, racingPlayerY, true);
  matrix.setPixel(RACING_PLAYER_X + 1, racingPlayerY, true);

  for (uint8_t i = 0; i < MAX_RACING_OBSTACLES; i++)
  {
    if (!racingObstacles[i].active) continue;
    for (uint8_t dy = 0; dy < racingObstacles[i].height; dy++)
      matrix.setPixel(racingObstacles[i].x, racingObstacles[i].y + dy, true);
  }

  if (racingGameState == RACING_GAME_OVER)
  {
    matrix.setPixel(0, 2, true);
    matrix.setPixel(1, 3, true);
    matrix.setPixel(2, 2, true);
    matrix.setPixel(MATRIX_WIDTH - 3, 5, true);
    matrix.setPixel(MATRIX_WIDTH - 2, 4, true);
    matrix.setPixel(MATRIX_WIDTH - 1, 5, true);
  }
}

void resetRacingGame()
{
  mode = MODE_RACING_GAME;
  racingPlayerY = 4;
  racingScore = 0;
  racingStepMs = racingStartStepMs;
  racingStepsUntilSpawn = 9;
  racingGameState = RACING_READY;
  lastRacingStepMs = millis();
  clearRacingObstacles();
  renderRacingGame();
}

void spawnRacingObstacle()
{
  for (uint8_t i = 0; i < MAX_RACING_OBSTACLES; i++)
  {
    if (racingObstacles[i].active) continue;
    RacingObstacle& obstacle = racingObstacles[i];
    obstacle.height = (uint8_t)random(1, 3);
    obstacle.y = (uint8_t)random(1, (int)MATRIX_HEIGHT - obstacle.height);
    obstacle.x = MATRIX_WIDTH - 1;
    obstacle.active = true;
    return;
  }
}

void moveRacingPlayer(int8_t delta)
{
  if (mode != MODE_RACING_GAME || racingGameState == RACING_GAME_OVER) return;
  int16_t nextY = (int16_t)racingPlayerY + delta;
  if (nextY < 1) nextY = 1;
  if (nextY > MATRIX_HEIGHT - 2) nextY = MATRIX_HEIGHT - 2;
  racingPlayerY = (uint8_t)nextY;
  for (uint8_t i = 0; i < MAX_RACING_OBSTACLES; i++)
  {
    if (racingObstacleHitsPlayer(racingObstacles[i]))
    {
      racingGameState = RACING_GAME_OVER;
      break;
    }
  }
  renderRacingGame();
}

void updateRacingGame()
{
  if (mode != MODE_RACING_GAME || racingGameState != RACING_RUNNING) return;
  const uint32_t now = millis();
  if ((uint32_t)(now - lastRacingStepMs) < racingStepMs) return;
  lastRacingStepMs = now;

  for (uint8_t i = 0; i < MAX_RACING_OBSTACLES; i++)
  {
    if (!racingObstacles[i].active) continue;
    racingObstacles[i].x--;
    if (racingObstacleHitsPlayer(racingObstacles[i]))
    {
      racingGameState = RACING_GAME_OVER;
      renderRacingGame();
      return;
    }
    if (racingObstacles[i].x < 0)
    {
      racingObstacles[i].active = false;
      racingScore++;
      racingStepMs = max((uint16_t)35, (uint16_t)(racingStepMs > 3 ? racingStepMs - 3 : 35));
    }
  }

  if (racingStepsUntilSpawn > 0) racingStepsUntilSpawn--;
  if (racingStepsUntilSpawn == 0)
  {
    spawnRacingObstacle();
    const uint16_t reduction = racingScore < 24 ? (uint16_t)(racingScore / 6) : 4;
    racingStepsUntilSpawn = (uint16_t)random(8 - reduction, 15 - reduction);
  }
  renderRacingGame();
}

void renderScoreboard()
{
  char value[6];
  snprintf(value, sizeof(value), "%02u:%02u",
           (unsigned)scoreboardLeftScore, (unsigned)scoreboardRightScore);
  matrix.clear();
  const int16_t width = text.getTextWidth(value, FONT_6X8, 1);
  text.drawText((MATRIX_WIDTH - width) / 2, 0, value, FONT_6X8, 1);
}

static const uint16_t DOLPHIN_FRAMES[4][6] =
{
  {0x060, 0x1F8, 0x3FE, 0x7F3, 0x1F8, 0x060},
  {0x060, 0x1F8, 0x3FF, 0x7F0, 0x1F8, 0x064},
  {0x064, 0x1F8, 0x3FF, 0x7F0, 0x1F8, 0x060},
  {0x060, 0x1F8, 0x3FE, 0x7F3, 0x1F8, 0x020}
};

void drawDolphin(uint8_t frame, int16_t originX, int16_t originY, bool mirror)
{
  frame &= 0x03;
  for (uint8_t y = 0; y < 6; y++)
  {
    const uint16_t row = DOLPHIN_FRAMES[frame][y];
    for (uint8_t x = 0; x < 11; x++)
    {
      if ((row & ((uint16_t)1 << x)) == 0) continue;
      const int16_t drawX = mirror ? originX + (10 - x) : originX + x;
      matrix.setPixel(drawX, originY + y, true);
    }
  }
}

const char* animationStateName()
{
  return animationState == ANIMATION_PAUSED ? "paused" : "playing";
}

void renderAnimation()
{
  matrix.clear();
  drawDolphin(animationFrame, 7, 1, false);
  drawDolphin((uint8_t)(animationFrame + 2), 43, 1, true);
  matrix.setPixel(30 + (animationFrame & 0x01), 1, true);
  matrix.setPixel(34 - (animationFrame & 0x01), 3, true);
}

void resetAnimation()
{
  mode = MODE_ANIMATION;
  animationFrame = 0;
  animationState = ANIMATION_PLAYING;
  lastAnimationFrameMs = millis();
  renderAnimation();
}

void updateAnimation()
{
  if (mode != MODE_ANIMATION || animationState != ANIMATION_PLAYING) return;
  const uint32_t now = millis();
  if ((uint32_t)(now - lastAnimationFrameMs) < animationSpeedMs) return;
  lastAnimationFrameMs = now;
  animationFrame = (uint8_t)((animationFrame + 1) & 0x03);
  renderAnimation();
}

static const uint16_t TETRIS_SHAPES[7][4] =
{
  {0x00F0, 0x2222, 0x0F00, 0x4444}, // I
  {0x0066, 0x0066, 0x0066, 0x0066}, // O
  {0x0072, 0x0262, 0x0270, 0x0232}, // T
  {0x0036, 0x0462, 0x0360, 0x0231}, // S
  {0x0063, 0x0264, 0x0630, 0x0132}, // Z
  {0x0071, 0x0226, 0x0470, 0x0322}, // J
  {0x0074, 0x0622, 0x0170, 0x0223}  // L
};

const char* tetrisStateName()
{
  switch (tetrisGameState)
  {
    case TETRIS_RUNNING: return "running";
    case TETRIS_PAUSED: return "paused";
    case TETRIS_GAME_OVER: return "gameover";
    default: return "ready";
  }
}

bool tetrisShapeCell(uint8_t type, uint8_t rotation,
                     uint8_t localX, uint8_t localY)
{
  if (type >= 7 || localX >= 4 || localY >= 4) return false;
  const uint16_t mask = TETRIS_SHAPES[type][rotation & 0x03];
  return (mask & ((uint16_t)1 << (localY * 4 + localX))) != 0;
}

bool tetrisPieceFits(const TetrisPiece& piece)
{
  for (uint8_t localY = 0; localY < 4; localY++)
  {
    for (uint8_t localX = 0; localX < 4; localX++)
    {
      if (!tetrisShapeCell(piece.type, piece.rotation, localX, localY)) continue;

      const int16_t boardX = (int16_t)piece.x + localX;
      const int16_t boardY = (int16_t)piece.y + localY;
      if (boardX < 0 || boardX >= TETRIS_WIDTH ||
          boardY < 0 || boardY >= TETRIS_HEIGHT) return false;
      if ((tetrisBoard[boardX] & ((uint8_t)1 << boardY)) != 0) return false;
    }
  }
  return true;
}

void drawTetrisCell(uint8_t boardX, uint8_t boardY)
{
  if (boardX >= TETRIS_WIDTH || boardY >= TETRIS_HEIGHT) return;
  matrix.setPixel(boardX, boardY, true);
}

void renderTetrisGame()
{
  matrix.clear();

  for (uint8_t x = 0; x < TETRIS_WIDTH; x++)
  {
    for (uint8_t y = 0; y < TETRIS_HEIGHT; y++)
    {
      if ((tetrisBoard[x] & ((uint8_t)1 << y)) != 0)
        drawTetrisCell(x, y);
    }
  }

  if (tetrisGameState != TETRIS_GAME_OVER)
  {
    for (uint8_t localY = 0; localY < 4; localY++)
    {
      for (uint8_t localX = 0; localX < 4; localX++)
      {
        if (!tetrisShapeCell(tetrisPiece.type, tetrisPiece.rotation,
                             localX, localY)) continue;
        const int16_t boardX = (int16_t)tetrisPiece.x + localX;
        const int16_t boardY = (int16_t)tetrisPiece.y + localY;
        if (boardX >= 0 && boardX < TETRIS_WIDTH &&
            boardY >= 0 && boardY < TETRIS_HEIGHT)
          drawTetrisCell((uint8_t)boardX, (uint8_t)boardY);
      }
    }
  }
  else
  {
    for (uint8_t i = 0; i < 8; i++)
    {
      matrix.setPixel(i, i, true);
      matrix.setPixel(7 - i, i, true);
      matrix.setPixel(MATRIX_WIDTH - 8 + i, i, true);
      matrix.setPixel(MATRIX_WIDTH - 1 - i, i, true);
    }
  }
}

void clearTetrisBoard()
{
  memset(tetrisBoard, 0, sizeof(tetrisBoard));
}

bool spawnTetrisPiece()
{
  tetrisPiece.type = (uint8_t)random(7);
  tetrisPiece.rotation = 0;
  tetrisPiece.x = TETRIS_WIDTH - 4;
  tetrisPiece.y = 2;

  if (!tetrisPieceFits(tetrisPiece))
  {
    tetrisGameState = TETRIS_GAME_OVER;
    return false;
  }
  return true;
}

void resetTetrisGame()
{
  mode = MODE_TETRIS_GAME;
  clearTetrisBoard();
  tetrisScore = 0;
  tetrisLines = 0;
  tetrisGameState = TETRIS_READY;
  lastTetrisStepMs = millis();
  spawnTetrisPiece();
  renderTetrisGame();
}

uint8_t clearTetrisLines()
{
  uint8_t cleared = 0;
  uint8_t x = 0;

  while (x < TETRIS_WIDTH)
  {
    if (tetrisBoard[x] != 0xFF)
    {
      x++;
      continue;
    }

    for (uint8_t moveX = x; moveX < TETRIS_WIDTH - 1; moveX++)
      tetrisBoard[moveX] = tetrisBoard[moveX + 1];
    tetrisBoard[TETRIS_WIDTH - 1] = 0;
    cleared++;
  }

  return cleared;
}

void lockTetrisPiece()
{
  for (uint8_t localY = 0; localY < 4; localY++)
  {
    for (uint8_t localX = 0; localX < 4; localX++)
    {
      if (!tetrisShapeCell(tetrisPiece.type, tetrisPiece.rotation,
                           localX, localY)) continue;
      const int16_t boardX = (int16_t)tetrisPiece.x + localX;
      const int16_t boardY = (int16_t)tetrisPiece.y + localY;
      if (boardX >= 0 && boardX < TETRIS_WIDTH &&
          boardY >= 0 && boardY < TETRIS_HEIGHT)
        tetrisBoard[boardX] |= ((uint8_t)1 << boardY);
    }
  }

  const uint8_t cleared = clearTetrisLines();
  tetrisLines += cleared;
  if (cleared == 1) tetrisScore += 100;
  else if (cleared == 2) tetrisScore += 300;
  else if (cleared == 3) tetrisScore += 500;
  else if (cleared >= 4) tetrisScore += 800;

  if (!spawnTetrisPiece()) return;
}

void moveTetrisVertical(int8_t delta)
{
  if (mode != MODE_TETRIS_GAME || tetrisGameState != TETRIS_RUNNING) return;
  TetrisPiece candidate = tetrisPiece;
  candidate.y += delta;
  if (tetrisPieceFits(candidate))
  {
    tetrisPiece = candidate;
    renderTetrisGame();
  }
}

void rotateTetrisPiece()
{
  if (mode != MODE_TETRIS_GAME || tetrisGameState != TETRIS_RUNNING) return;

  TetrisPiece candidate = tetrisPiece;
  candidate.rotation = (candidate.rotation + 1) & 0x03;
  static const int8_t KICKS[5][2] =
  {
    {0, 0}, {0, -1}, {0, 1}, {1, 0}, {-1, 0}
  };

  for (uint8_t i = 0; i < 5; i++)
  {
    TetrisPiece kicked = candidate;
    kicked.x += KICKS[i][0];
    kicked.y += KICKS[i][1];
    if (tetrisPieceFits(kicked))
    {
      tetrisPiece = kicked;
      renderTetrisGame();
      return;
    }
  }
}

void hardDropTetrisPiece()
{
  if (mode != MODE_TETRIS_GAME || tetrisGameState != TETRIS_RUNNING) return;

  TetrisPiece candidate = tetrisPiece;
  while (true)
  {
    TetrisPiece next = candidate;
    next.x--;
    if (!tetrisPieceFits(next)) break;
    candidate = next;
  }

  tetrisPiece = candidate;
  lockTetrisPiece();
  renderTetrisGame();
}

void updateTetrisGame()
{
  if (mode != MODE_TETRIS_GAME || tetrisGameState != TETRIS_RUNNING) return;

  const uint32_t now = millis();
  if ((uint32_t)(now - lastTetrisStepMs) < tetrisStepMs) return;
  lastTetrisStepMs = now;

  TetrisPiece candidate = tetrisPiece;
  candidate.x--;

  if (tetrisPieceFits(candidate))
  {
    tetrisPiece = candidate;
  }
  else
  {
    lockTetrisPiece();
  }

  renderTetrisGame();
}

bool getPaintPixel(uint8_t x, uint8_t y)
{
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return false;
  return (paintBitmap[y][x / 8] & (uint8_t)(1U << (x % 8))) != 0;
}

void setPaintPixel(uint8_t x, uint8_t y, bool state)
{
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return;
  const uint8_t mask = (uint8_t)(1U << (x % 8));
  if (state) paintBitmap[y][x / 8] |= mask;
  else paintBitmap[y][x / 8] &= (uint8_t)~mask;
}

void renderPaint()
{
  matrix.clear();
  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++)
  {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++)
    {
      if (getPaintPixel(x, y)) matrix.setPixel(x, y, true);
    }
  }

  if (mode == MODE_PAINT && paintCursorVisible)
    matrix.setPixel(paintCursorX, paintCursorY, !getPaintPixel(paintCursorX, paintCursorY));
}

void showPaintMode()
{
  cancelDisplayStack(false);
  mode = MODE_PAINT;
  paintCursorVisible = true;
  lastPaintCursorBlinkMs = millis();
  renderPaint();
}

void schedulePaintSave()
{
  paintSavePending = true;
  paintSaveDueMs = millis() + PAINT_SAVE_DELAY_MS;
}

void flushPendingPaintSave()
{
  if (!paintSavePending) return;
  if ((int32_t)(millis() - paintSaveDueMs) < 0) return;
  saveDisplayState();
}

void updatePaintDisplay()
{
  if (mode != MODE_PAINT) return;
  const uint32_t now = millis();
  if ((uint32_t)(now - lastPaintCursorBlinkMs) < PAINT_CURSOR_BLINK_MS) return;
  lastPaintCursorBlinkMs = now;
  paintCursorVisible = !paintCursorVisible;
  renderPaint();
}

String paintBitmapHex()
{
  static const char HEX_DIGITS[] = "0123456789ABCDEF";
  String result;
  result.reserve(sizeof(paintBitmap) * 2);
  const uint8_t* bytes = &paintBitmap[0][0];
  for (size_t i = 0; i < sizeof(paintBitmap); i++)
  {
    result += HEX_DIGITS[(bytes[i] >> 4) & 0x0F];
    result += HEX_DIGITS[bytes[i] & 0x0F];
  }
  return result;
}

int8_t hexNibble(char value)
{
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

bool loadPaintBitmapHex(const String& value)
{
  if (value.length() != sizeof(paintBitmap) * 2) return false;
  uint8_t parsed[sizeof(paintBitmap)];
  for (size_t i = 0; i < sizeof(parsed); i++)
  {
    const int8_t high = hexNibble(value[i * 2]);
    const int8_t low = hexNibble(value[i * 2 + 1]);
    if (high < 0 || low < 0) return false;
    parsed[i] = (uint8_t)((high << 4) | low);
  }
  memcpy(paintBitmap, parsed, sizeof(paintBitmap));
  return true;
}

void configureNtpSync()
{
  if (WiFi.status() != WL_CONNECTED || config.ntpServer[0] == '\0') return;
  configTime(0, 0, config.ntpServer);
  ntpConfiguredForConnection = true;
  lastNtpSyncRequestMs = millis();
  timeWasSetManually = false;
}

void setSystemEpoch(time_t epoch)
{
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  timeWasSetManually = true;
  lastRenderedClockSecond = 0;
  lastRenderedCalendarKey = 0xFFFFFFFFUL;
}

void applyDisplaySettings()
{
  if (mode == MODE_TEST_PATTERN)
  {
    loadTestPattern();
  }
  else if (mode == MODE_STATIC_TEXT)
  {
    loadStaticText();
  }
  else if (mode == MODE_SCROLL_TEXT)
  {
    configureRunningText(displayText, displaySpeedMs);
  }
  else if (mode == MODE_SPACE_GAME)
  {
    renderSpaceGame();
  }
  else if (mode == MODE_TREX_GAME)
  {
    renderTrexGame();
  }
  else if (mode == MODE_GRAVITY_GAME)
  {
    renderGravityGame();
  }
  else if (mode == MODE_TETRIS_GAME)
  {
    renderTetrisGame();
  }
  else if (mode == MODE_RACING_GAME)
  {
    renderRacingGame();
  }
  else if (mode == MODE_SCOREBOARD)
  {
    renderScoreboard();
  }
  else if (mode == MODE_ANIMATION)
  {
    renderAnimation();
  }
  else if (mode == MODE_TIME)
  {
    renderClock();
  }
  else if (mode == MODE_CALENDAR)
  {
    renderCalendar();
  }
  else if (mode == MODE_TIMER)
  {
    renderTimer();
  }
  else if (mode == MODE_STOPWATCH)
  {
    renderStopwatch();
  }
  else
  {
    renderPaint();
  }
}

bool addressesShareSubnet(const IPAddress& first, const IPAddress& second,
                          const IPAddress& subnetMask)
{
  for (uint8_t i = 0; i < 4; i++)
  {
    if ((first[i] & subnetMask[i]) != (second[i] & subnetMask[i])) return false;
  }
  return true;
}

void restoreDisplayAfterStaIpAnnouncement()
{
  if (queuedMessageActive)
  {
    if (activeQueuedMessage.presentation == TEXT_PRESENTATION_STATIC)
      renderStaticText(activeQueuedMessage.value);
    else
      scroller.update();
    return;
  }

  if (mode == MODE_TEST_PATTERN) loadTestPattern();
  else if (mode == MODE_STATIC_TEXT) loadStaticText();
  else if (mode == MODE_SCROLL_TEXT) scroller.update();
  else if (mode == MODE_SPACE_GAME) renderSpaceGame();
  else if (mode == MODE_TREX_GAME) renderTrexGame();
  else if (mode == MODE_GRAVITY_GAME) renderGravityGame();
  else if (mode == MODE_TETRIS_GAME) renderTetrisGame();
  else if (mode == MODE_RACING_GAME) renderRacingGame();
  else if (mode == MODE_SCOREBOARD) renderScoreboard();
  else if (mode == MODE_ANIMATION) renderAnimation();
  else if (mode == MODE_TIME) renderClock();
  else if (mode == MODE_CALENDAR) renderCalendar();
  else if (mode == MODE_TIMER) renderTimer();
  else if (mode == MODE_STOPWATCH) renderStopwatch();
  else renderPaint();
}

void stopStaIpAnnouncement()
{
  if (!staIpAnnouncementActive) return;

  const uint32_t now = millis();
  if (queuedMessageActive &&
      activeQueuedMessage.presentation == TEXT_PRESENTATION_STATIC)
  {
    // Do not consume a static queued message's hold time while the IP overlay
    // is being displayed.
    uint32_t pauseStartedMs = staIpAnnouncementStartedMs;
    if ((int32_t)(queuedMessageStartedMs - pauseStartedMs) > 0)
      pauseStartedMs = queuedMessageStartedMs;
    queuedMessageStartedMs += (uint32_t)(now - pauseStartedMs);
  }

  staIpAnnouncementActive = false;
  restoreDisplayAfterStaIpAnnouncement();
}

void startStaIpAnnouncement()
{
  const bool staConnected = WiFi.status() == WL_CONNECTED;
  const String ipAddress = staConnected
    ? WiFi.localIP().toString()
    : WiFi.softAPIP().toString();
  const char* prefix = staConnected ? "STA IP" : "AP IP";

  char requestedText[sizeof(staIpAnnouncementText)];
  snprintf(requestedText, sizeof(requestedText), "%s %s", prefix, ipAddress.c_str());
  if (staIpAnnouncementActive && strcmp(requestedText, staIpAnnouncementText) == 0) return;

  copyString(staIpAnnouncementText, sizeof(staIpAnnouncementText), String(requestedText));
  staIpScroller.setFont(FONT_6X8);
  staIpScroller.setText(staIpAnnouncementText);
  staIpScroller.setSpacing(1);
  staIpScroller.setY(0);
  staIpScroller.setSpeed(55);
  staIpScroller.reset();

  staIpAnnouncementStartedMs = millis();
  staIpAnnouncementActive = true;
}

void noteWebClient()
{
  webClientSeen = true;
  lastWebClientSeenMs = millis();
  stopStaIpAnnouncement();
}

bool hasConnectedClient()
{
  if (WiFi.softAPgetStationNum() > 0) return true;
  if (!webClientSeen) return false;
  return (uint32_t)(millis() - lastWebClientSeenMs) <= WEB_CLIENT_ACTIVE_MS;
}

void maintainIpAnnouncement()
{
  if (hasConnectedClient())
  {
    stopStaIpAnnouncement();
    ipAnnouncementTimedOut = false;
    return;
  }

  const bool staConnected = WiFi.status() == WL_CONNECTED;
  const String ipAddress = staConnected
    ? WiFi.localIP().toString()
    : WiFi.softAPIP().toString();
  const char* prefix = staConnected ? "STA IP" : "AP IP";
  char requestedText[sizeof(staIpAnnouncementText)];
  snprintf(requestedText, sizeof(requestedText), "%s %s", prefix, ipAddress.c_str());

  // A changed address starts a fresh 20-second announcement window.
  if (strcmp(requestedText, staIpAnnouncementText) != 0)
  {
    ipAnnouncementTimedOut = false;
    startStaIpAnnouncement();
    return;
  }

  if (staIpAnnouncementActive)
  {
    if ((uint32_t)(millis() - staIpAnnouncementStartedMs) >= IP_ANNOUNCEMENT_TIMEOUT_MS)
    {
      stopStaIpAnnouncement();
      ipAnnouncementTimedOut = true;
    }
    return;
  }

  if (!ipAnnouncementTimedOut) startStaIpAnnouncement();
}

void storeEspNowPacket(const uint8_t* senderMac, const uint8_t* data, int length)
{
  if (length < (int)(sizeof(EspNowDisplayPacket) - sizeof(((EspNowDisplayPacket*)0)->text))) return;
  if (espNowPacketPending)
  {
    espNowDroppedPackets++;
    return;
  }

  memset(&espNowPendingPacket, 0, sizeof(espNowPendingPacket));
  const size_t copyLength = (length < (int)sizeof(espNowPendingPacket)) ? (size_t)length : sizeof(espNowPendingPacket);
  memcpy(&espNowPendingPacket, data, copyLength);
  espNowPendingPacket.text[sizeof(espNowPendingPacket.text) - 1] = '\0';
  memcpy(espNowPendingSender, senderMac, sizeof(espNowPendingSender));
  __sync_synchronize();
  espNowPacketPending = true;
}

#if defined(ESP8266)
void onEspNowReceive(uint8_t* senderMac, uint8_t* data, uint8_t length)
{
  storeEspNowPacket(senderMac, data, length);
}
#elif defined(ESP32) && defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowReceive(const esp_now_recv_info_t* receiveInfo, const uint8_t* data, int length)
{
  if (receiveInfo == nullptr) return;
  storeEspNowPacket(receiveInfo->src_addr, data, length);
}
#else
void onEspNowReceive(const uint8_t* senderMac, const uint8_t* data, int length)
{
  storeEspNowPacket(senderMac, data, length);
}
#endif

bool startEspNow()
{
#if defined(ESP8266)
  if (esp_now_init() != 0) return false;
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onEspNowReceive);
  return true;
#else
  if (esp_now_init() != ESP_OK) return false;
  return esp_now_register_recv_cb(onEspNowReceive) == ESP_OK;
#endif
}

void processPendingEspNowPacket()
{
  if (!espNowPacketPending) return;

  __sync_synchronize();
  EspNowDisplayPacket packet;
  uint8_t sender[6];
  memcpy(&packet, &espNowPendingPacket, sizeof(packet));
  memcpy(sender, espNowPendingSender, sizeof(sender));
  espNowPacketPending = false;

  if (packet.magic != ESPNOW_DISPLAY_MAGIC || packet.version != ESPNOW_DISPLAY_VERSION)
  {
    espNowDroppedPackets++;
    return;
  }

  snprintf(espNowLastSender, sizeof(espNowLastSender), "%02X:%02X:%02X:%02X:%02X:%02X",
           sender[0], sender[1], sender[2], sender[3], sender[4], sender[5]);
  espNowReceivedPackets++;

  uint16_t speed = packet.speedMs == 0 ? 60 : constrain(packet.speedMs, (uint16_t)15, (uint16_t)500);
  uint16_t duration = packet.durationMs == 0 ? 3000 : constrain(packet.durationMs, (uint16_t)250, (uint16_t)60000);
  uint8_t repeats = packet.repeatCount == 0 ? 1 : constrain(packet.repeatCount, (uint8_t)1, (uint8_t)10);
  const bool stack = (packet.flags & ESPNOW_FLAG_STACK) != 0;

  if (packet.command == ESPNOW_CLEAR_STACK)
  {
    cancelDisplayStack(true);
    return;
  }

  if (packet.command == ESPNOW_SHOW_TIME)
  {
    cancelDisplayStack(false);
    mode = MODE_TIME;
    renderClock();
    saveDisplayState();
    return;
  }

  if (packet.command == ESPNOW_SHOW_STATIC)
  {
    if (stack) stackTextStatic(packet.text, duration);
    else showTextStatic(packet.text);
  }
  else if (packet.command == ESPNOW_SHOW_RUNNING)
  {
    if (stack) stackTextRunning(packet.text, speed, repeats);
    else showTextRunning(packet.text, speed);
  }
  else
  {
    espNowDroppedPackets++;
  }
}

void handleStatus()
{
  const bool connected = WiFi.status() == WL_CONNECTED;
  updateClockText();
  updateCalendarText();

  String activeDisplayText = String(displayText);
  if (staIpAnnouncementActive) activeDisplayText = String(staIpAnnouncementText);
  else if (queuedMessageActive) activeDisplayText = String(activeQueuedMessage.value);
  else if (mode == MODE_SPACE_GAME) activeDisplayText = F("SPACE RUNNER");
  else if (mode == MODE_TREX_GAME) activeDisplayText = F("T-REX RUNNER");
  else if (mode == MODE_GRAVITY_GAME) activeDisplayText = F("GRAVITY GUY");
  else if (mode == MODE_TETRIS_GAME) activeDisplayText = F("TETRIS");
  else if (mode == MODE_RACING_GAME) activeDisplayText = F("RACING");
  else if (mode == MODE_SCOREBOARD) activeDisplayText = F("SCOREBOARD");
  else if (mode == MODE_ANIMATION) activeDisplayText = F("DOLPHINS");
  else if (mode == MODE_PAINT) activeDisplayText = F("PAINT");
  else if (mode == MODE_TIME) activeDisplayText = String(clockText);
  else if (mode == MODE_CALENDAR) activeDisplayText = String(calendarText);
  else if (mode == MODE_TIMER)
  {
    formatDuration(timerRemainingSeconds(), timerText, sizeof(timerText));
    if (timerPresentation == TIMER_PRESENTATION_BAR)
    {
      activeDisplayText = F("TIMER BAR ");
      activeDisplayText += String(timerProgressPercent());
      activeDisplayText += '%';
    }
    else activeDisplayText = String(timerText);
  }
  else if (mode == MODE_STOPWATCH)
  {
    formatDuration(stopwatchElapsedSeconds(), stopwatchText, sizeof(stopwatchText));
    activeDisplayText = String(stopwatchText);
  }

  String body = F("{");
  body += F("\"staConnected\":");
  body += connected ? F("true") : F("false");
  body += F(",\"configuredStaSsid\":\"");
  body += jsonEscape(String(config.staSsid));
  body += F("\",\"staSsid\":\"");
  body += jsonEscape(connected ? WiFi.SSID() : String(config.staSsid));
  body += F("\",\"staIp\":\"");
  body += connected ? WiFi.localIP().toString() : String();
  body += F("\",\"staIpAnnouncement\":");
  body += staIpAnnouncementActive ? F("true") : F("false");
  body += F(",\"ipAnnouncementTimeoutMs\":");
  body += String(IP_ANNOUNCEMENT_TIMEOUT_MS);
  body += F(",\"rssi\":");
  body += connected ? String(WiFi.RSSI()) : String(0);
  body += F(",\"apSsid\":\"");
  body += jsonEscape(String(config.apSsid));
  body += F("\",\"apIp\":\"");
  body += WiFi.softAPIP().toString();
  body += F("\",\"apOpen\":");
  body += (config.apPassword[0] == '\0') ? F("true") : F("false");
  body += F(",\"apClients\":");
  body += String(WiFi.softAPgetStationNum());
  body += F(",\"text\":\"");
  body += jsonEscape(String(displayText));
  body += F("\",\"activeText\":\"");
  body += jsonEscape(activeDisplayText);
  body += F("\",\"queueDepth\":");
  body += String(getDisplayStackDepth());
  body += F(",\"speed\":");
  body += String(displaySpeedMs);
  body += F(",\"mode\":\"");
  if (mode == MODE_STATIC_TEXT) body += F("static");
  else if (mode == MODE_SPACE_GAME) body += F("space");
  else if (mode == MODE_TREX_GAME) body += F("trex");
  else if (mode == MODE_GRAVITY_GAME) body += F("gravity");
  else if (mode == MODE_TETRIS_GAME) body += F("tetris");
  else if (mode == MODE_RACING_GAME) body += F("racing");
  else if (mode == MODE_SCOREBOARD) body += F("scoreboard");
  else if (mode == MODE_ANIMATION) body += F("animation");
  else if (mode == MODE_TIME) body += F("time");
  else if (mode == MODE_CALENDAR) body += F("calendar");
  else if (mode == MODE_TIMER) body += F("timer");
  else if (mode == MODE_STOPWATCH) body += F("stopwatch");
  else if (mode == MODE_PAINT) body += F("paint");
  else body += F("scroll");
  body += F("\",\"activeGame\":\"");
  if (mode == MODE_TREX_GAME) body += F("trex");
  else if (mode == MODE_GRAVITY_GAME) body += F("gravity");
  else if (mode == MODE_TETRIS_GAME) body += F("tetris");
  else if (mode == MODE_RACING_GAME) body += F("racing");
  else body += F("space");
  body += F("\",\"gameState\":\"");
  if (mode == MODE_TREX_GAME) body += trexStateName();
  else if (mode == MODE_GRAVITY_GAME) body += gravityStateName();
  else if (mode == MODE_TETRIS_GAME) body += tetrisStateName();
  else if (mode == MODE_RACING_GAME) body += racingStateName();
  else body += spaceStateName();
  body += F("\",\"gameScore\":");
  if (mode == MODE_TREX_GAME) body += String(trexScore);
  else if (mode == MODE_GRAVITY_GAME) body += String(gravityScore);
  else if (mode == MODE_TETRIS_GAME) body += String(tetrisScore);
  else if (mode == MODE_RACING_GAME) body += String(racingScore);
  else body += String(spaceScore);
  body += F(",\"gameSpeed\":");
  if (mode == MODE_TREX_GAME) body += String(trexStepMs);
  else if (mode == MODE_GRAVITY_GAME) body += String(gravityStepMs);
  else if (mode == MODE_TETRIS_GAME) body += String(tetrisStepMs);
  else if (mode == MODE_RACING_GAME) body += String(racingStepMs);
  else body += String(spaceStepMs);
  body += F(",\"spaceState\":\"");
  body += spaceStateName();
  body += F("\",\"spaceScore\":");
  body += String(spaceScore);
  body += F(",\"spaceSpeed\":");
  body += String(spaceStartStepMs);
  body += F(",\"spaceCurrentSpeed\":");
  body += String(spaceStepMs);
  body += F(",\"shipY\":");
  body += String(shipY);
  body += F(",\"trexState\":\"");
  body += trexStateName();
  body += F("\",\"trexScore\":");
  body += String(trexScore);
  body += F(",\"trexSpeed\":");
  body += String(trexStartStepMs);
  body += F(",\"trexCurrentSpeed\":");
  body += String(trexStepMs);
  body += F(",\"trexJumping\":");
  body += trexJumping ? F("true") : F("false");
  body += F(",\"gravityState\":\"");
  body += gravityStateName();
  body += F("\",\"gravityScore\":");
  body += String(gravityScore);
  body += F(",\"gravitySpeed\":");
  body += String(gravityStartStepMs);
  body += F(",\"gravityCurrentSpeed\":");
  body += String(gravityStepMs);
  body += F(",\"gravitySurface\":\"");
  body += gravitySurfaceName();
  body += F("\",\"gravityPlayerY\":");
  body += String(gravityPlayerY);
  body += F(",\"tetrisState\":\"");
  body += tetrisStateName();
  body += F("\",\"tetrisScore\":");
  body += String(tetrisScore);
  body += F(",\"tetrisLines\":");
  body += String(tetrisLines);
  body += F(",\"tetrisSpeed\":");
  body += String(tetrisStepMs);
  body += F(",\"racingState\":\"");
  body += racingStateName();
  body += F("\",\"racingScore\":");
  body += String(racingScore);
  body += F(",\"racingSpeed\":");
  body += String(racingStartStepMs);
  body += F(",\"racingCurrentSpeed\":");
  body += String(racingStepMs);
  body += F(",\"racingPlayerY\":");
  body += String(racingPlayerY);
  body += F(",\"scoreboardLeft\":");
  body += String(scoreboardLeftScore);
  body += F(",\"scoreboardRight\":");
  body += String(scoreboardRightScore);
  body += F(",\"animationState\":\"");
  body += animationStateName();
  body += F("\",\"animationSpeed\":");
  body += String(animationSpeedMs);
  body += F(",\"animationFrame\":");
  body += String(animationFrame);
  body += F(",\"paintData\":\"");
  body += paintBitmapHex();
  body += F("\",\"paintCursorX\":");
  body += String(paintCursorX);
  body += F(",\"paintCursorY\":");
  body += String(paintCursorY);
  body += F(",\"clockTime\":\"");
  body += clockText;
  body += F("\",\"timeValid\":");
  body += isClockValid() ? F("true") : F("false");
  body += F(",\"timeSource\":\"");
  if (!isClockValid()) body += ntpConfiguredForConnection ? F("syncing") : F("unset");
  else body += timeWasSetManually ? F("manual") : F("ntp");
  body += F("\",\"timezoneOffset\":");
  body += String(config.timezoneOffsetMinutes);
  body += F(",\"ntpServer\":\"");
  body += jsonEscape(String(config.ntpServer));
  body += F("\",\"calendarDate\":\"");
  body += calendarText;
  body += F("\",\"rotation\":");
  body += config.rotation180 ? F("180") : F("0");
  body += F(",\"ntpResyncHours\":12");
  body += F(",\"timerText\":\"");
  formatDuration(timerRemainingSeconds(), timerText, sizeof(timerText));
  body += timerText;
  body += F("\",\"timerState\":\"");
  body += chronoStateName(timerState);
  body += F("\",\"timerDuration\":");
  body += String(timerDurationSeconds);
  body += F(",\"timerRemaining\":");
  body += String(timerRemainingSeconds());
  body += F(",\"timerPresentation\":\"");
  body += timerPresentation == TIMER_PRESENTATION_BAR ? F("bar") : F("time");
  body += F("\",\"timerProgress\":");
  body += String(timerProgressPercent());
  body += F(",\"timerBlinking\":");
  body += timerState == CHRONO_FINISHED ? F("true") : F("false");
  body += F(",\"stopwatchText\":\"");
  formatDuration(stopwatchElapsedSeconds(), stopwatchText, sizeof(stopwatchText));
  body += stopwatchText;
  body += F("\",\"stopwatchState\":\"");
  body += chronoStateName(stopwatchState);
  body += F("\",\"stopwatchElapsed\":");
  body += String(stopwatchElapsedSeconds());
  body += F(",\"espNowReady\":");
  body += espNowReady ? F("true") : F("false");
  body += F(",\"espNowMac\":\"");
  body += WiFi.macAddress();
  body += F("\",\"wifiChannel\":");
  body += String(WiFi.channel());
  body += F(",\"espNowReceived\":");
  body += String(espNowReceivedPackets);
  body += F(",\"espNowDropped\":");
  body += String((uint32_t)espNowDroppedPackets);
  body += F(",\"espNowLastSender\":\"");
  body += espNowLastSender;
  body += F("\"}");

  sendJson(200, body);
}

void handleTextUpdate()
{
  if (!webServer.hasArg("text"))
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Missing text value.\"}"));
    return;
  }

  char requestedText[128];
  sanitizeText(webServer.arg("text"), requestedText, sizeof(requestedText));
  const TextPresentation presentation = (webServer.arg("mode") == F("static"))
    ? TEXT_PRESENTATION_STATIC
    : TEXT_PRESENTATION_RUNNING;

  long requestedSpeed = webServer.arg("speed").toInt();
  if (requestedSpeed < 15) requestedSpeed = 15;
  if (requestedSpeed > 500) requestedSpeed = 500;

  long requestedHold = webServer.arg("hold").toInt();
  if (requestedHold < 250) requestedHold = 3000;
  if (requestedHold > 60000) requestedHold = 60000;

  long requestedRepeats = webServer.arg("repeats").toInt();
  if (requestedRepeats < 1) requestedRepeats = 1;
  if (requestedRepeats > 10) requestedRepeats = 10;

  const bool stack = webServer.arg("queue") == F("1");
  if (stack)
  {
    const bool accepted = stackTextMessage(requestedText, presentation,
      (uint16_t)requestedSpeed, (uint16_t)requestedHold, (uint8_t)requestedRepeats);
    if (!accepted)
    {
      sendJson(409, F("{\"ok\":false,\"error\":\"The display stack is full.\"}"));
      return;
    }
  }
  else
  {
    showText(requestedText, presentation, (uint16_t)requestedSpeed);
  }

  String body = F("{\"ok\":true,\"queued\":");
  body += stack ? F("true") : F("false");
  body += F(",\"queueDepth\":");
  body += String(getDisplayStackDepth());
  body += F(",\"text\":\"");
  body += jsonEscape(String(requestedText));
  body += F("\"}");
  sendJson(200, body);
}

void handleQueueControl()
{
  if (webServer.arg("action") != F("clear"))
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Unknown stack action.\"}"));
    return;
  }

  cancelDisplayStack(true);
  sendJson(200, F("{\"ok\":true,\"queueDepth\":0}"));
}

void handleGameControl()
{
  const String action = webServer.arg("action");
  const String requestedGame = webServer.arg("game");

  const bool useGravity = requestedGame == F("gravity") ||
    (requestedGame.length() == 0 && mode == MODE_GRAVITY_GAME);
  const bool useTetris = requestedGame == F("tetris") ||
    (requestedGame.length() == 0 && mode == MODE_TETRIS_GAME);
  const bool useRacing = requestedGame == F("racing") ||
    (requestedGame.length() == 0 && mode == MODE_RACING_GAME);
  const bool useTrex = !useGravity && !useTetris && !useRacing &&
    (requestedGame == F("trex") ||
     (requestedGame.length() == 0 && mode == MODE_TREX_GAME));

  if (action == F("start") || action == F("reset"))
    cancelDisplayStack(false);

  if (webServer.hasArg("speed"))
  {
    long requestedSpeed = webServer.arg("speed").toInt();
    long minimumSpeed = 60;
    long maximumSpeed = 500;

    if (useGravity)
    {
      minimumSpeed = 45;
      maximumSpeed = 350;
    }
    else if (useTetris)
    {
      minimumSpeed = 120;
      maximumSpeed = 1200;
    }
    else if (useRacing)
    {
      minimumSpeed = 60;
      maximumSpeed = 500;
    }

    requestedSpeed = constrain(requestedSpeed, minimumSpeed, maximumSpeed);

    if (useTrex)
    {
      trexStartStepMs = (uint16_t)requestedSpeed;
      trexStepMs = trexStartStepMs;
    }
    else if (useGravity)
    {
      gravityStartStepMs = (uint16_t)requestedSpeed;
      gravityStepMs = gravityStartStepMs;
    }
    else if (useTetris) tetrisStepMs = (uint16_t)requestedSpeed;
    else if (useRacing)
    {
      racingStartStepMs = (uint16_t)requestedSpeed;
      racingStepMs = racingStartStepMs;
    }
    else
    {
      spaceStartStepMs = (uint16_t)requestedSpeed;
      spaceStepMs = spaceStartStepMs;
    }
  }

  if (useGravity)
  {
    if (action == F("reset"))
    {
      resetGravityGame();
      saveDisplayState();
    }
    else if (action == F("start"))
    {
      if (mode != MODE_GRAVITY_GAME || gravityGameState == GRAVITY_GAME_OVER)
        resetGravityGame();

      mode = MODE_GRAVITY_GAME;
      gravityGameState = GRAVITY_RUNNING;
      lastGravityStepMs = millis();
      renderGravityGame();
      saveDisplayState();
    }
    else if (action == F("pause"))
    {
      if (mode != MODE_GRAVITY_GAME)
      {
        sendJson(409, F("{\"ok\":false,\"error\":\"Start Gravity Guy first.\"}"));
        return;
      }

      if (gravityGameState == GRAVITY_RUNNING)
        gravityGameState = GRAVITY_PAUSED;
      else if (gravityGameState == GRAVITY_PAUSED ||
               gravityGameState == GRAVITY_READY)
      {
        gravityGameState = GRAVITY_RUNNING;
        lastGravityStepMs = millis();
      }
      renderGravityGame();
    }
    else if (action == F("flip"))
    {
      if (mode != MODE_GRAVITY_GAME ||
          gravityGameState != GRAVITY_RUNNING)
      {
        sendJson(409, F("{\"ok\":false,\"error\":\"Start Gravity Guy first.\"}"));
        return;
      }
      flipGravityGuy();
    }
    else if (action != F("speed"))
    {
      sendJson(400, F("{\"ok\":false,\"error\":\"Unknown Gravity Guy action.\"}"));
      return;
    }

    String body = F("{\"ok\":true,\"game\":\"gravity\",\"state\":\"");
    body += gravityStateName();
    body += F("\",\"score\":");
    body += String(gravityScore);
    body += F(",\"speed\":");
    body += String(gravityStepMs);
    body += F(",\"surface\":\"");
    body += gravitySurfaceName();
    body += F("\",\"playerY\":");
    body += String(gravityPlayerY);
    body += F("}");
    sendJson(200, body);
    return;
  }

  if (useTetris)
  {
    if (action == F("reset"))
    {
      resetTetrisGame();
      saveDisplayState();
    }
    else if (action == F("start"))
    {
      if (mode != MODE_TETRIS_GAME || tetrisGameState == TETRIS_GAME_OVER)
        resetTetrisGame();

      mode = MODE_TETRIS_GAME;
      tetrisGameState = TETRIS_RUNNING;
      lastTetrisStepMs = millis();
      renderTetrisGame();
      saveDisplayState();
    }
    else if (action == F("pause"))
    {
      if (mode != MODE_TETRIS_GAME)
      {
        sendJson(409, F("{\"ok\":false,\"error\":\"Start Tetris first.\"}"));
        return;
      }

      if (tetrisGameState == TETRIS_RUNNING)
        tetrisGameState = TETRIS_PAUSED;
      else if (tetrisGameState == TETRIS_PAUSED ||
               tetrisGameState == TETRIS_READY)
      {
        tetrisGameState = TETRIS_RUNNING;
        lastTetrisStepMs = millis();
      }
      renderTetrisGame();
    }
    else if (action == F("up"))
    {
      moveTetrisVertical(-1);
    }
    else if (action == F("down"))
    {
      moveTetrisVertical(1);
    }
    else if (action == F("rotate"))
    {
      rotateTetrisPiece();
    }
    else if (action == F("drop"))
    {
      hardDropTetrisPiece();
    }
    else if (action != F("speed"))
    {
      sendJson(400, F("{\"ok\":false,\"error\":\"Unknown Tetris action.\"}"));
      return;
    }

    String body = F("{\"ok\":true,\"game\":\"tetris\",\"state\":\"");
    body += tetrisStateName();
    body += F("\",\"score\":");
    body += String(tetrisScore);
    body += F(",\"lines\":");
    body += String(tetrisLines);
    body += F(",\"speed\":");
    body += String(tetrisStepMs);
    body += F("}");
    sendJson(200, body);
    return;
  }

  if (useRacing)
  {
    if (action == F("reset"))
    {
      resetRacingGame();
      saveDisplayState();
    }
    else if (action == F("start"))
    {
      if (mode != MODE_RACING_GAME || racingGameState == RACING_GAME_OVER)
        resetRacingGame();
      mode = MODE_RACING_GAME;
      racingGameState = RACING_RUNNING;
      lastRacingStepMs = millis();
      renderRacingGame();
      saveDisplayState();
    }
    else if (action == F("pause"))
    {
      if (mode != MODE_RACING_GAME)
      {
        sendJson(409, F("{\"ok\":false,\"error\":\"Start Racing first.\"}"));
        return;
      }
      if (racingGameState == RACING_RUNNING) racingGameState = RACING_PAUSED;
      else if (racingGameState == RACING_PAUSED || racingGameState == RACING_READY)
      {
        racingGameState = RACING_RUNNING;
        lastRacingStepMs = millis();
      }
      renderRacingGame();
    }
    else if (action == F("up")) moveRacingPlayer(-1);
    else if (action == F("down")) moveRacingPlayer(1);
    else if (action != F("speed"))
    {
      sendJson(400, F("{\"ok\":false,\"error\":\"Unknown Racing action.\"}"));
      return;
    }

    String body = F("{\"ok\":true,\"game\":\"racing\",\"state\":\"");
    body += racingStateName();
    body += F("\",\"score\":");
    body += String(racingScore);
    body += F(",\"speed\":");
    body += String(racingStepMs);
    body += F(",\"playerY\":");
    body += String(racingPlayerY);
    body += F("}");
    sendJson(200, body);
    return;
  }

  if (useTrex)
  {
    if (action == F("reset"))
    {
      resetTrexGame();
      saveDisplayState();
    }
    else if (action == F("start"))
    {
      if (mode != MODE_TREX_GAME || trexGameState == TREX_GAME_OVER)
        resetTrexGame();

      mode = MODE_TREX_GAME;
      trexGameState = TREX_RUNNING;
      lastTrexStepMs = millis();
      renderTrexGame();
      saveDisplayState();
    }
    else if (action == F("pause"))
    {
      if (mode != MODE_TREX_GAME)
      {
        sendJson(409, F("{\"ok\":false,\"error\":\"Start T-Rex Runner first.\"}"));
        return;
      }

      if (trexGameState == TREX_RUNNING)
        trexGameState = TREX_PAUSED;
      else if (trexGameState == TREX_PAUSED || trexGameState == TREX_READY)
      {
        trexGameState = TREX_RUNNING;
        lastTrexStepMs = millis();
      }
      renderTrexGame();
    }
    else if (action == F("jump"))
    {
      if (mode != MODE_TREX_GAME || trexGameState != TREX_RUNNING)
      {
        sendJson(409, F("{\"ok\":false,\"error\":\"Start T-Rex Runner first.\"}"));
        return;
      }
      jumpTrex();
    }
    else if (action != F("speed"))
    {
      sendJson(400, F("{\"ok\":false,\"error\":\"Unknown T-Rex action.\"}"));
      return;
    }

    String body = F("{\"ok\":true,\"game\":\"trex\",\"state\":\"");
    body += trexStateName();
    body += F("\",\"score\":");
    body += String(trexScore);
    body += F(",\"speed\":");
    body += String(trexStepMs);
    body += F(",\"jumping\":");
    body += trexJumping ? F("true") : F("false");
    body += F("}");
    sendJson(200, body);
    return;
  }

  if (action == F("reset"))
  {
    resetSpaceGame();
    saveDisplayState();
  }
  else if (action == F("start"))
  {
    if (mode != MODE_SPACE_GAME || spaceGameState == SPACE_GAME_OVER)
      resetSpaceGame();

    mode = MODE_SPACE_GAME;
    spaceGameState = SPACE_RUNNING;
    lastSpaceStepMs = millis();
    renderSpaceGame();
    saveDisplayState();
  }
  else if (action == F("pause"))
  {
    if (mode != MODE_SPACE_GAME)
    {
      sendJson(409, F("{\"ok\":false,\"error\":\"Start Space Runner first.\"}"));
      return;
    }

    if (spaceGameState == SPACE_RUNNING)
      spaceGameState = SPACE_PAUSED;
    else if (spaceGameState == SPACE_PAUSED || spaceGameState == SPACE_READY)
    {
      spaceGameState = SPACE_RUNNING;
      lastSpaceStepMs = millis();
    }
    renderSpaceGame();
  }
  else if (action == F("up"))
  {
    moveShip(-1);
  }
  else if (action == F("down"))
  {
    moveShip(1);
  }
  else if (action != F("speed"))
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Unknown Space Runner action.\"}"));
    return;
  }

  String body = F("{\"ok\":true,\"game\":\"space\",\"state\":\"");
  body += spaceStateName();
  body += F("\",\"score\":");
  body += String(spaceScore);
  body += F(",\"speed\":");
  body += String(spaceStepMs);
  body += F("}");
  sendJson(200, body);
}

void handleScoreboardControl()
{
  const String action = webServer.arg("action");
  cancelDisplayStack(false);
  mode = MODE_SCOREBOARD;

  if (action == F("left_inc")) scoreboardLeftScore = min((uint8_t)99, (uint8_t)(scoreboardLeftScore + 1));
  else if (action == F("left_dec")) { if (scoreboardLeftScore > 0) scoreboardLeftScore--; }
  else if (action == F("right_inc")) scoreboardRightScore = min((uint8_t)99, (uint8_t)(scoreboardRightScore + 1));
  else if (action == F("right_dec")) { if (scoreboardRightScore > 0) scoreboardRightScore--; }
  else if (action == F("reset")) { scoreboardLeftScore = 0; scoreboardRightScore = 0; }
  else if (action == F("swap")) { const uint8_t value = scoreboardLeftScore; scoreboardLeftScore = scoreboardRightScore; scoreboardRightScore = value; }
  else if (action != F("show"))
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Unknown scoreboard action.\"}"));
    return;
  }

  renderScoreboard();
  saveDisplayState();
  String body = F("{\"ok\":true,\"left\":");
  body += String(scoreboardLeftScore);
  body += F(",\"right\":");
  body += String(scoreboardRightScore);
  body += F("}");
  sendJson(200, body);
}

void handleAnimationControl()
{
  const String action = webServer.arg("action");
  if (webServer.hasArg("speed"))
  {
    long value = webServer.arg("speed").toInt();
    animationSpeedMs = (uint16_t)constrain(value, 80L, 1000L);
  }

  if (action == F("show") || action == F("play"))
  {
    cancelDisplayStack(false);
    mode = MODE_ANIMATION;
    animationState = ANIMATION_PLAYING;
    lastAnimationFrameMs = millis();
    renderAnimation();
    saveDisplayState();
  }
  else if (action == F("pause"))
  {
    cancelDisplayStack(false);
    mode = MODE_ANIMATION;
    animationState = animationState == ANIMATION_PLAYING ? ANIMATION_PAUSED : ANIMATION_PLAYING;
    lastAnimationFrameMs = millis();
    renderAnimation();
    saveDisplayState();
  }
  else if (action == F("reset"))
  {
    cancelDisplayStack(false);
    resetAnimation();
    saveDisplayState();
  }
  else if (action == F("speed"))
  {
    if (mode == MODE_ANIMATION) renderAnimation();
  }
  else
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Unknown animation action.\"}"));
    return;
  }

  String body = F("{\"ok\":true,\"state\":\"");
  body += animationStateName();
  body += F("\",\"speed\":");
  body += String(animationSpeedMs);
  body += F("}");
  sendJson(200, body);
}

bool updateTimeSettingsFromRequest(String& error)
{
  if (webServer.hasArg("timezoneOffset"))
  {
    const long offset = webServer.arg("timezoneOffset").toInt();
    if (offset < -720 || offset > 840)
    {
      error = F("UTC offset must be between -720 and 840 minutes.");
      return false;
    }
    config.timezoneOffsetMinutes = (int16_t)offset;
  }

  if (webServer.hasArg("ntpServer"))
  {
    const String server = webServer.arg("ntpServer");
    if (server.length() == 0 || server.length() > 63)
    {
      error = F("NTP server must contain 1-63 characters.");
      return false;
    }
    copyString(config.ntpServer, sizeof(config.ntpServer), server);
  }
  return true;
}

void handleTimeControl()
{
  const String action = webServer.arg("action");
  if (webServer.hasArg("presentation"))
  {
    const String presentation = webServer.arg("presentation");
    if (presentation == F("bar")) timerPresentation = TIMER_PRESENTATION_BAR;
    else if (presentation == F("time")) timerPresentation = TIMER_PRESENTATION_TIME;
    else
    {
      sendJson(400, F("{\"ok\":false,\"error\":\"Timer presentation must be time or bar.\"}"));
      return;
    }
  }
  String error;
  if (!updateTimeSettingsFromRequest(error))
  {
    String body = F("{\"ok\":false,\"error\":\"");
    body += jsonEscape(error);
    body += F("\"}");
    sendJson(400, body);
    return;
  }

  if (action == F("save"))
  {
    saveConfig();
  }
  else if (action == F("show"))
  {
    saveConfig();
    cancelDisplayStack(false);
    mode = MODE_TIME;
    renderClock();
  }
  else if (action == F("calendar_show"))
  {
    saveConfig();
    cancelDisplayStack(false);
    mode = MODE_CALENDAR;
    renderCalendar();
  }
  else if (action == F("set"))
  {
    const time_t epoch = (time_t)webServer.arg("epoch").toInt();
    if (epoch < 1700000000)
    {
      sendJson(400, F("{\"ok\":false,\"error\":\"Invalid date/time value.\"}"));
      return;
    }
    saveConfig();
    setSystemEpoch(epoch);
    cancelDisplayStack(false);
    mode = MODE_TIME;
    renderClock();
  }
  else if (action == F("ntp"))
  {
    saveConfig();
    if (WiFi.status() != WL_CONNECTED)
    {
      sendJson(409, F("{\"ok\":false,\"error\":\"Home Wi-Fi must be connected for NTP sync.\"}"));
      return;
    }
    configureNtpSync();
    cancelDisplayStack(false);
    mode = MODE_TIME;
    renderClock();
  }
  else if (action == F("timer_set") || action == F("timer_start"))
  {
    const long requestedDuration = webServer.arg("duration").toInt();
    if (requestedDuration < 1 || requestedDuration > 359999)
    {
      sendJson(400, F("{\"ok\":false,\"error\":\"Timer duration must be between 1 second and 99:59:59.\"}"));
      return;
    }
    timerDurationSeconds = (uint32_t)requestedDuration;
    timerRemainingMs = timerDurationSeconds * 1000UL;
    timerState = CHRONO_READY;
    resetTimerBlink();

    cancelDisplayStack(false);
    mode = MODE_TIMER;
    if (action == F("timer_start"))
    {
      if (timerRemainingMs == 0) timerRemainingMs = timerDurationSeconds * 1000UL;
      timerState = CHRONO_RUNNING;
      timerLastUpdateMs = millis();
      resetTimerBlink();
    }
    timerLastRenderedSecond = 0xFFFFFFFFUL;
    renderTimer();
  }
  else if (action == F("timer_pause"))
  {
    cancelDisplayStack(false);
    mode = MODE_TIMER;
    if (timerState == CHRONO_RUNNING)
    {
      advanceTimer();
      if (timerState == CHRONO_RUNNING) timerState = CHRONO_PAUSED;
    }
    else if (timerState == CHRONO_PAUSED || timerState == CHRONO_READY || timerState == CHRONO_FINISHED)
    {
      if (timerRemainingMs == 0) timerRemainingMs = timerDurationSeconds * 1000UL;
      timerState = CHRONO_RUNNING;
      timerLastUpdateMs = millis();
      resetTimerBlink();
    }
    renderTimer();
  }
  else if (action == F("timer_reset"))
  {
    cancelDisplayStack(false);
    timerRemainingMs = timerDurationSeconds * 1000UL;
    timerState = CHRONO_READY;
    timerLastRenderedSecond = 0xFFFFFFFFUL;
    resetTimerBlink();
    mode = MODE_TIMER;
    renderTimer();
  }
  else if (action == F("timer_show"))
  {
    cancelDisplayStack(false);
    mode = MODE_TIMER;
    renderTimer();
  }
  else if (action == F("stopwatch_start"))
  {
    cancelDisplayStack(false);
    mode = MODE_STOPWATCH;
    if (stopwatchState != CHRONO_RUNNING)
    {
      stopwatchStartedMs = millis();
      stopwatchState = CHRONO_RUNNING;
    }
    renderStopwatch();
  }
  else if (action == F("stopwatch_pause"))
  {
    cancelDisplayStack(false);
    mode = MODE_STOPWATCH;
    if (stopwatchState == CHRONO_RUNNING)
    {
      stopwatchElapsedMs = stopwatchElapsedMilliseconds();
      stopwatchState = CHRONO_PAUSED;
    }
    else
    {
      stopwatchStartedMs = millis();
      stopwatchState = CHRONO_RUNNING;
    }
    renderStopwatch();
  }
  else if (action == F("stopwatch_reset"))
  {
    cancelDisplayStack(false);
    stopwatchElapsedMs = 0;
    stopwatchStartedMs = millis();
    stopwatchState = CHRONO_READY;
    stopwatchLastRenderedSecond = 0xFFFFFFFFUL;
    mode = MODE_STOPWATCH;
    renderStopwatch();
  }
  else if (action == F("stopwatch_show"))
  {
    cancelDisplayStack(false);
    mode = MODE_STOPWATCH;
    renderStopwatch();
  }
  else
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Unknown time action.\"}"));
    return;
  }

  saveDisplayState();
  updateClockText();
  updateCalendarText();
  formatDuration(timerRemainingSeconds(), timerText, sizeof(timerText));
  formatDuration(stopwatchElapsedSeconds(), stopwatchText, sizeof(stopwatchText));
  String body = F("{\"ok\":true,\"time\":\"");
  body += clockText;
  body += F("\",\"calendar\":\"");
  body += calendarText;
  body += F("\",\"valid\":");
  body += isClockValid() ? F("true") : F("false");
  body += F(",\"timer\":\"");
  body += timerText;
  body += F("\",\"timerState\":\"");
  body += chronoStateName(timerState);
  body += F("\",\"stopwatch\":\"");
  body += stopwatchText;
  body += F("\",\"stopwatchState\":\"");
  body += chronoStateName(stopwatchState);
  body += F("\"}");
  sendJson(200, body);
}

void handlePaintControl()
{
  const String action = webServer.arg("action");
  bool bitmapChanged = false;

  if (webServer.hasArg("x"))
    paintCursorX = (uint8_t)constrain(webServer.arg("x").toInt(), 0L, (long)MATRIX_WIDTH - 1);
  if (webServer.hasArg("y"))
    paintCursorY = (uint8_t)constrain(webServer.arg("y").toInt(), 0L, (long)MATRIX_HEIGHT - 1);

  if (action == F("show"))
  {
    showPaintMode();
    saveDisplayState();
  }
  else if (action == F("bitmap"))
  {
    if (!loadPaintBitmapHex(webServer.arg("data")))
    {
      sendJson(400, F("{\"ok\":false,\"error\":\"Paint bitmap must contain exactly 128 hexadecimal characters.\"}"));
      return;
    }
    bitmapChanged = true;
  }
  else if (action == F("pixel"))
  {
    const String value = webServer.arg("value");
    if (value == F("toggle")) setPaintPixel(paintCursorX, paintCursorY, !getPaintPixel(paintCursorX, paintCursorY));
    else if (value == F("1")) setPaintPixel(paintCursorX, paintCursorY, true);
    else if (value == F("0")) setPaintPixel(paintCursorX, paintCursorY, false);
    else
    {
      sendJson(400, F("{\"ok\":false,\"error\":\"Pixel value must be 0, 1, or toggle.\"}"));
      return;
    }
    bitmapChanged = true;
  }
  else if (action == F("move"))
  {
    const long nextX = (long)paintCursorX + webServer.arg("dx").toInt();
    const long nextY = (long)paintCursorY + webServer.arg("dy").toInt();
    paintCursorX = (uint8_t)constrain(nextX, 0L, (long)MATRIX_WIDTH - 1);
    paintCursorY = (uint8_t)constrain(nextY, 0L, (long)MATRIX_HEIGHT - 1);
    if (webServer.hasArg("draw"))
    {
      const String draw = webServer.arg("draw");
      if (draw == F("1")) setPaintPixel(paintCursorX, paintCursorY, true);
      else if (draw == F("0")) setPaintPixel(paintCursorX, paintCursorY, false);
      bitmapChanged = draw == F("1") || draw == F("0");
    }
  }
  else if (action == F("clear"))
  {
    memset(paintBitmap, 0, sizeof(paintBitmap));
    bitmapChanged = true;
  }
  else if (action == F("fill"))
  {
    memset(paintBitmap, 0xFF, sizeof(paintBitmap));
    bitmapChanged = true;
  }
  else if (action == F("invert"))
  {
    uint8_t* bytes = &paintBitmap[0][0];
    for (size_t i = 0; i < sizeof(paintBitmap); i++) bytes[i] = (uint8_t)~bytes[i];
    bitmapChanged = true;
  }
  else
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Unknown paint action.\"}"));
    return;
  }

  if (action != F("show"))
  {
    showPaintMode();
    if (bitmapChanged) schedulePaintSave();
    else saveDisplayState();
  }

  String body = F("{\"ok\":true,\"mode\":\"paint\",\"data\":\"");
  body += paintBitmapHex();
  body += F("\",\"cursorX\":");
  body += String(paintCursorX);
  body += F(",\"cursorY\":");
  body += String(paintCursorY);
  body += F("}");
  sendJson(200, body);
}

void handleDeviceSettings()
{
  const long requestedRotation = webServer.arg("rotation").toInt();
  if (requestedRotation != 0 && requestedRotation != 180)
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Rotation must be 0 or 180 degrees.\"}"));
    return;
  }

  config.rotation180 = requestedRotation == 180 ? 1 : 0;
  matrix.setRotation180(config.rotation180 != 0);
  saveConfig();
  applyDisplaySettings();

  String body = F("{\"ok\":true,\"rotation\":");
  body += config.rotation180 ? F("180") : F("0");
  body += F("}");
  sendJson(200, body);
}

void handleWifiUpdate()
{
  String newStaSsid = webServer.arg("staSsid");
  String submittedStaPassword = webServer.arg("staPassword");
  String newApSsid = webServer.arg("apSsid");
  String submittedApPassword = webServer.arg("apPassword");
  const bool openAp = webServer.arg("apOpen") == F("1");

  if (newStaSsid.length() > 32 || (newStaSsid.length() == 0 && submittedStaPassword.length() > 0))
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Home SSID must be 1-32 characters, or both home fields must be blank.\"}"));
    return;
  }

  if (newApSsid.length() == 0 || newApSsid.length() > 32)
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Setup AP name must be 1-32 characters.\"}"));
    return;
  }

  if (submittedStaPassword.length() > 63 || submittedApPassword.length() > 63)
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"A Wi-Fi password cannot exceed 63 characters.\"}"));
    return;
  }

  String effectiveApPassword;
  if (openAp)
  {
    effectiveApPassword = String();
  }
  else if (submittedApPassword.length() > 0)
  {
    effectiveApPassword = submittedApPassword;
  }
  else
  {
    effectiveApPassword = String(config.apPassword);
  }

  if (!openAp && (effectiveApPassword.length() < 8 || effectiveApPassword.length() > 63))
  {
    sendJson(400, F("{\"ok\":false,\"error\":\"Enter an 8-63 character AP password, or select an open AP.\"}"));
    return;
  }

  if (newStaSsid.length() == 0)
  {
    copyString(config.staSsid, sizeof(config.staSsid), String());
    copyString(config.staPassword, sizeof(config.staPassword), String());
  }
  else
  {
    const bool staSsidChanged = newStaSsid != String(config.staSsid);
    copyString(config.staSsid, sizeof(config.staSsid), newStaSsid);
    if (staSsidChanged || submittedStaPassword.length() > 0)
      copyString(config.staPassword, sizeof(config.staPassword), submittedStaPassword);
  }

  copyString(config.apSsid, sizeof(config.apSsid), newApSsid);
  copyString(config.apPassword, sizeof(config.apPassword), effectiveApPassword);
  saveConfig();

  sendJson(200, F("{\"ok\":true,\"restarting\":true}"));
  restartPending = true;
  restartAtMs = millis() + 1500;
}

void configureWebServer()
{
  webServer.on("/", HTTP_GET, sendWebUi);
  webServer.on("/api/status", HTTP_GET, handleStatus);
  webServer.on("/api/text", HTTP_POST, handleTextUpdate);
  webServer.on("/api/queue", HTTP_POST, handleQueueControl);
  webServer.on("/api/game", HTTP_POST, handleGameControl);
  webServer.on("/api/scoreboard", HTTP_POST, handleScoreboardControl);
  webServer.on("/api/animation", HTTP_POST, handleAnimationControl);
  webServer.on("/api/time", HTTP_POST, handleTimeControl);
  webServer.on("/api/paint", HTTP_POST, handlePaintControl);
  webServer.on("/api/device", HTTP_POST, handleDeviceSettings);
  webServer.on("/api/wifi", HTTP_POST, handleWifiUpdate);

  webServer.on("/generate_204", HTTP_GET, sendWebUi);
  webServer.on("/hotspot-detect.html", HTTP_GET, sendWebUi);
  webServer.on("/fwlink", HTTP_GET, sendWebUi);
  webServer.on("/favicon.ico", HTTP_GET, []() { webServer.send(204, "text/plain", ""); });

  webServer.onNotFound([]()
  {
    if (webServer.uri().startsWith("/api/"))
      sendJson(404, F("{\"ok\":false,\"error\":\"Not found.\"}"));
    else
      sendWebUi();
  });

  webServer.begin();
}

void startWifi()
{
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);

  if (strlen(config.apPassword) >= 8)
    WiFi.softAP(config.apSsid, config.apPassword);
  else
    WiFi.softAP(config.apSsid);

  if (config.staSsid[0] != '\0')
  {
    WiFi.begin(config.staSsid, config.staPassword);
    lastReconnectAttemptMs = millis();
  }

  espNowReady = startEspNow();
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  configureWebServer();
}

void maintainStaConnection()
{
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected)
  {
    if (!staWasConnected)
      staWasConnected = true;

    const uint32_t now = millis();
    if (config.ntpServer[0] != '\0' &&
        (!ntpConfiguredForConnection ||
         (uint32_t)(now - lastNtpSyncRequestMs) >= NTP_RESYNC_INTERVAL_MS))
      configureNtpSync();
    return;
  }

  if (staWasConnected)
    staWasConnected = false;

  ntpConfiguredForConnection = false;
  lastNtpSyncRequestMs = 0;
  if (config.staSsid[0] == '\0') return;

  const uint32_t now = millis();
  if ((uint32_t)(now - lastReconnectAttemptMs) >= 30000)
  {
    lastReconnectAttemptMs = now;
    WiFi.begin(config.staSsid, config.staPassword);
  }
}

void setup()
{
  matrix.begin();
  loadConfig();
  matrix.setRotation180(config.rotation180 != 0);

  sanitizeDisplayText(config.lastDisplayText);
  displaySpeedMs = constrain(config.lastDisplaySpeedMs, (uint16_t)15, (uint16_t)500);
  memcpy(paintBitmap, config.paintBitmap, sizeof(paintBitmap));
  clearDisplayStackStorage();
  queuedMessageActive = false;

#if defined(ESP8266)
  randomSeed(micros() ^ ESP.getChipId());
#else
  randomSeed(micros() ^ (uint32_t)ESP.getEfuseMac());
#endif

  resetSpaceGame();
  resetTrexGame();
  resetGravityGame();
  resetTetrisGame();
  resetRacingGame();
  animationFrame = 0;
  animationState = ANIMATION_PLAYING;
  lastAnimationFrameMs = millis();
  timerDurationSeconds = constrain(config.timerDurationSeconds, (uint32_t)1, (uint32_t)359999);
  timerPresentation = config.timerPresentation == (uint8_t)TIMER_PRESENTATION_BAR
    ? TIMER_PRESENTATION_BAR : TIMER_PRESENTATION_TIME;
  timerRemainingMs = timerDurationSeconds * 1000UL;
  timerState = CHRONO_READY;
  stopwatchElapsedMs = 0;
  stopwatchState = CHRONO_READY;
  formatDuration(timerDurationSeconds, timerText, sizeof(timerText));
  formatDuration(0, stopwatchText, sizeof(stopwatchText));

  mode = isRestorableMode(config.lastMode)
    ? (AppMode)config.lastMode
    : MODE_SCROLL_TEXT;
  applyDisplaySettings();

  startWifi();
}

void loop()
{
  matrix.update();

  processPendingEspNowPacket();
  maintainStaConnection();
  maintainIpAnnouncement();

  if (staIpAnnouncementActive)
  {
    staIpScroller.update();
  }
  else if (!updateDisplayStack())
  {
    if (mode == MODE_SCROLL_TEXT) scroller.update();
    else if (mode == MODE_SPACE_GAME) updateSpaceGame();
    else if (mode == MODE_TREX_GAME) updateTrexGame();
    else if (mode == MODE_GRAVITY_GAME) updateGravityGame();
    else if (mode == MODE_TETRIS_GAME) updateTetrisGame();
    else if (mode == MODE_RACING_GAME) updateRacingGame();
    else if (mode == MODE_ANIMATION) updateAnimation();
    else if (mode == MODE_TIME) updateClockDisplay();
    else if (mode == MODE_CALENDAR) updateCalendarDisplay();
    else if (mode == MODE_TIMER) updateTimerDisplay();
    else if (mode == MODE_STOPWATCH) updateStopwatchDisplay();
    else if (mode == MODE_PAINT) updatePaintDisplay();
  }

  flushPendingPaintSave();
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (restartPending && (int32_t)(millis() - restartAtMs) >= 0)
    ESP.restart();
}
