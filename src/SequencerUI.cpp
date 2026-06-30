#include "SequencerUI.h"
#include <set>
#include <cmath>
#include <cstdlib>
#include <algorithm>
using std::min;
using std::max;

SequencerUI::SequencerUI(Sequencer seqs[], int numSeqs, NoteResolver& resolver, HarmonyEngine& harmony)
    : seqs_(seqs), numSeqs_(numSeqs), resolver_(resolver), harmony_(harmony)
{
}

void SequencerUI::drawText(const string& s, float x, float y) const {
    if(jbFont_) jbFont_->drawString(s, x, y);
    else        ofDrawBitmapString(s, x, y);
}

void SequencerUI::drawText14(const string& s, float x, float y) const {
    if(jbFont14_) jbFont14_->drawString(s, x, y);
    else          ofDrawBitmapString(s, x, y);
}

void SequencerUI::drawText12(const string& s, float x, float y) const {
    if(jbFont12_) jbFont12_->drawString(s, x, y);
    else          ofDrawBitmapString(s, x, y);
}

const StepUIData& SequencerUI::getStepData(int step) const {
    static StepUIData def;
    if(step < 0 || step >= TOTAL_STEPS) return def;
    return stepData_[step];
}

int SequencerUI::resolveNote(int step) {
    if(step < 0 || step >= TOTAL_STEPS) return 60;
    const StepUIData& d = stepData_[step];
    int base = (d.noteOverride >= 0) ? d.noteOverride : resolver_.resolveNote(step);
    return ofClamp(base + d.octShift * 12, 9, 120);
}

bool SequencerUI::isSkip(int step) const {
    if(step < 0 || step >= TOTAL_STEPS) return false;
    return stepData_[step].mode == StepUIData::SKIP;
}

string SequencerUI::noteNameStr(int midi) const {
    if(midi < 0 || midi > 127) return "---";
    const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int oct = (midi / 12) - 1;
    return string(names[midi % 12]) + ofToString(oct);
}

void SequencerUI::getStepRect(int idx, float startY, float& ox, float& oy) const {
    int col = idx % COLS, row = idx / COLS;
    ox = originX_ + gridPadX_ + col * (stepW_ + gapX_);
    oy = startY    + gridPadY_ + row * (stepH_ + gapY_);
}

int SequencerUI::stepAtPos(int mx, int my, float startY) const {
    for(int i = 0; i < TOTAL_STEPS; i++) {
        float sx, sy; getStepRect(i, startY, sx, sy);
        if(mx >= sx && mx <= sx + stepW_ - 4 && my >= sy && my <= sy + stepH_ - 4) return i;
    }
    return -1;
}

void SequencerUI::cycleStepMode(int idx) {
    StepUIData& d = stepData_[idx];
    switch(d.mode) {
        case StepUIData::ON:   d.mode = StepUIData::OFF;  break;
        case StepUIData::OFF:  d.mode = StepUIData::SKIP; break;
        case StepUIData::SKIP: d.mode = StepUIData::ON;   break;
    }
    for(int v = 0; v < voiceCount_; v++) {
        if(voices_[v].active && idx >= voices_[v].startStep && idx <= voices_[v].endStep && v < numSeqs_)
            seqs_[v].setGate(idx - voices_[v].startStep, d.mode == StepUIData::ON);
    }
}

int SequencerUI::voiceAtStep(int step) const {
    for(int v = 0; v < voiceCount_; v++) {
        if(voices_[v].active && step >= voices_[v].startStep && step <= voices_[v].endStep) return v;
    }
    return -1;
}

bool SequencerUI::rangeOverlaps(int s, int e) const {
    for(int v = 0; v < voiceCount_; v++) {
        if(!voices_[v].active) continue;
        if(s <= voices_[v].endStep && e >= voices_[v].startStep) return true;
    }
    return false;
}

bool SequencerUI::rangeOverlapsExcept(int s, int e, int excludeVoice) const {
    for(int v = 0; v < voiceCount_; v++) {
        if(!voices_[v].active || v == excludeVoice) continue;
        if(s <= voices_[v].endStep && e >= voices_[v].startStep) return true;
    }
    return false;
}

bool SequencerUI::isSoundSetInUse(int setIdx, int exceptVoice) const {
    for(int v = 0; v < voiceCount_; v++) {
        if(v == exceptVoice || !voices_[v].active) continue;
        if(voices_[v].oscIndex == setIdx) return true;
    }
    return false;
}

int SequencerUI::findAvailableSoundSet(int preferredSet, int exceptVoice) const {
    preferredSet = ofClamp(preferredSet, 0, MAX_VOICES - 1);
    if(!isSoundSetInUse(preferredSet, exceptVoice)) return preferredSet;
    for(int i = 0; i < MAX_VOICES; i++) {
        if(!isSoundSetInUse(i, exceptVoice)) return i;
    }
    return preferredSet;
}

int SequencerUI::stepAvailableSoundSet(int currentSet, int delta, int exceptVoice) const {
    int dir = delta >= 0 ? 1 : -1;
    for(int n = 1; n <= MAX_VOICES; n++) {
        int next = (currentSet + dir * n + MAX_VOICES * 2) % MAX_VOICES;
        if(!isSoundSetInUse(next, exceptVoice)) return next;
    }
    return currentSet;
}

void SequencerUI::addVoiceRange(int startStep, int endStep) {
    if(voiceCount_ >= MAX_VOICES) return;
    if(rangeOverlaps(startStep, endStep)) return;
    int v = voiceCount_;
    voices_[v].startStep = startStep;
    voices_[v].endStep   = endStep;
    voices_[v].oscIndex  = findAvailableSoundSet(v % MAX_VOICES, v);
    voices_[v].active    = true;
    voices_[v].panelOpen = false;
    voices_[v].resizing  = false;
    voices_[v].playing   = true;
    if(v < numSeqs_) {
        seqs_[v].setLoopRange(0, endStep - startStep);
    }
    voiceCount_++;
}

void SequencerUI::removeVoice(int voiceIdx) {
    if(voiceIdx < 0 || voiceIdx >= voiceCount_) return;
    for(int s = voices_[voiceIdx].startStep; s <= voices_[voiceIdx].endStep; s++){
        if(s >= 0 && s < TOTAL_STEPS){
            stepData_[s].noteOverride = -1;
            stepData_[s].chordNoteCount = 0;
            for(int p=0;p<7;p++) stepData_[s].chordNotes[p] = -1;
            stepData_[s].chordName = "";
            stepData_[s].octDelta = 0;
        }
    }
    if(voiceIdx < numSeqs_) seqs_[voiceIdx].stop();
    for(int i = voiceIdx; i < voiceCount_ - 1; i++) {
        voices_[i] = voices_[i+1];
    }
    voices_[voiceCount_-1] = VoiceRange();
    voiceCount_--;
}

void SequencerUI::applyVoiceCommonToSteps(int voiceIdx) {
    if(voiceIdx < 0 || voiceIdx >= voiceCount_) return;
    VoiceRange& vr = voices_[voiceIdx];
    VoiceCommonParam& cp = vr.common;
    for(int i = vr.startStep; i <= vr.endStep; i++) {
        if(!stepData_[i].lockVelocity)   stepData_[i].velocity   = cp.velocity;
        if(!stepData_[i].lockGate)       stepData_[i].gate        = cp.gate;
        if(!stepData_[i].lockProb)       stepData_[i].prob        = cp.prob;
        if(!stepData_[i].lockGridDiv)    stepData_[i].gridDiv     = cp.gridDiv;
        if(!stepData_[i].lockNoteRepeat) stepData_[i].noteRepeat  = cp.noteRepeat;
        if(!stepData_[i].lockGlide)      stepData_[i].glide       = cp.glide;
        if(!stepData_[i].lockOctShift)   stepData_[i].octShift    = cp.octShift;
        if(!stepData_[i].lockNote and cp.noteOverride >= 0){
            stepData_[i].noteOverride = cp.noteOverride;
            stepData_[i].chordName = "";
            stepData_[i].chordNoteCount = 0;
            for(int p=0;p<7;p++) stepData_[i].chordNotes[p] = -1;
            stepData_[i].octDelta = 0;
        }
    }
}

void SequencerUI::applyVoiceScaleToSteps(int voiceIdx) {
    if(voiceIdx < 0 || voiceIdx >= voiceCount_) return;
    VoiceCommonParam& cp = voices_[voiceIdx].common;
    auto notes = harmony_.getAvailableNotes(cp.scaleName, cp.scaleRoot);
    std::vector<int> octaveNotes;
    int startMidi = cp.pianoOctave * 12;
    for(int n : notes) {
        if(n >= startMidi) octaveNotes.push_back(n);
    }
    if(octaveNotes.empty()) return;
    int ni = 0;
    for(int st = voices_[voiceIdx].startStep; st <= voices_[voiceIdx].endStep; st++) {
        if(st < 0 || st >= TOTAL_STEPS) continue;
        StepUIData& d = stepData_[st];
        d.noteOverride = octaveNotes[ni++] % 128;
        d.chordName = "";
        d.chordNoteCount = 0;
        for(int p = 0; p < 7; p++) d.chordNotes[p] = -1;
        if(ni >= (int)octaveNotes.size()) ni = 0;
    }
}

bool SequencerUI::isScaleNote(int midi) const {
    return isScaleNote(midi, -1);
}
bool SequencerUI::isScaleNote(int midi, int voiceIdx) const {
    std::string scaleName = "naturalMinor";
    int scaleRoot = 0;
    if(voiceIdx >= 0 && voiceIdx < voiceCount_) {
        scaleName = voices_[voiceIdx].common.scaleName;
        scaleRoot = voices_[voiceIdx].common.scaleRoot;
    }
    auto notes = harmony_.getAvailableNotes(scaleName, scaleRoot);
    int pc = midi % 12;
    for(int n : notes) { if(n % 12 == pc) return true; }
    return false;
}

static void setWaveRawWithRemainingLimit(int raw[4], int idx, int value) {
    int otherTotal = 0;
    for(int i = 0; i < 4; i++) {
        if(i != idx) otherTotal += raw[i];
    }
    raw[idx] = ofClamp(value, 0, max(0, 100 - otherTotal));
}

static float soundParamDragStep(int paramIdx) {
    if((paramIdx >= 0 && paramIdx <= 3) || (paramIdx >= 10 && paramIdx <= 13)) return 1.0f;
    switch(paramIdx) {
        case 4: case 14: return 0.25f;
        case 5: case 7: case 15: case 17: return 1.0f;
        case 20: return 80.0f;
        case 22: return 0.01f;
        case 24: case 25: return 0.01f;
        case 27: return 0.02f;
        case 28: return 0.1f;
        case 30: case 31: return 0.05f;
        default: return 0.01f;
    }
}

static float soundParamWheelStep(int paramIdx) {
    if((paramIdx >= 0 && paramIdx <= 3) || (paramIdx >= 10 && paramIdx <= 13)) return 1.0f;
    switch(paramIdx) {
        case 4: case 14: return 1.0f;
        case 5: case 7: case 15: case 17: return 1.0f;
        case 6: case 8: case 9:
        case 16: case 18: case 19:
        case 21: case 23:
        case 26: case 29:
        case 32: case 33: case 34: return 0.01f;
        case 20: return 100.0f;
        case 22: return 0.01f;
        case 24: case 25: case 27: return 0.01f;
        case 28: return 0.1f;
        case 30: case 31: return 1.0f;
        default: return 0.01f;
    }
}

float SequencerUI::getSoundParamValue(int setIdx, int paramIdx) const {
    if(setIdx < 0 || setIdx >= MAX_VOICES) return 0.0f;
    const SoundSetUIData& s = soundSets_[setIdx];
    if(paramIdx >= 0 && paramIdx <= 3) return (float)s.mainWave[paramIdx];
    if(paramIdx >= 10 && paramIdx <= 13) return (float)s.main2Wave[paramIdx - 10];
    switch(paramIdx) {
        case 4: return s.tune;
        case 5: return s.fine;
        case 6: return s.level;
        case 7: return s.subDetune;
        case 8: return s.subLevel;
        case 9: return s.subDelay;
        case 14: return s.tune2;
        case 15: return s.fine2;
        case 16: return s.level2;
        case 17: return s.sub2Detune;
        case 18: return s.sub2Level;
        case 19: return s.sub2Delay;
        case 20: return s.filterCutoff;
        case 21: return s.filterResonance;
        case 22: return s.filterEnvAmt;
        case 23: return s.filterKeyTrk;
        case 24: return s.envAttack;
        case 25: return s.envDecay;
        case 26: return s.envSustain;
        case 27: return s.envRelease;
        case 28: return s.lfoRate;
        case 29: return s.lfoDepth;
        case 30: return s.lfoWave;
        case 31: return s.lfoTarget;
        case 32: return s.reverbRoom;
        case 33: return s.reverbDamp;
        case 34: return s.reverbWet;
        default: return 0.0f;
    }
}

void SequencerUI::setSoundParamValue(int setIdx, int paramIdx, float value) {
    if(setIdx < 0 || setIdx >= MAX_VOICES) return;
    SoundSetUIData& s = soundSets_[setIdx];
    if(paramIdx >= 0 && paramIdx <= 3) {
        setWaveRawWithRemainingLimit(s.mainWave, paramIdx, (int)round(value));
        return;
    }
    if(paramIdx >= 10 && paramIdx <= 13) {
        setWaveRawWithRemainingLimit(s.main2Wave, paramIdx - 10, (int)round(value));
        return;
    }
    switch(paramIdx) {
        case 4: s.tune = ofClamp(value, -24.0f, 24.0f); break;
        case 5: s.fine = ofClamp(value, -100.0f, 100.0f); break;
        case 6: s.level = ofClamp(value, 0.0f, 1.0f); break;
        case 7: s.subDetune = ofClamp(value, -100.0f, 100.0f); break;
        case 8: s.subLevel = ofClamp(value, 0.0f, 1.0f); break;
        case 9: s.subDelay = ofClamp(value, 0.0f, 1.0f); break;
        case 14: s.tune2 = ofClamp(value, -24.0f, 24.0f); break;
        case 15: s.fine2 = ofClamp(value, -100.0f, 100.0f); break;
        case 16: s.level2 = ofClamp(value, 0.0f, 1.0f); break;
        case 17: s.sub2Detune = ofClamp(value, -100.0f, 100.0f); break;
        case 18: s.sub2Level = ofClamp(value, 0.0f, 1.0f); break;
        case 19: s.sub2Delay = ofClamp(value, 0.0f, 1.0f); break;
        case 20: s.filterCutoff = ofClamp(value, 20.0f, 20000.0f); break;
        case 21: s.filterResonance = ofClamp(value, 0.0f, 1.0f); break;
        case 22: s.filterEnvAmt = ofClamp(value, -1.0f, 1.0f); break;
        case 23: s.filterKeyTrk = ofClamp(value, 0.0f, 1.0f); break;
        case 24: s.envAttack = ofClamp(value, 0.001f, 5.0f); break;
        case 25: s.envDecay = ofClamp(value, 0.001f, 5.0f); break;
        case 26: s.envSustain = ofClamp(value, 0.0f, 1.0f); break;
        case 27: s.envRelease = ofClamp(value, 0.001f, 10.0f); break;
        case 28: s.lfoRate = ofClamp(value, 0.1f, 20.0f); break;
        case 29: s.lfoDepth = ofClamp(value, 0.0f, 1.0f); break;
        case 30: s.lfoWave = ofClamp((float)round(value), 0.0f, 2.0f); break;
        case 31: s.lfoTarget = ofClamp((float)round(value), 0.0f, 2.0f); break;
        case 32: s.reverbRoom = ofClamp(value, 0.0f, 1.0f); break;
        case 33: s.reverbDamp = ofClamp(value, 0.0f, 1.0f); break;
        case 34: s.reverbWet = ofClamp(value, 0.0f, 1.0f); break;
    }
}

bool SequencerUI::soundParamAtPos(int x, int y, int voiceIdx, int& setIdx, int& paramIdx) const {
    if(voiceIdx < 0 || voiceIdx >= voiceCount_) return false;
    setIdx = voices_[voiceIdx].oscIndex;
    paramIdx = -1;

    float dx, contentY, halfW, halfH;
    getPanelArea(dx, contentY, halfW, halfH);
    float pad = 8.0f;
    float lowerX = dx + pad;
    float lowerY = contentY + halfH + 18.0f;
    float lowerBottom = uiY_ - 8.0f;
    float lowerH = max(0.0f, lowerBottom - lowerY);
    if(lowerH <= 150.0f || x < lowerX || y < lowerY || y > lowerBottom) return false;

    auto hit = [&](float cx, float cy) -> bool {
        float ddx = (float)x - cx;
        float ddy = (float)y - cy;
        return ddx * ddx + ddy * ddy <= 20.0f * 20.0f;
    };

    float lowerW = halfW * 2.0f - pad * 2.0f;
    float gap = 8.0f;
    if(voices_[voiceIdx].soundPage == 0) {
        float subW = 210.0f;
        float oscW = (lowerW - subW - gap * 2.0f) / 2.0f - 12.0f;
        oscW = max(198.0f, oscW);
        float oscH = lowerH;
        float subH = (oscH - 8.0f) / 2.0f;
        float leftKnobX = 24.0f;
        float rightKnobX = oscW * 0.5f + 24.0f;
        float rowTop = 54.0f;
        float rowBottom = oscH - 72.0f;
        float rowGap = (rowBottom - rowTop) / 3.0f;
        float rows[4] = {rowTop, rowTop + rowGap, rowTop + rowGap * 2.0f, rowBottom};
        float subRightKnobX = subW * 0.5f + 24.0f;
        float subRows[2] = {subH * 0.50f, subH * 0.82f};

        float osc1X = lowerX;
        int p1[7] = {0,1,2,3,4,5,6};
        float kx[7] = {leftKnobX,rightKnobX,leftKnobX,rightKnobX,leftKnobX,rightKnobX,leftKnobX};
        float ky[7] = {rows[0],rows[0],rows[1],rows[1],rows[2],rows[2],rows[3]};
        for(int i = 0; i < 7; i++) {
            if(hit(osc1X + kx[i], lowerY + ky[i])) { paramIdx = p1[i]; return true; }
        }

        float subX = lowerX + oscW + gap;
        if(hit(subX + leftKnobX, lowerY + subRows[0])) { paramIdx = 7; return true; }
        if(hit(subX + subRightKnobX, lowerY + subRows[0])) { paramIdx = 8; return true; }
        if(hit(subX + leftKnobX, lowerY + subRows[1])) { paramIdx = 9; return true; }
        float sub2Y = lowerY + subH + gap;
        if(hit(subX + leftKnobX, sub2Y + subRows[0])) { paramIdx = 17; return true; }
        if(hit(subX + subRightKnobX, sub2Y + subRows[0])) { paramIdx = 18; return true; }
        if(hit(subX + leftKnobX, sub2Y + subRows[1])) { paramIdx = 19; return true; }

        float osc2X = subX + subW + gap;
        int p2[7] = {10,11,12,13,14,15,16};
        for(int i = 0; i < 7; i++) {
            if(hit(osc2X + kx[i], lowerY + ky[i])) { paramIdx = p2[i]; return true; }
        }
    } else {
        float modGap = 8.0f;
        float modW = 150.0f;
        float kx = 24.0f;
        float fourGap = 47.0f;
        float fourStart = (lowerH - fourGap * 3.0f) / 2.0f + 18.0f;
        float rows[4] = {fourStart, fourStart + fourGap, fourStart + fourGap * 2.0f, fourStart + fourGap * 3.0f};
        const int moduleParams[4][4] = {
            {20,21,22,23},
            {24,25,26,27},
            {28,29,30,31},
            {32,33,34,-1}
        };
        for(int m = 0; m < 4; m++) {
            float mx = lowerX + m * (modW + modGap);
            for(int r = 0; r < 4; r++) {
                if(moduleParams[m][r] < 0) continue;
                if(hit(mx + kx, lowerY + rows[r])) {
                    paramIdx = moduleParams[m][r];
                    return true;
                }
            }
        }
    }
    return false;
}

int SequencerUI::voiceBadgeAtPos(int mx, int my, float startY) const {
    for(int v = 0; v < voiceCount_; v++) {
        if(!voices_[v].active) continue;
        float rx, ry;
        getGroupRowRect(v, rx, ry);
        float rowH, groupW, oscX, oscW, playX2, playW2;
        float multX[5], multW[5], delX, delW;
        getGroupRowLayout(v, ry, rowH, groupW, oscX, oscW, playX2, playW2, multX, multW, delX, delW);
        if(mx >= playX2 && mx <= delX + delW && my >= ry && my <= ry + rowH) return v;
    }
    return -1;
}

int SequencerUI::voiceTagAtPos(int mx, int my, float startY) const {
    for(int v = 0; v < voiceCount_; v++) {
        if(!voices_[v].active) continue;
        string label = "Grp" + ofToString(v + 1);
        float th14 = jbFont14_ ? jbFont14_->stringHeight("Ag") : 14.0f;
        float tw14 = jbFont14_ ? jbFont14_->stringWidth(label) : (float)label.size() * 8.4f;
        float tagW = tw14 + 12.0f;
        float tagH = th14 + 4.0f;
        int s = voices_[v].startStep;
        int e = voices_[v].endStep;
        for(int i = s; i <= e; ) {
            int rowStart = i;
            int rowEnd = min(e, (i / COLS + 1) * COLS - 1);
            float sx, sy;
            getStepRect(rowStart, startY, sx, sy);
            float tagX = sx + 2.0f;
            float tagY = sy - tagH / 2.0f;
            if(mx >= tagX && mx <= tagX + tagW && my >= tagY && my <= tagY + tagH) return v;
            i = rowEnd + 1;
        }
    }
    return -1;
}

int SequencerUI::voiceEdgeAtPos(int mx, int my, float startY, bool& isLeftEdge) const {
    for(int v = 0; v < voiceCount_; v++) {
        if(!voices_[v].active) continue;
        float sx, sy; getStepRect(voices_[v].startStep, startY, sx, sy);
        float ex, ey; getStepRect(voices_[v].endStep,   startY, ex, ey);
        if(my < sy || my > sy + stepH_ - 4) continue;
        float leftX  = sx;
        float rightX = ex + stepW_ - 4;
        if(mx >= leftX - 6 && mx <= leftX + 8)   { isLeftEdge = true;  return v; }
        if(mx >= rightX - 8 && mx <= rightX + 6) { isLeftEdge = false; return v; }
    }
    return -1;
}

void SequencerUI::getPanelArea(float& dx, float& contentY, float& halfW, float& halfH) const {
    float leftListW = SQUARE_W * 2.0f / 7.0f;
    dx       = originX_ + leftListW;
    float dw = SQUARE_W * 5.0f / 7.0f;
    float th12 = jbFont12_ ? jbFont12_->stringHeight("Ag") : 12.0f;
    float rowH12 = th12 + 8.0f;
    float groupH = 3.0f * (rowH12 + 6.0f) + 4.0f;
    float transportH = 40.0f;
    float contentH = MONITOR_H - transportH - groupH;
    contentY = monitorTop_ + transportH;
    halfW = dw / 2.0f;
    halfH = contentH / 2.0f;
}

void SequencerUI::getVoicePanelRect(int voiceIdx, float& px, float& py, float& pw, float& ph) const {
    pw = 230; ph = 480;
    float bsx, bsy; getStepRect(voices_[voiceIdx].startStep, uiY_, bsx, bsy);
    px = bsx + voices_[voiceIdx].panelOffsetX;
    py = bsy + stepH_ + 4 + voices_[voiceIdx].panelOffsetY;
}

void SequencerUI::getGroupRowRect(int voiceIdx, float& rx, float& ry) const {
    float th   = jbFont12_ ? jbFont12_->stringHeight("Ag") : 12.0f;
    float rowH = th + 8.0f;
    int rows = voiceCount_;
    float listTop = uiY_ - 2.0f - rows * (rowH + 6.0f);
    int row = voiceIdx;
    rx = originX_ + 8.0f;
    ry = listTop + row * (rowH + 6.0f);
}

void SequencerUI::getGroupRowLayout(int voiceIdx, float& ry, float& rowH,
                                     float& groupW, float& oscX, float& oscW,
                                     float& playX2, float& playW2,
                                     float multX[5], float multW[5],
                                     float& delX, float& delW) const {
    float rx; getGroupRowRect(voiceIdx, rx, ry);

    float th  = jbFont12_ ? jbFont12_->stringHeight("Ag") : 12.0f;
    float padX = 6.0f;
    float gap  = 6.0f;
    rowH = th + 8.0f;

    const char* multLabels[] = {"x.5","x1","x2","x3","x4"};

    auto sw = [&](const string& s) -> float {
        return jbFont12_ ? jbFont12_->stringWidth(s) : (float)s.size()*7.2f;
    };

    groupW = rowH;

    oscX = rx + groupW + gap;
    oscW = 0.0f;

    playX2 = oscX;
    playW2 = sw("STOP") + padX*2.0f;

    float curX = playX2 + playW2 + gap;
    for(int m=0;m<5;m++){
        multW[m] = sw(multLabels[m]) + padX*2.0f;
        multX[m] = curX;
        curX += multW[m] + 2.0f;
    }

    delX = curX + gap;
    delW = sw("DEL") + padX*2.0f;
}

// ========== draw ==========

void SequencerUI::draw(float originX, float monitorTop) {
    originX_    = originX;
    monitorTop_ = monitorTop;
    uiY_        = monitorTop_ + MONITOR_H;

    drawMainMonitorChrome();
    drawGlobalTransport();
    drawVoiceRanges(uiY_);
    drawGrid(uiY_);
    drawVoiceBadges(uiY_);
    if(panelVoice_ >= 0) drawVoiceMainPanel(panelVoice_);
    else if(panelStep_ >= 0) drawPanel(panelStep_);

    if(groupGesture_ != GroupGesture::NONE && gestureAnchor_ >= 0 && gestureLive_ >= 0) {
        bool isCreate = (groupGesture_ == GroupGesture::CREATE);
        int s = min(gestureAnchor_, gestureLive_);
        int e = max(gestureAnchor_, gestureLive_);
        for(int i = s; i <= e; i++) {
            if(isCreate && voiceAtStep(i) >= 0) continue;
            float sx, sy; getStepRect(i, uiY_, sx, sy);
            ofSetColor(colPink_.r, colPink_.g, colPink_.b, 60); ofFill();
            ofDrawRectangle(sx, sy, stepW_-4, stepH_-4);
        }
        for(int i = s; i <= e; ) {
            int rowStart = i, rowEnd = min(e, (i/COLS+1)*COLS-1);
            float sx0,sy0,sx1,sy1;
            getStepRect(rowStart,uiY_,sx0,sy0); getStepRect(rowEnd,uiY_,sx1,sy1);
            ofSetColor(colPink_); ofNoFill(); ofSetLineWidth(2);
            ofDrawRectangle(sx0,sy0,sx1-sx0+stepW_-4,stepH_-4); ofFill();
            i = rowEnd + 1;
        }
    }
}

void SequencerUI::drawMainMonitorChrome() {
    float leftListW = SQUARE_W * 2.0f / 7.0f;
    float menuW = 176.0f;
    float menuX = originX_ + leftListW / 2.0f - menuW / 2.0f;
    float menuY = monitorTop_ + 56.0f;
    float rowH = 30.0f;

    ofSetColor(colBg_); ofFill();
    ofDrawRectangle(originX_, monitorTop_, SQUARE_W, MONITOR_H);

    ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectangle(originX_, monitorTop_, leftListW, MONITOR_H);
    ofFill();

    drawPanelTabButton(menuX, menuY, "PIANO ROLL", panelShowPiano_);
    drawPanelTabButton(menuX, menuY + rowH, "PARAMETER", panelShowParameter_);
}

void SequencerUI::drawPanelTabButton(float x, float y, const char* label, bool active) {
    const float menuW = 176.0f;
    const float rowH = 30.0f;
    float th12 = jbFont12_ ? jbFont12_->stringHeight("Ag") : 12.0f;
    const char* state = active ? "ON" : "OFF";
    float sw = jbFont12_ ? jbFont12_->stringWidth(state) : (float)strlen(state) * 7.2f;
    float baseline = y + rowH / 2.0f + th12 / 2.0f - 2.0f;
    bool hover = ofGetMouseX() >= x && ofGetMouseX() <= x + menuW &&
                 ofGetMouseY() >= y && ofGetMouseY() <= y + rowH;

    ofSetColor(active ? colDim_ : colBg_);
    ofFill();
    ofDrawRectangle(x, y, menuW, rowH);
    ofSetColor(hover ? colYellow_ : colCyan_);
    ofNoFill();
    ofSetLineWidth(1);
    ofDrawRectangle(x, y, menuW, rowH);
    ofFill();
    if(active){
        ofDrawRectangle(x, y, 3.0f, rowH);
    }
    drawText12(label, x + 12.0f, baseline);
    drawText12(state, x + menuW - sw - 12.0f, baseline);
    ofDrawLine(x, y + rowH, x + menuW, y + rowH);
    ofSetLineWidth(1);
}

void SequencerUI::getTransportLayout(float& playX, float& playW, float& bpmX, float& bpmW,
                                      float& volX, float& volW, float& rowY, float& rowH,
                                      float& iconCx, float& iconCy) const {
    float leftListW = SQUARE_W * 2.0f / 7.0f;
    float detailX   = originX_ + leftListW;
    float x0   = detailX + 8.0f;
    float padX = 8.0f;
    float gap  = 8.0f;

    string playLabel = globalPlaying_ ? "GO" : "STOP";
    string bpmLabel  = "BPM "+ofToString((int)globalBpm_);
    string volLabel  = "VOL "+ofToString((int)(globalVolume_*100))+"%";

    float tw1 = jbFont14_ ? jbFont14_->stringWidth("STOP") : (float)string("STOP").size()*8.0f;
    float tw2 = jbFont14_ ? jbFont14_->stringWidth(bpmLabel)  : (float)bpmLabel.size()*8.0f;
    float tw3 = jbFont14_ ? jbFont14_->stringWidth(volLabel)  : (float)volLabel.size()*8.0f;
    float th  = jbFont14_ ? jbFont14_->stringHeight("Ag")     : 14.0f;

    rowY = monitorTop_ + 8.0f;
    rowH = th + 8.0f;

    playW = tw1 + padX*2.0f;
    bpmW  = tw2 + padX*2.0f;
    volW  = tw3 + padX*2.0f;

    playX = x0;
    bpmX  = playX + playW + gap;
    volX  = bpmX  + bpmW  + gap;

    iconCx = volX + volW + 30.0f;
    iconCy = rowY + rowH/2.0f;
}

void SequencerUI::drawGlobalTransport() {
    float playX,playW,bpmX,bpmW,volX,volW,rowY,rowH,iconCx,iconCy;
    getTransportLayout(playX,playW,bpmX,bpmW,volX,volW,rowY,rowH,iconCx,iconCy);
    int mx = ofGetMouseX();
    int my = ofGetMouseY();
    auto hoverRect = [&](float x, float y, float w, float h) -> bool {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    };

    string playLabel = globalPlaying_ ? "GO" : "STOP";
    string bpmLabel  = "BPM "+ofToString((int)globalBpm_);
    string volLabel  = "VOL "+ofToString((int)(globalVolume_*100))+"%";

    bool playHover = hoverRect(playX,rowY,playW,rowH);
    ofSetColor(playHover ? colYellow_ : (globalPlaying_ ? colCyan_ : colPink_)); ofFill();
    ofDrawRectRounded(playX, rowY, playW, rowH, 3);
    ofSetColor(20);
    {
        float tw = jbFont14_ ? jbFont14_->stringWidth(playLabel) : (float)playLabel.size()*8.0f;
        drawText14(playLabel, playX + (playW - tw) / 2.0f, rowY+rowH-7.0f);
    }

    bool bpmHover = hoverRect(bpmX,rowY,bpmW,rowH);
    ofSetColor(bpmHover ? colYellow_ : colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectangle(bpmX, rowY, bpmW, rowH); ofFill();
    ofSetColor(bpmHover ? colYellow_ : colCyan_);
    drawText14(bpmLabel, bpmX+8.0f, rowY+rowH-7.0f);

    bool volHover = hoverRect(volX,rowY,volW,rowH);
    ofSetColor(volHover ? colYellow_ : colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectangle(volX, rowY, volW, rowH); ofFill();
    ofSetColor(volHover ? colYellow_ : colCyan_);
    drawText14(volLabel, volX+8.0f, rowY+rowH-7.0f);

    float idx = mx - iconCx;
    float idy = my - iconCy;
    bool iconHover = idx*idx + idy*idy <= 14.0f*14.0f;
    drawIconChan(iconCx, iconCy, 14.0f, iconHover ? colYellow_ : colCyan_);
}

void SequencerUI::drawIconChan(float cx, float cy, float r, ofColor fillCol) {
    static const float mouthPts[][2] = {
        { 4.127f,-1.428f},{ 4.663f,-0.770f},{ 5.059f,-0.041f},{ 5.306f, 0.737f},
        { 5.385f, 1.538f},{ 5.306f, 2.339f},{ 5.059f, 3.117f},{ 4.663f, 3.846f},
        { 4.127f, 4.504f},{ 3.461f, 5.072f},{ 2.6925f,5.537f},{ 1.842f, 5.972f},
        { 0.935f, 6.083f},{ 0.000f, 6.153f},{-0.935f, 6.083f},{-1.842f, 5.972f},
        {-2.3075f,5.537f},{-3.461f, 5.072f},{-4.127f, 4.504f},{-4.663f, 3.846f},
        {-5.059f, 3.117f},{-5.306f, 2.339f},{-5.385f, 1.538f},{-5.306f, 0.737f},
        {-5.059f,-0.041f},{-4.663f,-0.770f},{-4.127f,-1.428f}
    };
    float s = r / 10.0f;
    ofSetColor(fillCol); ofFill(); ofDrawCircle(cx, cy, r);
    ofSetColor(colBg_); ofFill();
    ofDrawCircle(cx - 3.846f*s, cy - 2.308f*s, 1.923f*s);
    ofDrawCircle(cx + 3.846f*s, cy - 2.308f*s, 1.923f*s);
    ofNoFill(); ofSetLineWidth(1.538f*s);
    ofBeginShape();
    for(auto& p : mouthPts) ofVertex(cx + p[0]*s, cy + p[1]*s);
    ofEndShape(false);
    ofFill();
}

void SequencerUI::drawVoiceRanges(float startY) {
    static const ofColor groupCols[6] = {
        ofColor::fromHex(0x6BE4FF), ofColor::fromHex(0xFF6B9D), ofColor::fromHex(0xE1FF00),
        ofColor::fromHex(0x39FF14), ofColor::fromHex(0xFF6A00), ofColor::fromHex(0xBF00FF)
    };
    for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].active) continue;
        int s=voices_[v].startStep, e=voices_[v].endStep;
        ofColor vc=groupCols[v%6];
        for(int i=s;i<=e;){
            int rowStart=i, rowEnd=min(e,(i/COLS+1)*COLS-1);
            float sx,sy,ex2,ey2;
            getStepRect(rowStart,startY,sx,sy); getStepRect(rowEnd,startY,ex2,ey2);
            ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(2);
            ofDrawRectangle(sx-4, sy, (ex2-sx)+stepW_-2+8, stepH_-4);
            ofFill();
            {
                string label = "Grp"+ofToString(v+1);
                float th14 = jbFont14_ ? jbFont14_->stringHeight("Ag") : 14.0f;
                float tw14 = jbFont14_ ? jbFont14_->stringWidth(label)  : (float)label.size()*8.4f;
                float tagW = tw14 + 12.0f, tagH = th14 + 4.0f;
                float tagX = sx+2.0f, tagY = sy - tagH/2.0f;
                bool tagHover = ofGetMouseX() >= tagX && ofGetMouseX() <= tagX + tagW &&
                                ofGetMouseY() >= tagY && ofGetMouseY() <= tagY + tagH;
                ofSetColor(colBg_); ofFill();
                ofDrawRectangle(tagX, tagY, tagW, tagH);
                ofSetColor(tagHover ? colYellow_ : vc);
                drawText14(label, tagX+6.0f, tagY+tagH-4.0f);
            }
            i = rowEnd + 1;
        }
    }
}

void SequencerUI::drawVoiceBadges(float startY) {
    const float multVals[] = {0.5f,1.0f,2.0f,3.0f,4.0f};
    const char* multLabels[] = {"x.5","x1","x2","x3","x4"};
    float th  = jbFont12_ ? jbFont12_->stringHeight("Ag") : 12.0f;
    float textY_offset = th + 3.0f;

    for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].active) continue;
        float rx,ry; getGroupRowRect(v,rx,ry);
        float rowH2,groupW,oscX,oscW,playX2,playW2;
        float multX[5],multW[5],delX,delW;
        getGroupRowLayout(v,ry,rowH2,groupW,oscX,oscW,playX2,playW2,multX,multW,delX,delW);
        int mx=ofGetMouseX(), my=ofGetMouseY();

        ofSetColor(colCyan_);
        ofFill();
        ofDrawRectangle(rx,ry,groupW,rowH2);
        ofSetColor(colBg_);
        {
            string groupNum=ofToString(v+1);
            float nw=jbFont12_?jbFont12_->stringWidth(groupNum):(float)groupNum.size()*7.2f;
            drawText12(groupNum,rx+(groupW-nw)/2.0f,ry+textY_offset);
        }

        if(oscW>0.0f){
            bool oscHover=mx>=oscX&&mx<=oscX+oscW&&my>=ry&&my<=ry+rowH2;
            ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
            if(oscHover) ofSetColor(colYellow_);
            ofDrawRectangle(oscX, ry, oscW, rowH2); ofFill();
            ofSetColor(oscHover?colYellow_:colCyan_);
            drawText12("OSC"+ofToString(voices_[v].oscIndex+1), oscX+6, ry+textY_offset);
        }

        bool isPlaying = voices_[v].playing;
        bool playHover=mx>=playX2&&mx<=playX2+playW2&&my>=ry&&my<=ry+rowH2;
        ofSetColor(playHover ? colYellow_ : (isPlaying ? colCyan_ : colPink_)); ofFill();
        ofDrawRectRounded(playX2, ry, playW2, rowH2, 3);
        ofSetColor(20);
        {
            string pl = isPlaying ? "GO" : "STOP";
            float tw = jbFont12_ ? jbFont12_->stringWidth(pl) : (float)pl.size()*7.2f;
            drawText12(pl, playX2 + (playW2 - tw) / 2.0f, ry+textY_offset);
        }

        for(int m=0;m<5;m++){
            bool act=(voices_[v].common.bpmMult==multVals[m]);
            bool multHover=mx>=multX[m]&&mx<=multX[m]+multW[m]&&my>=ry&&my<=ry+rowH2;
            if(act){ ofSetColor(colYellow_); ofFill(); }
            else    { ofSetColor(multHover?colYellow_:colCyan_);  ofNoFill(); ofSetLineWidth(1); }
            ofDrawRectRounded(multX[m], ry, multW[m], rowH2, 2);
            ofFill();
            ofSetColor(act?ofColor(20):(multHover?colYellow_:colCyan_));
            drawText12(multLabels[m], multX[m]+6, ry+textY_offset);
        }

        if(voices_[v].delPending){
            bool delHover=mx>=delX&&mx<=delX+delW&&my>=ry&&my<=ry+rowH2;
            ofSetColor(delHover?colYellow_:ofColor(200,40,40)); ofFill();
            ofDrawRectRounded(delX, ry, delW, rowH2, 3);
            ofSetColor(255);
            drawText12("DEL", delX+6, ry+textY_offset);
        }
    }
}

void SequencerUI::drawGrid(float startY) {
    for(int i=0;i<TOTAL_STEPS;i++){
        float sx,sy; getStepRect(i,startY,sx,sy);
        drawStep(i,sx,sy);
    }
}

void SequencerUI::drawStep(int idx, float x, float y) {
    StepUIData& d=stepData_[idx];
    bool isCurrent=false;
    for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].active||v>=numSeqs_) continue;
        if(voices_[v].startStep+seqs_[v].getCurrentStep()==idx){isCurrent=true;break;}
    }
    bool isSelected = (panelStep_==idx);

    float cx = x + stepW_/2.0f - 2.0f;
    float cy = y + stepH_/2.0f;
    float r  = min(stepW_, stepH_) * 0.40f;

    float menuY = cy + r + 4.0f;
    float menuHalfW = 7.0f;
    ofSetColor(colCyan_);
    ofSetLineWidth(4);
    ofDrawLine(cx - menuHalfW, menuY, cx + menuHalfW, menuY);
    ofSetLineWidth(1);

    {
        ofSetColor(colBg_); ofFill(); ofDrawCircle(cx, cy, r);
        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(2); ofDrawCircle(cx, cy, r); ofFill();
        if(isCurrent){
            // グロー(外側・半透明)
            ofSetColor(colPink_.r, colPink_.g, colPink_.b, 60); ofFill();
            ofDrawCircle(cx, cy, r*0.85f);
            ofSetColor(colPink_.r, colPink_.g, colPink_.b, 100); ofFill();
            ofDrawCircle(cx, cy, r*0.78f);
            // 本体(r×0.7塗りつぶし)
            ofSetColor(colPink_); ofFill();
            ofDrawCircle(cx, cy, r*0.7f);
        }
        if(isSelected){
            ofSetColor(colYellow_); ofNoFill(); ofSetLineWidth(2);
            ofDrawCircle(cx, cy, r*0.62f); ofFill();
        }

        if(d.mode==StepUIData::OFF){
            ofSetColor(colPink_); ofSetLineWidth(3);
            ofDrawLine(cx-r*0.7f, cy-r*0.7f, cx+r*0.7f, cy+r*0.7f);
        } else if(d.mode==StepUIData::SKIP){
            ofSetColor(colPink_); ofSetLineWidth(3);
            ofDrawLine(cx-r*0.7f, cy-r*0.7f, cx+r*0.7f, cy+r*0.7f);
            ofDrawLine(cx+r*0.7f, cy-r*0.7f, cx-r*0.7f, cy+r*0.7f);
        }
        ofSetLineWidth(1);
    }


    if(showVelocity_) drawVelocityBar(idx,x,y);
}

void SequencerUI::drawVelocityBar(int idx, float x, float y) {
    StepUIData& d=stepData_[idx];
    float barW=stepW_-12,barMaxH=14,barH=barMaxH*(d.velocity/127.0f);
    float barX=x+4,barY=y+stepH_-4-barMaxH;
    ofSetColor(40); ofFill(); ofDrawRectangle(barX,barY,barW,barMaxH);
    ofSetColor(colRed_); ofDrawRectangle(barX,barY+barMaxH-barH,barW,barH);
}

void SequencerUI::drawMiniPiano(float x, float y, float w, float h, int stepIdx, bool isVoicePanel, int voiceIdx) {
    int selectedNote = -1;
    int octave = pianoOctave_;
    if(isVoicePanel && voiceIdx >= 0) {
        selectedNote = voices_[voiceIdx].common.noteOverride;
        octave = voices_[voiceIdx].common.pianoOctave;
    } else {
        if(stepIdx >= 0 && stepIdx < TOTAL_STEPS)
            selectedNote = stepData_[stepIdx].noteOverride;
    }
    int rootMidi = octave * 12;
    float keyW = w / 12.0f;
    float usedW = keyW * 7.0f;
    const int whiteNotes[] = {0, 2, 4, 5, 7, 9, 11};
    const int blackNotes[] = {1, 3, 6, 8, 10};
    const int blackSlots[] = {0, 1, 3, 4, 5};
    float blackH = h * 0.6f;

    ofSetColor(colCyan_);
    ofNoFill();
    ofSetLineWidth(1);
    for(int i=0;i<7;i++){
        float kx = x + i * keyW;
        ofDrawRectangle(kx, y, keyW, h);
        int midi = rootMidi + whiteNotes[i];
        if(selectedNote == midi){
            ofSetLineWidth(2);
            ofDrawRectangle(kx + 2.0f, y + 2.0f, keyW - 4.0f, h - 4.0f);
            ofSetLineWidth(1);
        }
    }
    for(int i=0;i<5;i++){
        float kx = x + (blackSlots[i] + 1.0f) * keyW - keyW * 0.5f;
        ofSetColor(colBg_);
        ofFill();
        ofDrawRectangle(kx + 1.0f, y + 1.0f, keyW - 2.0f, blackH - 2.0f);
        ofSetColor(colCyan_);
        ofNoFill();
        ofSetLineWidth(1);
        ofDrawLine(kx, y, kx, y + blackH);
        ofDrawLine(kx + keyW, y, kx + keyW, y + blackH);
        ofDrawLine(kx, y + blackH, kx + keyW, y + blackH);
        ofDrawLine(kx + 1.0f, y, kx + keyW - 1.0f, y);
        int midi = rootMidi + blackNotes[i];
        if(selectedNote == midi){
            ofSetLineWidth(2);
            ofDrawRectangle(kx + 2.0f, y + 2.0f, keyW - 4.0f, blackH - 4.0f);
            ofSetLineWidth(1);
        }
    }
    ofNoFill();
    ofDrawRectangle(x,y,usedW,h);
    ofFill();
    ofSetColor(colGold_);
    ofDrawBitmapString("<",x-10,y+h/2+4);
    ofDrawBitmapString(">",x+usedW+2,y+h/2+4);
}

// ========== VOICEパネル ==========

void SequencerUI::drawVoicePanel(int voiceIdx) {
    if(voiceIdx<0||voiceIdx>=voiceCount_) return;
    VoiceRange& vr=voices_[voiceIdx];
    VoiceCommonParam& cp=vr.common;
    float px,py,pw,ph; getVoicePanelRect(voiceIdx,px,py,pw,ph);
    ofColor vc[]={ofColor(200,60,60),ofColor(60,160,200),ofColor(60,200,120),
                  ofColor(200,160,60),ofColor(160,60,200),ofColor(200,100,60)};
    ofColor col=vc[voiceIdx%6];
    ofSetColor(22); ofFill(); ofDrawRectRounded(px,py,pw,ph,5);
    ofSetColor(col); ofNoFill(); ofSetLineWidth(2); ofDrawRectRounded(px,py,pw,ph,5); ofFill();
    ofSetColor(col); ofDrawBitmapString("VOICE "+ofToString(voiceIdx+1)+" COMMON",px+8,py+15);
    ofSetColor(colWhite_); ofDrawBitmapString("(overrides all steps in range)",px+8,py+27);
    float cx=px+8, cy=py+38;
    ofSetColor(colWhite_); ofDrawBitmapString("PITCH",cx,cy+10);
    string noteName=(cp.noteOverride>=0)?noteNameStr(cp.noteOverride):"AUTO";
    ofSetColor(colGold_);
    ofDrawBitmapString(noteName,cx+50,cy+10);
    ofDrawBitmapString("OCT"+ofToString(cp.pianoOctave),cx+110,cy+10);
    ofSetColor(colGold_); ofFill(); ofDrawRectRounded(cx+160,cy-2,36,14,2);
    ofSetColor(20); ofDrawBitmapString("APPLY",cx+162,cy+9);
    drawMiniPiano(cx,cy+14,pw-16,38,-1,true,voiceIdx);
    float sliderX=cx+48,sliderW=pw-60,sliderH=12;
    float vy=cy+62,gy=vy+22,pry=gy+22;
    ofSetColor(colWhite_); ofDrawBitmapString("VEL",cx,vy+10);
    ofSetColor(45); ofFill(); ofDrawRectangle(sliderX,vy,sliderW,sliderH);
    ofSetColor(colRed_); ofDrawRectangle(sliderX,vy,sliderW*(cp.velocity/127.0f),sliderH);
    ofSetColor(0,0,0,150); ofFill(); ofDrawRectangle(sliderX+sliderW/2-20,vy,40,sliderH);
    ofSetColor(colGold_); ofDrawBitmapString(ofToString(cp.velocity),sliderX+sliderW/2-18,vy+sliderH-2);
    ofSetColor(colWhite_); ofDrawBitmapString("GATE",cx,gy+10);
    ofSetColor(45); ofFill(); ofDrawRectangle(sliderX,gy,sliderW,sliderH);
    ofSetColor(colRed_); ofDrawRectangle(sliderX,gy,sliderW*cp.gate,sliderH);
    ofSetColor(0,0,0,150); ofFill(); ofDrawRectangle(sliderX+sliderW/2-20,gy,40,sliderH);
    ofSetColor(colGold_); ofDrawBitmapString(ofToString(cp.gate,2),sliderX+sliderW/2-18,gy+sliderH-2);
    ofSetColor(colWhite_); ofDrawBitmapString("PROB",cx,pry+10);
    ofSetColor(45); ofFill(); ofDrawRectangle(sliderX,pry,sliderW,sliderH);
    ofSetColor(colRed_); ofDrawRectangle(sliderX,pry,sliderW*cp.prob,sliderH);
    ofSetColor(0,0,0,150); ofFill(); ofDrawRectangle(sliderX+sliderW/2-20,pry,40,sliderH);
    ofSetColor(colGold_); ofDrawBitmapString(ofToString((int)(cp.prob*100))+"%",sliderX+sliderW/2-18,pry+sliderH-2);
    float gry=pry+26;
    ofSetColor(colWhite_); ofDrawBitmapString("GRID",cx,gry+10);
    for(int g=1;g<=8;g++){
        float gbx=cx+48+(g-1)*18; bool act=(cp.gridDiv==g);
        ofSetColor(act?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(gbx,gry,16,16,2);
        ofSetColor(act?ofColor(20):colGold_); ofDrawBitmapString(ofToString(g),gbx+4,gry+12);
    }
    float nry=gry+22;
    ofSetColor(colWhite_); ofDrawBitmapString("REPT",cx,nry+10);
    for(int r=1;r<=8;r++){
        float rbx=cx+48+(r-1)*18; bool act=(cp.noteRepeat==r);
        ofSetColor(act?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(rbx,nry,16,16,2);
        ofSetColor(act?ofColor(20):colGold_); ofDrawBitmapString(ofToString(r),rbx+4,nry+12);
    }
    float gly=nry+22;
    ofSetColor(colWhite_); ofDrawBitmapString("GLIDE",cx,gly+10);
    auto drawToggle=[&](float tx,float ty,string label,bool active){
        ofSetColor(active?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(tx,ty,42,16,2);
        ofSetColor(active?ofColor(20):colGold_); ofDrawBitmapString(label,tx+8,ty+12);
    };
    drawToggle(cx+48,gly,"ON", cp.glide);
    drawToggle(cx+96,gly,"OFF",!cp.glide);
    float ocy=gly+22;
    ofSetColor(colWhite_); ofDrawBitmapString("OCT",cx,ocy+10);
    const int octVals[]={-2,-1,0,1,2};
    for(int o=0;o<5;o++){
        float obx=cx+48+o*28; bool act=(cp.octShift==octVals[o]);
        ofSetColor(act?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(obx,ocy,24,16,2);
        ofSetColor(act?ofColor(20):colGold_);
        string ol=(octVals[o]>0?"+":"")+ofToString(octVals[o]);
        ofDrawBitmapString(ol,obx+3,ocy+12);
    }
    float aby=ocy+26;
    ofSetColor(col); ofFill(); ofDrawRectRounded(cx,aby,pw-16,20,3);
    ofSetColor(20); ofDrawBitmapString("APPLY TO ALL STEPS IN RANGE",cx+8,aby+14);
    float laby=aby+26;
    bool allLk=vr.allLocked;
    ofSetColor(allLk?colRed_:ofColor(60)); ofFill(); ofDrawRectRounded(cx,laby,pw-16,20,3);
    ofSetColor(allLk?ofColor(20):colWhite_); ofDrawBitmapString(allLk?"LOCK ALL (ON)":"LOCK ALL (OFF)",cx+8,laby+14);

    float scy=laby+28;
    ofSetColor(colWhite_); ofDrawBitmapString("SCALE",cx,scy+10);
    ofSetColor(ofColor(200,40,40)); ofFill(); ofDrawRectRounded(cx+pw-26,scy,20,16,3);
    ofSetColor(255); ofDrawBitmapString("X",cx+pw-20,scy+12);
    const char* noteNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    for(int r=0;r<12;r++){
        float rbx=cx+(r%6)*20, rby=scy+(r/6)*20;
        bool sel=(cp.scaleRoot%12==r);
        ofSetColor(sel?colGold_:ofColor(50)); ofFill(); ofDrawRectRounded(rbx,rby+14,18,16,2);
        ofSetColor(sel?ofColor(20):colWhite_); ofDrawBitmapString(noteNames[r],rbx+2,rby+26);
    }
    if(vr.scaleOpen){
        const char* scaleNames[]={"major","lydian","lydianb7","mixolydian",
            "naturalMinor","harmonicMin","melodicMin","dorian","phrygian",
            "phrygianDom","locrian","locrianS2","majorPenta","minorPenta",
            "diminished","augmented","minorBlues","majorBlues","wholeTone",
            "chromatic","bebopMajor","bebopDom"};
        const char* scaleLabels[]={"Major","Lydian","Lydian b7","Mixolydian",
            "Nat Minor","Harm Min","Mel Min","Dorian","Phrygian",
            "Phryg Dom","Locrian","Locrian#2","Maj Penta","Min Penta",
            "Diminish","Augmented","Min Blues","Maj Blues","Whole Tone",
            "Chromatic","Bebop Maj","Bebop Dom"};
        float spx=cx+pw, spy=py+ph-22*22-8;
        ofSetColor(22); ofFill(); ofDrawRectRounded(spx,spy,160,22*22+8,5);
        ofSetColor(colGold_); ofNoFill(); ofSetLineWidth(1); ofDrawRectRounded(spx,spy,160,22*22+8,5); ofFill();
        for(int s=0;s<22;s++){
            bool sel=(cp.scaleName==scaleNames[s]);
            ofSetColor(sel?colGold_:ofColor(40)); ofFill(); ofDrawRectRounded(spx+4,spy+4+s*22,152,20,2);
            ofSetColor(sel?ofColor(20):colWhite_); ofDrawBitmapString(scaleLabels[s],spx+8,spy+18+s*22);
        }
    }
}

void SequencerUI::drawVoiceMainPanel(int voiceIdx) {
    if(voiceIdx < 0 || voiceIdx >= voiceCount_) return;
    VoiceRange& vr = voices_[voiceIdx];
    VoiceCommonParam& cp = vr.common;

    float dx,contentY,halfW,halfH;
    getPanelArea(dx,contentY,halfW,halfH);

    float pad=8.0f;
    float pianoX=dx+pad, pianoY=contentY+pad;
    float pianoW=halfW-pad*2, pianoH=halfH-pad*2;
    float pianoInnerW=pianoW-8.0f;
    float pianoUsedW=pianoInnerW/12.0f*7.0f;
    if(panelShowPiano_){
        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
        ofDrawRectangle(pianoX,pianoY,pianoUsedW+8.0f,pianoH); ofFill();
        drawMiniPiano(pianoX+4,pianoY+4,pianoInnerW,pianoH-8,-1,true,voiceIdx);
    }

    float rX=dx+halfW+pad, rY=contentY+pad, rW=halfW-pad*2;
    float modeH=24.0f;
    float subW=(rW-8.0f)/2.0f;
    float stepInfoW=subW-16.0f;
    float modeW=((rW-8.0f)/3.0f)*0.9f;
    float modeTotalW=modeW*3.0f+8.0f;
    float modeX=rX+rW-modeTotalW;
    float pianoRight=pianoX+pianoUsedW+8.0f;
    float stepGap=max(8.0f,(modeX-pianoRight-stepInfoW)/2.0f);
    float stepInfoX=pianoRight+stepGap;
    float th14=jbFont14_?jbFont14_->stringHeight("Ag"):14.0f;
    float th12=jbFont12_?jbFont12_->stringHeight("Ag"):12.0f;
    float lineH=th14+8.0f;
    int mx=ofGetMouseX(), my=ofGetMouseY();
    auto hoverRect=[&](float x,float y,float w,float h)->bool{
        return mx>=x&&mx<=x+w&&my>=y&&my<=y+h;
    };

    string groupPrefix="Group ";
    string groupNumber=ofToString(voiceIdx+1);
    float groupPrefixW=jbFont14_?jbFont14_->stringWidth(groupPrefix):(float)groupPrefix.size()*8.4f;
    float groupTextX=stepInfoX+(stepInfoW-(groupPrefixW+24.0f))/2.0f;
    ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectRounded(stepInfoX,rY,stepInfoW,modeH,3); ofFill();
    ofSetColor(colCyan_);
    drawText14(groupPrefix,groupTextX,rY+modeH-6.0f);
    drawText14(groupNumber,groupTextX+groupPrefixW,rY+modeH-6.0f);

    float soundY=rY+modeH+4.0f;
    string soundLabel="Sound Set "+ofToString(vr.oscIndex+1);
    float soundTw=jbFont14_?jbFont14_->stringWidth(soundLabel):(float)soundLabel.size()*8.4f;
    bool soundHover=hoverRect(stepInfoX,soundY,stepInfoW,modeH);
    ofSetColor(soundHover?colYellow_:colCyan_);
    ofNoFill();
    ofSetLineWidth(1);
    ofDrawRectRounded(stepInfoX,soundY,stepInfoW,modeH,3);
    ofFill();
    ofSetColor(colYellow_);
    drawText14(soundLabel,stepInfoX+(stepInfoW-soundTw)/2.0f,soundY+modeH-6.0f);

    bool applyHover=hoverRect(modeX,rY,modeTotalW,modeH);
    ofSetColor(applyHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectRounded(modeX,rY,modeTotalW,modeH,3); ofFill();
    ofSetColor(applyHover?colYellow_:colCyan_);
    {
        string applyLabel="APPLY";
        float tw=jbFont14_?jbFont14_->stringWidth(applyLabel):(float)applyLabel.size()*8.4f;
        drawText14(applyLabel,modeX+(modeTotalW-tw)/2.0f,rY+modeH-6.0f);
    }

    float subY=soundY+modeH+8.0f;
    float subH=contentY+halfH-pad-subY;
    float lSubX=stepInfoX;

    if(panelShowPiano_){
        float ly=subY;
        float noteFrameX=lSubX;
        float noteFrameY=subY-2.0f;
        float noteFrameW=subW-16.0f;
        float noteFrameH=lineH*3.0f-2.0f;
        ofSetColor(colCyan_);
        ofNoFill();
        ofSetLineWidth(1);
        ofDrawRectRounded(noteFrameX,noteFrameY,noteFrameW,noteFrameH,3);
        ofFill();

        string noteStr=(cp.noteOverride>=0)?noteNameStr(cp.noteOverride):"AUTO";
        ofSetColor(colCyan_); drawText14("PITCH",lSubX+8.0f,ly+th14);
        ofSetColor(colYellow_); drawText14(noteStr,lSubX+80.0f,ly+th14);
        ly+=lineH;

        ofSetColor(colCyan_); drawText14("OCT",lSubX+8.0f,ly+th14);
        ofSetColor(colYellow_); drawText14(ofToString(cp.pianoOctave),lSubX+80.0f,ly+th14);
        bool octDownHover=hoverRect(lSubX+100,ly,20,lineH-4);
        bool octUpHover=hoverRect(lSubX+124,ly,20,lineH-4);
        ofSetColor(octDownHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1);
        ofDrawRectRounded(lSubX+100,ly,20,lineH-4,3); ofFill();
        ofSetColor(octDownHover?colYellow_:colCyan_); drawText14("<",lSubX+104,ly+th14);
        ofSetColor(octUpHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1);
        ofDrawRectRounded(lSubX+124,ly,20,lineH-4,3); ofFill(); ofSetColor(octUpHover?colYellow_:colCyan_); drawText14(">",lSubX+128,ly+th14);
        ly+=lineH;

        string scaleLabel="SCALE "+cp.scaleName;
        float stw=jbFont14_?jbFont14_->stringWidth(scaleLabel):(float)scaleLabel.size()*8.4f;
        if(stw > subW-24.0f) scaleLabel="SCALE";
        stw=jbFont14_?jbFont14_->stringWidth(scaleLabel):(float)scaleLabel.size()*8.4f;
        float scaleX=lSubX+(subW-16.0f-stw)/2.0f;
        bool scaleHover=hoverRect(scaleX,ly,stw,lineH);
        ofSetColor(scaleHover?colYellow_:(vr.scaleOpen?colPink_:colCyan_));
        drawText14(scaleLabel,scaleX,ly+th14);
        ly+=lineH;

        bool lockHover=hoverRect(lSubX,ly,subW-16,lineH-4);
        ofSetColor(lockHover?colYellow_:(vr.allLocked?colPink_:colCyan_)); ofNoFill(); ofSetLineWidth(1);
        ofDrawRectRounded(lSubX,ly,subW-16,lineH-4,3); ofFill();
        ofSetColor(lockHover?colYellow_:(vr.allLocked?colPink_:colCyan_));
        string lockLabel=vr.allLocked?"LOCK ALL ON":"LOCK ALL OFF";
        float ltw=jbFont14_?jbFont14_->stringWidth(lockLabel):(float)lockLabel.size()*8.4f;
        drawText14(lockLabel,lSubX+(subW-16-ltw)/2.0f,ly+th14);
    }

    if(panelShowParameter_){
        float graphX=modeX, graphW=modeTotalW;
        float graphTop=rY+modeH+26.0f;
        float graphBottom=pianoY+pianoH;
        float labelBaseline=graphBottom-2.0f;
        float barW=18.0f;
        float barGap=(graphW-7.0f*barW)/6.0f;
        if(barGap<6.0f){
            barGap=6.0f;
            barW=(graphW-6.0f*barGap)/7.0f;
        }
        float barY=graphTop+10.0f;
        float barH=max(24.0f,labelBaseline-th12-2.0f-barY);
        float totalBarW=7.0f*(barW+barGap)-barGap;
        float bStartX=graphX+(graphW-totalBarW)/2.0f;

        auto drawVBar=[&](float bx,float val,float maxVal,const char* label){
            bool barHover=hoverRect(bx,barY,barW,barH);
            float filled=ofClamp(val/maxVal,0.0f,1.0f);
            ofSetColor(colBg_); ofFill(); ofDrawRectangle(bx,barY,barW,barH);
            ofSetColor(barHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY,barW,barH); ofFill();
            ofSetColor(barHover?colYellow_:colCyan_); ofFill(); ofDrawRectangle(bx,barY+barH*(1.0f-filled),barW,barH*filled);
            float lw=jbFont12_?jbFont12_->stringWidth(label):(float)strlen(label)*7.2f;
            ofSetColor(colCyan_); drawText12(label,bx+(barW-lw)/2.0f,labelBaseline);
        };
        drawVBar(bStartX,              (float)cp.velocity,127.0f,"VEL");
        drawVBar(bStartX+(barW+barGap),cp.gate,           1.0f,  "GAT");
        drawVBar(bStartX+2*(barW+barGap),cp.prob,         1.0f,  "PRB");

        auto drawStepBar=[&](float bx,int val,int maxVal,const char* label){
            bool barHover=hoverRect(bx,barY,barW,barH);
            float btnH=(barH-((float)(maxVal-1))*2.0f)/(float)maxVal;
            ofSetColor(barHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY,barW,barH); ofFill();
            for(int g=1;g<=maxVal;g++){
                float gy=barY+barH-(g*btnH+(g-1)*2);
                ofSetColor(g==val?colYellow_:colBg_); ofFill(); ofDrawRectangle(bx+1,gy,barW-2,btnH);
            }
            float lw=jbFont12_?jbFont12_->stringWidth(label):(float)strlen(label)*7.2f;
            ofSetColor(colCyan_); drawText12(label,bx+(barW-lw)/2.0f,labelBaseline);
        };
        drawStepBar(bStartX+3*(barW+barGap),cp.gridDiv,  8,"GRD");
        drawStepBar(bStartX+4*(barW+barGap),cp.noteRepeat,8,"RPT");

        auto drawToggleBar=[&](float bx,bool val,const char* label){
            bool barHover=hoverRect(bx,barY,barW,barH);
            float glH=(barH-4)/2.0f;
            float onw=jbFont12_?jbFont12_->stringWidth("ON"):(float)2*7.2f;
            float ofw=jbFont12_?jbFont12_->stringWidth("OF"):(float)2*7.2f;
            ofSetColor(val?colCyan_:colBg_); ofFill(); ofDrawRectangle(bx,barY,barW,glH);
            ofSetColor(barHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY,barW,glH); ofFill();
            ofSetColor(val?ofColor(20):colCyan_); drawText12("ON",bx+(barW-onw)/2.0f,barY+glH/2.0f+th12/2.0f);
            ofSetColor(!val?colCyan_:colBg_); ofFill(); ofDrawRectangle(bx,barY+glH+4,barW,glH);
            ofSetColor(barHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY+glH+4,barW,glH); ofFill();
            ofSetColor(!val?ofColor(20):colCyan_); drawText12("OF",bx+(barW-ofw)/2.0f,barY+glH+4+glH/2.0f+th12/2.0f);
            float lw=jbFont12_?jbFont12_->stringWidth(label):(float)strlen(label)*7.2f;
            ofSetColor(colCyan_); drawText12(label,bx+(barW-lw)/2.0f,labelBaseline);
        };
        drawToggleBar(bStartX+5*(barW+barGap),cp.glide,"GLD");

        auto drawOctBar=[&](float bx){
            bool barHover=hoverRect(bx,barY,barW,barH);
            const int octVals[]={-2,-1,0,1,2};
            float btnH=(barH-4.0f*2.0f)/5.0f;
            ofSetColor(barHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY,barW,barH); ofFill();
            for(int o=0;o<5;o++){
                float oy=barY+barH-((o+1)*btnH+o*2.0f);
                ofSetColor(cp.octShift==octVals[o]?colYellow_:colBg_); ofFill();
                ofDrawRectangle(bx+1.0f,oy,barW-2.0f,btnH);
            }
            float lw=jbFont12_?jbFont12_->stringWidth("OCT"):(float)3*7.2f;
            ofSetColor(barHover?colYellow_:colCyan_); drawText12("OCT",bx+(barW-lw)/2.0f,labelBaseline);
        };
        drawOctBar(bStartX+6*(barW+barGap));
    }

    {
        float lowerX=dx+pad;
        float lowerY=contentY+halfH+18.0f;
        float lowerBottom=uiY_-8.0f;
        float lowerH=max(0.0f,lowerBottom-lowerY);
        if(lowerH>150.0f){
            const SoundSetUIData& ss = soundSets_[vr.oscIndex];
            auto intText=[&](float v)->string{ return ofToString((int)round(v)); };
            auto pctText=[&](float v)->string{ return ofToString((int)round(v*100.0f)); };
            auto norm=[&](float v,float mn,float mx)->float{
                if(mx<=mn) return 0.0f;
                return ofClamp((v-mn)/(mx-mn),0.0f,1.0f);
            };
            auto drawKnob=[&](float cx,float cy,const string& label,const string& value,float amount){
                ofColor valueCol=(amount>0.0f)?colYellow_:colCyan_;
                ofSetColor(valueCol);
                ofNoFill();
                ofSetLineWidth(1);
                ofDrawCircle(cx,cy,14.0f);
                float ang=ofMap(ofClamp(amount,0.0f,1.0f),0.0f,1.0f,-140.0f,140.0f)*DEG_TO_RAD;
                ofDrawLine(cx,cy,cx+cos(ang)*10.0f,cy+sin(ang)*10.0f);
                ofFill();
                ofSetColor(colCyan_);
                drawText12(label,cx+22.0f,cy-5.0f);
                ofSetColor(valueCol);
                drawText12(value,cx+22.0f,cy+16.0f);
            };

            auto drawModule=[&](float x,float y,float w,float h,const char* title){
                ofSetColor(colCyan_);
                ofNoFill();
                ofSetLineWidth(1);
                ofDrawRectangle(x,y,w,h);
                ofFill();
                float tw=jbFont12_?jbFont12_->stringWidth(title):(float)strlen(title)*7.2f;
                float titleW=w-2.0f;
                float titleH=16.0f;
                float titleX=x+1.0f;
                float titleY=y+1.0f;
                ofSetColor(colCyan_);
                ofFill();
                ofDrawRectangle(titleX,titleY,titleW,titleH);
                ofSetColor(colBg_);
                drawText12(title,x+(w-tw)/2.0f,titleY+13.0f);
            };

            auto drawLinkLine=[&](float x1,float y1,float x2,float y2){
                ofSetColor(colCyan_);
                ofNoFill();
                ofSetLineWidth(2);
                ofDrawLine(x1,y1,x2,y2);
                ofFill();
                ofDrawCircle(x1,y1,3.5f);
                ofDrawCircle(x2,y2,3.5f);
                ofSetLineWidth(1);
            };

            float lowerW=halfW*2.0f-pad*2.0f;
            float gap=8.0f;
            float subW=210.0f;
            float oscW=(lowerW-subW-gap*2.0f)/2.0f-12.0f;
            oscW=max(198.0f,oscW);
            float oscH=lowerH;
            float subH=(oscH-8.0f)/2.0f;
            float x=lowerX;
            float y=lowerY;

            float leftKnobX=24.0f;
            float rightKnobX=oscW*0.5f+24.0f;
            float rowTop=54.0f;
            float rowBottom=oscH-72.0f;
            float rowGap=(rowBottom-rowTop)/3.0f;
            float row1=rowTop;
            float row2=rowTop+rowGap;
            float row3=rowTop+rowGap*2.0f;
            float row4=rowBottom;
            float footerY=oscH-28.0f;
            float subRightKnobX=subW*0.5f+24.0f;
            float subKnobY1=subH*0.50f;
            float subKnobY2=subH*0.82f;

            if(vr.soundPage==0){
                drawModule(x,y,oscW,oscH,"OSC 1");
                drawKnob(x+leftKnobX,y+row1,"SAW",intText(ss.mainWave[0]),ss.mainWave[0]/100.0f);
                drawKnob(x+rightKnobX,y+row1,"SQR",intText(ss.mainWave[1]),ss.mainWave[1]/100.0f);
                drawKnob(x+leftKnobX,y+row2,"TRI",intText(ss.mainWave[2]),ss.mainWave[2]/100.0f);
                drawKnob(x+rightKnobX,y+row2,"SIN",intText(ss.mainWave[3]),ss.mainWave[3]/100.0f);
                drawKnob(x+leftKnobX,y+row3,"TUNE",intText(ss.tune),norm(ss.tune,-24.0f,24.0f));
                drawKnob(x+rightKnobX,y+row3,"FINE",intText(ss.fine),norm(ss.fine,-100.0f,100.0f));
                drawKnob(x+leftKnobX,y+row4,"LEVEL",pctText(ss.level),ss.level);
                ofSetColor(colYellow_); drawText12("S 100%",x+50.0f,y+footerY);

                x+=oscW+gap;
                drawModule(x,y,subW,subH,"SUB 1");
                drawKnob(x+leftKnobX,y+subKnobY1,"DETUNE",intText(ss.subDetune),norm(ss.subDetune,-100.0f,100.0f));
                drawKnob(x+subRightKnobX,y+subKnobY1,"LEVEL",pctText(ss.subLevel),ss.subLevel);
                drawKnob(x+leftKnobX,y+subKnobY2,"DELAY",pctText(ss.subDelay),ss.subDelay);
                drawLinkLine(x+12.0f,y+24.0f,x-gap-12.0f,y+24.0f);

                y+=subH+gap;
                drawModule(x,y,subW,subH,"SUB 2");
                drawKnob(x+leftKnobX,y+subKnobY1,"DETUNE",intText(ss.sub2Detune),norm(ss.sub2Detune,-100.0f,100.0f));
                drawKnob(x+subRightKnobX,y+subKnobY1,"LEVEL",pctText(ss.sub2Level),ss.sub2Level);
                drawKnob(x+leftKnobX,y+subKnobY2,"DELAY",pctText(ss.sub2Delay),ss.sub2Delay);

                x+=subW+gap;
                y=lowerY;
                drawLinkLine(x-gap-12.0f,lowerY+subH+gap+subH-20.0f,x+12.0f,lowerY+subH+gap+subH-20.0f);
                drawModule(x,y,oscW,oscH,"OSC 2");
                drawKnob(x+leftKnobX,y+row1,"SAW",intText(ss.main2Wave[0]),ss.main2Wave[0]/100.0f);
                drawKnob(x+rightKnobX,y+row1,"SQR",intText(ss.main2Wave[1]),ss.main2Wave[1]/100.0f);
                drawKnob(x+leftKnobX,y+row2,"TRI",intText(ss.main2Wave[2]),ss.main2Wave[2]/100.0f);
                drawKnob(x+rightKnobX,y+row2,"SIN",intText(ss.main2Wave[3]),ss.main2Wave[3]/100.0f);
                drawKnob(x+leftKnobX,y+row3,"TUNE",intText(ss.tune2),norm(ss.tune2,-24.0f,24.0f));
                drawKnob(x+rightKnobX,y+row3,"FINE",intText(ss.fine2),norm(ss.fine2,-100.0f,100.0f));
                drawKnob(x+leftKnobX,y+row4,"LEVEL",pctText(ss.level2),ss.level2);
                ofSetColor(colYellow_); drawText12("S 100%",x+50.0f,y+footerY);
            } else {
                float modGap=8.0f;
                float modW=150.0f;
                float modH=lowerH;
                float modX=lowerX;
                float kx=24.0f;
                float fourGap=47.0f;
                float fourStart=(modH-fourGap*3.0f)/2.0f+18.0f;
                float kr1=fourStart;
                float kr2=fourStart+fourGap;
                float kr3=fourStart+fourGap*2.0f;
                float kr4=fourStart+fourGap*3.0f;

                drawModule(modX,lowerY,modW,modH,"FILTER");
                drawKnob(modX+kx,lowerY+kr1,"CUT",intText(ss.filterCutoff),norm(ss.filterCutoff,20.0f,20000.0f));
                drawKnob(modX+kx,lowerY+kr2,"RES",pctText(ss.filterResonance),ss.filterResonance);
                drawKnob(modX+kx,lowerY+kr3,"ENV",intText(ss.filterEnvAmt*100.0f),norm(ss.filterEnvAmt,-1.0f,1.0f));
                drawKnob(modX+kx,lowerY+kr4,"KEY",pctText(ss.filterKeyTrk),ss.filterKeyTrk);

                modX+=modW+modGap;
                drawModule(modX,lowerY,modW,modH,"ENV");
                drawKnob(modX+kx,lowerY+kr1,"ATK",intText(ss.envAttack*1000.0f),norm(ss.envAttack,0.001f,5.0f));
                drawKnob(modX+kx,lowerY+kr2,"DEC",intText(ss.envDecay*1000.0f),norm(ss.envDecay,0.001f,5.0f));
                drawKnob(modX+kx,lowerY+kr3,"SUS",pctText(ss.envSustain),ss.envSustain);
                drawKnob(modX+kx,lowerY+kr4,"REL",intText(ss.envRelease*1000.0f),norm(ss.envRelease,0.001f,10.0f));

                modX+=modW+modGap;
                drawModule(modX,lowerY,modW,modH,"LFO");
                drawKnob(modX+kx,lowerY+kr1,"RATE",intText(ss.lfoRate),norm(ss.lfoRate,0.1f,20.0f));
                drawKnob(modX+kx,lowerY+kr2,"DEPTH",pctText(ss.lfoDepth),ss.lfoDepth);
                drawKnob(modX+kx,lowerY+kr3,"WAVE",intText(ss.lfoWave),norm(ss.lfoWave,0.0f,2.0f));
                ofSetColor(colCyan_); drawText12("TARGET",modX+kx+22.0f,lowerY+kr4-8.0f);
                drawText12(intText(ss.lfoTarget),modX+kx+22.0f,lowerY+kr4+13.0f);

                modX+=modW+modGap;
                drawModule(modX,lowerY,modW,modH,"REVERB");
                drawKnob(modX+kx,lowerY+kr1,"ROOM",pctText(ss.reverbRoom),ss.reverbRoom);
                drawKnob(modX+kx,lowerY+kr2,"DAMP",pctText(ss.reverbDamp),ss.reverbDamp);
                drawKnob(modX+kx,lowerY+kr3,"WET",pctText(ss.reverbWet),ss.reverbWet);
            }

            float blink=0.45f+0.55f*(0.5f+0.5f*sin(ofGetElapsedTimef()*2.2f));
            ofSetColor(colCyan_,(int)(255.0f*blink));
            drawText14(vr.soundPage==0?">":"<",lowerX+lowerW-18.0f,lowerY+lowerH/2.0f+th14/2.0f);
        }
    }

    if(panelShowPiano_ && vr.scaleOpen){
        const char* scaleNames[]={"major","lydian","lydianb7","mixolydian",
            "naturalMinor","harmonicMin","melodicMin","dorian","phrygian",
            "phrygianDom","locrian","locrianS2","majorPenta","minorPenta",
            "diminished","augmented","minorBlues","majorBlues","wholeTone",
            "chromatic","bebopMajor","bebopDom"};
        const char* scaleLabels[]={"Major","Lydian","Lydian b7","Mixolydian",
            "Nat Minor","Harm Min","Mel Min","Dorian","Phrygian",
            "Phryg Dom","Locrian","Locrian#2","Maj Penta","Min Penta",
            "Diminish","Augmented","Min Blues","Maj Blues","Whole Tone",
            "Chromatic","Bebop Maj","Bebop Dom"};
        float rowH=18.0f;
        float cpx=modeX, cpy=subY+lineH*2;
        float panelW=modeTotalW;
        float maxPanelH=subH-lineH*2-8.0f;
        float visH=min(24.0f*rowH+8.0f,maxPanelH);
        ofSetColor(22); ofFill(); ofDrawRectangle(cpx,cpy,panelW,visH);
        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(cpx,cpy,panelW,visH); ofFill();
        int visRows=(int)(visH/rowH);
        const char* noteNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        float rootW=panelW/6.0f;
        for(int r=0;r<12;r++){
            float rx=cpx+(r%6)*rootW;
            float ry=cpy+(r/6)*rowH;
            bool sel=(cp.scaleRoot%12)==r;
            bool rootHover=hoverRect(rx,ry,rootW,rowH);
            ofSetColor((sel||rootHover)?colYellow_:colCyan_);
            drawText12(noteNames[r],rx+4.0f,ry+15.0f);
        }
        for(int s=0;s<22 && s<visRows-2;s++){
            bool sel=(cp.scaleName==scaleNames[s]);
            bool scaleRowHover=hoverRect(cpx,cpy+(s+2)*rowH,panelW,rowH);
            ofSetColor((sel||scaleRowHover)?colYellow_:colCyan_);
            drawText12(scaleLabels[s],cpx+8.0f,cpy+15.0f+(s+2)*rowH);
        }
    }
}

// ========== ステップパネル ==========

void SequencerUI::drawPanel(int stepIdx) {
    if(stepIdx<0||stepIdx>=TOTAL_STEPS||panelStep_<0) return;
    StepUIData& d=stepData_[stepIdx];

    float dx,contentY,halfW,halfH;
    getPanelArea(dx,contentY,halfW,halfH);

    float pad=8.0f;
    float pianoX=dx+pad, pianoY=contentY+pad;
    float pianoW=halfW-pad*2, pianoH=halfH-pad*2;
    float pianoInnerW=pianoW-8.0f;
    float pianoUsedW=pianoInnerW/12.0f*7.0f;
    if(panelShowPiano_){
        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
        ofDrawRectangle(pianoX,pianoY,pianoUsedW+8.0f,pianoH); ofFill();
        drawMiniPiano(pianoX+4,pianoY+4,pianoInnerW,pianoH-8,stepIdx,false,voiceAtStep(stepIdx));
    }

    float rX=dx+halfW+pad, rY=contentY+pad, rW=halfW-pad*2;
    float modeH=24.0f;
    float subW=(rW-8.0f)/2.0f;
    float stepInfoW=subW-16.0f;
    float modeW=((rW-8.0f)/3.0f)*0.9f;
    float modeTotalW=modeW*3.0f+8.0f;
    float modeX=rX+rW-modeTotalW;
    float pianoRight=pianoX+pianoUsedW+8.0f;
    float stepGap=max(8.0f,(modeX-pianoRight-stepInfoW)/2.0f);
    float stepInfoX=pianoRight+stepGap;
    string stepPrefix="STEP ";
    string stepNumber=ofToString(stepIdx+1);
    float stepPrefixW=jbFont14_?jbFont14_->stringWidth(stepPrefix):(float)stepPrefix.size()*8.4f;
    float stepTextX=stepInfoX+(stepInfoW-(stepPrefixW+24.0f))/2.0f;
    ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectRounded(stepInfoX,rY,stepInfoW,modeH,3); ofFill();
    ofSetColor(colCyan_);
    drawText14(stepPrefix,stepTextX,rY+modeH-6.0f);
    drawText14(stepNumber,stepTextX+stepPrefixW,rY+modeH-6.0f);

    const char* modeLabels[]={"ON","OFF","SKIP"};
    StepUIData::StepMode modes[]={StepUIData::ON,StepUIData::OFF,StepUIData::SKIP};
    for(int m=0;m<3;m++){
        bool act=(d.mode==modes[m]);
        bool modeHover=ofGetMouseX()>=modeX+m*(modeW+4)&&ofGetMouseX()<=modeX+m*(modeW+4)+modeW&&
                       ofGetMouseY()>=rY&&ofGetMouseY()<=rY+modeH;
        if(act){ ofSetColor(colPink_); ofFill(); }
        else   { ofSetColor(modeHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1); }
        ofDrawRectRounded(modeX+m*(modeW+4),rY,modeW,modeH,3); ofFill();
        ofSetColor(act?ofColor(20):(modeHover?colYellow_:colCyan_));
        float tw=jbFont14_?jbFont14_->stringWidth(modeLabels[m]):(float)strlen(modeLabels[m])*8.4f;
        drawText14(modeLabels[m],modeX+m*(modeW+4)+(modeW-tw)/2.0f,rY+modeH-6.0f);
    }

    float subY=rY+modeH+8.0f;
    float subH=contentY+halfH-pad-subY;
    float lSubX=stepInfoX;
    float th14=jbFont14_?jbFont14_->stringHeight("Ag"):14.0f;
    float lineH=th14+8.0f;
    float th12=jbFont12_?jbFont12_->stringHeight("Ag"):12.0f;
    int mx=ofGetMouseX(), my=ofGetMouseY();
    auto hoverRect=[&](float x,float y,float w,float h)->bool{
        return mx>=x&&mx<=x+w&&my>=y&&my<=y+h;
    };

    auto drawLock14=[&](float lx,float ly,bool locked){
        ofSetColor(locked?colPink_:colCyan_);
        ofNoFill();
        ofSetLineWidth(1.5f);
        ofDrawRectangle(lx-5.0f,ly-1.0f,10.0f,8.0f);
        ofDrawLine(lx-3.0f,ly-1.0f,lx-3.0f,ly-5.0f);
        ofDrawLine(lx+3.0f,ly-1.0f,lx+3.0f,ly-5.0f);
        if(locked){
            ofDrawLine(lx-3.0f,ly-5.0f,lx+3.0f,ly-5.0f);
            ofFill();
            ofDrawRectangle(lx-1.0f,ly+2.0f,2.0f,3.0f);
        } else {
            ofDrawLine(lx+3.0f,ly-5.0f,lx+6.0f,ly-3.0f);
        }
        ofFill();
        ofSetLineWidth(1);
    };

    if(panelShowPiano_){
        float ly=subY;
        float noteFrameX=lSubX;
        float noteFrameY=subY-2.0f;
        float noteFrameW=subW-16.0f;
        float noteFrameH=lineH*3.0f-2.0f;
        ofSetColor(d.lockNote?colPink_:colCyan_);
        ofNoFill();
        ofSetLineWidth(1);
        ofDrawRectRounded(noteFrameX,noteFrameY,noteFrameW,noteFrameH,3);
        ofFill();

        bool hasChord=!d.chordName.empty();
        string noteLabel=hasChord?"ROOT":"NOTE";
        int rootNote=(hasChord&&d.chordNoteCount>0&&d.chordNotes[0]>=0)?d.chordNotes[0]:d.noteOverride;
        string noteStr=(rootNote>=0)?noteNameStr(rootNote):"AUTO";
        float lockIconX=(stepInfoX-(pianoX+pianoUsedW+8.0f))/2.0f+(pianoX+pianoUsedW+8.0f);
        ofSetColor(colCyan_); drawText14(noteLabel,lSubX+8.0f,ly+th14);
        ofSetColor(colYellow_); drawText14(noteStr,lSubX+80.0f,ly+th14);
        drawLock14(lockIconX,ly+th14/2.0f,d.lockNote);
        ly+=lineH;

        ofSetColor(colCyan_); drawText14("OCT",lSubX+8.0f,ly+th14);
        ofSetColor(colYellow_); drawText14(ofToString(pianoOctave_),lSubX+80.0f,ly+th14);
        bool octDownHover=hoverRect(lSubX+100,ly,20,lineH-4);
        bool octUpHover=hoverRect(lSubX+124,ly,20,lineH-4);
        ofSetColor(octDownHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1);
        ofDrawRectRounded(lSubX+100,ly,20,lineH-4,3); ofFill(); ofSetColor(octDownHover?colYellow_:colCyan_); drawText14("<",lSubX+104,ly+th14);
        ofSetColor(octUpHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1);
        ofDrawRectRounded(lSubX+124,ly,20,lineH-4,3); ofFill(); ofSetColor(octUpHover?colYellow_:colCyan_); drawText14(">",lSubX+128,ly+th14);
        ly+=lineH;

        string chordLabel=d.chordName.empty()?"CHORD":d.chordName;
        float ctw=jbFont14_?jbFont14_->stringWidth(chordLabel):(float)chordLabel.size()*8.4f;
        float chordX=lSubX+(subW-16-ctw)/2.0f;
        bool chordHover=hoverRect(chordX,ly,ctw,lineH);
        ofSetColor((!d.chordName.empty()||chordHover)?colYellow_:(d.chordPanelOpen?colPink_:colCyan_));
        drawText14(chordLabel,chordX,ly+th14);
        ly+=lineH;

        int vi2=voiceAtStep(stepIdx); bool allLocked=(vi2>=0)?voices_[vi2].allLocked:false;
        bool lockHover=hoverRect(lSubX,ly,subW-16,lineH-4);
        ofSetColor(lockHover?colYellow_:(allLocked?colPink_:colCyan_)); ofNoFill(); ofSetLineWidth(1);
        ofDrawRectRounded(lSubX,ly,subW-16,lineH-4,3); ofFill();
        ofSetColor(lockHover?colYellow_:(allLocked?colPink_:colCyan_));
        string lockLabel=allLocked?"ALL UNLOCK":"ALL LOCK";
        float ltw=jbFont14_?jbFont14_->stringWidth(lockLabel):(float)lockLabel.size()*8.4f;
        drawText14(lockLabel,lSubX+(subW-16-ltw)/2.0f,ly+th14);
    }

    if(panelShowParameter_){
        float graphX=modeX, graphW=modeTotalW;
        float graphTop=rY+modeH+26.0f;
        float graphBottom=pianoY+pianoH;
        float labelBaseline=graphBottom-2.0f;
        float barW=18.0f;
        float barGap=(graphW-7.0f*barW)/6.0f;
        if(barGap<6.0f){
            barGap=6.0f;
            barW=(graphW-6.0f*barGap)/7.0f;
        }
        float barY=graphTop+10.0f;
        float paramLockY=(rY+modeH+barY)/2.0f;
        float barH=max(24.0f,labelBaseline-th12-2.0f-barY);
        float totalBarW=7.0f*(barW+barGap)-barGap;
        float bStartX=graphX+(graphW-totalBarW)/2.0f;

        auto drawVBar=[&](float bx,float val,float maxVal,bool locked,const char* label){
            bool barHover=hoverRect(bx,barY,barW,barH);
            float filled=val/maxVal;
            ofSetColor(colBg_); ofFill(); ofDrawRectangle(bx,barY,barW,barH);
            ofSetColor(barHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY,barW,barH); ofFill();
            ofSetColor(barHover?colYellow_:(locked?colPink_:colCyan_)); ofFill(); ofDrawRectangle(bx,barY+barH*(1.0f-filled),barW,barH*filled);
            float lw=jbFont12_?jbFont12_->stringWidth(label):(float)strlen(label)*7.2f;
            ofSetColor(colCyan_); drawText12(label,bx+(barW-lw)/2.0f,labelBaseline);
            drawLock14(bx+barW/2.0f,paramLockY,locked);
        };
        drawVBar(bStartX,              (float)d.velocity,127.0f,d.lockVelocity,  "VEL");
        drawVBar(bStartX+(barW+barGap),d.gate,           1.0f,  d.lockGate,      "GAT");
        drawVBar(bStartX+2*(barW+barGap),d.prob,         1.0f,  d.lockProb,      "PRB");

        auto drawStepBar=[&](float bx,int val,int maxVal,bool locked,const char* label){
            bool barHover=hoverRect(bx,barY,barW,barH);
            float btnH=(barH-((float)(maxVal-1))*2.0f)/(float)maxVal;
            ofSetColor(barHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY,barW,barH); ofFill();
            for(int g=1;g<=maxVal;g++){
                float gy=barY+barH-(g*btnH+(g-1)*2);
                ofSetColor(g==val?colYellow_:colBg_); ofFill(); ofDrawRectangle(bx+1,gy,barW-2,btnH);
            }
            float lw=jbFont12_?jbFont12_->stringWidth(label):(float)strlen(label)*7.2f;
            ofSetColor(colCyan_); drawText12(label,bx+(barW-lw)/2.0f,labelBaseline);
            drawLock14(bx+barW/2.0f,paramLockY,locked);
        };
        drawStepBar(bStartX+3*(barW+barGap),d.gridDiv,  8,d.lockGridDiv,  "GRD");
        drawStepBar(bStartX+4*(barW+barGap),d.noteRepeat,8,d.lockNoteRepeat,"RPT");

        auto drawToggleBar=[&](float bx,bool val,bool locked,const char* label){
            bool barHover=hoverRect(bx,barY,barW,barH);
            float glH=(barH-4)/2.0f;
            float onw=jbFont12_?jbFont12_->stringWidth("ON"):(float)2*7.2f;
            float ofw=jbFont12_?jbFont12_->stringWidth("OF"):(float)2*7.2f;
            ofSetColor(val?colCyan_:colBg_); ofFill(); ofDrawRectangle(bx,barY,barW,glH);
            ofSetColor(barHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY,barW,glH); ofFill();
            ofSetColor(val?ofColor(20):colCyan_); drawText12("ON",bx+(barW-onw)/2.0f,barY+glH/2.0f+th12/2.0f);
            ofSetColor(!val?colCyan_:colBg_); ofFill(); ofDrawRectangle(bx,barY+glH+4,barW,glH);
            ofSetColor(barHover?colYellow_:colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY+glH+4,barW,glH); ofFill();
            ofSetColor(!val?ofColor(20):colCyan_); drawText12("OF",bx+(barW-ofw)/2.0f,barY+glH+4+glH/2.0f+th12/2.0f);
            float lw=jbFont12_?jbFont12_->stringWidth(label):(float)strlen(label)*7.2f;
            ofSetColor(colCyan_); drawText12(label,bx+(barW-lw)/2.0f,labelBaseline);
            drawLock14(bx+barW/2.0f,paramLockY,locked);
        };
        drawToggleBar(bStartX+5*(barW+barGap),d.glide,  d.lockGlide,"GLD");
        drawToggleBar(bStartX+6*(barW+barGap),d.strumOn,d.lockStrum,"STR");
    }

    if(panelShowPiano_ && d.chordPanelOpen){
        int vi=voiceAtStep(stepIdx);
        int scaleRoot=(vi>=0)?voices_[vi].common.scaleRoot:0;
        string scaleName=(vi>=0)?voices_[vi].common.scaleName:"naturalMinor";
        int octave=(vi>=0)?voices_[vi].common.pianoOctave:4;
        auto scaleNotes=harmony_.getAvailableNotes(scaleName,scaleRoot);
        std::vector<int> octaveNotes; int startMidi=octave*12;
        for(int n:scaleNotes) if(n>=startMidi&&n<startMidi+12) octaveNotes.push_back(n);
        const char* chordTypes2[]={"maj","min","dim","aug","sus2","sus4","maj7","min7","dom7","dim7","m7b5","pow5","pow5oct"};
        const char* chordLabels2[]={"maj","min","dim","aug","sus2","sus4","maj7","min7","dom7","dim7","m7b5","pow5","p5+8"};
        const char* noteNames2[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        int nTypes=13;
        struct CE2{int rm;string tn,lb;};
        std::vector<CE2> entries2; std::set<int> spc2;
        for(int n:scaleNotes) spc2.insert(n%12);
        const int iv2[13][7]={{0,4,7,-1,-1,-1,-1},{0,3,7,-1,-1,-1,-1},{0,3,6,-1,-1,-1,-1},{0,4,8,-1,-1,-1,-1},{0,2,7,-1,-1,-1,-1},{0,5,7,-1,-1,-1,-1},{0,4,7,11,-1,-1,-1},{0,3,7,10,-1,-1,-1},{0,4,7,10,-1,-1,-1},{0,3,6,9,-1,-1,-1},{0,3,6,10,-1,-1,-1},{0,7,-1,-1,-1,-1,-1},{0,7,12,-1,-1,-1,-1}};
        for(int n:octaveNotes){int rp=n%12;for(int t=0;t<nTypes;t++){bool ok=true;for(int i=0;i<7;i++){if(iv2[t][i]<0)break;if(spc2.find((rp+iv2[t][i])%12)==spc2.end()){ok=false;break;}}if(!ok)continue;entries2.push_back({n,chordTypes2[t],string(noteNames2[n%12])+chordLabels2[t]});}}
        int nEntries=(int)entries2.size();
        int actionRows=2;
        int totalRows=nEntries+actionRows;
        float rowH=18.0f;
        float visH=min((float)(totalRows*rowH+8.0f),subH-lineH*2-8.0f);
        float panelW=modeW;
        float cpx=modeX, cpy=subY+lineH*2;
        ofSetColor(22); ofFill(); ofDrawRectangle(cpx,cpy,panelW,visH);
        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(cpx,cpy,panelW,visH); ofFill();
        int visRows=(int)(visH/rowH);
        float deleteW=jbFont12_?jbFont12_->stringWidth(". DELETE"):(float)8*7.2f;
        float panelOffW=jbFont12_?jbFont12_->stringWidth(". PANEL OFF"):(float)11*7.2f;
        bool deleteHover=hoverRect(cpx+8,cpy,deleteW,rowH);
        bool panelOffHover=hoverRect(cpx+8,cpy+rowH,panelOffW,rowH);
        ofSetColor(deleteHover?colYellow_:colPink_);
        ofDrawBitmapString(". DELETE",cpx+8,cpy+15);
        ofSetColor(panelOffHover?colYellow_:colPink_);
        ofDrawBitmapString(". PANEL OFF",cpx+8,cpy+33);
        int entryRows=max(0,visRows-actionRows);
        int selIdx=0; for(int i=0;i<nEntries;i++) if(d.chordName==entries2[i].lb){selIdx=i;break;}
        int scrollOff=ofClamp(selIdx-entryRows/2,0,max(0,nEntries-entryRows));
        for(int cc=0;cc<nEntries;cc++){
            int vy=cc-scrollOff; if(vy<0||vy>=entryRows) continue;
            int row=vy+actionRows;
            bool sel=(d.chordName==entries2[cc].lb);
            float entryW=jbFont12_?jbFont12_->stringWidth(entries2[cc].lb):(float)entries2[cc].lb.size()*7.2f;
            bool entryHover=hoverRect(cpx+8,cpy+row*rowH,entryW,rowH);
            ofSetColor((sel||entryHover)?colYellow_:colCyan_);
            ofDrawBitmapString(entries2[cc].lb,cpx+8,cpy+15+row*rowH);
        }
    }
}

// ========== mousePressed ==========

void SequencerUI::mousePressed(int x,int y,int button){
    // 全体トランスポート行(START/STOP・BPM・VOL・RESYNC)
    {
        float playX,playW,bpmX,bpmW,volX,volW,rowY,rowH,iconCx,iconCy;
        getTransportLayout(playX,playW,bpmX,bpmW,volX,volW,rowY,rowH,iconCx,iconCy);

        if(x>=playX && x<=playX+playW && y>=rowY && y<=rowY+rowH){
            globalPlaying_=!globalPlaying_;
            return;
        }
        if(x>=bpmX && x<=bpmX+bpmW && y>=rowY && y<=rowY+rowH){
            draggingGlobalBpm_=true; dragStartY_=(float)y; dragStartVal_=globalBpm_;
            return;
        }
        if(x>=volX && x<=volX+volW && y>=rowY && y<=rowY+rowH){
            draggingGlobalVolume_=true; dragStartY_=(float)y; dragStartVal_=globalVolume_;
            return;
        }
        float idx2=x-iconCx, idy2=y-iconCy;
        if(idx2*idx2+idy2*idy2 <= 14.0f*14.0f){
            resetMasterClock_=true;
            return;
        }
    }
    // コードパネル先行判定
    {
        const float menuW = 176.0f;
        const float rowH = 30.0f;
        float leftListW = SQUARE_W * 2.0f / 7.0f;
        float menuX = originX_ + leftListW / 2.0f - menuW / 2.0f;
        float menuY = monitorTop_ + 56.0f;
        if(x>=menuX && x<=menuX+menuW){
            if(y>=menuY && y<=menuY+rowH){ panelShowPiano_=!panelShowPiano_; return; }
            if(y>=menuY+rowH && y<=menuY+rowH*2.0f){ panelShowParameter_=!panelShowParameter_; return; }
        }
    }
    if(false && panelShowParameter_ && panelStep_ >= 0 && stepData_[panelStep_].chordPanelOpen){
        float sx,sy; getStepRect(panelStep_,uiY_,sx,sy);
        float ppw=230;
        float ppx=panelOnRight_?sx+stepW_+2:sx-ppw-2;
        if(ppx+ppw>originX_+SQUARE_W-2) ppx=sx-ppw-2;
        if(ppx<originX_+2) ppx=originX_+2;
        float ppy2=sy;
        if(ppy2+400>monitorTop_+SQUARE_W-10) ppy2=monitorTop_+SQUARE_W-10-400;
        if(ppy2<uiY_+10)  ppy2=uiY_+10;
        float cpx=panelOnRight_?ppx+ppw+4:ppx-204;
        int vi=voiceAtStep(panelStep_);
        int scaleRoot=(vi>=0)?voices_[vi].common.scaleRoot:0;
        string scaleName=(vi>=0)?voices_[vi].common.scaleName:"naturalMinor";
        int octave=(vi>=0)?voices_[vi].common.pianoOctave:4;
        auto scaleNotes=harmony_.getAvailableNotes(scaleName,scaleRoot);
        std::vector<int> octaveNotes;
        int startMidi=octave*12;
        for(int n:scaleNotes) if(n>=startMidi&&n<startMidi+12) octaveNotes.push_back(n);
        const char* chordTypes[]={"maj","min","dim","aug","sus2","sus4","maj7","min7","dom7","dim7","m7b5","pow5","pow5oct"};
        const char* chordLabels2[]={"maj","min","dim","aug","sus2","sus4","maj7","min7","dom7","dim7","m7b5","pow5","p5+8"};
        const char* noteNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        int nTypes=13;
        struct ChordEntry{ int rootMidi; string typeName; string label; };
        std::vector<ChordEntry> entries;
        std::set<int> scalePcSet2;
        for(int n:scaleNotes) scalePcSet2.insert(n%12);
        const int ivTable2[13][7]={
            {0,4,7,-1,-1,-1,-1},{0,3,7,-1,-1,-1,-1},{0,3,6,-1,-1,-1,-1},{0,4,8,-1,-1,-1,-1},
            {0,2,7,-1,-1,-1,-1},{0,5,7,-1,-1,-1,-1},{0,4,7,11,-1,-1,-1},{0,3,7,10,-1,-1,-1},
            {0,4,7,10,-1,-1,-1},{0,3,6,9,-1,-1,-1},{0,3,6,10,-1,-1,-1},
            {0,7,-1,-1,-1,-1,-1},{0,7,12,-1,-1,-1,-1}
        };
        for(int n:octaveNotes){
            int rootPc=n%12;
            for(int t=0;t<nTypes;t++){
                bool valid=true;
                for(int i=0;i<7;i++){
                    if(ivTable2[t][i]<0) break;
                    int pc=(rootPc+ivTable2[t][i])%12;
                    if(scalePcSet2.find(pc)==scalePcSet2.end()){valid=false;break;}
                }
                if(!valid) continue;
                ChordEntry e;
                e.rootMidi=n;
                e.typeName=chordTypes[t];
                e.label=string(noteNames[n%12])+chordLabels2[t];
                entries.push_back(e);
            }
        }
        int nEntries=(int)entries.size();
        float cpanelH2=nEntries*18+8;
        float cpy2=ppy2+400-cpanelH2;
        if(x>=cpx+4&&x<=cpx+196){
            int c=(int)((y-cpy2-4)/18); if(c<0||c>=nEntries){return;}
            if(c>=0&&c<nEntries){
                StepUIData& ds=stepData_[panelStep_];
                ds.chordName=entries[c].label;
                auto cd=harmony_.getChord(entries[c].typeName,entries[c].rootMidi);
                ds.chordNoteCount=0;
                for(int n=0;n<(int)cd.intervals.size()&&n<7;n++){
                    ds.chordNotes[n]=cd.intervals[n];
                    ds.chordNoteCount++;
                }
                return;
            }
        }
    }
    // スケール種類パネル先行判定
    for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].panelOpen||!voices_[v].scaleOpen) continue;
        float px,py,pw,ph; getVoicePanelRect(v,px,py,pw,ph);
        float cx=px+8;
        float spx=cx+pw,spy=py+ph-22*22-8;
        const char* sn[]={"major","lydian","lydianb7","mixolydian","naturalMinor","harmonicMin","melodicMin","dorian","phrygian","phrygianDom","locrian","locrianS2","majorPenta","minorPenta","diminished","augmented","minorBlues","majorBlues","wholeTone","chromatic","bebopMajor","bebopDom"};
        if(x>=spx+4&&x<=spx+156&&y>=spy+4&&y<=spy+4+22*22){
            for(int s=0;s<22;s++){
                if(y>=spy+4+s*22&&y<=spy+24+s*22){
                    voices_[v].common.scaleName=sn[s];
                    int defRoot = voices_[v].common.scaleRoot + 48;
                    auto notes = harmony_.getAvailableNotes(sn[s], defRoot % 12);
                    std::vector<int> filteredNotes;
                    for(int n : notes) if(n >= 48) filteredNotes.push_back(n);
                    notes = filteredNotes;
                    int ni=0;
                    for(int st=voices_[v].startStep;st<=voices_[v].endStep;st++){
                        if(ni<(int)notes.size()){
                            stepData_[st].noteOverride=notes[ni++]%128;
                            stepData_[st].chordName="";
                            stepData_[st].chordNoteCount=0;
                            for(int p=0;p<7;p++) stepData_[st].chordNotes[p]=-1;
                        }
                        if(ni>=(int)notes.size()) ni=0;
                    }
                    return;
                }
            }
        }
    }
    // VOICEパネルドラッグ判定
    for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].panelOpen) continue;
        float px,py,pw,ph; getVoicePanelRect(v,px,py,pw,ph);
        if(x>=px&&x<=px+pw&&y>=py&&y<=py+20){
            panelDragVoice_=v;
            panelDragStartX_=x;
            panelDragStartY_=y;
            panelDragOffsetX_=voices_[v].panelOffsetX;
            panelDragOffsetY_=voices_[v].panelOffsetY;
            return;
        }
    }
    float startY=uiY_;

    // VOICEパネル内クリック
    for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].panelOpen) continue;
        float px,py,pw,ph; getVoicePanelRect(v,px,py,pw,ph);
        bool inScalePanel = voices_[v].scaleOpen && x>=px+pw && x<=px+pw+160;
        if(!inScalePanel && (x<px||x>px+pw||y<py||y>py+ph)) continue;
        VoiceCommonParam& cp=voices_[v].common;
        float cx=px+8, cy=py+38;
        float pix=cx, piw=pw-16;
        if(x>=cx+160&&x<=cx+196&&y>=cy-2&&y<=cy+12){
            auto allNotes=harmony_.getAvailableNotes(voices_[v].common.scaleName, voices_[v].common.scaleRoot);
            std::vector<int> notes;
            int startMidi=voices_[v].common.pianoOctave*12;
            for(int n:allNotes) if(n>=startMidi) notes.push_back(n);
            int ni=0;
            for(int st=voices_[v].startStep;st<=voices_[v].endStep;st++){
                if(ni<(int)notes.size()) stepData_[st].noteOverride=notes[ni++];
                if(ni>=(int)notes.size()) ni=0;
            }
            return;
        }
        if(x>=pix-12&&x<=pix-2&&y>=cy+14&&y<=cy+52){cp.pianoOctave=ofClamp(cp.pianoOctave-1,0,10);return;}
        float voiceKeyW=piw/12.0f;
        float voiceUsedW=voiceKeyW*7.0f;
        if(x>=pix+voiceUsedW+2&&x<=pix+voiceUsedW+12&&y>=cy+14&&y<=cy+52){cp.pianoOctave=ofClamp(cp.pianoOctave+1,0,10);return;}
        if(y>=cy+14&&y<=cy+52&&x>=pix&&x<=pix+voiceUsedW){
            int rootMidi=cp.pianoOctave*12;
            const int whiteNotes[]={0,2,4,5,7,9,11};
            const int blackNotes[]={1,3,6,8,10};
            const int blackSlots[]={0,1,3,4,5};
            int pc=-1;
            if(y<=cy+14+38.0f*0.6f){
                for(int bi=0;bi<5;bi++){
                    float bx=pix+(blackSlots[bi]+1.0f)*voiceKeyW-voiceKeyW*0.5f;
                    if(x>=bx&&x<=bx+voiceKeyW){ pc=blackNotes[bi]; break; }
                }
            }
            if(pc<0){
                int wi=(int)((x-pix)/voiceKeyW);
                if(wi>=0&&wi<7) pc=whiteNotes[wi];
            }
            if(pc>=0){
                int midi=rootMidi+pc;
                if(!isScaleNote(midi, v)) break;
                cp.noteOverride=(cp.noteOverride==midi)?-1:midi;
                applyVoiceCommonToSteps(v);
            }
            return;
        }
        float sliderX=cx+48,sliderW=pw-60,sliderH=12;
        float vy=cy+62,gy=vy+22,pry=gy+22;
        if(y>=vy&&y<=vy+sliderH&&x>=sliderX&&x<=sliderX+sliderW){dragStep_=v;dragType_=0;dragIsVoice_=true;dragVoiceIdx_=v;dragStartY_=y;dragStartVal_=cp.velocity;return;}
        if(y>=gy&&y<=gy+sliderH&&x>=sliderX&&x<=sliderX+sliderW){dragStep_=v;dragType_=1;dragIsVoice_=true;dragVoiceIdx_=v;dragStartY_=y;dragStartVal_=cp.gate;return;}
        if(y>=pry&&y<=pry+sliderH&&x>=sliderX&&x<=sliderX+sliderW){dragStep_=v;dragType_=2;dragIsVoice_=true;dragVoiceIdx_=v;dragStartY_=y;dragStartVal_=cp.prob;return;}
        float gry=pry+26;
        if(y>=gry&&y<=gry+16){for(int g=1;g<=8;g++){float gbx=cx+48+(g-1)*18;if(x>=gbx&&x<=gbx+16){cp.gridDiv=g;applyVoiceCommonToSteps(v);return;}}}
        float nry=gry+22;
        if(y>=nry&&y<=nry+16){for(int r=1;r<=8;r++){float rbx=cx+48+(r-1)*18;if(x>=rbx&&x<=rbx+16){cp.noteRepeat=r;applyVoiceCommonToSteps(v);return;}}}
        float gly=nry+22;
        if(y>=gly&&y<=gly+16){
            if(x>=cx+48&&x<=cx+90){cp.glide=true;applyVoiceCommonToSteps(v);return;}
            if(x>=cx+96&&x<=cx+138){cp.glide=false;applyVoiceCommonToSteps(v);return;}
        }
        float ocy=gly+22;
        if(y>=ocy&&y<=ocy+16){
            const int ov[]={-2,-1,0,1,2};
            for(int o=0;o<5;o++){float obx=cx+48+o*28;if(x>=obx&&x<=obx+24){cp.octShift=ov[o];applyVoiceCommonToSteps(v);return;}}
        }
        float aby=ocy+26;
        if(y>=aby&&y<=aby+20&&x>=cx&&x<=cx+pw-16){applyVoiceCommonToSteps(v);return;}
        float laby2=aby+26;
        if(y>=laby2&&y<=laby2+20&&x>=cx&&x<=cx+pw-16){
            voices_[v].allLocked=!voices_[v].allLocked;
            bool lk=voices_[v].allLocked;
            for(int i=voices_[v].startStep;i<=voices_[v].endStep;i++){
                stepData_[i].lockVelocity=lk; stepData_[i].lockGate=lk;
                stepData_[i].lockProb=lk; stepData_[i].lockGridDiv=lk;
                stepData_[i].lockNoteRepeat=lk; stepData_[i].lockGlide=lk;
                stepData_[i].lockOctShift=lk; stepData_[i].lockNote=lk; stepData_[i].lockStrum=lk;
            }
            return;
        }
        float scy2=laby2+28;
        if(x>=cx+pw-26&&x<=cx+pw-6&&y>=scy2&&y<=scy2+16){voices_[v].scaleOpen=false;return;}
        for(int r=0;r<12;r++){
            float rbx=cx+(r%6)*20,rby=scy2+(r/6)*20;
            if(x>=rbx&&x<=rbx+18&&y>=rby+14&&y<=rby+30){
                voices_[v].common.scaleRoot=r;
                voices_[v].scaleOpen=true;
                return;
            }
        }
        if(voices_[v].scaleOpen){
            const char* sn[]={"major","lydian","lydianb7","mixolydian","naturalMinor","harmonicMin","melodicMin","dorian","phrygian","phrygianDom","locrian","locrianS2","majorPenta","minorPenta","diminished","augmented","minorBlues","majorBlues","wholeTone","chromatic","bebopMajor","bebopDom"};
            float spx=cx+pw,spy=py+ph-22*22-8;
            for(int s=0;s<22;s++){
                if(x>=spx+4&&x<=spx+156&&y>=spy+4+s*22&&y<=spy+24+s*22){
                    voices_[v].common.scaleName=sn[s];
                    int defRoot = voices_[v].common.scaleRoot + 48;
                    auto notes = harmony_.getAvailableNotes(sn[s], defRoot % 12);
                    std::vector<int> filteredNotes;
                    for(int n : notes) if(n >= 48) filteredNotes.push_back(n);
                    notes = filteredNotes;
                    int ni=0;
                    for(int st=voices_[v].startStep;st<=voices_[v].endStep;st++){
                        if(ni<(int)notes.size()){
                            stepData_[st].noteOverride=notes[ni++]%128;
                            stepData_[st].chordName="";
                            stepData_[st].chordNoteCount=0;
                            for(int p=0;p<7;p++) stepData_[st].chordNotes[p]=-1;
                        }
                        if(ni>=(int)notes.size()) ni=0;
                    }
                    return;
                }
            }
        }
        return;
    }

    // バッジクリック(detail-area下部のGroup一覧)
    int vbHit=voiceBadgeAtPos(x,y,startY);
    if(vbHit>=0){
        float rx,ry; getGroupRowRect(vbHit,rx,ry);
        float rowH2,groupW,oscX,oscW,playX2,playW2;
        float multX[5],multW[5],delX,delW;
        getGroupRowLayout(vbHit,ry,rowH2,groupW,oscX,oscW,playX2,playW2,multX,multW,delX,delW);
        if(button==2){
            voices_[vbHit].delPending=!voices_[vbHit].delPending;
            return;
        }
        if(voices_[vbHit].delPending&&x>=delX&&x<=delX+delW){
            removeVoice(vbHit);
            if(panelVoice_==vbHit) panelVoice_=-1;
            else if(panelVoice_>vbHit) panelVoice_--;
            return;
        }
        if(x>=playX2&&x<=playX2+playW2){
            voices_[vbHit].playing=!voices_[vbHit].playing;
            if(vbHit<numSeqs_){
                if(voices_[vbHit].playing){
                    seqs_[vbHit].reset();
                    seqs_[vbHit].play();
                } else {
                    seqs_[vbHit].stop();
                }
            }
            return;
        }
        const float multVals[] = {0.5f,1.0f,2.0f,3.0f,4.0f};
        for(int m=0;m<5;m++){
            if(x>=multX[m]&&x<=multX[m]+multW[m]){
                voices_[vbHit].common.bpmMult=multVals[m];
                return;
            }
        }
        if(oscW>0.0f&&x>=oscX&&x<=oscX+oscW){
            voices_[vbHit].oscIndex=stepAvailableSoundSet(voices_[vbHit].oscIndex,1,vbHit);
            selectedOscVoice_=voices_[vbHit].oscIndex;
        }
        return;
    }

    int tagHit=voiceTagAtPos(x,y,startY);
    if(tagHit>=0){
        panelVoice_=(panelVoice_==tagHit)?-1:tagHit;
        panelStep_=-1;
        return;
    }

    // ステップボタントグル
    int stepIdx=stepAtPos(x,y,startY);
    if(stepIdx >= 0) {
        float sx,sy; getStepRect(stepIdx,startY,sx,sy);
        float stepCx = sx + stepW_/2.0f - 2.0f;
        float stepCy = sy + stepH_/2.0f;
        float stepR = min(stepW_, stepH_) * 0.40f;
        float menuY = stepCy + stepR + 4.0f;
        float menuHalfW = 9.0f;
        float menuHalfH = 5.0f;
        if(x>=stepCx-menuHalfW && x<=stepCx+menuHalfW && y>=menuY-menuHalfH && y<=menuY+menuHalfH){
            bool insidePanel=false;
            if(panelStep_>=0){
                float psx,psy; getStepRect(panelStep_,startY,psx,psy);
                float ppw=230,pph=400;
                float ppx=panelOnRight_?psx+stepW_+2:psx-ppw-2; float ppy=psy;
                if(ppx+ppw>originX_+SQUARE_W-2) ppx=psx-ppw-2;
                if(ppx<originX_+2) ppx=originX_+2;
                if(ppy+pph>monitorTop_+SQUARE_W-10) ppy=monitorTop_+SQUARE_W-10-pph;
                if(ppy<startY+10) ppy=startY+10;
                if(x>=ppx&&x<=ppx+ppw&&y>=ppy&&y<=ppy+pph) insidePanel=true;
            }
            if(!insidePanel){
                if(panelStep_==stepIdx) panelStep_=-1;
                else { panelStep_=stepIdx; panelVoice_=-1; panelOnRight_=(stepIdx%COLS<COLS-3); }
                return;
            }
        }
    }

    float mainPanelDx,mainPanelContentY,mainPanelHalfW,mainPanelHalfH;
    getPanelArea(mainPanelDx,mainPanelContentY,mainPanelHalfW,mainPanelHalfH);
    bool mainPanelClick = x>=mainPanelDx && x<=mainPanelDx+mainPanelHalfW*2.0f &&
                          y>=mainPanelContentY && y<=uiY_;

    if(mainPanelClick&&panelVoice_>=0){
        int v=panelVoice_;
        VoiceRange& vr=voices_[v];
        VoiceCommonParam& cp=vr.common;
        float dx,contentY,halfW,halfH;
        getPanelArea(dx,contentY,halfW,halfH);
        float pad=8.0f;

        float pianoX=dx+pad,pianoY2=contentY+pad;
        float pianoW2=halfW-pad*2,pianoH2=halfH-pad*2;
        float rX=dx+halfW+pad, rY=contentY+pad, rW=halfW-pad*2;
        float modeH=24.0f;
        float subY2=rY+modeH*2.0f+12.0f;
        float subH2=contentY+halfH-pad-subY2;
        float subW2=(rW-8.0f)/2.0f;
        float stepInfoW2=subW2-16.0f;
        float modeW=((rW-8.0f)/3.0f)*0.9f;
        float modeTotalW=modeW*3.0f+8.0f;
        float modeX=rX+rW-modeTotalW;
        float pianoUsedW2=(pianoW2-8.0f)/12.0f*7.0f;
        float pianoRight2=dx+pad+pianoUsedW2+8.0f;
        float stepGap2=max(8.0f,(modeX-pianoRight2-stepInfoW2)/2.0f);
        float stepInfoX2=pianoRight2+stepGap2;
        float lSubX2=stepInfoX2;
        float th14=jbFont14_?jbFont14_->stringHeight("Ag"):14.0f;
        float th12b=jbFont12_?jbFont12_->stringHeight("Ag"):12.0f;
        float lineH2=th14+8.0f;

        float soundY2=rY+modeH+4.0f;
        if(x>=stepInfoX2&&x<=stepInfoX2+stepInfoW2&&y>=soundY2&&y<=soundY2+modeH){
            vr.oscIndex=stepAvailableSoundSet(vr.oscIndex,1,v);
            selectedOscVoice_=vr.oscIndex;
            return;
        }
        {
            float lowerX=dx+pad;
            float lowerY=contentY+halfH+18.0f;
            float lowerBottom=uiY_-8.0f;
            float lowerH=max(0.0f,lowerBottom-lowerY);
            float lowerW=halfW*2.0f-pad*2.0f;
            float nextX=lowerX+lowerW-24.0f;
            float nextY=lowerY+lowerH/2.0f-18.0f;
            if(x>=nextX&&x<=nextX+24.0f&&y>=nextY&&y<=nextY+36.0f){
                vr.soundPage=(vr.soundPage+1)%2;
                return;
            }
        }
        {
            int setIdx=-1, paramIdx=-1;
            if(soundParamAtPos(x,y,v,setIdx,paramIdx)){
                soundDragSet_=setIdx;
                soundDragParam_=paramIdx;
                soundDragStartY_=(float)y;
                soundDragStartVal_=getSoundParamValue(setIdx,paramIdx);
                return;
            }
        }

        if(y>=rY&&y<=rY+modeH&&x>=modeX&&x<=modeX+modeTotalW){
            applyVoiceCommonToSteps(v);
            return;
        }

        if(panelShowPiano_){
            float pix=pianoX+4,piw=pianoW2-8,piy=pianoY2+4;
            if(x>=pix-12&&x<=pix-2&&y>=piy&&y<=piy+pianoH2){ cp.pianoOctave=ofClamp(cp.pianoOctave-1,0,10); return; }
            float voiceKeyW=piw/12.0f;
            float voiceUsedW=voiceKeyW*7.0f;
            if(x>=pix+voiceUsedW+2&&x<=pix+voiceUsedW+12&&y>=piy&&y<=piy+pianoH2){ cp.pianoOctave=ofClamp(cp.pianoOctave+1,0,10); return; }
            if(x>=pix&&x<=pix+voiceUsedW&&y>=piy&&y<=piy+pianoH2){
                int rootMidi=cp.pianoOctave*12;
                const int whiteNotes[]={0,2,4,5,7,9,11};
                const int blackNotes[]={1,3,6,8,10};
                const int blackSlots[]={0,1,3,4,5};
                int pc=-1;
                if(y<=piy+(pianoH2-8.0f)*0.6f){
                    for(int bi=0;bi<5;bi++){
                        float bx=pix+(blackSlots[bi]+1.0f)*voiceKeyW-voiceKeyW*0.5f;
                        if(x>=bx&&x<=bx+voiceKeyW){ pc=blackNotes[bi]; break; }
                    }
                }
                if(pc<0){
                    int wi=(int)((x-pix)/voiceKeyW);
                    if(wi>=0&&wi<7) pc=whiteNotes[wi];
                }
                if(pc>=0){
                    int midi=rootMidi+pc;
                    if(isScaleNote(midi,v)){
                        cp.noteOverride=(cp.noteOverride==midi)?-1:midi;
                        applyVoiceCommonToSteps(v);
                    }
                }
                return;
            }

            float ly2=subY2+lineH2;
            if(y>=ly2&&y<=ly2+lineH2){
                if(x>=lSubX2+100&&x<=lSubX2+120){cp.pianoOctave=ofClamp(cp.pianoOctave-1,0,10);return;}
                if(x>=lSubX2+124&&x<=lSubX2+144){cp.pianoOctave=ofClamp(cp.pianoOctave+1,0,10);return;}
            }
            ly2+=lineH2;
            if(y>=ly2&&y<=ly2+lineH2){
                float stw=jbFont14_?jbFont14_->stringWidth("SCALE"):(float)5*8.4f;
                float sx2=lSubX2+(subW2-16.0f-stw)/2.0f;
                if(x>=sx2&&x<=sx2+stw){vr.scaleOpen=!vr.scaleOpen;return;}
            }
            ly2+=lineH2;
            if(y>=ly2&&y<=ly2+lineH2&&x>=lSubX2&&x<=lSubX2+subW2-16){
                vr.allLocked=!vr.allLocked;
                bool lk=vr.allLocked;
                for(int i=vr.startStep;i<=vr.endStep;i++){
                    stepData_[i].lockVelocity=lk; stepData_[i].lockGate=lk;
                    stepData_[i].lockProb=lk; stepData_[i].lockGridDiv=lk;
                    stepData_[i].lockNoteRepeat=lk; stepData_[i].lockGlide=lk;
                    stepData_[i].lockOctShift=lk; stepData_[i].lockNote=lk; stepData_[i].lockStrum=lk;
                }
                return;
            }

            if(vr.scaleOpen){
                float cpx2=modeX, cpy2=subY2+lineH2*2;
                float panelW2=modeTotalW;
                float rowH3=18.0f;
                float maxPanelH2=subH2-lineH2*2-8.0f;
                if(x>=cpx2&&x<=cpx2+panelW2&&y>=cpy2&&y<=cpy2+maxPanelH2){
                    const char* sn[]={"major","lydian","lydianb7","mixolydian","naturalMinor","harmonicMin","melodicMin","dorian","phrygian","phrygianDom","locrian","locrianS2","majorPenta","minorPenta","diminished","augmented","minorBlues","majorBlues","wholeTone","chromatic","bebopMajor","bebopDom"};
                    int row=(int)((y-cpy2)/rowH3);
                    if(row>=0&&row<2){
                        float rootW=panelW2/6.0f;
                        int rootCol=(int)((x-cpx2)/rootW);
                        int root=row*6+rootCol;
                        if(root>=0&&root<12){
                            cp.scaleRoot=root;
                            applyVoiceScaleToSteps(v);
                        }
                    } else if(row>=2&&row<24){
                        int scaleIdx=row-2;
                        cp.scaleName=sn[scaleIdx];
                        applyVoiceScaleToSteps(v);
                    }
                    return;
                }
            }
        }

        if(!panelShowParameter_) return;

        float graphX2=modeX, graphW2=modeTotalW;
        float graphTop2=rY+modeH+26.0f;
        float graphBottom2=contentY+halfH-pad;
        float labelBaseline2=graphBottom2-2.0f;
        float barW2=18.0f;
        float barGap2=(graphW2-7.0f*barW2)/6.0f;
        if(barGap2<6.0f){
            barGap2=6.0f;
            barW2=(graphW2-6.0f*barGap2)/7.0f;
        }
        float barY2=graphTop2+10.0f;
        float barH2=max(24.0f,labelBaseline2-th12b-2.0f-barY2);
        float totalBarW2=7.0f*(barW2+barGap2)-barGap2;
        float bStartX2=graphX2+(graphW2-totalBarW2)/2.0f;
        {float bx=bStartX2; if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){dragStep_=v;dragType_=0;dragIsVoice_=true;dragVoiceIdx_=v;dragStartY_=y;dragStartVal_=cp.velocity;return;}}
        {float bx=bStartX2+(barW2+barGap2); if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){dragStep_=v;dragType_=1;dragIsVoice_=true;dragVoiceIdx_=v;dragStartY_=y;dragStartVal_=cp.gate;return;}}
        {float bx=bStartX2+2*(barW2+barGap2); if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){dragStep_=v;dragType_=2;dragIsVoice_=true;dragVoiceIdx_=v;dragStartY_=y;dragStartVal_=cp.prob;return;}}
        {
            float bx=bStartX2+3*(barW2+barGap2);
            if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){
                float btnH=(barH2-7*2)/8.0f;
                int g=(int)((barY2+barH2-y)/(btnH+2))+1;
                if(g>=1&&g<=8){cp.gridDiv=g;applyVoiceCommonToSteps(v);return;}
            }
        }
        {
            float bx=bStartX2+4*(barW2+barGap2);
            if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){
                float btnH=(barH2-7*2)/8.0f;
                int r=(int)((barY2+barH2-y)/(btnH+2))+1;
                if(r>=1&&r<=8){cp.noteRepeat=r;applyVoiceCommonToSteps(v);return;}
            }
        }
        {
            float bx=bStartX2+5*(barW2+barGap2); float glH=(barH2-4)/2.0f;
            if(x>=bx&&x<=bx+barW2){
                if(y>=barY2&&y<=barY2+glH){cp.glide=true;applyVoiceCommonToSteps(v);return;}
                if(y>=barY2+glH+4&&y<=barY2+glH+4+glH){cp.glide=false;applyVoiceCommonToSteps(v);return;}
            }
        }
        {
            float bx=bStartX2+6*(barW2+barGap2);
            if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){
                const int ov[]={-2,-1,0,1,2};
                float btnH=(barH2-4.0f*2.0f)/5.0f;
                int idx=(int)((barY2+barH2-y)/(btnH+2.0f));
                if(idx>=0&&idx<5){cp.octShift=ov[idx];applyVoiceCommonToSteps(v);return;}
            }
        }
        return;
    }

    // 開いているパネルの内部クリック
    if(mainPanelClick&&panelStep_>=0){
        StepUIData& ds=stepData_[panelStep_];
        float dx,contentY,halfW,halfH;
        getPanelArea(dx,contentY,halfW,halfH);
        float pad=8.0f;

        float rX=dx+halfW+pad, rY=contentY+pad, rW=halfW-pad*2;
        float modeH=24.0f;
        float modeW=((rW-8.0f)/3.0f)*0.9f;
        float modeTotalW=modeW*3.0f+8.0f;
        float modeX=rX+rW-modeTotalW;
        float subY2=rY+modeH+8.0f;
        float subH2=contentY+halfH-pad-subY2;
        float subW2=(rW-8.0f)/2.0f;
        float stepInfoW2=subW2-16.0f;
        float pianoUsedW2=(halfW-pad*2-8.0f)/12.0f*7.0f;
        float pianoRight2=dx+pad+pianoUsedW2+8.0f;
        float stepGap2=max(8.0f,(modeX-pianoRight2-stepInfoW2)/2.0f);
        float stepInfoX2=pianoRight2+stepGap2;
        float lSubX2=stepInfoX2;
        float lockIconX2=(stepInfoX2-(dx+pad+pianoUsedW2+8.0f))/2.0f+(dx+pad+pianoUsedW2+8.0f);
        float th14=jbFont14_?jbFont14_->stringHeight("Ag"):14.0f;
        float lineH2=th14+8.0f;

        if(panelShowPiano_){
            float pianoX=dx+pad,pianoY2=contentY+pad;
            float pianoW2=halfW-pad*2,pianoH2=halfH-pad*2;
            float pix=pianoX+4,piw=pianoW2-8,piy=pianoY2+4;
            if(x>=pix-12&&x<=pix-2&&y>=piy&&y<=piy+pianoH2){ pianoOctave_=ofClamp(pianoOctave_-1,0,10); return; }
            float panelKeyW=piw/12.0f;
            float panelUsedW=panelKeyW*7.0f;
            if(x>=pix+panelUsedW+2&&x<=pix+panelUsedW+12&&y>=piy&&y<=piy+pianoH2){ pianoOctave_=ofClamp(pianoOctave_+1,0,10); return; }
            if(x>=pix&&x<=pix+panelUsedW&&y>=piy&&y<=piy+pianoH2){
                int rootMidi=pianoOctave_*12;
                const int whiteNotes[]={0,2,4,5,7,9,11};
                const int blackNotes[]={1,3,6,8,10};
                const int blackSlots[]={0,1,3,4,5};
                int pc=-1;
                if(y<=piy+(pianoH2-8.0f)*0.6f){
                    for(int bi=0;bi<5;bi++){
                        float bx=pix+(blackSlots[bi]+1.0f)*panelKeyW-panelKeyW*0.5f;
                        if(x>=bx&&x<=bx+panelKeyW){ pc=blackNotes[bi]; break; }
                    }
                }
                if(pc<0){
                    int wi=(int)((x-pix)/panelKeyW);
                    if(wi>=0&&wi<7) pc=whiteNotes[wi];
                }
                if(pc>=0){int midi=rootMidi+pc; int vi=voiceAtStep(panelStep_); if(vi<0||isScaleNote(midi,vi)) ds.noteOverride=(ds.noteOverride==midi)?-1:midi;}
                return;
            }
        }

        if(y>=rY&&y<=rY+modeH){
            if(x>=modeX&&x<=modeX+modeW)             {ds.mode=StepUIData::ON;   return;}
            if(x>=modeX+modeW+4&&x<=modeX+modeW*2+4) {ds.mode=StepUIData::OFF;  return;}
            if(x>=modeX+modeW*2+8&&x<=modeX+modeTotalW){ds.mode=StepUIData::SKIP; return;}
        }

        if(panelShowPiano_){
            float ly2=subY2;

            if(y>=ly2&&y<=ly2+lineH2){ if(abs(x-lockIconX2)<=9){ds.lockNote=!ds.lockNote;return;} }
            ly2+=lineH2;
            if(y>=ly2&&y<=ly2+lineH2){
                if(x>=lSubX2+100&&x<=lSubX2+120){pianoOctave_=ofClamp(pianoOctave_-1,0,10);return;}
                if(x>=lSubX2+124&&x<=lSubX2+144){pianoOctave_=ofClamp(pianoOctave_+1,0,10);return;}
            }
            ly2+=lineH2;
            if(y>=ly2&&y<=ly2+lineH2){
                string chordLabel2=ds.chordName.empty()?"CHORD":ds.chordName;
                float ctw2=jbFont14_?jbFont14_->stringWidth(chordLabel2):(float)chordLabel2.size()*8.4f;
                float ctx2=lSubX2+(subW2-16.0f-ctw2)/2.0f;
                if(x>=ctx2&&x<=ctx2+ctw2){ds.chordPanelOpen=!ds.chordPanelOpen;return;}
            }
            ly2+=lineH2;
            if(y>=ly2&&y<=ly2+lineH2&&x>=lSubX2&&x<=lSubX2+subW2-16){
                int vi3=voiceAtStep(panelStep_);
                if(vi3>=0){
                    voices_[vi3].allLocked=!voices_[vi3].allLocked;
                    bool lk=voices_[vi3].allLocked;
                    for(int i=voices_[vi3].startStep;i<=voices_[vi3].endStep;i++){
                        stepData_[i].lockVelocity=lk; stepData_[i].lockGate=lk;
                        stepData_[i].lockProb=lk; stepData_[i].lockGridDiv=lk;
                        stepData_[i].lockNoteRepeat=lk; stepData_[i].lockGlide=lk;
                        stepData_[i].lockOctShift=lk; stepData_[i].lockNote=lk; stepData_[i].lockStrum=lk;
                    }
                }
                return;
            }

            if(ds.chordPanelOpen){
                float cpx2=modeX, cpy2=subY2+lineH2*2;
                float panelW2=modeW;
                float rowH2=18.0f;
                float maxPanelH2=subH2-lineH2*2-8.0f;
                if(x>=cpx2&&x<=cpx2+panelW2&&y>=cpy2&&y<=cpy2+maxPanelH2){
                    int vi=voiceAtStep(panelStep_);
                    int scaleRoot=(vi>=0)?voices_[vi].common.scaleRoot:0;
                    string scaleName=(vi>=0)?voices_[vi].common.scaleName:"naturalMinor";
                    int octave=(vi>=0)?voices_[vi].common.pianoOctave:4;
                    auto scaleNotes=harmony_.getAvailableNotes(scaleName,scaleRoot);
                    std::vector<int> octaveNotes; int startMidi2=octave*12;
                    for(int n:scaleNotes) if(n>=startMidi2&&n<startMidi2+12) octaveNotes.push_back(n);
                    const char* chordTypes2[]={"maj","min","dim","aug","sus2","sus4","maj7","min7","dom7","dim7","m7b5","pow5","pow5oct"};
                    const char* chordLabels2[]={"maj","min","dim","aug","sus2","sus4","maj7","min7","dom7","dim7","m7b5","pow5","p5+8"};
                    const char* noteNames2[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                    int nTypes2=13;
                    struct CE2{int rm;string tn,lb;};
                    std::vector<CE2> entries2; std::set<int> spc2;
                    for(int n:scaleNotes) spc2.insert(n%12);
                    const int iv2[13][7]={{0,4,7,-1,-1,-1,-1},{0,3,7,-1,-1,-1,-1},{0,3,6,-1,-1,-1,-1},{0,4,8,-1,-1,-1,-1},{0,2,7,-1,-1,-1,-1},{0,5,7,-1,-1,-1,-1},{0,4,7,11,-1,-1,-1},{0,3,7,10,-1,-1,-1},{0,4,7,10,-1,-1,-1},{0,3,6,9,-1,-1,-1},{0,3,6,10,-1,-1,-1},{0,7,-1,-1,-1,-1,-1},{0,7,12,-1,-1,-1,-1}};
                    for(int n:octaveNotes){int rp=n%12;for(int t=0;t<nTypes2;t++){bool ok=true;for(int i=0;i<7;i++){if(iv2[t][i]<0)break;if(spc2.find((rp+iv2[t][i])%12)==spc2.end()){ok=false;break;}}if(!ok)continue;entries2.push_back({n,chordTypes2[t],string(noteNames2[n%12])+chordLabels2[t]});}}
                    int ne2=(int)entries2.size();
                    int actionRows2=2;
                    float visH2=min((float)((ne2+actionRows2)*rowH2+8.0f),maxPanelH2);
                    if(y>cpy2+visH2) return;
                    int selIdx2=0; for(int i=0;i<ne2;i++) if(ds.chordName==entries2[i].lb){selIdx2=i;break;}
                    int visRows2=(int)(visH2/rowH2);
                    int entryRows2=max(0,visRows2-actionRows2);
                    int rawRow2=(int)((y-cpy2)/rowH2);
                    if(rawRow2==0){
                        float dtw2=jbFont12_?jbFont12_->stringWidth(". DELETE"):(float)8*7.2f;
                        if(!(x>=cpx2+8.0f&&x<=cpx2+8.0f+dtw2)) return;
                        ds.chordName=""; ds.chordNoteCount=0; ds.chordPanelOpen=false;
                        for(int p=0;p<7;p++) ds.chordNotes[p]=-1;
                        return;
                    }
                    if(rawRow2==1){
                        float ptw2=jbFont12_?jbFont12_->stringWidth(". PANEL OFF"):(float)11*7.2f;
                        if(!(x>=cpx2+8.0f&&x<=cpx2+8.0f+ptw2)) return;
                        ds.chordPanelOpen=false;
                        return;
                    }
                    int scrollOff2=ofClamp(selIdx2-entryRows2/2,0,max(0,ne2-entryRows2));
                    int row2=rawRow2-actionRows2+scrollOff2;
                    if(row2>=0&&row2<ne2){
                        float etw2=jbFont12_?jbFont12_->stringWidth(entries2[row2].lb):(float)entries2[row2].lb.size()*7.2f;
                        if(!(x>=cpx2+8.0f&&x<=cpx2+8.0f+etw2)) return;
                        ds.chordName=entries2[row2].lb;
                        auto cd=harmony_.getChord(entries2[row2].tn,entries2[row2].rm);
                        ds.chordNoteCount=0;
                        for(int n=0;n<(int)cd.intervals.size()&&n<7;n++){ds.chordNotes[n]=cd.intervals[n];ds.chordNoteCount++;}
                    }
                    return;
                }
            }
        }

        if(!panelShowParameter_) return;

        float th12b=jbFont12_?jbFont12_->stringHeight("Ag"):12.0f;
        float graphX2=modeX, graphW2=modeTotalW;
        float graphTop2=rY+modeH+26.0f;
        float graphBottom2=contentY+halfH-pad;
        float labelBaseline2=graphBottom2-2.0f;
        float barW2=18.0f;
        float barGap2=(graphW2-7.0f*barW2)/6.0f;
        if(barGap2<6.0f){
            barGap2=6.0f;
            barW2=(graphW2-6.0f*barGap2)/7.0f;
        }
        float barY2=graphTop2+10.0f;
        float paramLockY2=(rY+modeH+barY2)/2.0f;
        float barH2=max(24.0f,labelBaseline2-th12b-2.0f-barY2);
        float totalBarW2=7.0f*(barW2+barGap2)-barGap2;
        float bStartX2=graphX2+(graphW2-totalBarW2)/2.0f;
        for(int b=0;b<3;b++){
            float bx=bStartX2+b*(barW2+barGap2);
            if(abs(x-(bx+barW2/2))<=9&&abs(y-paramLockY2)<=9){
                if(b==0) ds.lockVelocity=!ds.lockVelocity;
                if(b==1) ds.lockGate=!ds.lockGate;
                if(b==2) ds.lockProb=!ds.lockProb;
                return;
            }
        }
        {float bx=bStartX2; if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){dragStep_=panelStep_;dragType_=0;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[panelStep_].velocity;return;}}
        {float bx=bStartX2+(barW2+barGap2); if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){dragStep_=panelStep_;dragType_=1;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[panelStep_].gate;return;}}
        {float bx=bStartX2+2*(barW2+barGap2); if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){dragStep_=panelStep_;dragType_=2;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[panelStep_].prob;return;}}
        {float bx=bStartX2+3*(barW2+barGap2); if(abs(x-(bx+barW2/2))<=9&&abs(y-paramLockY2)<=9){ds.lockGridDiv=!ds.lockGridDiv;return;}}
        {
            float bx=bStartX2+3*(barW2+barGap2);
            if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){
                float btnH=(barH2-7*2)/8.0f;
                int g=(int)((barY2+barH2-y)/(btnH+2))+1;
                if(g>=1&&g<=8){ds.gridDiv=g;return;}
            }
        }
        {float bx=bStartX2+4*(barW2+barGap2); if(abs(x-(bx+barW2/2))<=9&&abs(y-paramLockY2)<=9){ds.lockNoteRepeat=!ds.lockNoteRepeat;return;}}
        {
            float bx=bStartX2+4*(barW2+barGap2);
            if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){
                float btnH=(barH2-7*2)/8.0f;
                int r=(int)((barY2+barH2-y)/(btnH+2))+1;
                if(r>=1&&r<=8){ds.noteRepeat=r;return;}
            }
        }
        {float bx=bStartX2+5*(barW2+barGap2); if(abs(x-(bx+barW2/2))<=9&&abs(y-paramLockY2)<=9){ds.lockGlide=!ds.lockGlide;return;}}
        {float bx=bStartX2+6*(barW2+barGap2); if(abs(x-(bx+barW2/2))<=9&&abs(y-paramLockY2)<=9){ds.lockStrum=!ds.lockStrum;return;}}
        {
            float bx=bStartX2+5*(barW2+barGap2); float glH=(barH2-4)/2.0f;
            if(x>=bx&&x<=bx+barW2){
                if(y>=barY2&&y<=barY2+glH){ds.glide=true;return;}
                if(y>=barY2+glH+4&&y<=barY2+glH+4+glH){ds.glide=false;return;}
            }
        }
        {
            float bx=bStartX2+6*(barW2+barGap2); float glH=(barH2-4)/2.0f;
            if(x>=bx&&x<=bx+barW2){
                if(y>=barY2&&y<=barY2+glH){ds.strumOn=true;return;}
                if(y>=barY2+glH+4&&y<=barY2+glH+4+glH){ds.strumOn=false;return;}
            }
        }
        return;
    }

    if(stepIdx<0) return;
    float sx,sy; getStepRect(stepIdx,startY,sx,sy);
    if(showVelocity_){
        float barX=sx+4,barY=sy+stepH_-4-14;
        if(x>=barX&&x<=barX+stepW_-12&&y>=barY&&y<=barY+14){
            dragStep_=stepIdx;dragType_=0;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[stepIdx].velocity;return;
        }
    }
    int ownerVoice=voiceAtStep(stepIdx);
    pendingEdgeVoice_=-1;
    if(ownerVoice>=0){
        if(stepIdx==voices_[ownerVoice].startStep){ pendingEdgeVoice_=ownerVoice; pendingEdgeIsLeft_=true; }
        else if(stepIdx==voices_[ownerVoice].endStep){ pendingEdgeVoice_=ownerVoice; pendingEdgeIsLeft_=false; }
    }
    pendingClickStep_=stepIdx; pendingClickX_=(float)x; pendingClickY_=(float)y;
}

void SequencerUI::mouseDragged(int x,int y,int button){
    if(soundDragSet_>=0&&soundDragParam_>=0){
        float dy=soundDragStartY_-(float)y;
        setSoundParamValue(soundDragSet_,soundDragParam_,
                           soundDragStartVal_+dy*soundParamDragStep(soundDragParam_));
        return;
    }
    if(draggingGlobalBpm_){
        float dy=dragStartY_-y;
        globalBpm_=ofClamp(dragStartVal_+dy, 40.0f, 200.0f);
        return;
    }
    if(draggingGlobalVolume_){
        float dy=dragStartY_-y;
        globalVolume_=ofClamp(dragStartVal_+dy/100.0f, 0.0f, 1.0f);
        return;
    }
    if(panelDragVoice_>=0){
        voices_[panelDragVoice_].panelOffsetX=panelDragOffsetX_+(x-panelDragStartX_);
        voices_[panelDragVoice_].panelOffsetY=panelDragOffsetY_+(y-panelDragStartY_);
        return;
    }
    if(dragStep_>=0){
        float dy=dragStartY_-y;
        if(dragIsVoice_){
            VoiceCommonParam& cp=voices_[dragVoiceIdx_].common;
            if(dragType_==0){cp.velocity=(int)ofClamp(dragStartVal_+dy,0,127);applyVoiceCommonToSteps(dragVoiceIdx_);}
            if(dragType_==1){cp.gate    =ofClamp(dragStartVal_+dy/100.0f,0.0f,1.0f);applyVoiceCommonToSteps(dragVoiceIdx_);}
            if(dragType_==2){cp.prob    =ofClamp(dragStartVal_+dy/100.0f,0.0f,1.0f);applyVoiceCommonToSteps(dragVoiceIdx_);}
        } else {
            if(dragType_==0) stepData_[dragStep_].velocity=(int)ofClamp(dragStartVal_+dy,0,127);
            if(dragType_==1) stepData_[dragStep_].gate    =ofClamp(dragStartVal_+dy/100.0f,0.0f,1.0f);
            if(dragType_==2) stepData_[dragStep_].prob    =ofClamp(dragStartVal_+dy/100.0f,0.0f,1.0f);
        }
        return;
    }
    if(groupGesture_==GroupGesture::RESIZE_LEFT || groupGesture_==GroupGesture::RESIZE_RIGHT){
        int s=stepAtPos(x,y,uiY_);
        if(s<0) return;
        if(groupGesture_==GroupGesture::RESIZE_LEFT){
            int fixedEnd=gestureAnchor_;
            if(s<=fixedEnd-(MIN_VOICE_LEN-1) &&
               !rangeOverlapsExcept(s,fixedEnd,gestureVoiceIdx_)){
                gestureLive_=s;
            }
        } else {
            int fixedStart=gestureAnchor_;
            if(s>=fixedStart+(MIN_VOICE_LEN-1) &&
               !rangeOverlapsExcept(fixedStart,s,gestureVoiceIdx_)){
                gestureLive_=s;
            }
        }
        return;
    }
    if(pendingClickStep_>=0){
        float ddx=x-pendingClickX_, ddy=y-pendingClickY_;
        if(ddx*ddx+ddy*ddy >= CLICK_DRAG_THRESHOLD*CLICK_DRAG_THRESHOLD){
            int s0=pendingClickStep_;
            int ev=pendingEdgeVoice_;
            bool isLeft=pendingEdgeIsLeft_;
            pendingClickStep_=-1; pendingEdgeVoice_=-1;
            if(ev>=0){
                groupGesture_   = isLeft ? GroupGesture::RESIZE_LEFT : GroupGesture::RESIZE_RIGHT;
                gestureVoiceIdx_= ev;
                gestureAnchor_  = isLeft ? voices_[ev].endStep : voices_[ev].startStep;
                gestureLive_    = isLeft ? voices_[ev].startStep : voices_[ev].endStep;
            } else if(voiceAtStep(s0)<0){
                groupGesture_=GroupGesture::CREATE; gestureAnchor_=s0; gestureLive_=s0;
            }
        } else {
            return;
        }
    }
    if(groupGesture_==GroupGesture::CREATE){
        int s=stepAtPos(x,y,uiY_);
        if(s>=0&&voiceAtStep(s)<0) gestureLive_=s;
    }
}

void SequencerUI::mouseReleased(int x,int y,int button){
    draggingGlobalBpm_=false; draggingGlobalVolume_=false;
    panelDragVoice_=-1;
    dragStep_=-1; dragType_=-1; dragIsVoice_=false; dragVoiceIdx_=-1;
    soundDragSet_=-1; soundDragParam_=-1;
    if(pendingClickStep_>=0){
        cycleStepMode(pendingClickStep_);
        pendingClickStep_=-1; pendingEdgeVoice_=-1;
    }
    if(groupGesture_==GroupGesture::RESIZE_LEFT){
        int newStart=gestureLive_;
        int fixedEnd=gestureAnchor_;
        if(newStart<=fixedEnd-(MIN_VOICE_LEN-1) &&
           !rangeOverlapsExcept(newStart,fixedEnd,gestureVoiceIdx_)){
            voices_[gestureVoiceIdx_].startStep=newStart;
            if(gestureVoiceIdx_<numSeqs_)
                seqs_[gestureVoiceIdx_].setLoopRange(0, fixedEnd-newStart);
        }
        groupGesture_=GroupGesture::NONE; gestureVoiceIdx_=-1; gestureAnchor_=-1; gestureLive_=-1;
        return;
    }
    if(groupGesture_==GroupGesture::RESIZE_RIGHT){
        int newEnd=gestureLive_;
        int fixedStart=gestureAnchor_;
        if(newEnd>=fixedStart+(MIN_VOICE_LEN-1) &&
           !rangeOverlapsExcept(fixedStart,newEnd,gestureVoiceIdx_)){
            voices_[gestureVoiceIdx_].endStep=newEnd;
            if(gestureVoiceIdx_<numSeqs_)
                seqs_[gestureVoiceIdx_].setLoopRange(0, newEnd-fixedStart);
        }
        groupGesture_=GroupGesture::NONE; gestureVoiceIdx_=-1; gestureAnchor_=-1; gestureLive_=-1;
        return;
    }
    if(groupGesture_==GroupGesture::CREATE){
        if(gestureAnchor_>=0&&gestureLive_>=0&&gestureAnchor_!=gestureLive_){
            int s=min(gestureAnchor_,gestureLive_);
            int e=max(gestureAnchor_,gestureLive_);
            if(e-s+1>=MIN_VOICE_LEN) {
                int beforeCount = voiceCount_;
                addVoiceRange(s,e);
                if(voiceCount_ > beforeCount) {
                    panelVoice_ = voiceCount_ - 1;
                    panelStep_ = -1;
                }
            }
        }
        groupGesture_=GroupGesture::NONE; gestureAnchor_=-1; gestureLive_=-1;
    }
}

void SequencerUI::mouseScrolled(int x,int y,float scrollX,float scrollY){
    {
        float playX,playW,bpmX,bpmW,volX,volW,rowY,rowH,iconCx,iconCy;
        getTransportLayout(playX,playW,bpmX,bpmW,volX,volW,rowY,rowH,iconCx,iconCy);
        if(x>=bpmX && x<=bpmX+bpmW && y>=rowY && y<=rowY+rowH){
            globalBpm_=ofClamp(globalBpm_+scrollY*2.0f, 40.0f, 200.0f);
            return;
        }
        if(x>=volX && x<=volX+volW && y>=rowY && y<=rowY+rowH){
            globalVolume_=ofClamp(globalVolume_+scrollY*0.05f, 0.0f, 1.0f);
            return;
        }
    }
    bool chordOpen=panelStep_>=0 && stepData_[panelStep_].chordPanelOpen;
    if(!chordOpen) for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].panelOpen) continue;
        float px,py,pw,ph; getVoicePanelRect(v,px,py,pw,ph);
        bool inScalePanel = voices_[v].scaleOpen && x>=px+pw && x<=px+pw+160;
        if(!inScalePanel && (x<px||x>px+pw||y<py||y>py+ph)) continue;
        VoiceCommonParam& cp=voices_[v].common;
        float cx=px+8,cy=py+38;
        float voicePianoUsedW=(pw-16.0f)/12.0f*7.0f;
        if(x>=cx&&x<=cx+voicePianoUsedW&&y>=cy+14&&y<=cy+52){
            cp.pianoOctave=ofClamp(cp.pianoOctave+(scrollY>0?1:-1),0,10); return;
        }
        float sliderX=cx+48,sliderW=pw-60;
        float vy=cy+62,gy=vy+22,pry=gy+22;
        if(x>=sliderX&&x<=sliderX+sliderW){
            if(y>=vy &&y<=vy+12){cp.velocity=ofClamp(cp.velocity+(int)(scrollY*5),0,127);applyVoiceCommonToSteps(v);}
            if(y>=gy &&y<=gy+12){cp.gate    =ofClamp(cp.gate   +scrollY*0.05f,0.0f,1.0f);applyVoiceCommonToSteps(v);}
            if(y>=pry&&y<=pry+12){cp.prob   =ofClamp(cp.prob   +scrollY*0.05f,0.0f,1.0f);applyVoiceCommonToSteps(v);}
        }
        if(voices_[v].scaleOpen){
            float spx=cx+pw,spy=py+ph-22*22-8;
            if(x>=spx&&x<=spx+160&&y>=spy&&y<=spy+22*22+8){
                const char* sn[]={"major","lydian","lydianb7","mixolydian","naturalMinor","harmonicMin","melodicMin","dorian","phrygian","phrygianDom","locrian","locrianS2","majorPenta","minorPenta","diminished","augmented","minorBlues","majorBlues","wholeTone","chromatic","bebopMajor","bebopDom"};
                int cur=0;
                for(int s=0;s<22;s++) if(cp.scaleName==sn[s]){cur=s;break;}
                cur=ofClamp(cur-(int)scrollY,0,21);
                cp.scaleName=sn[cur];
                return;
            }
        }
        return;
    }
    if(panelVoice_>=0){
        int v=panelVoice_;
        VoiceRange& vr=voices_[v];
        VoiceCommonParam& cp=voices_[v].common;
        float dx,contentY,halfW,halfH;
        getPanelArea(dx,contentY,halfW,halfH);
        float pad=8.0f;
        float pianoX=dx+pad,pianoY2=contentY+pad;
        float pianoW2=halfW-pad*2,pianoH2=halfH-pad*2;
        float pianoUsedW=(pianoW2-8.0f)/12.0f*7.0f+8.0f;
        float rX=dx+halfW+pad, rY=contentY+pad, rW=halfW-pad*2;
        float modeH=24.0f;
        float subW2=(rW-8.0f)/2.0f;
        float stepInfoW2=subW2-16.0f;
        float modeW=((rW-8.0f)/3.0f)*0.9f;
        float modeTotalW=modeW*3.0f+8.0f;
        float modeX=rX+rW-modeTotalW;
        float pianoRight2=dx+pad+(pianoW2-8.0f)/12.0f*7.0f+8.0f;
        float stepGap2=max(8.0f,(modeX-pianoRight2-stepInfoW2)/2.0f);
        float stepInfoX2=pianoRight2+stepGap2;
        float soundY2=rY+modeH+4.0f;
        if(x>=stepInfoX2&&x<=stepInfoX2+stepInfoW2&&y>=soundY2&&y<=soundY2+modeH){
            int delta=scrollY>0?1:-1;
            vr.oscIndex=stepAvailableSoundSet(vr.oscIndex,delta,v);
            selectedOscVoice_=vr.oscIndex;
            return;
        }
        {
            float lowerX=dx+pad;
            float lowerY=contentY+halfH+18.0f;
            float lowerBottom=uiY_-8.0f;
            float lowerH=max(0.0f,lowerBottom-lowerY);
            float lowerW=halfW*2.0f-pad*2.0f;
            float nextX=lowerX+lowerW-24.0f;
            float nextY=lowerY+lowerH/2.0f-18.0f;
            if(x>=nextX&&x<=nextX+24.0f&&y>=nextY&&y<=nextY+36.0f){
                int delta=scrollY>0?1:-1;
                vr.soundPage=(vr.soundPage+delta+2)%2;
                return;
            }
        }
        {
            int setIdx=-1, paramIdx=-1;
            if(soundParamAtPos(x,y,v,setIdx,paramIdx)){
                setSoundParamValue(setIdx,paramIdx,
                    getSoundParamValue(setIdx,paramIdx)+scrollY*soundParamWheelStep(paramIdx));
                return;
            }
        }
        if(panelShowPiano_&&x>=pianoX&&x<=pianoX+pianoUsedW&&y>=pianoY2&&y<=pianoY2+pianoH2){
            cp.pianoOctave=ofClamp(cp.pianoOctave+(scrollY>0?1:-1),0,10);
            return;
        }
        if(!panelShowParameter_) return;
        float th12=jbFont12_?jbFont12_->stringHeight("Ag"):12.0f;
        float graphX2=modeX, graphW2=modeTotalW;
        float graphTop2=rY+modeH+26.0f;
        float graphBottom2=contentY+halfH-pad;
        float labelBaseline2=graphBottom2-2.0f;
        float barW2=18.0f;
        float barGap2=(graphW2-7.0f*barW2)/6.0f;
        if(barGap2<6.0f){
            barGap2=6.0f;
            barW2=(graphW2-6.0f*barGap2)/7.0f;
        }
        float barY2=graphTop2+10.0f;
        float barH2=max(24.0f,labelBaseline2-th12-2.0f-barY2);
        float totalBarW2=7.0f*(barW2+barGap2)-barGap2;
        float bStartX2=graphX2+(graphW2-totalBarW2)/2.0f;
        if(x>=bStartX2&&x<=bStartX2+totalBarW2&&y>=barY2&&y<=barY2+barH2){
            float bx=bStartX2;
            if(x<=bx+barW2){cp.velocity=ofClamp(cp.velocity+(int)(scrollY*5),0,127);applyVoiceCommonToSteps(v);return;}
            bx+=barW2+barGap2;
            if(x<=bx+barW2){cp.gate=ofClamp(cp.gate+scrollY*0.05f,0.0f,1.0f);applyVoiceCommonToSteps(v);return;}
            bx+=barW2+barGap2;
            if(x<=bx+barW2){cp.prob=ofClamp(cp.prob+scrollY*0.05f,0.0f,1.0f);applyVoiceCommonToSteps(v);return;}
        }
        return;
    }
    if(panelStep_<0) return;
    StepUIData& d=stepData_[panelStep_];
    float dx,contentY,halfW,halfH;
    getPanelArea(dx,contentY,halfW,halfH);
    float pad=8.0f;

    float pianoX=dx+pad,pianoY2=contentY+pad;
    float pianoW2=halfW-pad*2,pianoH2=halfH-pad*2;
    float pianoUsedW=(pianoW2-8.0f)/12.0f*7.0f+8.0f;
    if(panelShowPiano_&&x>=pianoX&&x<=pianoX+pianoUsedW&&y>=pianoY2&&y<=pianoY2+pianoH2){
        if(d.chordNoteCount>0){
            int shift=(scrollY>0?12:-12);
            for(int p=0;p<d.chordNoteCount;p++) d.chordNotes[p]=ofClamp(d.chordNotes[p]+shift,9,120);
            if(d.noteOverride>=0) d.noteOverride=ofClamp(d.noteOverride+shift,9,120);
            d.octDelta+=scrollY>0?1:-1;
        }
        pianoOctave_=ofClamp(pianoOctave_+(scrollY>0?1:-1),0,10);
        return;
    }
    if(!panelShowParameter_) return;

    float rX=dx+halfW+pad, rY=contentY+pad, rW=halfW-pad*2;
    float modeH=24.0f;
    float modeW=((rW-8.0f)/3.0f)*0.9f;
    float modeTotalW=modeW*3.0f+8.0f;
    float modeX=rX+rW-modeTotalW;
    float th12=jbFont12_?jbFont12_->stringHeight("Ag"):12.0f;
    float graphX2=modeX, graphW2=modeTotalW;
    float graphTop2=rY+modeH+26.0f;
    float graphBottom2=contentY+halfH-pad;
    float labelBaseline2=graphBottom2-2.0f;
    float barW2=18.0f;
    float barGap2=(graphW2-7.0f*barW2)/6.0f;
    if(barGap2<6.0f){
        barGap2=6.0f;
        barW2=(graphW2-6.0f*barGap2)/7.0f;
    }
    float barY2=graphTop2+10.0f;
    float barH2=max(24.0f,labelBaseline2-th12-2.0f-barY2);
    float totalBarW2=7.0f*(barW2+barGap2)-barGap2;
    float bStartX2=graphX2+(graphW2-totalBarW2)/2.0f;
    if(x>=bStartX2&&x<=bStartX2+totalBarW2&&y>=barY2&&y<=barY2+barH2){
        float bx=bStartX2;
        if(x<=bx+barW2){d.velocity=ofClamp(d.velocity+(int)(scrollY*5),0,127);return;}
        bx+=barW2+barGap2;
        if(x<=bx+barW2){d.gate=ofClamp(d.gate+scrollY*0.05f,0.0f,1.0f);return;}
        bx+=barW2+barGap2;
        if(x<=bx+barW2){d.prob=ofClamp(d.prob+scrollY*0.05f,0.0f,1.0f);return;}
    }
}
