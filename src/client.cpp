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
#include "kodi/xbmc_pvr_dll.h"
#include "kodi/libKODI_guilib.h"
#include "platform/util/util.h"
#include "Tvheadend.h"
#include "tvheadend/Settings.h"

using namespace std;
using namespace ADDON;
using namespace PLATFORM;

/* **************************************************************************
 * Global variables
 * *************************************************************************/

/*
 * Client state
 */
ADDON_STATUS m_CurStatus = ADDON_STATUS_UNKNOWN;
bool         m_bAlertHtspVersionMismatch = true;

/*
 * Global configuration
 */
CMutex     g_mutex;
string     g_strHostname         = DEFAULT_HOST;
int        g_iPortHTSP           = DEFAULT_HTSP_PORT;
int        g_iPortHTTP           = DEFAULT_HTTP_PORT;
int        g_iConnectTimeout     = DEFAULT_CONNECT_TIMEOUT;
int        g_iResponseTimeout    = DEFAULT_RESPONSE_TIMEOUT;
string     g_strUsername         = "";
string     g_strPassword         = "";
bool       g_bTraceDebug         = false;
bool       g_bAsyncEpg           = false;
bool       g_bPreTunerEnabled    = false;
int        g_iTotalTuners        = DEFAULT_TOTAL_TUNERS;
int        g_iPreTunerCloseDelay = DEFAULT_PRETUNER_CLOSEDELAY;
int        g_iAutorecApproxTime  = DEFAULT_APPROX_TIME;
int        g_iAutorecMaxDiff     = DEFAULT_AUTOREC_MAXDIFF;

/*
 * Global state
 */
CHelper_libXBMC_addon *XBMC      = NULL;
CHelper_libXBMC_pvr   *PVR       = NULL;
CHelper_libXBMC_codec *CODEC     = NULL;
PVR_MENUHOOK          *menuHook  = NULL;
CTvheadend            *tvh       = NULL;

/* **************************************************************************
 * ADDON setup
 * *************************************************************************/

extern "C" {

void ADDON_ReadSettings(void)
{
#define UPDATE_INT(var, key, def)\
  if (!XBMC->GetSetting(key, &var))\
    var = def;

#define UPDATE_STR(var, key, tmp, def)\
  if (XBMC->GetSetting(key, tmp))\
    var = tmp;\
  else\
    var = def;

  char buffer[1024];

  /* Connection */
  UPDATE_STR(g_strHostname, "host", buffer, DEFAULT_HOST);
  UPDATE_STR(g_strUsername, "user", buffer, "");
  UPDATE_STR(g_strPassword, "pass", buffer, "");
  UPDATE_INT(g_iPortHTSP,   "htsp_port", DEFAULT_HTSP_PORT);
  UPDATE_INT(g_iPortHTTP,   "http_port", DEFAULT_HTTP_PORT);
  UPDATE_INT(g_iConnectTimeout,  "connect_timeout",  DEFAULT_CONNECT_TIMEOUT);
  UPDATE_INT(g_iResponseTimeout, "response_timeout", DEFAULT_RESPONSE_TIMEOUT);
  UPDATE_INT(g_iTotalTuners,  "total_tuners",  DEFAULT_TOTAL_TUNERS);
  UPDATE_INT(g_iPreTunerCloseDelay, "pretuner_closedelay",  DEFAULT_PRETUNER_CLOSEDELAY);
  UPDATE_INT(g_bPreTunerEnabled, "pretuner_enabled", false);

  /* Data Transfer */
  UPDATE_INT(g_bAsyncEpg,   "epg_async", false);

  /* Debug */
  UPDATE_INT(g_bTraceDebug, "trace_debug", false);

  /* Auto recordings */
  UPDATE_INT(g_iAutorecApproxTime, "autorec_approxtime", DEFAULT_APPROX_TIME);
  UPDATE_INT(g_iAutorecMaxDiff, "autorec_maxdiff", DEFAULT_AUTOREC_MAXDIFF);

  /* TODO: Transcoding */

#undef UPDATE_INT
#undef UPDATE_STR
}

ADDON_STATUS ADDON_Create(void* hdl, void* _unused(props))
{
  if (!hdl)
    return m_CurStatus;
  
  /* Instantiate helpers */
  XBMC  = new CHelper_libXBMC_addon;
  CODEC = new CHelper_libXBMC_codec;
  PVR   = new CHelper_libXBMC_pvr;
  
  if (!XBMC->RegisterMe(hdl) ||
      !CODEC->RegisterMe(hdl) || !PVR->RegisterMe(hdl))
  {
    SAFE_DELETE(PVR);
    SAFE_DELETE(CODEC);
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  tvhinfo("starting PVR client");

  ADDON_ReadSettings();
  
  /* Create a settings object that can be used without locks */
  tvheadend::Settings &settings = tvheadend::Settings::GetInstance();
  settings.strHostname = g_strHostname;
  settings.iPortHTSP = g_iPortHTSP;
  settings.iPortHTTP = g_iPortHTTP;
  settings.strUsername = g_strUsername;
  settings.strPassword = g_strPassword;
  settings.bTraceDebug = g_bTraceDebug;
  settings.bAsyncEpg = g_bAsyncEpg;

  /* Timeouts are defined in seconds but we expect them to be in milliseconds. */
  settings.iConnectTimeout = (g_iConnectTimeout * 1000);
  settings.iResponseTimeout = (g_iResponseTimeout * 1000);

  if (g_bPreTunerEnabled)
  {
    settings.iTotalTuners = g_iTotalTuners;
    settings.iPreTuneCloseDelay = g_iPreTunerCloseDelay;
  }
  else
  {
    /* When we don't want to use predictive tuning */
    settings.iTotalTuners = 1;
    settings.iPreTuneCloseDelay = 0;
  }

  settings.bAutorecApproxTime = (g_iAutorecApproxTime > 0);
  settings.iAutorecMaxDiff = g_iAutorecMaxDiff;

  tvh = new CTvheadend();
  tvh->Start();

  /* Wait for connection */
  if (!tvh->WaitForConnection())
  {
    if (m_bAlertHtspVersionMismatch &&
        (tvh->GetProtocol() > 0) && // 0 => tvh server down
        (tvh->GetProtocol() < HTSP_MIN_SERVER_VERSION))
    {
      m_bAlertHtspVersionMismatch = false; // alert max once during addon lifetime

      /* client/server API version mismatch */
      XBMC->QueueNotification(
        QUEUE_ERROR,
        XBMC->GetLocalizedString(30300), tvh->GetProtocol(), HTSP_MIN_SERVER_VERSION);
    }

    SAFE_DELETE(tvh);
    SAFE_DELETE(PVR);
    SAFE_DELETE(CODEC);
    SAFE_DELETE(XBMC);

    return ADDON_STATUS_LOST_CONNECTION;
  }

  m_CurStatus     = ADDON_STATUS_OK;
  return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus()
{
  CLockObject lock(g_mutex);

  // Check that we're still connected
  if (m_CurStatus == ADDON_STATUS_OK && !tvh->IsConnected())
    m_CurStatus = ADDON_STATUS_LOST_CONNECTION;

  return m_CurStatus;
}

void ADDON_Destroy()
{
  CLockObject lock(g_mutex);
  SAFE_DELETE(tvh);
  SAFE_DELETE(PVR);
  SAFE_DELETE(CODEC);
  SAFE_DELETE(XBMC);
  SAFE_DELETE(menuHook);
  m_CurStatus = ADDON_STATUS_UNKNOWN;
}

bool ADDON_HasSettings()
{
  return true;
}

unsigned int ADDON_GetSettings
  (ADDON_StructSetting ***_unused(sSet))
{
  return 0;
}

ADDON_STATUS ADDON_SetSetting
  (const char *settingName, const void *settingValue)
{
#define UPDATE_STR(key, var)\
  if (!strcmp(settingName, key))\
  {\
    if (strcmp(var.c_str(), (const char*)settingValue) != 0)\
    {\
      tvhdebug("update %s from '%s' to '%s'",\
               settingName, var.c_str(), settingValue);\
      return ADDON_STATUS_NEED_RESTART;\
    }\
    return ADDON_STATUS_OK;\
  }

#define UPDATE_INT(key, type, var)\
  if (!strcmp(settingName, key))\
  {\
    if (var != *(type*)settingValue)\
    {\
      tvhdebug("update %s from '%d' to '%d'",\
               settingName, var, (int)*(type*)settingValue);\
      return ADDON_STATUS_NEED_RESTART;\
    }\
    return ADDON_STATUS_OK;\
  }

  /* Connection */
  UPDATE_STR("host", g_strHostname);
  UPDATE_STR("user", g_strUsername);
  UPDATE_STR("pass", g_strPassword);
  UPDATE_INT("htsp_port", int, g_iPortHTSP);
  UPDATE_INT("http_port", int, g_iPortHTTP);
  UPDATE_INT("connect_timeout", int, g_iConnectTimeout);
  UPDATE_INT("response_timeout", int, g_iResponseTimeout);
  
  /* Data transfer */
  UPDATE_INT("epg_async", bool, g_bAsyncEpg);

  /* Debug */
  UPDATE_INT("trace_debug", bool, g_bTraceDebug);

  /* Predictive Tuning */
  UPDATE_INT("total_tuners", int, g_iTotalTuners);
  UPDATE_INT("pretuner_closedelay", int, g_iPreTunerCloseDelay);
  UPDATE_INT("pretuner_enabled", bool, g_bPreTunerEnabled);

  /* Auto Recordings */
  UPDATE_INT("autorec_approxtime", int, g_iAutorecApproxTime);
  UPDATE_INT("autorec_maxdiff", int, g_iAutorecMaxDiff);

  return ADDON_STATUS_OK;

#undef UPDATE_INT
#undef UPDATE_STR
}

void ADDON_Stop()
{
}

void ADDON_FreeSettings()
{
}

void ADDON_Announce
  (const char *flag, const char *sender, const char *message, 
   const void *_unused(data))
{
  tvhdebug("Announce(flag=%s, sender=%s, message=%s)", flag, sender, message);

  /* XBMC/System */
  if (!strcmp(sender, "xbmc") && !strcmp(flag, "System"))
  {
    if (!strcmp("OnSleep", message))
      tvh->OnSleep();
    else if (!strcmp("OnWake", message))
      tvh->OnWake();
  }
}

/* **************************************************************************
 * Versioning
 * *************************************************************************/

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

/* **************************************************************************
 * Capabilities / Info
 * *************************************************************************/

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
  pCapabilities->bSupportsEPG                = true;
  pCapabilities->bSupportsTV                 = true;
  pCapabilities->bSupportsRadio              = true;
  pCapabilities->bSupportsRecordings         = true;
  pCapabilities->bSupportsRecordingsUndelete = false;
  pCapabilities->bSupportsTimers             = true;
  pCapabilities->bSupportsChannelGroups      = true;
  pCapabilities->bHandlesInputStream         = true;
  pCapabilities->bHandlesDemuxing            = true;
  pCapabilities->bSupportsRecordingEdl       = true;

  return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{
  static std::string serverName = tvh->GetServerName();
  return serverName.c_str();
}

const char *GetBackendVersion(void)
{
  static std::string serverVersion = tvh->GetServerVersion();
  return serverVersion.c_str();
}

const char *GetConnectionString(void)
{
  static std::string serverString = tvh->GetServerString();
  return serverString.c_str();
}

const char *GetBackendHostname(void)
{
  return g_strHostname.c_str();
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
  return tvh->GetDriveSpace(iTotal, iUsed);
}

/* **************************************************************************
 * GUI hooks
 * *************************************************************************/

PVR_ERROR CallMenuHook
  (const PVR_MENUHOOK &_unused(menuhook),
   const PVR_MENUHOOK_DATA &_unused(data))
{
  return PVR_ERROR_NO_ERROR;
}

/* **************************************************************************
 * Demuxer
 * *************************************************************************/

bool CanPauseStream(void)
{
  return tvh->HasCapability("timeshift");
}

bool CanSeekStream(void)
{
  return tvh->HasCapability("timeshift");
}

bool IsTimeshifting(void)
{
  return tvh->DemuxGetTimeshiftTime() != 0;
}

bool OpenLiveStream(const PVR_CHANNEL &channel)
{
  return tvh->DemuxOpen(channel);
}

void CloseLiveStream(void)
{
  tvh->DemuxClose();
}

bool SeekTime(int time,bool backward,double *startpts)
{
  return tvh->DemuxSeek(time, backward, startpts);
}

void SetSpeed(int speed)
{
  tvh->DemuxSpeed(speed);
}

int GetCurrentClientChannel(void)
{
  return -1; // XBMC doesn't even use this
}

bool SwitchChannel(const PVR_CHANNEL &channel)
{
  return tvh->DemuxOpen(channel);
}

PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties)
{
  return tvh->DemuxCurrentStreams(pProperties);
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
  return tvh->DemuxCurrentSignal(signalStatus);
}

DemuxPacket* DemuxRead(void)
{
  return tvh->DemuxRead();
}

void DemuxAbort(void)
{
  tvh->DemuxAbort();
}

void DemuxReset(void)
{
}

void DemuxFlush(void)
{
  tvh->DemuxFlush();
}

/* **************************************************************************
 * Channel Management
 * *************************************************************************/

int GetChannelGroupsAmount(void)
{
  return tvh->GetTagCount();
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  return tvh->GetTags(handle, bRadio);
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  return tvh->GetTagMembers(handle, group);
}

int GetChannelsAmount(void)
{
  return tvh->GetChannelCount();
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  return tvh->GetChannels(handle, bRadio);
}

/* **************************************************************************
 * EPG
 * *************************************************************************/

PVR_ERROR GetEPGForChannel
  (ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  return tvh->GetEpg(handle, channel, iStart, iEnd);
}

/* **************************************************************************
 * Recording Management
 * *************************************************************************/

int GetRecordingsAmount(bool deleted)
{
  return tvh->GetRecordingCount();
}

PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted)
{
  return tvh->GetRecordings(handle);
}

PVR_ERROR GetRecordingEdl
  (const PVR_RECORDING &rec, PVR_EDL_ENTRY edl[], int *num)
{
  return tvh->GetRecordingEdl(rec, edl, num);
}

PVR_ERROR DeleteRecording(const PVR_RECORDING &rec)
{
  return tvh->DeleteRecording(rec);
}

PVR_ERROR RenameRecording(const PVR_RECORDING &rec)
{
  return tvh->RenameRecording(rec);
}

PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR DeleteAllRecordingsFromTrash()
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size)
{
  return tvh->GetTimerTypes(types, size);
}

int GetTimersAmount(void)
{
  return tvh->GetTimerCount();
}

PVR_ERROR GetTimers(ADDON_HANDLE handle)
{
  return tvh->GetTimers(handle);
}

PVR_ERROR AddTimer(const PVR_TIMER &timer)
{
  return tvh->AddTimer(timer);
}

PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete)
{
  return tvh->DeleteTimer(timer, bForceDelete);
}

PVR_ERROR UpdateTimer(const PVR_TIMER &timer)
{
  return tvh->UpdateTimer(timer);
}
 
/* **************************************************************************
 * Recording VFS
 * *************************************************************************/

bool OpenRecordedStream(const PVR_RECORDING &recording)
{
  return tvh->VfsOpen(recording);
}

void CloseRecordedStream(void)
{
  tvh->VfsClose();
}

int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  return static_cast<int>(tvh->VfsRead(pBuffer, iBufferSize));
}

long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */)
{
  return tvh->VfsSeek(iPosition, iWhence);
}

long long PositionRecordedStream(void)
{
  return tvh->VfsTell();
}

long long LengthRecordedStream(void)
{
  return tvh->VfsSize();
}

/* **************************************************************************
 * Unused Functions
 * *************************************************************************/

unsigned int GetChannelSwitchDelay(void) { return 0; }

/* Recording History */
PVR_ERROR SetRecordingPlayCount
  (const PVR_RECORDING &_unused(recording), int _unused(count))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR SetRecordingLastPlayedPosition  
  (const PVR_RECORDING &_unused(recording), int _unused(lastplayedposition))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
int GetRecordingLastPlayedPosition(const PVR_RECORDING &_unused(recording))
{
  return -1;
}

/* Channel Management */
PVR_ERROR OpenDialogChannelScan(void)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR DeleteChannel(const PVR_CHANNEL &_unused(channel))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR RenameChannel(const PVR_CHANNEL &_unused(channel))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR MoveChannel(const PVR_CHANNEL &_unused(channel))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL &_unused(channel))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL &_unused(channel))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

/* Timeshift?? - not sure if we can use these? */
void PauseStream(bool _unused(bPaused))
{
}

static time_t ConvertMusecsToTime(int64_t musecs)
{
  // tvheadend reports microseconds for timeshifting values,
  // Kodi expects it to be an absolute UNIX timestamp
  return time(NULL) - static_cast<time_t>(static_cast<double>(musecs / 1000000));
}

time_t GetPlayingTime()
{
  return ConvertMusecsToTime(tvh->DemuxGetTimeshiftTime());
}
time_t GetBufferTimeStart()
{
  return ConvertMusecsToTime(tvh->DemuxGetTimeshiftBufferStart());
}
time_t GetBufferTimeEnd()
{
  return ConvertMusecsToTime(tvh->DemuxGetTimeshiftBufferEnd());
}

/* Live stream (VFS interface - not relevant) */
int ReadLiveStream
  (unsigned char *_unused(pBuffer), unsigned int _unused(iBufferSize))
{
  return 0;
}
long long SeekLiveStream
  (long long _unused(iPosition), int _unused(iWhence))
{
  return -1;
}
long long PositionLiveStream(void)
{
  return -1;
}
long long LengthLiveStream(void)
{
  return -1;
}
const char * GetLiveStreamURL(const PVR_CHANNEL &_unused(channel))
{
  return "";
}

const char* GetGUIAPIVersion(void)
{
  return KODI_GUILIB_API_VERSION;
}

const char* GetMininumGUIAPIVersion(void)
{
  return KODI_GUILIB_MIN_API_VERSION;
}

} /* extern "C" */
