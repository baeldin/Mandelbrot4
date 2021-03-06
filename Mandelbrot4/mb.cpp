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

    void RenderUI()
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


        static bool showGradientEditor = false;
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
            if (ImGui::BeginMenu("Windows"))
            {
                if (ImGui::MenuItem("Gradient Editor", ""))
                {
                    showGradientEditor = (showGradientEditor) ? false : true;
                }
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
        static char mainDisplayString[50] = "Main Display (100\%)";
        // render settings
        static int imgWidth = 1280;
        static int imgHeight = 720;
        static int imgWidthOld = 1280; // used to detect changes in img size
        static int imgHeightOld = 720; // used to detect changes in img size
        static int quality = 0; // Quality determines the number of passes
        static int max_passes = (int)pow(2, quality); // 2^quality passes when rendering
        static std::vector<float> vec_img_f(imgWidth * imgHeight * 3, 0);
        static GLuint texture; // OpenGL texture ID for displaying the fractal
        static bool need_texture = true;
        static char png_out_name[64] = "frychtal.png";
        static float colorDensity = 1.f;
        static float colorOffset = 0.f;
        static float progress = 1.f;
        static GLuint gradient_img;
        // fix a weird offset when zooming:
        static int WeirdX = 1;
        static int WeirdY = 90;
        static int WeirdFac = 1;
        // timing:
        static auto t1 = high_resolution_clock::now();
        static auto t2 = high_resolution_clock::now();
        // threading stuff
        static int current_passes = 0;
        static std::thread t;
        static bool running = false;
        static bool stop = false;
        static std::mutex m;
        // ======================================================================================

        // CENTER SETTINGS + back and forth conversions for text > double and vice versa
        static fractalPosition pos;
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
        pos.center = Complex(center[0], center[1]);
        // Magnification Slider
        ImGui::InputDouble("Magnification", &magn);
        ImGui::SliderScalar("Slider magn", ImGuiDataType_Double, &magn, &magn_slider_min, &magn_slider_max, "Magnification: %.10f", ImGuiSliderFlags_Logarithmic);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            magn_slider_min = max(0.01 * magn, 0.00000000000001);
            magn_slider_max = 100 * magn;
        }
        pos.magn = magn;
        // Rotation
        static double rotation = 0;
        ImGui::InputDouble("Rotation", &rotation);
        pos.rotation = Complex(cos(rotation / 180.f * 3.1415926f), sin(rotation / 180.f * 3.1415926f));
        // Max Iter Slider
        ImGui::InputDouble("Max Iterations", &max_iter_float);
        ImGui::SliderScalar("slider max iter", ImGuiDataType_Double, &max_iter_float, &max_iter_float_slider_min, &max_iter_float_slider_max, "Max. Iterations: %0f", ImGuiSliderFlags_Logarithmic);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            max_iter_float_slider_min = max(0.5 * max_iter_float, 1);
            max_iter_float_slider_max = 2 * max_iter_float;
        }
        pos.maxIter = (int)max_iter_float;
        ImGui::End();


        // Coloring settings
        ImGui::Begin("Coloring");
        static colorSettings colors;
        ImGui::InputFloat("Color Density", &colorDensity);
        colors.colorDensity = colorDensity;
        ImGui::InputFloat("Gradient Offset", &colorOffset);
        colors.colorOffset = colorOffset;
        ImGui::Text("Transfer function");
        static bool repeat_gradient = true;
        ImGui::Checkbox("Repeat Gradient", &repeat_gradient);
        colors.repeat_gradient = repeat_gradient;
        //const char* items[] = { "Linear", "Square", "Cube", "Square Root", "Cube Root", "Log" };
        //static int item_current_idx = 0; // Here we store our selection data as an index.
        //const char* combo_preview_value = items[item_current_idx];  // Pass in the preview value visible before opening the combo (it could be anything)
        //if (ImGui::BeginCombo("Transfer Function", combo_preview_value, 0)); //, flags))
        //{
        //    for (int n = 0; n < IM_ARRAYSIZE(items); n++)
        //    {
        //        const bool is_selected = (item_current_idx == n);
        //        if (ImGui::Selectable(items[n], is_selected))
        //            item_current_idx = n;

        //        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
        //        if (is_selected)
        //            ImGui::SetItemDefaultFocus();
        //    }
        //    ImGui::EndCombo();
        //}
        static color insideColor(0, 0, 0);
        ImGui::ColorEdit3("Inside Color:", (float*)&insideColor);
        colors.insideColor = insideColor;
        const char* items[] = { "UF Default", "UF Default Muted", "Volcano Under a Glacier", "Jet", "CBR_coldhot", "Default"};
        static int current_gradient = 0;
        static int previious_gradient = -1; // make sure that this is different initially
        static std::vector<color> gradient_img_data(400 * 30, color(0));
        static std::vector<Gradient> gradients = { uf_default, standard_muted, volcano_under_a_glacier, jet, CBR_coldhot, Gradient() };
        ImGui::Combo("Gradent", &current_gradient, items, IM_ARRAYSIZE(items));
        // if new gradient is selected:
        static bool needGradientImg = true;
        if (current_gradient != previious_gradient)
        {
            // fix memory leak (??)
            if (!needGradientImg)
            {
                glDeleteTextures(1, &gradient_img);
                needGradientImg = true;
            }
            colors.gradient = gradients[current_gradient];
            gradient_img_data = colors.gradient.draw();
            glGenTextures(1, &gradient_img);
            glBindTexture(GL_TEXTURE_2D, gradient_img);

            // Setup filtering parameters for display
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

            // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 400, 30, 0, GL_RGB, GL_FLOAT, gradient_img_data.data());
            needGradientImg = false;
        }
        ImGui::Image((void*)(intptr_t)gradient_img, ImVec2(400, 30)); // , texturesize);
        static float cidx = 0.5;
        ImGui::DragFloat("Color Index", &cidx, 0.0002, 0.f, 1.f, "%.4f");
        static color test_color = colors.gradient.get_color(cidx);
        if (ImGui::Button("Get Color"))
        {
            test_color = colors.gradient.get_color(cidx);
            cout << "Test color received: " << test_color.r << " " << test_color.g << " " << test_color.b << "\n";
        }
        ImGui::SameLine();
        if (ImGui::Button("Print Gradient"))
        {
            colors.gradient.print_fine();
            colors.gradient.print();
        }
        ImGui::SameLine();
        if (ImGui::Button("Refill"))
        {
            colors.gradient.fill();
        }
        ImGui::End();


        if (showGradientEditor)
        {
            ImGui::Begin("Gradient Editor");
            ImGui::End();
        }
        ImGui::Begin("Rendering");
        // Quality Settings:
        static renderSettings rendering;
        static int imgSize[2] = { imgWidth, imgHeight };
        ImGui::Text("Image Resolution");
        ImGui::InputInt2("input int2", imgSize);
        rendering.imgWidth = imgSize[0];
        rendering.imgHeight = imgSize[1];
        rendering.aspect = (float)rendering.imgWidth / (float)rendering.imgHeight;
        ImGui::DragInt("Render Quality:", &quality, 0.025, 0, 12, "Render Quality: %d", ImGuiSliderFlags_AlwaysClamp);
        max_passes = (int)pow(2.0f, (float)quality);
        rendering.maxPasses = max_passes;
        ImGui::Text("Render with %d passes", max_passes);
        // Press Start to Play:
        if (running)
        {
            if (ImGui::Button("Stop"))
            {
                stop = true; // if this is encountered in loop, the thread calls exit(0)
                running = false;
            }
        }
        else
        {
            if (ImGui::Button("Calculate"))
            {
                int max_iter = (int)max_iter_float; // TODO: this is dumb, fix it
                stop = false;
                t = std::thread(calc_mb_onearg, pos, colors, rendering, &current_passes, &vec_img_f, &progress, &m);
                running = true;
            }
        }
        ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
        ImGui::InputText("File name", png_out_name, IM_ARRAYSIZE(png_out_name));
        if (ImGui::Button("Save to PNG"))
        {
            cout << "Saving to PNG\n";
            save_to_png(&vec_img_f, rendering.imgWidth, rendering.imgHeight, png_out_name);
        }

        if (current_passes > max_passes && running)
        {
            t.join();
            running = false;
            current_passes = 0;
            cout << "Joining t";
        }
        ImGui::End();


        // MAIN DISPLAY REFRESH
        t2 = high_resolution_clock::now();
        auto ms_int = duration_cast<milliseconds>(t2 - t1);
        int duration = ms_int.count();
        if (duration >= 50) // do this once every N ms only
        {
            t1 = high_resolution_clock::now();
            
            // has image size changed since last check? If so, update the vector
            if (rendering.imgWidth != imgWidthOld || rendering.imgHeight != imgHeightOld)
            {
                vec_img_f.resize(rendering.imgWidth * rendering.imgHeight * 3);
                std::fill(vec_img_f.begin(), vec_img_f.end(), 0);
                //vec_img_f.clear();
                imgWidthOld = rendering.imgWidth;
                imgHeightOld = rendering.imgHeight;
            }

            // TODO: memory leak is fixed, but find a way to re-use texture instead of recreating it all the time... (bind, fill, or smth?)
            // MAKE TEXTURE FROM IMAGE DATA
            // create texture to hold image data for rendering
            if (!need_texture)
            {
                glDeleteTextures(1, &texture);
                need_texture = true;
            }
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);

            // Setup filtering parameters for display
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

            // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            need_texture = false;
#endif
            m.lock(); // lock vec_img_f
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, rendering.imgWidth, rendering.imgHeight, 0, GL_RGB, GL_FLOAT, vec_img_f.data());
            m.unlock();
            need_texture = false;
            GLenum err;
            while((err = glGetError()) != GL_NO_ERROR){
            	std::cout << err;
            }
        }

        // MAIN DISPLAY
        sprintf(mainDisplayString, "Main Display (%.2f%%)###Main Display", 100.f * zoom_factor);
        ImGui::Begin(mainDisplayString, nullptr, ImGuiWindowFlags_HorizontalScrollbar);
        ImVec2 ws = ImGui::GetWindowSize();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 coursorPos = ImGui::GetCursorScreenPos();
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

        // some fun ImGui example code:
        if (ImGui::IsItemHovered())
        {
            // cout << imgContentMin.x << " " << imgContentMax.x << " " << imgContentMin.y << " " << imgContentMax.y << "\n";
            if (io.KeyCtrl && io.KeyShift)
            {
                ImGui::BeginTooltip();
                float region_sz = 32.0f;
                float region_x = io.MousePos.x - coursorPos.x - region_sz * 0.5f;
                float region_y = io.MousePos.y - coursorPos.y - region_sz * 0.5f;
                float zoom = 4.0f;
                if (region_x < 0.0f) { region_x = 0.0f; }
                else if (region_x > imgWidth - region_sz) { region_x = imgWidth - region_sz; }
                if (region_y < 0.0f) { region_y = 0.0f; }
                else if (region_y > imgHeight - region_sz) { region_y = imgHeight - region_sz; }
                ImGui::Text("Coursor pos on image is %.2f, %.2f", io.MousePos.x - coursorPos.x, io.MousePos.y - coursorPos.y);
                ImGui::Text("Max scroll is: %.2f, %.2f", imgScrollMax.x, imgScrollMax.y);
                ImGui::Text("Coursor pos on window is %.2f, %.2f", ws.x + io.MousePos.x - coursorPos.y, ws.x + io.MousePos.y - coursorPos.y);
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
                float coursorInImgX = io.MousePos.x - coursorPos.x + WeirdFac * WeirdX;
                float coursorInImgY = io.MousePos.y - coursorPos.y + WeirdFac * WeirdY;
                float coursorInImgWindowX = io.MousePos.x - coursorPos.x - ImGui::GetScrollX();
                float coursorInImgWindowY = io.MousePos.y - coursorPos.y - ImGui::GetScrollY();
                float zoom_ratio = zoom_factor / prev_zoom_fac;
                newXScroll = (int)(coursorInImgX * zoom_ratio - (coursorInImgX - ImGui::GetScrollX()) + WeirdFac * WeirdX);
                newYScroll = (int)(coursorInImgY * zoom_ratio - (coursorInImgY - ImGui::GetScrollY()) + WeirdFac * WeirdY);
                cout << "Zoom factor new and old:     " << zoom_factor << " " << prev_zoom_fac << "\n";
                cout << "Coursor position on image:   " << io.MousePos.x - coursorPos.x << " " << io.MousePos.y - coursorPos.y << "\n";
                cout << "Coursor position in window:  " << coursorInImgWindowX << " " << coursorInImgWindowY << "\n";
                cout << "New coursor position on img: " << (io.MousePos.x - coursorPos.x) * zoom_factor / prev_zoom_fac << " " << (io.MousePos.y - coursorPos.y) * zoom_factor / prev_zoom_fac << "\n";
                cout << "Old scroll values X and Y:   " << ImGui::GetScrollX() << " " << ImGui::GetScrollY() << "\n";
                cout << "New scroll values X and Y:   " << newXScroll << " " << newYScroll << "\n";
                cout << "Scroll values are now:       " << ImGui::GetScrollX() << " " << ImGui::GetScrollY() << "\n";
                image_was_zoomed = true;
            }
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                // move image by dragging if it is bigger than the window
                ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseDelta.x); //(int)mouse_drag.x);
                ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y); //(int)mouse_drag.y);
            }
            prev_zoom_fac = zoom_factor;
        }
        
        ImGui::End(); // Main Display
        ImGui::End(); // Dockspace
        ImGui::ShowDemoWindow();
	}
}