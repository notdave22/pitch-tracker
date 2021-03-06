#include <algorithm>
#include "audio_stream.hpp"
#include "pitchtracker.hpp"
#include <iterator>
#include <fstream>
#include <iostream>
#include <string>
#include "RtAudio.h"

#define FORMAT RTAUDIO_SINT16
typedef signed short s_type;

template <typename T>
void writeToFile(std::string f_path, const std::vector<T>& buffer_data){
  std::ofstream f_out(f_path, std::ios::app);
  std::copy(buffer_data.begin(), buffer_data.end(), std::ostream_iterator<T>(f_out, " "));
  f_out.close();
}

/* Callback (audio processing/mpm here) */
int input( void * /* out_buff */, void *inputBuffer, unsigned int nBufferFrames,
           double /*streamTime*/, RtAudioStreamStatus /*status*/, void *data )
{
  InputData *iData = (InputData *) data;
  // Simply copy the data to our allocated buffer.
  unsigned int frames = nBufferFrames;
  unsigned int *bytes = (unsigned int *) data;
  if ( iData->frameCounter + nBufferFrames > iData->totalFrames ) {
    frames = iData->totalFrames - iData->frameCounter;
    iData->bufferBytes = frames * iData->channels * sizeof( signed short );
  }

  //send the data to a vector<double> and then call MPM on it 
  unsigned long offset = iData->frameCounter * iData->channels;
  std::vector<double> temp_buffer;
  memcpy( iData->buffer, inputBuffer, iData->bufferBytes );
  unsigned size = sizeof(iData->buffer) / sizeof(s_type);
  temp_buffer.insert(temp_buffer.end(), &iData->buffer[0], &iData->buffer[940]);
  double estimate = mpm(temp_buffer, 44100);
  //writeToFile("/home/dave/projects/pitch-tracker/src/rec.txt", temp_buffer);
  std::cout <<  "\nestimation: " << estimate << "\r" << std::flush;
  //iData->frameCounter += frames;
  if ( iData->frameCounter >= iData->totalFrames ) return 2;
  return 0;
}

//for now we'll just write the results to stdout 
void trackPitch(/*int total_bytes*/){
  //rtaudio setup
  unsigned int channels = 1;
  int sampleRate = 44100;
  unsigned int buffSize = 940; 
  //unsigned int nBuffers = 4;
  s_type*  buffer;
  unsigned int device = 0; //default
  unsigned int offset = 0;
    
  RtAudio audio;
  RtAudio::StreamParameters iParams; 

  if(audio.getDeviceCount() < 1){
    std::cout << "no devices found!" << std::endl;
    return;
  }

  if(device==0)
    iParams.deviceId = audio.getDefaultInputDevice();
  else
    iParams.deviceId = device;

  iParams.nChannels = channels;
  iParams.firstChannel = offset; 
  
  InputData data;
  data.buffer = 0;

  try {
    audio.openStream(NULL, &iParams, FORMAT, sampleRate, &buffSize, &input, (void *)&data);
  }
  catch (RtAudioError& e) {
      std:: cout << '\n' << e.getMessage() << '\n' << std::endl;
      return;
  }

  double time = 2.0; //effectively just the buffer width
  data.bufferBytes = buffSize * channels * sizeof (s_type);
  data.totalFrames = (unsigned long) (sampleRate * time);
  data.frameCounter = 0;
  data.channels = channels;
  unsigned long totalBytes;
  totalBytes = data.totalFrames * channels * sizeof (s_type);
  std::cout << "total frames: " << data.totalFrames << std::endl;
  std::cout << "total bytes: " << totalBytes << std::endl;
  
  //malloc the buffer 
  data.buffer = (s_type*) malloc(totalBytes);
  if(data.buffer == 0){
    std::cout << "ALLOC ERROR\n";
    return; 
  }

  try{
    audio.startStream();
  } catch(RtAudioError& e){
    std:: cout << '\n' << e.getMessage() << '\n';
    return;
  }
  std::cout << "STARTING STREAM: " << std::endl;
  
  //look into window overlapping (might be necessary)
  while (audio.isStreamRunning()){
    /* analyze every 100ms of frames using MPM */
    /* possible approach: continuosly divide the buffer into chunks of data, and analyze each chunk
       e.g. [0....cur_buffer_position] | -> chunk -> MPM(chunk) [old_buffer_position .... cur_buffer_position] -> chunk -> MPM(Chunk)
       */
    SLEEP(100);
  }
  
  try{
    audio.closeStream();
  }catch(RtAudioError &e){
    e.printMessage();
  }
  if(data.buffer)
    free(data.buffer);
}

int main(){
  //...cli here...
  std::cout << "running tracker...";
  trackPitch();
  return 0;
}
