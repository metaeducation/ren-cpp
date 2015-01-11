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
            if (not enginePtr)
                enginePtr = &Engine::runFinder();

            static Context user (*enginePtr, "USER");
            return user;
        };
    }
    return finder(enginePtr);
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
