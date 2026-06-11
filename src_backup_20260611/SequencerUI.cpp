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
    ox = padX_ + col * stepW_;
    oy = startY + 50 + row * (stepH_ + 20);
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
        float bsx, bsy; getStepRect(voices_[v].startStep, startY, bsx, bsy);
        if(mx >= bsx && mx <= bsx + 290 && my >= bsy - 18 && my <= bsy - 2) return v;
    }
    return -1;
}

int SequencerUI::voiceRightEdgeAtPos(int mx, int my, float startY) const {
    for(int v = 0; v < voiceCount_; v++) {
        if(!voices_[v].active) continue;
        float ex, ey; getStepRect(voices_[v].endStep, startY, ex, ey);
        float rightX = ex + stepW_ - 4;
        if(mx >= rightX - 8 && mx <= rightX + 6 && my >= ey && my <= ey + stepH_ - 4) return v;
    }
    return -1;
}

void SequencerUI::getVoicePanelRect(int voiceIdx, float& px, float& py, float& pw, float& ph) const {
    pw = 230; ph = 480;
    float bsx, bsy; getStepRect(voices_[voiceIdx].startStep, uiY_, bsx, bsy);
    px = bsx + voices_[voiceIdx].panelOffsetX;
    py = bsy + stepH_ + 4 + voices_[voiceIdx].panelOffsetY;
}

// ========== draw ==========

void SequencerUI::draw(float startY) {
    uiY_ = startY;
    drawTopBar(startY);
    drawVoiceRanges(startY);
    drawGrid(startY);
    drawVoiceBadges(startY);  // バッジは最後（最前面）
    if(panelStep_ >= 0) drawPanel(panelStep_);
    for(int v = 0; v < voiceCount_; v++) {
        if(voices_[v].panelOpen) drawVoicePanel(v);
    }

    if((voiceDragging_ || voiceResizeIdx_ >= 0) && voiceDragStart_ >= 0 && voiceDragEnd_ >= 0) {
        int s = min(voiceDragStart_, voiceDragEnd_);
        int e = max(voiceDragStart_, voiceDragEnd_);
        for(int i = s; i <= e; i++) {
            if(voiceDragging_ && voiceAtStep(i) >= 0) continue;
            float sx, sy; getStepRect(i, startY, sx, sy);
            ofSetColor(colRed_.r, colRed_.g, colRed_.b, 60); ofFill();
            ofDrawRectangle(sx, sy, stepW_-4, stepH_-4);
        }
        for(int i = s; i <= e; ) {
            int rowStart = i, rowEnd = min(e, (i/COLS+1)*COLS-1);
            float sx0,sy0,sx1,sy1;
            getStepRect(rowStart,startY,sx0,sy0); getStepRect(rowEnd,startY,sx1,sy1);
            ofSetColor(colRed_); ofNoFill(); ofSetLineWidth(2);
            ofDrawRectangle(sx0,sy0,sx1-sx0+stepW_-4,stepH_-4); ofFill();
            i = rowEnd + 1;
        }
    }
}

void SequencerUI::drawTopBar(float startY) {
    ofSetColor(25); ofFill(); ofDrawRectangle(0,startY,1024,44);
    ofSetColor(colGold_); ofDrawBitmapString("SEQUENCER",12,startY+16);
    float bx=130,by=startY+8;
    ofSetColor(showVelocity_?colRed_:ofColor(55)); ofFill(); ofDrawRectRounded(bx,by,40,22,3);
    ofSetColor(showVelocity_?ofColor(20):colGold_); ofDrawBitmapString("VEL",bx+10,by+15);
    int skipCount=0;
    for(int i=0;i<TOTAL_STEPS;i++) if(stepData_[i].mode==StepUIData::SKIP) skipCount++;
    int active=TOTAL_STEPS-skipCount;
    float barX=200,barY=startY+14,barW=300,barH=12;
    ofSetColor(40); ofFill(); ofDrawRectangle(barX,barY,barW,barH);
    ofSetColor(colRed_); ofDrawRectangle(barX,barY,barW*((float)active/TOTAL_STEPS),barH);
    ofSetColor(colWhite_);
    ofDrawBitmapString("ACTIVE "+ofToString(active)+" / "+ofToString(TOTAL_STEPS),barX+barW+10,barY+10);

    // スマイルボタン（全VOICE先頭揃え）
    float smX=820, smY=startY+8, smR=13;
    ofSetColor(colGold_); ofFill(); ofDrawCircle(smX,smY+smR,smR);
    ofSetColor(20);
    ofDrawCircle(smX-5, smY+smR-3, 2.5f);
    ofDrawCircle(smX+5, smY+smR-3, 2.5f);
    ofNoFill(); ofSetLineWidth(2);
    ofBeginShape();
    for(int a=-40;a<=220;a+=10){
        float rad=ofDegToRad(a);
        ofVertex(smX+cos(rad)*7, smY+smR+sin(rad)*6+2);
    }
    ofEndShape(false);
    ofFill();
}

void SequencerUI::drawVoiceRanges(float startY) {
    ofColor voiceCols[6]={ofColor(200,60,60),ofColor(60,160,200),ofColor(60,200,120),
                          ofColor(200,160,60),ofColor(160,60,200),ofColor(200,100,60)};
    for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].active) continue;
        int s=voices_[v].startStep, e=voices_[v].endStep;
        ofColor vc=voiceCols[v%6];
        for(int i=s;i<=e;){
            int rowStart=i, rowEnd=min(e,(i/COLS+1)*COLS-1);
            float sx,sy,ex2,ey2;
            getStepRect(rowStart,startY,sx,sy); getStepRect(rowEnd,startY,ex2,ey2);
            ofSetColor(vc); ofNoFill(); ofSetLineWidth(2);
            ofDrawRectangle(sx-1,sy-1,(ex2-sx)+stepW_-2,stepH_-2);
            if(rowEnd == e) {
                float rightX = ex2 + stepW_ - 4;
                ofSetColor(vc); ofSetLineWidth(2);
                ofDrawLine(rightX+3, sy-1, rightX+3, sy+stepH_-2);
                ofDrawLine(rightX+6, sy-1, rightX+6, sy+stepH_-2);
            }
            ofFill();
            i = rowEnd + 1;
        }
    }
}

// バッジ描画を独立関数に（最前面描画用）
void SequencerUI::drawVoiceBadges(float startY) {
    ofColor voiceCols[6]={ofColor(200,60,60),ofColor(60,160,200),ofColor(60,200,120),
                          ofColor(200,160,60),ofColor(160,60,200),ofColor(200,100,60)};
    const float multVals[] = {0.5f,1.0f,2.0f,3.0f,4.0f};
    const char* multLabels[] = {"x.5","x1","x2","x3","x4"};
    for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].active) continue;
        ofColor vc=voiceCols[v%6];
        float bsx, bsy; getStepRect(voices_[v].startStep, startY, bsx, bsy);
        // バッジ本体
        ofSetColor(vc); ofFill();
        ofDrawRectRounded(bsx, bsy-18, 80, 16, 3);
        ofSetColor(20);
        string label = "V"+ofToString(v+1)+" OSC"+ofToString(voices_[v].oscIndex+1);
        ofDrawBitmapString(label, bsx+4, bsy-6);
        // STOP/GOボタン
        bool isPlaying = voices_[v].playing;
        ofSetColor(isPlaying ? ofColor(60,160,60) : ofColor(160,60,60)); ofFill();
        ofDrawRectRounded(bsx+84, bsy-18, 36, 16, 3);
        ofSetColor(20);
        ofDrawBitmapString(isPlaying ? "GO" : "STP", bsx+90, bsy-6);
        // BPM倍率ボタン
        for(int m=0;m<5;m++){
            bool act=(voices_[v].common.bpmMult==multVals[m]);
            ofSetColor(act?colGold_:ofColor(50)); ofFill();
            ofDrawRectRounded(bsx+124+m*26, bsy-18, 24, 16, 2);
            ofSetColor(act?ofColor(20):ofColor(150));
            ofDrawBitmapString(multLabels[m], bsx+125+m*26, bsy-6);
        }
        // DEL（右クリックで表示）
        if(voices_[v].delPending){
            ofSetColor(ofColor(200,40,40)); ofFill();
            ofDrawRectRounded(bsx+254, bsy-18, 30, 16, 3);
            ofSetColor(255);
            ofDrawBitmapString("DEL", bsx+256, bsy-6);
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
        if(globalStep>=0&&globalStep<TOTAL_STEPS){
            float sx,sy; getStepRect(globalStep,startY,sx,sy);
            ofSetColor(255,255,255,50); ofFill();
            ofDrawRectRounded(sx,sy,stepW_-4,stepH_-4,4);
        }
    }
}

void SequencerUI::drawStep(int idx, float x, float y) {
    StepUIData& d=stepData_[idx];
    bool isCurrent=false;
    for(int v=0;v<voiceCount_;v++){
        if(!voices_[v].active||v>=numSeqs_) continue;
        if(voices_[v].startStep+seqs_[v].getCurrentStep()==idx){isCurrent=true;break;}
    }
    ofSetColor(colDim_); ofFill(); ofDrawRectRounded(x,y,stepW_-4,stepH_-4,4);

    float btnW=stepW_-10,btnH=18,btnX=x+4,btnY=y+2;
    ofSetColor(panelStep_==idx?ofColor(20):colGold_); ofFill();
    ofDrawRectRounded(btnX,btnY,btnW,btnH,2);
    ofSetColor(ofColor(20));
    ofDrawBitmapString(ofToString(idx+1),btnX+3,btnY+13);

    float ledCX=x+stepW_/2-2,ledCY=y+22+ledR_;
    ofSetColor(colGold_.r,colGold_.g,colGold_.b,80); ofNoFill(); ofSetLineWidth(2);
    ofDrawCircle(ledCX,ledCY,ledR_+4); ofFill();

    ofColor ledCol;
    if(d.mode==StepUIData::SKIP)     ledCol=ofColor(30,30,30);
    else if(d.mode==StepUIData::OFF) ledCol=ofColor(55,18,18);
    else if(isCurrent){float t=fmod(ofGetElapsedTimef()*10.0f,1.0f);ledCol=ofColor((t<0.5f)?240:160,40,40);}
    else ledCol=ofColor(180,40,40);
    ofSetColor(ledCol); ofFill(); ofDrawCircle(ledCX,ledCY,ledR_);
    if(!d.chordName.empty()||d.noteOverride>=0){
        string od=d.octDelta>0?"+"+ofToString(d.octDelta):(d.octDelta<0?ofToString(d.octDelta):"0");
        ofSetColor(ofColor(225,185,0));
        ofDrawBitmapString(od,ledCX-3*(int)od.size(),ledCY+4);
    }

    if(d.mode==StepUIData::SKIP){
        ofSetColor(80); ofSetLineWidth(1.5);
        ofDrawLine(ledCX-7,ledCY-7,ledCX+7,ledCY+7);
        ofDrawLine(ledCX+7,ledCY-7,ledCX-7,ledCY+7);
    }
    if(showVelocity_) drawVelocityBar(idx,x,y);
    else{
        float vbx=x+4,vby=y+stepH_-17;
        ofSetColor(colGold_); ofFill(); ofDrawRectRounded(vbx,vby,stepW_-12,13,2);
        string dispLabel=d.chordName.empty()?(d.noteOverride>=0?noteNameStr(d.noteOverride+d.octShift*12):"--"):d.chordName;
        float tx=d.chordName.empty()?vbx+(stepW_-12-(float)dispLabel.size()*6)*0.5f:vbx+2;
        ofSetColor(ofColor(20)); ofDrawBitmapString(dispLabel,tx,vby+10);
    }
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

    // スケール選択UI
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
    // スケール種類展開
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
    if(stepIdx<0||stepIdx>=TOTAL_STEPS) return;
    StepUIData& d=stepData_[stepIdx];
    float sx,sy; getStepRect(stepIdx,uiY_,sx,sy);
    float pw=230,ph=400;
    float px=panelOnRight_?sx+stepW_+2:sx-pw-2;
    float py=sy;
    if(px+pw>1022) px=sx-pw-2; if(px<2) px=2;
    if(py+ph>1190) py=1190-ph; if(py<uiY_+50) py=uiY_+50;
    ofSetColor(22); ofFill(); ofDrawRectRounded(px,py,pw,ph,5);
    ofSetColor(colGold_); ofNoFill(); ofSetLineWidth(1); ofDrawRectRounded(px,py,pw,ph,5); ofFill();
    ofSetColor(colGold_); ofDrawBitmapString("STEP "+ofToString(stepIdx+1),px+8,py+15);
    float cx=px+8,cy=py+22,bw=52,bh=18;
    auto drawModeBtn=[&](float x,float y,string label,bool active){
        ofSetColor(active?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(x,y,bw,bh,3);
        ofSetColor(active?ofColor(20):colGold_);
        ofDrawBitmapString(label,x+bw/2-(float)label.size()*4,y+bh/2+4);
    };
    drawModeBtn(cx,    cy,"ON",  d.mode==StepUIData::ON);
    drawModeBtn(cx+58, cy,"OFF", d.mode==StepUIData::OFF);
    drawModeBtn(cx+116,cy,"SKIP",d.mode==StepUIData::SKIP);
    float pianoY=cy+bh+8;
    ofSetColor(colWhite_); ofDrawBitmapString("PITCH",cx,pianoY+10);
    string pitchStr=!d.chordName.empty()?d.chordName:(d.noteOverride>=0?noteNameStr(d.noteOverride):"AUTO");
    string octStr=d.octDelta>0?"+"+ofToString(d.octDelta):(d.octDelta<0?ofToString(d.octDelta):"0");
    ofSetColor(colGold_);
    ofDrawBitmapString(pitchStr+" ("+octStr+")",cx+50,pianoY+10);
    ofDrawBitmapString("OCT"+ofToString(pianoOctave_),cx+150,pianoY+10);
    drawMiniPiano(cx,pianoY+14,pw-16,38,stepIdx,false,voiceAtStep(stepIdx));
    float sliderX=cx+48,sliderW=pw-76,sliderH=12;
    float lockX=px+pw-12;
    // ロックボタン描画ラムダ
    auto drawLock=[&](float x,float y,bool locked){
        if(locked){
            ofSetColor(colRed_); ofFill(); ofDrawCircle(x,y,5);
            ofSetColor(colRed_); ofSetLineWidth(1.5);
            ofDrawLine(x-5,y-5,x+5,y+5);
        } else {
            ofSetColor(180); ofNoFill(); ofSetLineWidth(1.5); ofDrawCircle(x,y,5);
        }
        ofFill(); ofSetLineWidth(1);
    };
    float vy=pianoY+62,gy=vy+22,pry=gy+22;
    ofSetColor(colWhite_); ofDrawBitmapString("VEL",cx,vy+10);
    ofSetColor(45); ofFill(); ofDrawRectangle(sliderX,vy,sliderW,sliderH);
    ofSetColor(colRed_); ofDrawRectangle(sliderX,vy,sliderW*(d.velocity/127.0f),sliderH);
    ofSetColor(0,0,0,150); ofFill(); ofDrawRectangle(sliderX+sliderW/2-16,vy,32,sliderH);
    ofSetColor(colGold_); ofDrawBitmapString(ofToString(d.velocity),sliderX+sliderW/2-14,vy+sliderH-2);
    drawLock(lockX,vy+sliderH/2,d.lockVelocity);
    ofSetColor(colWhite_); ofDrawBitmapString("GATE",cx,gy+10);
    ofSetColor(45); ofFill(); ofDrawRectangle(sliderX,gy,sliderW,sliderH);
    ofSetColor(colRed_); ofDrawRectangle(sliderX,gy,sliderW*d.gate,sliderH);
    ofSetColor(0,0,0,150); ofFill(); ofDrawRectangle(sliderX+sliderW/2-16,gy,32,sliderH);
    ofSetColor(colGold_); ofDrawBitmapString(ofToString(d.gate,2),sliderX+sliderW/2-14,gy+sliderH-2);
    drawLock(lockX,gy+sliderH/2,d.lockGate);
    ofSetColor(colWhite_); ofDrawBitmapString("PROB",cx,pry+10);
    ofSetColor(45); ofFill(); ofDrawRectangle(sliderX,pry,sliderW,sliderH);
    ofSetColor(colRed_); ofDrawRectangle(sliderX,pry,sliderW*d.prob,sliderH);
    ofSetColor(0,0,0,150); ofFill(); ofDrawRectangle(sliderX+sliderW/2-16,pry,32,sliderH);
    ofSetColor(colGold_); ofDrawBitmapString(ofToString((int)(d.prob*100))+"%",sliderX+sliderW/2-14,pry+sliderH-2);
    drawLock(lockX,pry+sliderH/2,d.lockProb);
    float gry=pry+26;
    ofSetColor(colWhite_); ofDrawBitmapString("GRID",cx,gry+10);
    for(int g=1;g<=8;g++){
        float gbx=cx+48+(g-1)*18; bool act=(d.gridDiv==g);
        ofSetColor(act?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(gbx,gry,16,16,2);
        ofSetColor(act?ofColor(20):colGold_); ofDrawBitmapString(ofToString(g),gbx+4,gry+12);
    }
    drawLock(lockX,gry+8,d.lockGridDiv);
    float nry=gry+22;
    ofSetColor(colWhite_); ofDrawBitmapString("REPT",cx,nry+10);
    for(int r=1;r<=8;r++){
        float rbx=cx+48+(r-1)*18; bool act=(d.noteRepeat==r);
        ofSetColor(act?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(rbx,nry,16,16,2);
        ofSetColor(act?ofColor(20):colGold_); ofDrawBitmapString(ofToString(r),rbx+4,nry+12);
    }
    drawLock(lockX,nry+8,d.lockNoteRepeat);
    float gly=nry+22;
    ofSetColor(colWhite_); ofDrawBitmapString("GLIDE",cx,gly+10);
    auto drawToggle=[&](float tx,float ty,string label,bool active){
        ofSetColor(active?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(tx,ty,42,16,2);
        ofSetColor(active?ofColor(20):colGold_); ofDrawBitmapString(label,tx+8,ty+12);
    };
    drawToggle(cx+48,gly,"ON", d.glide);
    drawToggle(cx+96,gly,"OFF",!d.glide);
    drawLock(lockX,gly+8,d.lockGlide);
    float ocy=gly+22;
    ofSetColor(colWhite_); ofDrawBitmapString("OCT",cx,ocy+10);
    const int octVals[]={-2,-1,0,1,2};
    for(int o=0;o<5;o++){
        float obx=cx+48+o*28; bool act=(d.octShift==octVals[o]);
        ofSetColor(act?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(obx,ocy,24,16,2);
        ofSetColor(act?ofColor(20):colGold_);
        string ol=(octVals[o]>0?"+":"")+ofToString(octVals[o]);
        ofDrawBitmapString(ol,obx+3,ocy+12);
    }
    drawLock(lockX,ocy+8,d.lockOctShift);
    // PITCHロック
    drawLock(lockX,pianoY+6,d.lockNote);
    // DEFボタン
    float defy=ocy+26;
    ofSetColor(ofColor(60,60,60)); ofFill(); ofDrawRectRounded(cx,defy,pw-16,18,3);
    ofSetColor(colWhite_); ofDrawBitmapString("DEF - unlock all",cx+8,defy+13);

    // ストラムON/OFF
    float strmy=defy+24;
    ofSetColor(colWhite_); ofDrawBitmapString("STRUM",cx,strmy+10);
    bool son=d.strumOn;
    ofSetColor(son?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(cx+48,strmy,42,16,2);
    ofSetColor(son?ofColor(20):colGold_); ofDrawBitmapString("ON",cx+56,strmy+12);
    bool soff=!d.strumOn;
    ofSetColor(soff?colRed_:ofColor(50)); ofFill(); ofDrawRectRounded(cx+96,strmy,42,16,2);
    ofSetColor(soff?ofColor(20):colGold_); ofDrawBitmapString("OFF",cx+104,strmy+12);

    // コードボタン
    float chordy=strmy+24;
    ofSetColor(colWhite_); ofDrawBitmapString("CHORD",cx,chordy+10);
    float chordBtnW=d.chordName.empty()?pw-64:pw-82;
    ofSetColor(d.chordPanelOpen?colGold_:ofColor(50)); ofFill(); ofDrawRectRounded(cx+48,chordy,chordBtnW,16,2);
    ofSetColor(d.chordPanelOpen?ofColor(20):colWhite_);
    string cLabel=d.chordName.empty()?"SELECT":d.chordName;
    ofDrawBitmapString(cLabel,cx+52,chordy+12);
    if(!d.chordName.empty()){
        ofSetColor(ofColor(220,50,50)); ofFill(); ofDrawRectRounded(cx+pw-32,chordy,14,14,2);
        ofSetColor(colWhite_); ofDrawBitmapString("x",cx+pw-28,chordy+11);
    }

    // コードパネル展開
    if(d.chordPanelOpen){
        int vi=voiceAtStep(stepIdx);
        int scaleRoot=(vi>=0)?voices_[vi].common.scaleRoot:0;
        string scaleName=(vi>=0)?voices_[vi].common.scaleName:"naturalMinor";
        int octave=(vi>=0)?voices_[vi].common.pianoOctave:4;
        // スケール音をルートとしてコードリストを生成
        auto scaleNotes=harmony_.getAvailableNotes(scaleName,scaleRoot);
        std::vector<int> octaveNotes;
        int startMidi=octave*12;
        for(int n:scaleNotes) if(n>=startMidi&&n<startMidi+12) octaveNotes.push_back(n);
        const char* chordTypes[]={"maj","min","dim","aug","sus2","sus4","maj7","min7","dom7","dim7","m7b5","pow5","pow5oct"};
        const char* chordLabels[]={"maj","min","dim","aug","sus2","sus4","maj7","min7","dom7","dim7","m7b5","pow5","p5+8"};
        int nTypes=13;
        const char* noteNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        // エントリー生成
        struct ChordEntry{ int rootMidi; string typeName; string label; };
        std::vector<ChordEntry> entries;
        // スケール音のピッチクラスセット
        std::set<int> scalePcSet;
        for(int n:scaleNotes) scalePcSet.insert(n%12);
        // インターバルテーブル（相対値）
        const int ivTable[13][7]={
            {0,4,7,-1,-1,-1,-1},   // maj
            {0,3,7,-1,-1,-1,-1},   // min
            {0,3,6,-1,-1,-1,-1},   // dim
            {0,4,8,-1,-1,-1,-1},   // aug
            {0,2,7,-1,-1,-1,-1},   // sus2
            {0,5,7,-1,-1,-1,-1},   // sus4
            {0,4,7,11,-1,-1,-1},   // maj7
            {0,3,7,10,-1,-1,-1},   // min7
            {0,4,7,10,-1,-1,-1},   // dom7
            {0,3,6,9,-1,-1,-1},    // dim7
            {0,3,6,10,-1,-1,-1},   // m7b5
            {0,7,-1,-1,-1,-1,-1},  // pow5
            {0,7,12,-1,-1,-1,-1},  // pow5oct
        };
        for(int n:octaveNotes){
            int rootPc=n%12;
            for(int t=0;t<nTypes;t++){
                bool valid=true;
                for(int i=0;i<7;i++){
                    if(ivTable[t][i]<0) break;
                    int pc=(rootPc+ivTable[t][i])%12;
                    if(scalePcSet.find(pc)==scalePcSet.end()){valid=false;break;}
                }
                if(!valid) continue;
                ChordEntry e;
                e.rootMidi=n;
                e.typeName=chordTypes[t];
                e.label=string(noteNames[n%12])+chordLabels[t];
                entries.push_back(e);
            }
        }
        int nEntries=(int)entries.size();
        float cpanelH=nEntries*18+8;
        float cpx=panelOnRight_?px+pw+4:px-204;
        float cpyRaw=py+ph-cpanelH;
        float visH=min(cpanelH,(float)(1190-cpyRaw));
        float cpy=cpyRaw;
        ofSetColor(22); ofFill(); ofDrawRectangle(cpx,cpy,200,cpanelH);
        ofSetColor(colGold_); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(cpx,cpy,200,cpanelH); ofFill();
        int visRows=(int)(visH/18);
        int selIdx=0; for(int i=0;i<nEntries;i++) if(d.chordName==entries[i].label){selIdx=i;break;}
        int scrollOff=ofClamp(selIdx-visRows/2,0,max(0,nEntries-visRows));
        for(int c=0;c<nEntries;c++){
            int vy=c-scrollOff;
            if(vy<0||vy>=visRows) continue;
            bool sel=(d.chordName==entries[c].label);
            ofSetColor(sel?colGold_:ofColor(40)); ofFill(); ofDrawRectRounded(cpx+4,cpy+4+vy*18,192,16,2);
            ofSetColor(sel?ofColor(20):colWhite_); ofDrawBitmapString(entries[c].label,cpx+8,cpy+15+vy*18);
        }
    }
}

// ========== mousePressed ==========

void SequencerUI::mousePressed(int x,int y,int button){
    // スマイルボタン：全VOICE masterClock リセット要求
    float smX=820, smY=uiY_+8, smR=13;
    float dx=x-smX, dy=y-(smY+smR);
    if(dx*dx+dy*dy <= smR*smR) {
        resetMasterClock_ = true;
        return;
    }
    // コードパネル先行判定
    if(panelStep_ >= 0 && stepData_[panelStep_].chordPanelOpen){
        float sx,sy; getStepRect(panelStep_,uiY_,sx,sy);
        float ppw=230;
        float ppx=panelOnRight_?sx+stepW_+2:sx-ppw-2;
        if(ppx+ppw>1022) ppx=sx-ppw-2; if(ppx<2) ppx=2;
        float ppy2=sy;
        if(ppy2+400>1190) ppy2=1190-400;
        if(ppy2<uiY_+50)  ppy2=uiY_+50;
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
                    // スケール音をステップに順番に割り当て
                    int defRoot = voices_[v].common.scaleRoot + 48; // オクターブ4から開始
                    auto notes = harmony_.getAvailableNotes(sn[s], defRoot % 12);
                    // オクターブ4以上の音だけ使う
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

    if(x>=130&&x<=170&&y>=startY+8&&y<=startY+30){ showVelocity_=!showVelocity_; return; }

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
                    // スケール音をステップに順番に割り当て
                    int defRoot = voices_[v].common.scaleRoot + 48; // オクターブ4から開始
                    auto notes = harmony_.getAvailableNotes(sn[s], defRoot % 12);
                    // オクターブ4以上の音だけ使う
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

    // 右端2重線クリック→リサイズ開始
    int reHit=voiceRightEdgeAtPos(x,y,startY);
    if(reHit>=0){
        voiceResizeIdx_=reHit;
        voiceDragStart_=voices_[reHit].startStep;
        voiceDragEnd_  =voices_[reHit].endStep;
        return;
    }

    // バッジクリック
    int vbHit=voiceBadgeAtPos(x,y,startY);
    if(vbHit>=0){
        float bsx,bsy; getStepRect(voices_[vbHit].startStep,startY,bsx,bsy);
        // 右クリック→delPendingトグル
        if(button==2){
            voices_[vbHit].delPending=!voices_[vbHit].delPending;
            return;
        }
        // DELボタン（delPending時のみ有効）
        if(voices_[vbHit].delPending&&x>=bsx+254&&x<=bsx+284){
            removeVoice(vbHit);
            return;
        }
        // STOP/GOボタン
        if(x>=bsx+84&&x<=bsx+120){
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
        // BPM倍率ボタン
        const float multVals[] = {0.5f,1.0f,2.0f,3.0f,4.0f};
        for(int m=0;m<5;m++){
            if(x>=bsx+124+m*26&&x<=bsx+148+m*26){
                voices_[vbHit].common.bpmMult=multVals[m];
                return;
            }
        }
        // バッジ本体：左半分でパネルトグル、右半分でOSCサイクル
        if(x>=bsx+50&&x<=bsx+80){ voices_[vbHit].oscIndex=(voices_[vbHit].oscIndex+1)%6; selectedOscVoice_=voices_[vbHit].oscIndex; }
        else                      voices_[vbHit].panelOpen=!voices_[vbHit].panelOpen;
        return;
    }

    // ステップボタントグル
    int stepIdx=stepAtPos(x,y,startY);
    if(stepIdx >= 0) {
        float sx,sy; getStepRect(stepIdx,startY,sx,sy);
        if(y>=sy+2&&y<=sy+20){
            // パネルが開いてて今クリックがパネル内なら無視
            bool insidePanel=false;
            if(panelStep_>=0){
                float psx,psy; getStepRect(panelStep_,startY,psx,psy);
                float ppw=230,pph=400;
                float ppx=panelOnRight_?psx+stepW_+2:psx-ppw-2; float ppy=psy;
                if(ppx+ppw>1022) ppx=psx-ppw-2; if(ppx<2) ppx=2;
                if(ppy+pph>1190) ppy=1190-pph; if(ppy<startY+50) ppy=startY+50;
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
        float sx,sy; getStepRect(panelStep_,startY,sx,sy);
        float pw=230,ph=400;
        float px=panelOnRight_?sx+stepW_+2:sx-pw-2; float py=sy;
        if(px+pw>1022) px=sx-pw-2; if(px<2) px=2;
        if(py+ph>1190) py=1190-ph; if(py<startY+50) py=startY+50;
        if(x>=px&&x<=px+pw&&y>=py&&y<=py+ph){
            StepUIData& d=stepData_[panelStep_];
            float cx=px+8,cy=py+22,bw=52,bh=18;
            if(y>=cy&&y<=cy+bh){
                if(x>=cx    &&x<=cx+bw)    {d.mode=StepUIData::ON;   return;}
                if(x>=cx+58 &&x<=cx+58+bw) {d.mode=StepUIData::OFF;  return;}
                if(x>=cx+116&&x<=cx+168)   {d.mode=StepUIData::SKIP; return;}
            }
            float pianoY=cy+bh+8; float pix=cx,piw=pw-16;
            if(x>=pix-12&&x<=pix-2&&y>=pianoY+14&&y<=pianoY+52){pianoOctave_=ofClamp(pianoOctave_-1,0,10);return;}

            if(x>=pix+piw+2&&x<=pix+piw+12&&y>=pianoY+14&&y<=pianoY+52){pianoOctave_=ofClamp(pianoOctave_+1,0,10);return;}
            if(y>=pianoY+14&&y<=pianoY+52&&x>=pix&&x<=pix+piw){
                int rootMidi=pianoOctave_*12; float keyW=piw/12.0f;
                int k=(int)((x-pix)/keyW);
                if(k>=0&&k<12){int midi=rootMidi+k; int vi=voiceAtStep(panelStep_); if(vi<0||isScaleNote(midi,vi)) stepData_[panelStep_].noteOverride=(stepData_[panelStep_].noteOverride==midi)?-1:midi;}
                return;
            }
            float sliderX=cx+48,sliderW=pw-60,sliderH=12;
            float vy=pianoY+62,gy=vy+22,pry=gy+22;
            float lockX2=px+pw-14;
            // ロックボタンを先にチェック
            if(abs(x-lockX2)<=6){
                if(abs(y-(vy+sliderH/2))<=10){stepData_[panelStep_].lockVelocity=!stepData_[panelStep_].lockVelocity;return;}
                if(abs(y-(gy+sliderH/2))<=10){stepData_[panelStep_].lockGate=!stepData_[panelStep_].lockGate;return;}
                if(abs(y-(pry+sliderH/2))<=10){stepData_[panelStep_].lockProb=!stepData_[panelStep_].lockProb;return;}
            }
            if(y>=vy  &&y<=vy+sliderH  &&x>=sliderX&&x<=sliderX+sliderW){dragStep_=panelStep_;dragType_=0;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[panelStep_].velocity;return;}
            if(y>=gy  &&y<=gy+sliderH  &&x>=sliderX&&x<=sliderX+sliderW){dragStep_=panelStep_;dragType_=1;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[panelStep_].gate;return;}
            if(y>=pry &&y<=pry+sliderH &&x>=sliderX&&x<=sliderX+sliderW){dragStep_=panelStep_;dragType_=2;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[panelStep_].prob;return;}
            float gry=pry+26;
            if(y>=gry&&y<=gry+16){for(int g=1;g<=8;g++){float gbx=cx+48+(g-1)*18;if(x>=gbx&&x<=gbx+16){stepData_[panelStep_].gridDiv=g;return;}}}
            float nry=gry+22;
            if(y>=nry&&y<=nry+16){for(int r=1;r<=8;r++){float rbx=cx+48+(r-1)*18;if(x>=rbx&&x<=rbx+16){stepData_[panelStep_].noteRepeat=r;return;}}}
            float gly=nry+22;
            if(y>=gly&&y<=gly+16){if(x>=cx+48&&x<=cx+90){stepData_[panelStep_].glide=true;return;} if(x>=cx+96&&x<=cx+138){stepData_[panelStep_].glide=false;return;}}
            float ocy=gly+22;
            if(y>=ocy&&y<=ocy+16){const int ov[]={-2,-1,0,1,2};for(int o=0;o<5;o++){float obx=cx+48+o*28;if(x>=obx&&x<=obx+24){stepData_[panelStep_].octShift=ov[o];return;}}}
            float defy=ocy+26;
            float lockX=px+pw-14;
            StepUIData& ds=stepData_[panelStep_];
            // DEFボタン
            if(y>=defy&&y<=defy+18&&x>=cx&&x<=cx+pw-16){
                ds.lockVelocity=false; ds.lockGate=false; ds.lockProb=false;
                ds.lockGridDiv=false; ds.lockNoteRepeat=false; ds.lockGlide=false;
                ds.lockOctShift=false; ds.lockNote=false; return;
            }
            // ストラムON/OFF
            float strmy2=defy+24;
            if(y>=strmy2&&y<=strmy2+16){
                if(x>=cx+48&&x<=cx+90){ds.strumOn=true;return;}
                if(x>=cx+96&&x<=cx+138){ds.strumOn=false;return;}
            }
            // コードボタン開閉
            float chordy2=strmy2+24;
            if(!ds.chordName.empty()&&y>=chordy2&&y<=chordy2+14&&x>=cx+pw-32&&x<=cx+pw-18){
                ds.chordName=""; ds.chordNoteCount=0;
                for(int p=0;p<7;p++) ds.chordNotes[p]=-1;
                return;
            }
            if(y>=chordy2&&y<=chordy2+16&&x>=cx+48&&x<=cx+pw-16){
                ds.chordPanelOpen=!ds.chordPanelOpen;return;
            }
            // コードパネル選択
            if(ds.chordPanelOpen){
                int vi=voiceAtStep(panelStep_);
                int scaleRoot=(vi>=0)?voices_[vi].common.scaleRoot:0;
                const char* ck[]={"maj","min","dim","aug","sus2","sus4","maj6","min6","maj7","min7","dom7","dim7","m7b5","minmaj7","aug7","augmaj7","dom9","maj9","min9","dom7b9","dom7s9","add9","minAdd9","dom11","min11","dom13","min13","dom7alt","pow5","pow5oct"};
                float cpx=px+pw+4,cpy=py;
                if(x>=cpx+4&&x<=cpx+196&&y>=cpy+4&&y<=cpy+4+30*20){
                    int c=(int)((y-cpy-4)/20);
                    if(c>=0&&c<30){
                        ds.chordName=ck[c];
                        auto cd=harmony_.getChord(ck[c],scaleRoot);
                        ds.chordNoteCount=0;
                        for(int n=0;n<(int)cd.intervals.size()&&n<7;n++){
                            ds.chordNotes[n]=cd.intervals[n];
                            ds.chordNoteCount++;
                        }
                        return;
                    }
                }
            }
            // ロックボタン（各パラメーター行）
            float pianoY2=cy+bh+8;
            float vy2=pianoY2+62,gy2=vy2+22,pry2=gy2+22;
            float gry2=pry2+26,nry2=gry2+22,gly2=nry2+22,ocy2=gly2+22;
            if(abs(x-lockX)<=12){
                if(abs(y-(pianoY2+6))<=12){ds.lockNote=!ds.lockNote;return;}

                if(abs(y-(gry2+8))<=12){ds.lockGridDiv=!ds.lockGridDiv;return;}
                if(abs(y-(nry2+8))<=12){ds.lockNoteRepeat=!ds.lockNoteRepeat;return;}
                if(abs(y-(gly2+8))<=12){ds.lockGlide=!ds.lockGlide;return;}
                if(abs(y-(ocy2+8))<=12){ds.lockOctShift=!ds.lockOctShift;return;}
            }
            return;
        } else {
            panelStep_=-1;
        }
    }

    if(stepIdx<0) return;
    float sx,sy; getStepRect(stepIdx,startY,sx,sy);
    float ledCX=sx+stepW_/2-2,ledCY=sy+22+ledR_;
    float ddx=x-ledCX,ddy=y-ledCY;
    if(sqrt(ddx*ddx+ddy*ddy)<=ledR_+3){cycleStepMode(stepIdx);return;}
    if(showVelocity_){
        float barX=sx+4,barY=sy+stepH_-4-14;
        if(x>=barX&&x<=barX+stepW_-12&&y>=barY&&y<=barY+14){
            dragStep_=stepIdx;dragType_=0;dragIsVoice_=false;dragStartY_=y;dragStartVal_=stepData_[stepIdx].velocity;return;
        }
    }
    if(voiceAtStep(stepIdx)>=0) return;
    voiceDragging_=true; voiceDragStart_=stepIdx; voiceDragEnd_=stepIdx;
}

void SequencerUI::mouseDragged(int x,int y,int button){
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
    if(voiceResizeIdx_>=0){
        int s=stepAtPos(x,y,uiY_);
        if(s>=0 && s>=voices_[voiceResizeIdx_].startStep &&
           !rangeOverlapsExcept(voices_[voiceResizeIdx_].startStep,s,voiceResizeIdx_)){
            voiceDragEnd_=s;
        }
        return;
    }
    if(voiceDragging_){
        int s=stepAtPos(x,y,uiY_);
        if(s>=0&&voiceAtStep(s)<0) voiceDragEnd_=s;
    }
}

void SequencerUI::mouseReleased(int x,int y,int button){
    panelDragVoice_=-1;
    dragStep_=-1; dragType_=-1; dragIsVoice_=false; dragVoiceIdx_=-1;
    if(voiceResizeIdx_>=0){
        int newEnd=voiceDragEnd_;
        if(newEnd >= voices_[voiceResizeIdx_].startStep &&
           !rangeOverlapsExcept(voices_[voiceResizeIdx_].startStep,newEnd,voiceResizeIdx_)){
            voices_[voiceResizeIdx_].endStep=newEnd;
            if(voiceResizeIdx_<numSeqs_)
                seqs_[voiceResizeIdx_].setLoopRange(0, newEnd - voices_[voiceResizeIdx_].startStep);
        }
        voiceResizeIdx_=-1; voiceDragStart_=-1; voiceDragEnd_=-1;
        return;
    }
    if(voiceDragging_){
        if(voiceDragStart_>=0&&voiceDragEnd_>=0&&voiceDragStart_!=voiceDragEnd_){
            int s=min(voiceDragStart_,voiceDragEnd_);
            int e=max(voiceDragStart_,voiceDragEnd_);
            addVoiceRange(s,e);
        }
        voiceDragging_=false; voiceDragStart_=-1; voiceDragEnd_=-1;
    }
}

void SequencerUI::mouseScrolled(int x,int y,float scrollX,float scrollY){
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
        // スケール種類スクロール
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
    float sx,sy; getStepRect(panelStep_,uiY_,sx,sy);
    float pw=230;
    float px=panelOnRight_?sx+stepW_+2:sx-pw-2;
    if(px+pw>1022) px=sx-pw-2; if(px<2) px=2;
    float cx=px+8,cy=(sy>uiY_+50)?sy:uiY_+50;
    float bh=18,pianoY=cy+22+bh+8;
    if(x>=cx&&x<=cx+pw-16&&y>=pianoY+14&&y<=pianoY+52){
        StepUIData& ds=stepData_[panelStep_];
        if(ds.chordNoteCount>0){
            int shift=(scrollY>0?12:-12);
            for(int p=0;p<ds.chordNoteCount;p++) ds.chordNotes[p]=ofClamp(ds.chordNotes[p]+shift,9,120);
            if(ds.noteOverride>=0) ds.noteOverride=ofClamp(ds.noteOverride+shift,9,120);
            ds.octDelta+=scrollY>0?1:-1;
            pianoOctave_=ofClamp(pianoOctave_+(scrollY>0?1:-1),0,10);
        } else {
            ds.noteOverride=ofClamp(ds.noteOverride+(scrollY>0?12:-12),9,120);
            ds.octDelta+=scrollY>0?1:-1;
            pianoOctave_=ofClamp(pianoOctave_+(scrollY>0?1:-1),0,10);
        }
        return;
    }
    float sliderX=cx+48,sliderW=pw-60;
    float vy=pianoY+62,gy=vy+22,pry=gy+22;
    StepUIData& d=stepData_[panelStep_];
    if(x>=sliderX&&x<=sliderX+sliderW){
        if(y>=vy &&y<=vy+12)  d.velocity=ofClamp(d.velocity+(int)(scrollY*5),0,127);
        if(y>=gy &&y<=gy+12)  d.gate    =ofClamp(d.gate   +scrollY*0.05f,0.0f,1.0f);
        if(y>=pry&&y<=pry+12) d.prob    =ofClamp(d.prob   +scrollY*0.05f,0.0f,1.0f);
    }
    float py=sy;
    if(panelStep_>=0 && stepData_[panelStep_].chordPanelOpen){
        // PITCH行とCHORD行上では無視
        float sliderX2=cx+48,sliderW2=pw-60;
        float vy2=pianoY+62,gy2=vy2+22,pry2=gy2+22;
        float gry2=pry2+26,nry2=gry2+22,gly2=nry2+22,ocy2=gly2+22,defy2=ocy2+26;
        float strmy2=defy2+24,chordy2=strmy2+24;
        if(y>=pianoY&&y<=pianoY+52) return;
        if(y>=chordy2&&y<=chordy2+16) return;
        float cpx2=panelOnRight_?px+pw+4:px-204;

        int vi2=voiceAtStep(panelStep_);
        int sr2=(vi2>=0)?voices_[vi2].common.scaleRoot:0;
        string sn2=(vi2>=0)?voices_[vi2].common.scaleName:"naturalMinor";
        int oc2=(vi2>=0)?voices_[vi2].common.pianoOctave:4;
        auto scNotes=harmony_.getAvailableNotes(sn2,sr2);
        std::vector<int> onotes; int sm2=oc2*12;
        for(int n:scNotes) if(n>=sm2&&n<sm2+12) onotes.push_back(n);
        const char* ct[]={"maj","min","dim","aug","sus2","sus4","maj7","min7","dom7","dim7","m7b5","pow5","pow5oct"};
        const char* cl[]={"maj","min","dim","aug","sus2","sus4","maj7","min7","dom7","dim7","m7b5","pow5","p5+8"};
        const char* nn[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        const int iv[13][7]={{0,4,7,-1,-1,-1,-1},{0,3,7,-1,-1,-1,-1},{0,3,6,-1,-1,-1,-1},{0,4,8,-1,-1,-1,-1},{0,2,7,-1,-1,-1,-1},{0,5,7,-1,-1,-1,-1},{0,4,7,11,-1,-1,-1},{0,3,7,10,-1,-1,-1},{0,4,7,10,-1,-1,-1},{0,3,6,9,-1,-1,-1},{0,3,6,10,-1,-1,-1},{0,7,-1,-1,-1,-1,-1},{0,7,12,-1,-1,-1,-1}};
        std::set<int> spc; for(int n:scNotes) spc.insert(n%12);
        struct CE{int rm;string tn,lb;};
        std::vector<CE> ents;
        for(int n:onotes){int rp=n%12;for(int t=0;t<13;t++){bool ok=true;for(int i=0;i<7;i++){if(iv[t][i]<0)break;if(spc.find((rp+iv[t][i])%12)==spc.end()){ok=false;break;}}if(!ok)continue;ents.push_back({n,ct[t],string(nn[n%12])+cl[t]});}}
        int ne=(int)ents.size(); if(ne==0) return;
        float cpanelH2_=ne*18+8;
        float cpy2=py+400.0f-cpanelH2_;
        if(!(x>=cpx2+4&&x<=cpx2+196&&y>=cpy2&&y<=cpy2+cpanelH2_)) return;
        int cur=0; for(int i=0;i<ne;i++) if(stepData_[panelStep_].chordName==ents[i].lb){cur=i;break;}
        cur=ofClamp(cur-(int)scrollY,0,ne-1);
        chordScrollOffset_=-cur;
        StepUIData& ds2=stepData_[panelStep_];
        ds2.chordName=ents[cur].lb;
        auto cd2=harmony_.getChord(ents[cur].tn,ents[cur].rm);
        ds2.chordNoteCount=0;
        for(int n=0;n<(int)cd2.intervals.size()&&n<7;n++){ds2.chordNotes[n]=cd2.intervals[n];ds2.chordNoteCount++;}
        
    }
}
