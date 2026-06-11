#pragma once
#include "ofMain.h"
#include "maximilian.h"
#include "HarmonyEngine.h"
#include "Sequencer.h"
#include "NoteResolver.h"
#include "SequencerUI.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void audioOut(ofSoundBuffer& buffer);

    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseScrolled(int x, int y, float scrollX, float scrollY);
    void mouseEntered(int x, int y);
    void mouseExited(int x, int y);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);

    ofSoundStream soundStream;

    float bpm      = 60.0f;
    bool  isPlaying = true;

    struct OscWaveMix {
        int raw[4] = {100, 0, 0, 0};
        double normalized[4] = {100.0, 0.0, 0.0, 0.0};
        void set(int idx, int val) { raw[idx] = val; normalize(); }
        void normalize() {
            int sum = 0; for(int i=0;i<4;i++) sum+=raw[i];
            if(sum==0){for(int i=0;i<4;i++) normalized[i]=0.0;return;}
            for(int i=0;i<4;i++) normalized[i]=(double)raw[i]/(double)sum*100.0;
        }
        string getActiveLabel() {
            const char labels[4]={'S','Q','T','N'}; string s="";
            for(int i=0;i<4;i++){if(raw[i]>0){if(s.size()>0)s+=" ";s+=labels[i];}}
            return s;
        }
        void resetToMain(const OscWaveMix& main){for(int i=0;i<4;i++) raw[i]=main.raw[i];normalize();}
    };

    struct OscVoice {
        maxiOsc main_saw,main_sqr,main_tri,main_sin;
        OscWaveMix mainWave;
        float tune=0.0f, fine=0.0f, level=0.8f;
        maxiOsc sub_saw,sub_sqr,sub_tri,sub_sin;
        OscWaveMix subWave;
        float subDetune=0.0f, subDelay=0.0f, subLevel=0.0f;
        bool subUseMain=true;
        maxiOsc main2_saw,main2_sqr,main2_tri,main2_sin;
        OscWaveMix main2Wave;
        float tune2=0.0f, fine2=0.0f, level2=0.0f;
        maxiOsc sub2_saw,sub2_sqr,sub2_tri,sub2_sin;
        OscWaveMix sub2Wave;
        float sub2Detune=0.0f, sub2Delay=0.0f, sub2Level=0.0f;
        bool sub2UseMain=true;
        maxiOsc poly_saw[7],poly_sqr[7],poly_tri[7],poly_sin[7];
        maxiOsc poly2_saw[7],poly2_sqr[7],poly2_tri[7],poly2_sin[7];
        float filterCutoff=1000.0f,filterResonance=0.5f,filterEnvAmt=0.0f,filterKeyTrk=0.0f;
        int   filterMode=0;
        float envAttack=0.01f,envDecay=0.1f,envSustain=0.8f,envRelease=0.3f;
        int   lfoWave=0;
        float lfoRate=1.0f,lfoDepth=0.0f;
        int   lfoTarget=0;
        maxiSVF filter;
        maxiOsc lfo;
        float reverbRoomSize=0.5f,reverbDamp=0.5f,reverbWet=0.0f;
    };
    OscVoice oscVoices[6];
    int selectedOscVoice_ = 0;


    enum EnvState { ENV_IDLE, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };

    // VOICE分のエンベロープ状態（最大6VOICE）
    static constexpr int MAX_VOICES = 6;
    EnvState envState[MAX_VOICES];
    double   envAmplitude[MAX_VOICES];
    double   gateTimer[MAX_VOICES];
    double   gateLength[MAX_VOICES];
    bool     gateOpen[MAX_VOICES];
    double   glideFreq[MAX_VOICES];
    double   polyFreqs[MAX_VOICES][7] = {};
    double   strumTimers[MAX_VOICES][7] = {};
    bool     strumPending[MAX_VOICES][7] = {};
    double   glideTarget[MAX_VOICES];
    double   glideSamples[MAX_VOICES];
    double   glideDuration[MAX_VOICES];
    double   smoothedAmp[MAX_VOICES] = {};
    static constexpr double SMOOTH_COEFF = 0.9717;


    // ========== Schroeder Reverb ==========
    struct CombFilter {
        std::vector<double> buf;
        int pos=0; double feedback=0.5, damp=0.5, last=0.0;
        void init(int size){ buf.resize(size,0.0); }
        double process(double in){
            double out=buf[pos];
            last=out*(1.0-damp)+last*damp;
            buf[pos]=in+last*feedback;
            pos=(pos+1)%buf.size();
            return out;
        }
    };
    struct AllpassFilter {
        std::vector<double> buf;
        int pos=0; double feedback=0.5;
        void init(int size){ buf.resize(size,0.0); }
        double process(double in){
            double out=buf[pos]-in;
            buf[pos]=in+buf[pos]*feedback;
            pos=(pos+1)%buf.size();
            return out;
        }
    };
    CombFilter comb[4];
    AllpassFilter allpass[2];
    void initReverb(){
        comb[0].init(1116); comb[1].init(1188); comb[2].init(1277); comb[3].init(1356);
        allpass[0].init(556); allpass[1].init(441);
        for(auto& c:comb){ c.feedback=0.84; c.damp=0.2; }
        for(auto& a:allpass) a.feedback=0.5;
    }
    double processReverb(double in){
        double out=0.0;
        for(auto& c:comb) out+=c.process(in);
        out/=4.0;
        out=allpass[0].process(out);
        out=allpass[1].process(out);
        return out;
    }


    // Sequencer6本
    Sequencer seqs[MAX_VOICES];
    double seqSampleCount[MAX_VOICES];
    int lastVoiceCount_ = 0;
    double masterClock_ = 0.0;

    HarmonyEngine harmony;
    NoteResolver  noteResolver{harmony};

    float masterVolume=1.0f;

    SequencerUI* seqUI=nullptr;

    double playOsc(maxiOsc& saw,maxiOsc& sqr,maxiOsc& tri,maxiOsc& sin,OscWaveMix& mix,double freq);
    double midiToFreq(int note);
    double centToRatio(float cent);

    int   dragTarget=-1;
    float dragStartY=0,dragStartVal=0;

    struct KnobInfo {
        float cx,cy,r,minVal,maxVal; float* target;
        KnobInfo():cx(0),cy(0),r(0),minVal(0),maxVal(1),target(nullptr){}
        KnobInfo(float cx,float cy,float r,float mn,float mx,float* t):cx(cx),cy(cy),r(r),minVal(mn),maxVal(mx),target(t){}
    };
    KnobInfo knobs[41];
    void buildKnobTable();
};
