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
    static constexpr int MAX_VOICES = 6;

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
        OscWaveMix mainWave;
        float tune=0.0f, fine=0.0f, level=0.8f;
        OscWaveMix subWave;
        float subDetune=0.0f, subDelay=0.0f, subLevel=0.0f;
        bool subUseMain=true;
        OscWaveMix main2Wave;
        float tune2=0.0f, fine2=0.0f, level2=0.0f;
        OscWaveMix sub2Wave;
        float sub2Detune=0.0f, sub2Delay=0.0f, sub2Level=0.0f;
        bool sub2UseMain=true;
        float filterCutoff=1000.0f,filterResonance=0.5f,filterEnvAmt=0.0f,filterKeyTrk=0.0f;
        int   filterMode=0;
        float envAttack=0.01f,envDecay=0.1f,envSustain=0.8f,envRelease=0.3f;
        int   lfoWave=0;
        float lfoRate=1.0f,lfoDepth=0.0f;
        int   lfoTarget=0;
        float reverbRoomSize=0.5f,reverbDamp=0.5f,reverbWet=0.0f;
    };
    OscVoice oscVoices[MAX_VOICES];
    int selectedOscVoice_ = 0;


    enum EnvState { ENV_IDLE, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };

    // VOICE分のエンベロープ状態（最大6VOICE）
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
    struct AudioVoiceState {
        maxiOsc sub_saw,sub_sqr,sub_tri,sub_sin;
        maxiOsc sub2_saw,sub2_sqr,sub2_tri,sub2_sin;
        maxiOsc poly_saw[7],poly_sqr[7],poly_tri[7],poly_sin[7];
        maxiOsc poly2_saw[7],poly2_sqr[7],poly2_tri[7],poly2_sin[7];
        maxiSVF filter;
        maxiOsc lfo;
        CombFilter combL[4];
        CombFilter combR[4];
        AllpassFilter allpassL[2];
        AllpassFilter allpassR[2];
        double reverbFeedback = 0.78;
        double reverbDamp = 0.2;
        double reverbAllpassFeedback = 0.5;

        void initReverb(){
            combL[0].init(1116); combL[1].init(1188); combL[2].init(1277); combL[3].init(1356);
            combR[0].init(1139); combR[1].init(1211); combR[2].init(1300); combR[3].init(1379);
            allpassL[0].init(556); allpassL[1].init(441);
            allpassR[0].init(579); allpassR[1].init(464);
            for(auto& c:combL){ c.feedback=0.78; c.damp=0.2; }
            for(auto& c:combR){ c.feedback=0.78; c.damp=0.2; }
            for(auto& a:allpassL) a.feedback=0.5;
            for(auto& a:allpassR) a.feedback=0.5;
        }

        void processReverb(double in, float room, float damp, double& outL, double& outR){
            double targetFeedback = ofClamp(0.62 + room * 0.33, 0.0, 0.96);
            double targetDamp = ofClamp((double)damp * 0.92, 0.0, 0.92);
            double targetAllpass = ofClamp(0.42 + room * 0.18, 0.35, 0.65);
            reverbFeedback += (targetFeedback - reverbFeedback) * 0.001;
            reverbDamp += (targetDamp - reverbDamp) * 0.001;
            reverbAllpassFeedback += (targetAllpass - reverbAllpassFeedback) * 0.001;

            outL=0.0;
            for(auto& c:combL) {
                c.feedback = reverbFeedback;
                c.damp = reverbDamp;
                outL+=c.process(in);
            }
            outL/=4.0;
            for(auto& a:allpassL) a.feedback = reverbAllpassFeedback;
            outL=allpassL[0].process(outL);
            outL=allpassL[1].process(outL);

            outR=0.0;
            for(auto& c:combR) {
                c.feedback = reverbFeedback;
                c.damp = reverbDamp;
                outR+=c.process(in);
            }
            outR/=4.0;
            for(auto& a:allpassR) a.feedback = reverbAllpassFeedback;
            outR=allpassR[0].process(outR);
            outR=allpassR[1].process(outR);
        }
    };
    AudioVoiceState audioVoices[MAX_VOICES];


    // Sequencer6本
    Sequencer seqs[MAX_VOICES];
    double seqSampleCount[MAX_VOICES];
    int lastVoiceCount_ = 0;
    double masterClock_ = 0.0;

    HarmonyEngine harmony;
    NoteResolver  noteResolver{harmony};

    float masterVolume=0.5f;

    SequencerUI* seqUI=nullptr;
    ofTrueTypeFont jbMonoFont;
    ofTrueTypeFont jbMonoFont14;
    ofTrueTypeFont jbMonoFont12;

    double playOsc(maxiOsc& saw,maxiOsc& sqr,maxiOsc& tri,maxiOsc& sin,const OscWaveMix& mix,double freq);
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
