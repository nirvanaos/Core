/// \file
/*
* Nirvana Core.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2021 Igor Popov.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*
* Send comments and/or bug reports to:
*  popov.nirvana@gmail.com
*/
#ifndef NIRVANA_CORE_UNRECOVERABLE_ERROR_H_
#define NIRVANA_CORE_UNRECOVERABLE_ERROR_H_
#pragma once

#include <Nirvana/Nirvana.h>

namespace Nirvana {
namespace Core {
namespace Port {

NIRVANA_NORETURN void _unrecoverable_error (int code, const char* file, unsigned line) noexcept;

}
}
}

/// Unrecoverable error macro. Currently unused.
#define unrecoverable_error(code) ::Nirvana::Core::Port::_unrecoverable_error (code, __FILE__, __LINE__)

#endif
