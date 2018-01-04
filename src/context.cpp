//
// context.cpp
// This file is part of RenCpp
// Copyright (C) 2015-2018 HostileFork.com
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
#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/engine.hpp"
#include "rencpp/runtime.hpp"
#include "rencpp/context.hpp"

#include "common.hpp"


namespace ren {

AnyContext::Finder AnyContext::finder;



AnyContext AnyContext::lookup(const char * name, Engine * engine)
{
    if (engine == nullptr)
        engine = &Engine::runFinder();

    AnyContext result (Dont::Initialize);

    if (::RenFindContext(result->cell, engine->handle, name) != REN_SUCCESS)
        throw std::runtime_error ("Couldn't find named context");

    result->finishInit(engine->handle);
    return result;
}



AnyContext AnyContext::current(Engine * engine) {
    if (finder == nullptr) {
        finder = [] (Engine * engine) -> AnyContext & {
            if (engine == nullptr)
                engine = &Engine::runFinder();

            static AnyContext user = lookup("USER", engine);
            return user;
        };
    }
    return finder(engine);
}


//
// TYPE DETECTION
//

bool AnyContext::isValid(REBVAL const * cell) {
    return IS_OBJECT(cell);
}



//
// CONSTRUCTION
//

AnyContext::AnyContext (
    internal::Loadable const loadables[],
    size_t numLoadables,
    internal::CellFunction cellfun,
    AnyContext const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    (*cellfun)(this->cell);

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

AnyContext::AnyContext (
    AnyValue const values[],
    size_t numValues,
    internal::CellFunction cellfun,
    AnyContext const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    (*cellfun)(this->cell);

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

//
// TYPE HEADER INITIALIZATION
//

void AnyContext::initObject(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_OBJECT);
}

void AnyContext::initError(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_ERROR);
}


} // end namespace ren
