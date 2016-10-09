/*
 *      Copyright (C) 2008-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <kodi/audiodecoder/AudioDecoder.h>
#include <kodi/General.h>
#include <kodi/VFS.h>

#include "StSoundLibrary.h"
#include "YmMusic.h"

#include <stdio.h>
#include <stdint.h>

class CYMCodecAddon
  : public kodi::addon::CInstanceAudioDecoder
{
public:
  CYMCodecAddon(KODI_HANDLE instance);
  virtual ~CYMCodecAddon();

  virtual bool Init(std::string file, unsigned int filecache,
                    AUDIODEC_STREAM_INFO& streamInfo);

  virtual int64_t Seek(int64_t time);
  virtual int ReadPCM(uint8_t* buffer, int size, int& actualsize);
  virtual bool ReadTag(std::string file, std::string& title, std::string& artist, int& length);

private:
  YMMUSIC *m_pMusic;
};


CYMCodecAddon::CYMCodecAddon(KODI_HANDLE instance)
  : CInstanceAudioDecoder(instance),
    m_pMusic(nullptr)
{
}

CYMCodecAddon::~CYMCodecAddon()
{
  if (m_pMusic)
  {
    ymMusicStop(m_pMusic);
    ymMusicDestroy(m_pMusic);
  }
}

bool CYMCodecAddon::Init(std::string filename, unsigned int filecache,
                    AUDIODEC_STREAM_INFO& streamInfo)
{
  m_pMusic = ymMusicCreate();
  if (!m_pMusic)
    return false;

  kodi::vfs::CFile file;
  if (!file.OpenFile(filename))
    return false;

  int len = file.GetLength();
  char *data = new char[len];
  if (!data)
  {
    ymMusicDestroy(m_pMusic);
    return false;
  }

  file.Read(data, len);

  int res = ymMusicLoadMemory(m_pMusic, data, len);
  delete[] data;
  if (res)
  {
    ymMusicSetLoopMode(m_pMusic, YMFALSE);
    ymMusicPlay(m_pMusic);
    ymMusicInfo_t info;
    ymMusicGetInfo(m_pMusic, &info);

    streamInfo.channels = 1;
    streamInfo.samplerate = 44100;
    streamInfo.bitspersample = 16;
    streamInfo.totaltime  = info.musicTimeInSec*1000;
    streamInfo.format = AUDIO_FMT_S16NE;
    streamInfo.channellist = { AUDIO_CH_FL, AUDIO_CH_FR, AUDIO_CH_NULL };
    streamInfo.bitrate = 0;

    return m_pMusic;
  }

  ymMusicDestroy(m_pMusic);
  m_pMusic = nullptr;

  return false;
}

int64_t CYMCodecAddon::Seek(int64_t time)
{
  if (ymMusicIsSeekable(m_pMusic))
  {
    ymMusicSeek(m_pMusic, time);
    return time;
  }

  return 0;
}

int CYMCodecAddon::ReadPCM(uint8_t* buffer, int size, int& actualsize)
{
  if (!buffer)
    return 1;

  if (ymMusicCompute(m_pMusic,(ymsample*)buffer,size/2))
  {
    actualsize = size;
    return 0;
  }
  else
    return 1;
}

bool CYMCodecAddon::ReadTag(std::string filename, std::string& title, std::string& artist, int& length)
{
  kodi::vfs::CFile file;
  if (!file.OpenFile(filename))
    return false;

  int len = file.GetLength();
  char *data = new char[len];
  YMMUSIC *pMusic = (YMMUSIC*)new CYmMusic;

  if (!data || !pMusic)
    return false;

  file.Read(data, len);

  length = 0;
  if (ymMusicLoadMemory(pMusic, data, len))
  {
    ymMusicInfo_t info;
    ymMusicGetInfo(pMusic, &info);
    title = info.pSongName;
    artist = info.pSongAuthor;
    length = info.musicTimeInSec;
  }
  delete[] data;

  ymMusicDestroy(pMusic);

  return (length != 0);
}


class CMyAddon : public ::kodi::addon::CAddonBase
{
public:
  CMyAddon() { }
  virtual ADDON_STATUS CreateInstance(int instanceType,
                                      std::string instanceID,
                                      KODI_HANDLE instance,
                                      KODI_HANDLE& addonInstance) override;
};

ADDON_STATUS CMyAddon::CreateInstance(int instanceType, std::string instanceID, KODI_HANDLE instance, KODI_HANDLE& addonInstance)
{
  kodi::Log(LOG_NOTICE, "Creating Timidity Audio Decoder");
  addonInstance = new CYMCodecAddon(instance);
  return ADDON_STATUS_OK;
}

ADDONCREATOR(CMyAddon);
