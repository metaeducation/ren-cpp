//
// valueexplorer.cpp
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

#include <sstream>

#include <QMessageBox>

#include "valueexplorer.h"

using namespace ren;

ValueExplorer::ValueExplorer (QWidget * parent) :
    QPlainTextEdit (parent)
{
    setReadOnly(true);
    zoomOut(); // make slightly smaller than normal?  :-/
}


void ValueExplorer::setValue(
    AnyValue const & helpFunction,
    optional<AnyValue> const & value
) {
    if (not this->isVisible())
        return;

    if (value == nullopt) {
        document()->setPlainText("");
    }
    else {
        try {
            std::stringstream ss;

            auto & oldStream = Engine::runFinder().setOutputStream(ss);
            static_cast<Function>(helpFunction)(value);
            Engine::runFinder().setOutputStream(oldStream);

            document()->setPlainText(ss.str().c_str());
        }
        catch (std::exception const & e) {
            // Some error... tell user so (but don't crash)
            auto msg = e.what();
            QMessageBox::information(
                NULL,
                "HELP function internal error during Value ValueExplorer",
                msg
            );
        }
        catch (...) {
            assert(false); // some non-std::exception??  :-/
            throw;
        }
    }
}


ValueExplorer::~ValueExplorer ()
{
}
