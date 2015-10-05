//
// engine.cpp
// This file is part of RenCpp
// Copyright (C) 2015 HostileFork.com
//
// Licensed under the Boost License, Version 1.0 (the "License")
//
//      http://www.boost.org/LICENSE_1_0.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.  See the License for the specific language governing
// permissions and limitations under the License.
//
// See http://rencpp.hostilefork.com for more information on this project
//

#include "rencpp/engine.hpp"
#include "rencpp/ren.hpp"


namespace ren {

Engine::Finder Engine::finder;


std::ostream & Engine::setOutputStream(std::ostream & os) {
    auto temp = osPtr;
    osPtr = &os;
    return *temp;
}


std::istream & Engine::setInputStream(std::istream & is) {
    auto temp = isPtr;
    isPtr = &is;
    return *temp;
}


std::ostream & Engine::getOutputStream() {
    return *osPtr;
}


std::istream & Engine::getInputStream() {
    return *isPtr;
}


optional<AnyValue> Engine::evaluate(
    std::initializer_list<internal::BlockLoadable<Block>> loadables,
    Engine & engine
) {
    return runtime.evaluate(
        loadables.begin(),
        loadables.size(),
        nullptr,
        &engine
    );
}

}
