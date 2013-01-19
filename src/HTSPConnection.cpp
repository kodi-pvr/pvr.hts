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

#include "HTSPConnection.h"
#include "platform/threads/mutex.h"
#include "platform/util/timeutils.h"
#include "platform/sockets/tcp.h"
#include "client.h"

extern "C" {
#include "platform/util/atomic.h"
#include "libhts/htsmsg_binary.h"
#include "libhts/sha1.h"
}

using namespace std;
using namespace ADDON;
using namespace PLATFORM;

CHTSResult::CHTSResult(void) :
    message(NULL),
    status(PVR_ERROR_NO_ERROR)
{
}

CHTSResult::~CHTSResult(void)
{
  if (message != NULL)
    htsmsg_destroy(message);
}

string CHTSResult::GetErrorMessage(void)
{
  if (m_strError.empty())
  {
    if (!message)
    {
      m_strError = "No response received";
    }
    else
    {
      const char* error;
      if((error = htsmsg_get_str(message, "error")))
        m_strError = error;
    }
  }
  return m_strError;
}

bool CHTSResult::IsError(void)
{
  return !GetErrorMessage().empty();
}

bool CHTSResult::NoAccess(void)
{
  uint32_t noaccess;
  return (message && !htsmsg_get_u32(message, "noaccess", &noaccess) && noaccess);
}

CHTSPConnection::CHTSPConnection(CHTSPConnectionCallback* callback) :
    m_socket(new CTcpConnection(g_strHostname, g_iPortHTSP)),
    m_challenge(NULL),
    m_iChallengeLength(0),
    m_iProtocol(0),
    m_iPortnumber(g_iPortHTSP),
    m_iConnectTimeout(g_iConnectTimeout * 1000),
    m_strUsername(g_strUsername),
    m_strPassword(g_strPassword),
    m_strHostname(g_strHostname),
    m_bIsConnected(false),
    m_bTimeshiftSupport(false),
    m_bTimeshiftSeekSupport(false),
    m_bTranscodingSupport(false),
    m_iQueueSize(1000),
    m_callback(callback),
    m_iReadTimeout(-1)
{
  m_reconnect = new CHTSPReconnect(this);
}

CHTSPConnection::~CHTSPConnection()
{
  // close the connection and stop the thread
  Close();

  delete m_socket;
  delete m_reconnect;
}

const CStdString CHTSPConnection::GetWebURL (const char *fmt, ...) const
{
  CStdString url;
  CStdString auth;

  /* Authentication */
  if (!g_strUsername.empty()) {
    auth = g_strUsername;
    if (!g_strPassword.empty())
      auth.AppendFormat(":%s", g_strPassword.c_str());
    auth += "@";
  } else {
    auth = "";
  }

  /* URL root */
  url.Format("http://%s%s:%i%s", auth.c_str(), g_strHostname.c_str(), g_iPortHTTP, m_strWebroot.c_str());

  va_list args;
  va_start(args, fmt);
  url.AppendFormatV(fmt, args);
  va_end(args);

  return url;
}

void CHTSPConnection::SetReadTimeout(int iTimeout)
{
  CLockObject lock(m_mutex);
  m_iReadTimeout = iTimeout;
  m_readTimeout.Init(iTimeout);
}

bool CHTSPConnection::OpenSocket(void)
{
  CLockObject lock(m_mutex);
  // already open
  if (m_socket && m_socket->IsOpen())
    return true;

  // check if the socket could be created
  if (!m_socket)
  {
    XBMC->Log(LOG_ERROR, "%s - failed to connect to the backend (couldn't create a socket)", __FUNCTION__);
    return false;
  }

  XBMC->Log(LOG_DEBUG, "%s - connecting to '%s', port '%d'", __FUNCTION__, m_strHostname.c_str(), m_iPortnumber);

  // try to open the socket
  CTimeout timeout(m_iConnectTimeout);
  while (!m_socket->IsOpen() && timeout.TimeLeft() > 0)
  {
    if (!m_socket->Open(timeout.TimeLeft()))
      CEvent::Sleep(100);
  }

  // check if the socket is open
  if (!m_socket->IsOpen())
  {
    XBMC->Log(LOG_ERROR, "%s - failed to connect to the backend (%s)", __FUNCTION__, m_socket->GetError().c_str());
    return false;
  }

  // socket opened
  m_bIsConnected = true;
  XBMC->Log(LOG_DEBUG, "%s - connected to '%s', port '%d'", __FUNCTION__, m_strHostname.c_str(), m_iPortnumber);
  return true;
}

bool CHTSPConnection::Connect(void)
{
  bool bFailed(false);
  {
    CLockObject lock(m_mutex);

    // already connected
    if (m_bIsConnected)
      return true;

    // open a socket
    if (!OpenSocket())
      return false;

    // send the greeting, get the protocol version and capabilities
    if (!SendGreeting())
    {
      XBMC->Log(LOG_ERROR, "%s - failed to read greeting from the backend", __FUNCTION__);
      m_socket->Close();
      return false;
    }

    // check whether the proto is v2+
    if(m_iProtocol < 2)
    {
      XBMC->Log(LOG_ERROR, "%s - incompatible protocol version %d", __FUNCTION__, m_iProtocol);
      m_socket->Close();
      return false;
    }

    // create reader thread
    if (!IsRunning() && !CreateThread(true))
    {
      XBMC->Log(LOG_ERROR, "%s - failed to create data processing thread", __FUNCTION__);
      bFailed = true;
    }

    // send authentication
    if (!bFailed && !Auth())
    {
      XBMC->Log(LOG_ERROR, "%s - failed to authenticate", __FUNCTION__);
      bFailed = true;
    }
  }

  if (bFailed)
    Close();

  // connected
  CLockObject lock(m_mutex);
  m_connectEvent.Broadcast();
  return true;
}

void CHTSPConnection::TriggerReconnect(void)
{
  XBMC->Log(LOG_DEBUG, "reconnect triggered");
  CLockObject lock(m_mutex);
  m_socket->Close();
  m_bIsConnected = false;
}

void CHTSPConnection::Close()
{
  // stop the reader thread
  StopThread();

  // close the socket
  CLockObject lock(m_mutex);
  m_bIsConnected = false;

  if(m_socket && m_socket->IsOpen())
    m_socket->Close();

  // cleanup
  if(m_challenge)
  {
    free(m_challenge);
    m_challenge        = NULL;
    m_iChallengeLength = 0;
  }

  for (deque<htsmsg_t*>::iterator it = m_queue.begin(); it != m_queue.end();)
    delete *(it++);
  m_queue.clear();

  m_connectEvent.Broadcast();
}

htsmsg_t* CHTSPConnection::ReadMessage(int iInitialTimeout /* = 10000 */, int iDatapacketTimeout /* = 10000 */)
{
  void*    buf;
  uint32_t l;

  // get the first queued message if any
  if(m_queue.size())
  {
    htsmsg_t* m = m_queue.front();
    m_queue.pop_front();
    return m;
  }

  {
    CLockObject lock(m_mutex);
    // check whether the socket is open
    if (!m_socket || !m_socket->IsOpen())
    {
      XBMC->Log(LOG_ERROR, "%s - not connected", __FUNCTION__);
      return NULL;
    }

    // read the size
    if (m_socket->Read(&l, 4, iInitialTimeout) != 4)
    {
      // timed out
      if(m_socket->GetErrorNumber() == ETIMEDOUT)
        return NULL;

      // read error, close the connection
      XBMC->Log(LOG_ERROR, "%s - failed to read packet size (%s)", __FUNCTION__, m_socket->GetError().c_str());
      TriggerReconnect();
      return NULL;
    }

    l = ntohl(l);

    // empty message
    if(l == 0)
      return htsmsg_create_map();

    // read the data
    buf = malloc(l);
    if(m_socket->Read(buf, l, iDatapacketTimeout) != (ssize_t)l)
    {
      // failed to read (wrong size), close the connection
      XBMC->Log(LOG_ERROR, "%s - failed to read packet (%s)", __FUNCTION__, m_socket->GetError().c_str());
      free(buf);
      TriggerReconnect();
      return NULL;
    }
  }

  // return the data
  return htsmsg_binary_deserialize(buf, l, buf); /* consumes 'buf' */
}

bool CHTSPConnection::TransmitMessage(htsmsg_t* m)
{
  void*  buf;
  size_t len;

  // check whether the socket is open
  if (!m_socket || !m_socket->IsOpen())
  {
    XBMC->Log(LOG_ERROR, "%s - not connected", __FUNCTION__);
    htsmsg_destroy(m);
    return false;
  }

  // serialise the data
  if(htsmsg_binary_serialize(m, &buf, &len, -1) < 0)
  {
    htsmsg_destroy(m);
    return false;
  }
  htsmsg_destroy(m);

  // write the data
  CLockObject lock(m_mutex);
  ssize_t iWriteResult = m_socket->Write(buf, len);
  if (iWriteResult != (ssize_t)len)
  {
    // failed to write, close the connection
    XBMC->Log(LOG_ERROR, "%s - failed to write packet (%s)", __FUNCTION__, m_socket->GetError().c_str());
    free(buf);
    TriggerReconnect();
    return false;
  }
  free(buf);
  return true;
}

void CHTSPConnection::ReadResult(htsmsg_t *m, CHTSResult &result, const char* strAction /* = NULL */)
{
  // check whether we're connected
  if (!IsConnected())
  {
    htsmsg_destroy(m);
    result.status = PVR_ERROR_SERVER_ERROR;
    if (strAction)
      XBMC->Log(LOG_ERROR, "%s - '%s' failed - not connected", __FUNCTION__, strAction);
    return;
  }

  // store in the message queue
  result.status = PVR_ERROR_NO_ERROR;
  uint32_t seq = HTSPNextSequenceNumber();

  SMessage &message(m_messageQueue[seq]);
  message.event = new CEvent;
  message.msg   = NULL;

  // transmit the message
  htsmsg_add_u32(m, "seq", seq);
  if(!TransmitMessage(m))
  {
    // command couldn't be sent
    if (strAction)
      XBMC->Log(LOG_ERROR, "%s - '%s' failed - failed to send command", __FUNCTION__, strAction);
    else
      XBMC->Log(LOG_ERROR, "%s - failed to send command", __FUNCTION__);
    result.status = PVR_ERROR_SERVER_ERROR;
  }
  else if(!message.event->Wait(g_iResponseTimeout * 1000))
  {
    // no response
    if (strAction)
      XBMC->Log(LOG_ERROR, "%s - '%s' failed - request timed out after %d seconds", __FUNCTION__, strAction, g_iResponseTimeout);
    else
      XBMC->Log(LOG_ERROR, "%s - request timed out after %d seconds", __FUNCTION__, g_iResponseTimeout);
    result.status = PVR_ERROR_SERVER_TIMEOUT;
  }
  else
  {
    // response received
    result.message = message.msg;

    if (result.NoAccess())
    {
      // access denied
      if (strAction)
        XBMC->Log(LOG_ERROR, "%s - '%s' failed - access denied", __FUNCTION__, strAction);
      else
        XBMC->Log(LOG_ERROR, "%s - command failed - access denied", __FUNCTION__);
      XBMC->QueueNotification(QUEUE_ERROR, "Access denied");
      result.status = PVR_ERROR_REJECTED;
    }

    if (result.IsError())
    {
      // server reported an error
      string strError = result.GetErrorMessage();
      if (strAction)
        XBMC->Log(LOG_ERROR, "%s - '%s' failed - %s", __FUNCTION__, strAction, strError.c_str());
      else
        XBMC->Log(LOG_ERROR, "%s - command failed - %s", __FUNCTION__, strError.c_str());
      XBMC->QueueNotification(QUEUE_ERROR, "Command failed: %s", strError.c_str());
      result.status = PVR_ERROR_REJECTED;
    }
  }

  // delete from the queue
  {
    CLockObject lock(m_mutex);
    delete message.event;
    m_messageQueue.erase(seq);
  }
}

bool CHTSPConnection::ReadSuccess(htsmsg_t* m, const char* strAction /* = NULL */)
{
  CHTSResult result;
  ReadResult(m, result, strAction);
  return result.status == PVR_ERROR_NO_ERROR;
}

bool CHTSPConnection::SendGreeting(void)
{
  htsmsg_t *m, *cap;
  htsmsg_field_t *f;
  const char *server, *version, *webroot;
  const void * chall = NULL;
  size_t chall_len = 0;
  int32_t proto = 0;

  // send hello
  m = htsmsg_create_map();
  htsmsg_add_str(m, "method", "hello");
  htsmsg_add_str(m, "clientname", "XBMC Media Center");
  htsmsg_add_u32(m, "htspversion", 8);

  CLockObject lock(m_mutex);

  // read welcome
  if (!TransmitMessage(m))
  {
    XBMC->Log(LOG_ERROR, "CHTSPConnection - %s - failed to transmit greeting", __FUNCTION__);
    return false;
  }

  m = ReadMessage(g_iConnectTimeout * 1000, g_iConnectTimeout * 1000);
  if (m == NULL || m->hm_data == NULL)
  {
    if (m)
      htsmsg_destroy(m);
    // no welcome received
    XBMC->Log(LOG_ERROR, "CHTSPConnection - %s - failed get a reply after the greeting", __FUNCTION__);
    return false;
  }

            htsmsg_get_str(m,  "method");
            htsmsg_get_s32(m,  "htspversion", &proto);
  server  = htsmsg_get_str(m,  "servername");
  version = htsmsg_get_str(m,  "serverversion");
            htsmsg_get_bin(m,  "challenge", &chall, &chall_len);
  cap     = htsmsg_get_list(m, "servercapability");
  webroot = htsmsg_get_str(m,  "webroot");

  // process capabilities
  m_bTimeshiftSupport     = false;
  m_bTimeshiftSeekSupport = false;
  m_bTranscodingSupport   = false;
  if (cap)
  {
    HTSMSG_FOREACH(f, cap)
    {
      if (f->hmf_type == HMF_STR)
      {
        if (!strcmp("timeshift", f->hmf_str))
        {
          m_bTimeshiftSupport = true;
          m_bTimeshiftSeekSupport = true;
        }
        else if (!strcmp("transcoding", f->hmf_str))
          m_bTranscodingSupport = true;
      }
    }
  }

  m_strServerName = server;
  m_strVersion    = version;
  m_iProtocol     = proto;
  m_strWebroot    = webroot ? webroot : "";

  if(chall && chall_len)
  {
    m_challenge        = malloc(chall_len);
    m_iChallengeLength = chall_len;
    memcpy(m_challenge, chall, chall_len);
  }
  htsmsg_destroy(m);

  XBMC->Log(LOG_NOTICE, "CHTSPConnection - %s - connection opened to '%s %s', protocol v%d%s", __FUNCTION__, m_strServerName.c_str(), m_strVersion.c_str(), m_iProtocol, m_bTimeshiftSupport ? " (timeshift enabled)" : "");
  return true;
}

bool CHTSPConnection::Auth(void)
{
  CLockObject lock(m_mutex);
  // no username set, don't authenticate
  if (m_strUsername.empty())
  {
    XBMC->Log(LOG_DEBUG, "CHTSPConnection - %s - no username set. not authenticating", __FUNCTION__);
    return true;
  }

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method"  , "authenticate");
  htsmsg_add_str(m, "username", m_strUsername.c_str());

  if(!m_strPassword.empty() && m_challenge)
  {
    XBMC->Log(LOG_DEBUG, "CHTSPConnection - %s - authenticating as user '%s' with a password", __FUNCTION__, m_strUsername.c_str());

    struct HTSSHA1* shactx = (struct HTSSHA1*) malloc(hts_sha1_size);
    uint8_t d[20];
    hts_sha1_init(shactx);
    hts_sha1_update(shactx, (const uint8_t *) m_strPassword.c_str(), m_strPassword.length());
    hts_sha1_update(shactx, (const uint8_t *) m_challenge, m_iChallengeLength);
    hts_sha1_final(shactx, d);
    htsmsg_add_bin(m, "digest", d, 20);
    free(shactx);
  }
  else
  {
    XBMC->Log(LOG_DEBUG, "CHTSPConnection - %s - authenticating as user '%s' without a password", __FUNCTION__, m_strUsername.c_str());
  }

  if (!TransmitMessage(m))
  {
    XBMC->Log(LOG_ERROR, "CHTSPConnection - %s - failed to transmit auth command", __FUNCTION__);
    XBMC->QueueNotification(QUEUE_ERROR, "Access denied");
    return false;
  }

  CHTSResult result;
  result.message = ReadMessage(g_iConnectTimeout * 1000, g_iConnectTimeout * 1000);
  if (result.message == NULL)
  {
    XBMC->Log(LOG_ERROR, "CHTSPConnection - %s - failed to get a reply from the auth command", __FUNCTION__);
    XBMC->QueueNotification(QUEUE_ERROR, "Access denied");
    return false;
  }
  else if (result.NoAccess())
  {
    // access denied
    XBMC->Log(LOG_ERROR, "%s - auth failed - access denied", __FUNCTION__);
    XBMC->QueueNotification(QUEUE_ERROR, "Access denied");
    return false;
  }
  else if (result.IsError())
  {
    // server reported an error
    string strError = result.GetErrorMessage();
    XBMC->Log(LOG_ERROR, "%s - auth failed - %s", __FUNCTION__, strError.c_str());
    XBMC->QueueNotification(QUEUE_ERROR, "Access denied");
    return false;
  }

  return true;
}

bool CHTSPConnection::CheckConnection(uint32_t iTimeout)
{
  CLockObject lock(m_mutex);
  if (IsConnected())
    return true;

  return m_connectEvent.Wait(m_mutex, m_bIsConnected, iTimeout);
}

bool CHTSPConnection::IsConnected(void)
{
  CLockObject lock(m_mutex);
  return m_bIsConnected && m_socket && m_socket->IsOpen();
}

bool CHTSPConnection::CanTimeshift(void)
{
  return m_bTimeshiftSupport;
}

bool CHTSPConnection::CanSeekLiveStream(void)
{
  return m_bTimeshiftSeekSupport;
}

void* CHTSPConnection::Process(void)
{
  htsmsg_t* msg(NULL);
  while (!IsStopped())
  {
    if (!IsConnected() && !m_reconnect->IsRunning())
    {
      XBMC->Log(LOG_ERROR, "connection dropped, trying to restore");
      m_reconnect->CreateThread(true);
    }
    else
    {
      // if there's anything in the buffer, read it
      {
        {
          CLockObject lock(m_mutex);
          msg = ReadMessage(5);
        }
        if(msg == NULL || msg->hm_data == NULL)
        {
          if (msg)
            htsmsg_destroy(msg);

          {
            CLockObject lock(m_mutex);
            if (!m_reconnect->IsRunning() && m_iReadTimeout > 0 && m_readTimeout.TimeLeft() == 0)
            {
              TriggerReconnect();
              continue;
            }
          }

          Sleep(5);
          continue;
        }
      }

      {
        CLockObject lock(m_mutex);
        if (!m_reconnect->IsRunning() && m_iReadTimeout > 0)
          m_readTimeout.Init(m_iReadTimeout);
      }

      // signal if 'seq' is set
      uint32_t seq;
      if(htsmsg_get_u32(msg, "seq", &seq) == 0)
      {
        CLockObject lock(m_mutex);
        SMessages::iterator it = m_messageQueue.find(seq);
        if(it != m_messageQueue.end())
        {
          it->second.msg = msg;
          it->second.event->Broadcast();
          continue;
        }
      }

      // process the message
      m_callback->ProcessMessage(msg);
      htsmsg_destroy(msg);
    }
  }

  m_reconnect->StopThread();

  return NULL;
}

void* CHTSPReconnect::Process(void)
{
  if (m_connection->m_callback)
    m_connection->m_callback->OnConnectionDropped();

  while (!m_connection->IsConnected() && !IsStopped())
  {
    {
      CLockObject lock(m_connection->m_mutex);
      for (SMessages::iterator it = m_connection->m_messageQueue.begin(); it != m_connection->m_messageQueue.end(); it++)
        it->second.event->Broadcast();

      m_connection->m_bIsConnected = false;
      if(m_connection->m_challenge)
      {
        free(m_connection->m_challenge);
        m_connection->m_challenge        = NULL;
        m_connection->m_iChallengeLength = 0;
      }
    }

    if (m_connection->Connect())
    {
      if (m_connection->m_callback && m_connection->m_callback->OnConnectionRestored())
      {
        m_connection->m_bIsConnected = true;
        if (m_connection->m_iReadTimeout > 0)
          m_connection->m_readTimeout.Init(m_connection->m_iReadTimeout);
        XBMC->Log(LOG_DEBUG, "connection restored");
      }
      else
      {
        m_connection->TriggerReconnect();
        Sleep(1000);
      }
    }
    else
    {
      if (m_connection->m_callback)
        m_connection->m_callback->OnConnectionDropped();
    }
  }
  return NULL;
}
