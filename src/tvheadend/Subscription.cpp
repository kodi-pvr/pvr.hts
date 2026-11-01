/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Subscription.h"

#include "HTSPConnection.h"
#include "utilities/Logger.h"

#include "kodi/General.h"

#include <atomic>
#include <cstring>

using namespace tvheadend;
using namespace tvheadend::utilities;

Subscription::Subscription(HTSPConnection& conn)
  : m_id(0),
    m_channelId(0),
    m_weight(SUBSCRIPTION_WEIGHT_NORMAL),
    m_speed(1000),
    m_state(SUBSCRIPTION_STOPPED),
    m_conn(conn)
{
}

bool Subscription::IsActive() const
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return (GetState() != SUBSCRIPTION_STOPPED);
}

uint32_t Subscription::GetId() const
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return m_id;
}

uint32_t Subscription::GetChannelId() const
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return m_channelId;
}

uint32_t Subscription::GetWeight() const
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return m_weight;
}

void Subscription::SetWeight(uint32_t weight)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  m_weight = weight;
}

int32_t Subscription::GetSpeed() const
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return m_speed;
}

void Subscription::SetSpeed(int32_t speed)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  m_speed = speed;
}

eSubsriptionState Subscription::GetState() const
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return m_state;
}

void Subscription::SetState(eSubsriptionState state)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  m_state = state;
}

std::string Subscription::GetProfile() const
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return m_profile;
}

void Subscription::SetProfile(const std::string& profile)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  m_profile = profile;
}

void Subscription::SendRecordingSubscribe(std::unique_lock<std::recursive_mutex>& lock,
                                          uint32_t dvrId)
{
  OpenSubscription(lock, 0, dvrId, SUBSCRIPTION_WEIGHT_NORMAL);
}

void Subscription::SendSubscribe(std::unique_lock<std::recursive_mutex>& lock,
                                 uint32_t channelId,
                                 uint32_t weight)
{
  OpenSubscription(lock, channelId, std::nullopt, weight);
}

void Subscription::OpenSubscription(std::unique_lock<std::recursive_mutex>& lock,
                                    uint32_t channelId,
                                    std::optional<uint32_t> dvrId,
                                    uint32_t weight)
{
  {
    std::lock_guard<std::recursive_mutex> guard(m_mutex);
    m_dvrId = dvrId;
    m_channelId = channelId;
    m_weight = weight;
    m_id = GetNextId();
    m_speed = 1000;
  }
  SendSubscribe0(lock, false);
}

void Subscription::Restart(std::unique_lock<std::recursive_mutex>& lock)
{
  SendSubscribe0(lock, true);
}

void Subscription::SendSubscribe0(std::unique_lock<std::recursive_mutex>& lock, bool restart)
{
  std::optional<uint32_t> dvrId;
  {
    std::lock_guard<std::recursive_mutex> guard(m_mutex);
    dvrId = m_dvrId;
  }
  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  const uint32_t sourceId = dvrId.value_or(GetChannelId());
  htsmsg_add_s32(m, dvrId ? "dvrId" : "channelId", sourceId);
  htsmsg_add_u32(m, "subscriptionId", GetId());
  htsmsg_add_u32(m, "weight", GetWeight());
  if (!dvrId)
    htsmsg_add_u32(m, "timeshiftPeriod", static_cast<uint32_t>(~0));
  htsmsg_add_u32(m, "normts", 1);
  htsmsg_add_u32(m, "queueDepth", dvrId ? RECORDING_QUEUE_DEPTH : PACKET_QUEUE_DEPTH);

  /* Use the specified profile if it has been set */
  if (!GetProfile().empty())
    htsmsg_add_str(m, "profile", GetProfile().c_str());

  Logger::Log(LogLevel::LEVEL_DEBUG, "demux subscribe to %s %u", dvrId ? "recording" : "channel",
              sourceId);

  /* The receive thread can see START immediately after the subscribe reply. */
  SetState(SUBSCRIPTION_STARTING);
  /* Send and Wait for response */
  if (restart)
    m = m_conn.SendAndWait0(lock, "subscribe", m);
  else
    m = m_conn.SendAndWait(lock, "subscribe", m);
  if (!m)
  {
    SetState(SUBSCRIPTION_STOPPED);
    return;
  }
  htsmsg_destroy(m);
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux subscribed to %s %u, subscription id %u",
              dvrId ? "recording" : "channel", sourceId, GetId());
}

void Subscription::SetStopped()
{
  SetState(SUBSCRIPTION_STOPPED);
}

void Subscription::SendUnsubscribe(std::unique_lock<std::recursive_mutex>& lock)
{
  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", GetId());
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux unsubscribe subscription %u", GetId());

  /* Mark subscription as inactive immediately in case this command fails */
  SetStopped();

  /* Send and Wait */
  m = m_conn.SendAndWait(lock, "unsubscribe", m);
  if (!m)
    return;

  htsmsg_destroy(m);
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux successfully unsubscribed subscription %u", GetId());
}

bool Subscription::SendSeek(std::unique_lock<std::recursive_mutex>& lock, double time)
{
  return SendSeek0(lock, time, false);
}

bool Subscription::RestoreSeek(std::unique_lock<std::recursive_mutex>& lock, double time)
{
  return SendSeek0(lock, time, true);
}

bool Subscription::SendSeek0(std::unique_lock<std::recursive_mutex>& lock,
                             double time,
                             bool restart)
{
  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", GetId());
  htsmsg_add_s64(m, "time", static_cast<int64_t>(time * 1000LL));
  htsmsg_add_u32(m, "absolute", 1);
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux send seek %.0f", time);

  /* Send and Wait */
  m = restart ? m_conn.SendAndWait0(lock, "subscriptionSeek", m)
              : m_conn.SendAndWait(lock, "subscriptionSeek", m);
  if (!m)
    return false;

  htsmsg_destroy(m);
  return true;
}

htsmsg_t* Subscription::SpeedMessage() const
{
  htsmsg_t* message = htsmsg_create_map();
  htsmsg_add_u32(message, "subscriptionId", GetId());
  htsmsg_add_s32(message, "speed",
                 GetSpeed() / 10); // Kodi uses values an order of magnitude larger than tvheadend
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux send speed %d", GetSpeed() / 10);
  return message;
}

void Subscription::SendSpeed(std::unique_lock<std::recursive_mutex>& lock, int32_t speed)
{
  SetSpeed(speed);
  htsmsg_t* reply = m_conn.SendAndWait(lock, "subscriptionSpeed", SpeedMessage());
  if (reply)
    htsmsg_destroy(reply);
}

void Subscription::SendSpeedAsync(std::unique_lock<std::recursive_mutex>& lock, int32_t speed)
{
  SetSpeed(speed);
  m_conn.SendAsync(lock, "subscriptionSpeed", SpeedMessage());
}

void Subscription::RestoreSpeed(std::unique_lock<std::recursive_mutex>& lock)
{
  htsmsg_t* reply = m_conn.SendAndWait0(lock, "subscriptionSpeed", SpeedMessage());
  if (reply)
    htsmsg_destroy(reply);
}

void Subscription::SendWeight(std::unique_lock<std::recursive_mutex>& lock, uint32_t weight)
{
  SetWeight(weight);

  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", GetId());
  htsmsg_add_s32(m, "weight", GetWeight());
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux send weight %u", GetWeight());

  /* Send and Wait */
  m = m_conn.SendAndWait(lock, "subscriptionChangeWeight", m);
  if (m)
    htsmsg_destroy(m);
}

void Subscription::ParseSubscriptionStatus(htsmsg_t* m)
{
  /* Not for preTuning and postTuning subscriptions */
  if (GetWeight() == static_cast<uint32_t>(SUBSCRIPTION_WEIGHT_PRETUNING) ||
      GetWeight() == static_cast<uint32_t>(SUBSCRIPTION_WEIGHT_POSTTUNING))
  {
    SetState(SUBSCRIPTION_PREPOSTTUNING);
    return;
  }

  const char* status = htsmsg_get_str(m, "status");
  const char* error = htsmsg_get_str(m, "subscriptionError");

  /* This field is absent when everything is fine */
  if (error)
  {
    if (!std::strcmp("badSignal", error))
      SetState(SUBSCRIPTION_NOSIGNAL);
    else if (!std::strcmp("scrambled", error))
      SetState(SUBSCRIPTION_SCRAMBLED);
    else if (!std::strcmp("userLimit", error))
      SetState(SUBSCRIPTION_USERLIMIT);
    else if (!std::strcmp("noFreeAdapter", error))
      SetState(SUBSCRIPTION_NOFREEADAPTER);
    else if (!std::strcmp("tuningFailed", error))
      SetState(SUBSCRIPTION_TUNINGFAILED);
    else if (!std::strcmp("userAccess", error))
      SetState(SUBSCRIPTION_NOACCESS);
    else
      SetState(SUBSCRIPTION_UNKNOWN);

    /* Show an OSD message */
    ShowStateNotification();
  }
  else
    SetState(SUBSCRIPTION_RUNNING);
}

void Subscription::ShowStateNotification()
{
  if (GetState() == SUBSCRIPTION_NOFREEADAPTER)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::addon::GetLocalizedString(30450));
  else if (GetState() == SUBSCRIPTION_SCRAMBLED)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::addon::GetLocalizedString(30451));
  else if (GetState() == SUBSCRIPTION_NOSIGNAL)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::addon::GetLocalizedString(30452));
  else if (GetState() == SUBSCRIPTION_TUNINGFAILED)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::addon::GetLocalizedString(30453));
  else if (GetState() == SUBSCRIPTION_USERLIMIT)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::addon::GetLocalizedString(30454));
  else if (GetState() == SUBSCRIPTION_NOACCESS)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::addon::GetLocalizedString(30455));
  else if (GetState() == SUBSCRIPTION_UNKNOWN)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::addon::GetLocalizedString(30456));
}

uint32_t Subscription::GetNextId()
{
  static std::atomic<uint32_t> id{0};
  return ++id;
}
