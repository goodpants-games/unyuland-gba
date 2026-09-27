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

// disable float support on a full-release build, since Well. The game shouldn't
// be using floats anyway. floats are only used debug logging displaying fixed-
// point values.
#ifndef DEVDEBUG
#define PRINTF_DISABLE_SUPPORT_FLOAT
#define PRINTF_DISABLE_SUPPORT_EXPONENTIAL

#endif

void _putchar(char character) {}
