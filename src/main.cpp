#include "Arduino.h"
#include <SoftwareSerial.h>
#include "DFMiniMp3.h"
#include <EEPROM.h>

#define EEPROM_PLAYLIST_BEGIN 10
#define EEPROM_VOLUME 20

#define BUTTON_VOLUME_DOWN 2
#define BUTTON_VOLUME_UP 3
#define BUTTON_PREVIOUS 4
#define BUTTON_PLAYLIST_1 5
#define BUTTON_PLAYLIST_2 6
#define BUTTON_PLAYLIST_3 7
#define BUTTON_PLAYLIST_4 8
#define BUTTON_PLAYLIST_5 9
#define BUTTON_PlAYLIST_6 12

int playlistButtons[6] = { BUTTON_PLAYLIST_1, BUTTON_PLAYLIST_2, BUTTON_PLAYLIST_3, BUTTON_PLAYLIST_4, BUTTON_PLAYLIST_5, BUTTON_PlAYLIST_6 };

class Mp3Notify;

// define a handy type using serial and our notify class
typedef DFMiniMp3<SoftwareSerial, Mp3Notify, Mp3ChipMH2024K16SS> DfMp3;

SoftwareSerial mySoftwareSerial(10, 11); // RX, TX
DfMp3 dfmp3(mySoftwareSerial);

uint8_t currentPlaylist = 1;
uint8_t currentTrack = 1;
uint8_t volume = 3;

void playPlaylist(int playlist);

// implement a notification class,
// its member methods will get called
//
class Mp3Notify
{
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action)
  {
    if (source & DfMp3_PlaySources_Sd)
    {
      Serial.print("SD Card, ");
    }
    if (source & DfMp3_PlaySources_Usb)
    {
      Serial.print("USB Disk, ");
    }
    if (source & DfMp3_PlaySources_Flash)
    {
      Serial.print("Flash, ");
    }
    Serial.println(action);
  }
  static void OnError(DfMp3& mp3, uint16_t errorCode)
  {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }
  static void OnPlayFinished(DfMp3& mp3, DfMp3_PlaySources source, uint16_t track)
  {
    Serial.print("Play finished for #");
    Serial.println(track);
    playPlaylist(currentPlaylist);
  }
  static void OnPlaySourceOnline(DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "online");
  }
  static void OnPlaySourceInserted(DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "inserted");

    uint16_t count = dfmp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
    Serial.print("files ");
    Serial.println(count);
  }
  static void OnPlaySourceRemoved(DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "removed");
  }
};

void saveCurrentTrackPlaylist(int playlist, uint8_t currentTrack)
{
  EEPROM.write(EEPROM_PLAYLIST_BEGIN + playlist, currentTrack);
}

uint8_t readCurrentTrackPlaylist(int playlist)
{
  return EEPROM.read(EEPROM_PLAYLIST_BEGIN + playlist);
}

void playPlaylist(int playlist)
{
  uint8_t trackCount = dfmp3.getFolderTrackCount(playlist);
  Serial.print("track count in ");
  Serial.print(playlist);
  Serial.print(": ");
  Serial.println(trackCount);
  if (trackCount == 0)
  {
    return;
  }

  if (currentPlaylist != playlist)
  {
    Serial.print("Switch to playlist #");
    Serial.println(playlist);
    // play current
    currentTrack = readCurrentTrackPlaylist(playlist);
    currentPlaylist = playlist;
    if (currentTrack > trackCount)
    {
      currentTrack = trackCount;
    }
  }
  else
  {
    Serial.println("Next track");
    // play next
    currentTrack = (currentTrack % trackCount) + 1;
  }

  Serial.print("Play ");
  Serial.print(playlist);
  Serial.print(" ");
  Serial.println(currentTrack);
  dfmp3.playFolderTrack(playlist, currentTrack);
  saveCurrentTrackPlaylist(playlist, currentTrack);
}

void setVolume(uint8_t newVolume)
{
  if (newVolume > 10)
  {
    volume = 10;
  }
  else {
    volume = newVolume;
  }

  EEPROM.write(EEPROM_VOLUME, volume);
  Serial.print("Set volume ");
  Serial.println(volume);
  dfmp3.setVolume(volume);
}

void playPreviousTrack()
{
  static unsigned long lastPrevious = 0;
  if (millis() - lastPrevious > 5000)
  {
    // back to begin of the song
    dfmp3.playFolderTrack(currentPlaylist, currentTrack);
  }
  else
  {
    // previous song
    if (currentTrack > 1)
    {
      currentTrack -= 1;
    }
    else
    {
      currentTrack = dfmp3.getFolderTrackCount(currentPlaylist);
    }
    dfmp3.playFolderTrack(currentPlaylist, currentTrack);

    saveCurrentTrackPlaylist(currentPlaylist, currentTrack);
  }

  lastPrevious = millis();
}

void buttonUp(int button)
{
  if (button == BUTTON_VOLUME_UP)
  {
    Serial.println("VOLUME UP");
    setVolume(volume + 1);
  }

  if (button == BUTTON_VOLUME_DOWN)
  {
    Serial.println("VOLUME DOWN");
    if (volume > 0) {
      setVolume(volume - 1);
    }
  }

  if (button == BUTTON_PREVIOUS)
  {
    Serial.println("PREV SONG");
    playPreviousTrack();
  }

  for (int i = 0; i < 6; i++)
  {
    if (button == playlistButtons[i])
    {
      playPlaylist(1 + i);
    }
  }
}

void readButtonStates()
{
  static int lastPressed = -1;
  static int currentButton = -1;
  unsigned int debounceDelay = 50;
  static unsigned long lastDebounceTime = 0;

  int buttons[] = { BUTTON_VOLUME_DOWN,
                   BUTTON_VOLUME_UP,
                   BUTTON_PREVIOUS,
                   BUTTON_PLAYLIST_1,
                   BUTTON_PLAYLIST_2,
                   BUTTON_PLAYLIST_3,
                   BUTTON_PLAYLIST_4,
                   BUTTON_PLAYLIST_5,
                   BUTTON_PlAYLIST_6 };
  int pressed = -1;
  for (int i = 0; i < 9; i++)
  {
    if (digitalRead(buttons[i]) == 0)
    {
      pressed = buttons[i];
      break;
    }
  }

  // Reset the lastDebounceTime if the reading is not stable yet
  if (lastPressed != pressed)
  {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay)
  {

    if (pressed == -1 && currentButton != -1)
    {
      buttonUp(currentButton);
    }
    currentButton = pressed;
  }

  lastPressed = pressed;
}

void setupButtons()
{
  pinMode(BUTTON_VOLUME_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_VOLUME_UP, INPUT_PULLUP);
  pinMode(BUTTON_PREVIOUS, INPUT_PULLUP);

  pinMode(BUTTON_PLAYLIST_1, INPUT_PULLUP);
  pinMode(BUTTON_PLAYLIST_2, INPUT_PULLUP);
  pinMode(BUTTON_PLAYLIST_3, INPUT_PULLUP);
  pinMode(BUTTON_PLAYLIST_4, INPUT_PULLUP);
  pinMode(BUTTON_PLAYLIST_5, INPUT_PULLUP);
  pinMode(BUTTON_PlAYLIST_6, INPUT_PULLUP);
}

void setupDFPlayer()
{
  dfmp3.begin(9600);

  Serial.print("Reset... ");
  dfmp3.reset();
  delay(1000);
  Serial.println("Done");

  volume = EEPROM.read(EEPROM_VOLUME);
  if (volume > 10) volume = 3;
  setVolume(volume);
  delay(200);
  Serial.println("Done");

  uint8_t volume = dfmp3.getVolume();
  Serial.print("volume ");
  Serial.println(volume);

  uint16_t count = dfmp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
  Serial.print("files ");
  Serial.println(count);

  for (int i = 1; i <= 6; i++) {
    count = dfmp3.getFolderTrackCount(i);
    Serial.print("files ");
    Serial.println(count);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("initializing...");

  setupDFPlayer();
  setupButtons();

  currentTrack = readCurrentTrackPlaylist(currentPlaylist);
}

void waitMilliseconds(uint16_t msWait)
{
  uint32_t start = millis();

  while ((millis() - start) < msWait)
  {
    dfmp3.loop();
    readButtonStates();
    delay(1);
  }
}

void loop()
{
  waitMilliseconds(1000);
}