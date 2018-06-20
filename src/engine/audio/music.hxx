#ifndef MUSIC_HXX_
#define MUSIC_HXX_

#include <string>

#include "../basics/log.hxx"
#include "../basics/resources.hxx"

#include "SDL2/SDL.h"
#include "SDL2/SDL_mixer.h"


class Music
{
public:
  Music();
  ~Music();

  /**
    * Loads a music file.
    * @param filename Filename of the music file that should be loaded
    */
  void loadMusic(const std::string &filename);
  
  /**
    * Plays the loaded music file.
    * @param loops Amount of loops the track. -1 = infinite
    */
  void play(int loops);

  /**
    * Stops the music
    */
  void stop();

  /**
    * Pauses the music
    */
  void pause();

  /**
    * Resumes the music
    */
  void resume();

  /**
  * Checks if this audio object is currently playing and music.
  * @returns bool is music playing
  */
  bool isPlaying();

private:
  Mix_Music* _music = nullptr;
};

#endif