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

#include "DialogTranscodeSettings.h"
#include "libXBMC_gui.h"

#define BUTTON_OK                       1
#define BUTTON_CANCEL                   2

#define CONTROL_RADIO_BUTTON_TRANSCODE  10
#define SPIN_CONTROL_AUDIO_CODEC        11
#define SPIN_CONTROL_VIDEO_CODEC        12
#define SPIN_CONTROL_RESOLUTION         13


DialogTranscodeSettings::DialogTranscodeSettings()
: m_window(0), m_spinAudioCodec(0), m_spinVideoCodec(0), m_spinResolution(0),
  m_radioTranscode(0)
{
	m_window = GUI->Window_create("DialogTranscode.xml", "Confluence", false, true);
	m_window->m_cbhdl = this;
	m_window->CBOnInit = OnInitCB;
	m_window->CBOnFocus = OnFocusCB;
	m_window->CBOnClick = OnClickCB;
	m_window->CBOnAction = OnActionCB;
}


DialogTranscodeSettings::~DialogTranscodeSettings()
{
	GUI->Window_destroy(m_window);
}


bool DialogTranscodeSettings::OnInitCB(GUIHANDLE cbhdl)
{
	DialogTranscodeSettings* dialog = static_cast<DialogTranscodeSettings*>(cbhdl);
	return dialog->OnInit();
}


bool DialogTranscodeSettings::OnClickCB(GUIHANDLE cbhdl, int controlId)
{
	DialogTranscodeSettings* dialog = static_cast<DialogTranscodeSettings*>(cbhdl);
	return dialog->OnClick(controlId);
}


bool DialogTranscodeSettings::OnFocusCB(GUIHANDLE cbhdl, int controlId)
{
	DialogTranscodeSettings* dialog = static_cast<DialogTranscodeSettings*>(cbhdl);
	return dialog->OnFocus(controlId);
}


bool DialogTranscodeSettings::OnActionCB(GUIHANDLE cbhdl, int actionId)
{
	DialogTranscodeSettings* dialog = static_cast<DialogTranscodeSettings*>(cbhdl);
	return dialog->OnAction(actionId);
}


bool DialogTranscodeSettings::Show()
{
	if(m_window)
		return m_window->Show();

	return false;
}


void DialogTranscodeSettings::Close()
{
	if(m_window)
		m_window->Close();
}


void DialogTranscodeSettings::DoModal()
{
	if(m_window)
		m_window->DoModal();
}


bool DialogTranscodeSettings::OnInit()
{
	m_spinAudioCodec = GUI->Control_getSpin(m_window, SPIN_CONTROL_AUDIO_CODEC);
	m_spinVideoCodec = GUI->Control_getSpin(m_window, SPIN_CONTROL_VIDEO_CODEC);
	m_spinResolution = GUI->Control_getSpin(m_window, SPIN_CONTROL_RESOLUTION);

	m_radioTranscode = GUI->Control_getRadioButton(m_window, CONTROL_RADIO_BUTTON_TRANSCODE);

	m_spinAudioCodec->Clear();
	m_spinVideoCodec->Clear();
	m_spinResolution->Clear();

	m_spinAudioCodec->AddLabel("MPEG-2 Audio", CODEC_ID_MP2);
	m_spinAudioCodec->AddLabel("AAC",          CODEC_ID_AAC);
	m_spinAudioCodec->AddLabel("Passthrough",  CODEC_ID_NONE);

	m_spinVideoCodec->AddLabel("MPEG-2 Video", CODEC_ID_MPEG2VIDEO);
	m_spinVideoCodec->AddLabel("H264",         CODEC_ID_H264);
	m_spinVideoCodec->AddLabel("MPEG-4",       CODEC_ID_MPEG4);
	m_spinVideoCodec->AddLabel("Passthrough",  CODEC_ID_NONE);

	m_spinResolution->AddLabel("192p", 192);
	m_spinResolution->AddLabel("288p", 288);
	m_spinResolution->AddLabel("384p", 384);
	m_spinResolution->AddLabel("480p", 480);
	m_spinResolution->AddLabel("576p", 576);
	m_spinResolution->AddLabel("720p", 720);

	if(g_iResolution <= 192)
		m_spinResolution->SetValue(192);
	else if(g_iResolution <= 288)
		m_spinResolution->SetValue(288);
	else if(g_iResolution <= 384)
		m_spinResolution->SetValue(384);
	else if(g_iResolution <= 480)
		m_spinResolution->SetValue(480);
	else if(g_iResolution <= 576)
		m_spinResolution->SetValue(576);
	else
		m_spinResolution->SetValue(720);

	m_radioTranscode->SetSelected(g_bTranscode);
	m_spinAudioCodec->SetValue(g_iAudioCodec);
	m_spinVideoCodec->SetValue(g_iVideoCodec);

	return true;
}


bool DialogTranscodeSettings::OnClick(int controlId)
{
	if(controlId == BUTTON_CANCEL)
	{
		m_window->Close();
	    GUI->Control_releaseSpin(m_spinAudioCodec);
	    GUI->Control_releaseSpin(m_spinVideoCodec);
	    GUI->Control_releaseSpin(m_spinResolution);

	    GUI->Control_releaseRadioButton(m_radioTranscode);
	}
	else if(controlId == BUTTON_OK)
	{
		g_bTranscode  = m_radioTranscode->IsSelected();
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


bool DialogTranscodeSettings::OnFocus(int controlId)
{
	return true;
}


bool DialogTranscodeSettings::OnAction(int actionId)
{
	if (actionId == ADDON_ACTION_CLOSE_DIALOG || actionId == ADDON_ACTION_PREVIOUS_MENU)
		return OnClick(BUTTON_CANCEL);
	else
		return true;
}

