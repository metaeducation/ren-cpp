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

Context::Finder Context::finder;



Context Context::lookup(const char * name, Engine * engine)
{
    Context result (Dont::Initialize);

    if (::RenFindContext(engine->handle, name, &result->cell) != REN_SUCCESS)
        throw std::runtime_error ("Couldn't find named context");

    result->finishInit(engine);
    return result;
}



Context Context::runFinder(Engine * engine) {
    if (not finder) {
        finder = [] (Engine * engine) -> Context & {
            if (not engine)
                engine = &Engine::runFinder();

            static Context user = lookup("USER", engine);
            return user;
        };
    }
    return finder(engine);
}


Context::Context (
    internal::Loadable const loadables[],
    size_t numLoadables,
    Context const * contextPtr,
    Engine * engine
) :
    Value (Dont::Initialize)
{
    VAL_SET(&cell, REB_OBJECT);

    // Here, a null context pointer means null.  No finder is invoked.

    RenEngineHandle realEngine = contextPtr ? contextPtr->getEngine() :
        (engine ? engine->getHandle() : Engine::runFinder().getHandle());

    constructOrApplyInitialize(
        realEngine,
        contextPtr,
        nullptr, // no applicand
        loadables,
        numLoadables,
        this, // Do construct
        nullptr // Don't apply
    );
}



// TBD: Finish version where you can use values directly as an array
/*

Context::Context (
    Value const values[],
    size_t numValues,
    Context const * contextPtr,
    Engine * engine
) :
    Value (Dont::Initialize)
{
    VAL_SET(&cell, REB_OBJECT);

    // Here, a null context pointer means null.  No finder is invoked.

    RenEngineHandle realEngine = contextPtr ? contextPtr->getEngine() :
        (engine ? engine->getHandle() : Engine::runFinder().getHandle());

    constructOrApplyInitialize(
        realEngine,
        nullptr, // no applicand
        loadables,
        numLoadables,
        this, // Do construct
        nullptr // Don't apply
    );
}

*/


} // end namespace ren
