#include "NoteResolver.h"
#include <cstdlib>
#include <algorithm>

NoteResolver::NoteResolver(HarmonyEngine& harmony)
    : harmony_(harmony) {
    update();
}

void NoteResolver::setScale(const std::string& scaleName, int rootMidi) {
    scaleName_ = scaleName;
    rootMidi_  = rootMidi;
    update();
}

void NoteResolver::setChord(const std::string& chordName, int rootMidi) {
    chordName_ = chordName;
    rootMidi_  = rootMidi;
    update();
}

void NoteResolver::setOctaveRange(int minMidi, int maxMidi) {
    minMidi_ = minMidi;
    maxMidi_ = maxMidi;
    update();
}

void NoteResolver::setMode(Mode mode) {
    mode_ = mode;
}

void NoteResolver::update() {
    // スケール上の使用可能音を音域内に絞る
    std::vector<int> all = harmony_.getAvailableNotes(scaleName_, rootMidi_);
    availableNotes_.clear();
    for(int n : all) {
        if(n >= minMidi_ && n <= maxMidi_) {
            availableNotes_.push_back(n);
        }
    }

    // コード構成音も更新
    ChordDef chord = harmony_.getChord(chordName_, rootMidi_);
    chordNotes_.clear();
    for(int n : chord.intervals) {
        if(n >= minMidi_ && n <= maxMidi_) {
            chordNotes_.push_back(n);
        }
    }
}

int NoteResolver::resolveNote(int step) {
    if(availableNotes_.empty()) return 60;

    switch(mode_) {
        case SCALE:
            return availableNotes_[step % availableNotes_.size()];

        case CHORD:
            if(chordNotes_.empty()) return 60;
            return chordNotes_[step % chordNotes_.size()];

        case ARPEGGIO:
            if(chordNotes_.empty()) return 60;
            // 上昇→下降
            {
                int sz = chordNotes_.size();
                int pos = step % (sz * 2);
                if(pos < sz) return chordNotes_[pos];
                else         return chordNotes_[sz * 2 - 1 - pos];
            }

        default:
            return availableNotes_[step % availableNotes_.size()];
    }
}

std::vector<int> NoteResolver::resolveChord(int step) {
    update();
    return chordNotes_;
}

int NoteResolver::nearestNote(int midi) {
    if(availableNotes_.empty()) return midi;
    int nearest = availableNotes_[0];
    int minDist = abs(midi - nearest);
    for(int n : availableNotes_) {
        int dist = abs(midi - n);
        if(dist < minDist) { minDist = dist; nearest = n; }
    }
    return nearest;
}