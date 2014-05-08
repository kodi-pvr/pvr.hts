#pragma once

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
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301  USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "client.h"
#include "HTSPTypes.h"
#include "platform/threads/threads.h"

extern "C" {
#include "libhts/net.h"
#include "libhts/htsmsg.h"
}

namespace PLATFORM
{
  class CTcpConnection;
}

class CHTSPConnectionCallback
{
public:
  CHTSPConnectionCallback(void) {}
  virtual ~CHTSPConnectionCallback(void) {}

  virtual bool OnConnectionDropped(void) { return true; }
  virtual bool OnConnectionRestored(void) { return true; }
  virtual bool ProcessMessage(htsmsg* msg) = 0;
};

struct SMessage
{
  PLATFORM::CEvent* event;
  htsmsg_t*         msg;
};
typedef std::map<uint32_t, SMessage> SMessages;

class CHTSResult
{
public:
  CHTSResult(void);
  ~CHTSResult(void);

  std::string GetErrorMessage(void);
  bool        IsError(void);
  bool        NoAccess(void);

  htsmsg*   message;      /*!< the response message */
  PVR_ERROR status;       /*!< the return code */

private:
  std::string m_strError; /*!< the error response */
};

class CHTSPConnection;

class CHTSPReconnect : public PLATFORM::CThread
{
public:
  CHTSPReconnect(CHTSPConnection* connection) :
    m_connection(connection) {}
  virtual ~CHTSPReconnect(void) {}

  void* Process(void);

private:
  CHTSPConnection* m_connection;
};

class CHTSPConnection : private PLATFORM::CThread
{
  friend class CHTSPReconnect;

public:
  CHTSPConnection(CHTSPConnectionCallback* callback);
  ~CHTSPConnection();

  bool        Connect(void);
  void        Close();
  bool        IsConnected(void);
  bool        CheckConnection(uint32_t iTimeout);
  int         GetProtocol() const { return m_iProtocol; }
  const char *GetServerName() const { return m_strServerName.c_str(); }
  const char *GetVersion() const { return m_strVersion.c_str(); }
  const char *GetWebroot() const { return m_strWebroot.c_str(); }
  const       CStdString GetWebURL(const char *fmt, ...) const;

  bool        TransmitMessage(htsmsg_t* m);
  void        ReadResult(htsmsg_t *m, CHTSResult &result, const char* strAction = NULL);
  bool        ReadSuccess(htsmsg_t* m, const char* strAction = NULL);

  bool        CanTimeshift(void);
  bool        CanSeekLiveStream(void);

  bool        CanTranscode(void) const  { return m_bTranscodingSupport; }
  void        SetReadTimeout(int iTimeout);
  void        TriggerReconnect(void);

private:
  bool       OpenSocket(void);
  void*      Process(void);
  bool       SendGreeting(void);
  bool       Auth(void);
  htsmsg_t*  ReadMessage(int iInitialTimeout = 1000, int iDatapacketTimeout = 1000);

  PLATFORM::CMutex          m_mutex;
  PLATFORM::CTcpConnection* m_socket;
  void*                     m_challenge;
  int                       m_iChallengeLength;
  int                       m_iProtocol;
  int                       m_iPortnumber;
  int                       m_iConnectTimeout;
  std::string               m_strServerName;
  std::string               m_strUsername;
  std::string               m_strPassword;
  std::string               m_strVersion;
  std::string               m_strHostname;
  std::string               m_strWebroot;
  bool                      m_bIsConnected;
  bool                      m_bTimeshiftSupport;
  bool                      m_bTimeshiftSeekSupport;
  bool                      m_bTranscodingSupport;

  std::deque<htsmsg_t*>     m_queue;
  const unsigned int        m_iQueueSize;
  CHTSPConnectionCallback*  m_callback;
  PLATFORM::CCondition<bool> m_connectEvent;
  SMessages                  m_messageQueue;
  PLATFORM::CTimeout        m_readTimeout;
  int                       m_iReadTimeout;
  CHTSPReconnect*           m_reconnect;
};
