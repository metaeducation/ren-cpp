//
// context.cpp
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

#include <cassert>
#include <stdexcept>

#include "rencpp/context.hpp"
#include "rencpp/engine.hpp"
#include "rencpp/runtime.hpp"

namespace ren {

AnyContext::Finder AnyContext::finder;



AnyContext AnyContext::lookup(const char * name, Engine * engine)
{
    if (not engine)
        engine = &Engine::runFinder();

    AnyContext result (Dont::Initialize);

    if (::RenFindContext(result->cell, engine->handle, name) != REN_SUCCESS)
        throw std::runtime_error ("Couldn't find named context");

    result->finishInit(engine->handle);
    return result;
}



AnyContext AnyContext::current(Engine * engine) {
    if (not finder) {
        finder = [] (Engine * engine) -> AnyContext & {
            if (not engine)
                engine = &Engine::runFinder();

            static AnyContext user = lookup("USER", engine);
            return user;
        };
    }
    return finder(engine);
}


} // end namespace ren
