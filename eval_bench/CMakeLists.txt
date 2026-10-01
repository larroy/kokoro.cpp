# G2P evaluation tools: dump our phonemes for comparison against espeak-ng (compare_g2p.py).
add_executable(g2p_dump g2p_dump.cpp)
target_link_libraries(g2p_dump PRIVATE kokoro_core)
