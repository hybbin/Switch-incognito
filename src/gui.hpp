// Copyright (C) 2020 averne
//
// This file is part of Turnips.
//
// Turnips is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Turnips is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Turnips.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <cstdio>
#include <utility>
#include <math.h>
#include "json.hpp"
#include "incognito.h"
#include "gui.hpp"
#include "theme.hpp"
#include "incognito.h"

#include <cstdint>
#include <array>
#include <string>
#include <imgui.h>
#include <switch.h>
#include "incognito.h"
#include "Lang.hpp"


namespace im {
    using namespace ImGui;
} // namespace im

namespace gui {

bool init();
bool loop();
void render();
void exit();

bool create_background(const std::string &path);

void draw_main_function();


template <typename F>
void do_with_color(std::uint32_t col, F f) {
    im::PushStyleColor(ImGuiCol_Text, col);
    f();
    im::PopStyleColor();
}

} // namespace gui
