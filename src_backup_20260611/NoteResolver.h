#pragma once
#include "HarmonyEngine.h"
#include <vector>
#include <string>

class NoteResolver {
public:
    NoteResolver(HarmonyEngine& harmony);

    // スケール・ルート設定
    void setScale(const std::string& scaleName, int rootMidi);

    // コード設定
    void setChord(const std::string& chordName, int rootMidi);

    // オクターブ範囲設定
    void setOctaveRange(int minMidi, int maxMidi);

    // ステップから音を返す
    int resolveNote(int step);

    // コードの構成音を返す（ボイシング用）
    std::vector<int> resolveChord(int step);

    // 使用可能音リスト更新
    void update();

    // モード切替
    enum Mode { SCALE, CHORD, ARPEGGIO };
    void setMode(Mode mode);

private:
    HarmonyEngine&   harmony_;
    std::string      scaleName_  = "naturalMinor";
    std::string      chordName_  = "min7";
    int              rootMidi_   = 52;   // E
    int              minMidi_    = 40;   // E2
    int              maxMidi_    = 76;   // E5
    Mode             mode_       = SCALE;
    std::vector<int> availableNotes_;
    std::vector<int> chordNotes_;

    int nearestNote(int midi);
};