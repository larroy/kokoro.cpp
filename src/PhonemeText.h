#pragma once
#include <string>

// Drops spaces before closing punctuation (,.;:!?…)”) and after opening punctuation ((“¿¡), collapses runs, trims.
std::u16string tidy_spaces(const std::u16string& text);
