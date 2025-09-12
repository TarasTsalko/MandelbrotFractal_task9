#pragma once

#include <SFML/Graphics.hpp>
#include <stdexec/execution.hpp>

#include "types.hpp"

class SfmlEventHandler {
public:
    using sender_concept = stdexec::sender_t;

    template <typename Receiver>
    struct OperationState {
        Receiver receiver_;
        sf::RenderWindow &window_;
        RenderSettings render_settings_;
        AppState &state_;
        sf::Clock &zoom_clock_;

        static constexpr float ZOOM_INTERVAL_MS = 100.0f;

        template <typename R>
        explicit OperationState(R &&r, sf::RenderWindow &window, RenderSettings render_settings, AppState &state,
                                sf::Clock &zoom_clock)
            : receiver_{std::forward<R>(r)}, window_{window}, render_settings_{render_settings}, state_{state},
              zoom_clock_{zoom_clock} {}

        void start() noexcept {
            HandleEvents();
            // stdexec::set_value(receiver_);
            if (state_.should_exit || state_.need_rerender) {
                // Если нужно выйти, завершаем sender
                stdexec::set_value(receiver_);
            }
        }

    private:
        void HandleEvents() {
            sf::Event event;
            while (window_.isOpen()) {
                window_.pollEvent(event);
                switch (event.type) {
                case sf::Event::Closed: {
                    // Обработка закрытия окна
                    state_.should_exit = true;
                    break;
                }
                case sf::Event::KeyPressed: {
                    if (event.key.code == sf::Keyboard::Escape) {
                        state_.should_exit = true;  // Дополнительный способ выхода
                    }
                    break;
                }
                case sf::Event::Resized: {
                    if (event.size.width != render_settings_.width || event.size.height != render_settings_.height) {
                        state_.need_rerender = true;
                        render_settings_.height = event.size.height;
                        render_settings_.width = event.size.width;
                        break;
                    }
                }
                case sf::Event::MouseWheelMoved: {
                    HandleContinuousZoom();
                    state_.need_rerender = true;
                    break;
                }
                case sf::Event::MouseButtonPressed: {
                    if (event.mouseButton.button == sf::Mouse::Left)
                        state_.left_mouse_pressed = true;
                    if (event.mouseButton.button == sf::Mouse::Right)
                        state_.right_mouse_pressed = true;
                    break;
                }
                case sf::Event::MouseButtonReleased: {
                    if (event.mouseButton.button == sf::Mouse::Left)
                        state_.left_mouse_pressed = false;
                    if (event.mouseButton.button == sf::Mouse::Right)
                        state_.right_mouse_pressed = false;
                    break;
                }

                default:
                    break;
                }

                if (state_.should_exit || state_.need_rerender) {
                    break;
                }
            }
        }

        void HandleContinuousZoom() {
            if ((state_.left_mouse_pressed || state_.right_mouse_pressed) &&
                zoom_clock_.getElapsedTime().asMilliseconds() >= ZOOM_INTERVAL_MS) {

                sf::Vector2i mouse_pos = sf::Mouse::getPosition(window_);

                if (mouse_pos.x >= 0 && mouse_pos.x < static_cast<int>(render_settings_.width) && mouse_pos.y >= 0 &&
                    mouse_pos.y < static_cast<int>(render_settings_.height)) {

                    ZoomToPoint(mouse_pos.x, mouse_pos.y, state_.left_mouse_pressed);
                    zoom_clock_.restart();
                }
            }
        }

        void ZoomToPoint(int pixel_x, int pixel_y, bool zoom_in, double factor = 1.8) {

            // Константы для ограничения масштаба
            const double MIN_ZOOM_FACTOR = 0.001;  // Минимальный допустимый масштаб
            const double MAX_ZOOM_FACTOR = 1000.0; // Максимальный допустимый масштаб

            // Вычисляем координаты целевой точки в комплексных числах
            const double target_x = state_.viewport.x_min +
                                (static_cast<double>(pixel_x) / render_settings_.width) * state_.viewport.width();
            const double target_y = state_.viewport.y_min +
                                (static_cast<double>(pixel_y) / render_settings_.height) * state_.viewport.height();

            // Определяем коэффициент масштабирования
            const double zoom_factor = zoom_in ? factor : (1.0 / factor);

            // Получаем текущий масштаб
            const double current_zoom = state_.viewport.width() / render_settings_.width;

            // Вычисляем новый масштаб с учетом ограничений
            double new_zoom = current_zoom * zoom_factor;
            new_zoom = std::max(MIN_ZOOM_FACTOR, std::min(MAX_ZOOM_FACTOR, new_zoom));

            // Вычисляем новые размеры области просмотра
            const double new_width = state_.viewport.width() * zoom_factor;
            const double new_height = state_.viewport.height() * zoom_factor;

            // Обновляем границы viewport, сохраняя целевую точку в центре
            state_.viewport.x_min = target_x - new_width / 2.0;
            state_.viewport.y_min = target_y - new_height / 2.0;
            state_.viewport.x_max = target_x + new_width / 2.0;
            state_.viewport.y_max = target_y + new_height / 2.0;

            const double MANDELBROT_X_MIN_BOUND = -2.5;
            const double MANDELBROT_X_MAX_BOUND = 1.5;
            const double MANDELBROT_Y_MIN_BOUND = -2.0;
            const double MANDELBROT_Y_MAX_BOUND = 2.0;

            // Дополнительные проверки границ, чтобы не выйти за пределы множества Мандельброта
            state_.viewport.x_min = std::max(state_.viewport.x_min, MANDELBROT_X_MIN_BOUND);
            state_.viewport.x_max = std::min(state_.viewport.x_max, MANDELBROT_X_MAX_BOUND);
            state_.viewport.y_min = std::max(state_.viewport.y_min, MANDELBROT_Y_MIN_BOUND);
            state_.viewport.y_max = std::min(state_.viewport.y_max, MANDELBROT_Y_MAX_BOUND);
        }
    };

    SfmlEventHandler(sf::RenderWindow &window, RenderSettings render_settings, AppState &state, sf::Clock &zoom_clock)
        : window_{window}, render_settings_{render_settings}, state_{state}, zoom_clock_{zoom_clock} {}

    /* Ваш код здесь  */

    template <typename Receiver>
    auto connect(Receiver &&rx) const noexcept {
        return OperationState<Receiver>(std::forward<Receiver>(rx), window_, render_settings_, state_, zoom_clock_);
    }

    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_error_t(std::exception_ptr),
                                              stdexec::set_error_t(std::error_code)>{};
    }

private:
    sf::RenderWindow &window_;
    RenderSettings render_settings_;
    AppState &state_;
    sf::Clock &zoom_clock_;
};
