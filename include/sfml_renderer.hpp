#pragma once

#include <SFML/Graphics.hpp>
#include <print>
#include <stdexec/execution.hpp>

#include "types.hpp"

class SFMLRender {
public:
    using sender_concept = stdexec::sender_t;

    RenderResult render_result_;
    sf::Image &image_;
    sf::Texture &texture_;
    sf::Sprite &sprite_;
    sf::RenderWindow &window_;
    RenderSettings render_settings_;

    SFMLRender(RenderResult render_result, sf::Image &image, sf::Texture &texture, sf::Sprite &sprite,
               sf::RenderWindow &window, RenderSettings render_settings)
        : render_result_(render_result), image_(image), texture_(texture), sprite_(sprite), window_(window),
          render_settings_(render_settings) {}

    // Метод connect
    template <typename Receiver>
    auto connect(Receiver &&rx) const noexcept {
        return OperationState<Receiver>{
            std::forward<Receiver>(rx), render_result_, image_, texture_, sprite_, window_, render_settings_};
    }

    // Метод получения сигнатур завершения
    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_error_t(std::exception_ptr),
                                              stdexec::set_error_t(std::error_code)>{};
    }

private:
    template <typename Receiver>
    struct OperationState {
        Receiver receiver_;
        RenderResult render_result_;
        sf::Image &image_;
        sf::Texture &texture_;
        sf::Sprite &sprite_;
        sf::RenderWindow &window_;
        RenderSettings render_settings_;

        OperationState(Receiver &&receiver, RenderResult render_result, sf::Image &image, sf::Texture &texture,
                       sf::Sprite &sprite, sf::RenderWindow &window, RenderSettings render_settings)
            : receiver_(std::forward<Receiver>(receiver)), render_result_(render_result), image_(image),
              texture_(texture), sprite_(sprite), window_(window), render_settings_(render_settings) {}

        void start() noexcept { (*this)(); }
        void operator()() noexcept {
            try {
                if (!render_result_.color_data.empty()) {
                    image_.create(render_settings_.width, render_settings_.height);
                    texture_.create(render_settings_.width, render_settings_.height);

                    for (std::size_t y = 0; y < render_settings_.height; ++y) {
                        for (std::size_t x = 0; x < render_settings_.width; ++x) {
                            const auto &color = render_result_.color_data[y][x];
                            image_.setPixel(x, y, sf::Color(color.r, color.g, color.b, 255));
                        }
                    }

                    window_.clear();
                    texture_.loadFromImage(image_);
                    sprite_.setTexture(texture_);
                    window_.draw(sprite_);
                    window_.display();
                }

                stdexec::set_value(receiver_);
            } catch (...) {
                stdexec::set_error(receiver_, std::current_exception());
            }
        }
    };
};