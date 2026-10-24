// Sound and SoundUnit. Included by mfc_cfg.cpp; original resource geometry.
CSoundPage::CSoundPage() : m_pSound(NULL), m_pMIDI(NULL), m_nMasterOrg(-1), m_nMIDIOrg(-1), m_nTimerID(0) {
    m_dwID = MAKEID('S','N','D',' '); m_nTemplate = IDD_SOUNDPAGE; m_uHelpID = IDC_SOUND_HELP;
}
BEGIN_MESSAGE_MAP(CSoundPage, CConfigPage)
    ON_WM_VSCROLL()
    ON_WM_HSCROLL()
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_SOUND_DEVICEC, OnSelChange)
END_MESSAGE_MAP()

static void SoundSlider(CWnd *page, int id, int label, int value, int maximum = 100) {
    CSliderCtrl *slider = (CSliderCtrl*)page->GetDlgItem(id);
    slider->SetRangeMin(0); slider->SetRangeMax(maximum); slider->SetPos(value < 0 ? 0 : value);
    slider->EnableWindow(value >= 0);
    CString text;
    if (value >= 0) text.Format(_T(" %d"), maximum ? int((__int64)value * 100 / maximum) : 0);
    page->SetDlgItemText(label, text);
}
BOOL CSoundPage::OnInitDialog() {
    CConfigPage::OnInitDialog();
    CFrmWnd *frame = ResolveFrmWnd(this); if (!frame) return FALSE;
    m_pSound = frame->GetSound(); m_pMIDI = frame->GetMIDI();
    CComboBox *combo = (CComboBox*)GetDlgItem(IDC_SOUND_DEVICEC);
    combo->AddString(_T("(None)"));
    for (int i = 0; i < m_pSound->m_nDeviceNum; ++i) combo->AddString(m_pSound->m_DeviceDescr[i]);
    combo->SetCurSel(m_pConfig->sample_rate && m_pConfig->sound_device < m_pSound->m_nDeviceNum ? m_pConfig->sound_device + 1 : 0);
    combo = (CComboBox*)GetDlgItem(IDC_TG_INPUT); combo->AddString(_T("(None)"));
    for (int i = 0; i < m_pSound->m_nInputNum; ++i) combo->AddString(m_pSound->m_InputDescr[i]);
    combo->SetCurSel(m_pConfig->sound_input < m_pSound->m_nInputNum ? m_pConfig->sound_input + 1 : 0);
    if (m_pConfig->sample_rate) CheckRadioButton(IDC_SOUND_RATE0, IDC_SOUND_RATE4, IDC_SOUND_RATE0 + m_pConfig->sample_rate - 1);
    CSpinButtonCtrl *spin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF1S);
    spin->SetRange(2,100); spin->SetPos(m_pConfig->primary_buffer);
    SetDlgItemInt(IDC_SOUND_BUF1E, m_pConfig->primary_buffer * 10);
    spin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF2S);
    spin->SetRange(1,100); spin->SetPos(m_pConfig->polling_buffer);
    SetDlgItemInt(IDC_SOUND_BUF2E, m_pConfig->polling_buffer);
    int maximum = 100; m_nMasterOrg = m_pSound->GetMasterVol(maximum);
    SoundSlider(this, IDC_VOL_VOLS, IDC_VOL_VOLN, m_nMasterOrg, maximum);
    SoundSlider(this, IDC_VOL_MASTERS, IDC_VOL_MASTERN, m_pSound->GetVolume());
    m_nMIDIOrg = m_pMIDI->GetOutVolume();
    SoundSlider(this, IDC_VOL_SEPS, IDC_VOL_SEPN, m_nMIDIOrg,65535);
    CheckDlgButton(IDC_TG_BACKGROUND,m_pConfig->sound_global_focus);
    m_nTimerID = SetTimer(IDD_SOUNDPAGE,100,NULL); OnSelChange(); return TRUE;
}
void CSoundPage::EnableControls(BOOL enabled) {
    for (int i = 0; i < 5; ++i) GetDlgItem(IDC_SOUND_RATE0+i)->EnableWindow(enabled);
}
void CSoundPage::OnSelChange() {
    BOOL enabled = ((CComboBox*)GetDlgItem(IDC_SOUND_DEVICEC))->GetCurSel() > 0;
    EnableControls(enabled);
    if (enabled && !GetCheckedRadioButton(IDC_SOUND_RATE0,IDC_SOUND_RATE4)) CheckRadioButton(IDC_SOUND_RATE0,IDC_SOUND_RATE4,IDC_SOUND_RATE4);
}
void CSoundPage::OnVScroll(UINT, UINT pos, CScrollBar *bar) {
    const int id = bar->GetDlgCtrlID();
    if (id == IDC_SOUND_BUF1S) SetDlgItemInt(IDC_SOUND_BUF1E, pos * 10);
    if (id == IDC_SOUND_BUF2S) SetDlgItemInt(IDC_SOUND_BUF2E, pos);
}
void CSoundPage::OnHScroll(UINT, UINT, CScrollBar *bar) {
    const int id = bar->GetDlgCtrlID(), pos = ((CSliderCtrl*)bar)->GetPos();
    if (id == IDC_VOL_VOLS) m_pSound->SetMasterVol(pos);
    if (id == IDC_VOL_MASTERS) { ::LockVM(); m_pSound->SetVolume(pos); ::UnlockVM(); SetDlgItemInt(IDC_VOL_MASTERN,pos); }
    if (id == IDC_VOL_SEPS) m_pMIDI->SetOutVolume(pos);
    OnTimer(m_nTimerID);
}
void CSoundPage::OnTimer(UINT_PTR) {
    int maximum = 100, value = m_pSound->GetMasterVol(maximum);
    SoundSlider(this, IDC_VOL_VOLS,IDC_VOL_VOLN,value,maximum);
    SoundSlider(this, IDC_VOL_SEPS,IDC_VOL_SEPN,m_pMIDI->GetOutVolume(),65535);
}
void CSoundPage::OnOK() {
    if (m_nTimerID) { KillTimer(m_nTimerID); m_nTimerID=0; }
    const int device = ((CComboBox*)GetDlgItem(IDC_SOUND_DEVICEC))->GetCurSel();
    if (!device) m_pConfig->sample_rate=0;
    else { m_pConfig->sound_device=device-1; m_pConfig->sample_rate=GetCheckedRadioButton(IDC_SOUND_RATE0,IDC_SOUND_RATE4)-IDC_SOUND_RATE0+1; }
    const int input = ((CComboBox*)GetDlgItem(IDC_TG_INPUT))->GetCurSel();
    m_pConfig->sound_input=input > 0 ? input-1 : 16;
    m_pConfig->primary_buffer=LOWORD(((CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF1S))->GetPos());
    m_pConfig->polling_buffer=LOWORD(((CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF2S))->GetPos());
    m_pConfig->master_volume=((CSliderCtrl*)GetDlgItem(IDC_VOL_MASTERS))->GetPos();
    if (m_nMIDIOrg>=0) m_pConfig->midi_volume=((CSliderCtrl*)GetDlgItem(IDC_VOL_SEPS))->GetPos();
    m_pConfig->sound_global_focus=IsDlgButtonChecked(IDC_TG_BACKGROUND)!=0;
    CConfigPage::OnOK();
}
void CSoundPage::OnCancel() {
    if (m_nTimerID) { KillTimer(m_nTimerID); m_nTimerID=0; }
    ::LockVM(); m_pSound->SetVolume(m_pConfig->master_volume); ::UnlockVM();
    if (m_nMasterOrg>=0) m_pSound->SetMasterVol(m_nMasterOrg);
    if (m_nMIDIOrg>=0) m_pMIDI->SetOutVolume(m_nMIDIOrg);
    CConfigPage::OnCancel();
}

CVolPage::CVolPage() : m_pSound(NULL), m_pOPMIF(NULL), m_pADPCM(NULL) {
    m_dwID=MAKEID('V','O','L',' '); m_nTemplate=IDD_VOLPAGE; m_uHelpID=IDC_VOL_HELP;
}
BEGIN_MESSAGE_MAP(CVolPage,CConfigPage)
    ON_WM_HSCROLL()
    ON_BN_CLICKED(IDC_VOL_FMC,OnFMCheck)
    ON_BN_CLICKED(IDC_VOL_ADPCMC,OnADPCMCheck)
END_MESSAGE_MAP()
BOOL CVolPage::OnInitDialog() {
    CConfigPage::OnInitDialog(); CFrmWnd *frame=ResolveFrmWnd(this); if (!frame) return FALSE;
    m_pSound=frame->GetSound();
    m_pOPMIF=(OPMIF*)::GetVM()->SearchDevice(MAKEID('O','P','M',' '));
    m_pADPCM=(ADPCM*)::GetVM()->SearchDevice(MAKEID('A','P','C','M'));
    SoundSlider(this,IDC_VOL_FMS,IDC_VOL_FMN,m_pConfig->fm_volume);
    SoundSlider(this,IDC_VOL_ADPCMS,IDC_VOL_ADPCMN,m_pConfig->adpcm_volume);
    CheckDlgButton(IDC_VOL_FMC,m_pConfig->fm_enable); CheckDlgButton(IDC_VOL_ADPCMC,m_pConfig->adpcm_enable);
    CheckDlgButton(IDC_TG_FM_BUSY,m_pConfig->fm_write_busy);
    CheckDlgButton(IDC_TG_FM_INTERP,m_pConfig->fm_interp);
    CheckDlgButton(IDC_TG_ADPCM_INTERP,m_pConfig->adpcm_interp);
    CheckDlgButton(IDC_TG_ADPCM_ANALOG,m_pConfig->adpcm_analog);
    CheckDlgButton(IDC_TG_ADPCM_LPF,m_pConfig->adpcm_lpf);
    CComboBox *combo=(CComboBox*)GetDlgItem(IDC_TG_FM_ENGINE); combo->AddString(_T("fmgen"));
#if defined(XM6CORE_ENABLE_YMFM)
    combo->AddString(_T("ymfm"));
#endif
    combo->SetCurSel(m_pConfig->fm_engine < combo->GetCount() ? m_pConfig->fm_engine : 0);
    return TRUE;
}
void CVolPage::OnHScroll(UINT,UINT,CScrollBar *bar) {
    const int id=bar->GetDlgCtrlID(),pos=((CSliderCtrl*)bar)->GetPos(); ::LockVM();
    if (id==IDC_VOL_FMS) m_pSound->SetFMVol(pos); else if (id==IDC_VOL_ADPCMS) m_pSound->SetADPCMVol(pos);
    ::UnlockVM(); SetDlgItemInt(id==IDC_VOL_FMS ? IDC_VOL_FMN : IDC_VOL_ADPCMN,pos);
}
void CVolPage::OnFMCheck() { ::LockVM();m_pOPMIF->EnableFM(IsDlgButtonChecked(IDC_VOL_FMC)!=0);::UnlockVM(); }
void CVolPage::OnADPCMCheck() { ::LockVM();m_pADPCM->EnableADPCM(IsDlgButtonChecked(IDC_VOL_ADPCMC)!=0);::UnlockVM(); }
void CVolPage::OnOK() {
    m_pConfig->fm_enable=IsDlgButtonChecked(IDC_VOL_FMC)!=0;
    m_pConfig->adpcm_enable=IsDlgButtonChecked(IDC_VOL_ADPCMC)!=0;
    m_pConfig->fm_volume=((CSliderCtrl*)GetDlgItem(IDC_VOL_FMS))->GetPos();
    m_pConfig->adpcm_volume=((CSliderCtrl*)GetDlgItem(IDC_VOL_ADPCMS))->GetPos();
    m_pConfig->fm_write_busy=IsDlgButtonChecked(IDC_TG_FM_BUSY)!=0;
    m_pConfig->fm_interp=IsDlgButtonChecked(IDC_TG_FM_INTERP)!=0;
    m_pConfig->adpcm_interp=IsDlgButtonChecked(IDC_TG_ADPCM_INTERP)!=0;
    m_pConfig->adpcm_analog=IsDlgButtonChecked(IDC_TG_ADPCM_ANALOG)!=0;
    m_pConfig->adpcm_lpf=IsDlgButtonChecked(IDC_TG_ADPCM_LPF)!=0;
    m_pConfig->fm_engine=((CComboBox*)GetDlgItem(IDC_TG_FM_ENGINE))->GetCurSel();
    CConfigPage::OnOK();
}
void CVolPage::OnCancel() {
    ::LockVM();m_pOPMIF->EnableFM(m_pConfig->fm_enable);m_pADPCM->EnableADPCM(m_pConfig->adpcm_enable);
    m_pSound->SetFMVol(m_pConfig->fm_volume);m_pSound->SetADPCMVol(m_pConfig->adpcm_volume);::UnlockVM();
    CConfigPage::OnCancel();
}
