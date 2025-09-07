#include <gtest/gtest.h>

#include "mandelbrot_renderer.hpp"

// Тестовый класс для проверки рендеринга
class MandelbrotRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Инициализация тестового окружения
        settings_ = RenderSettings{.width = 800, .height = 600, .max_iterations = 100, .escape_radius = 2.0};

        viewport_ = mandelbrot::ViewPort{.x_min = -2.5, .x_max = 1.5, .y_min = -2.0, .y_max = 2.0};

        renderer_ = std::make_shared<MandelbrotRenderer>(2);  // Создаем рендерер с 2 потоками
    }

    RenderSettings settings_;
    mandelbrot::ViewPort viewport_;
    std::shared_ptr<MandelbrotRenderer> renderer_;
};

TEST_F(MandelbrotRendererTest, BasicRendering) {
    // Запускаем рендеринг
    auto result = renderer_->RenderAsync<2>(viewport_, settings_);
    RenderResult render_result;
    auto result_sender = result | stdexec::then([&](RenderResult &&rr) { render_result = std::move(rr); });
    stdexec::sync_wait(result_sender);

    // Проверяем размеры результата
    EXPECT_EQ(render_result.pixel_data.size(), settings_.height);
    EXPECT_EQ(render_result.color_data.size(), settings_.height);

    for (const auto &row : render_result.pixel_data) {
        EXPECT_EQ(row.size(), settings_.width);
    }

    for (const auto &row : render_result.color_data) {
        EXPECT_EQ(row.size(), settings_.width);
    }

    // Проверяем время рендеринга
    EXPECT_LE(render_result.render_time.count(), 5000);  // Меньше 5 секунд
}

TEST_F(MandelbrotRendererTest, CorrectViewport) {
    auto result = renderer_->RenderAsync<2>(viewport_, settings_);
    RenderResult render_result;
    auto result_sender = result | stdexec::then([&](RenderResult &&rr) { render_result = std::move(rr); });
    stdexec::sync_wait(result_sender);

    // Проверяем, что viewport сохранился корректно
    EXPECT_EQ(render_result.viewport.x_min, viewport_.x_min);
    EXPECT_EQ(render_result.viewport.x_max, viewport_.x_max);
    EXPECT_EQ(render_result.viewport.y_min, viewport_.y_min);
    EXPECT_EQ(render_result.viewport.y_max, viewport_.y_max);
}

TEST_F(MandelbrotRendererTest, CorrectSettings) {
    auto result = renderer_->RenderAsync<2>(viewport_, settings_);
    RenderResult render_result;
    auto result_sender = result | stdexec::then([&](RenderResult &&rr) { render_result = std::move(rr); });
    stdexec::sync_wait(result_sender);

    // Проверяем, что настройки сохранились корректно
    EXPECT_EQ(render_result.settings.width, settings_.width);
    EXPECT_EQ(render_result.settings.height, settings_.height);
    EXPECT_EQ(render_result.settings.max_iterations, settings_.max_iterations);
    EXPECT_EQ(render_result.settings.escape_radius, settings_.escape_radius);
}