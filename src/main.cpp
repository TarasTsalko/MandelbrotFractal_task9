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

#include <SFML/Window.hpp>

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
    explicit WaitForFPS(FrameClock &clock, size_t fps) : clock_(clock), fps_(fps) {}

    void operator()() const {
        const auto frame_time = clock_.GetFrameTime();

        // Рассчитываем максимально допустимую длительность кадра
        // 1000 мс делим на желаемую частоту кадров (fps_)
        // Например, при fps_ = 60 получаем 16.67 мс на кадр
        const auto max_frame_duration = std::chrono::milliseconds(1000 / fps_);
        // Преобразуем время кадра в миллисекунды для удобства вычислений
        const auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(frame_time);
        // Вычисляем время, которое нужно подождать до следующего кадра
        // wait_time = максимально допустимая длительность - затраченное время
        const auto wait_time = max_frame_duration - elapsed_time;

        // Если время ожидания положительное - ждем
        if (wait_time > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(wait_time);
        }

        // Обновляем счетчик времени кадра
        clock_.Reset();
    }

private:
    FrameClock &clock_;  // Ссылка на объект FrameClock
    size_t fps_;         // Желаемая частота кадров
};

std::string get_thread_info() {
    std::ostringstream oss;
    oss << "Thread ID: " << std::this_thread::get_id()
        << ", Hash: " << std::hash<std::thread::id>()(std::this_thread::get_id());
    return oss.str();
}

class MandelbrotApp {
private:
    RenderSettings render_settings_{.width = 800, .height = 600, .max_iterations = 100, .escape_radius = 2.0};
    sf::ContextSettings settings;

    sf::RenderWindow window_;
    sf::Image image_;
    sf::Texture texture_;
    sf::Sprite sprite_;
    MandelbrotRenderer renderer_;
    AppState state_;

public:
    MandelbrotApp()
        : window_{sf::VideoMode{render_settings_.width, render_settings_.height}, "Mandelbrot Fractal",
                  sf::Style::Default, settings},
          renderer_{THREAD_POOL_SIZE} {

        image_.create(render_settings_.width, render_settings_.height);
        texture_.create(render_settings_.width, render_settings_.height);

        window_.setKeyRepeatEnabled(false);
        window_.setVerticalSyncEnabled(false);
        window_.clear(sf::Color(255, 255, 255));
    }

    void Run() {

        FrameClock frame_clock;
        sf::Clock zoom_clock;

        std::cout << get_thread_info() << std::endl;
        auto pipeline = SfmlEventHandler{window_, render_settings_, state_, zoom_clock} |  //
                        stdexec::let_value([this]() {
                            std::cout << get_thread_info() << std::endl;
                            return CalculateMandelbrotAsyncSender{state_, render_settings_, renderer_};
                        }) |
                        stdexec::let_value([this](RenderResult data) {
                            std::cout << get_thread_info() << std::endl;
                            return SFMLRender{std::move(data), image_, texture_, sprite_, window_, render_settings_};
                        }) |
                        stdexec::then([&]() {
                            std::cout << get_thread_info() << std::endl;
                            WaitForFPS{frame_clock, 60}();
                        });

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