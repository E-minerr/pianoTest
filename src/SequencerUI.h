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
    bool lockStrum      = false;
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

    // originX: 正方形メインエリアの左端Xピクセル(=左Margin幅。今回は468固定)
    // monitorTop: メインモニター(左メニュー+detail-area)の上端Yピクセル
    void draw(float originX, float monitorTop);
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
    float globalBpm_     = 60.0f;
    float globalVolume_  = 1.0f;
    bool  globalPlaying_ = true;
    ofTrueTypeFont* jbFont_   = nullptr;
    ofTrueTypeFont* jbFont14_ = nullptr;
    ofTrueTypeFont* jbFont12_ = nullptr;
    void setBaseBpm(float bpm) { baseBpm_ = bpm; }

    int  getVoiceCount() const { return voiceCount_; }
    const VoiceRange& getVoice(int v) const { return voices_[v]; }

    static constexpr int COLS        = 16;
    static constexpr int ROWS        = 6;
    static constexpr int TOTAL_STEPS = COLS * ROWS;
    static constexpr int MAX_VOICES  = 6;

    // 正方形メインエリアの固定寸法(decisions/2026-06-14.md準拠)
    static constexpr float SQUARE_W  = 984.0f;
    static constexpr float MONITOR_H = 492.0f;
    static constexpr float GRID_H    = 492.0f;

private:
    Sequencer*     seqs_;
    int            numSeqs_;
    NoteResolver&  resolver_;
    HarmonyEngine& harmony_;

    StepUIData stepData_[TOTAL_STEPS];
    VoiceRange voices_[MAX_VOICES];
    std::atomic<int> voiceCount_ = 0;
    float      baseBpm_    = 60.0f;

    // ===== レイアウト原点(draw()で毎回更新) =====
    float originX_    = 468.0f;  // 正方形左端
    float monitorTop_ = 68.0f;   // メインモニター上端
    float uiY_         = 560.0f; // ステップグリッド上端(= monitorTop_ + MONITOR_H)

    // ===== グリッド寸法(984幅・padding10・gap4/8で算出) =====
    float stepW_     = 56.5f;
    float stepH_     = 72.0f;
    float gridPadX_  = 10.0f;
    float gridPadY_  = 10.0f;
    float gapX_      = 4.0f;
    float gapY_      = 8.0f;
    float padX_      = 8.0f;   // 互換用(未使用化)
    float ledR_      = 14.0f;

    // ===== 新配色(decisions/2026-06-14.md) =====
    ofColor colCyan_   = ofColor::fromHex(0x6BE4FF);
    ofColor colPink_   = ofColor::fromHex(0xFF6B9D);
    ofColor colYellow_ = ofColor::fromHex(0xE1FF00);
    ofColor colBg_     = ofColor::fromHex(0x1A1A1A);

    // 旧配色(パネル類で当面流用)
    ofColor colGold_  = ofColor(200, 164, 74);
    ofColor colRed_   = ofColor(200, 60,  60);
    ofColor colWhite_ = ofColor(220, 220, 220);
    ofColor colDim_   = ofColor(45,  45,  45);

    // icon-chan色循環(クリックでテスト用に色が変わる、機能は持たない)
    ofColor iconChanColors_[4] = {
        ofColor::fromHex(0xC8A44A), ofColor::fromHex(0x6BE4FF),
        ofColor::fromHex(0xFF6B9D), ofColor::fromHex(0xE1FF00)
    };
    int iconChanColorIdx_ = 0;

    bool showVelocity_ = false;
    int  panelStep_    = -1;
    bool panelOnRight_ = true;
    bool panelShowPiano_ = true;
    bool panelShowParameter_ = true;
    int  chordScrollOffset_ = 0;
    int  pianoOctave_  = 4;

    int   dragStep_     = -1;
    int   dragType_     = -1;
    float dragStartY_   = 0;
    float dragStartVal_ = 0;
    bool  dragIsVoice_  = false;
    int   dragVoiceIdx_ = -1;

    int   panelDragVoice_  = -1;
    float panelDragStartX_ = 0.0f;
    float panelDragStartY_ = 0.0f;
    float panelDragOffsetX_= 0.0f;
    float panelDragOffsetY_= 0.0f;

    // ===== Group(VoiceRange)作成・リサイズ 統一ジェスチャー状態 =====
    enum class GroupGesture { NONE, CREATE, RESIZE_LEFT, RESIZE_RIGHT };
    GroupGesture groupGesture_  = GroupGesture::NONE;
    int  gestureVoiceIdx_ = -1;   // RESIZE時: 対象Voiceのインデックス
    int  gestureAnchor_   = -1;   // CREATE時: ドラッグ開始ステップ / RESIZE時: 固定端ステップ
    int  gestureLive_     = -1;   // 現在ドラッグ中のステップ(可動端)
    static constexpr int MIN_VOICE_LEN = 2;

    // ===== クリック/ドラッグ判定用(移動量で後から判定) =====
    int   pendingClickStep_  = -1;
    float pendingClickX_     = 0.0f;
    float pendingClickY_     = 0.0f;
    int   pendingEdgeVoice_  = -1;   // 押した位置が既存Groupの端だった場合の対象Voice
    bool  pendingEdgeIsLeft_ = false;
    static constexpr float CLICK_DRAG_THRESHOLD = 6.0f;

    // ===== グローバルトランスポート(BPM/VOLドラッグ用) =====
    bool draggingGlobalBpm_    = false;
    bool draggingGlobalVolume_ = false;

    void drawGrid(float startY);
    void drawStep(int idx, float x, float y);
    void drawVelocityBar(int idx, float x, float y);
    void drawPanel(int stepIdx);
    void drawVoicePanel(int voiceIdx);
    void drawMiniPiano(float x, float y, float w, float h, int stepIdx, bool isVoicePanel, int voiceIdx);
    void drawVoiceRanges(float startY);
    void drawVoiceBadges(float startY);
    void drawIconChan(float cx, float cy, float r, ofColor fillCol);
    void drawMainMonitorChrome();
    void drawPanelTabButton(float x, float y, const char* label, bool active);
    void drawGlobalTransport();
    void drawText(const string& s, float x, float y) const;
    void drawText14(const string& s, float x, float y) const;
    void drawText12(const string& s, float x, float y) const;
    void getTransportLayout(float& playX, float& playW, float& bpmX, float& bpmW,
                             float& volX, float& volW, float& rowY, float& rowH,
                             float& iconCx, float& iconCy) const;

    void getStepRect(int idx, float startY, float& ox, float& oy) const;
    int  stepAtPos(int mx, int my, float startY) const;
    int  voiceBadgeAtPos(int mx, int my, float startY) const;
    int  voiceEdgeAtPos(int mx, int my, float startY, bool& isLeftEdge) const;
    void getPanelArea(float& dx, float& contentY, float& halfW, float& halfH) const;
    void cycleStepMode(int idx);
    void removeVoice(int voiceIdx);
    string noteNameStr(int midi) const;
    int  voiceAtStep(int step) const;
    void addVoiceRange(int startStep, int endStep);
    bool rangeOverlaps(int startStep, int endStep) const;
    bool rangeOverlapsExcept(int startStep, int endStep, int excludeVoice) const;
    bool isScaleNote(int midi) const;

    void getVoicePanelRect(int voiceIdx, float& px, float& py, float& pw, float& ph) const;
    void getGroupRowRect(int voiceIdx, float& rx, float& ry) const;
    void getGroupRowLayout(int voiceIdx, float& ry, float& rowH,
                           float& groupW, float& oscX, float& oscW,
                           float& playX2, float& playW2,
                           float multX[5], float multW[5],
                           float& delX, float& delW) const;
    void applyVoiceCommonToSteps(int voiceIdx);
};
