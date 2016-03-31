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

#include "p8-platform/threads/mutex.h"
#include "p8-platform/util/StringUtils.h"
#include "p8-platform/sockets/tcp.h"

extern "C" {
#include "libhts/htsmsg_binary.h"
#include "libhts/sha1.h"
}

#include "tvheadend/utilities/Logger.h"
#include "Tvheadend.h"

using namespace std;
using namespace ADDON;
using namespace P8PLATFORM;
using namespace tvheadend;
using namespace tvheadend::utilities;

/*
 * HTSP Response objct
 */
CHTSPResponse::CHTSPResponse ()
  : m_flag(false), m_msg(NULL)
{
}

CHTSPResponse::~CHTSPResponse ()
{
  if (m_msg) htsmsg_destroy(m_msg);
  Set(NULL); // ensure signal is sent
}

void CHTSPResponse::Set ( htsmsg_t *msg )
{
  m_msg  = msg;
  m_flag = true;
  m_cond.Broadcast();
}

htsmsg_t *CHTSPResponse::Get ( CMutex &mutex, uint32_t timeout )
{
  m_cond.Wait(mutex, m_flag, timeout);
  htsmsg_t *r = m_msg;
  m_msg       = NULL;
  m_flag      = false;
  return r;
}

/*
 * Registration thread
 */
CHTSPRegister::CHTSPRegister ( CHTSPConnection *conn )
  : m_conn(conn)
{
}

CHTSPRegister::~CHTSPRegister ()
{
  StopThread(0);
}

void *CHTSPRegister::Process ( void )
{
  m_conn->Register();
  return NULL;
}

/*
 * HTSP Connection handler
 */

CHTSPConnection::CHTSPConnection ()
  : m_socket(NULL), m_regThread(this), m_ready(false), m_seq(0),
    m_serverName(""), m_serverVersion(""), m_htspVersion(0),
    m_webRoot(""), m_challenge(NULL), m_challengeLen(0), m_suspended(false),
    m_state(PVR_CONNECTION_STATE_UNKNOWN)
{
}

CHTSPConnection::~CHTSPConnection()
{
  StopThread(-1);
  Disconnect();
  StopThread(0);
}

void CHTSPConnection::Start()
{
  // Note: "connecting" must only be set one time, before the very first connection attempt, not on every reconnect.
  SetState(PVR_CONNECTION_STATE_CONNECTING);
  CreateThread();
}

void CHTSPConnection::Stop()
{
  StopThread(-1);
  Disconnect();
}

/*
 * Info
 */

std::string CHTSPConnection::GetWebURL ( const char *fmt, ... ) const
{
  va_list va;
  const Settings &settings = Settings::GetInstance();

  // Generate the authentication string (user:pass@)
  std::string auth = settings.GetUsername();
  if (!(auth.empty() || settings.GetPassword().empty()))
    auth += ":" + settings.GetPassword();
  if (!auth.empty())
    auth += "@";

  std::string url = StringUtils::Format("http://%s%s:%d", auth.c_str(), settings.GetHostname().c_str(), settings.GetPortHTTP());

  CLockObject lock(m_mutex);
  va_start(va, fmt);
  url += m_webRoot;
  url += StringUtils::FormatV(fmt, va);
  va_end(va);

  return url;
}

bool CHTSPConnection::WaitForConnection ( void )
{
  if (!m_ready)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "waiting for registration...");
    m_regCond.Wait(m_mutex, m_ready, Settings::GetInstance().GetConnectTimeout());
  }
  return m_ready;
}

int  CHTSPConnection::GetProtocol ( void ) const
{
  CLockObject lock(m_mutex);
  return m_htspVersion;
}

std::string CHTSPConnection::GetServerName ( void ) const
{
  CLockObject lock(m_mutex);
  return m_serverName;
}

std::string CHTSPConnection::GetServerVersion ( void ) const
{
  CLockObject lock(m_mutex);
  return StringUtils::Format("%s (HTSP v%d)", m_serverVersion.c_str(), m_htspVersion);
}

std::string CHTSPConnection::GetServerString ( void ) const
{
  const Settings &settings = Settings::GetInstance();

  CLockObject lock(m_mutex);
  return StringUtils::Format("%s:%d", settings.GetHostname().c_str(), settings.GetPortHTSP());
}

bool CHTSPConnection::HasCapability(const std::string &capability) const
{
  return std::find(m_capabilities.begin(), m_capabilities.end(), capability) 
         != m_capabilities.end();
}

void CHTSPConnection::OnSleep ( void )
{
  CLockObject lock(m_mutex);

  Logger::Log(LogLevel::LEVEL_TRACE, "going to sleep (OnSleep)");

  /* close connection, prevent reconnect while suspending/suspended */
  m_suspended = true;
}

void CHTSPConnection::OnWake ( void )
{
  CLockObject lock(m_mutex);

  Logger::Log(LogLevel::LEVEL_TRACE, "waking up (OnWake)");

  /* recreate connection */
  m_suspended = false;
}

void CHTSPConnection::SetState ( PVR_CONNECTION_STATE state )
{
  PVR_CONNECTION_STATE prevState(PVR_CONNECTION_STATE_UNKNOWN);
  PVR_CONNECTION_STATE newState(PVR_CONNECTION_STATE_UNKNOWN);

  {
    CLockObject lock(m_mutex);

    /* No notification if no state change or while suspended. */
    if (m_state != state && !m_suspended)
    {
      prevState = m_state;
      newState  = state;
      m_state   = newState;

      Logger::Log(LogLevel::LEVEL_DEBUG, "connection state change (%d -> %d)", prevState, newState);
    }
  }

  if (prevState != newState)
  {
    static std::string serverString;

    /* Notify connection state change (callback!) */
    serverString = GetServerString();
    PVR->ConnectionStateChange(serverString.c_str(), newState, NULL);
  }
}

/*
 * Close the connection
 */
void CHTSPConnection::Disconnect ( void )
{
  CLockObject lock(m_mutex);

  /* Close socket */
  if (m_socket) {
    m_socket->Shutdown();
    m_socket->Close();
  }

  /* Signal all waiters and erase messages */
  m_messages.clear();
}

/*
 * Read message from socket
 *
 * Return false if an error occurs and the connection should be terminated
 */
bool CHTSPConnection::ReadMessage ( void )
{
  uint8_t *buf;
  uint8_t  lb[4];
  size_t   len, cnt;
  ssize_t  r; 
  uint32_t seq;
  htsmsg_t *msg;
  const char *method;

  /* Read 4 byte len */
  len = m_socket->Read(&lb, sizeof(lb));
  if (len != sizeof(lb))
    return false;
  len = (lb[0] << 24) + (lb[1] << 16) + (lb[2] << 8) + lb[3];

  /* Read rest of packet */ 
  buf = (uint8_t*)malloc(len);
  cnt = 0;
  while (cnt < len)
  {
    r = m_socket->Read((char*)buf + cnt, len - cnt, Settings::GetInstance().GetResponseTimeout());
    if (r < 0)
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "failed to read packet (%s)",
               m_socket->GetError().c_str());
      free(buf);
      return false;
    } 
    cnt += r;
  }

  /* Deserialize */
  if (!(msg = htsmsg_binary_deserialize(buf, len, buf)))
  {
    /* Do not free buf here. Already done by htsmsg_binary_deserialize. */
    Logger::Log(LogLevel::LEVEL_ERROR, "failed to decode message");
    return false;
  }

  /* Sequence number - response */
  if (htsmsg_get_u32(msg, "seq", &seq) == 0)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "received response [%d]", seq);
    CLockObject lock(m_mutex);
    CHTSPResponseList::iterator it;
    if ((it = m_messages.find(seq)) != m_messages.end())
    {
      it->second->Set(msg);
      return true;
    }
  }

  /* Get method */
  if (!(method = htsmsg_get_str(msg, "method")))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "message without a method");
    htsmsg_destroy(msg);
    return true;
  }
  Logger::Log(LogLevel::LEVEL_TRACE, "receive message [%s]", method);

  /* Pass (if return is true, message is finished) */
  if (tvh->ProcessMessage(method, msg))
    htsmsg_destroy(msg);
  // TODO: maybe a copy should be made if it needs to be kept?

  return true;
}

/*
 * Send message to server
 */
bool CHTSPConnection::SendMessage0 ( const char *method, htsmsg_t *msg )
{
  int     e;
  void   *buf;
  size_t  len;
  ssize_t c;
  uint32_t seq;

  if (!htsmsg_get_u32(msg, "seq", &seq))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "sending message [%s : %d]", method, seq);
  }
  else
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "sending message [%s]", method);
  }
  htsmsg_add_str(msg, "method", method);

  /* Serialise */
  e = htsmsg_binary_serialize(msg, &buf, &len, -1);
  htsmsg_destroy(msg);
  if (e < 0)
    return false;

  /* Send data */
  c = m_socket->Write(buf, len);
  free(buf);
  if (c != (ssize_t)len)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "failed to write (%s)",
              m_socket->GetError().c_str());
    if (!m_suspended)
      Disconnect();
    return false;
  }

  return true;
}

/*
 * Send a message and wait for response
 */
htsmsg_t *CHTSPConnection::SendAndWait0 ( const char *method, htsmsg_t *msg, int iResponseTimeout )
{
  if (iResponseTimeout == -1)
    iResponseTimeout = Settings::GetInstance().GetResponseTimeout();
  
  uint32_t seq;

  /* Add Sequence number */
  CHTSPResponse resp;
  seq = ++m_seq;
  htsmsg_add_u32(msg, "seq", seq);
  m_messages[seq] = &resp;

  /* Send Message (bypass TX check) */
  if (!SendMessage0(method, msg))
  {
    m_messages.erase(seq);
    Logger::Log(LogLevel::LEVEL_ERROR, "failed to transmit");
    return NULL;
  }

  /* Wait for response */
  msg = resp.Get(m_mutex, iResponseTimeout);
  m_messages.erase(seq);
  if (!msg)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "Command %s failed: No response received", method);
    if (!m_suspended)
      Disconnect();
    return NULL;
  }

  /* Check result for errors and announce. */
  uint32_t noaccess;
  if (!htsmsg_get_u32(msg, "noaccess", &noaccess) && noaccess)
  {
    // access denied
    Logger::Log(LogLevel::LEVEL_ERROR, "Command %s failed: Access denied", method);
    htsmsg_destroy(msg);
    return NULL;
  }
  else
  {
    const char* strError;
    if((strError = htsmsg_get_str(msg, "error")) != NULL)
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "Command %s failed: %s", method, strError);
      htsmsg_destroy(msg);
      return NULL;
    }
  }

  return msg;
}

/*
 * Send and wait for response
 */
htsmsg_t *CHTSPConnection::SendAndWait ( const char *method, htsmsg_t *msg, int iResponseTimeout )
{
  if (iResponseTimeout == -1)
    iResponseTimeout = Settings::GetInstance().GetResponseTimeout();
  
  if (!WaitForConnection())
    return NULL;
  return SendAndWait0(method, msg, iResponseTimeout);
}

bool CHTSPConnection::SendHello ( void )
{
  /* Build message */
  htsmsg_t *msg = htsmsg_create_map();
  htsmsg_add_str(msg, "clientname", "Kodi Media Center");
  htsmsg_add_u32(msg, "htspversion", HTSP_CLIENT_VERSION);

  /* Send and Wait */
  if (!(msg = SendAndWait0("hello", msg)))
    return false;
  
  /* Process */
  const char *webroot;
  const void *chal;
  size_t chal_len;
  htsmsg_t *cap;

  /* Basic Info */
  webroot = htsmsg_get_str(msg, "webroot");
  m_serverName    = htsmsg_get_str(msg, "servername");
  m_serverVersion = htsmsg_get_str(msg, "serverversion");
  m_htspVersion   = htsmsg_get_u32_or_default(msg, "htspversion", 0);
  m_webRoot       = webroot ? webroot : "";
  Logger::Log(LogLevel::LEVEL_DEBUG, "connected to %s / %s (HTSPv%d)",
            m_serverName.c_str(), m_serverVersion.c_str(), m_htspVersion);

  /* Capabilities */
  if ((cap = htsmsg_get_list(msg, "servercapability")))
  {
    htsmsg_field_t *f;
    HTSMSG_FOREACH(f, cap)
    {
      if (f->hmf_type == HMF_STR)
        m_capabilities.push_back(f->hmf_str);
    }
  }
      
  /* Authentication */
  htsmsg_get_bin(msg, "challenge", &chal, &chal_len);
  if (chal && chal_len)
  {
    m_challenge    = malloc(chal_len);
    m_challengeLen = chal_len;
    memcpy(m_challenge, chal, chal_len);
  }

  htsmsg_destroy(msg);
 
  return true;
}

bool CHTSPConnection::SendAuth
  ( const std::string &user, const std::string &pass )
{
  htsmsg_t *msg = htsmsg_create_map();
  htsmsg_add_str(msg, "username", user.c_str());

  /* Add Password */
  // Note: we MUST send a digest or TVH will not evaluate the
  struct HTSSHA1* sha = (struct HTSSHA1*)malloc(hts_sha1_size);
  uint8_t d[20];
  hts_sha1_init(sha);
  hts_sha1_update(sha, (const uint8_t*)pass.c_str(), pass.length());
  if (m_challenge)
    hts_sha1_update(sha, (const uint8_t*)m_challenge, m_challengeLen);
  hts_sha1_final(sha, d);
  htsmsg_add_bin(msg, "digest", d, sizeof(d));
  free(sha);

  /* Send and Wait */
  msg = SendAndWait0("authenticate", msg);

  return (msg != NULL);
}

/**
 * Register the connection, hello+auth
 */
void CHTSPConnection::Register ( void )
{
  std::string user = Settings::GetInstance().GetUsername();
  std::string pass = Settings::GetInstance().GetPassword();

  {
    CLockObject lock(m_mutex);

    /* Send Greeting */
    Logger::Log(LogLevel::LEVEL_DEBUG, "sending hello");
    if (!SendHello())
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "failed to send hello");
      SetState(PVR_CONNECTION_STATE_SERVER_MISMATCH);
      goto fail;
    }

    /* Check htsp server version against client minimum htsp version */
    if (m_htspVersion < HTSP_MIN_SERVER_VERSION)
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "server htsp version (v%d) does not match minimum htsp version required by client (v%d)", m_htspVersion, HTSP_MIN_SERVER_VERSION);
      SetState(PVR_CONNECTION_STATE_VERSION_MISMATCH);
      goto fail;
    }

    /* Send Auth */
    Logger::Log(LogLevel::LEVEL_DEBUG, "sending auth");
    
    if (!SendAuth(user, pass))
    {
      SetState(PVR_CONNECTION_STATE_ACCESS_DENIED);
      goto fail;
    }

    /* Rebuild state */
    Logger::Log(LogLevel::LEVEL_DEBUG, "rebuilding state");
    if (!tvh->Connected())
      goto fail;

    Logger::Log(LogLevel::LEVEL_DEBUG, "registered");
    SetState(PVR_CONNECTION_STATE_CONNECTED);
    m_ready = true;
    m_regCond.Broadcast();
    return;
  }

fail:
  if (!m_suspended)
    Disconnect();
}

/*
 * Main thread loop for connection and rx handling
 */
void* CHTSPConnection::Process ( void )
{
  static bool log = false;
  static unsigned int retryAttempt = 0;
  const Settings &settings = Settings::GetInstance();

  while (!IsStopped())
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "new connection requested");

    std::string host = settings.GetHostname();
    int port, timeout;
    port = settings.GetPortHTSP();
    timeout = settings.GetConnectTimeout();

    /* Create socket (ensure mutex protection) */
    {
      CLockObject lock(m_mutex);
      if (m_socket)
        delete m_socket;
      tvh->Disconnected();
      m_socket = new CTcpSocket(host.c_str(), port);
      m_ready  = false;
      m_seq    = 0;
      if (m_challenge) {
        free(m_challenge);
        m_challenge = NULL;
      }
    }

    while (m_suspended)
    {
      Logger::Log(LogLevel::LEVEL_DEBUG, "suspended. Waiting for wakeup...");

      /* Wait for wakeup */
      Sleep(1000);
    }

    if (!log)
    {
      Logger::Log(LogLevel::LEVEL_DEBUG, "connecting to %s:%d", host.c_str(), port);
      log = true;
    }
    else
    {
      Logger::Log(LogLevel::LEVEL_TRACE, "connecting to %s:%d", host.c_str(), port);
    }

    /* Connect */
    Logger::Log(LogLevel::LEVEL_TRACE, "waiting for connection...");
    if (!m_socket->Open(timeout))
    {
      /* Unable to connect */
      Logger::Log(LogLevel::LEVEL_ERROR, "unable to connect to %s:%d", host.c_str(), port);
      SetState(PVR_CONNECTION_STATE_SERVER_UNREACHABLE);

      // Retry a few times with a short interval, after that with the default timeout
      if (++retryAttempt <= FAST_RECONNECT_ATTEMPTS)
        Sleep(FAST_RECONNECT_INTERVAL);
      else
        Sleep(timeout);

      continue;
    }
    Logger::Log(LogLevel::LEVEL_DEBUG, "connected");
    log = false;
    retryAttempt = 0;

    /* Start connect thread */
    m_regThread.CreateThread(true);

    /* Receive loop */
    while (!IsStopped())
    {
      if (!ReadMessage())
        break;
    }

    /* Stop connect thread (if not already) */
    m_regThread.StopThread(0);
  }

  return NULL;
}
