/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <mutex>
#include <optional>
#include <string>

extern "C"
{
#include "libhts/htsmsg.h"
}

namespace tvheadend
{

class HTSPConnection;

/* streaming uses a weight of 100 by default on the tvh side  */
/* lowest configurable streaming weight in tvh is 50          */
/* predictive tuning should be lower to avoid conflicts       */
/* weight 0 means that tvh will use the weight of it's config */
enum eSubscriptionWeight
{
  SUBSCRIPTION_WEIGHT_NORMAL = 100,
  SUBSCRIPTION_WEIGHT_PRETUNING = 40,
  SUBSCRIPTION_WEIGHT_POSTTUNING = 30,
  SUBSCRIPTION_WEIGHT_SERVERCONF = 0,
};

enum eSubsriptionState
{
  SUBSCRIPTION_STOPPED = 0, /* subscription is stopped or not started yet */
  SUBSCRIPTION_STARTING = 1, /* subscription is starting */
  SUBSCRIPTION_RUNNING = 2, /* subscription is running normal */
  SUBSCRIPTION_NOFREEADAPTER = 3, /* subscription has no free adapter to use */
  SUBSCRIPTION_SCRAMBLED = 4, /* subscription is not running because the channel is scrambled */
  SUBSCRIPTION_NOSIGNAL = 5, /* subscription is not running because of a weak/no input signal */
  SUBSCRIPTION_TUNINGFAILED = 6, /* subscription could not be started because of a tuning error */
  SUBSCRIPTION_USERLIMIT = 7, /* userlimit is reached, so we could not start this subscription */
  SUBSCRIPTION_NOACCESS = 8, /* we have no rights to watch this channel */
  SUBSCRIPTION_UNKNOWN =
      9, /* subscription state is unknown, also used for pretuning and posttuning subscriptions */
  SUBSCRIPTION_PREPOSTTUNING =
      10, /* used for pre and posttuning subscriptions (we do not care what the actual state is) */
};

static const int PACKET_QUEUE_DEPTH = 10000000;
static const int RECORDING_QUEUE_DEPTH = 500000;

class Subscription
{
public:
  Subscription(HTSPConnection& conn);

  bool IsActive() const;
  uint32_t GetId() const;
  uint32_t GetChannelId() const;
  uint32_t GetWeight() const;
  int32_t GetSpeed() const;
  eSubsriptionState GetState() const;
  std::string GetProfile() const;

  /**
   * Subscribe to a channel on the backend
   * @param channelId the channel to subscribe to
   * @param weight the desired subscription weight
   */
  void SendSubscribe(std::unique_lock<std::recursive_mutex>& lock,
                     uint32_t channelId,
                     uint32_t weight);

  /** Subscribe to a recording using its DVR identifier. */
  void SendRecordingSubscribe(std::unique_lock<std::recursive_mutex>& lock, uint32_t dvrId);

  /** Restore the current source while registering a reconnected transport. */
  void Restart(std::unique_lock<std::recursive_mutex>& lock);

  /**
   * Unsubscribe from a channel or recording on the backend
   */
  void SendUnsubscribe(std::unique_lock<std::recursive_mutex>& lock);

  /** Mark inactive when the transport is unavailable, without sending. */
  void SetStopped();

  /**
   * Send a seek to the backend
   * @param time timestamp to seek to
   * @return false if the command failed, true otherwise
   */
  bool SendSeek(std::unique_lock<std::recursive_mutex>& lock, double time);

  /** Restore the seek position while registering a reconnected transport. */
  bool RestoreSeek(std::unique_lock<std::recursive_mutex>& lock, double time);

  /**
   * Change the subscription speed on the backend
   * @param speed the desired speed of the subscription
   */
  void SendSpeed(std::unique_lock<std::recursive_mutex>& lock, int32_t speed);

  /** Send speed without awaiting a response from the media receiver. */
  void SendSpeedAsync(std::unique_lock<std::recursive_mutex>& lock, int32_t speed);

  /** Restore the stored speed while registering a reconnected transport. */
  void RestoreSpeed(std::unique_lock<std::recursive_mutex>& lock);

  /**
   * Change the subscription weight on the backend
   * @param weight the desired subscription weight
   */
  void SendWeight(std::unique_lock<std::recursive_mutex>& lock, uint32_t weight);

  /**
   * Parse the subscription status out of the incoming htsp data
   * @param m message containing the status field
   */
  void ParseSubscriptionStatus(htsmsg_t* m);

  /**
   * Use the specified profile for all new subscriptions
   * @param profile the profile
   */
  void SetProfile(const std::string& profile);

private:
  void OpenSubscription(std::unique_lock<std::recursive_mutex>& lock,
                        uint32_t channelId,
                        std::optional<uint32_t> dvrId,
                        uint32_t weight);
  void SendSubscribe0(std::unique_lock<std::recursive_mutex>& lock, bool restart);
  bool SendSeek0(std::unique_lock<std::recursive_mutex>& lock, double time, bool restart);
  htsmsg_t* SpeedMessage() const;
  void SetWeight(uint32_t weight);
  void SetSpeed(int32_t speed);
  void SetState(eSubsriptionState state);

  /**
   * Show a notification to the user depending on the subscription state
   */
  void ShowStateNotification();

  /**
   * Get the next unique subscription Id
   */
  static uint32_t GetNextId();

  uint32_t m_id;
  uint32_t m_channelId;
  std::optional<uint32_t> m_dvrId;
  uint32_t m_weight;
  int32_t m_speed;
  eSubsriptionState m_state;
  std::string m_profile;
  HTSPConnection& m_conn;

  mutable std::recursive_mutex m_mutex;
};

} // namespace tvheadend
