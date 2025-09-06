#pragma once

#include "mandelbrot_renderer.hpp"

#include <iostream>

template <typename Receiver>
class CalculateMandelbrotOpState {
public:
    explicit CalculateMandelbrotOpState(Receiver &&rx, AppState &state, RenderSettings render_settings,
                                        MandelbrotRenderer &renderer)
        : receiver_(std::move(rx)), state_(state), render_settings_(render_settings), renderer_(renderer) {}

    void operator()() noexcept {
        try {
            exec();
        } catch (...) {
            stdexec::set_error(std::move(receiver_), std::current_exception());
        }
    }

    void start() noexcept { (*this)(); }

private:
    void exec() {
        // Проверяем необходимость рендеринга
        if (!state_.need_rerender) {
            // Если рендеринг не нужен, возвращаем пустой результат
            stdexec::set_value(std::move(receiver_), RenderResult{});
            return;
        }

        // Запускаем асинхронное вычисление
        auto result = renderer_.RenderAsync<THREAD_POOL_SIZE>(state_.viewport, render_settings_);

        auto result_sender = result | stdexec::then([this](RenderResult &&render_result) {
                                 stdexec::set_value(receiver_, std::forward<RenderResult>(render_result));
                             });

        stdexec::sync_wait(result_sender);
    }

    Receiver receiver_;
    AppState &state_;
    RenderSettings render_settings_;
    MandelbrotRenderer &renderer_;
};

class CalculateMandelbrotAsyncSender {
public:
    using sender_concept = stdexec::sender_t;

    explicit CalculateMandelbrotAsyncSender(AppState &state, RenderSettings render_settings,
                                            MandelbrotRenderer &renderer)
        : state_(state), render_settings_(render_settings), renderer_(renderer) {}

    template <typename Receiver>
    auto connect(Receiver &&rx) const noexcept {
        return CalculateMandelbrotOpState<Receiver>(std::forward<Receiver>(rx), state_, render_settings_, renderer_);
    }

    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(RenderResult),
                                              stdexec::set_error_t(std::exception_ptr),
                                              stdexec::set_error_t(std::error_code)>{};
    }

private:
    AppState &state_;
    RenderSettings render_settings_;
    MandelbrotRenderer &renderer_;
};
