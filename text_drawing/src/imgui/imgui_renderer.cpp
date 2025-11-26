static bool WidgetsNeedRefresh = false;

bool RendererImGui::Init(ArenaAllocator* arena)
{
    const u32 max_widgets = 128;
    ini_filename = kraft::StringFormat(arena, "%S/kraft_sample_app_imgui_config.ini", kraft::Engine::base_path);
    widgets = ArenaPushArray(arena, ImGuiWidget, max_widgets);
    widget_count = 0;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    io.IniFilename = (char*)ini_filename.ptr;

    kraft::r::VulkanImgui::Init();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    const float DPI = kraft::Platform::GetWindow()->DPI;

    // Upload Fonts
    ImGuiIO& IO = ImGui::GetIO();
    {
        fs::FileHandle Handle;
        KASSERT(fs::OpenFile(String8Raw("res/fonts/PlusJakartaSans-Regular.ttf"), fs::FILE_OPEN_MODE_READ, true, &Handle));

        u64  Size = fs::GetFileSize(&Handle);
        u8*  Buffer = (u8*)kraft::Malloc(Size);
        bool JakartaRegular = fs::ReadAllBytes(&Handle, &Buffer, &Size);
        KASSERT(JakartaRegular);

        // Atlas will be freed by ImGui
        IO.Fonts->AddFontFromMemoryTTF(Buffer, (int)Size, 14.0f * DPI);
        // ImGui_ImplVulkan_CreateFontsTexture();

        // kraft::Free(Buffer/*, Size, kraft::MEMORY_TAG_FILE_BUF*/);
    }

    // ImGui::GetStyle().WindowPadding = ImVec2(14, 8);
    // ImGui::GetStyle().FramePadding = ImVec2(10, 8);
    // ImGui::GetStyle().ItemSpacing = ImVec2(8, 12);
    // ImGui::GetStyle().ItemInnerSpacing = ImVec2(4, 4);

    // Borders
    // ImGui::GetStyle().WindowBorderSize = 1.0f;
    // ImGui::GetStyle().ChildBorderSize = 1.0f;
    // ImGui::GetStyle().PopupBorderSize = 1.0f;
    // ImGui::GetStyle().FrameBorderSize = 1.0f;

    // Border Rounding
    ImGui::GetStyle().FrameRounding = 2.0f;

    IO.DisplaySize = ImVec2(IO.DisplaySize.x * DPI, IO.DisplaySize.y * DPI);
    ImGui::GetStyle().ScaleAllSizes(DPI);

    // Colors
    // {
    //     ImVec4* colors = ImGui::GetStyle().Colors;
    //     colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    //     colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    //     colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    //     colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    //     colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    //     colors[ImGuiCol_Border] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    //     colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    //     colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    //     colors[ImGuiCol_FrameBgHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    //     colors[ImGuiCol_FrameBgActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    //     colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
    //     colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
    //     colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    //     colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    //     colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    //     colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    //     colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    //     colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    //     colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    //     colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    //     colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    //     colors[ImGuiCol_Button] = ImVec4(0.90f, 0.28f, 0.30f, 1.00f);
    //     colors[ImGuiCol_ButtonHovered] = ImVec4(0.93f, 0.36f, 0.37f, 1.00f);
    //     colors[ImGuiCol_ButtonActive] = ImVec4(0.93f, 0.36f, 0.37f, 1.00f);
    //     colors[ImGuiCol_Header] = ImVec4(1.00f, 0.09f, 0.25f, 0.18f);
    //     colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    //     colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    //     colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    //     colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    //     colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    //     colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    //     colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    //     colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    //     colors[ImGuiCol_Tab] = ImVec4(1.00f, 0.09f, 0.25f, 0.18f);
    //     colors[ImGuiCol_TabHovered] = ImVec4(1.00f, 0.04f, 0.23f, 0.27f);
    //     colors[ImGuiCol_TabActive] = ImVec4(1.00f, 0.04f, 0.23f, 0.27f);
    //     colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    //     colors[ImGuiCol_TabUnfocusedActive] = ImVec4(1.00f, 0.09f, 0.25f, 0.18f);
    //     colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    //     colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    //     colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    //     colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    //     colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    //     colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    //     colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    //     colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    //     colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    //     colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    //     colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    //     colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    //     colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    //     colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    //     colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    //     colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    //     colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    // }

    return true;
}

void RendererImGui::AddWidget(String8 name, ImGuiRenderCallback callback)
{
    widgets[widget_count++] = ImGuiWidget(name, callback);

    KINFO("Added imgui widget %S", name);
}

ImTextureID RendererImGui::AddTexture(kraft::r::Handle<kraft::Texture> texture, kraft::r::Handle<kraft::TextureSampler> sampler)
{
    return kraft::r::VulkanImgui::AddTexture(texture, sampler);
}

void RendererImGui::RemoveTexture(ImTextureID TextureID)
{
    return kraft::r::VulkanImgui::RemoveTexture(TextureID);
}

void RendererImGui::RenderWidgets()
{
    for (int i = 0; i < widget_count; i++)
    {
        widgets[i].callback(WidgetsNeedRefresh);
    }

    WidgetsNeedRefresh = false;
}

void RendererImGui::OnResize(int width, int height)
{
    WidgetsNeedRefresh = true;
}

void RendererImGui::Destroy()
{
    kraft::r::VulkanImgui::Destroy();
}

// Only valid if multi-viewports is enabled
void RendererImGui::EndFrameUpdatePlatformWindows()
{
    // Update and Render additional Platform Windows
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    kraft::r::VulkanImgui::PostFrameCleanup();
}

void RendererImGui::BeginFrame()
{
    kraft::r::VulkanImgui::BeginFrame();
    ImGui::NewFrame();
}

void RendererImGui::EndFrame()
{
    ImGui::Render();
    ImDrawData* DrawData = ImGui::GetDrawData();

    kraft::r::VulkanImgui::EndFrame(DrawData);
}
