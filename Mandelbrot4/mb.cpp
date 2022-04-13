#include <stdio.h>
#include "imgui.h"
#include <windows.h>

// threading stuff
#include <thread>
#include <future>
#include <mutex>

// allow fout
#pragma warning(disable : 4996)

// mandelbrot includes
#include "mandelbrot.h"
#include "mb.h"

static void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

namespace MB
{
    using real = double;

    void RenderUI(SDL_Window* window, SDL_GLContext context)
    {
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;


        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        else
        {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", nullptr, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }



        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Options"))
            {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.
                ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
                ImGui::MenuItem("Padding", NULL, &opt_padding);
                ImGui::Separator();

                if (ImGui::MenuItem("Flag: NoSplit", "", (dockspace_flags & ImGuiDockNodeFlags_NoSplit) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoSplit; }
                if (ImGui::MenuItem("Flag: NoResize", "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoResize; }
                if (ImGui::MenuItem("Flag: NoDockingInCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingInCentralNode) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingInCentralNode; }
                if (ImGui::MenuItem("Flag: AutoHideTabBar", "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar; }
                if (ImGui::MenuItem("Flag: PassthruCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0, opt_fullscreen)) { dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode; }
                ImGui::Separator();

            }

            ImGui::EndMenuBar();
        }
        ImGui::Begin("Location");
        // ======================================================================================
        // SETTING VARIABLES
        // TODO: move some or all of these somewhere else where it makes more sense
        //       maybe make some structs that countain the values and can be passed
        //       back and forth?
        static double magn = 1.; // default magnification on startup
        static double magn_slider_min = 0.01; // magnification slider starting values
        static double magn_slider_max = 100; // magnification slider starting values
        static double max_iter_float = 250; // default max_iter on startup
        static double max_iter_float_slider_min = 125; // max iter slider starting values
        static double max_iter_float_slider_max = 500; // max iter slider starting values
        // center starting values
        static double center[2] = { -0.5, 0.0 };
        //static double center[2] = {-0.12210986062462065, 0.64954319871203645};
        static double center_slider_minima[2] = { center[0] - 1.5 / magn, center[1] - 1.5 / magn };
        static double center_slider_maxima[2] = { center[0] + 1.5 / magn, center[1] + 1.5 / magn };
        // display parameters
        static int display_magn = 1; // intermediate variable for zoom
        static float zoom_factor = 1.f; // zoom in main display
        static float prev_zoom_fac = 1.f; // previous zoom, used for zoom placement
        static bool image_was_zoomed = false; // used for mouse wheel zoom in main display
        static float newXScroll = 0.; // used for mouse wheel zoom in main display
        static float newYScroll = 0.; // used for mouse wheel zoom in main display
        // render settings
        static int imgWidth = 1280;
        static int imgHeight = 720;
        static int imgWidthOld = 1280;
        static int imgHeightOld = 720;
        static int quality = 0; // Quality determines the number of passes
        static int max_passes = (int)pow(2, quality); // 2^quality passes when rendering
        static std::vector<float*> vec_img_f;
        //static float* image_float = new float[imgWidth * imgHeight * 3]; // not used atm
        static GLuint texture; // OpenGL texture ID for displaying the fractal
        //bool ret = false; // ???
        static float colorDensity = 1.f;
        static float colorOffset = 0.f;
        // fix a weird offset when zooming:
        static int WeirdX = 1;
        static int WeirdY = 90;
        static int WeirdFac = 1;
        // timing:
        static auto t1 = high_resolution_clock::now();
        static auto t2 = high_resolution_clock::now();
        // ======================================================================================


        // CENTER SETTINGS + back and forth conversions for text > double and vice versa
        ImGui::Text("Center:");
        ImGui::SameLine(); HelpMarker("Complex value of the point at the center of the image");
        ImGui::InputDouble("center real", &center[0]);
        ImGui::SliderScalar("Slider center real", ImGuiDataType_Double, &center[0], &center_slider_minima[0], &center_slider_maxima[0], "%.15f");
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            center_slider_minima[0] = center[0] - 1.5 / magn; // TODO: span of 1.5 is 'stolen' from UF's behavior, make this more sensible
            center_slider_maxima[0] = center[0] + 1.5 / magn; //       and move it to where it makes more sense
        }
        ImGui::InputDouble("center imag", &center[1]);
        ImGui::SliderScalar("Slider center imag", ImGuiDataType_Double, &center[1], &center_slider_minima[1], &center_slider_maxima[1], "%.15f");
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            center_slider_minima[1] = center[1] - 1.5 / magn;
            center_slider_maxima[1] = center[1] + 1.5 / magn;
        }

        // Magnification Slider
        ImGui::SliderScalar("Slider magn", ImGuiDataType_Double, &magn, &magn_slider_min, &magn_slider_max, "Magnification: %.10f", ImGuiSliderFlags_Logarithmic);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            magn_slider_min = max(0.01 * magn, 0.00000000000001);
            magn_slider_max = 100 * magn;
        }
        // Max Iter Slider
        ImGui::SliderScalar("slider max iter", ImGuiDataType_Double, &max_iter_float, &max_iter_float_slider_min, &max_iter_float_slider_max, "Max. Iterations: %0f", ImGuiSliderFlags_Logarithmic);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            max_iter_float_slider_min = max(0.5 * max_iter_float, 1);
            max_iter_float_slider_max = 2 * max_iter_float;
        }
        ImGui::End();

        
        // Coloring settings
        ImGui::Begin("Coloring");
        ImGui::InputFloat("Color Density", &colorDensity);
        ImGui::InputFloat("Gradient Offset", &colorOffset);
        ImGui::Text("Transfer function");
        const char* items[] = { "Linear", "Square", "Cube", "Square Root", "Cube Root", "Log" };
        static int item_current_idx = 0; // Here we store our selection data as an index.
        const char* combo_preview_value = items[item_current_idx];  // Pass in the preview value visible before opening the combo (it could be anything)
        if (ImGui::BeginCombo("Transfer Function", combo_preview_value, 0)); //, flags))
        {
            for (int n = 0; n < IM_ARRAYSIZE(items); n++)
            {
                const bool is_selected = (item_current_idx == n);
                if (ImGui::Selectable(items[n], is_selected))
                    item_current_idx = n;

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::End();


        ImGui::Begin("Rendering");
        // Quality Settings:
        static int imgSize[2] = { 1280, 720 };
        ImGui::Text("Image Resolution");
        ImGui::InputInt2("input int2", imgSize);
        ImGui::DragInt("Render Quality:", &quality, 0.025, 0, 12, "Render Quality: %d", ImGuiSliderFlags_AlwaysClamp);
        max_passes = (int)pow(2.0f, (float)quality);
        ImGui::Text("Render with %d passes", max_passes);
        // Press Start to Play:
        if (ImGui::Button("Calculate"))
        {
            int max_iter = (int)max_iter_float; // TODO: this is dumb, fix it
            calc_mb(center[0], center[1], magn, max_iter, colorDensity, colorOffset, max_passes, &texture, &imgSize[0], &imgSize[1]);
            //calc_mb(center[0], center[1], magn, max_iter, colorDensity, colorOffset, max_passes, vec_img_f, &imgSize[0], &imgSize[1]);
        }

        //ImGui::Spacing();
        //ImGui::Text("100\% Professional Debugging Stuff D:");
        //ImGui::SliderInt("Weird X offset", &WeirdX, -100, 100);
        //ImGui::SliderInt("Weird Y offset", &WeirdY, -100, 100);
        ImGui::End();


//        // MAIN DISPLAY REFRESH
//        t2 = high_resolution_clock::now();
//        auto ms_int = duration_cast<milliseconds>(t2 - t1);
//        int duration = ms_int.count();
//        if (duration >= 1000) // do this once a second only
//        {
//            t1 = high_resolution_clock::now();
//
//            if (imgWidth != imgWidthOld || imgHeight != imgHeightOld)
//            {
//                vec_img_f.resize(imgWidth * imgHeight * 3);
//                imgWidthOld = imgWidth;
//                imgHeightOld = imgHeight;
//            }
//            // MAKE TEXTURE FROM IMAGE DATA
//            GLuint texture;
//            // create texture to hold image data for rendering
//            glGenTextures(1, &texture);
//            glBindTexture(GL_TEXTURE_2D, texture);
//
//            // Setup filtering parameters for display
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same
//
//            // Upload pixels into texture
//#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
//            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
//#endif
//            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, imgWidth, imgHeight, 0, GL_RGB, GL_FLOAT, vec_img_f.data());
//            //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, imgWidth, imgHeight, 0, GL_RGB, GL_FLOAT, &image_float[0]);
//            cout << "Update tick, reset clock. Cycle took " << duration << " ms.\n";
//        }
        // MAIN DISPLAY
        ImGui::Begin("Main Display", nullptr, ImGuiWindowFlags_HorizontalScrollbar);
        //ImGui::Begin("Main Display" );
        ImVec2 ws = ImGui::GetWindowSize();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
        ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
        prev_zoom_fac = zoom_factor;
        ImGui::Image((void*)(intptr_t)texture, ImVec2(zoom_factor * imgSize[0] , zoom_factor * imgSize[1])); // , texturesize);
        if (image_was_zoomed)
        {
            ImGui::SetScrollX(min(ImGui::GetScrollMaxX(), max(0, newXScroll)));
            ImGui::SetScrollY(min(ImGui::GetScrollMaxY(), max(0, newYScroll)));
            image_was_zoomed = false;
        }
        ImVec2 imgContentMax = ImGui::GetWindowContentRegionMax();
        ImVec2 imgContentMin = ImGui::GetWindowContentRegionMin();
        ImVec2 imgScroll = { ImGui::GetScrollX(), ImGui::GetScrollY() };
        ImVec2 imgScrollMax = { ImGui::GetScrollMaxX(), ImGui::GetScrollMaxY() };
        //ImGui::SetScrollX(500);
        //ImGui::SetScrollY(500);

        // some fun ImGui example code:
        if (ImGui::IsItemHovered())
        {
            // cout << imgContentMin.x << " " << imgContentMax.x << " " << imgContentMin.y << " " << imgContentMax.y << "\n";
            if (io.KeyCtrl && io.KeyShift)
            {
                ImGui::BeginTooltip();
                float region_sz = 32.0f;
                float region_x = io.MousePos.x - pos.x - region_sz * 0.5f;
                float region_y = io.MousePos.y - pos.y - region_sz * 0.5f;
                float zoom = 4.0f;
                if (region_x < 0.0f) { region_x = 0.0f; }
                else if (region_x > imgWidth - region_sz) { region_x = imgWidth - region_sz; }
                if (region_y < 0.0f) { region_y = 0.0f; }
                else if (region_y > imgHeight - region_sz) { region_y = imgHeight - region_sz; }
                ImGui::Text("Coursor pos on image is %.2f, %.2f", io.MousePos.x - pos.x, io.MousePos.y - pos.y);
                ImGui::Text("Max scroll is: %.2f, %.2f", imgScrollMax.x, imgScrollMax.y);
                ImGui::Text("Coursor pos on window is %.2f, %.2f", ws.x + io.MousePos.x - pos.y, ws.x + io.MousePos.y - pos.y);
                ImGui::Text("Window content region: x %.2f, %.2f; y %.2f, %.2f, ", imgContentMin.x, imgContentMax.x - imgContentMin.x, 
                    imgContentMin.y, imgContentMax.y - imgContentMin.y);
                ImGui::Text("Scroll values: %.2f/%.2f, %.2f/%.2f", imgScroll.x, imgScrollMax.x, imgScroll.y, imgScrollMax.y);
                ImGui::Text("Min: (%.2f, %.2f)", region_x, region_y);
                ImGui::Text("Max: (%.2f, %.2f)", region_x + region_sz, region_y + region_sz);
                ImVec2 uv0 = ImVec2((region_x) / imgWidth, (region_y) / imgHeight);
                ImVec2 uv1 = ImVec2((region_x + region_sz) / imgWidth, (region_y + region_sz) / imgHeight);
                ImGui::Image((void*)(intptr_t)texture, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, tint_col, border_col);
                ImGui::EndTooltip();
            }
            if (io.MouseWheel != 0)
            {
                if (io.MouseWheel == 1) {
                    display_magn++; 
                    WeirdFac = 1;
                }
                else if (io.MouseWheel == -1) {
                    display_magn--; 
                    WeirdFac = -1;
                }
                zoom_factor = (display_magn > 0) ? display_magn + 1 : 1.f / (1.f - display_magn);
                float coursorInImgX = io.MousePos.x - pos.x + WeirdFac * WeirdX;
                float coursorInImgY = io.MousePos.y - pos.y + WeirdFac * WeirdY;
                float coursorInImgWindowX = io.MousePos.x - pos.x - ImGui::GetScrollX();
                float coursorInImgWindowY = io.MousePos.y - pos.y - ImGui::GetScrollY();
                float zoom_ratio = zoom_factor / prev_zoom_fac;
                newXScroll = (int)(coursorInImgX * zoom_ratio - (coursorInImgX - ImGui::GetScrollX()) + WeirdFac * WeirdX);
                newYScroll = (int)(coursorInImgY * zoom_ratio - (coursorInImgY - ImGui::GetScrollY()) + WeirdFac * WeirdY);
                cout << "Zoom factor new and old:     " << zoom_factor << " " << prev_zoom_fac << "\n";
                cout << "Coursor position on image:   " << io.MousePos.x - pos.x << " " << io.MousePos.y - pos.y << "\n";
                cout << "Coursor position in window:  " << coursorInImgWindowX << " " << coursorInImgWindowY << "\n";
                cout << "New coursor position on img: " << (io.MousePos.x - pos.x) * zoom_factor / prev_zoom_fac << " " << (io.MousePos.y - pos.y) * zoom_factor / prev_zoom_fac << "\n";
                cout << "Old scroll values X and Y:   " << ImGui::GetScrollX() << " " << ImGui::GetScrollY() << "\n";
                cout << "New scroll values X and Y:   " << newXScroll << " " << newYScroll << "\n";
                cout << "Scroll values are now:       " << ImGui::GetScrollX() << " " << ImGui::GetScrollY() << "\n";
                image_was_zoomed = true;
                
            }
            prev_zoom_fac = zoom_factor;
        }
        
        ImGui::End(); // Main Display
        ImGui::End(); // Dockspace
	}
}