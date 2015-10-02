#ifndef RENGARDEN_VALUEEXPLORER_H
#define RENGARDEN_VALUEEXPLORER_H

//
// valueexplorer.h
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

#include "optional/optional.hpp"

#include <QPlainTextEdit>

#include "rencpp/ren.hpp"

class ValueExplorer : public QPlainTextEdit {
    Q_OBJECT

private:
	ren::optional<ren::Value> value;

public:
    ValueExplorer (QWidget * parent);
    ~ValueExplorer () override;

public slots:
	void setValue(
		ren::Value const & helpFunction,
		ren::optional<ren::Value> const & value
	);
};

#endif
