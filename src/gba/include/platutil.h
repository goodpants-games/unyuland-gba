/*
Copyright (C) 2026  pkhead

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef PLATUTIL_H
#define PLATUTIL_H

// TODO: do i need to put long_call?
#define ARM_FUNC __attribute__((section(".iwram"), long_call, target("arm")))
#define NO_INLINE __attribute__((noinline))

#endif