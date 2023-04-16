// Display images inside a terminal
// Copyright (C) 2023  JustKidding
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "sixel.hpp"
#include "os.hpp"
#include "util.hpp"

#include <iostream>

#include <fmt/format.h>
#include <unistd.h>
#include <cstdlib>

int sixel_draw_callback(char *data, int size, void* priv)
{
    auto stream = static_cast<std::stringstream*>(priv);
    stream->write(data, size);
    return size;
}

SixelCanvas::SixelCanvas()
{
    sixel_output_new(&output, sixel_draw_callback, &ss, nullptr);
    sixel_output_set_encode_policy(output, SIXEL_ENCODEPOLICY_FAST);
}

SixelCanvas::~SixelCanvas()
{
    draw_thread.reset();

    sixel_output_unref(output);
    sixel_dither_unref(dither);
}

auto SixelCanvas::init(const Dimensions& dimensions,
        std::shared_ptr<Image> image) -> void
{
    x = dimensions.x + 1;
    y = dimensions.y + 1;
    max_width = dimensions.max_w;
    max_height = dimensions.max_h;
    this->image = image;

    // create dither and palette from image
    sixel_dither_new(&dither, -1, nullptr);
    sixel_dither_initialize(dither,
            const_cast<unsigned char*>(image->data()),
            image->width(),
            image->height(),
            SIXEL_PIXELFORMAT_RGB888,
            SIXEL_LARGE_LUM,
            SIXEL_REP_CENTER_BOX,
            SIXEL_QUALITY_HIGH);
}

auto SixelCanvas::draw() -> void
{
    if (!image->is_animated()) {
        draw_frame();
        return;
    }

    // start drawing loop
    draw_thread = std::make_unique<std::jthread>([&] (std::stop_token token) {
        while (!token.stop_requested()) {
            draw_frame();
            image->next_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(image->frame_delay()));
        }
    });
}

auto SixelCanvas::clear() -> void
{
    if (max_width == 0 && max_height == 0) return;
    draw_thread.reset();
    sixel_dither_unref(dither);
    ss.str("");

    // clear terminal
    auto line_clear = std::string(max_width, ' ');
    util::save_cursor_position();
    for (int i = y; i <= max_height + 2; ++i) {
        util::move_cursor(i, x);
        std::cout << line_clear;
    }
    std::cout << std::flush;
    util::restore_cursor_position();
}

auto SixelCanvas::draw_frame() -> void
{
    // output sixel content to stream
    sixel_encode(const_cast<unsigned char*>(image->data()),
            image->width(),
            image->height(),
            3 /*unused*/, dither, output);

    util::save_cursor_position();
    util::move_cursor(y, x);
    std::cout << ss.rdbuf() << std::flush;
    util::restore_cursor_position();
}
