//
// evaluator.cpp
// This file is part of Ren Garden
// Copyright (C) 2015 MetÆducation
//
// Ren Garden is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Ren Garden is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Ren Garden.  If not, see <http://www.gnu.org/licenses/>.
//
// See http://ren-garden.metaeducation.com for more information on this project
//

#include <QtWidgets>

#include "rencpp/ren.hpp"
using namespace ren;

#include "evaluator.h"


void EvaluatorWorker::initialize() {
    // Ren/C++ does lazy initialization right now, so the first usage will
    // configure it without an explicit call.  (Interesting as an option,
    // though it shouldn't be the only way...and you should be able to
    // compile with that disabled)

    ren::String lazy = "lazy initialize";

    emit initializeDone();
}


void EvaluatorWorker::doWork(
    ren::Value const & dialectValue,
    ren::Value const & contextValue,
    QString const & input,
    bool meta
) {
    // See notes on MainWindow about qRegisterMetaType about why dialect
    // and context are passed as ren::Value instead of ren::Function and
    // ren::Context (also why it needs the ren:: prefix for slots)

    Function dialect = static_cast<Function>(dialectValue);
    Context context = static_cast<Context>(contextValue);

	optional<Value> result;
    bool success = false;

    try {
        // We *always* generate a block to pass to the dialect.  This
        // is Ren Garden, not "arbitrary shell"... so if you want to
        // pass an arbitrary string you must type it in as {49+3hfa} in
        // a properly loadable string.

        auto loaded = context.create<Block>(input.toUtf8().constData());

        if (meta) {
            if (not runtime("find words-of quote", dialect, "/meta"))
                throw Error {"current dialect has no /meta refinement"};

            result = context(Path {dialect, "meta"}, loaded);
        }
        else
            result = context(dialect, loaded);

        success = true;
    }
    catch (evaluation_throw const & t) {
		if (t.name() != nullopt && t.name()->isWord()) {
			Word word = static_cast<Word>(*t.name());
            if (word.hasSpelling("exit") || word.hasSpelling("quit")) {
                // A programmatic request to quit the system (e.g. QUIT).
                // Might be interesting to have some UI to configure it
                // not actually exiting the whole GUI app, if you don't
                // want it to:
                //
                // https://github.com/metaeducation/ren-garden/issues/17

                if (t.value() == nullopt || t.value()->isNone()) {
                    qApp->exit(0);
                }
                else if (t.value()->isInteger()) {
                    qApp->exit(static_cast<Integer>(*t.value()));
                }
                else {
                    // Do whatever Rebol does...
                    qApp->exit(1);
                }

                // We have submitted our quit message but will have to
                // get back to the message pump... go ahead and return
                // none...
				result = nullopt;
                success = true;
            }
        }

        if (!success) {
            std::string message = std::string("No CATCH for: ") + t.what();
            result = Error {message.c_str()};
        }
    }
    catch (load_error const & e) {
        // Syntax errors which would trip up RenCpp even if no runtime was
        // loaded, so things like `runtime("print {Foo");`

        result = e.error();
    }
    catch (evaluation_error const & e) {
        // Evaluation errors, like `first 100`

        result = e.error();
    }
    catch (evaluation_halt const & e) {
        // Cancellation as with hitting the escape key during an infinite
        // loop.  (Such requests must originate from the GUI thread.)
        // Let returning none for the error mean cancellation.

		result = nullopt;
    }
    catch (std::exception const & e) {
        const char * what = e.what();
        emit caughtNonRebolException(what);

        // !!! MainWindow will show a dialog box.  Should we make up an error?
        result = nullopt;
    }
    catch (...) {
        // !!! MainWindow will show a dialog box.  Should we make up an error?
        result = nullopt;

        emit caughtNonRebolException(nullptr);
    }

    emit resultReady(success, result);
}
