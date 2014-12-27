#ifndef RENCPP_EXCEPTIONS_HPP
#define RENCPP_EXCEPTIONS_HPP

///
/// EXCEPTION CLASSES
///

#include <stdexcept>

#include "values.hpp"

//
// This is where to place any custom exceptions that are part of the contract
// between Value-based operations and the user, who may wish to catch these
// named exceptions and handle them.
//
// Exactly how many exceptions will be exposed is up for debate, as opposed
// to handling the exceptions inside of the code on the other side of the
// binding and returning a ren::Error.  Also: is it necessary to recreate
// the entire error! spectrum with a matching exception, or just catch all
// with one that gives you an error! ?
//

namespace ren {


class evaluation_error : public std::exception {
private:
    Value errorValue;
    std::string whatString;

public:
    evaluation_error (Value const & error) :
        errorValue (error),
        whatString (static_cast<std::string>(errorValue))
    {
    }

    virtual char const * what() const noexcept {
        return whatString.c_str();
    }

    Value error() const noexcept {
        return errorValue;
    }
};


//
// too_many_args is thrown by the binding for generalized apply and there is
// no error!, but maybe we could make one and fold it in with evaluation
// error?
//

class too_many_args : public std::exception {
private:
    std::string whatString;

public:
    too_many_args (std::string const & whatString) :
        whatString (whatString)
    {
    }

    virtual char const * what() const noexcept {
        return whatString.c_str();
    }

};


class exit_command : public std::exception {
private:
    int codeValue;
public:
    exit_command (int code) :
        codeValue (code)
    {
    }

    int code() {
        return codeValue;
    }
};

}

#endif
