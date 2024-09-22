//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2024, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------

#include "tsSingleton.h"


//-----------------------------------------------------------------------------
// Hand-crafted singleton for ts::atexit().
// It cannot use ts::Singleton because it is used to implement ts::Singleton.
//-----------------------------------------------------------------------------

namespace {

    // Self-contained singleton class.
    class ExitContext
    {
        TS_NOCOPY(ExitContext);                                       \
    public:
        // Get the instance of the singleton.
        static ExitContext& Instance();

        // Register a termination function.
        void add(void (*func)(void*), void* param);

    private:
        // Private default constructor to avoid direct instantiation.
        ExitContext() = default;

        // List of registered functions.
        using ExitHandler = std::pair<void (*)(void*), void*>;
        std::vector<ExitHandler> _handlers {};
        std::mutex _mutex {};

        // Used to build the singleton instance.
        static ExitContext* volatile _instance;
        static std::once_flag _once_flag;

        // Executed at the termination of the program, call all registered functions.
        static void _cleanup();
    };

    // Used to build the singleton instance.
    ExitContext* volatile ExitContext::_instance = nullptr;
    std::once_flag ExitContext::_once_flag {};

    // Get the instance of the singleton.
    ExitContext& ExitContext::Instance()
    {
        // Use a double check lock to avoid calling call_once() more than necessary.
        // To avoid a race condition, _instance must be volatile.
        if (_instance == nullptr) {
            std::call_once(_once_flag, []() {
                _instance = new ExitContext;
                // std::atexit() is called only once, well below the 32 entries limit.
                std::atexit(ExitContext::_cleanup);
            });
        }
        return *_instance;
    }

    // Register a termination function.
    void ExitContext::add(void (*func)(void*), void* param)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _handlers.push_back(std::make_pair(func, param));
    }

    // Executed at the termination of the program.
    void ExitContext::_cleanup()
    {
        if (_instance != nullptr) {
            // No need to lock, we are in a std::atexit() handler, during process termination.
            for (auto it = _instance->_handlers.rbegin(); it != _instance->_handlers.rend(); ++it) {
                it->first(it->second);
            }
            delete _instance;
            _instance = nullptr;
        }
    }
}


//----------------------------------------------------------------------------
// Register a function to execute when the application exits.
//----------------------------------------------------------------------------

void ts::atexit(void (*func)(void*), void* param)
{
    ExitContext::Instance().add(func, param);
}