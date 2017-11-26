//
// lager - library for functional interactive c++ programs
// Copyright (C) 2017 Juan Pedro Bolivar Puente
//
// This file is part of lager.
//
// lager is free software: you can redistribute it and/or modify
// it under the terms of the MIT License, as detailed in the LICENSE
// file located at the root of this source code distribution,
// or here: <https://github.com/arximboldi/lager/blob/master/LICENSE>
//

#include <SDL.h>
#include <SDL_ttf.h>

#include <lager/store.hpp>
#include <lager/event_loop/sdl.hpp>

#include <variant>
#include <string>
#include <iostream>
#include <complex>
#include <cmath>

using point = std::complex<float>;

constexpr auto window_width  = 800;
constexpr auto window_height = 600;
constexpr auto padding       = 20;
constexpr auto border        = 4;
constexpr auto ball_r        = 4;
constexpr auto ball_init_v   = point{ 0.2f, 0.2f };
constexpr auto ball_a        = 1.1;
constexpr auto paddle_width  = 100;
constexpr auto paddle_height = 10;
constexpr auto paddle_y      = window_height - 2*padding - paddle_height;
constexpr auto font_size     = 32;
constexpr auto game_rect     = SDL_Rect {
    padding,
    padding,
    window_width  - 2 * padding,
    window_height - 2 * padding,
};

struct game
{
    int score      = 0;
    point ball     = { window_width / 2, padding * 2 };
    point ball_v   = ball_init_v;
    float paddle_x = window_width / 2 - paddle_width / 2;
};

float x(const point& p)    { return p.real(); }
void  x(point& p, float v) { return p.real(v); }
float y(const point& p)    { return p.imag(); }
void  y(point& p, float v) { return p.imag(v); }

float dot(const point& a, const point& b)
{
    return x(conj(a) * b);
}

// squared distance between two segments
float segment_norm(point l1p1, point l1p2, point l2p1, point l2p2)
{
    constexpr auto epsilon = 0.00000001;
    auto u = l1p2 - l1p1;
    auto v = l2p2 - l2p1;
    auto w = l1p1 - l2p1;
    auto a = dot(u, u), b = dot(u, v),
         c = dot(v, v), d = dot(u, w),
         e = dot(v, w);
    auto D = a * c - b * b;
    float sc, sN, sD = D;
    float tc, tN, tD = D;
    if (D < epsilon) {
        sN = 0.0; sD = 1.0; tN = e; tD = c;
    } else {
        sN = (b*e - c*d); tN = (a*e - b*d);
        if (sN < 0.0) {
            sN = 0.0; tN = e; tD = c;
        } else if (sN > sD) {
            sN = sD; tN = e + b; tD = c;
        }
    }
    if (tN < 0.0) {
        tN = 0.0;
        if (-d < 0.0)    sN = 0.0;
        else if (-d > a) sN = sD;
        else {           sN = -d; sD = a; }
    } else if (tN > tD) {
        tN = tD;
        if ((-d + b) < 0.0)    sN = 0;
        else if ((-d + b) > a) sN = sD;
        else {                 sN = (-d + b); sD = a; }
    }
    sc = (std::abs(sN) < epsilon ? 0.0 : sN / sD);
    tc = (std::abs(tN) < epsilon ? 0.0 : tN / tD);
    auto dP = w + (u * sc) - (v * tc);
    return norm(dP);
}

struct paddle_move_action { int center_x; };
struct tick_action { float delta; };

using action = std::variant<
    paddle_move_action,
    tick_action>;

game update(game g, action a)
{
    return std::visit(lager::visitor{
        [&] (paddle_move_action a)
        {
            g.paddle_x = a.center_x - paddle_width / 2;
            return g;
        },
        [&] (tick_action a)
        {
            auto ball = g.ball + g.ball_v * a.delta;
            if ((x(g.ball_v) < 0 && x(ball) - ball_r <= padding) ||
                (x(g.ball_v) > 0 && x(ball) + ball_r >= window_width - padding))
                x(g.ball_v, -x(g.ball_v));
            if (y(g.ball_v) < 0 && y(ball) - ball_r <= padding)
                y(g.ball_v, -y(g.ball_v));
            if (y(g.ball_v) > 0 && ball_r * ball_r
                > segment_norm(g.ball, ball,
                               point{g.paddle_x, paddle_y},
                               point{g.paddle_x + paddle_width, paddle_y})) {
                y(g.ball_v, -y(g.ball_v));
                g.ball_v *= ball_a;
                g.score ++;
            } else if (y(g.ball_v) > 0 && y(ball) - ball_r >= window_height - padding) {
                g.score  = 0;
                g.ball_v = ball_init_v;
                g.ball   = {
                    padding + (float) std::rand() / RAND_MAX
                    * (window_width - padding * 4),
                    padding * 2
                };
            } else
                g.ball = ball;
            return g;
        }
    }, a);
}

std::string font_path()
{
    using namespace std::string_literals;
    return lager::resources_path() + "/SourceSansPro-Bold.ttf"s;
}

struct sdl_view
{
    SDL_Window* window     = SDL_CreateWindow("Lager Counter Example",
                                              SDL_WINDOWPOS_UNDEFINED,
                                              SDL_WINDOWPOS_UNDEFINED,
                                              window_width, window_height,
                                              SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    TTF_Font* font         = TTF_OpenFont(font_path().c_str(), font_size);
};

void draw(sdl_view& v, game g)
{
    SDL_RenderClear(v.renderer);
    // render background
    {
        SDL_SetRenderDrawColor(v.renderer, 0, 0, 0, 255);
        auto rect = SDL_Rect{0, 0, window_width, window_height};
        SDL_RenderFillRect(v.renderer, &rect);
    }
    // render text
    {
        auto msg  = std::to_string(g.score);
        auto surf = TTF_RenderText_Blended(v.font, msg.c_str(), {255, 255, 255, 255});
        auto text = SDL_CreateTextureFromSurface(v.renderer, surf);
        SDL_FreeSurface(surf);

        auto rect = SDL_Rect{2 * padding, 2 * padding - font_size / 3 };
        SDL_QueryTexture(text, nullptr, nullptr, &rect.w, &rect.h);
        SDL_RenderCopy(v.renderer, text, nullptr, &rect);
    }
    // render border
    {
        SDL_SetRenderDrawColor(v.renderer, 255, 255, 255, 255);
        auto rect = SDL_Rect{padding - border, padding - border,
                             window_width - 2*padding + 2*border,
                             window_height - 2*padding + 2*border};
        for (auto i = 0; i < border; ++i, ++rect.x, ++rect.y, ----rect.w, ----rect.h)
            SDL_RenderDrawRect(v.renderer, &rect);
    }
    // render game
    {
        SDL_RenderSetClipRect(v.renderer, &game_rect);
        SDL_SetRenderDrawColor(v.renderer, 255, 255, 255, 255);
        // paddle
        {
            auto rect = SDL_Rect{(int) g.paddle_x, paddle_y,
                                 paddle_width, paddle_height};
            SDL_RenderFillRect(v.renderer, &rect);
        }
        // ball
        {
            auto rect = SDL_Rect{(int) x(g.ball) - ball_r,
                                 (int) y(g.ball) - ball_r,
                                 ball_r * 2, ball_r * 2};
            SDL_RenderFillRect(v.renderer, &rect);
        }
        SDL_RenderSetClipRect(v.renderer, nullptr);
    }
    SDL_RenderPresent(v.renderer);
}

std::optional<action> intent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_MOUSEMOTION:
        return paddle_move_action{event.motion.x};
    default:
        return std::nullopt;
    }
}

int main()
{
    using namespace std::placeholders;

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_ShowCursor(false);

    auto view  = sdl_view{};
    auto loop  = lager::sdl_event_loop{};
    auto store = lager::make_store<action>(
        game{},
        update,
        std::bind(draw, view, _1),
        lager::with_sdl_event_loop{loop});

    loop.run(
        [&] (const SDL_Event& ev) {
            if (auto act = intent(ev))
                store.dispatch(*act);
            return ev.type != SDL_QUIT;
        },
        [&] (float delta) {
            store.dispatch(tick_action{delta});
            return true;
        });
}
