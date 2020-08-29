/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Subscription.h"

#include "HTSPConnection.h"
#include "utilities/Logger.h"

#include "kodi/General.h"

#include <cstring>

using namespace P8PLATFORM;
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
  CLockObject lock(m_mutex);
  return (GetState() != SUBSCRIPTION_STOPPED);
}

uint32_t Subscription::GetId() const
{
  CLockObject lock(m_mutex);
  return m_id;
}

void Subscription::SetId(uint32_t id)
{
  CLockObject lock(m_mutex);
  m_id = id;
}

uint32_t Subscription::GetChannelId() const
{
  CLockObject lock(m_mutex);
  return m_channelId;
}

void Subscription::SetChannelId(uint32_t id)
{
  CLockObject lock(m_mutex);
  m_channelId = id;
}

uint32_t Subscription::GetWeight() const
{
  CLockObject lock(m_mutex);
  return m_weight;
}

void Subscription::SetWeight(uint32_t weight)
{
  CLockObject lock(m_mutex);
  m_weight = weight;
}

int32_t Subscription::GetSpeed() const
{
  CLockObject lock(m_mutex);
  return m_speed;
}

void Subscription::SetSpeed(int32_t speed)
{
  CLockObject lock(m_mutex);
  m_speed = speed;
}

eSubsriptionState Subscription::GetState() const
{
  CLockObject lock(m_mutex);
  return m_state;
}

void Subscription::SetState(eSubsriptionState state)
{
  CLockObject lock(m_mutex);
  m_state = state;
}

std::string Subscription::GetProfile() const
{
  CLockObject lock(m_mutex);
  return m_profile;
}

void Subscription::SetProfile(const std::string& profile)
{
  CLockObject lock(m_mutex);
  m_profile = profile;
}

void Subscription::SendSubscribe(uint32_t channelId, uint32_t weight, bool restart)
{
  /* We don't want to change anything when restarting a subscription */
  if (!restart)
  {
    SetChannelId(channelId);
    SetWeight(weight);
    SetId(GetNextId());
    SetSpeed(1000); //set back to normal
  }

  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_s32(m, "channelId", GetChannelId());
  htsmsg_add_u32(m, "subscriptionId", GetId());
  htsmsg_add_u32(m, "weight", GetWeight());
  htsmsg_add_u32(m, "timeshiftPeriod", static_cast<uint32_t>(~0));
  htsmsg_add_u32(m, "normts", 1);
  htsmsg_add_u32(m, "queueDepth", PACKET_QUEUE_DEPTH);

  /* Use the specified profile if it has been set */
  if (!GetProfile().empty())
    htsmsg_add_str(m, "profile", GetProfile().c_str());

  Logger::Log(LogLevel::LEVEL_DEBUG, "demux subscribe to %d", GetChannelId());

  /* Send and Wait for response */
  if (restart)
    m = m_conn.SendAndWait0("subscribe", m);
  else
    m = m_conn.SendAndWait("subscribe", m);
  if (!m)
    return;

  htsmsg_destroy(m);

  SetState(SUBSCRIPTION_STARTING);
  Logger::Log(LogLevel::LEVEL_DEBUG,
              "demux successfully subscribed to channel id %d, subscription id %d", GetChannelId(),
              GetId());
}

void Subscription::SendUnsubscribe()
{
  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", GetId());
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux unsubscribe from %d", GetChannelId());

  /* Mark subscription as inactive immediately in case this command fails */
  SetState(SUBSCRIPTION_STOPPED);

  /* Send and Wait */
  m = m_conn.SendAndWait("unsubscribe", m);
  if (!m)
    return;

  htsmsg_destroy(m);
  Logger::Log(LogLevel::LEVEL_DEBUG,
              "demux successfully unsubscribed from channel id %d, subscription id %d",
              GetChannelId(), GetId());
}

bool Subscription::SendSeek(double time)
{
  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", GetId());
  htsmsg_add_s64(m, "time", static_cast<int64_t>(time * 1000LL));
  htsmsg_add_u32(m, "absolute", 1);
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux send seek %d", time);

  /* Send and Wait */
  m = m_conn.SendAndWait("subscriptionSeek", m);
  if (!m)
    return false;

  htsmsg_destroy(m);
  return true;
}

void Subscription::SendSpeed(int32_t speed, bool restart)
{
  /* We don't want to change the speed when restarting a subscription */
  if (!restart)
    SetSpeed(speed);

  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", GetId());
  htsmsg_add_s32(m, "speed",
                 GetSpeed() / 10); // Kodi uses values an order of magnitude larger than tvheadend
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux send speed %d", GetSpeed() / 10);

  if (restart)
    m = m_conn.SendAndWait0("subscriptionSpeed", m);
  else
    m = m_conn.SendAndWait("subscriptionSpeed", m);

  if (m)
    htsmsg_destroy(m);
}

void Subscription::SendWeight(uint32_t weight)
{
  SetWeight(weight);

  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", GetId());
  htsmsg_add_s32(m, "weight", GetWeight());
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux send weight %u", GetWeight());

  /* Send and Wait */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("subscriptionChangeWeight", m);
  }
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

  /* 'subscriptionErrors' was added in htsp v20, use 'status' for older backends */
  if (m_conn.GetProtocol() >= 20)
  {
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
  else
  {
    /* This field is absent when everything is fine */
    if (status)
    {
      SetState(SUBSCRIPTION_UNKNOWN);

      /* Show an OSD message */
      kodi::QueueNotification(QUEUE_INFO, "", status);
    }
    else
      SetState(SUBSCRIPTION_RUNNING);
  }
}

void Subscription::ShowStateNotification()
{
  if (GetState() == SUBSCRIPTION_NOFREEADAPTER)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::GetLocalizedString(30450));
  else if (GetState() == SUBSCRIPTION_SCRAMBLED)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::GetLocalizedString(30451));
  else if (GetState() == SUBSCRIPTION_NOSIGNAL)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::GetLocalizedString(30452));
  else if (GetState() == SUBSCRIPTION_TUNINGFAILED)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::GetLocalizedString(30453));
  else if (GetState() == SUBSCRIPTION_USERLIMIT)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::GetLocalizedString(30454));
  else if (GetState() == SUBSCRIPTION_NOACCESS)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::GetLocalizedString(30455));
  else if (GetState() == SUBSCRIPTION_UNKNOWN)
    kodi::QueueNotification(QUEUE_WARNING, "", kodi::GetLocalizedString(30456));
}

uint32_t Subscription::GetNextId()
{
  static uint32_t id = 0;
  return ++id;
}
