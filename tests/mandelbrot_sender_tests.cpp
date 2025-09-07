#include <gtest/gtest.h>
#include <limits>

#include "mandelbrot_sender.hpp"

class TestReceiver {
public:
    using value_type = PixelMatrix;
    using receiver_concept = stdexec::receiver_t;

    void set_value(const PixelMatrix &value) noexcept {
        result_ = value;
        value_called_ = true;
    }

    template <class Error>
    void set_error(Error &&error) noexcept {
        error_ = std::forward<Error>(error);
        error_called_ = true;
    }

    bool value_called() const noexcept { return value_called_; }
    bool error_called() const noexcept { return error_called_; }

    const std::exception_ptr &get_error() const { return error_; }
    const PixelMatrix &get_result() const { return result_; }

private:
    PixelMatrix result_;
    bool value_called_ = false;
    bool error_called_ = false;
    std::exception_ptr error_;
};

const RenderSettings kDefaultSettings = {.width = 800, .height = 600, .max_iterations = 100, .escape_radius = 2.0};

const PixelRegion kDefaultRegion = {.start_row = 0, .end_row = 10, .start_col = 0, .end_col = 10};

const mandelbrot::ViewPort kDefaultViewPort = {.x_min = -1.0, .x_max = 3.0, .y_min = -1.5, .y_max = 1.5};

// Тест базового функционала
TEST(MandelbrotSenderTest, BasicOperation) {
    const auto sender = MakeMandelbrotSender(kDefaultViewPort, kDefaultSettings, kDefaultRegion);

    TestReceiver receiver;
    auto op_state = sender.connect(receiver);
    op_state.start();

    // Проверяем, что значение было получено
    EXPECT_TRUE(receiver.value_called());

    // Проверяем размер матрицы
    const auto &result = receiver.get_result();
    EXPECT_EQ(result.size(), kDefaultRegion.end_row - kDefaultRegion.start_row);
    for (const auto &row : result) {
        EXPECT_EQ(row.size(), kDefaultRegion.end_col - kDefaultRegion.start_col);
    }
}

// Тест граничных значений
TEST(MandelbrotSenderTest, BoundaryValues) {
    // Создаем регион с одной точкой
    PixelRegion single_pixel_region = {.start_row = 0, .end_row = 1, .start_col = 0, .end_col = 1};

    MandelbrotSender single_pixel_sender =
        MakeMandelbrotSender(kDefaultViewPort, kDefaultSettings, single_pixel_region);

    TestReceiver receiver;
    auto op_state = single_pixel_sender.connect(receiver);
    op_state.start();

    const auto &result = receiver.get_result();
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].size(), 1);
}

TEST(MandelbrotSenderTest, CustomSettings) {
    const RenderSettings custom_settings = {.width = 400, .height = 300, .max_iterations = 200, .escape_radius = 2.5};

    const MandelbrotSender sender = MakeMandelbrotSender(kDefaultViewPort, custom_settings, kDefaultRegion);

    TestReceiver receiver;
    auto op_state = sender.connect(receiver);
    op_state.start();

    const auto &result = receiver.get_result();
    EXPECT_EQ(result.size(), kDefaultRegion.end_row - kDefaultRegion.start_row);

    for (const auto &row : result) {
        EXPECT_EQ(row.size(), kDefaultRegion.end_col - kDefaultRegion.start_col);

        // Проверяем, что все значения находятся в допустимом диапазоне
        for (const auto &value : row) {
            EXPECT_LE(value, custom_settings.max_iterations) << "Iteration value exceeds the maximum allowed limit";
            EXPECT_NE(value, std::numeric_limits<std::uint32_t>::max())
                << "Iteration value should not be equal to maximum uint32_t value";
        }
    }
}

// Тест для проверки обработки ошибок
TEST(MandelbrotSenderTest, EmptyRegion) {
    PixelRegion empty_region = {
        .start_row = 5,
        .end_row = 5,  // Пустой диапазон
        .start_col = 5,
        .end_col = 5  // Пустой диапазон
    };

    MandelbrotSender empty_sender = MakeMandelbrotSender(kDefaultViewPort, kDefaultSettings, empty_region);

    TestReceiver receiver;
    auto op_state = empty_sender.connect(receiver);
    op_state.start();

    // Проверяем, что ошибка была получена
    EXPECT_FALSE(receiver.value_called());
    EXPECT_TRUE(receiver.error_called());

    try {
        std::rethrow_exception(receiver.get_error());
    } catch (const std::invalid_argument &e) {
        EXPECT_STREQ("Empty or invalid pixel region specified", e.what());
    }
}