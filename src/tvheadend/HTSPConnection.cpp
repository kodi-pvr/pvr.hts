/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "HTSPConnection.h"

extern "C"
{
#include "libhts/htsmsg_binary.h"
#include "libhts/sha1.h"
}

#include "IHTSPConnectionListener.h"
#include "Settings.h"
#include "utilities/Logger.h"

#include "kodi/Network.h"
#include "kodi/addon-instance/PVR.h"
#include "p8-platform/os.h"
#include "p8-platform/util/StringUtils.h"

using namespace P8PLATFORM;
using namespace tvheadend;
using namespace tvheadend::utilities;

#define FAST_RECONNECT_ATTEMPTS (5)
#define FAST_RECONNECT_INTERVAL (500) // ms
#define SLOW_RECONNECT_INTERVAL (5000) // ms

#define HTSP_MIN_SERVER_VERSION (19) // Server must support at least this htsp version
#define HTSP_CLIENT_VERSION \
  (34) // Client uses HTSP features up to this version. If the respective \
      // addon feature requires htsp features introduced after \
      // HTSP_MIN_SERVER_VERSION this feature will only be available if the \
      // actual server HTSP version matches (runtime htsp version check).

namespace tvheadend
{

/*
 * HTSP Response handler
 */
class HTSPResponse
{
public:
  HTSPResponse() : m_flag(false), m_msg(nullptr) {}

  ~HTSPResponse()
  {
    if (m_msg)
      htsmsg_destroy(m_msg);

    Set(nullptr); // ensure signal is sent
  }

  htsmsg_t* Get(P8PLATFORM::CMutex& mutex, uint32_t timeout)
  {
    m_cond.Wait(mutex, m_flag, timeout);
    htsmsg_t* r = m_msg;
    m_msg = nullptr;
    m_flag = false;
    return r;
  }

  void Set(htsmsg_t* msg)
  {
    m_msg = msg;
    m_flag = true;
    m_cond.Broadcast();
  }

private:
  P8PLATFORM::CCondition<volatile bool> m_cond;
  bool m_flag;
  htsmsg_t* m_msg;
};

} // namespace tvheadend

/*
 * HTSP Connection handler
 */

HTSPConnection::HTSPConnection(IHTSPConnectionListener& connListener)
  : m_connListener(connListener),
    m_socket(nullptr),
    m_regThread(new HTSPRegister(this)),
    m_ready(false),
    m_seq(0),
    m_serverName(""),
    m_serverVersion(""),
    m_htspVersion(0),
    m_webRoot(""),
    m_challenge(nullptr),
    m_challengeLen(0),
    m_suspended(false),
    m_state(PVR_CONNECTION_STATE_UNKNOWN)
{
}

HTSPConnection::~HTSPConnection()
{
  StopThread(-1);
  Disconnect();
  StopThread(0);
  delete m_regThread;
}

void HTSPConnection::Start()
{
  // Note: "connecting" must only be set one time, before the very first connection attempt, not on every reconnect.
  SetState(PVR_CONNECTION_STATE_CONNECTING);
  CreateThread();
}

void HTSPConnection::Stop()
{
  StopThread(-1);
  Disconnect();
}

/*
 * Info
 */

std::string HTSPConnection::GetWebURL(const char* fmt, ...) const
{
  const Settings& settings = Settings::GetInstance();

  // Generate the authentication string (user:pass@)
  std::string auth = settings.GetUsername();
  if (!(auth.empty() || settings.GetPassword().empty()))
    auth += ":" + settings.GetPassword();
  if (!auth.empty())
    auth += "@";

  const char* proto = settings.GetUseHTTPS() ? "https" : "http";
  std::string url = StringUtils::Format("%s://%s%s:%d", proto, auth.c_str(),
                                        settings.GetHostname().c_str(), settings.GetPortHTTP());

  va_list va;

  CLockObject lock(m_mutex);
  va_start(va, fmt);
  url += m_webRoot;
  url += StringUtils::FormatV(fmt, va);
  va_end(va);

  return url;
}

bool HTSPConnection::WaitForConnection()
{
  if (!m_ready)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "waiting for registration...");
    m_regCond.Wait(m_mutex, m_ready, Settings::GetInstance().GetConnectTimeout());
  }
  return m_ready;
}

int HTSPConnection::GetProtocol() const
{
  CLockObject lock(m_mutex);
  return m_htspVersion;
}

std::string HTSPConnection::GetServerName() const
{
  CLockObject lock(m_mutex);
  return m_serverName;
}

std::string HTSPConnection::GetServerVersion() const
{
  CLockObject lock(m_mutex);
  return StringUtils::Format("%s (HTSP v%d)", m_serverVersion.c_str(), m_htspVersion);
}

std::string HTSPConnection::GetServerString() const
{
  const Settings& settings = Settings::GetInstance();

  CLockObject lock(m_mutex);
  return StringUtils::Format("%s:%d", settings.GetHostname().c_str(), settings.GetPortHTSP());
}

bool HTSPConnection::HasCapability(const std::string& capability) const
{
  return std::find(m_capabilities.begin(), m_capabilities.end(), capability) !=
         m_capabilities.end();
}

void HTSPConnection::OnSleep()
{
  CLockObject lock(m_mutex);

  Logger::Log(LogLevel::LEVEL_TRACE, "going to sleep (OnSleep)");

  /* close connection, prevent reconnect while suspending/suspended */
  Disconnect();
  m_suspended = true;
}

void HTSPConnection::OnWake()
{
  CLockObject lock(m_mutex);

  Logger::Log(LogLevel::LEVEL_TRACE, "waking up (OnWake)");

  /* recreate connection */
  m_suspended = false;
}

void HTSPConnection::SetState(PVR_CONNECTION_STATE state)
{
  PVR_CONNECTION_STATE prevState(PVR_CONNECTION_STATE_UNKNOWN);
  PVR_CONNECTION_STATE newState(PVR_CONNECTION_STATE_UNKNOWN);

  {
    CLockObject lock(m_mutex);

    /* No notification if no state change or while suspended. */
    if (m_state != state && !m_suspended)
    {
      prevState = m_state;
      newState = state;
      m_state = newState;

      Logger::Log(LogLevel::LEVEL_DEBUG, "connection state change (%d -> %d)", prevState, newState);
    }
  }

  if (prevState != newState)
  {
    static std::string serverString;

    /* Notify connection state change (callback!) */
    serverString = GetServerString();
    m_connListener.ConnectionStateChange(serverString, newState, "");
  }
}

/*
 * Close the connection
 */
void HTSPConnection::Disconnect()
{
  CLockObject lock(m_mutex);

  /* Close socket */
  if (m_socket)
  {
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
bool HTSPConnection::ReadMessage()
{
  /* Read 4 byte len */
  uint8_t lb[4];
  size_t len = m_socket->Read(&lb, sizeof(lb));
  if (len != sizeof(lb))
    return false;

  len = (lb[0] << 24) + (lb[1] << 16) + (lb[2] << 8) + lb[3];

  /* Read rest of packet */
  uint8_t* buf = static_cast<uint8_t*>(malloc(len));
  size_t cnt = 0;
  while (cnt < len)
  {
    ssize_t r = m_socket->Read(buf + cnt, len - cnt, Settings::GetInstance().GetResponseTimeout());
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
  htsmsg_t* msg = htsmsg_binary_deserialize(buf, len, buf);
  if (!msg)
  {
    /* Do not free buf here. Already done by htsmsg_binary_deserialize. */
    Logger::Log(LogLevel::LEVEL_ERROR, "failed to decode message");
    return false;
  }

  /* Sequence number - response */
  uint32_t seq = 0;
  if (htsmsg_get_u32(msg, "seq", &seq) == 0)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "received response [%d]", seq);
    CLockObject lock(m_mutex);
    HTSPResponseList::iterator it = m_messages.find(seq);
    if (it != m_messages.end())
    {
      it->second->Set(msg);
      return true;
    }
  }

  /* Get method */
  const char* method = htsmsg_get_str(msg, "method");
  if (!method)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "message without a method");
    htsmsg_destroy(msg);
    return true;
  }
  Logger::Log(LogLevel::LEVEL_TRACE, "receive message [%s]", method);

  /* Pass (if return is true, message is finished) */
  if (m_connListener.ProcessMessage(method, msg))
    htsmsg_destroy(msg);
  // TODO: maybe a copy should be made if it needs to be kept?

  return true;
}

/*
 * Send message to server
 */
bool HTSPConnection::SendMessage0(const char* method, htsmsg_t* msg)
{
  uint32_t seq = 0;
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
  void* buf = nullptr;
  size_t len = 0;
  int e = htsmsg_binary_serialize(msg, &buf, &len, -1);
  htsmsg_destroy(msg);
  if (e < 0)
    return false;

  /* Send data */
  ssize_t c = m_socket->Write(buf, len);
  free(buf);

  if (c != static_cast<ssize_t>(len))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "failed to write (%s)", m_socket->GetError().c_str());
    if (!m_suspended)
      Disconnect();

    return false;
  }

  return true;
}

/*
 * Send a message and wait for response
 */
htsmsg_t* HTSPConnection::SendAndWait0(const char* method, htsmsg_t* msg, int iResponseTimeout)
{
  if (iResponseTimeout == -1)
    iResponseTimeout = Settings::GetInstance().GetResponseTimeout();

  /* Add Sequence number */
  uint32_t seq = ++m_seq;
  htsmsg_add_u32(msg, "seq", seq);

  HTSPResponse resp;
  m_messages[seq] = &resp;

  /* Send Message (bypass TX check) */
  if (!SendMessage0(method, msg))
  {
    m_messages.erase(seq);
    Logger::Log(LogLevel::LEVEL_ERROR, "failed to transmit");
    return nullptr;
  }

  /* Wait for response */
  msg = resp.Get(m_mutex, iResponseTimeout);
  m_messages.erase(seq);
  if (!msg)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "Command %s failed: No response received", method);
    if (!m_suspended)
      Disconnect();

    return nullptr;
  }

  /* Check result for errors and announce. */
  uint32_t noaccess = 0;
  if (!htsmsg_get_u32(msg, "noaccess", &noaccess) && noaccess)
  {
    // access denied
    Logger::Log(LogLevel::LEVEL_ERROR, "Command %s failed: Access denied", method);
    htsmsg_destroy(msg);
    return nullptr;
  }
  else
  {
    const char* strError = htsmsg_get_str(msg, "error");
    if (strError)
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "Command %s failed: %s", method, strError);
      htsmsg_destroy(msg);
      return nullptr;
    }
  }

  return msg;
}

/*
 * Send and wait for response
 */
htsmsg_t* HTSPConnection::SendAndWait(const char* method, htsmsg_t* msg, int iResponseTimeout)
{
  if (iResponseTimeout == -1)
    iResponseTimeout = Settings::GetInstance().GetResponseTimeout();

  if (!WaitForConnection())
    return nullptr;

  return SendAndWait0(method, msg, iResponseTimeout);
}

bool HTSPConnection::SendHello()
{
  /* Build message */
  htsmsg_t* msg = htsmsg_create_map();
  htsmsg_add_str(msg, "clientname", "Kodi Media Center");
  htsmsg_add_u32(msg, "htspversion", HTSP_CLIENT_VERSION);

  /* Send and Wait */
  msg = SendAndWait0("hello", msg);
  if (!msg)
    return false;

  /* Process */

  /* Basic Info */
  const char* webroot = htsmsg_get_str(msg, "webroot");
  m_serverName = htsmsg_get_str(msg, "servername");
  m_serverVersion = htsmsg_get_str(msg, "serverversion");
  m_htspVersion = htsmsg_get_u32_or_default(msg, "htspversion", 0);
  m_webRoot = webroot ? webroot : "";
  Logger::Log(LogLevel::LEVEL_DEBUG, "connected to %s / %s (HTSPv%d)", m_serverName.c_str(),
              m_serverVersion.c_str(), m_htspVersion);

  /* Capabilities */
  htsmsg_t* cap = htsmsg_get_list(msg, "servercapability");
  if (cap)
  {
    htsmsg_field_t* f = nullptr;
    HTSMSG_FOREACH(f, cap)
    {
      if (f->hmf_type == HMF_STR)
        m_capabilities.emplace_back(f->hmf_str);
    }
  }

  /* Authentication */
  const void* chal = nullptr;
  size_t chal_len = 0;
  htsmsg_get_bin(msg, "challenge", &chal, &chal_len);
  if (chal && chal_len)
  {
    m_challenge = malloc(chal_len);
    m_challengeLen = chal_len;
    std::memcpy(m_challenge, chal, chal_len);
  }

  htsmsg_destroy(msg);
  return true;
}

bool HTSPConnection::SendAuth(const std::string& user, const std::string& pass)
{
  htsmsg_t* msg = htsmsg_create_map();
  htsmsg_add_str(msg, "username", user.c_str());

  /* Add Password */
  // Note: we MUST send a digest or TVH will not evaluate the
  struct HTSSHA1* sha = static_cast<struct HTSSHA1*>(malloc(hts_sha1_size));
  uint8_t d[20];
  hts_sha1_init(sha);
  hts_sha1_update(sha, reinterpret_cast<const uint8_t*>(pass.c_str()), pass.length());
  if (m_challenge)
    hts_sha1_update(sha, static_cast<const uint8_t*>(m_challenge), m_challengeLen);
  hts_sha1_final(sha, d);
  htsmsg_add_bin(msg, "digest", d, sizeof(d));
  free(sha);

  /* Send and Wait */
  msg = SendAndWait0("authenticate", msg);

  if (!msg)
    return 0;

  if (m_htspVersion >= 26)
  {
    /* Log received permissions */
    Logger::Log(LogLevel::LEVEL_INFO, "  Received permissions:");

    uint32_t u32 = 0;
    if (!htsmsg_get_u32(msg, "admin", &u32))
      Logger::Log(LogLevel::LEVEL_INFO, "  administrator              : %i", u32);
    if (!htsmsg_get_u32(msg, "streaming", &u32))
      Logger::Log(LogLevel::LEVEL_INFO, "  HTSP streaming             : %i", u32);
    if (!htsmsg_get_u32(msg, "dvr", &u32))
      Logger::Log(LogLevel::LEVEL_INFO, "  HTSP DVR                   : %i", u32);
    if (!htsmsg_get_u32(msg, "faileddvr", &u32))
      Logger::Log(LogLevel::LEVEL_INFO, "  Failed/aborted DVR         : %i", u32);
    if (!htsmsg_get_u32(msg, "anonymous", &u32))
      Logger::Log(LogLevel::LEVEL_INFO, "  anonymous HTSP only        : %i", u32);
    if (!htsmsg_get_u32(msg, "limitall", &u32))
      Logger::Log(LogLevel::LEVEL_INFO, "  global connection limit    : %i", u32);
    if (!htsmsg_get_u32(msg, "limitdvr", &u32))
      Logger::Log(LogLevel::LEVEL_INFO, "  DVR connection limit       : %i", u32);
    if (!htsmsg_get_u32(msg, "limitstreaming", &u32))
      Logger::Log(LogLevel::LEVEL_INFO, "  streaming connection limit : %i", u32);
  }

  htsmsg_destroy(msg);
  return 1;
}

/**
 * Register the connection, hello+auth
 */
void HTSPConnection::Register()
{
  std::string user = Settings::GetInstance().GetUsername();
  std::string pass = Settings::GetInstance().GetPassword();

  {
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
      Logger::Log(
          LogLevel::LEVEL_ERROR,
          "server htsp version (v%d) does not match minimum htsp version required by client (v%d)",
          m_htspVersion, HTSP_MIN_SERVER_VERSION);
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
    if (!m_connListener.Connected())
      goto fail;

    Logger::Log(LogLevel::LEVEL_DEBUG, "registered");
    SetState(PVR_CONNECTION_STATE_CONNECTED);

    {
      CLockObject lock(m_mutex);
      m_ready = true;
      m_regCond.Broadcast();
    }
    return;
  }

fail:
  if (!m_suspended)
  {
    /* Don't immediately reconnect (spare server CPU cycles) */
    Sleep(SLOW_RECONNECT_INTERVAL);
    Disconnect();
  }
}

/*
 * Main thread loop for connection and rx handling
 */
void* HTSPConnection::Process()
{
  static bool log = false;
  static unsigned int retryAttempt = 0;
  const Settings& settings = Settings::GetInstance();

  while (!IsStopped())
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "new connection requested");

    std::string host = settings.GetHostname();
    int port = settings.GetPortHTSP();
    int timeout = settings.GetConnectTimeout();

    /* Create socket (ensure mutex protection) */
    {
      CLockObject lock(m_mutex);
      if (m_socket)
        delete m_socket;

      m_socket = new CTcpSocket(host.c_str(), port);
      m_ready = false;
      m_seq = 0;
      if (m_challenge)
      {
        free(m_challenge);
        m_challenge = nullptr;
      }
    }

    m_connListener.Disconnected();

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

    /* wakeup server */
    std::string wol_mac = settings.GetWolMac();
    if (!wol_mac.empty())
    {
      Logger::Log(LogLevel::LEVEL_TRACE, "send wol packet...");
      if (!kodi::network::WakeOnLan(wol_mac))
      {
        Logger::Log(LogLevel::LEVEL_ERROR, "Error waking up Server at MAC-Address %s",
                    wol_mac.c_str());
      }
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
    m_regThread->CreateThread(true);

    /* Receive loop */
    while (!IsStopped())
    {
      if (!ReadMessage())
        break;
    }

    /* Stop connect thread (if not already) */
    m_regThread->StopThread(0);
  }

  return nullptr;
}
