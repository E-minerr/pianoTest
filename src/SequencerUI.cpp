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

void SequencerUI::addVoiceRange(int startStep, int endStep) {
    if(voiceCount_ >= MAX_VOICES) return;
    if(rangeOverlaps(startStep, endStep)) return;
    int v = voiceCount_;
    voices_[v].startStep = startStep;
    voices_[v].endStep   = endStep;
    voices_[v].oscIndex  = v % 6;
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

int SequencerUI::voiceBadgeAtPos(int mx, int my, float startY) const {
    for(int v = 0; v < voiceCount_; v++) {
        if(!voices_[v].active) continue;
        float rx, ry; getGroupRowRect(v, rx, ry);
        float th  = jbFont12_ ? jbFont12_->stringHeight("Ag") : 12.0f;
        float rowH = th + 8.0f;
        float detailW = SQUARE_W * 5.0f / 7.0f;
        float colW = (detailW - 16.0f) / 2.0f;
        if(mx >= rx && mx <= rx + colW && my >= ry && my <= ry + rowH) return v;
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
    float leftListW = SQUARE_W * 2.0f / 7.0f;
    float detailX   = originX_ + leftListW;
    float th   = jbFont12_ ? jbFont12_->stringHeight("Ag") : 12.0f;
    float rowH = th + 8.0f;
    float detailW = SQUARE_W * 5.0f / 7.0f;
    float colW = (detailW - 16.0f) / 2.0f;
    int rows = (voiceCount_ + 1) / 2;
    float listTop = uiY_ - 2.0f - rows * (rowH + 6.0f);
    int row = voiceIdx / 2;
    int col = voiceIdx % 2;
    rx = detailX + 8.0f + col * colW;
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

    groupW = sw("Grp"+ofToString(voiceIdx+1)) + padX*2.0f;

    oscX = rx + groupW;
    oscW = sw("OSC"+ofToString(voices_[voiceIdx].oscIndex+1)) + padX*2.0f;

    string playLabel = voices_[voiceIdx].playing ? "GO" : "STOP";
    playX2 = oscX + oscW + gap;
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
    if(panelStep_ >= 0) drawPanel(panelStep_);
    for(int v = 0; v < voiceCount_; v++) {
        if(voices_[v].panelOpen) drawVoicePanel(v);
    }

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

    ofSetColor(colBg_); ofFill();
    ofDrawRectangle(originX_, monitorTop_, SQUARE_W, MONITOR_H);

    ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectangle(originX_, monitorTop_, leftListW, MONITOR_H);
    ofFill();
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

    string playLabel = globalPlaying_ ? "GO" : "STOP";
    string bpmLabel  = "BPM "+ofToString((int)globalBpm_);
    string volLabel  = "VOL "+ofToString((int)(globalVolume_*100))+"%";

    ofSetColor(globalPlaying_ ? colCyan_ : colPink_); ofFill();
    ofDrawRectRounded(playX, rowY, playW, rowH, 3);
    ofSetColor(20);
    {
        float tw = jbFont14_ ? jbFont14_->stringWidth(playLabel) : (float)playLabel.size()*8.0f;
        drawText14(playLabel, playX + (playW - tw) / 2.0f, rowY+rowH-7.0f);
    }

    ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectangle(bpmX, rowY, bpmW, rowH); ofFill();
    ofSetColor(colCyan_);
    drawText14(bpmLabel, bpmX+8.0f, rowY+rowH-7.0f);

    ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectangle(volX, rowY, volW, rowH); ofFill();
    ofSetColor(colCyan_);
    drawText14(volLabel, volX+8.0f, rowY+rowH-7.0f);

    drawIconChan(iconCx, iconCy, 14.0f, colCyan_);
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
                ofSetColor(colBg_); ofFill();
                ofDrawRectangle(tagX, tagY, tagW, tagH);
                ofSetColor(vc);
                drawText14(label, tagX+6.0f, tagY+tagH-4.0f);
            }
            i = rowEnd + 1;
        }
    }
}

void SequencerUI::drawVoiceBadges(float startY) {
    static const ofColor groupCols[6] = {
        ofColor::fromHex(0x6BE4FF), ofColor::fromHex(0xFF6B9D), ofColor::fromHex(0xE1FF00),
        ofColor::fromHex(0x39FF14), ofColor::fromHex(0xFF6A00), ofColor::fromHex(0xBF00FF)
    };
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
        ofColor gc=groupCols[v%6];

        ofSetColor(gc);
        drawText12("Grp"+ofToString(v+1), rx+6, ry+textY_offset);

        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
        ofDrawRectangle(oscX, ry, oscW, rowH2); ofFill();
        ofSetColor(colCyan_);
        drawText12("OSC"+ofToString(voices_[v].oscIndex+1), oscX+6, ry+textY_offset);

        bool isPlaying = voices_[v].playing;
        ofSetColor(isPlaying ? colCyan_ : colPink_); ofFill();
        ofDrawRectRounded(playX2, ry, playW2, rowH2, 3);
        ofSetColor(20);
        {
            string pl = isPlaying ? "GO" : "STOP";
            float tw = jbFont12_ ? jbFont12_->stringWidth(pl) : (float)pl.size()*7.2f;
            drawText12(pl, playX2 + (playW2 - tw) / 2.0f, ry+textY_offset);
        }

        for(int m=0;m<5;m++){
            bool act=(voices_[v].common.bpmMult==multVals[m]);
            if(act){ ofSetColor(colYellow_); ofFill(); }
            else    { ofSetColor(colCyan_);  ofNoFill(); ofSetLineWidth(1); }
            ofDrawRectRounded(multX[m], ry, multW[m], rowH2, 2);
            ofFill();
            ofSetColor(act?ofColor(20):colCyan_);
            drawText12(multLabels[m], multX[m]+6, ry+textY_offset);
        }

        if(voices_[v].delPending){
            ofSetColor(ofColor(200,40,40)); ofFill();
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
    for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].active||v>=numSeqs_) continue;
        int localStep=seqs_[v].getCurrentStep();
        int globalStep=voices_[v].startStep+localStep;

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

    if(idx==0 || idx==TOTAL_STEPS-1){
        drawIconChan(cx, cy, r, iconChanColors_[iconChanColorIdx_]);
    } else {
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
    const bool isBlack[]={false,true,false,true,false,false,true,false,true,false,true,false};
    for(int k=0;k<12;k++){
        if(!isBlack[k]){
            int midi=rootMidi+k;
            bool selected=(selectedNote==midi); bool inScale=isScaleNote(midi, voiceIdx);
            ofColor col; if(selected) col=colRed_; else if(inScale) col=ofColor(200,60,60); else col=ofColor(200);
            ofSetColor(col); ofFill(); ofDrawRectangle(x+k*keyW,y,keyW-1,h);
        }
    }
    for(int k=0;k<12;k++){
        if(isBlack[k]){
            int midi=rootMidi+k;
            bool selected=(selectedNote==midi); bool inScale=isScaleNote(midi, voiceIdx);
            ofColor col; if(selected) col=colRed_; else if(inScale) col=ofColor(160,40,40); else col=ofColor(28);
            ofSetColor(col); ofFill(); ofDrawRectangle(x+k*keyW,y,keyW-1,h*0.62f);
        }
    }
    ofSetColor(80); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(x,y,w,h); ofFill();
    ofSetColor(colGold_);
    ofDrawBitmapString("<",x-10,y+h/2+4);
    ofDrawBitmapString(">",x+w+2,y+h/2+4);
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

// ========== ステップパネル ==========

void SequencerUI::drawPanel(int stepIdx) {
    if(stepIdx<0||stepIdx>=TOTAL_STEPS||panelStep_<0) return;
    StepUIData& d=stepData_[stepIdx];

    float dx,contentY,halfW,halfH;
    getPanelArea(dx,contentY,halfW,halfH);

    float pad=8.0f;
    float pianoX=dx+pad, pianoY=contentY+pad;
    float pianoW=halfW-pad*2, pianoH=halfH-pad*2;
    ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectangle(pianoX,pianoY,pianoW,pianoH); ofFill();
    drawMiniPiano(pianoX+4,pianoY+4,pianoW-8,pianoH-8,stepIdx,false,voiceAtStep(stepIdx));

    float rX=dx+halfW+pad, rY=contentY+pad, rW=halfW-pad*2;
    float modeH=24.0f, modeW=(rW-8.0f)/3.0f;
    const char* modeLabels[]={"ON","OFF","SKIP"};
    StepUIData::StepMode modes[]={StepUIData::ON,StepUIData::OFF,StepUIData::SKIP};
    for(int m=0;m<3;m++){
        bool act=(d.mode==modes[m]);
        if(act){ ofSetColor(colPink_); ofFill(); }
        else   { ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1); }
        ofDrawRectRounded(rX+m*(modeW+4),rY,modeW,modeH,3); ofFill();
        ofSetColor(act?ofColor(20):colCyan_);
        float tw=jbFont14_?jbFont14_->stringWidth(modeLabels[m]):(float)strlen(modeLabels[m])*8.4f;
        drawText14(modeLabels[m],rX+m*(modeW+4)+(modeW-tw)/2.0f,rY+modeH-6.0f);
    }

    float subY=rY+modeH+8.0f, subH=halfH-modeH-pad*2-8.0f;
    float subW=(rW-8.0f)/2.0f, lSubX=rX, rSubX=rX+subW+8.0f;
    float th14=jbFont14_?jbFont14_->stringHeight("Ag"):14.0f;
    float lineH=th14+8.0f;

    auto drawLock14=[&](float lx,float ly,bool locked){
        ofSetColor(locked?colPink_:colCyan_);
        if(locked){ ofFill(); ofDrawCircle(lx,ly,5); ofSetLineWidth(1.5f); ofDrawLine(lx-4,ly-4,lx+4,ly+4); }
        else       { ofNoFill(); ofSetLineWidth(1.5f); ofDrawCircle(lx,ly,5); }
        ofFill(); ofSetLineWidth(1);
    };

    float ly=subY;
    string pitchStr=!d.chordName.empty()?d.chordName:(d.noteOverride>=0?noteNameStr(d.noteOverride):"AUTO");
    ofSetColor(colCyan_); drawText14("PITCH",lSubX,ly+th14);
    ofSetColor(colYellow_); drawText14(pitchStr,lSubX+52.0f,ly+th14);
    drawLock14(lSubX+subW-8.0f,ly+th14/2.0f,d.lockNote);
    ly+=lineH;

    ofSetColor(colCyan_); drawText14("OCT",lSubX,ly+th14);
    ofSetColor(colYellow_); drawText14(ofToString(pianoOctave_),lSubX+52.0f,ly+th14);
    ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectRounded(lSubX+80,ly,20,lineH-4,3); ofFill(); ofSetColor(colCyan_); drawText14("<",lSubX+84,ly+th14);
    ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectRounded(lSubX+104,ly,20,lineH-4,3); ofFill(); ofSetColor(colCyan_); drawText14(">",lSubX+108,ly+th14);
    ly+=lineH;

    string chordLabel=d.chordName.empty()?"CHORD":d.chordName;
    if(d.chordPanelOpen){ ofSetColor(colPink_); } else { ofSetColor(colCyan_); }
    ofNoFill(); ofSetLineWidth(1); ofDrawRectRounded(lSubX,ly,subW-16,lineH-4,3); ofFill();
    ofSetColor(d.chordPanelOpen?colPink_:colCyan_);
    float ctw=jbFont14_?jbFont14_->stringWidth(chordLabel):(float)chordLabel.size()*8.4f;
    drawText14(chordLabel,lSubX+(subW-16-ctw)/2.0f,ly+th14);
    if(!d.chordName.empty()){
        ofSetColor(colPink_); ofFill(); ofDrawRectRounded(lSubX+subW-14,ly,14,lineH-4,3);
        ofSetColor(20); drawText14("x",lSubX+subW-11,ly+th14);
    }
    ly+=lineH;

    int vi2=voiceAtStep(stepIdx); bool allLocked=(vi2>=0)?voices_[vi2].allLocked:false;
    ofSetColor(allLocked?colPink_:colCyan_); ofNoFill(); ofSetLineWidth(1);
    ofDrawRectRounded(lSubX,ly,subW-16,lineH-4,3); ofFill();
    ofSetColor(allLocked?colPink_:colCyan_);
    string lockLabel=allLocked?"ALL UNLOCK":"ALL LOCK";
    float ltw=jbFont14_?jbFont14_->stringWidth(lockLabel):(float)lockLabel.size()*8.4f;
    drawText14(lockLabel,lSubX+(subW-16-ltw)/2.0f,ly+th14);

    float barW=18.0f, barH=subH-lineH-8.0f, barGap=8.0f, barY=subY+2.0f;
    float th12=jbFont12_?jbFont12_->stringHeight("Ag"):12.0f;
    float totalBarW=7.0f*(barW+barGap)-barGap;
    float bStartX=rSubX+(subW-totalBarW)/2.0f;

    auto drawVBar=[&](float bx,float val,float maxVal,bool locked,const char* label){
        float filled=val/maxVal;
        ofSetColor(colBg_); ofFill(); ofDrawRectangle(bx,barY,barW,barH);
        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY,barW,barH); ofFill();
        ofSetColor(locked?colPink_:colCyan_); ofFill(); ofDrawRectangle(bx,barY+barH*(1.0f-filled),barW,barH*filled);
        float lw=jbFont12_?jbFont12_->stringWidth(label):(float)strlen(label)*7.2f;
        ofSetColor(colCyan_); drawText12(label,bx+(barW-lw)/2.0f,barY+barH+th12+2.0f);
        ofSetColor(locked?colPink_:colCyan_);
        if(locked){ ofFill(); ofDrawCircle(bx+barW/2,barY-6,4); }
        else{ ofNoFill(); ofSetLineWidth(1); ofDrawCircle(bx+barW/2,barY-6,4); ofFill(); }
    };
    drawVBar(bStartX,              (float)d.velocity,127.0f,d.lockVelocity,  "VEL");
    drawVBar(bStartX+(barW+barGap),d.gate,           1.0f,  d.lockGate,      "GAT");
    drawVBar(bStartX+2*(barW+barGap),d.prob,         1.0f,  d.lockProb,      "PRB");

    auto drawStepBar=[&](float bx,int val,int maxVal,bool locked,const char* label){
        float btnH=(barH-((float)(maxVal-1))*2.0f)/(float)maxVal;
        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY,barW,barH); ofFill();
        for(int g=1;g<=maxVal;g++){
            float gy=barY+barH-(g*btnH+(g-1)*2);
            ofSetColor(g==val?colYellow_:colBg_); ofFill(); ofDrawRectangle(bx+1,gy,barW-2,btnH);
        }
        float lw=jbFont12_?jbFont12_->stringWidth(label):(float)strlen(label)*7.2f;
        ofSetColor(colCyan_); drawText12(label,bx+(barW-lw)/2.0f,barY+barH+th12+2.0f);
        ofSetColor(locked?colPink_:colCyan_);
        if(locked){ ofFill(); ofDrawCircle(bx+barW/2,barY-6,4); }
        else{ ofNoFill(); ofSetLineWidth(1); ofDrawCircle(bx+barW/2,barY-6,4); ofFill(); }
    };
    drawStepBar(bStartX+3*(barW+barGap),d.gridDiv,  8,d.lockGridDiv,  "GRD");
    drawStepBar(bStartX+4*(barW+barGap),d.noteRepeat,8,d.lockNoteRepeat,"RPT");

    auto drawToggleBar=[&](float bx,bool val,bool locked,const char* label){
        float glH=(barH-4)/2.0f;
        float onw=jbFont12_?jbFont12_->stringWidth("ON"):(float)2*7.2f;
        float ofw=jbFont12_?jbFont12_->stringWidth("OF"):(float)2*7.2f;
        ofSetColor(val?colCyan_:colBg_); ofFill(); ofDrawRectangle(bx,barY,barW,glH);
        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY,barW,glH); ofFill();
        ofSetColor(val?ofColor(20):colCyan_); drawText12("ON",bx+(barW-onw)/2.0f,barY+glH/2.0f+th12/2.0f);
        ofSetColor(!val?colCyan_:colBg_); ofFill(); ofDrawRectangle(bx,barY+glH+4,barW,glH);
        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(bx,barY+glH+4,barW,glH); ofFill();
        ofSetColor(!val?ofColor(20):colCyan_); drawText12("OF",bx+(barW-ofw)/2.0f,barY+glH+4+glH/2.0f+th12/2.0f);
        float lw=jbFont12_?jbFont12_->stringWidth(label):(float)strlen(label)*7.2f;
        ofSetColor(colCyan_); drawText12(label,bx+(barW-lw)/2.0f,barY+barH+th12+2.0f);
        ofSetColor(locked?colPink_:colCyan_);
        if(locked){ ofFill(); ofDrawCircle(bx+barW/2,barY-6,4); }
        else{ ofNoFill(); ofSetLineWidth(1); ofDrawCircle(bx+barW/2,barY-6,4); ofFill(); }
    };
    drawToggleBar(bStartX+5*(barW+barGap),d.glide,  d.lockGlide,"GLD");
    drawToggleBar(bStartX+6*(barW+barGap),d.strumOn,false,      "STR");

    if(d.chordPanelOpen){
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
        float visH=min((float)(nEntries*18+8),halfH-lineH*2-8.0f);
        float cpx=lSubX+subW+4.0f, cpy=subY+lineH*2;
        ofSetColor(22); ofFill(); ofDrawRectangle(cpx,cpy,160,visH);
        ofSetColor(colCyan_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(cpx,cpy,160,visH); ofFill();
        int visRows=(int)(visH/18);
        int selIdx=0; for(int i=0;i<nEntries;i++) if(d.chordName==entries2[i].lb){selIdx=i;break;}
        int scrollOff=ofClamp(selIdx-visRows/2,0,max(0,nEntries-visRows));
        for(int cc=0;cc<nEntries;cc++){
            int vy=cc-scrollOff; if(vy<0||vy>=visRows) continue;
            bool sel=(d.chordName==entries2[cc].lb);
            ofSetColor(sel?colYellow_:colBg_); ofFill(); ofDrawRectRounded(cpx+4,cpy+4+vy*18,152,16,2);
            ofSetColor(sel?ofColor(20):colCyan_); ofDrawBitmapString(entries2[cc].lb,cpx+8,cpy+15+vy*18);
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
    if(panelStep_ >= 0 && stepData_[panelStep_].chordPanelOpen){
        float sx,sy; getStepRect(panelStep_,uiY_,sx,sy);
        float ppw=230;
        float ppx=panelOnRight_?sx+stepW_+2:sx-ppw-2;
        if(ppx+ppw>originX_+SQUARE_W-2) ppx=sx-ppw-2; if(ppx<originX_+2) ppx=originX_+2;
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
        float cx=px+8,cy=py+38;
        float gly2=cy+62+22+22+26+22+22+22;
        float ocy2=gly2+22;
        float aby2=ocy2+26;
        float laby3=aby2+26;
        float scy3=laby3+28;
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
        if(x>=pix+piw+2&&x<=pix+piw+12&&y>=cy+14&&y<=cy+52){cp.pianoOctave=ofClamp(cp.pianoOctave+1,0,10);return;}
        if(y>=cy+14&&y<=cy+52&&x>=pix&&x<=pix+piw){
            int rootMidi=cp.pianoOctave*12; float keyW=piw/12.0f;
            int k=(int)((x-pix)/keyW);
            if(k>=0&&k<12){
                int midi=rootMidi+k;
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
                stepData_[i].lockOctShift=lk; stepData_[i].lockNote=lk;
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
        if(x>=oscX&&x<=oscX+oscW){ voices_[vbHit].oscIndex=(voices_[vbHit].oscIndex+1)%6; selectedOscVoice_=voices_[vbHit].oscIndex; }
        return;
    }

    // ステップボタントグル
    int stepIdx=stepAtPos(x,y,startY);
    if(stepIdx >= 0) {
        float sx,sy; getStepRect(stepIdx,startY,sx,sy);
        if(y>=sy+2&&y<=sy+20){
            bool insidePanel=false;
            if(panelStep_>=0){
                float psx,psy; getStepRect(panelStep_,startY,psx,psy);
                float ppw=230,pph=400;
                float ppx=panelOnRight_?psx+stepW_+2:psx-ppw-2; float ppy=psy;
                if(ppx+ppw>originX_+SQUARE_W-2) ppx=psx-ppw-2; if(ppx<originX_+2) ppx=originX_+2;
                if(ppy+pph>monitorTop_+SQUARE_W-10) ppy=monitorTop_+SQUARE_W-10-pph; if(ppy<startY+10) ppy=startY+10;
                if(x>=ppx&&x<=ppx+ppw&&y>=ppy&&y<=ppy+pph) insidePanel=true;
            }
            if(!insidePanel){
                if(panelStep_==stepIdx) panelStep_=-1;
                else { panelStep_=stepIdx; panelOnRight_=(stepIdx%COLS<COLS-3); }
                return;
            }
        }
    }

    // 開いているパネルの内部クリック
    if(panelStep_>=0){
        StepUIData& ds=stepData_[panelStep_];
        float dx,contentY,halfW,halfH;
        getPanelArea(dx,contentY,halfW,halfH);
        float pad=8.0f;

        float pianoX=dx+pad,pianoY2=contentY+pad;
        float pianoW2=halfW-pad*2,pianoH2=halfH-pad*2;
        float pix=pianoX+4,piw=pianoW2-8,piy=pianoY2+4;
        if(x>=pix-12&&x<=pix-2&&y>=piy&&y<=piy+pianoH2){ pianoOctave_=ofClamp(pianoOctave_-1,0,10); return; }
        if(x>=pix+piw+2&&x<=pix+piw+12&&y>=piy&&y<=piy+pianoH2){ pianoOctave_=ofClamp(pianoOctave_+1,0,10); return; }
        if(x>=pix&&x<=pix+piw&&y>=piy&&y<=piy+pianoH2){
            int rootMidi=pianoOctave_*12; float keyW=piw/12.0f;
            int k=(int)((x-pix)/keyW);
            if(k>=0&&k<12){int midi=rootMidi+k; int vi=voiceAtStep(panelStep_); if(vi<0||isScaleNote(midi,vi)) ds.noteOverride=(ds.noteOverride==midi)?-1:midi;}
            return;
        }

        float rX=dx+halfW+pad, rY=contentY+pad, rW=halfW-pad*2;
        float modeH=24.0f, modeW=(rW-8.0f)/3.0f;
        if(y>=rY&&y<=rY+modeH){
            if(x>=rX&&x<=rX+modeW)             {ds.mode=StepUIData::ON;   return;}
            if(x>=rX+modeW+4&&x<=rX+modeW*2+4) {ds.mode=StepUIData::OFF;  return;}
            if(x>=rX+modeW*2+8&&x<=rX+rW)      {ds.mode=StepUIData::SKIP; return;}
        }

        float subY2=rY+modeH+8.0f, subH2=halfH-modeH-pad*2-8.0f;
        float subW2=(rW-8.0f)/2.0f;
        float lSubX2=rX, rSubX2=rX+subW2+8.0f;
        float th14=jbFont14_?jbFont14_->stringHeight("Ag"):14.0f;
        float lineH2=th14+8.0f;
        float ly2=subY2;

        if(y>=ly2&&y<=ly2+lineH2){ if(abs(x-(lSubX2+subW2-8.0f))<=8){ds.lockNote=!ds.lockNote;return;} }
        ly2+=lineH2;
        if(y>=ly2&&y<=ly2+lineH2){
            if(x>=lSubX2+80&&x<=lSubX2+100){pianoOctave_=ofClamp(pianoOctave_-1,0,10);return;}
            if(x>=lSubX2+104&&x<=lSubX2+124){pianoOctave_=ofClamp(pianoOctave_+1,0,10);return;}
        }
        ly2+=lineH2;
        if(y>=ly2&&y<=ly2+lineH2){
            if(!ds.chordName.empty()&&x>=lSubX2+subW2-14&&x<=lSubX2+subW2){
                ds.chordName=""; ds.chordNoteCount=0;
                for(int p=0;p<7;p++) ds.chordNotes[p]=-1; return;
            }
            if(x>=lSubX2&&x<=lSubX2+subW2-16){ds.chordPanelOpen=!ds.chordPanelOpen;return;}
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
                    stepData_[i].lockOctShift=lk; stepData_[i].lockNote=lk;
                }
            }
            return;
        }

        if(ds.chordPanelOpen){
            float cpx2=lSubX2+subW2+4.0f, cpy2=subY2+lineH2*2;
            float visH2=min(400.0f,halfH-lineH2*2-8.0f);
            if(x>=cpx2&&x<=cpx2+160&&y>=cpy2&&y<=cpy2+visH2){
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
                int selIdx2=0; for(int i=0;i<ne2;i++) if(ds.chordName==entries2[i].lb){selIdx2=i;break;}
                int visRows2=(int)(visH2/18); int scrollOff2=ofClamp(selIdx2-visRows2/2,0,max(0,ne2-visRows2));
                int row2=(int)((y-cpy2)/18)+scrollOff2;
                if(row2>=0&&row2<ne2){
                    ds.chordName=entries2[row2].lb;
                    auto cd=harmony_.getChord(entries2[row2].tn,entries2[row2].rm);
                    ds.chordNoteCount=0;
                    for(int n=0;n<(int)cd.intervals.size()&&n<7;n++){ds.chordNotes[n]=cd.intervals[n];ds.chordNoteCount++;}
                }
                return;
            }
        }

        float barW2=18.0f, barH2=subH2-lineH2-8.0f, barGap2=8.0f, barY2=subY2+2.0f;
        float totalBarW2=7.0f*(barW2+barGap2)-barGap2;
        float bStartX2=rSubX2+(subW2-totalBarW2)/2.0f;
        for(int b=0;b<3;b++){
            float bx=bStartX2+b*(barW2+barGap2);
            if(abs(x-(bx+barW2/2))<=6&&abs(y-(barY2-6))<=6){
                if(b==0) ds.lockVelocity=!ds.lockVelocity;
                if(b==1) ds.lockGate=!ds.lockGate;
                if(b==2) ds.lockProb=!ds.lockProb;
                return;
            }
        }
        {float bx=bStartX2; if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){dragStep_=panelStep_;dragType_=0;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[panelStep_].velocity;return;}}
        {float bx=bStartX2+(barW2+barGap2); if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){dragStep_=panelStep_;dragType_=1;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[panelStep_].gate;return;}}
        {float bx=bStartX2+2*(barW2+barGap2); if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){dragStep_=panelStep_;dragType_=2;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[panelStep_].prob;return;}}
        {float bx=bStartX2+3*(barW2+barGap2); if(abs(x-(bx+barW2/2))<=6&&abs(y-(barY2-6))<=6){ds.lockGridDiv=!ds.lockGridDiv;return;}}
        {
            float bx=bStartX2+3*(barW2+barGap2);
            if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){
                float btnH=(barH2-7*2)/8.0f;
                int g=(int)((barY2+barH2-y)/(btnH+2))+1;
                if(g>=1&&g<=8){ds.gridDiv=g;return;}
            }
        }
        {float bx=bStartX2+4*(barW2+barGap2); if(abs(x-(bx+barW2/2))<=6&&abs(y-(barY2-6))<=6){ds.lockNoteRepeat=!ds.lockNoteRepeat;return;}}
        {
            float bx=bStartX2+4*(barW2+barGap2);
            if(x>=bx&&x<=bx+barW2&&y>=barY2&&y<=barY2+barH2){
                float btnH=(barH2-7*2)/8.0f;
                int r=(int)((barY2+barH2-y)/(btnH+2))+1;
                if(r>=1&&r<=8){ds.noteRepeat=r;return;}
            }
        }
        {float bx=bStartX2+5*(barW2+barGap2); if(abs(x-(bx+barW2/2))<=6&&abs(y-(barY2-6))<=6){ds.lockGlide=!ds.lockGlide;return;}}
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
            if(e-s+1>=MIN_VOICE_LEN) addVoiceRange(s,e);
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
        if(x>=cx&&x<=cx+pw-16&&y>=cy+14&&y<=cy+52){
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
    if(panelStep_<0) return;
    StepUIData& d=stepData_[panelStep_];
    float dx,contentY,halfW,halfH;
    getPanelArea(dx,contentY,halfW,halfH);
    float pad=8.0f;

    float pianoX=dx+pad,pianoY2=contentY+pad;
    float pianoW2=halfW-pad*2,pianoH2=halfH-pad*2;
    if(x>=pianoX&&x<=pianoX+pianoW2&&y>=pianoY2&&y<=pianoY2+pianoH2){
        if(d.chordNoteCount>0){
            int shift=(scrollY>0?12:-12);
            for(int p=0;p<d.chordNoteCount;p++) d.chordNotes[p]=ofClamp(d.chordNotes[p]+shift,9,120);
            if(d.noteOverride>=0) d.noteOverride=ofClamp(d.noteOverride+shift,9,120);
            d.octDelta+=scrollY>0?1:-1;
        }
        pianoOctave_=ofClamp(pianoOctave_+(scrollY>0?1:-1),0,10);
        return;
    }

    float rX=dx+halfW+pad, rY=contentY+pad, rW=halfW-pad*2;
    float modeH=24.0f, subY2=rY+modeH+8.0f, subH2=halfH-modeH-pad*2-8.0f;
    float subW2=(rW-8.0f)/2.0f, rSubX2=rX+subW2+8.0f;
    float th14=jbFont14_?jbFont14_->stringHeight("Ag"):14.0f;
    float lineH2=th14+8.0f;
    float barW2=18.0f, barH2=subH2-lineH2-8.0f, barGap2=8.0f, barY2=subY2+2.0f;
    float totalBarW2=7.0f*(barW2+barGap2)-barGap2;
    float bStartX2=rSubX2+(subW2-totalBarW2)/2.0f;
    if(x>=bStartX2&&x<=bStartX2+totalBarW2&&y>=barY2&&y<=barY2+barH2){
        float bx=bStartX2;
        if(x<=bx+barW2){d.velocity=ofClamp(d.velocity+(int)(scrollY*5),0,127);return;}
        bx+=barW2+barGap2;
        if(x<=bx+barW2){d.gate=ofClamp(d.gate+scrollY*0.05f,0.0f,1.0f);return;}
        bx+=barW2+barGap2;
        if(x<=bx+barW2){d.prob=ofClamp(d.prob+scrollY*0.05f,0.0f,1.0f);return;}
    }
}
