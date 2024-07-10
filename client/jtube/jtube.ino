// JPEGDEC example for Adafruit GFX displays

#include <HTTPClient.h>
#include <WiFi.h>
#include "JPEGDEC.h"
#include <M5Cardputer.h>

#define SAMPLES_PER_CHUNK 16000
#define SAMPLE_RATE 16000

const short DISPLAY_WIDTH = 240;
const short DISPLAY_HEIGHT = 135;
long currentTime = 0;
long lastLoopTime = 0;

// You will need to change these ips to be your computer ips
const char* SEARCH_URL = "http://10.0.0.194:8123/search";
const char* WIFI_SSID = "YOUR WIFI NAME HERE";
const char* WIFI_PASSWORD = "YOUR WIFI PASS HERE";
const char* FRAME_URL = "http://10.0.0.194:8123/frame/0/";
const char* AUDIO_URL = "http://10.0.0.194:8123/audio/0/";

long mCurrentAudioSample = 0;
int8_t* audioBuffer;
size_t bufferLength;

HTTPClient http;
JPEGDEC jpeg;

String searchQuery = "enter search...";
bool playingVideo = false;

int JPEGDraw(JPEGDRAW *pDraw)
{
  M5.Lcd.drawBitmap((int16_t)pDraw->x, (int16_t)pDraw->y, (int16_t)pDraw->iWidth, (int16_t)pDraw->iHeight, pDraw->pPixels);
  return 1;
} /* JPEGDraw() */

void setup()
{
  Serial.begin(115200);
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(255);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    M5.Lcd.fillScreen(TFT_YELLOW);
    delay(500);
  }
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  M5Cardputer.Display.setTextColor(GREEN);
  M5Cardputer.Display.setTextDatum(middle_center);
  M5Cardputer.Display.setTextFont(&fonts::FreeSerifBoldItalic18pt7b);
  M5Cardputer.Display.setTextSize(1);

  bufferLength = SAMPLES_PER_CHUNK;
  audioBuffer = (int8_t *)malloc(bufferLength);

  http.setReuse(true);

  M5.Lcd.fillScreen(TFT_BLACK);
  M5Cardputer.Display.drawString(searchQuery,
  M5Cardputer.Display.width() / 2,
  M5Cardputer.Display.height() / 2);
} /* setup() */

void getVideo()
{
  char url[64];
  snprintf(url, 64, "%s%ld", FRAME_URL, currentTime);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0)
  {
      if (httpCode == HTTP_CODE_OK)
      {  
        int jpegLength = http.getSize();
        uint8_t downloadBuffer[jpegLength];
        http.getStreamPtr()->readBytes(downloadBuffer, jpegLength);

        if (jpeg.openRAM(downloadBuffer, jpegLength, JPEGDraw))
        {
          jpeg.decode(0, 0, 0);
          jpeg.close();
        }
      }
      else 
      {
        M5.Lcd.fillScreen(TFT_RED);
      }
  }
  else
  {
    M5.Lcd.fillScreen(TFT_BLUE);
  }
  http.end();
}

int getAudioSamples(int8_t **buffer, size_t &bufferSize, int currentAudioSample)
{
  // resize the buffer if needed
  if (bufferSize < SAMPLES_PER_CHUNK)
  {
    *buffer = (int8_t *)realloc(*buffer, SAMPLES_PER_CHUNK);
    bufferSize = SAMPLES_PER_CHUNK;
  }

  char url[64];
  snprintf(url, 64, "%s%d/%d", AUDIO_URL, currentAudioSample, bufferSize);
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    // read the audio data into the buffer
    int audioLength = http.getSize();
    if (audioLength > 0) {
      http.getStreamPtr()->readBytes((uint8_t *) *buffer, audioLength);
    }
    http.end();
    return audioLength;
  }
  http.end();
  return 0;
}

void playVideo()
{
  if(!M5Cardputer.Speaker.isPlaying()) {
      int audioLength = getAudioSamples(&audioBuffer, bufferLength, mCurrentAudioSample);
      if (audioLength > 0) {
          // play the audio
          M5Cardputer.Speaker.playRaw(audioBuffer, audioLength, SAMPLE_RATE, false, 1, false);
          mCurrentAudioSample += audioLength;
          currentTime = 1000 * mCurrentAudioSample / SAMPLE_RATE;
          getVideo();
          lastLoopTime = millis();
      }
  }
  else {
    long now = millis();
    long diff = now - lastLoopTime;
    if(diff > 1000 / 15)
    {
        currentTime += diff;
        getVideo();
        lastLoopTime = millis();
    }
  }

  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange()) {
    if (M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        if (status.del)
        {
          playingVideo = false;
        }
    }
  }
}

void showSearchScreen()
{
  delay(1);
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange()) {

      M5.Lcd.fillScreen(TFT_BLACK);
      M5Cardputer.Display.drawString(searchQuery,
      M5Cardputer.Display.width() / 2,
      M5Cardputer.Display.height() / 2);

      if (M5Cardputer.Keyboard.isPressed()) {
          Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

          for (auto i : status.word)
          {
              searchQuery += i;
          }

          if (status.del)
          {
              searchQuery.remove(searchQuery.length() - 1);
          }

          if (status.enter)
          {
              sendSearchQuery();
              playingVideo = true;
              searchQuery = "";
          }
      }
  }
}

void sendSearchQuery()
{
  M5.Lcd.fillScreen(TFT_BLACK);
  M5Cardputer.Display.drawString("LOADING...",
  M5Cardputer.Display.width() / 2,
  M5Cardputer.Display.height() / 2);

  // Creating new client because I am setting headers and am not sure if those will be unset when I call end. And I can afford the resources
  HTTPClient searchHttp;
  searchHttp.begin(SEARCH_URL);
  http.addHeader("Content-Type", "text/plain");
  int httpCode = searchHttp.POST(searchQuery);
  searchHttp.end();
}

void loop()
{
  if(playingVideo)
  {
    playVideo();
  }
  else
  {
    showSearchScreen();
  }
} 
