#include <chrono>
#include <print>
#include <thread>
#include <utility>

#include <SFML/Graphics.hpp>
#include <exec/repeat_effect_until.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "mandelbrot.hpp"
#include "mandelbrot_renderer.hpp"
#include "sfml_events_handler.hpp"
#include "sfml_renderer.hpp"

#include "mandelbrot_fractal_utils.hpp"

#include <chrono>
#include <iostream>

using namespace std::chrono_literals;
class FrameClock {
public:
    FrameClock() { Reset(); }

    void Reset() noexcept { frame_start_ = std::chrono::steady_clock::now(); }
    auto GetFrameTime() const noexcept { return std::chrono::steady_clock::now() - frame_start_; }

private:
    std::chrono::time_point<std::chrono::steady_clock> frame_start_;
};

class WaitForFPS {
public:
};

int Test() { return 0; }

class MandelbrotApp {
private:
    RenderSettings render_settings_{.width = 800, .height = 600, .max_iterations = 100, .escape_radius = 2.0};

    sf::RenderWindow window_;
    sf::Image image_;
    sf::Texture texture_;
    sf::Sprite sprite_;
    MandelbrotRenderer renderer_;
    AppState state_;

public:
    MandelbrotApp()
        : window_{sf::VideoMode{render_settings_.width, render_settings_.height}, "Mandelbrot Fractal"},
          renderer_{THREAD_POOL_SIZE} {

        image_.create(render_settings_.width, render_settings_.height);
        texture_.create(render_settings_.width, render_settings_.height);

        window_.setKeyRepeatEnabled(false);
        window_.clear(sf::Color(255, 255, 255));
    }

    void Run() {
        FrameClock frame_clock;
        sf::Clock zoom_clock;

        auto pipeline = SfmlEventHandler{window_, render_settings_, state_, zoom_clock} |  //
                        stdexec::let_value(
                            [this]() { return CalculateMandelbrotAsyncSender{state_, render_settings_, renderer_}; }) |
                        stdexec::let_value([&](RenderResult data) {
                            return SFMLRender{std::move(data), image_, texture_, sprite_, window_, render_settings_};
                        }) |
                        stdexec::then([this]() { return; });  //
                                                              // stdexec::then(WaitForFPS{frame_clock, 60});

        auto repeated_pipeline =
            std::move(pipeline) | stdexec::then([this]() { return state_.should_exit; }) | exec::repeat_effect_until();

        // Основной поток обрабатывает события
        stdexec::sync_wait(repeated_pipeline);
    }
};

int main() {
    try {

        MandelbrotApp app;
        app.Run();
    } catch (const std::exception &e) {
        std::println("Error: {}", e.what());
        return 1;
    }
    return 0;
}