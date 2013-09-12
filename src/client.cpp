/*
 *      Copyright (C) 2005-2011 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "client.h"
#include "xbmc_pvr_dll.h"
#include "HTSPData.h"
#include "HTSPDemux.h"
#include "GUIDialogTranscode.h"
#include "platform/threads/mutex.h"
#include "platform/util/atomic.h"
#include "platform/util/util.h"

using namespace std;
using namespace ADDON;
using namespace PLATFORM;

bool         m_bCreated  = false;
ADDON_STATUS m_CurStatus = ADDON_STATUS_UNKNOWN;
int          g_iClientId = -1;
long         g_iPacketSequence = 0;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string     g_strHostname         = DEFAULT_HOST;
int             g_iPortHTSP           = DEFAULT_HTSP_PORT;
int             g_iPortHTTP           = DEFAULT_HTTP_PORT;
int             g_iConnectTimeout     = DEFAULT_CONNECT_TIMEOUT;
int             g_iResponseTimeout    = DEFAULT_RESPONSE_TIMEOUT;
bool            g_bTranscode          = DEFAULT_TRANSCODE;
CodecDescriptor g_audioCodec;
CodecDescriptor g_videoCodec;
int             g_iResolution         = DEFAULT_RESOLUTION;
std::string     g_strUsername         = "";
std::string     g_strPassword         = "";
std::string     g_strUserPath         = "";
std::string     g_strClientPath       = "";

CHelper_libXBMC_addon *XBMC           = NULL;
CHelper_libXBMC_codec *CODEC          = NULL;
CHelper_libXBMC_pvr   *PVR            = NULL;
CHelper_libXBMC_gui   *GUI            = NULL;
PVR_MENUHOOK          *menuHook       = NULL;
CHTSPData *            HTSPData       = NULL;
CMutex g_seqMutex;

uint32_t HTSPNextSequenceNumber(void)
{
  long lSequence = atomic_inc(&g_iPacketSequence);

  if ((uint32_t)lSequence != lSequence)
  {
    CLockObject lock(g_seqMutex);
    if ((uint32_t)g_iPacketSequence != g_iPacketSequence)
      g_iPacketSequence = 0;

    lSequence = atomic_inc(&g_iPacketSequence);
  }

  return (uint32_t)lSequence;
}

extern "C" {

void ADDON_ReadSettings(void)
{
  /* read setting "host" from settings.xml */
  char * buffer;
  buffer = (char*) malloc (1024);
  buffer[0] = 0; /* Set the end of string */

  if (XBMC->GetSetting("host", buffer))
    g_strHostname = buffer;
  else
    g_strHostname = DEFAULT_HOST;
  buffer[0] = 0; /* Set the end of string */

  /* read setting "user" from settings.xml */
  if (XBMC->GetSetting("user", buffer))
    g_strUsername = buffer;
  else
    g_strUsername = "";
  buffer[0] = 0; /* Set the end of string */

  /* read setting "pass" from settings.xml */
  if (XBMC->GetSetting("pass", buffer))
    g_strPassword = buffer;
  else
    g_strPassword = "";

  /* read setting "htsp_port" from settings.xml */
  if (!XBMC->GetSetting("htsp_port", &g_iPortHTSP))
    g_iPortHTSP = DEFAULT_HTSP_PORT;

  /* read setting "http_port" from settings.xml */
  if (!XBMC->GetSetting("http_port", &g_iPortHTTP))
    g_iPortHTTP = DEFAULT_HTTP_PORT;

  /* read setting "connect_timeout" from settings.xml */
  if (!XBMC->GetSetting("connect_timeout", &g_iConnectTimeout))
    g_iConnectTimeout = DEFAULT_CONNECT_TIMEOUT;

  /* read setting "read_timeout" from settings.xml */
  if (!XBMC->GetSetting("response_timeout", &g_iResponseTimeout))
    g_iResponseTimeout = DEFAULT_RESPONSE_TIMEOUT;

  /* read setting "transcode" from settings.xml */
  if (!XBMC->GetSetting("transcode", &g_bTranscode))
    g_bTranscode = DEFAULT_TRANSCODE;

  /* read setting "audio_codec_name" from settings.xml */
  if (XBMC->GetSetting("audio_codec_name", buffer))
    g_audioCodec = CodecDescriptor::GetCodecByName(buffer);
  else
    g_audioCodec = CodecDescriptor::GetCodecByName(DEFAULT_AUDIO_CODEC);

  /* read setting "video_codec_name" from settings.xml */
  if (XBMC->GetSetting("video_codec_name", buffer))
    g_videoCodec = CodecDescriptor::GetCodecByName(buffer);
  else
    g_videoCodec = CodecDescriptor::GetCodecByName(DEFAULT_VIDEO_CODEC);

  /* read setting "resolution" from settings.xml */
  if (!XBMC->GetSetting("resolution", &g_iResolution))
    g_iResolution = DEFAULT_RESOLUTION;

  free(buffer);
}

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!hdl || !props)
    return ADDON_STATUS_UNKNOWN;

  PVR_PROPERTIES* pvrprops = (PVR_PROPERTIES*)props;

  XBMC = new CHelper_libXBMC_addon;
  if (!XBMC->RegisterMe(hdl))
  {
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  GUI = new CHelper_libXBMC_gui;
  if (!GUI->RegisterMe(hdl))
  {
    SAFE_DELETE(GUI);
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  CODEC = new CHelper_libXBMC_codec;
  if (!CODEC->RegisterMe(hdl))
  {
    SAFE_DELETE(CODEC);
    SAFE_DELETE(GUI);
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  PVR = new CHelper_libXBMC_pvr;
  if (!PVR->RegisterMe(hdl))
  {
    SAFE_DELETE(PVR);
    SAFE_DELETE(CODEC);
    SAFE_DELETE(GUI);
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  XBMC->Log(LOG_DEBUG, "%s - Creating Tvheadend PVR-Client", __FUNCTION__);

  m_CurStatus     = ADDON_STATUS_UNKNOWN;
  g_strUserPath   = pvrprops->strUserPath;
  g_strClientPath = pvrprops->strClientPath;

  ADDON_ReadSettings();

  HTSPData = new CHTSPData;
  if (!HTSPData->Open())
  {
    SAFE_DELETE(HTSPData);
    SAFE_DELETE(CODEC);
    SAFE_DELETE(PVR);
    SAFE_DELETE(GUI);
    SAFE_DELETE(XBMC);
    m_CurStatus = ADDON_STATUS_LOST_CONNECTION;
    return m_CurStatus;
  }

  if(HTSPData->CanTranscode())
  {
    menuHook = new PVR_MENUHOOK();
    menuHook->category = PVR_MENUHOOK_ALL;
    menuHook->iHookId = 1;
    menuHook->iLocalizedStringId = 30100;
    PVR->AddMenuHook(menuHook);
  }

  m_CurStatus = ADDON_STATUS_OK;
  m_bCreated = true;
  return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus()
{
  /* check whether we're still connected */
  if (m_CurStatus == ADDON_STATUS_OK && !HTSPData->IsConnected())
    m_CurStatus = ADDON_STATUS_LOST_CONNECTION;

  return m_CurStatus;
}

void ADDON_Destroy()
{
  m_bCreated = false;
  SAFE_DELETE(HTSPData);
  SAFE_DELETE(CODEC);
  SAFE_DELETE(PVR);
  SAFE_DELETE(GUI);
  SAFE_DELETE(XBMC);
  SAFE_DELETE(menuHook);

  m_CurStatus = ADDON_STATUS_UNKNOWN;
}

bool ADDON_HasSettings()
{
  return true;
}

unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
  string str = settingName;
  if (str == "host")
  {
    string tmp_sHostname;
    XBMC->Log(LOG_INFO, "%s - Changed Setting 'host' from %s to %s", __FUNCTION__, g_strHostname.c_str(), (const char*) settingValue);
    tmp_sHostname = g_strHostname;
    g_strHostname = (const char*) settingValue;
    if (tmp_sHostname != g_strHostname)
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (str == "user")
  {
    string tmp_sUsername = g_strUsername;
    g_strUsername = (const char*) settingValue;
    if (tmp_sUsername != g_strUsername)
    {
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'user'", __FUNCTION__);
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (str == "pass")
  {
    string tmp_sPassword = g_strPassword;
    g_strPassword = (const char*) settingValue;
    if (tmp_sPassword != g_strPassword)
    {
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'pass'", __FUNCTION__);
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (str == "htsp_port")
  {
    if (g_iPortHTSP != *(int*) settingValue)
    {
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'htsp_port' from %u to %u", __FUNCTION__, g_iPortHTSP, *(int*) settingValue);
      g_iPortHTSP = *(int*) settingValue;
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (str == "http_port")
  {
    if (g_iPortHTTP != *(int*) settingValue)
    {
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'port' from %u to %u", __FUNCTION__, g_iPortHTTP, *(int*) settingValue);
      g_iPortHTTP = *(int*) settingValue;
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (str == "connect_timeout")
  {
    int iNewValue = *(int*) settingValue + 1;
    if (g_iConnectTimeout != iNewValue)
    {
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'connect_timeout' from %u to %u", __FUNCTION__, g_iConnectTimeout, iNewValue);
      g_iConnectTimeout = iNewValue;
      return ADDON_STATUS_OK;
    }
  }
  else if (str == "response_timeout")
  {
    int iNewValue = *(int*) settingValue + 1;
    if (g_iResponseTimeout != iNewValue)
    {
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'response_timeout' from %u to %u", __FUNCTION__, g_iResponseTimeout, iNewValue);
      g_iResponseTimeout = iNewValue;
      return ADDON_STATUS_OK;
    }
  }
  else if (str == "transcode")
  {
    int bNewValue = *(bool*) settingValue;
    XBMC->Log(LOG_INFO, "%s - Changed Setting 'transcode' from %u to %u", __FUNCTION__, g_bTranscode, bNewValue);
    g_bTranscode = (bNewValue == 1);
  }
  else if (str == "resolution")
  {
    int iNewValue = *(int*) settingValue + 1;
    if (g_iResolution != iNewValue)
    {
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'resolution' from %u to %u", __FUNCTION__, g_iResolution, iNewValue);
      g_iResolution = iNewValue;
      return ADDON_STATUS_OK;
    }
  }
  else if (str == "video_codec_name")
  {
    string tmp_strCodecname = g_videoCodec.Name();
    if (tmp_strCodecname != (const char*) settingValue)
    {
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'video_codec' from %s to %s", __FUNCTION__, tmp_strCodecname.c_str(), (const char*) settingValue);
      g_videoCodec = CodecDescriptor::GetCodecByName((const char*) settingValue);
      return ADDON_STATUS_OK;
    }
  }
  else if (str == "audio_codec_name")
  {
    string tmp_strCodecname = g_audioCodec.Name();
    if (tmp_strCodecname != (const char*) settingValue)
    {
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'audio_codec' from %s to %s", __FUNCTION__, tmp_strCodecname.c_str(), (const char*) settingValue);
      g_audioCodec = CodecDescriptor::GetCodecByName((const char*) settingValue);
      return ADDON_STATUS_OK;
    }
  }

  return ADDON_STATUS_OK;
}

void ADDON_Stop()
{
}

void ADDON_FreeSettings()
{
}

void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

const char* GetPVRAPIVersion(void)
{
  static const char *strApiVersion = XBMC_PVR_API_VERSION;
  return strApiVersion;
}

const char* GetMininumPVRAPIVersion(void)
{
  static const char *strMinApiVersion = XBMC_PVR_MIN_API_VERSION;
  return strMinApiVersion;
}

const char* GetGUIAPIVersion(void)
{
  static const char *strGuiApiVersion = XBMC_GUI_API_VERSION;
  return strGuiApiVersion;
}

const char* GetMininumGUIAPIVersion(void)
{
  static const char *strMinGuiApiVersion = XBMC_GUI_MIN_API_VERSION;
  return strMinGuiApiVersion;
}

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
  pCapabilities->bSupportsEPG              = true;
  pCapabilities->bSupportsTV               = true;
  pCapabilities->bSupportsRadio            = true;
  pCapabilities->bSupportsRecordings       = true;
  pCapabilities->bSupportsTimers           = true;
  pCapabilities->bSupportsChannelGroups    = true;
  pCapabilities->bHandlesInputStream       = true;
  pCapabilities->bHandlesDemuxing          = true;
  pCapabilities->bSupportsRecordingFolders = true;
  return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{
  static const char *strBackendName = HTSPData ? HTSPData->GetServerName() : "unknown";
  return strBackendName;
}

const char *GetBackendVersion(void)
{
  static CStdString strBackendVersion;
  if (HTSPData)
    strBackendVersion.Format("%s (Protocol: %i)", HTSPData->GetVersion(), HTSPData->GetProtocol());
  return strBackendVersion.c_str();
}

const char *GetConnectionString(void)
{
  static CStdString strConnectionString;
  if (HTSPData)
    strConnectionString.Format("%s:%i%s", g_strHostname.c_str(), g_iPortHTSP, HTSPData->IsConnected() ? "" : " (Not connected!)");
  else
    strConnectionString.Format("%s:%i (addon error!)", g_strHostname.c_str(), g_iPortHTSP);
  return strConnectionString.c_str();
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  if (HTSPData->GetDriveSpace(iTotal, iUsed))
    return PVR_ERROR_NO_ERROR;

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->GetEpg(handle, channel, iStart, iEnd);
}

int GetChannelsAmount(void)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return 0;

  return HTSPData->GetNumChannels();
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->GetChannels(handle, bRadio);
}

int GetRecordingsAmount(void)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return 0;

  return HTSPData->GetNumRecordings();
}

PVR_ERROR GetRecordings(ADDON_HANDLE handle)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->GetRecordings(handle);
}

PVR_ERROR DeleteRecording(const PVR_RECORDING &recording)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->DeleteRecording(recording);
}

PVR_ERROR RenameRecording(const PVR_RECORDING &recording)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->RenameRecording(recording, recording.strTitle);
}

int GetTimersAmount(void)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return 0;

  return HTSPData->GetNumTimers();
}

PVR_ERROR GetTimers(ADDON_HANDLE handle)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->GetTimers(handle);
}

PVR_ERROR AddTimer(const PVR_TIMER &timer)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->AddTimer(timer);
}

PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->DeleteTimer(timer, bForceDelete);
}

PVR_ERROR UpdateTimer(const PVR_TIMER &timer)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return timer.state == PVR_TIMER_STATE_CANCELLED || timer.state == PVR_TIMER_STATE_ABORTED ?
      HTSPData->DeleteTimer(timer, false) :
      HTSPData->UpdateTimer(timer);
}

bool OpenLiveStream(const PVR_CHANNEL &channel)
{
  return HTSPData ?
      HTSPData->OpenLiveStream(channel) :
      false;
}

void CloseLiveStream(void)
{
  if (HTSPData)
    HTSPData->CloseLiveStream();
}

int GetCurrentClientChannel(void)
{
  return HTSPData ?
      HTSPData->GetCurrentClientChannel():
      -1;
}

bool SwitchChannel(const PVR_CHANNEL &channel)
{
  return HTSPData ?
      HTSPData->SwitchChannel(channel) :
      false;
}

PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties)
{
  return HTSPData ?
      HTSPData->GetStreamProperties(pProperties) :
      PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
  return HTSPData ?
      HTSPData->SignalStatus(signalStatus) :
      PVR_ERROR_SERVER_ERROR;
}

void DemuxAbort(void)
{
  if (HTSPData)
    HTSPData->DemuxAbort();
}

DemuxPacket* DemuxRead(void)
{
  return HTSPData ?
      HTSPData->DemuxRead() :
      NULL;
}

int GetChannelGroupsAmount(void)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->GetNumChannelGroups();
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  /* tvheadend doesn't support separated groups, so we only support TV groups */
  if (bRadio)
    return PVR_ERROR_NO_ERROR;

  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->GetChannelGroups(handle);
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  /* tvheadend doesn't support separated groups, so we only support TV groups */
  if (group.bIsRadio)
    return PVR_ERROR_NO_ERROR;

  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return HTSPData->GetChannelGroupMembers(handle, group);
}

PVR_ERROR CallMenuHook(const PVR_MENUHOOK &menuhook, const PVR_MENUHOOK_DATA &item)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  if (!HTSPData->CanTranscode())
    return PVR_ERROR_REJECTED;

  CodecVector v = HTSPData->GetTranscodingCodecs();
  CGUIDialogTranscode settings(v);

  settings.DoModal();

  return PVR_ERROR_NO_ERROR;
}

bool OpenRecordedStream(const PVR_RECORDING &recording)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return false;
  return HTSPData->OpenRecordedStream(recording);
}

void CloseRecordedStream(void)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return;
  HTSPData->CloseRecordedStream();
}

int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return -1;
  return HTSPData->ReadRecordedStream(pBuffer, iBufferSize);
}

long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return -1;
  return HTSPData->SeekRecordedStream(iPosition, iWhence);
}

long long PositionRecordedStream(void)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return -1;
  return HTSPData->PositionRecordedStream();
}

long long LengthRecordedStream(void)
{
  if (!HTSPData || !HTSPData->IsConnected())
    return -1;
  return HTSPData->LengthRecordedStream();
}

bool CanPauseStream(void)
{
  if (HTSPData)
    return HTSPData->CanTimeshift();
  return false;
}

bool CanSeekStream(void)
{
  if (HTSPData)
    return HTSPData->CanSeekLiveStream();
  return false;
}

bool SeekTime(int time,bool backward,double *startpts)
{
  return HTSPData ?
      HTSPData->SeekTime(time, backward, startpts) :
      false;
}

void SetSpeed(int speed)
{
  if(HTSPData)
    HTSPData->SetSpeed(speed);
}

void DemuxFlush(void)
{
  if (HTSPData)
    HTSPData->DemuxFlush();
}

/** UNUSED API FUNCTIONS */
PVR_ERROR DialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR MoveChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DialogChannelSettings(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DialogAddChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
void DemuxReset(void) {}
int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekLiveStream(long long iPosition, int iWhence /* = SEEK_SET */) { return -1; }
long long PositionLiveStream(void) { return -1; }
long long LengthLiveStream(void) { return -1; }
const char * GetLiveStreamURL(const PVR_CHANNEL &channel) { return ""; }
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording) { return -1; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; };
unsigned int GetChannelSwitchDelay(void) { return 0; }
void PauseStream(bool bPaused) {}
time_t GetPlayingTime() { return 0; }
time_t GetBufferTimeStart() { return 0; }
time_t GetBufferTimeEnd() { return 0; }
}
