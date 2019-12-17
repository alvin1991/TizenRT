/****************************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <vector>

// pointer data();

#include <vector>
#include <cassert>
#include "test_macros.h"
#include "libcxx_tc_common.h"

#include "asan_testing.h"

struct Nasty {
    Nasty() : i_(0) {}
    Nasty(int i) : i_(i) {}
    ~Nasty() {}

//    Nasty * operator&() const { TC_ASSERT_EXPR(false); return nullptr; }
    int i_;
    };

int tc_libcxx_containers_vector_data_data(void)
{
    {
        std::vector<int> v;
        TC_ASSERT_EXPR(v.data() == 0);
        TC_ASSERT_EXPR(is_contiguous_container_asan_correct(v));
    }
    {
        std::vector<int> v(100);
        TC_ASSERT_EXPR(v.data() == std::addressof(v.front()));
        TC_ASSERT_EXPR(is_contiguous_container_asan_correct(v));
    }
    {
        std::vector<Nasty> v(100);
        TC_ASSERT_EXPR(v.data() == std::addressof(v.front()));
        TC_ASSERT_EXPR(is_contiguous_container_asan_correct(v));
    }
    TC_SUCCESS_RESULT();
    return 0;
}
