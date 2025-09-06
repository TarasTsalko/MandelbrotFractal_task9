#pragma once

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "mandelbrot_sender.hpp"
#include "types.hpp"

#include <any>
#include <array>

class MandelbrotRenderer {
private:
    exec::static_thread_pool thread_pool_;

private:
    template <typename... Senders>
    auto CombineSenders(Senders &&...senders) {
        return stdexec::when_all((std::move(senders))...);
    }

public:
    explicit MandelbrotRenderer(std::uint32_t num_threads = std::thread::hardware_concurrency())
        : thread_pool_{num_threads} {}

    template <size_t N>
    [[nodiscard]] auto RenderAsync(mandelbrot::ViewPort viewport, RenderSettings settings) {
        assert(N > 0 && "Количество задач должно быть больше нуля");
        assert(settings.height % N == 0 && "Высота должна делиться на N без остатка");

        auto start_time = std::chrono::steady_clock::now();

        // 1. Разделение экрана на N полос
        std::array<PixelRegion, N> regions;
        // Вычисляем базовую высоту полосы
        std::uint32_t base_height = settings.height / N;
        std::uint32_t remainder = settings.height % N;  // Остаток для распределения

        std::uint32_t current_start_row = 0;

        for (size_t i = 0; i < N; ++i) {
            // Распределяем остаток между первыми полосами
            std::uint32_t row_height = base_height + (i < remainder ? 1 : 0);

            // Формируем регион
            regions[i] = PixelRegion{current_start_row, current_start_row + row_height, 0, settings.width};

            // Обновляем начальную строку для следующей полосы
            current_start_row += row_height;
        }

        using BoundSender =
            decltype(stdexec::starts_on(thread_pool_.get_scheduler(), std::declval<MandelbrotSender<void>>()));

        // Функция преобразования матрицы итераций в матрицу цветов
        auto convert_to_colors = [settings](const PixelMatrix &matrix) -> ColorMatrix {
            ColorMatrix color_matrix(matrix.size());
            for (size_t row = 0; row < matrix.size(); ++row) {
                color_matrix[row].resize(matrix[row].size());
                for (size_t col = 0; col < matrix[row].size(); ++col) {
                    // Преобразуем итерации в цвет
                    color_matrix[row][col] = mandelbrot::IterationsToColor(matrix[row][col], settings.max_iterations);
                }
            }
            return color_matrix;
        };

        std::vector<BoundSender> senders;
        senders.reserve(N);

        // Создаем кортеж из элементов вектора
        auto tuple_of_senders = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::tuple{stdexec::starts_on(thread_pool_.get_scheduler(),
                                                 MakeMandelbrotSender(viewport, settings, regions[Is]))...};
        }(std::make_index_sequence<N>());

        // Объединяем sender'ы через apply
        auto combined_sender =
            std::apply([](auto &&...s) { return stdexec::when_all(std::forward<decltype(s)>(s)...); },
                       std::move(tuple_of_senders));

        return combined_sender | stdexec::then([start_time, viewport, settings](auto &&...results) {
                   // Распаковываем результаты
                   PixelMatrix all_pixels(settings.height);
                   size_t row_idx = 0;
                   auto add_matrix = [&](const PixelMatrix &matrix) {
                       for (const auto &row : matrix) {
                           all_pixels[row_idx].resize(settings.width);
                           for (size_t col = 0; col < settings.width; ++col) {
                               all_pixels[row_idx][col] = row[col];
                           }
                           row_idx++;
                       }
                   };

                   // Распаковываем все матрицы через fold expression
                   (add_matrix(std::forward<decltype(results)>(results)), ...);

                   ColorMatrix all_colors(settings.height);
                   for (size_t iRow = 0; iRow < all_pixels.size(); iRow++) {
                       const auto &row = all_pixels[iRow];
                       all_colors[iRow].resize(row.size());
                       for (size_t iCol = 0; iCol < row.size(); ++iCol) {
                           const size_t iteration = row[iCol];
                           all_colors[iRow][iCol] = mandelbrot::IterationsToColor(iteration, settings.max_iterations);
                       }
                   }

                   return RenderResult{std::move(all_pixels), std::move(all_colors), viewport, settings,
                                       std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - start_time)};
               });
    }
};
