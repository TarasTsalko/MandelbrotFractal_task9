#include <gtest/gtest.h>

#include "mandelbrot_fractal_utils.hpp"
#include "sfml_renderer.hpp"

// Функция для выполнения операции
class TestReceiver {

public:
    using receiver_concept = stdexec::receiver_t;

    void set_value() noexcept { executed = true; }

    void set_error(std::exception_ptr) noexcept { error_occurred = true; }

    bool get_value() const noexcept { return executed; }

    bool get_error() const noexcept { return error_occurred; }

private:
    bool executed = false;
    bool error_occurred = false;
};

TEST(SFMLRenderTest, RenderEmptyData) {
    sf::Image image;
    sf::Texture texture;
    sf::Sprite sprite;
    sf::RenderWindow window(sf::VideoMode(800, 600), "Test Window");
    RenderResult render_result;
    RenderSettings render_settings;

    // Пустые данные
    render_result.color_data.clear();

    SFMLRender renderer(render_result, image, texture, sprite, window, render_settings);
    TestReceiver receiver{};
    auto op = renderer.connect(receiver);
    op.start();

    // Проверяем, что ничего не произошло
    EXPECT_EQ(image.getSize().x, 0);
    EXPECT_EQ(image.getSize().y, 0);
}

TEST(SFMLRenderTest, RenderValidData) {
    sf::Image image;
    sf::Texture texture;
    sf::Sprite sprite;
    sf::RenderWindow window(sf::VideoMode(800, 600), "Test Window");
    RenderResult render_result;
    RenderSettings render_settings;

    // Заполняем тестовыми данными
    render_result.color_data.resize(render_settings.height);
    for (auto &row : render_result.color_data) {
        row.resize(render_settings.width, mandelbrot::RgbColor{255, 255, 255});
    }

    SFMLRender renderer(render_result, image, texture, sprite, window, render_settings);
    TestReceiver receiver{};
    auto op = renderer.connect(receiver);
    op.start();

    // Проверяем размеры
    EXPECT_EQ(image.getSize().x, 800);
    EXPECT_EQ(image.getSize().y, 600);

    // Проверяем цвета
    for (int y = 0; y < 600; ++y) {
        for (int x = 0; x < 800; ++x) {
            const sf::Color pixel = image.getPixel(x, y);
            EXPECT_EQ(pixel.r, 255);
            EXPECT_EQ(pixel.g, 255);
            EXPECT_EQ(pixel.b, 255);
        }
    }
}

TEST(SFMLRenderTest, RenderDifferentColors) {

    using namespace mandelbrot;
    sf::Image image;
    sf::Texture texture;
    sf::Sprite sprite;
    sf::RenderWindow window(sf::VideoMode(800, 600), "Test Window");
    RenderResult render_result;
    RenderSettings render_settings;

    // Заполняем тестовыми данными с разными цветами
    render_result.color_data.resize(render_settings.height);
    for (auto &row : render_result.color_data) {
        row.resize(render_settings.width);
        for (size_t x = 0; x < row.size(); ++x) {
            // Создаем градиент слева направо
            const uint8_t red = static_cast<int>(255.0 * x / render_settings.width);
            const uint8_t green = static_cast<int>(255.0 * (render_settings.width - x) / render_settings.width);
            const uint8_t blue = static_cast<int>(255.0 * (x % 128) / 128);
            row[x] = RgbColor{red, green, blue};
        }
    }

    SFMLRender renderer(render_result, image, texture, sprite, window, render_settings);

    // Создаем приемник для выполнения операции
    TestReceiver receiver;
    auto op = renderer.connect(receiver);
    op.start();

    // Проверяем размеры
    EXPECT_EQ(image.getSize().x, 800);
    EXPECT_EQ(image.getSize().y, 600);
    EXPECT_TRUE(receiver.get_value());

    // Проверяем градиент слева направо
    for (int y = 0; y < 600; ++y) {
        for (int x = 0; x < 800; ++x) {
            const uint8_t red = static_cast<int>(255.0 * x / 800);
            const uint8_t green = static_cast<int>(255.0 * (800 - x) / 800);
            const uint8_t blue = static_cast<int>(255.0 * (x % 128) / 128);
            sf::Color expected_color(red, green, blue, 255);
            EXPECT_EQ(image.getPixel(x, y), expected_color);
        }
    }
}
