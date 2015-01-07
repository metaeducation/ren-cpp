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

#include "rencpp/context.hpp"
#include "rencpp/engine.hpp"
#include "rencpp/runtime.hpp"

namespace ren {

Context::Finder Context::finder;

Context::Context (Engine & engine) :
    enginePtr (&engine),
    needsFree (true)
{
    if (::RenAllocContext(engine.handle, &handle) != REN_SUCCESS)
        throw std::runtime_error ("Couldn't initialize red runtime");
}


Context::Context (Engine & engine, const char * name) :
    enginePtr (&engine),
    needsFree (false)
{
    if (::RenFindContext(engine.handle, name, &handle) != REN_SUCCESS)
        throw std::runtime_error ("Couldn't find named context");
}


Context::Context () :
    Context (Engine::runFinder())
{
}


Context::Context (const char * name) :
    Context (Engine::runFinder(), name)
{
}


Engine & Context::getEngine() {
    // Technically we do not need to hold onto the engine handle.
    // We should be able to use ::RenGetEngineForContext and look up
    // the engine object instance from that, if we tracked engines
    // globally somehow.  Just stowing a pointer for now as I don't
    // expect a lot of contexts being created any time soon.

    return *enginePtr;
}


Context & Context::runFinder(Engine * enginePtr) {
    if (not finder) {
        finder = [] (Engine * enginePtr) -> Context & {
            assert(enginePtr == nullptr); // you are using default behavior
            static Context user (Engine::runFinder(), "USER");
            return user;
        };
    }
    return finder(enginePtr);
}


void Context::constructOrApplyInitialize(
    Value const * applicandPtr,
    RenCell * argsPtr,
    size_t numArgs,
    Value * constructResultUninitialized,
    Value * applyOutUninitialized
) {
    return constructOrApplyInitializeCore(
        enginePtr->handle,
        handle,
        applicandPtr,
        argsPtr,
        numArgs,
        constructResultUninitialized,
        applyOutUninitialized

    );
}

void Context::constructOrApplyInitializeCore(
    RenEngineHandle engineHandle,
    RenContextHandle contextHandle,
    Value const * applicandPtr,
    RenCell * argsPtr,
    size_t numArgs,
    Value * constructOutUninitialized,
    Value * applyOutUninitialized
) {
    Value errorOutUninitialized {Value::Dont::Initialize};

    auto result = ::RenConstructOrApply(
        engineHandle,
        contextHandle,
        evilMutablePointerCast(&applicandPtr->cell),
        argsPtr,
        numArgs,
        sizeof(internal::Loadable),
        constructOutUninitialized ? &constructOutUninitialized->cell : nullptr,
        applyOutUninitialized ? &applyOutUninitialized->cell : nullptr,
        &errorOutUninitialized.cell
    );

    // We must finalize the values before throwing errors

    if (constructOutUninitialized)
        constructOutUninitialized->finishInit(engineHandle);

    if (applyOutUninitialized)
        applyOutUninitialized->finishInit(engineHandle);

    switch (result) {
        case REN_SUCCESS:
            break;
        case REN_CONSTRUCT_ERROR:
        case REN_APPLY_ERROR:
            errorOutUninitialized.finishInit(engineHandle);
            throw evaluation_error(errorOutUninitialized);
            break;
        case REN_EVALUATION_CANCELLED:
            throw evaluation_cancelled();
        case REN_EVALUATION_EXITED:
            throw exit_command(VAL_INT32(&errorOutUninitialized.cell));
        default:
            throw std::runtime_error("Unknown error in RenConstructOrApply");
    }
}



void Context::close() {
    auto releaseMe = handle;
    handle = REN_CONTEXT_HANDLE_INVALID;
    if (needsFree)
        if (::RenFreeContext(enginePtr->handle, releaseMe) != REN_SUCCESS) {
            throw std::runtime_error ("Failed to shut down red environment");
        }
    enginePtr = nullptr;
}


Context::~Context() {
    if (needsFree and not(REN_IS_CONTEXT_HANDLE_INVALID(handle)))
        ::RenFreeContext(enginePtr->handle, handle);
    handle = REN_CONTEXT_HANDLE_INVALID;
}

}
