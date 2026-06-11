#include "Sequencer.h"
#include <cstdlib>
Sequencer::Sequencer() {
    calcStepSamples();
}
void Sequencer::calcStepSamples() {
    stepSamples = 44100.0 * 60.0 / (double)bpm_ / 4.0;
}
void Sequencer::tick(double& sampleCount) {
    if(!isPlaying) return;
    triggered_ = false;
    sampleCount++;
    if(sampleCount >= stepSamples) {
        sampleCount = 0;
        float r = (float)rand() / (float)RAND_MAX;
        if(steps[currentStep].gate && r <= steps[currentStep].prob) {
            triggered_ = true;
        }
        currentStep++;
        if(currentStep > loopEnd) currentStep = loopStart;
    }
}
bool Sequencer::triggered() { return triggered_; }
int  Sequencer::getNote()    { return steps[currentStep].note; }
void Sequencer::setNote(int step, int note) {
    if(step >= 0 && step < STEPS) steps[step].note = note;
}
void Sequencer::setGate(int step, bool gate) {
    if(step >= 0 && step < STEPS) steps[step].gate = gate;
}
void Sequencer::setMode(int step, StepData::Mode mode) {
    if(step >= 0 && step < STEPS) steps[step].mode = mode;
}
void Sequencer::setBpm(float bpm) { bpm_ = bpm; calcStepSamples(); }
void Sequencer::setLoopRange(int start, int end) {
    if(start >= 0 && end < STEPS && start <= end) {
        loopStart = start; loopEnd = end;
    }
}
void Sequencer::play()  { isPlaying = true; }
void Sequencer::stop()  { isPlaying = false; triggered_ = false; }
void Sequencer::reset() { currentStep = loopStart; sampleCount = 0; triggered_ = false; }
void Sequencer::tickMaster(double masterClock, double baseStepSamples, float bpmMult) {
    triggered_ = false;
    int loopLen = loopEnd - loopStart + 1;
    int effectiveSteps[STEPS];
    int effectiveCount = 0;
    for(int i = 0; i < loopLen; i++) {
        int idx = loopStart + i;
        if(steps[idx].mode != StepData::SKIP) {
            effectiveSteps[effectiveCount++] = i;
        }
    }
    if(effectiveCount == 0) return;
    double voiceStepSamples = baseStepSamples / bpmMult;
    int effectivePos = (int)(masterClock / voiceStepSamples) % effectiveCount;
    int newStep = loopStart + effectiveSteps[effectivePos];
    if(newStep != lastMasterStep) {
        lastMasterStep = newStep;
        currentStep = newStep - loopStart;
        if(steps[newStep].mode == StepData::OFF) {
            triggered_ = false;
        } else {
            float r = (float)rand() / (float)RAND_MAX;
            if(steps[newStep].gate && r <= steps[newStep].prob)
                if(skipNextTrigger_){ skipNextTrigger_=false; } else { triggered_ = true; }
        }
    } else if(effectiveCount == 1) {
        int fullCycles = (int)(masterClock / voiceStepSamples);
        if(fullCycles != lastCycleCount_) {
            lastCycleCount_ = fullCycles;
            if(steps[newStep].mode != StepData::OFF) {
                float r = (float)rand() / (float)RAND_MAX;
                if(steps[newStep].gate && r <= steps[newStep].prob)
                    if(skipNextTrigger_){ skipNextTrigger_=false; } else { triggered_ = true; }
            }
        }
    }
}
