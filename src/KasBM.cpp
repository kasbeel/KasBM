/******************************************************************************
MIT License

Copyright (c) 2026 Wladimir A. Jiménez B.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
******************************************************************************/
#include <KasBuild.hpp>
#include <KasLog.hpp>
#include <KasProjectConfig.hpp>
#include <KasRun.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace KasProjectConfig;

void check_dependencies() {
    if (std::system("pkg-config --version > /dev/null 2>&1") != 0) {
        KasLog::log(KasLog::Level::ERR,
            "pkg-config no está instalado y es requerido por kasbm.");
        std::cerr
            << "  Debian/Ubuntu : sudo apt install pkgconf\n"
            << "  Fedora/RHEL   : sudo dnf install pkgconf\n"
            << "  Arch          : sudo pacman -S pkgconf\n";
        std::exit(1);
    }
}

void print_help() {
    std::cout << R"(
KasBuildManagment - Simple C++ Build System Managment
USAGE:
  kasbm <command> [options]

COMMANDS:
  build                Build project
  run <command>        Run a command (ej: calculator)
  clean                Clean build artifacts
  help                 Show this help

OPTIONS:
  -t, --target <name>     Specify target to build
  -p, --profile <name>    Build profile (debug, release)
  -v, --verbose           Enable verbose output

EXAMPLES:
  kasbm build
  kasbm build -t kas_toolkit
  kasbm build -p release
  kasbm run calculator
  kasbm clean

)";
}

struct CliArgs {
    std::string command;
    std::string run;
    std::string target = "";
    std::string profile = "";
    bool verbose = false;
};

CliArgs parse_args(int argc, char *argv[]) {
    CliArgs args;
    int arg_index = 1;
    if (argc < 2) {
        print_help();
        exit(0);
    }

    args.command = argv[arg_index++];

    if (args.command == "run" && argc >= 3) {
        args.run = argv[arg_index++];
    }

    for (int i = arg_index; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--target" || arg == "-t") {
            if (i + 1 < argc)
                args.target = argv[++i];
        } else if (arg == "--profile" || arg == "-p") {
            if (i + 1 < argc)
                args.profile = argv[++i];
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        }
    }
    return args;
}

int main(int argc, char *argv[]) {

    check_dependencies();
    CliArgs args = parse_args(argc, argv);
    std::ifstream file("kas_package.json");

    if (!file.is_open()) {
        KasLog::log(KasLog::Level::ERR, "Could not find kas_package.json");
        exit(1);
    }

    json j;
    file >> j;

    KasProject project = j.get<KasProject>();

    KasLog::log(KasLog::Level::INFO, "Project Loaded: %s", project.project_name.c_str());

    if (args.command == "help") {
        print_help();
        return 0;
    }
    if (args.command == "run") {
        return KasRun::run_command(project.commands, args.run);
    }
    if (args.command == "build") {
        if (!KasBuild::build_target(project, args.target.empty() ? "all" : args.target)) {
            KasLog::log(KasLog::Level::ERR, "Build failed.");
            return 1;
        }
        KasLog::log(KasLog::Level::INFO, "Build successful!");
        return 0;
    }
    if (args.command == "clean") {
        KasLog::log(KasLog::Level::INFO, "Clean successful!");
        return 0;
    }
    return 0;
}