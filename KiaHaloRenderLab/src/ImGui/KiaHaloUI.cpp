#include "KiaHaloUI.h"
#include <string>

bool KiaHaloUI::DragFloat(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    // 获取当前窗口指针，用于后续操作

    if (window->SkipItems)
        return false;
    // 如果当前窗口跳过渲染，直接返回false

    ImGuiContext& g = *GImGui;
    // 获取ImGui上下文

    bool value_changed = false;
    // 用于记录值是否有变化

    ImGui::BeginGroup();
    // 开始一个组，用于放置多个控件

    ImGui::PushID(label);
    // 将标签作为ID压入ID栈中

    int length = sizeof(v) / sizeof(v[0]) + 1;
    ImGui::PushMultiItemsWidths(length, ImGui::CalcItemWidth());
    // 压入多个控件的宽度，并且计算每个控件的宽度

    for (int i = 0; i < length; i++)
    {
        ImGui::PushID(i);
        // 将索引作为ID压入ID栈中

        if (i > 0)
            ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
        // 如果不是第一个控件，将当前控件移到同一行
    
        value_changed |= ImGui::DragFloat("", &v[i], v_speed, v_min, v_max, (std::string(1, format[i]) + ":%.2f").data());
        // 拖拽控件，记录是否值有变化

        ImGui::PopID();
        // 弹出ID栈

        ImGui::PopItemWidth();
        // 弹出宽度栈
    }

    ImGui::PopID();
    // 弹出ID栈

    const char* label_end = ImGui::FindRenderedTextEnd(label);
    // 寻找标签的渲染结束位置

    if (label != label_end)
    {
        ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
        // 将下一个元素移到同一行

        ImGui::TextEx(label, label_end);
        // 渲染标签文本
    }

    ImGui::EndGroup();
    // 结束组

    return value_changed;
}