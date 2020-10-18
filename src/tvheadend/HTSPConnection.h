/*
 *  Copyright (C) 2017-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>

extern "C"
{
#include "libhts/htsmsg.h"
}

#include "kodi/addon-instance/pvr/General.h"
#include "kodi/tools/Thread.h"

namespace tvheadend
{

class HTSPRegister;
class HTSPResponse;
class IHTSPConnectionListener;

namespace utilities
{
class TCPSocket;
}

typedef std::map<uint32_t, HTSPResponse*> HTSPResponseList;

/*
 * HTSP Connection
 */
class HTSPConnection : public kodi::tools::CThread
{
public:
  HTSPConnection(IHTSPConnectionListener& connListener);
  ~HTSPConnection() override;

  void Start();
  void Stop();
  void Disconnect();

  bool SendMessage0(const char* method, htsmsg_t* m);
  htsmsg_t* SendAndWait0(std::unique_lock<std::recursive_mutex>& lock,
                         const char* method,
                         htsmsg_t* m,
                         int iResponseTimeout = -1);
  htsmsg_t* SendAndWait(std::unique_lock<std::recursive_mutex>& lock,
                        const char* method,
                        htsmsg_t* m,
                        int iResponseTimeout = -1);

  int GetProtocol() const;

  std::string GetWebURL(const char* fmt, ...) const;

  std::string GetServerName() const;
  std::string GetServerVersion() const;
  std::string GetServerString() const;

  bool HasCapability(const std::string& capability) const;

  std::recursive_mutex& Mutex() { return m_mutex; }

  void OnSleep();
  void OnWake();

private:
  // CThread iplementation
  void Process() override;

  void Register();
  bool ReadMessage();
  bool SendHello(std::unique_lock<std::recursive_mutex>& lock);
  bool SendAuth(std::unique_lock<std::recursive_mutex>& lock,
                const std::string& u,
                const std::string& p);

  void SetState(PVR_CONNECTION_STATE state);
  bool WaitForConnection(std::unique_lock<std::recursive_mutex>& lock);

  /*
   * HTSP Connection registration thread
   */
  class HTSPRegister : public kodi::tools::CThread
  {
  public:
    HTSPRegister(HTSPConnection* conn) : m_conn(conn) {}

    ~HTSPRegister() override { StopThread(true); }

  private:
    // CThread implementation
    void Process() override { m_conn->Register(); }

    HTSPConnection* m_conn;
  };

  IHTSPConnectionListener& m_connListener;
  tvheadend::utilities::TCPSocket* m_socket = nullptr;
  mutable std::recursive_mutex m_mutex;
  HTSPRegister* m_regThread;
  std::condition_variable_any m_regCond;
  bool m_ready;
  uint32_t m_seq;
  std::string m_serverName;
  std::string m_serverVersion;
  int m_htspVersion;
  std::string m_webRoot;
  void* m_challenge;
  int m_challengeLen;

  HTSPResponseList m_messages;
  std::vector<std::string> m_capabilities;

  bool m_suspended;
  PVR_CONNECTION_STATE m_state;
};

} // namespace tvheadend
