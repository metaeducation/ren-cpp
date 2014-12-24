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
    internal::Loadable * argsPtr,
    size_t numArgs,
    Value * constructResultUninitialized,
    Value * applyResultUninitialized
) {
    return constructOrApplyInitializeCore(
        enginePtr->handle,
        handle,
        applicandPtr,
        argsPtr,
        numArgs,
        constructResultUninitialized,
        applyResultUninitialized
    );
}

void Context::constructOrApplyInitializeCore(
    RenEngineHandle engineHandle,
    RenContextHandle contextHandle,
    Value const * applicandPtr,
    internal::Loadable * argsPtr,
    size_t numArgs,
    Value * constructOutUninitialized,
    Value * applyOutUninitialized
) {
    auto result = ::RenConstructOrApply(
        engineHandle,
        contextHandle,
        evilMutablePointerCast(&applicandPtr->cell),
        &argsPtr->cell,
        numArgs,
        sizeof(internal::Loadable),
        constructOutUninitialized ? &constructOutUninitialized->cell : nullptr,
        applyOutUninitialized ? &applyOutUninitialized->cell : nullptr
    );

    // We must finalize the values before throwing errors

    if (constructOutUninitialized) {
        constructOutUninitialized->finishInit(engineHandle);
    }

    if (applyOutUninitialized) {
        applyOutUninitialized->finishInit(engineHandle);
    }

    switch (result) {
        case REN_SUCCESS:
            break;
        case REN_ERROR_TOO_MANY_ARGS:
            throw too_many_args("Too many arguments for Generalized Apply");
            break;
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
