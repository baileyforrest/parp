/*
 * Copyright (C) 2015 Bailey Forrest <baileycforrest@gmail.com>
 *
 * This file is part of parp.
 *
 * parp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * parp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with parp.  If not, see <http://www.gnu.org/licenses/>.
 */
/**
 * Classes of characters for switch statements
 */

#ifndef UTIL_CHAR_CLASS_H_
#define UTIL_CHAR_CLASS_H_

// clang-format off

#define ASCII_LOWER                                                     \
    'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': \
case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o':   \
case 'p': case 'q': case 'r': case 's': case 't': case 'u': case 'v':   \
case 'w': case 'x': case 'y': case 'z'

#define ASCII_UPPER                                                     \
    'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': \
case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O':   \
case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V':   \
case 'W': case 'X': case 'Y': case 'Z'

// <letter>
#define ASCII_LETTER \
    ASCII_LOWER: case ASCII_UPPER

// <digit>
#define ASCII_DIGIT                                                     \
    '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': \
case '8': case '9'

// <special initial>
#define SPECIAL_INITIAL \
    '!': case '$': case '%': case '&': case '*': case '/': case ':': \
    case '<': case '=': case '>': case '?': case '^': case '_': case '~'

// <subsequent>
#define ID_SUBSEQUENT \
    ASCII_LETTER: case SPECIAL_INITIAL: case ASCII_DIGIT: \
    case '+': case '-': case '.': case '@'

#define RADIX_SPECIFIER \
    'b': case 'B': case 'o': case 'O': case 'd': case 'D': case 'x': case 'X'

#define EXACT_SPECIFIER \
    'i': case 'I': case 'e': case 'E'

// clang-format on

namespace util {

bool IsDelim(int c);

}  // namespace util

#endif  // UTIL_CHAR_CLASS_H_
