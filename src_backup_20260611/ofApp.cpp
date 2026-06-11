#include "ofApp.h"
#include "maximilian.h"

// ========== グローバル定数 ==========
#define SEC_OSC1   0
#define SEC_OSC2   250
#define SEC_FILTER 500
#define SEC_ENV    660
#define SEC_LFO    780
#define SEC_REVERB 920
#define SEC_MASTER 1010

#define W_OSC    250
#define W_FILTER 160
#define W_ENV    120
#define W_LFO    200
#define W_REVERB 90
#define W_MASTER 84

// ========== ヘルパー関数 ==========
double ofApp::midiToFreq(int note) {
    return 440.0 * pow(2.0, (note - 69) / 12.0);
}

double ofApp::centToRatio(float cent) {
    return pow(2.0, cent / 1200.0);
}

double ofApp::playOsc(maxiOsc& saw, maxiOsc& sqr, maxiOsc& tri, maxiOsc& sin,
                      OscWaveMix& mix, double freq) {
    double out = 0.0;
    if(mix.normalized[0] > 0.0) out += saw.saw(freq)      * (mix.normalized[0] / 100.0);
    if(mix.normalized[1] > 0.0) out += sqr.square(freq)   * (mix.normalized[1] / 100.0);
    if(mix.normalized[2] > 0.0) out += tri.triangle(freq) * (mix.normalized[2] / 100.0);
    if(mix.normalized[3] > 0.0) out += sin.sinewave(freq) * (mix.normalized[3] / 100.0);
    return out;
}

// ========== UI描画ヘルパー ==========
void drawKnob(float cx, float cy, float r, float val, ofColor col) {
    ofSetColor(40);
    ofDrawCircle(cx, cy, r);
    ofSetColor(col);
    ofNoFill();
    ofSetLineWidth(2);
    ofDrawCircle(cx, cy, r);
    ofFill();
    float angle = ofDegToRad(-135.0f + val * 270.0f);
    float nx = cx + cos(angle) * (r - 3);
    float ny = cy + sin(angle) * (r - 3);
    ofSetColor(col);
    ofSetLineWidth(2);
    ofDrawLine(cx, cy, nx, ny);
}

void drawKnobRight(float cx, float cy, float kr, float val, float minV, float maxV,
                   string label, ofColor col) {
    drawKnob(cx, cy, kr, (val - minV) / (maxV - minV), col);
    ofSetColor(col);
    ofDrawBitmapString(label, cx + kr + 8, cy - 4);
    ofSetColor(180);
    ofDrawBitmapString(ofToString(val, 2), cx + kr + 8, cy + 10);
}

void drawButton(float x, float y, float w, float h,
                string label, bool active, ofColor col) {
    if(active) {
        ofSetColor(col);
        ofDrawRectRounded(x, y, w, h, 3);
        ofSetColor(20);
    } else {
        ofSetColor(40);
        ofDrawRectRounded(x, y, w, h, 3);
        ofSetColor(col);
    }
    ofDrawBitmapString(label, x + w / 2 - label.size() * 4, y + h / 2 + 4);
}

void drawHeader(float x, float y, float w, string label, ofColor col) {
    ofSetColor(col);
    ofDrawRectRounded(x + 4, y, w - 8, 22, 3);
    ofSetColor(20);
    ofDrawBitmapString(label, x + w / 2 - label.size() * 4, y + 15);
}

// ========== setup ==========
void ofApp::setup() {
    auto devices = soundStream.getDeviceList();
    ofSoundStreamSettings settings;
    settings.numOutputChannels = 2;
    settings.sampleRate = 44100;
    settings.bufferSize = 512;
    settings.numBuffers = 4;

    for(auto& d : devices) {
        if(d.deviceID == 129) {
            settings.setOutDevice(d);
            break;
        }
    }

    soundStream.setup(settings);
    soundStream.setOutput(this);
    maxiSettings::setup(44100, 1, 512);

    for(int v = 0; v < MAX_VOICES; v++) {
        envState[v]      = ENV_IDLE;
        envAmplitude[v]  = 0.0;
        gateTimer[v]     = 0.0;
        gateLength[v]    = 0.0;
        gateOpen[v]      = false;
        glideFreq[v]     = 440.0;
        glideTarget[v]   = 440.0;
        glideSamples[v]  = 0.0;
        glideDuration[v] = 0.0;
        smoothedAmp[v]   = 0.0;
        seqSampleCount[v]= 0.0;
        for(int p = 0; p < 7; p++) {
            polyFreqs[v][p]    = 440.0;
            strumTimers[v][p]  = 0.0;
            strumPending[v][p] = false;
        }
    }

    initReverb();

    seqUI = new SequencerUI(seqs, MAX_VOICES, noteResolver, harmony);
    seqUI->setBaseBpm(bpm);

    buildKnobTable();
}

// ========== update ==========
void ofApp::update() {
    if(seqUI) seqUI->setBaseBpm(bpm);
}

// ========== draw ==========
void ofApp::draw() {
    ofBackground(20);

    ofColor amber(200, 164, 74);
    ofColor blue(74, 140, 200);
    ofColor pink(200, 74, 122);
    ofColor green(74, 200, 122);
    ofColor white(200, 200, 200);
    ofColor gray(120, 120, 120);

    float H  = 22;
    float Y  = 10;
    float kr = 16;
    float ky = Y + H + 20;

    auto& ov = oscVoices[selectedOscVoice_];

    // ========== OSC 1 ==========
    string osc1Header = "OSC 1 [V" + ofToString(selectedOscVoice_ + 1) + "]";
    drawHeader(SEC_OSC1, Y, W_OSC, osc1Header, amber);

    float col1x = SEC_OSC1 + 24;
    float col2x = SEC_OSC1 + 124;
    float row1y = ky + 20;
    float row2y = ky + 80;
    float row3y = ky + 140;
    float row4y = ky + 200;

    drawKnobRight(col1x, row1y, kr, ov.mainWave.raw[0], 0, 100, "SAW", amber);
    drawKnobRight(col2x, row1y, kr, ov.mainWave.raw[1], 0, 100, "SQR", amber);
    drawKnobRight(col1x, row2y, kr, ov.mainWave.raw[2], 0, 100, "TRI", amber);
    drawKnobRight(col2x, row2y, kr, ov.mainWave.raw[3], 0, 100, "SIN", amber);
    drawKnobRight(col1x, row3y, kr, ov.tune,  -24,  24,  "TUNE",  amber);
    drawKnobRight(col2x, row3y, kr, ov.fine,  -100, 100, "FINE",  amber);
    drawKnobRight(col1x, row4y, kr, ov.level,  0,   1,   "LEVEL", amber);
    ofSetColor(amber);
    ofDrawBitmapString(ov.mainWave.getActiveLabel() + " 100%", col1x + kr + 8, row4y + 24);

    float sub1y  = ky + 270;
    float srow1y = sub1y + 20;
    float srow2y = sub1y + 80;
    float srow3y = sub1y + 140;

    ofSetColor(50); ofSetLineWidth(1);
    ofDrawLine(SEC_OSC1 + 10, sub1y - 14, SEC_OSC1 + W_OSC - 10, sub1y - 14);
    ofSetColor(80);
    ofDrawBitmapString("SUB 1", SEC_OSC1 + 10, sub1y - 2);

    drawKnobRight(col1x, srow1y, kr, ov.subWave.raw[0], 0, 100, "SAW",    amber);
    drawKnobRight(col2x, srow1y, kr, ov.subWave.raw[1], 0, 100, "SQR",    amber);
    drawKnobRight(col1x, srow2y, kr, ov.subWave.raw[2], 0, 100, "TRI",    amber);
    drawKnobRight(col2x, srow2y, kr, ov.subWave.raw[3], 0, 100, "SIN",    amber);
    drawKnobRight(col1x, srow3y, kr, ov.subDetune, -100, 100, "DETUNE", amber);
    drawKnobRight(col2x, srow3y, kr, ov.subLevel,   0,   1,   "LEVEL",  amber);
    ofSetColor(amber);
    ofDrawBitmapString(ov.subWave.getActiveLabel() + " 100%", col2x + kr + 8, srow3y + 24);

    // ========== OSC 2 ==========
    drawHeader(SEC_OSC2, Y, W_OSC, "OSC 2", amber);

    float col3x = SEC_OSC2 + 24;
    float col4x = SEC_OSC2 + 124;

    drawKnobRight(col3x, row1y, kr, ov.main2Wave.raw[0], 0, 100, "SAW", amber);
    drawKnobRight(col4x, row1y, kr, ov.main2Wave.raw[1], 0, 100, "SQR", amber);
    drawKnobRight(col3x, row2y, kr, ov.main2Wave.raw[2], 0, 100, "TRI", amber);
    drawKnobRight(col4x, row2y, kr, ov.main2Wave.raw[3], 0, 100, "SIN", amber);
    drawKnobRight(col3x, row3y, kr, ov.tune2,  -24,  24,  "TUNE",  amber);
    drawKnobRight(col4x, row3y, kr, ov.fine2,  -100, 100, "FINE",  amber);
    drawKnobRight(col3x, row4y, kr, ov.level2,  0,   1,   "LEVEL", amber);
    ofSetColor(amber);
    ofDrawBitmapString(ov.main2Wave.getActiveLabel() + " 100%", col3x + kr + 8, row4y + 24);

    float sub2y   = ky + 270;
    float srow1y2 = sub2y + 20;
    float srow2y2 = sub2y + 80;
    float srow3y2 = sub2y + 140;

    ofSetColor(50);
    ofDrawLine(SEC_OSC2 + 10, sub2y - 14, SEC_OSC2 + W_OSC - 10, sub2y - 14);
    ofSetColor(80);
    ofDrawBitmapString("SUB 2", SEC_OSC2 + 10, sub2y - 2);

    drawKnobRight(col3x, srow1y2, kr, ov.sub2Wave.raw[0], 0, 100, "SAW",    amber);
    drawKnobRight(col4x, srow1y2, kr, ov.sub2Wave.raw[1], 0, 100, "SQR",    amber);
    drawKnobRight(col3x, srow2y2, kr, ov.sub2Wave.raw[2], 0, 100, "TRI",    amber);
    drawKnobRight(col4x, srow2y2, kr, ov.sub2Wave.raw[3], 0, 100, "SIN",    amber);
    drawKnobRight(col3x, srow3y2, kr, ov.sub2Detune, -100, 100, "DETUNE", amber);
    drawKnobRight(col4x, srow3y2, kr, ov.sub2Level,   0,   1,   "LEVEL",  amber);
    ofSetColor(amber);
    ofDrawBitmapString(ov.sub2Wave.getActiveLabel() + " 100%", col4x + kr + 8, srow3y2 + 22);

    // ========== FILTER ==========
    drawHeader(SEC_FILTER, Y, W_FILTER, "FILTER", blue);

    float fkx = SEC_FILTER + 24;
    drawKnobRight(fkx, ky + 20,  kr, ov.filterCutoff,    20, 20000, "CUTOFF",  blue);
    drawKnobRight(fkx, ky + 80,  kr, ov.filterResonance,  0, 1,     "RESO",    blue);
    drawKnobRight(fkx, ky + 140, kr, ov.filterEnvAmt,    -1, 1,     "ENV AMT", blue);
    drawKnobRight(fkx, ky + 200, kr, ov.filterKeyTrk,     0, 1,     "KEY TRK", blue);

    float bx = SEC_FILTER + 10;
    float by = ky + 270;
    drawButton(bx,      by, 40, 20, "LP", ov.filterMode == 0, blue);
    drawButton(bx + 48, by, 40, 20, "HP", ov.filterMode == 1, blue);
    drawButton(bx + 96, by, 40, 20, "BP", ov.filterMode == 2, blue);

    // ========== ENV ==========
    drawHeader(SEC_ENV, Y, W_ENV, "ENV", pink);

    float ekx = SEC_ENV + 24;
    drawKnobRight(ekx, ky + 20,  kr, ov.envAttack,  0, 5,  "ATK", pink);
    drawKnobRight(ekx, ky + 80,  kr, ov.envDecay,   0, 5,  "DCY", pink);
    drawKnobRight(ekx, ky + 140, kr, ov.envSustain, 0, 1,  "SUS", pink);
    drawKnobRight(ekx, ky + 200, kr, ov.envRelease, 0, 10, "REL", pink);

    // ========== LFO ==========
    drawHeader(SEC_LFO, Y, W_LFO, "LFO", green);

    float lbx = SEC_LFO + 10;
    float lby = ky + 10;
    drawButton(lbx,      lby, 40, 20, "SIN", ov.lfoWave == 0, green);
    drawButton(lbx + 48, lby, 40, 20, "TRI", ov.lfoWave == 1, green);
    drawButton(lbx,      lby + 26, 40, 20, "SQR", ov.lfoWave == 2, green);

    float lkx = SEC_LFO + 24;
    drawKnobRight(lkx, ky + 80,  kr, ov.lfoRate,  0.1, 20, "RATE",  green);
    drawKnobRight(lkx, ky + 140, kr, ov.lfoDepth, 0,   1,  "DEPTH", green);

    float ltx = SEC_LFO + 10;
    float lty = ky + 220;
    drawButton(ltx,       lty, 50, 20, "PITCH", ov.lfoTarget == 0, green);
    drawButton(ltx + 58,  lty, 50, 20, "FILT",  ov.lfoTarget == 1, green);
    drawButton(ltx + 116, lty, 50, 20, "AMP",   ov.lfoTarget == 2, green);

    // ========== REVERB ==========
    drawHeader(SEC_REVERB, Y, W_REVERB, "REV", white);

    float rkx = SEC_REVERB + 24;
    drawKnobRight(rkx, ky + 20,  kr, ov.reverbRoomSize, 0, 1, "ROOM", white);
    drawKnobRight(rkx, ky + 80,  kr, ov.reverbDamp,     0, 1, "DAMP", white);
    drawKnobRight(rkx, ky + 140, kr, ov.reverbWet,      0, 1, "WET",  white);

    // ========== MASTER ==========
    drawHeader(SEC_MASTER, Y, W_MASTER, "MST", gray);

    float mkx = SEC_MASTER + 24;
    drawKnobRight(mkx, ky + 20, kr, masterVolume, 0,  1,   "VOL", gray);
    drawKnobRight(mkx, ky + 80, kr, bpm,          40, 200, "BPM", gray);

    ofSetColor(isPlaying ? ofColor(74, 200, 122) : ofColor(120));
    ofDrawBitmapString(isPlaying ? "PLAY" : "STOP", mkx - 8, ky + 160);

    // 区切り線
    ofSetColor(50); ofSetLineWidth(1);
    ofDrawLine(SEC_OSC2,   Y, SEC_OSC2,   460);
    ofDrawLine(SEC_FILTER, Y, SEC_FILTER, 460);
    ofDrawLine(SEC_ENV,    Y, SEC_ENV,    460);
    ofDrawLine(SEC_LFO,    Y, SEC_LFO,    460);
    ofDrawLine(SEC_REVERB, Y, SEC_REVERB, 460);
    ofDrawLine(SEC_MASTER, Y, SEC_MASTER, 460);

    // ========== SEQUENCER ==========
    if(seqUI) seqUI->draw(470);
}

// ========== buildKnobTable ==========
void ofApp::buildKnobTable() {
    float H     = 22;
    float Y     = 10;
    float kr    = 16;
    float ky    = Y + H + 20;
    float row1y = ky + 20;
    float row2y = ky + 80;
    float row3y = ky + 140;
    float row4y = ky + 200;
    float sub1y  = ky + 270;
    float srow1y = sub1y + 20;
    float srow2y = sub1y + 80;
    float srow3y = sub1y + 140;

    auto& ov = oscVoices[selectedOscVoice_];

    float col1x = SEC_OSC1 + 24;
    float col2x = SEC_OSC1 + 124;

    knobs[0]  = KnobInfo(col1x, row1y,  kr, 0,    100,  nullptr);
    knobs[1]  = KnobInfo(col2x, row1y,  kr, 0,    100,  nullptr);
    knobs[2]  = KnobInfo(col1x, row2y,  kr, 0,    100,  nullptr);
    knobs[3]  = KnobInfo(col2x, row2y,  kr, 0,    100,  nullptr);
    knobs[4]  = KnobInfo(col1x, row3y,  kr, -24,  24,   &ov.tune);
    knobs[5]  = KnobInfo(col2x, row3y,  kr, -100, 100,  &ov.fine);
    knobs[6]  = KnobInfo(col1x, row4y,  kr, 0,    1,    &ov.level);
    knobs[7]  = KnobInfo(col1x, srow1y, kr, 0,    100,  nullptr);
    knobs[8]  = KnobInfo(col2x, srow1y, kr, 0,    100,  nullptr);
    knobs[9]  = KnobInfo(col1x, srow2y, kr, 0,    100,  nullptr);
    knobs[10] = KnobInfo(col2x, srow2y, kr, 0,    100,  nullptr);
    knobs[11] = KnobInfo(col1x, srow3y, kr, -100, 100,  &ov.subDetune);
    knobs[12] = KnobInfo(col2x, srow3y, kr, 0,    1,    &ov.subLevel);

    float col3x = SEC_OSC2 + 24;
    float col4x = SEC_OSC2 + 124;

    knobs[13] = KnobInfo(col3x, row1y,  kr, 0,    100,  nullptr);
    knobs[14] = KnobInfo(col4x, row1y,  kr, 0,    100,  nullptr);
    knobs[15] = KnobInfo(col3x, row2y,  kr, 0,    100,  nullptr);
    knobs[16] = KnobInfo(col4x, row2y,  kr, 0,    100,  nullptr);
    knobs[17] = KnobInfo(col3x, row3y,  kr, -24,  24,   &ov.tune2);
    knobs[18] = KnobInfo(col4x, row3y,  kr, -100, 100,  &ov.fine2);
    knobs[19] = KnobInfo(col3x, row4y,  kr, 0,    1,    &ov.level2);
    knobs[20] = KnobInfo(col3x, srow1y, kr, 0,    100,  nullptr);
    knobs[21] = KnobInfo(col4x, srow1y, kr, 0,    100,  nullptr);
    knobs[22] = KnobInfo(col3x, srow2y, kr, 0,    100,  nullptr);
    knobs[23] = KnobInfo(col4x, srow2y, kr, 0,    100,  nullptr);
    knobs[24] = KnobInfo(col3x, srow3y, kr, -100, 100,  &ov.sub2Detune);
    knobs[25] = KnobInfo(col4x, srow3y, kr, 0,    1,    &ov.sub2Level);

    float fkx = SEC_FILTER + 24;
    knobs[26] = KnobInfo(fkx, ky + 20,  kr, 20,  20000, &ov.filterCutoff);
    knobs[27] = KnobInfo(fkx, ky + 80,  kr, 0,   1,     &ov.filterResonance);
    knobs[28] = KnobInfo(fkx, ky + 140, kr, -1,  1,     &ov.filterEnvAmt);
    knobs[29] = KnobInfo(fkx, ky + 200, kr, 0,   1,     &ov.filterKeyTrk);

    float ekx = SEC_ENV + 24;
    knobs[30] = KnobInfo(ekx, ky + 20,  kr, 0, 5,  &ov.envAttack);
    knobs[31] = KnobInfo(ekx, ky + 80,  kr, 0, 5,  &ov.envDecay);
    knobs[32] = KnobInfo(ekx, ky + 140, kr, 0, 1,  &ov.envSustain);
    knobs[33] = KnobInfo(ekx, ky + 200, kr, 0, 10, &ov.envRelease);

    float lkx = SEC_LFO + 24;
    knobs[34] = KnobInfo(lkx, ky + 80,  kr, 0.1, 20, &ov.lfoRate);
    knobs[35] = KnobInfo(lkx, ky + 140, kr, 0,   1,  &ov.lfoDepth);

    float rkx = SEC_REVERB + 24;
    knobs[36] = KnobInfo(rkx, ky + 20,  kr, 0, 1, &ov.reverbRoomSize);
    knobs[37] = KnobInfo(rkx, ky + 80,  kr, 0, 1, &ov.reverbDamp);
    knobs[38] = KnobInfo(rkx, ky + 140, kr, 0, 1, &ov.reverbWet);

    float mkx = SEC_MASTER + 24;
    knobs[39] = KnobInfo(mkx, ky + 20, kr, 0,  1,   &masterVolume);
    knobs[40] = KnobInfo(mkx, ky + 80, kr, 40, 200, &bpm);
}

// ========== mousePressed ==========
void ofApp::mousePressed(int x, int y, int button) {
    // シーケンサーエリア（y>=470）はseqUIに渡す
    if(y >= 470) {
        if(seqUI) {
            seqUI->mousePressed(x, y, button);
            if(seqUI->selectedOscVoice_ != selectedOscVoice_) {
                selectedOscVoice_ = seqUI->selectedOscVoice_;
                buildKnobTable();
            }
        }
        return;
    }

    // OSCパネルエリア（y<470）
    for(int i = 0; i < 41; i++) {
        float dx = x - knobs[i].cx;
        float dy = y - knobs[i].cy;
        if(sqrt(dx * dx + dy * dy) <= knobs[i].r + 4) {
            dragTarget   = i;
            dragStartY   = y;
            dragStartVal = knobs[i].target ? *knobs[i].target : 0;
            return;
        }
    }

    auto& ov = oscVoices[selectedOscVoice_];
    float H  = 22;
    float Y  = 10;
    float ky = Y + H + 20;

    float bx = SEC_FILTER + 10;
    float by = ky + 270;
    if(x >= bx      && x <= bx + 40  && y >= by && y <= by + 20) { ov.filterMode = 0; return; }
    if(x >= bx + 48 && x <= bx + 88  && y >= by && y <= by + 20) { ov.filterMode = 1; return; }
    if(x >= bx + 96 && x <= bx + 136 && y >= by && y <= by + 20) { ov.filterMode = 2; return; }

    float lbx = SEC_LFO + 10;
    float lby = ky + 10;
    if(x >= lbx      && x <= lbx + 40  && y >= lby      && y <= lby + 20)      { ov.lfoWave = 0; return; }
    if(x >= lbx + 48 && x <= lbx + 88  && y >= lby      && y <= lby + 20)      { ov.lfoWave = 1; return; }
    if(x >= lbx      && x <= lbx + 40  && y >= lby + 26 && y <= lby + 46)      { ov.lfoWave = 2; return; }

    float ltx = SEC_LFO + 10;
    float lty = ky + 220;
    if(x >= ltx       && x <= ltx + 50  && y >= lty && y <= lty + 20) { ov.lfoTarget = 0; return; }
    if(x >= ltx + 58  && x <= ltx + 108 && y >= lty && y <= lty + 20) { ov.lfoTarget = 1; return; }
    if(x >= ltx + 116 && x <= ltx + 166 && y >= lty && y <= lty + 20) { ov.lfoTarget = 2; return; }

    float mkx = SEC_MASTER + 42;
    if(x >= mkx - 16 && x <= mkx + 32 && y >= ky + 150 && y <= ky + 165) {
        isPlaying = !isPlaying;
        return;
    }
}

// ========== mouseDragged ==========
void ofApp::mouseDragged(int x, int y, int button) {
    if(y >= 470 && dragTarget < 0) {
        if(seqUI) seqUI->mouseDragged(x, y, button);
        return;
    }
    if(dragTarget < 0) return;

    auto& ov = oscVoices[selectedOscVoice_];
    float dy    = dragStartY - y;
    float range = knobs[dragTarget].maxVal - knobs[dragTarget].minVal;
    float delta = dy / 200.0f * range;

    auto setWaveMix = [&](OscWaveMix& mix, int idx) {
        int newVal = (int)round((dragStartVal + delta) / 10.0f) * 10;
        newVal = ofClamp(newVal, 0, 100);
        mix.set(idx, newVal);
    };

    switch(dragTarget) {
        case 0:  setWaveMix(ov.mainWave, 0); break;
        case 1:  setWaveMix(ov.mainWave, 1); break;
        case 2:  setWaveMix(ov.mainWave, 2); break;
        case 3:  setWaveMix(ov.mainWave, 3); break;
        case 7:  ov.subUseMain = false; setWaveMix(ov.subWave, 0); break;
        case 8:  ov.subUseMain = false; setWaveMix(ov.subWave, 1); break;
        case 9:  ov.subUseMain = false; setWaveMix(ov.subWave, 2); break;
        case 10: ov.subUseMain = false; setWaveMix(ov.subWave, 3); break;
        case 13: setWaveMix(ov.main2Wave, 0); break;
        case 14: setWaveMix(ov.main2Wave, 1); break;
        case 15: setWaveMix(ov.main2Wave, 2); break;
        case 16: setWaveMix(ov.main2Wave, 3); break;
        case 20: ov.sub2UseMain = false; setWaveMix(ov.sub2Wave, 0); break;
        case 21: ov.sub2UseMain = false; setWaveMix(ov.sub2Wave, 1); break;
        case 22: ov.sub2UseMain = false; setWaveMix(ov.sub2Wave, 2); break;
        case 23: ov.sub2UseMain = false; setWaveMix(ov.sub2Wave, 3); break;
        default:
            if(knobs[dragTarget].target) {
                float newVal = ofClamp(dragStartVal + delta,
                                       knobs[dragTarget].minVal,
                                       knobs[dragTarget].maxVal);
                *knobs[dragTarget].target = newVal;
            }
            break;
    }
}

// ========== mouseReleased ==========
void ofApp::mouseReleased(int x, int y, int button) {
    if(seqUI) seqUI->mouseReleased(x, y, button);
    dragTarget = -1;
}

// ========== mouseScrolled ==========
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
    if(seqUI) seqUI->mouseScrolled(x, y, scrollX, scrollY);
}

// ========== audioOut ==========
void ofApp::audioOut(ofSoundBuffer& buffer) {
    if(!isPlaying) return;

    const double SR = 44100.0;

    if(seqUI && seqUI->resetMasterClock_) {
        masterClock_ = 0.0;
        for(int v = 0; v < MAX_VOICES; v++) seqSampleCount[v] = 0.0;
        seqUI->resetMasterClock_ = false;
    }

    int voiceCount = seqUI ? seqUI->getVoiceCount() : 0;

    for(size_t i = 0; i < buffer.getNumFrames(); i++) {

        masterClock_++;
        double outMix = 0.0;
        int activeVoices = 0;

        for(int v = 0; v < voiceCount; v++) {
            const VoiceRange& vr = seqUI->getVoice(v);
            if(!vr.active || !vr.playing) continue;

            auto& ov = oscVoices[vr.oscIndex];
            float bpmMult = vr.common.bpmMult;
            double baseStepSamples = SR * 60.0 / (double)bpm / 4.0;

            seqs[v].tickMaster(masterClock_, baseStepSamples, bpmMult);

            if(seqs[v].triggered()) {
                int globalStep = vr.startStep + seqs[v].getCurrentStep();
                const StepUIData& sd = seqUI->getStepData(globalStep);

                if(sd.mode == StepUIData::SKIP) continue;
                if((float)rand() / RAND_MAX > sd.prob) continue;

                double stepSamples = baseStepSamples / bpmMult;
                gateLength[v] = stepSamples * sd.gate;
                gateTimer[v]  = gateLength[v];
                gateOpen[v]   = true;

                double targetFreq;
                if(sd.chordNoteCount > 0) {
                    targetFreq = midiToFreq(sd.chordNotes[0] + sd.octShift * 12);
                    for(int p = 0; p < sd.chordNoteCount && p < 7; p++) {
                        polyFreqs[v][p] = midiToFreq(sd.chordNotes[p] + sd.octShift * 12);
                        if(sd.strumOn) {
                            strumTimers[v][p]  = (double)p * sd.strumDelay * SR;
                            strumPending[v][p] = true;
                        } else {
                            strumTimers[v][p]  = 0.0;
                            strumPending[v][p] = false;
                        }
                    }
                    for(int p = sd.chordNoteCount; p < 7; p++) polyFreqs[v][p] = -1.0;
                } else {
                    int note = seqUI->resolveNote(globalStep);
                    targetFreq = midiToFreq(note);
                    polyFreqs[v][0] = targetFreq;
                    for(int p = 1; p < 7; p++) polyFreqs[v][p] = -1.0;
                    for(int p = 0; p < 7; p++) strumPending[v][p] = false;
                }

                if(sd.glide && envState[v] != ENV_IDLE) {
                    glideTarget[v]   = targetFreq;
                    glideDuration[v] = SR * 0.05;
                    glideSamples[v]  = 0.0;
                } else {
                    glideFreq[v]     = targetFreq;
                    glideTarget[v]   = targetFreq;
                    glideSamples[v]  = 0.0;
                    glideDuration[v] = 0.0;
                    envAmplitude[v]  = 0.0;
                    envState[v]      = ENV_ATTACK;
                }
            }

            if(gateOpen[v]) {
                gateTimer[v]--;
                if(gateTimer[v] <= 0) {
                    gateOpen[v] = false;
                    envState[v] = ENV_RELEASE;
                }
            }

            if(glideDuration[v] > 0.0) {
                glideSamples[v]++;
                double t = glideSamples[v] / glideDuration[v];
                if(t >= 1.0) { glideFreq[v] = glideTarget[v]; glideDuration[v] = 0.0; }
                else glideFreq[v] = glideFreq[v] + (glideTarget[v] - glideFreq[v]) * t;
            }

            double attackInc  = 1.0 / (ov.envAttack  * SR);
            double decayDec   = 1.0 / (ov.envDecay   * SR);
            double releaseDec = 1.0 / (ov.envRelease * SR);

            if(envState[v] == ENV_ATTACK) {
                envAmplitude[v] += attackInc;
                if(envAmplitude[v] >= 1.0) { envAmplitude[v] = 1.0; envState[v] = ENV_DECAY; }
            } else if(envState[v] == ENV_DECAY) {
                envAmplitude[v] -= decayDec;
                if(envAmplitude[v] <= ov.envSustain) { envAmplitude[v] = ov.envSustain; envState[v] = ENV_SUSTAIN; }
            } else if(envState[v] == ENV_SUSTAIN) {
                envAmplitude[v] = ov.envSustain;
            } else if(envState[v] == ENV_RELEASE) {
                envAmplitude[v] -= releaseDec;
                if(envAmplitude[v] <= 0.0) { envAmplitude[v] = 0.0; envState[v] = ENV_IDLE; }
            }

            if(envState[v] == ENV_IDLE) continue;

            smoothedAmp[v] = SMOOTH_COEFF * smoothedAmp[v] + (1.0 - SMOOTH_COEFF) * envAmplitude[v];

            double lfoVal = 0.0;
            if(ov.lfoWave == 0)      lfoVal = ov.lfo.sinewave(ov.lfoRate) * ov.lfoDepth;
            else if(ov.lfoWave == 1) lfoVal = ov.lfo.triangle(ov.lfoRate) * ov.lfoDepth;
            else                     lfoVal = ov.lfo.square(ov.lfoRate)   * ov.lfoDepth;

            double voiceOut = 0.0;
            int polyCount = 0;

            for(int p = 0; p < 7; p++) {
                if(polyFreqs[v][p] < 0.0) break;

                if(strumPending[v][p]) {
                    strumTimers[v][p]--;
                    if(strumTimers[v][p] > 0.0) continue;
                    strumPending[v][p] = false;
                }

                double freq = polyFreqs[v][p] * pow(2.0, ov.tune / 12.0) * centToRatio(ov.fine);
                if(ov.lfoTarget == 0) freq += lfoVal * 100.0;

                double pOut = playOsc(ov.poly_saw[p], ov.poly_sqr[p], ov.poly_tri[p], ov.poly_sin[p],
                                      ov.mainWave, freq) * ov.level;

                OscWaveMix& subMix = ov.subUseMain ? ov.mainWave : ov.subWave;
                double freqSub = freq * centToRatio(ov.subDetune);
                pOut += playOsc(ov.sub_saw, ov.sub_sqr, ov.sub_tri, ov.sub_sin,
                                subMix, freqSub) * ov.subLevel;

                double freq2 = polyFreqs[v][p] * pow(2.0, ov.tune2 / 12.0) * centToRatio(ov.fine2);
                pOut += playOsc(ov.poly2_saw[p], ov.poly2_sqr[p], ov.poly2_tri[p], ov.poly2_sin[p],
                                ov.main2Wave, freq2) * ov.level2;

                OscWaveMix& sub2Mix = ov.sub2UseMain ? ov.main2Wave : ov.sub2Wave;
                double freq2Sub = freq2 * centToRatio(ov.sub2Detune);
                pOut += playOsc(ov.sub2_saw, ov.sub2_sqr, ov.sub2_tri, ov.sub2_sin,
                                sub2Mix, freq2Sub) * ov.sub2Level;

                voiceOut += pOut;
                polyCount++;
            }

            if(polyCount == 0) continue;
            voiceOut /= sqrt((double)polyCount);
            voiceOut *= smoothedAmp[v];

            double cutoff = ov.filterCutoff;
            cutoff += ov.filterEnvAmt * envAmplitude[v] * 5000.0;
            if(ov.lfoTarget == 1) cutoff += lfoVal * 5000.0;
            cutoff = ofClamp(cutoff, 20.0, 20000.0);
            ov.filter.setCutoff(cutoff);
            ov.filter.setResonance(ov.filterResonance);
            float lp = (ov.filterMode == 0) ? 1.0f : 0.0f;
            float hp = (ov.filterMode == 1) ? 1.0f : 0.0f;
            float bp = (ov.filterMode == 2) ? 1.0f : 0.0f;
            voiceOut = ov.filter.play(voiceOut, lp, bp, hp, 0.0f);

            if(ov.lfoTarget == 2) voiceOut *= (1.0 + lfoVal);

            if(ov.reverbWet > 0.0f) {
                double wet = processReverb(voiceOut);
                voiceOut = voiceOut * (1.0 - ov.reverbWet) + wet * ov.reverbWet;
            }

            outMix += voiceOut;
            activeVoices++;
        }

        if(activeVoices > 0) outMix /= sqrt((double)activeVoices);
        outMix *= masterVolume;

        buffer[i * 2]     = outMix;
        buffer[i * 2 + 1] = outMix;
    }
}

// ========== 残り空関数 ==========
void ofApp::keyPressed(int key){}
void ofApp::keyReleased(int key){}
void ofApp::mouseMoved(int x, int y){}
void ofApp::windowResized(int w, int h){}
void ofApp::gotMessage(ofMessage msg){}
void ofApp::dragEvent(ofDragInfo dragInfo){}
void ofApp::mouseEntered(int x, int y){}
void ofApp::mouseExited(int x, int y){}
