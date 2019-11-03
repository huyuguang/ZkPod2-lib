
// Copyright (c) 2010-2019 niXman (i dot nixman dog gmail dot com). All
// rights reserved.
//
// This file is part of YAS(https://github.com/niXman/yas) project.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//
//
// Boost Software License - Version 1.0 - August 17th, 2003
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#include <yas/serialize.hpp>

/***************************************************************************/

struct base {
    std::uint32_t x;

    base()
        :x()
    {}

    template<typename Ar>
    void serialize(Ar &ar) {
        ar & YAS_OBJECT(nullptr /* reserved */, x);
    }
};

struct derived: base {
    std::uint32_t y;

    derived()
        :y()
    {}

    template<typename Ar>
    void serialize(Ar &ar) {
        auto &x = yas::base_object<base>(*this);
        ar & YAS_OBJECT(nullptr /* reserved */, y, x);
    }
};

/***************************************************************************/

int main() {
    derived t1, t2;
    t1.x = 33;
    t1.y = 44;

    constexpr std::size_t flags = yas::mem|yas::json;

    yas::shared_buffer buf = yas::save<flags>(t1);

    yas::load<flags>(buf, t2);

    if ( t2.x != 33 || t2.y != 44 ) {
        YAS_THROW_EXCEPTION(std::runtime_error, "bad value");
    }
}

/***************************************************************************/
