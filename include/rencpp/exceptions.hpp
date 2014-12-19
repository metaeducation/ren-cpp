#ifndef RENCPP_EXCEPTIONS_HPP
#define RENCPP_EXCEPTIONS_HPP

///
/// EXCEPTION CLASSES
///

#include <stdexcept>
#include <typeinfo> // std::bad_cast

//
// This is where to place any custom exceptions that are part of the contract
// between Value-based operations and the user, who may wish to catch these
// named exceptions and handle them.
//
// Exactly how many exceptions will be exposed is up for debate, as opposed
// to handling the exceptions inside of the code on the other side of the
// binding and returning a ren::Error.
//

namespace ren {


class bad_value_cast final : public std::bad_cast {
private:
    std::string whatString;

public:
    bad_value_cast (std::string const & whatString) :
        whatString (whatString)
    {
    }

    char const * what() const noexcept override {
        return whatString.c_str();
    }
};


class evaluation_error : public std::exception {
private:
    std::string whatString;

public:
    evaluation_error (std::string const & whatString) :
        whatString (whatString)
    {
    }

    virtual char const * what() const noexcept {
        return whatString.c_str();
    }
};


class too_many_args : public evaluation_error {
public:
    too_many_args (std::string const & whatString) :
        evaluation_error (whatString)
    {
    }
};

}

#endif
