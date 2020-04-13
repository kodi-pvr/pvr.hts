/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "client.h"

#include "Tvheadend.h"
#include "kodi/xbmc_pvr_dll.h"
#include "p8-platform/util/util.h"
#include "tvheadend/Settings.h"
#include "tvheadend/utilities/Logger.h"

#include <cstring>

using namespace ADDON;
using namespace P8PLATFORM;
using namespace tvheadend;
using namespace tvheadend::utilities;

/* **************************************************************************
 * Global variables
 * *************************************************************************/

/*
 * Client state
 */
ADDON_STATUS m_CurStatus = ADDON_STATUS_UNKNOWN;

/*
 * Globals
 */
CMutex g_mutex;
CHelper_libXBMC_addon* XBMC = nullptr;
CHelper_libXBMC_pvr* PVR = nullptr;
PVR_MENUHOOK* menuHook = nullptr;
CTvheadend* tvh = nullptr;

/* **************************************************************************
 * ADDON setup
 * *************************************************************************/

extern "C"
{

void ADDON_ReadSettings() { Settings::GetInstance().ReadSettings(); }

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!hdl || !props)
    return m_CurStatus;

  /* Instantiate helpers */
  XBMC = new CHelper_libXBMC_addon;
  PVR = new CHelper_libXBMC_pvr;

  if (!XBMC->RegisterMe(hdl) || !PVR->RegisterMe(hdl))
  {
    SAFE_DELETE(PVR);
    SAFE_DELETE(XBMC);
    m_CurStatus = ADDON_STATUS_PERMANENT_FAILURE;
    return m_CurStatus;
  }

  /* Configure the logger */
  Logger::GetInstance().SetImplementation([](LogLevel level, const char* message) {
    /* Convert the log level */
    addon_log_t addonLevel;

    switch (level)
    {
      case LogLevel::LEVEL_FATAL:
        addonLevel = addon_log_t::LOG_FATAL;
        break;
      case LogLevel::LEVEL_ERROR:
        addonLevel = addon_log_t::LOG_ERROR;
        break;
      case LogLevel::LEVEL_WARNING:
        addonLevel = addon_log_t::LOG_WARNING;
        break;
      case LogLevel::LEVEL_INFO:
        addonLevel = addon_log_t::LOG_INFO;
        break;
      default:
        addonLevel = addon_log_t::LOG_DEBUG;
    }

    /* Don't log trace messages unless told so */
    if (level == LogLevel::LEVEL_TRACE && !Settings::GetInstance().GetTraceDebug())
      return;

    XBMC->Log(addonLevel, "%s", message);
  });

  Logger::GetInstance().SetPrefix("pvr.hts");

  Logger::Log(LogLevel::LEVEL_INFO, "starting PVR client");

  ADDON_ReadSettings();

  tvh = new CTvheadend(reinterpret_cast<AddonProperties_PVR*>(props));
  tvh->Start();

  m_CurStatus = ADDON_STATUS_OK;
  return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus()
{
  CLockObject lock(g_mutex);
  return m_CurStatus;
}

void ADDON_Destroy()
{
  CLockObject lock(g_mutex);
  tvh->Stop();
  SAFE_DELETE(tvh);
  SAFE_DELETE(PVR);
  SAFE_DELETE(XBMC);
  SAFE_DELETE(menuHook);
  m_CurStatus = ADDON_STATUS_UNKNOWN;
}

ADDON_STATUS ADDON_SetSetting(const char* settingName, const void* settingValue)
{
  CLockObject lock(g_mutex);
  m_CurStatus = Settings::GetInstance().SetSetting(settingName, settingValue);
  return m_CurStatus;
}

void OnSystemSleep() { tvh->OnSleep(); }

void OnSystemWake() { tvh->OnWake(); }

void OnPowerSavingActivated() {}

void OnPowerSavingDeactivated() {}

/* **************************************************************************
 * Capabilities / Info
 * *************************************************************************/

PVR_ERROR GetCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
  pCapabilities->bSupportsEPG = true;
  pCapabilities->bSupportsTV = true;
  pCapabilities->bSupportsRadio = true;
  pCapabilities->bSupportsRecordings = true;
  pCapabilities->bSupportsRecordingsUndelete = false;
  pCapabilities->bSupportsTimers = true;
  pCapabilities->bSupportsChannelGroups = true;
  pCapabilities->bHandlesInputStream = true;
  pCapabilities->bHandlesDemuxing = true;
  pCapabilities->bSupportsRecordingEdl = true;
  pCapabilities->bSupportsRecordingPlayCount =
      (tvh->GetProtocol() >= 27 && Settings::GetInstance().GetDvrPlayStatus());
  pCapabilities->bSupportsLastPlayedPosition =
      (tvh->GetProtocol() >= 27 && Settings::GetInstance().GetDvrPlayStatus());
  pCapabilities->bSupportsDescrambleInfo = true;
  pCapabilities->bSupportsAsyncEPGTransfer = Settings::GetInstance().GetAsyncEpg();

  if (tvh->GetProtocol() >= 28)
  {
    pCapabilities->bSupportsRecordingsRename = true;

    pCapabilities->bSupportsRecordingsLifetimeChange = true;

    /* PVR_RECORDING.iLifetime values and presentation.*/
    std::vector<std::pair<int, std::string>> lifetimeValues;
    tvh->GetLivetimeValues(lifetimeValues);

    pCapabilities->iRecordingsLifetimesSize = lifetimeValues.size();

    int i = 0;
    for (const auto& lifetimeValue : lifetimeValues)
    {
      pCapabilities->recordingsLifetimeValues[i].iValue = lifetimeValue.first;
      std::strncpy(pCapabilities->recordingsLifetimeValues[i].strDescription,
                   lifetimeValue.second.c_str(),
                   sizeof(pCapabilities->recordingsLifetimeValues[i].strDescription) - 1);
      ++i;
    }
  }

  pCapabilities->bSupportsRecordingSize = tvh->GetProtocol() >= 35;

  return PVR_ERROR_NO_ERROR;
}

const char* GetBackendName()
{
  static std::string serverName;

  serverName = tvh->GetServerName();
  return serverName.c_str();
}

const char* GetBackendVersion()
{
  static std::string serverVersion;

  serverVersion = tvh->GetServerVersion();
  return serverVersion.c_str();
}

const char* GetConnectionString()
{
  static std::string serverString;

  serverString = tvh->GetServerString();
  return serverString.c_str();
}

const char* GetBackendHostname() { return Settings::GetInstance().GetConstCharHostname(); }

PVR_ERROR GetDriveSpace(long long* iTotal, long long* iUsed)
{
  return tvh->GetDriveSpace(iTotal, iUsed);
}

/* **************************************************************************
 * GUI hooks
 * *************************************************************************/

PVR_ERROR CallMenuHook(const PVR_MENUHOOK&, const PVR_MENUHOOK_DATA&)
{
  return PVR_ERROR_NO_ERROR;
}

/* **************************************************************************
 * Demuxer
 * *************************************************************************/

PVR_ERROR GetStreamReadChunkSize(int* chunksize)
{
  if (!chunksize)
    return PVR_ERROR_INVALID_PARAMETERS;

  *chunksize = Settings::GetInstance().GetStreamReadChunkSize() * 1024;
  return PVR_ERROR_NO_ERROR;
}

bool CanPauseStream() { return tvh->HasCapability("timeshift"); }

bool CanSeekStream() { return tvh->HasCapability("timeshift"); }

PVR_ERROR GetStreamTimes(PVR_STREAM_TIMES* times)
{
  if (!times)
    return PVR_ERROR_INVALID_PARAMETERS;

  return tvh->GetStreamTimes(times);
}

bool IsTimeshifting() { return tvh->DemuxIsTimeShifting(); }

bool IsRealTimeStream()
{
  if (tvh->m_playingRecording)
    return tvh->VfsIsRealTimeStream();
  else
    return tvh->DemuxIsRealTimeStream();
}

bool OpenLiveStream(const PVR_CHANNEL& channel) { return tvh->DemuxOpen(channel); }

void CloseLiveStream() { tvh->DemuxClose(); }

bool SeekTime(double time, bool backward, double* startpts)
{
  return tvh->DemuxSeek(time, backward, startpts);
}

void SetSpeed(int speed) { tvh->DemuxSpeed(speed); }

void PauseStream(bool paused)
{
  if (tvh->m_playingRecording)
    tvh->VfsPauseStream(paused);
}

PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties)
{
  return tvh->DemuxCurrentStreams(pProperties);
}

void FillBuffer(bool mode) { tvh->DemuxFillBuffer(mode); }

PVR_ERROR GetSignalStatus(int channelUid, PVR_SIGNAL_STATUS* signalStatus)
{
  return tvh->DemuxCurrentSignal(signalStatus);
}

PVR_ERROR GetDescrambleInfo(int channelUid, PVR_DESCRAMBLE_INFO* descrambleInfo)
{
  if (!descrambleInfo)
    return PVR_ERROR_INVALID_PARAMETERS;

  return tvh->DemuxCurrentDescramble(descrambleInfo);
}

DemuxPacket* DemuxRead() { return tvh->DemuxRead(); }

void DemuxAbort() { tvh->DemuxAbort(); }

void DemuxReset() {}

void DemuxFlush() { tvh->DemuxFlush(); }

/* **************************************************************************
 * Channel Management
 * *************************************************************************/

int GetChannelGroupsAmount() { return tvh->GetTagCount(); }

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  return tvh->GetTags(handle, bRadio);
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP& group)
{
  return tvh->GetTagMembers(handle, group);
}

int GetChannelsAmount() { return tvh->GetChannelCount(); }

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  return tvh->GetChannels(handle, bRadio);
}

/* **************************************************************************
 * EPG
 * *************************************************************************/

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, int iChannelUid, time_t iStart, time_t iEnd)
{
  return tvh->GetEPGForChannel(handle, iChannelUid, iStart, iEnd);
}

PVR_ERROR SetEPGTimeFrame(int iDays) { return tvh->SetEPGTimeFrame(iDays); }

/* **************************************************************************
 * Recording Management
 * *************************************************************************/

int GetRecordingsAmount(bool deleted) { return tvh->GetRecordingCount(); }

PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted) { return tvh->GetRecordings(handle); }

PVR_ERROR GetRecordingEdl(const PVR_RECORDING& rec, PVR_EDL_ENTRY edl[], int* num)
{
  return tvh->GetRecordingEdl(rec, edl, num);
}

PVR_ERROR DeleteRecording(const PVR_RECORDING& rec) { return tvh->DeleteRecording(rec); }

PVR_ERROR RenameRecording(const PVR_RECORDING& rec) { return tvh->RenameRecording(rec); }

PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }

PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }

PVR_ERROR SetRecordingLifetime(const PVR_RECORDING* recording)
{
  if (!recording)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "recording must not be nullptr");
    return PVR_ERROR_INVALID_PARAMETERS;
  }
  return tvh->SetLifetime(*recording);
}

PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING& recording, int count)
{
  return tvh->SetPlayCount(recording, count);
}

PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING& recording, int lastplayedposition)
{
  return tvh->SetPlayPosition(recording, lastplayedposition);
}

int GetRecordingLastPlayedPosition(const PVR_RECORDING& recording)
{
  return tvh->GetPlayPosition(recording);
}

PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int* size)
{
  return tvh->GetTimerTypes(types, size);
}

int GetTimersAmount() { return tvh->GetTimerCount(); }

PVR_ERROR GetTimers(ADDON_HANDLE handle) { return tvh->GetTimers(handle); }

PVR_ERROR AddTimer(const PVR_TIMER& timer) { return tvh->AddTimer(timer); }

PVR_ERROR DeleteTimer(const PVR_TIMER& timer, bool bForceDelete)
{
  return tvh->DeleteTimer(timer, bForceDelete);
}

PVR_ERROR UpdateTimer(const PVR_TIMER& timer) { return tvh->UpdateTimer(timer); }

/* **************************************************************************
 * Recording VFS
 * *************************************************************************/

bool OpenRecordedStream(const PVR_RECORDING& recording) { return tvh->VfsOpen(recording); }

void CloseRecordedStream() { tvh->VfsClose(); }

int ReadRecordedStream(unsigned char* pBuffer, unsigned int iBufferSize)
{
  return static_cast<int>(tvh->VfsRead(pBuffer, iBufferSize));
}

long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */)
{
  return tvh->VfsSeek(iPosition, iWhence);
}

long long LengthRecordedStream() { return tvh->VfsSize(); }

/* **************************************************************************
 * Unused Functions
 * *************************************************************************/

PVR_ERROR OpenDialogChannelScan() { return PVR_ERROR_NOT_IMPLEMENTED; }

PVR_ERROR DeleteChannel(const PVR_CHANNEL&) { return PVR_ERROR_NOT_IMPLEMENTED; }

PVR_ERROR RenameChannel(const PVR_CHANNEL&) { return PVR_ERROR_NOT_IMPLEMENTED; }

PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL&) { return PVR_ERROR_NOT_IMPLEMENTED; }

PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL&) { return PVR_ERROR_NOT_IMPLEMENTED; }

int ReadLiveStream(unsigned char*, unsigned int) { return 0; }

long long SeekLiveStream(long long, int) { return -1; }

long long LengthLiveStream() { return -1; }

PVR_ERROR GetChannelStreamProperties(const PVR_CHANNEL*, PVR_NAMED_VALUE*, unsigned int*)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR GetRecordingStreamProperties(const PVR_RECORDING*, PVR_NAMED_VALUE*, unsigned int*)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR GetRecordingSize(const PVR_RECORDING* recording, int64_t* sizeInBytes) { return PVR_ERROR_NOT_IMPLEMENTED; }

PVR_ERROR IsEPGTagRecordable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }

PVR_ERROR IsEPGTagPlayable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }

PVR_ERROR GetEPGTagStreamProperties(const EPG_TAG*, PVR_NAMED_VALUE*, unsigned int*)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR GetEPGTagEdl(const EPG_TAG* epgTag, PVR_EDL_ENTRY edl[], int* size)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

} /* extern "C" */
