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
#include "BackOff.h"
#include <random>
#if defined (_MSVC_LANG)
#include <intrin.h>
#endif

namespace Nirvana {
namespace Core {

BackOff::BackOff () noexcept :
	iterations_ (1),
	rndgen_ ((RandomGen::result_type)(uintptr_t)this)
{}

BackOff::~BackOff ()
{}

inline void BackOff::cpu_relax () noexcept
{
#if defined (_MSVC_LANG)
	_mm_pause ();
#elif defined (__GNUG__) || defined (__clang__)
	asm ("pause");
#endif
}

void BackOff::operator () () noexcept
{
	// TODO: We can try other distributions: geometric, exponential...
	typedef std::uniform_int_distribution <unsigned> Dist;

	unsigned iterations = iterations_;
	if (iterations >= 4)
		iterations = Dist (iterations / 2 + 1, iterations) (rndgen_);

	if (iterations < iterations_yield () && SystemInfo::hardware_concurrency () > 1) {
		do
			cpu_relax ();
		while (--iterations);
	} else
		Port::BackOff::yield (iterations);

	unsigned imax = iterations_max ();
	if ((iterations_ <<= 1) > imax)
		iterations_ = imax;
}

}
}
