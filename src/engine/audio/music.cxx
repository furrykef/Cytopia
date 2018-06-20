#include "music.hxx"
//#include "basics\resources.hxx"

Music::Music()
{

}

Music::~Music()
{
  if (_music)
  {
    Mix_FreeMusic(_music);
  }
}

void Music::loadMusic(const std::string &filename)
{
  _music = Mix_LoadMUS(filename.c_str());
  if (!_music)
  {
    LOG(LOG_ERROR) << "Failed to load audio file " << filename << "\n" << Mix_GetError();
  }

}

void Music::play(int loops)
{
  if (Resources::settings.playMusic)
  {
    if (_music)
    {
      // stop the music playback first to start from the beginning
      stop();

      if ( Mix_PlayMusic(_music, loops) == -1)
      {
        LOG(LOG_ERROR) << "Failed to play music!\n" << Mix_GetError();
      }
    }
    else
    {
      LOG(LOG_ERROR) << "No music file loaded but play() is called!\n";
    }
  }
}

void Music::stop()
{
  if (Resources::settings.playMusic)
  {
    // Reset the music file to the beginning
    Mix_HookMusic(NULL, NULL);
    Mix_HaltMusic();
  }
}

void Music::pause()
{
  if (Resources::settings.playMusic)
  {
    Mix_PauseMusic();
  }
}

void Music::resume()
{
  if (Resources::settings.playMusic)
  {
    Mix_ResumeMusic();
  }
}

bool Music::isPlaying()
{
  if (Resources::settings.playMusic)
  {
    // returns amount of playing audiochannels
    return Mix_Playing(-1) != 0;
  }
  return false;
}
