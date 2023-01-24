boolean audioRec = false;
double fftResult[NUM_GEQ_CHANNELS];

// Read the UDP audio data sent by WLED-Audio
bool newReading;
void readAudioUDP() {

  if (sync.read()) {

    audioRec = true;

      for (int i = 0; i < NUM_GEQ_CHANNELS; i++) {
        fftResult[i] = sync.fftResult[i];
      }
      // Serial.println("Finished parsing UDP Sync Packet");
      newReading = true;
  }
  else {
    newReading = false;
  }
}
