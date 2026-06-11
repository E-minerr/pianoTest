#pragma once
struct StepData {
    int  note      = 60;
    bool gate      = true;
    float prob     = 1.0f;
    int  ratchet   = 1;
    enum Mode { ON, OFF, SKIP } mode = ON;
};
class Sequencer {
public:
    void primeStep(int step){ lastMasterStep = step; }
    void skipNext(){ skipNextTrigger_=true; }
    Sequencer();
    void tick(double& sampleCount);
    void tickMaster(double masterClock, double baseStepSamples, float bpmMult);
    bool triggered();
    int  getNote();
    int getCurrentStep() { return currentStep; }
    void setNote(int step, int note);
    void setGate(int step, bool gate);
    void setMode(int step, StepData::Mode mode);
    void setBpm(float bpm);
    void setLoopRange(int start, int end);
    void play();
    void stop();
    void reset();
    static const int STEPS = 96;
private:
    bool skipNextTrigger_ = false;
    StepData steps[STEPS];
    int      currentStep  = 0;
    int      lastMasterStep = -1;
    int      lastCycleCount_ = -1;
    double   stepSamples  = 0;
    double   sampleCount  = 0;
    bool     isPlaying    = true;
    bool     triggered_   = false;
    int      loopStart    = 0;
    int      loopEnd      = STEPS - 1;
    float    bpm_         = 60.0f;
    void calcStepSamples();
};
