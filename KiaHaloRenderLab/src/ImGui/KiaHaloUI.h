#pragma once

#include "imconfig.h"
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"
#include "imstb_rectpack.h"
#include "imstb_textedit.h"
#include "imstb_truetype.h"

namespace KiaHaloUI {
	bool DragFloat(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format);
}