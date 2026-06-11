#include <atomic>
#pragma once
#include "ofMain.h"
#include "Sequencer.h"
#include "NoteResolver.h"
#include "HarmonyEngine.h"
#include <vector>
#include <string>

struct StepUIData {
    int  velocity    = 100;
    float gate       = 0.5f;
    bool glide       = false;
    int  gridDiv     = 1;
    int  noteRepeat  = 1;
    float prob       = 1.0f;
    int  octShift    = 0;
    int  noteOverride = -1;
    int  chordNotes[7] = {-1,-1,-1,-1,-1,-1,-1};
    int  chordNoteCount = 0;
    enum StepMode { ON, OFF, SKIP };
    StepMode mode = ON;

    bool lockVelocity   = false;
    bool lockGate       = false;
    bool lockProb       = false;
    bool lockGridDiv    = false;
    bool lockNoteRepeat = false;
    bool lockGlide      = false;
    bool lockOctShift   = false;
    bool lockNote       = false;
    bool chordPanelOpen = false;
    std::string chordName = "";
    int  octDelta    = 0;
    bool strumOn = false;
    float strumDelay = 0.02f;
};

struct VoiceCommonParam {
    int   velocity    = 100;
    float gate        = 0.5f;
    float prob        = 1.0f;
    int   gridDiv     = 1;
    int   noteRepeat  = 1;
    bool  glide       = false;
    int   octShift    = 0;
    int   noteOverride = -1;
    int   pianoOctave = 4;
    float bpmMult     = 1.0f;
    std::string scaleName = "naturalMinor";
    int   scaleRoot   = 0;
};

struct VoiceRange {
    int startStep = -1;
    int endStep   = -1;
    int oscIndex  = 0;
    bool active   = false;
    VoiceCommonParam common;
    bool panelOpen = false;
    bool resizing  = false;
    bool playing   = false;
    bool delPending = false;
    bool allLocked  = false;
    bool scaleOpen  = false;
    float panelOffsetX = 0.0f;
    float panelOffsetY = 0.0f;
};

class SequencerUI {
public:
    bool isPanelOpen() const { return panelStep_>=0 && stepData_[panelStep_].chordPanelOpen; }
    SequencerUI(Sequencer seqs[], int numSeqs, NoteResolver& resolver, HarmonyEngine& harmony);

    void draw(float startY);
    void mousePressed(int x, int y, int button);
    void mouseDragged(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseScrolled(int x, int y, float scrollX, float scrollY);

    const StepUIData& getStepData(int step) const;
    int  resolveNote(int step);
    bool isSkip(int step) const;
    bool isScaleNote(int midi, int voiceIdx) const;
    bool resetMasterClock_ = false;
    int  selectedOscVoice_  = 0;
    void setBaseBpm(float bpm) { baseBpm_ = bpm; }

    int  getVoiceCount() const { return voiceCount_; }
    const VoiceRange& getVoice(int v) const { return voices_[v]; }

    static constexpr int COLS        = 16;
    static constexpr int ROWS        = 6;
    static constexpr int TOTAL_STEPS = COLS * ROWS;
    static constexpr int MAX_VOICES  = 6;

private:
    Sequencer*     seqs_;
    int            numSeqs_;
    NoteResolver&  resolver_;
    HarmonyEngine& harmony_;

    StepUIData stepData_[TOTAL_STEPS];
    VoiceRange voices_[MAX_VOICES];
    std::atomic<int> voiceCount_ = 0;
    float      baseBpm_    = 60.0f;

    float uiY_   = 470.0f;
    float stepW_ = 62.0f;
    float stepH_ = 72.0f;
    float padX_  = 8.0f;
    float ledR_  = 14.0f;

    ofColor colGold_  = ofColor(200, 164, 74);
    ofColor colRed_   = ofColor(200, 60,  60);
    ofColor colWhite_ = ofColor(220, 220, 220);
    ofColor colDim_   = ofColor(45,  45,  45);

    bool showVelocity_ = false;
    int  panelStep_    = -1;
    bool panelOnRight_ = true;
    int  chordScrollOffset_ = 0;
    int  pianoOctave_  = 4;

    int   dragStep_     = -1;
    int   dragType_     = -1;
    float dragStartY_   = 0;
    float dragStartVal_ = 0;
    bool  dragIsVoice_  = false;
    int   dragVoiceIdx_ = -1;

    bool  voiceDragging_  = false;
    int   panelDragVoice_  = -1;
    float panelDragStartX_ = 0.0f;
    float panelDragStartY_ = 0.0f;
    float panelDragOffsetX_= 0.0f;
    float panelDragOffsetY_= 0.0f;
    int   voiceDragStart_ = -1;
    int   voiceDragEnd_   = -1;
    int   voiceResizeIdx_ = -1;

    void drawTopBar(float startY);
    void drawGrid(float startY);
    void drawStep(int idx, float x, float y);
    void drawVelocityBar(int idx, float x, float y);
    void drawPanel(int stepIdx);
    void drawVoicePanel(int voiceIdx);
    void drawMiniPiano(float x, float y, float w, float h, int stepIdx, bool isVoicePanel, int voiceIdx);
    void drawVoiceRanges(float startY);
    void drawVoiceBadges(float startY);

    void getStepRect(int idx, float startY, float& ox, float& oy) const;
    int  stepAtPos(int mx, int my, float startY) const;
    int  voiceBadgeAtPos(int mx, int my, float startY) const;
    int  voiceRightEdgeAtPos(int mx, int my, float startY) const;
    void cycleStepMode(int idx);
    void removeVoice(int voiceIdx);
    string noteNameStr(int midi) const;
    int  voiceAtStep(int step) const;
    void addVoiceRange(int startStep, int endStep);
    bool rangeOverlaps(int startStep, int endStep) const;
    bool rangeOverlapsExcept(int startStep, int endStep, int excludeVoice) const;
    bool isScaleNote(int midi) const;

    void getVoicePanelRect(int voiceIdx, float& px, float& py, float& pw, float& ph) const;
    void applyVoiceCommonToSteps(int voiceIdx);
};
