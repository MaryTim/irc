#ifndef MODERESULT_HPP
#define MODERESULT_HPP

#include <string>
#include <vector>

struct ModeResult {
    bool anyChange;
    std::string appliedModes;   // "+it-k"
    std::vector<std::string> modeParams; // ["secretKey", "10"] 
    std::string broadcastLine;  // full IRC line we send to channel
    ModeResult(): anyChange(false) {}
};

#endif