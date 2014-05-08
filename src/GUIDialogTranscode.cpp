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
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301  USA
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

  m_spinAudioCodec->AddLabel("Passthrough", XBMC_INVALID_CODEC_ID);
  m_spinVideoCodec->AddLabel("Passthrough", XBMC_INVALID_CODEC_ID);

  int iSelectedAudio(0), iSelectedVideo(0);
  for (unsigned int iPtr = 0; iPtr < m_codecs.size(); iPtr++)
  {
    if (m_codecs.at(iPtr).Codec().codec_type == XBMC_CODEC_TYPE_AUDIO)
    {
      m_spinAudioCodec->AddLabel(m_codecs.at(iPtr).Name().c_str(), iPtr);
      if (m_codecs.at(iPtr).Codec().codec_id == g_audioCodec.Codec().codec_id)
        iSelectedAudio = iPtr;
    }
    else if (m_codecs.at(iPtr).Codec().codec_type == XBMC_CODEC_TYPE_VIDEO)
    {
      m_spinVideoCodec->AddLabel(m_codecs.at(iPtr).Name().c_str(), iPtr);
      if (m_codecs.at(iPtr).Codec().codec_id == g_videoCodec.Codec().codec_id)
        iSelectedVideo = iPtr;
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
  m_spinAudioCodec->SetValue(iSelectedAudio);
  m_spinVideoCodec->SetValue(iSelectedVideo);

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
    g_audioCodec = m_codecs.at(m_spinAudioCodec->GetValue());
    g_videoCodec = m_codecs.at(m_spinVideoCodec->GetValue());

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
    return false;
}

