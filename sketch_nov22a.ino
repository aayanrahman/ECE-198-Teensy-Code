#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// Audio objects
AudioInputI2S            i2s_input;      
AudioPlaySdWav           playSdWav1;     
AudioAnalyzePeak         peak1;          
AudioOutputI2S           i2s_output;     
AudioControlSGTL5000     sgtl5000_1;

// Audio connections
AudioConnection          patchCord1(i2s_input, 0, peak1, 0);       
AudioConnection          patchCord2(playSdWav1, 0, i2s_output, 0); 
AudioConnection          patchCord3(playSdWav1, 1, i2s_output, 1); 

// Settings
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  11  
#define SDCARD_SCK_PIN   13   
#define LOUD_THRESHOLD   0.8  // INCREASED - less sensitive now
#define LED_PIN          5    
#define NUM_SONGS        4    

String songs[] = {"music0.wav", "music1.wav", "music2.wav", "music3.wav"};
String songNames[] = {"Track 1", "Track 2", "Track 3", "Track 4"};
int currentSong = 0;
bool musicPlaying = false;
bool isPaused = false;

// Loud sound detection variables
bool soundDetected = false;
unsigned long lastLoudTime = 0;
const unsigned long DETECTION_COOLDOWN = 2000;

// Display refresh variables
unsigned long lastActivityTime = 0;
const unsigned long CLEAR_DELAY = 3000; // Clear after 3 seconds of inactivity
bool needsRefresh = false;

void setup() {
  Serial.begin(9600);
  delay(1000);
  
  // Seed random number generator
  randomSeed(analogRead(A0));
  currentSong = random(0, NUM_SONGS);
  
  // Allocate audio memory
  AudioMemory(40);
  
  // Enable audio shield
  if (!sgtl5000_1.enable()) {
    Serial.println("[ERROR] Audio shield failed!");
    while(1) delay(1000);
  }
  
  // Configure audio settings
  sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);  
  sgtl5000_1.micGain(24);
  sgtl5000_1.volume(0.6);           
  sgtl5000_1.lineOutLevel(13);      
  
  // Initialize SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  
  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("[ERROR] SD card failed!");
    Serial.println("Check card is inserted and formatted");
    while(1) delay(1000);
  }
  
  // Setup LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  showInterface();
  lastActivityTime = millis();
}

void showInterface() {
  
  Serial.println("\n========================================");
  Serial.println("      THERAPEUTIC MUSIC PLAYER");
  Serial.println("========================================");
  Serial.println("\nCommands:");
  Serial.println("  play   - Start playback");
  Serial.println("  pause  - Pause/Resume");
  Serial.println("  next   - Next track");  
  Serial.println("  rewind - Restart track");
  Serial.println("----------------------------------------");
  Serial.println("Ready. Monitoring for loud sounds...\n");
  
  // Show current status if playing
  if (musicPlaying) {
    if (isPaused) {
      Serial.print("|| Paused: ");
    } else {
      Serial.print("♪ Now Playing: ");
    }
    Serial.println(songNames[currentSong]);
  }
}

void loop() {
  // Check for serial commands
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    processCommand(command);
    lastActivityTime = millis();
    needsRefresh = true;
  }
  
  // Check microphone level for loud sound detection
  if (peak1.available()) {
    float volume = peak1.read();
    
    // Detect loud sound with cooldown period
    if (volume > LOUD_THRESHOLD) {
      if (!soundDetected && (millis() - lastLoudTime > DETECTION_COOLDOWN)) {
        Serial.println("\n[ALERT] LOUD SOUND DETECTED");
        Serial.println("Volume: " + String(volume));
        
        soundDetected = true;
        lastLoudTime = millis();
        lastActivityTime = millis();
        needsRefresh = true;
      }
    } else {
      soundDetected = false;
    }
  }
  
  // Auto-refresh display after inactivity
  if (needsRefresh && (millis() - lastActivityTime > CLEAR_DELAY)) {
    showInterface();
    needsRefresh = false;
  }
  
  // Auto-loop current song if playing and not paused
  if (musicPlaying && !isPaused && !playSdWav1.isPlaying()) {
    playSdWav1.play(songs[currentSong].c_str());
    delay(10);
  }
  
  // Independent LED blinking
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}

void processCommand(String cmd) {
  if (cmd == "play") {
    if (!musicPlaying) {
      playCurrentSong();
    } else if (isPaused) {
      resumeMusic();
    } else {
      Serial.println("Already playing");
    }
  }
  else if (cmd == "pause") {
    if (musicPlaying && !isPaused) {
      pauseMusic();
    } else if (isPaused) {
      resumeMusic();
    } else {
      Serial.println("Nothing to pause");
    }
  }
  else if (cmd == "next") {
    nextSong();
  }
  else if (cmd == "rewind") {
    rewindSong();
  }
  else if (cmd == "clear") {
    showInterface();
  }
  else if (cmd != "") {
    Serial.println("Unknown command");
  }
}

void playCurrentSong() {
  if (playSdWav1.play(songs[currentSong].c_str())) {
    Serial.print("\n♪ Playing: ");
    Serial.println(songNames[currentSong]);
    musicPlaying = true;
    isPaused = false;
  } else {
    Serial.println("Error playing file");
  }
}

void pauseMusic() {
  if (musicPlaying && playSdWav1.isPlaying()) {
    playSdWav1.stop();
    isPaused = true;
    Serial.print("\n|| Paused: ");
    Serial.println(songNames[currentSong]);
  }
}

void resumeMusic() {
  if (isPaused) {
    playSdWav1.play(songs[currentSong].c_str());
    isPaused = false;
    Serial.print("\n♪ Resumed: ");
    Serial.println(songNames[currentSong]);
  }
}

void nextSong() {
  if (playSdWav1.isPlaying()) {
    playSdWav1.stop();
  }
  
  currentSong = (currentSong + 1) % NUM_SONGS;
  
  if (musicPlaying) {
    playCurrentSong();
  } else {
    Serial.print("\nReady: ");
    Serial.println(songNames[currentSong]);
  }
}

void rewindSong() {
  if (musicPlaying) {
    playSdWav1.stop();
    delay(10);
    Serial.print("\n↺ Restarted: ");
    Serial.println(songNames[currentSong]);
    playSdWav1.play(songs[currentSong].c_str());
  } else {
    Serial.println("Nothing to rewind");
  }
}