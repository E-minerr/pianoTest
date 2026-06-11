#pragma once
#include <vector>
#include <string>
#include <map>
#include <algorithm>

struct ScaleDef {
    std::string name;
    std::vector<int> intervals;
};

struct ChordDef {
    std::string name;
    std::vector<int> intervals;
};

class HarmonyEngine {
public:
    HarmonyEngine();
    ScaleDef getScale(const std::string& name, int rootMidi);
    ChordDef getChord(const std::string& name, int rootMidi);
    std::vector<int> invert(std::vector<int> notes, int times);
    std::vector<int> getAvailableNotes(const std::string& scaleName, int rootMidi);
    std::vector<ChordDef> getDiatonicChords(const std::string& scaleName, int rootMidi);
private:
    std::map<std::string, ScaleDef> scaleTable;
    std::map<std::string, ChordDef> chordTable;
    void buildScaleTable();
    void buildChordTable();
};