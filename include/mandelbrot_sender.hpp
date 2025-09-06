#pragma once

#include <stdexec/execution.hpp>

#include "types.hpp"
#include <iostream>

template <typename Receiver>
struct MandelbrotOperationState {

    Receiver receiver_;
    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    PixelRegion region_;

    void start() noexcept { (*this)(); }

    void operator()() noexcept {
        using namespace mandelbrot;
        // Создаем матрицу для хранения результатов итераций
        PixelMatrix pixel_data(region_.end_row - region_.start_row,
                               std::vector<std::uint32_t>(region_.end_col - region_.start_col));

        for (std::uint32_t y = region_.start_row; y < region_.end_row; ++y) {
            for (std::uint32_t x = region_.start_col; x < region_.end_col; ++x) {
                // Преобразуем координаты пикселя в комплексное число
                Complex c = Pixel2DToComplex(x, y, viewport_, settings_.width, settings_.height);

                // Вычисляем количество итераций
                std::uint32_t iterations =
                    CalculateIterationsForPoint(c, settings_.max_iterations, settings_.escape_radius);

                // Сохраняем результат
                pixel_data[y - region_.start_row][x - region_.start_col] = iterations;
            }
        }
        // Отправляем результат ресиверу
        stdexec::set_value(receiver_, std::move(pixel_data));
    }
};

template <typename Receiver>
struct MandelbrotSender {
    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    PixelRegion region_;

    using sender_concept = stdexec::sender_t;

    MandelbrotSender(mandelbrot::ViewPort viewport, RenderSettings settings, PixelRegion region)
        : viewport_(viewport), settings_(settings), region_(region) {}

    template <typename R>
    auto connect(R &&receiver) const noexcept {
        return MandelbrotOperationState<R>{std::forward<R>(receiver), viewport_, settings_, region_};
    }

    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(PixelMatrix),
                                              stdexec::set_error_t(std::exception_ptr),
                                              stdexec::set_error_t(std::error_code)>{};
    }
};

[[nodiscard]] inline auto MakeMandelbrotSender(mandelbrot::ViewPort viewport, RenderSettings settings,
                                               PixelRegion region) {
    return MandelbrotSender<void>{viewport, settings, region};
}
