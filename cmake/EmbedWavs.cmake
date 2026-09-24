# Generates C++ byte arrays at build time so installed plugins do not need external WAV files.
set(files "1.wav" "2.wav" "3.wav" "4.wav" "5 cuffia.wav")
set(symbols "kNoise1" "kNoise2" "kNoise3" "kNoise4" "kNoiseCuffie")

file(WRITE "${OUTPUT}" "#pragma once\n\n#include <cstddef>\n#include <cstdint>\n\nnamespace CullamiWavData {\n")

list(LENGTH files fileCount)
math(EXPR lastIndex "${fileCount} - 1")
foreach(index RANGE ${lastIndex})
    list(GET files ${index} filename)
    list(GET symbols ${index} symbol)
    file(READ "${INPUT_DIR}/${filename}" hexData HEX)
    string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1," bytes "${hexData}")
    string(REGEX REPLACE "((0x[0-9A-Fa-f][0-9A-Fa-f],){16})" "\\1\n" bytes "${bytes}")
    file(APPEND "${OUTPUT}" "\nstatic const uint8_t ${symbol}[] = {${bytes}};\n")
    file(APPEND "${OUTPUT}" "static constexpr std::size_t ${symbol}Size = sizeof(${symbol});\n")
endforeach()

file(APPEND "${OUTPUT}" "\n} // namespace CullamiWavData\n")
