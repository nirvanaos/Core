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
#include <CORBA/CORBA.h>
#include <CORBA/Proxy/TypeCodeEnum.h>
#include <CORBA/Proxy/TypeCodeException.h>
#include "../Source/ORB/tc_impex.h"

namespace CORBA {
namespace Internal {

template <>
const Char TypeCodeName <CompletionStatus>::name_ [] = "CompletionStatus";

template <>
const Char* const TypeCodeEnum <CompletionStatus>::members_ [] = {
	"COMPLETED_YES",
	"COMPLETED_NO",
	"COMPLETED_MAYBE"
};

typedef TypeCodeEnum <CompletionStatus> TC_CompletionStatus;

template <>
const Parameter TypeCodeMembers <UnknownUserException>::members_ [] = {
	{ "exception", Type <Any>::type_code }
};

typedef TypeCodeException <UnknownUserException> TC_UnknownUserException;

}
}

TC_IMPEX_BY_ID (CompletionStatus)
TC_IMPEX_BY_ID (UnknownUserException)
