/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://xbmc.org
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

#include "GUIDialogTranscode.h"
#include "libXBMC_gui.h"

#define BUTTON_OK                       1
#define BUTTON_CANCEL                   2

#define CONTROL_RADIO_BUTTON_TRANSCODE  10
#define SPIN_CONTROL_AUDIO_CODEC        11
#define SPIN_CONTROL_VIDEO_CODEC        12
#define SPIN_CONTROL_RESOLUTION         13

CGUIDialogTranscode::CGUIDialogTranscode(const CodecVector &v) :
    m_window(0), m_spinAudioCodec(0), m_spinVideoCodec(0), m_spinResolution(0),
    m_radioTranscode(0), m_codecs(v)
{
  m_window = GUI->Window_create("DialogTranscode.xml", "Confluence", false, true);
  m_window->m_cbhdl = this;
  m_window->CBOnInit = OnInitCB;
  m_window->CBOnFocus = OnFocusCB;
  m_window->CBOnClick = OnClickCB;
  m_window->CBOnAction = OnActionCB;
}

CGUIDialogTranscode::~CGUIDialogTranscode()
{
  GUI->Window_destroy(m_window);
}

bool CGUIDialogTranscode::OnInitCB(GUIHANDLE cbhdl)
{
  CGUIDialogTranscode* dialog = static_cast<CGUIDialogTranscode*>(cbhdl);
  return dialog->OnInit();
}

bool CGUIDialogTranscode::OnClickCB(GUIHANDLE cbhdl, int controlId)
{
  CGUIDialogTranscode* dialog = static_cast<CGUIDialogTranscode*>(cbhdl);
  return dialog->OnClick(controlId);
}

bool CGUIDialogTranscode::OnFocusCB(GUIHANDLE cbhdl, int controlId)
{
  CGUIDialogTranscode* dialog = static_cast<CGUIDialogTranscode*>(cbhdl);
  return dialog->OnFocus(controlId);
}

bool CGUIDialogTranscode::OnActionCB(GUIHANDLE cbhdl, int actionId)
{
  CGUIDialogTranscode* dialog = static_cast<CGUIDialogTranscode*>(cbhdl);
  return dialog->OnAction(actionId);
}

bool CGUIDialogTranscode::Show()
{
  if (m_window)
    return m_window->Show();

  return false;
}

void CGUIDialogTranscode::Close()
{
  if (m_window)
    m_window->Close();
}

void CGUIDialogTranscode::DoModal()
{
  if (m_window)
    m_window->DoModal();
}

bool CGUIDialogTranscode::OnInit()
{
  m_spinAudioCodec = GUI->Control_getSpin(m_window, SPIN_CONTROL_AUDIO_CODEC);
  m_spinVideoCodec = GUI->Control_getSpin(m_window, SPIN_CONTROL_VIDEO_CODEC);
  m_spinResolution = GUI->Control_getSpin(m_window, SPIN_CONTROL_RESOLUTION);

  m_radioTranscode = GUI->Control_getRadioButton(m_window, CONTROL_RADIO_BUTTON_TRANSCODE);

  m_spinAudioCodec->Clear();
  m_spinVideoCodec->Clear();
  m_spinResolution->Clear();

  m_spinAudioCodec->AddLabel("Passthrough", CODEC_ID_NONE);
  m_spinVideoCodec->AddLabel("Passthrough", CODEC_ID_NONE);

  for (CodecVector::iterator it = m_codecs.begin(); it != m_codecs.end(); ++it)
  {
    switch (*it)
    {
    case CODEC_ID_MP2:
      m_spinAudioCodec->AddLabel("MPEG-2 Audio", CODEC_ID_MP2);
      break;

    case CODEC_ID_AAC:
      m_spinAudioCodec->AddLabel("AAC", CODEC_ID_AAC);
      break;

    case CODEC_ID_AC3:
      m_spinAudioCodec->AddLabel("AC3", CODEC_ID_AC3);
      break;

    case CODEC_ID_VORBIS:
      m_spinAudioCodec->AddLabel("Vorbis", CODEC_ID_VORBIS);
      break;

    case CODEC_ID_MPEG2VIDEO:
      m_spinVideoCodec->AddLabel("MPEG-2 Video", CODEC_ID_MPEG2VIDEO);
      break;

    case CODEC_ID_MPEG4:
      m_spinVideoCodec->AddLabel("MPEG-4", CODEC_ID_MPEG4);
      break;

    case CODEC_ID_VP8:
      m_spinVideoCodec->AddLabel("VP8", CODEC_ID_VP8);
      break;

    case CODEC_ID_H264:
      m_spinVideoCodec->AddLabel("H264", CODEC_ID_H264);
      break;

    default:
      break;
    }
  }

  m_spinResolution->AddLabel("192p", 192);
  m_spinResolution->AddLabel("288p", 288);
  m_spinResolution->AddLabel("384p", 384);
  m_spinResolution->AddLabel("480p", 480);
  m_spinResolution->AddLabel("576p", 576);
  m_spinResolution->AddLabel("720p", 720);

  if (g_iResolution <= 192)
    m_spinResolution->SetValue(192);
  else if (g_iResolution <= 288)
    m_spinResolution->SetValue(288);
  else if (g_iResolution <= 384)
    m_spinResolution->SetValue(384);
  else if (g_iResolution <= 480)
    m_spinResolution->SetValue(480);
  else if (g_iResolution <= 576)
    m_spinResolution->SetValue(576);
  else
    m_spinResolution->SetValue(720);

  m_radioTranscode->SetSelected(g_bTranscode);
  m_spinAudioCodec->SetValue(g_iAudioCodec);
  m_spinVideoCodec->SetValue(g_iVideoCodec);

  return true;
}

bool CGUIDialogTranscode::OnClick(int controlId)
{
  if (controlId == BUTTON_CANCEL)
  {
    m_window->Close();
    GUI->Control_releaseSpin(m_spinAudioCodec);
    GUI->Control_releaseSpin(m_spinVideoCodec);
    GUI->Control_releaseSpin(m_spinResolution);

    GUI->Control_releaseRadioButton(m_radioTranscode);
  }
  else if (controlId == BUTTON_OK)
  {
    g_bTranscode = m_radioTranscode->IsSelected();
    g_iResolution = m_spinResolution->GetValue();
    g_iAudioCodec = (CodecID) m_spinAudioCodec->GetValue();
    g_iVideoCodec = (CodecID) m_spinVideoCodec->GetValue();

    m_window->Close();

    GUI->Control_releaseSpin(m_spinAudioCodec);
    GUI->Control_releaseSpin(m_spinVideoCodec);
    GUI->Control_releaseSpin(m_spinResolution);

    GUI->Control_releaseRadioButton(m_radioTranscode);
  }

  return true;
}

bool CGUIDialogTranscode::OnFocus(int controlId)
{
  return true;
}

bool CGUIDialogTranscode::OnAction(int actionId)
{
  if (actionId == ADDON_ACTION_CLOSE_DIALOG
      || actionId == ADDON_ACTION_PREVIOUS_MENU)
    return OnClick(BUTTON_CANCEL);
  else
    return true;
}

