/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://www.xbmc.org
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

#include "Subscription.h"
#include "../Tvheadend.h"

using namespace PLATFORM;
using namespace tvheadend;

Subscription::Subscription(CHTSPConnection &conn) :
  m_channelId(0),
  m_speed(1000),
  m_weight(SUBSCRIPTION_WEIGHT_NORMAL),
  m_id(0),
  m_state(SUBSCRIPTION_STOPPED),
  m_prevState(SUBSCRIPTION_STOPPED),
  m_startTime(time(NULL)),
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

eSubsriptionState Subscription::GetPrevState() const
{
  CLockObject lock(m_mutex);
  return m_prevState;
}

void Subscription::SetState(eSubsriptionState state)
{
  CLockObject lock(m_mutex);
  if (state == m_state)
    return;

  m_prevState = m_state;
  m_state = state;
}

std::string Subscription::GetProfile() const
{
  CLockObject lock(m_mutex);
  return m_profile;
}

void Subscription::SetProfile(const std::string &profile)
{
  CLockObject lock(m_mutex);
  m_profile = profile;
}

time_t Subscription::GetStartTime() const
{
  CLockObject lock(m_mutex);
  return m_startTime;
}

void Subscription::SetStartTime(time_t time)
{
  CLockObject lock(m_mutex);
  m_startTime = time;
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
    SetStartTime(time(NULL)); // now
  }

  /* Build message */
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_s32(m, "channelId",       GetChannelId());
  htsmsg_add_u32(m, "subscriptionId",  GetId());
  htsmsg_add_u32(m, "weight",          GetWeight());
  htsmsg_add_u32(m, "timeshiftPeriod", static_cast<uint32_t>(~0));
  htsmsg_add_u32(m, "normts",          1);
  htsmsg_add_u32(m, "queueDepth",      PACKET_QUEUE_DEPTH);

  /* Use the specified profile if it has been set */
  if (!GetProfile().empty())
    htsmsg_add_str(m, "profile", GetProfile().c_str());

  tvhdebug("demux subscribe to %d",    GetChannelId());

  /* Send and Wait for response */
  if (restart)
    m = m_conn.SendAndWait0("subscribe", m);
  else
    m = m_conn.SendAndWait("subscribe", m);
  if (m == NULL)
    return;

  htsmsg_destroy(m);

  SetState(SUBSCRIPTION_STARTING);

  /* As this might be a pre- posttuning subscription */
  UpdateStateFromWeight();

  tvhdebug("demux successfully subscribed to channel id %d, subscription id %d", GetChannelId(), GetId());
}

void Subscription::SendUnsubscribe(void)
{
  /* Build message */
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId",   GetId());
  tvhdebug("demux unsubscribe from %d", GetChannelId());

  /* Mark subscription as inactive immediately in case this command fails */
  SetState(SUBSCRIPTION_STOPPED);

  /* Send and Wait */
  if ((m = m_conn.SendAndWait("unsubscribe", m)) == NULL)
    return;

  htsmsg_destroy(m);
  tvhdebug("demux successfully unsubscribed from channel id %d, subscription id %d", GetChannelId(), GetId());
}

bool Subscription::SendSeek(int time)
{
  /* Build message */
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", GetId());
  htsmsg_add_s64(m, "time",           static_cast<int64_t>(time * 1000LL));
  htsmsg_add_u32(m, "absolute",       1);
  tvhdebug("demux send seek %d",      time);

  /* Send and Wait */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("subscriptionSeek", m);
  }
  if (m)
  {
    htsmsg_destroy(m);
    return true;
  }

  return false;
}

void Subscription::SendSpeed(int32_t speed, bool restart)
{
  /* We don't want to change the speed when restarting a subscription */
  if (!restart)
    SetSpeed(speed);

  /* Build message */
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", GetId());
  htsmsg_add_s32(m, "speed",          GetSpeed() / 10); // Kodi uses values an order of magnitude larger than tvheadend
  tvhdebug("demux send speed %d",     GetSpeed() / 10);

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
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", GetId());
  htsmsg_add_s32(m, "weight",         GetWeight());
  tvhdebug("demux send weight %u",    GetWeight());

  /* Send and Wait */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("subscriptionChangeWeight", m);
  }
  if (m)
    htsmsg_destroy(m);

  /* As this might be a pre- posttuning subscription now */
  UpdateStateFromWeight();
}

void Subscription::UpdateStateFromWeight()
{
  if (GetWeight() == static_cast<uint32_t>(SUBSCRIPTION_WEIGHT_PRETUNING) ||
      GetWeight() == static_cast<uint32_t>(SUBSCRIPTION_WEIGHT_POSTTUNING))
  {
    SetState(SUBSCRIPTION_PREPOSTTUNING);
  }
  else if (GetState() == SUBSCRIPTION_PREPOSTTUNING)
  {
    /* Switched from pre- posttuning to active, initiate a virtual start */
    SetState(SUBSCRIPTION_STARTING);
    SetStartTime(time(NULL));
  }
}

void Subscription::ParseSubscriptionStatus ( htsmsg_t *m )
{
  /* Not for preTuning and postTuning subscriptions */
  if (GetState() == SUBSCRIPTION_PREPOSTTUNING)
    return;

  const char *status = htsmsg_get_str(m, "status");

  if (status != NULL)
    tvhinfo("Bad subscription status: %s", status);

  /* 'subscriptionErrors' was added in htsp v20, use 'status' for older backends */
  if (m_conn.GetProtocol() >= 20)
  {
    const char *error = htsmsg_get_str(m, "subscriptionError");

    /* This field is absent when everything is fine */
    if (error != NULL)
    {
      if (!strcmp("badSignal", error))
        SetState(SUBSCRIPTION_NOSIGNAL);
      else if (!strcmp("scrambled", error))
        SetState(SUBSCRIPTION_SCRAMBLED);
      else if (!strcmp("userLimit", error))
        SetState(SUBSCRIPTION_USERLIMIT);
      else if (!strcmp("noFreeAdapter", error))
      {
        /* If streaming conflict management enabled */
        if (Settings::GetInstance().GetStreamingConflict())
          HandleConflict(); // no free adapter, AKA conflict
        else
          SetState(SUBSCRIPTION_NOFREEADAPTER);
      }
      else if (!strcmp("tuningFailed", error))
        SetState(SUBSCRIPTION_TUNINGFAILED);
      else if (!strcmp("userAccess", error))
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
    if (status != NULL)
    {
      SetState(SUBSCRIPTION_UNKNOWN);

      /* Show an OSD message */
      XBMC->QueueNotification(ADDON::QUEUE_INFO, status);
    }
    else
      SetState(SUBSCRIPTION_RUNNING);
  }
}

void Subscription::ShowStateNotification(void)
{
  if (GetState() == SUBSCRIPTION_NOFREEADAPTER)
    XBMC->QueueNotification(ADDON::QUEUE_WARNING, XBMC->GetLocalizedString(30450));
  else if (GetState() == SUBSCRIPTION_SCRAMBLED)
    XBMC->QueueNotification(ADDON::QUEUE_WARNING, XBMC->GetLocalizedString(30451));
  else if (GetState() == SUBSCRIPTION_NOSIGNAL)
    XBMC->QueueNotification(ADDON::QUEUE_WARNING, XBMC->GetLocalizedString(30452));
  else if (GetState() == SUBSCRIPTION_TUNINGFAILED)
    XBMC->QueueNotification(ADDON::QUEUE_WARNING, XBMC->GetLocalizedString(30453));
  else if (GetState() == SUBSCRIPTION_USERLIMIT)
    XBMC->QueueNotification(ADDON::QUEUE_WARNING, XBMC->GetLocalizedString(30454));
  else if (GetState() == SUBSCRIPTION_NOACCESS)
    XBMC->QueueNotification(ADDON::QUEUE_WARNING, XBMC->GetLocalizedString(30455));
  else if (GetState() == SUBSCRIPTION_UNKNOWN)
    XBMC->QueueNotification(ADDON::QUEUE_WARNING, XBMC->GetLocalizedString(30456));
}

void Subscription::HandleConflict(void)
{
  if (GetState() != SUBSCRIPTION_NOFREEADAPTER_HANDLING)
    SetState(SUBSCRIPTION_NOFREEADAPTER);

  /*
   * Conflict case 1: (GetPrevState() == SUBSCRIPTION_RUNNING)
   * Subscription was running before, but the adapter got stolen by an other subscription
   * Ask user if he wants to continue watching by interrupting an other subscription (weight based)
   *
   * Conflict case 2: (GetPrevState() == SUBSCRIPTION_STARTING)
   * No free adapter found to start this channel from the beginning on
   * Ask user if he wants to start watching by interrupting an other subscription (weight based)
   * 'DIALOG_NOSTART_DELAY' is to prevent the dialog from popping up when zapping
   */

  if (GetPrevState() == SUBSCRIPTION_RUNNING ||
     (GetPrevState() == SUBSCRIPTION_STARTING && GetStartTime() + DIALOG_NOSTART_DELAY < time(NULL)))
  {
    std::thread(&Subscription::ShowConflictDialog, this).detach();
  }
}

void Subscription::ShowConflictDialog(void)
{
  tvhinfo("demux conflict dialog: open, state: %i, previous state: %i, weight: %i ,subscription id: %i", GetState(), GetPrevState(), GetWeight(), GetId());

  if (GetWeight() >= SUBSCRIPTION_WEIGHT_MAX)
    return;

  /* Save the initial subscription id */
  uint32_t initialId = GetId();

  /* Make a copy before changing the state */
  eSubsriptionState prevState = GetPrevState();

  /* Mark this conflict as handling */
  SetState(SUBSCRIPTION_NOFREEADAPTER_HANDLING);

  /*
   * Dialog Heading: TV conflict
   * All adapters are in use.
   * To keep watching TV, you can interrupt the lowest priority service.
   * This can be an active recording or an other TV client.
   *
   * Interrupt service <--> Do nothing
   */

  bool bDialogInterrrupt = GUI->Dialog_YesNo_ShowAndGetInput(
      XBMC->GetLocalizedString(30550), XBMC->GetLocalizedString(30551),
      XBMC->GetLocalizedString(prevState == SUBSCRIPTION_STARTING ? 30553 : 30552),
      XBMC->GetLocalizedString(30554), XBMC->GetLocalizedString(30555), XBMC->GetLocalizedString(30556));

  if (bDialogInterrrupt)
  {
    while (GetWeight() < SUBSCRIPTION_WEIGHT_MAX)
    {
      /* Channel changed or conflict solved */
      if (GetId() != initialId || GetState() != SUBSCRIPTION_NOFREEADAPTER_HANDLING)
        break;

      /* Gradually increase weight between min and max */
      if (GetWeight() < SUBSCRIPTION_WEIGHT_MIN)
        SendWeight(SUBSCRIPTION_WEIGHT_MIN);
      else if (GetWeight() + SUBSCRIPTION_WEIGHT_STEPSIZE > SUBSCRIPTION_WEIGHT_MAX)
        SendWeight(SUBSCRIPTION_WEIGHT_MAX);
      else
        SendWeight(GetWeight() + SUBSCRIPTION_WEIGHT_STEPSIZE);

      XBMC->QueueNotification(ADDON::QUEUE_INFO, XBMC->GetLocalizedString(30557),
          (GetWeight()-SUBSCRIPTION_WEIGHT_MIN)/((SUBSCRIPTION_WEIGHT_MAX-SUBSCRIPTION_WEIGHT_MIN)/100)); // Interrupting service... %i%

      sleep(1);
    }
  }

  /* Handling done and still not running, set state back to original */
  if (GetState() == SUBSCRIPTION_NOFREEADAPTER_HANDLING)
    SetState(SUBSCRIPTION_NOFREEADAPTER);
}

uint32_t Subscription::GetNextId()
{
  static uint32_t id = 0;
  return ++id;
}
