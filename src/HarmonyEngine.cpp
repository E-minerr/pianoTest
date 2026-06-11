#include "HarmonyEngine.h"

HarmonyEngine::HarmonyEngine() {
    buildScaleTable();
    buildChordTable();
}

// ========== スケールテーブル ==========
void HarmonyEngine::buildScaleTable() {

    // メジャー系
    scaleTable["major"]        = {"Major",           {0,2,4,5,7,9,11}};
    scaleTable["lydian"]       = {"Lydian",           {0,2,4,6,7,9,11}};
    scaleTable["lydianb7"]     = {"Lydian b7",        {0,2,4,6,7,9,10}};
    scaleTable["mixolydian"]   = {"Mixolydian",       {0,2,4,5,7,9,10}};

    // マイナー系
    scaleTable["naturalMinor"] = {"Natural Minor",    {0,2,3,5,7,8,10}};
    scaleTable["harmonicMin"]  = {"Harmonic Minor",   {0,2,3,5,7,8,11}};
    scaleTable["melodicMin"]   = {"Melodic Minor",    {0,2,3,5,7,9,11}};
    scaleTable["dorian"]       = {"Dorian",           {0,2,3,5,7,9,10}};
    scaleTable["phrygian"]     = {"Phrygian",         {0,1,3,5,7,8,10}};
    scaleTable["phrygianDom"]  = {"Phrygian Dominant",{0,1,4,5,7,8,10}};
    scaleTable["locrian"]      = {"Locrian",          {0,1,3,5,6,8,10}};
    scaleTable["locrianS2"]    = {"Locrian #2",       {0,2,3,5,6,8,10}};

    // ペンタ系
    scaleTable["majorPenta"]   = {"Major Pentatonic", {0,2,4,7,9}};
    scaleTable["minorPenta"]   = {"Minor Pentatonic", {0,3,5,7,10}};

    // クロマチック系
    scaleTable["diminished"]   = {"Diminished",       {0,2,3,5,6,8,9,11}};
    scaleTable["augmented"]    = {"Augmented",        {0,3,4,7,8,11}};

    // ブルース系
    scaleTable["minorBlues"]   = {"Minor Blues",      {0,3,5,6,7,10}};
    scaleTable["majorBlues"]   = {"Major Blues",      {0,2,3,4,7,9}};

    // その他
    scaleTable["wholeTone"]    = {"Whole Tone",       {0,2,4,6,8,10}};
    scaleTable["chromatic"]    = {"Chromatic",        {0,1,2,3,4,5,6,7,8,9,10,11}};

    // ビバップ系
    scaleTable["bebopMajor"]   = {"Bebop Major",      {0,2,4,5,7,8,9,11}};
    scaleTable["bebopDom"]     = {"Bebop Dominant",   {0,2,4,5,7,9,10,11}};
}

// ========== コードテーブル ==========
void HarmonyEngine::buildChordTable() {

    // 3和音
    chordTable["maj"]      = {"Major",           {0,4,7}};
    chordTable["min"]      = {"Minor",           {0,3,7}};
    chordTable["dim"]      = {"Diminished",      {0,3,6}};
    chordTable["aug"]      = {"Augmented",       {0,4,8}};
    chordTable["sus2"]     = {"Sus2",            {0,2,7}};
    chordTable["sus4"]     = {"Sus4",            {0,5,7}};

    // 6th系
    chordTable["maj6"]     = {"Major 6",         {0,4,7,9}};
    chordTable["min6"]     = {"Minor 6",         {0,3,7,9}};

    // 7th系
    chordTable["maj7"]     = {"Major 7",         {0,4,7,11}};
    chordTable["min7"]     = {"Minor 7",         {0,3,7,10}};
    chordTable["dom7"]     = {"Dominant 7",      {0,4,7,10}};
    chordTable["dim7"]     = {"Diminished 7",    {0,3,6,9}};
    chordTable["m7b5"]     = {"Half Diminished", {0,3,6,10}};
    chordTable["minmaj7"]  = {"Minor Major 7",   {0,3,7,11}};
    chordTable["aug7"]     = {"Augmented 7",     {0,4,8,10}};
    chordTable["augmaj7"]  = {"Augmented Maj7",  {0,4,8,11}};

    // 9th系
    chordTable["dom9"]     = {"Dominant 9",      {0,4,7,10,14}};
    chordTable["maj9"]     = {"Major 9",         {0,4,7,11,14}};
    chordTable["min9"]     = {"Minor 9",         {0,3,7,10,14}};
    chordTable["dom7b9"]   = {"Dom 7b9",         {0,4,7,10,13}};
    chordTable["dom7s9"]   = {"Dom 7#9",         {0,4,7,10,15}};
    chordTable["maj9s11"]  = {"Major 9#11",      {0,4,7,11,14,18}};
    chordTable["dom9s11"]  = {"Dom 9#11",        {0,4,7,10,14,18}};

    // 11th系
    chordTable["dom11"]    = {"Dominant 11",     {0,4,7,10,14,17}};
    chordTable["min11"]    = {"Minor 11",        {0,3,7,10,14,17}};
    chordTable["dom7s11"]  = {"Dom 7#11",        {0,4,7,10,18}};

    // 13th系
    chordTable["dom13"]       = {"Dominant 13",     {0,4,7,10,14,17,21}};
    chordTable["min13"]       = {"Minor 13",        {0,3,7,10,14,17,21}};
    chordTable["dom13s11"]    = {"Dom 13#11",       {0,4,7,10,14,18,21}};
    chordTable["dom7b9b13"]   = {"Dom 7b9b13",      {0,4,7,10,13,20}};

    // add系
    chordTable["add9"]     = {"Add 9",           {0,4,7,14}};
    chordTable["minAdd9"]  = {"Minor Add9",      {0,3,7,14}};
    chordTable["maj69"]    = {"Major 6/9",       {0,4,7,9,14}};
    chordTable["min69"]    = {"Minor 6/9",       {0,3,7,9,14}};

    // sus系
    chordTable["7sus4"]    = {"7 Sus4",          {0,5,7,10}};
    chordTable["sus4b9"]   = {"Sus4 b9",         {0,5,7,13}};

    // altered系
    chordTable["dom7b5"]   = {"Dom 7b5",         {0,4,6,10}};
    chordTable["dom7s5"]   = {"Dom 7#5",         {0,4,8,10}};
    chordTable["maj7s5"]   = {"Maj 7#5",         {0,4,8,11}};
    chordTable["dom7s5s9"] = {"Dom 7#5#9",       {0,4,8,10,15}};
    chordTable["dom9s5"]   = {"Dom 9#5",         {0,4,8,10,14}};
    chordTable["dom7alt"]  = {"Dom 7 Alt",       {0,4,8,10,13,15}};
    chordTable["pow5"]      = {"Power 5",         {0,7}};
    chordTable["pow5oct"]   = {"Power 5+Oct",     {0,7,12}};
}

// ========== スケール取得 ==========
ScaleDef HarmonyEngine::getScale(const std::string& name, int rootMidi) {
    ScaleDef def = scaleTable[name];
    std::vector<int> notes;
    for(int interval : def.intervals) {
        notes.push_back(rootMidi + interval);
    }
    def.intervals = notes;
    return def;
}

// ========== コード取得 ==========
ChordDef HarmonyEngine::getChord(const std::string& name, int rootMidi) {
    ChordDef def = chordTable[name];
    std::vector<int> notes;
    for(int interval : def.intervals) {
        notes.push_back(rootMidi + interval);
    }
    def.intervals = notes;
    return def;
}

// ========== 転回形計算 ==========
std::vector<int> HarmonyEngine::invert(std::vector<int> notes, int times) {
    for(int i = 0; i < times; i++) {
        notes[0] += 12;
        std::sort(notes.begin(), notes.end());
    }
    return notes;
}

// ========== 使用可能音取得 ==========
std::vector<int> HarmonyEngine::getAvailableNotes(const std::string& scaleName, int rootMidi) {
    std::vector<int> notes;
    ScaleDef def = scaleTable[scaleName];
    // 全オクターブ（MIDI 0〜127）に展開
    for(int oct = 0; oct < 11; oct++) {
        for(int interval : def.intervals) {
            int note = rootMidi + interval + (oct * 12);
            if(note >= 0 && note <= 127) {
                notes.push_back(note);
            }
        }
    }
    std::sort(notes.begin(), notes.end());
    return notes;
}

// ========== ダイアトニックコード取得 ==========
std::vector<ChordDef> HarmonyEngine::getDiatonicChords(const std::string& scaleName, int rootMidi) {
    std::vector<ChordDef> result;
    ScaleDef scale = scaleTable[scaleName];
    for(int interval : scale.intervals) {
        // 各スケール音をルートとしてメジャー・マイナー・ディミニッシュを試みる
        // 今はシンプルにスケール音上のトライアドを返す
        int root = rootMidi + interval;
        ChordDef c = getChord("maj", root);
        result.push_back(c);
    }
    return result;
}