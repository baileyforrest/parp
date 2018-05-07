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

#include "util/flags.h"

#include <getopt.h>

#include <cstdlib>
#include <iostream>
#include <map>

#include "gc/gc.h"

namespace util {
namespace {

constexpr char kOptionHeader[] = "--";

std::vector<std::string> g_argv;
std::map<std::string, std::string> g_arg_map;

void PrintFlags() {
  std::cout << "Flags:\n";
  std::cout << "  -h\t\t\t Display this message\n";
  std::cout << "  " << kOptionHeader << Flags::kDebugMemory
            << "\t Enable strict memory checking\n";
}

void PrintHelp() {
  std::cout << "Usage: " << g_argv[0] << " [OPTION]... [FILE]...\n";
  std::cout << "Invoke with no files to invoke interactive REPL\n\n";
  PrintFlags();
}

void PrintTestHelp() {
  std::cout << "Accepts standard gtest options\n\n";
  PrintFlags();
}

}  // namespace

// static
constexpr char Flags::kDebugMemory[];

// static
void Flags::Init(int argc, char** argv, bool test_mode) {
  g_argv.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    g_argv.push_back(argv[i]);
  }

  while (true) {
    static struct option kOptions[] = {{kDebugMemory, no_argument, 0, 0},
                                       {0, 0, 0, 0}};

    // Supress error messages
    if (test_mode) {
      opterr = 0;
    }
    int option_index = 0;
    int c = getopt_long(argc, argv, "h", kOptions, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 0:
        g_arg_map.emplace(kOptions[option_index].name, optarg ? optarg : "");
        break;
      case 'h':
        if (test_mode) {
          PrintTestHelp();
        } else {
          PrintHelp();
        }
        exit(EXIT_SUCCESS);
      default:
        if (test_mode) {
          break;
        }
        PrintHelp();
        exit(EXIT_FAILURE);
    }
  }

  if (IsSet(kDebugMemory)) {
    gc::Gc::Get().set_debug_mode(true);
  }
}

// static
const std::vector<std::string>& Flags::Argv() {
  return g_argv;
}

// static
bool Flags::IsSet(const std::string& value) {
  return g_arg_map.find(value) != g_arg_map.end();
}

}  // namespace util
