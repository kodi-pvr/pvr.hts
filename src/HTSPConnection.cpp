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

#include "kodi/threads/mutex.h"
#include "kodi/util/timeutils.h"
#include "kodi/sockets/tcp.h"

extern "C" {
#include "kodi/util/atomic.h"
#include "libhts/htsmsg_binary.h"
#include "libhts/sha1.h"
}

#include "Tvheadend.h"
#include "client.h"

using namespace std;
using namespace ADDON;
using namespace PLATFORM;

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
    m_webRoot(""), m_challenge(NULL), m_challengeLen(0)
{
}

CHTSPConnection::~CHTSPConnection()
{
  StopThread(-1);
  Disconnect();
  StopThread(0);
}

/*
 * Info
 */

CStdString CHTSPConnection::GetWebURL ( const char *fmt, ... )
{
  va_list va;
  CStdString auth, url;
  tvheadend::Settings settings = tvh->GetSettings();

  auth = settings.strUsername;
  if (auth != "" && settings.strPassword != "")
    auth += ":" + settings.strPassword;
  if (auth != "")
    auth += "@";
  url.Format("http://%s%s:%d", auth.c_str(), settings.strHostname.c_str(), settings.iPortHTTP);

  CLockObject lock(m_mutex);
  va_start(va, fmt);
  url += m_webRoot;
  url.AppendFormatV(fmt, va);
  va_end(va);

  return url;
}

bool CHTSPConnection::WaitForConnection ( void )
{
  if (!m_ready) {
    tvhtrace("waiting for registration...");
    m_regCond.Wait(m_mutex, m_ready, tvh->GetSettings().iConnectTimeout);
  }
  return m_ready;
}

const char *CHTSPConnection::GetServerName ( void )
{
  static CStdString str;
  CLockObject lock(m_mutex);
  str = m_serverName;
  return str.c_str();
}

const char *CHTSPConnection::GetServerVersion ( void )
{
  static CStdString str;
  CLockObject lock(m_mutex);
  str.Format("%s (HTSPv%d)", m_serverVersion.c_str(), m_htspVersion);
  return str.c_str();
}

const char *CHTSPConnection::GetServerString ( void )
{
  static CStdString str;
  tvheadend::Settings settings = tvh->GetSettings();

  CLockObject lock(m_mutex);
  str.Format("%s:%d [%s]", settings.strHostname.c_str(), settings.iPortHTSP,
             m_ready ? "connected" : "disconnected");
  return str.c_str();
}

bool CHTSPConnection::HasCapability(const std::string &capability) const
{
  return std::find(m_capabilities.begin(), m_capabilities.end(), capability) 
         != m_capabilities.end();
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
    r = m_socket->Read((char*)buf + cnt, len - cnt, tvh->GetSettings().iResponseTimeout);
    if (r < 0)
    {
      tvherror("failed to read packet (%s)",
               m_socket->GetError().c_str());
      free(buf);
      return false;
    } 
    cnt += r;
    if (cnt < len)
      printf("partial read\n");
  }

  /* Deserialize */
  if (!(msg = htsmsg_binary_deserialize(buf, len, buf)))
  {
    /* Do not free buf here. Already done by htsmsg_binary_deserialize. */
    tvherror("failed to decode message");
    return false;
  }

  /* Sequence number - response */
  if (htsmsg_get_u32(msg, "seq", &seq) == 0)
  {
    tvhtrace("received response [%d]", seq);
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
    tvherror("message without a method");
    htsmsg_destroy(msg);
    return true;
  }
  tvhtrace("receive message [%s]", method);

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
  ssize_t c = -1;
  uint32_t seq;

  if (!htsmsg_get_u32(msg, "seq", &seq))
    tvhtrace("sending message [%s : %d]", method, seq);
  else
    tvhtrace("sending message [%s]", method);
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
    tvherror("failed to write (%s)",
              m_socket->GetError().c_str());
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
    iResponseTimeout = tvh->GetSettings().iResponseTimeout;
  
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
    tvherror("failed to transmit");
    return NULL;
  }

  /* Wait for response */
  msg = resp.Get(m_mutex, iResponseTimeout);
  m_messages.erase(seq);
  if (!msg)
  {
    //XBMC->QueueNotification(QUEUE_ERROR, "Command %s failed: No response received", method);
    tvherror("Command %s failed: No response received", method);
    Disconnect();
    return NULL;
  }

  /* Check result for errors and announce. */
  uint32_t noaccess;
  if (!htsmsg_get_u32(msg, "noaccess", &noaccess) && noaccess)
  {
    // access denied
    //XBMC->QueueNotification(QUEUE_ERROR, "Command %s failed: Access denied", method);
    tvherror("Command %s failed: Access denied", method);
    htsmsg_destroy(msg);
    return NULL;
  }
  else
  {
    const char* strError;
    if(strError = htsmsg_get_str(msg, "error"))
    {
      //XBMC->QueueNotification(QUEUE_ERROR, "Command %s failed: %s", method, strError);
      tvherror("Command %s failed: %s", method, strError);
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
    iResponseTimeout = tvh->GetSettings().iResponseTimeout;
  
  if (!WaitForConnection())
    return NULL;
  return SendAndWait0(method, msg, iResponseTimeout);
}

bool CHTSPConnection::SendHello ( void )
{
  /* Build message */
  htsmsg_t *msg = htsmsg_create_map();
  htsmsg_add_str(msg, "clientname", "Kodi Media Center");
  htsmsg_add_u32(msg, "htspversion", HTSP_API_VERSION);

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
  tvhdebug("connected to %s / %s (HTSPv%d)",
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
  ( const CStdString &user, const CStdString &pass )
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
  CStdString user, pass;
  user = tvh->GetSettings().strUsername;
  pass = tvh->GetSettings().strPassword;

  {
    CLockObject lock(m_mutex);

    /* Send Greeting */
    tvhdebug("sending hello");
    if (!SendHello()) {
      tvherror("failed to send hello");
      goto fail;
    }

    /* Send Auth */
    tvhdebug("sending auth");
    
    if (!SendAuth(user, pass))
      goto fail;

    /* Rebuild state */
    tvhdebug("rebuilding state");
    if (!tvh->Connected())
      goto fail;

    tvhdebug("registered");
    m_ready = true;
    m_regCond.Broadcast();
    return;
  }

  /* Rate limit retry */
fail:
  Sleep(5000);
  Disconnect();
}

/*
 * Main thread loop for connection and rx handling
 */
void* CHTSPConnection::Process ( void )
{
  static bool log = false;
  static unsigned int retryAttempt = 0;
  tvheadend::Settings settings = tvh->GetSettings();

  while (!IsStopped())
  {
    CStdString host = settings.strHostname;
    int port, timeout;
    port = settings.iPortHTSP;
    timeout = settings.iConnectTimeout;

    /* Create socket (ensure mutex protection) */
    {
      CLockObject lock(m_mutex);
      if (m_socket)
        delete m_socket;
      tvh->Disconnected();
      if (!log)
        tvhdebug("connecting to %s:%d", host.c_str(), port);
      else
        tvhtrace("connecting to %s:%d", host.c_str(), port);
      log = true;
      m_socket = new CTcpSocket(host.c_str(), port);
      m_ready  = false;
      m_seq    = 0;
      if (m_challenge) {
        free(m_challenge);
        m_challenge = NULL;
      }
    }

    /* Connect */
    tvhtrace("waiting for connection...");
    if (!m_socket->Open(timeout))
    {
      /* Unable to connect, inform the user */
      tvherror("unable to connect to %s:%d", host.c_str(), port);
      
      // Retry a few times with a short interval, after that with the default timeout
      if (++retryAttempt <= FAST_RECONNECT_ATTEMPTS)
        Sleep(FAST_RECONNECT_INTERVAL);
      else
        Sleep(timeout);
      
      continue;
    }
    tvhdebug("connected");
    log = false;
    retryAttempt = 0;

    /* Start connect thread */
    m_regThread.CreateThread(true);

    /* Receive loop */
    while (!IsStopped())
    {
      if (!ReadMessage())
      {
        break;
      }
    }

    /* Stop connect thread (if not already) */
    m_regThread.StopThread(0);
  }

  return NULL;
}
