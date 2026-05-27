#include <KasBuild.hpp>

#include <cstdlib>
#include <future>
#include <thread>

namespace {
    std::string make_pkg_config_command(const std::string &query, const std::string &libs) {
        return "pkg-config " + query + " " + libs;
    }

    bool check_system_dependency(const std::string &lib_name) {
        const std::string command = "pkg-config --exists " + lib_name + " 2>/dev/null";

        int result = std::system(command.c_str());
        KasLog::log(KasLog::Level::DEBUG, "Checking system dependency: %s %s", lib_name.c_str(), (result == 0) ? "[OK]" : "[MISSING]");
        if (result == 0) {
            return true;
        }

        KasLog::log(KasLog::Level::ERR, "Missing system library: %s", lib_name.c_str());
        KasLog::log(KasLog::Level::INFO, "Suggestion: sudo apt install lib%s", (lib_name + "-dev").c_str());
        return false;
    }

    const KasProjectConfig::TargetConfig *find_target(const KasProjectConfig::KasProject &project, const std::string &name) {
        for (const auto &t : project.targets) {
            if (t.name == name)
                return &t;
        }
        return nullptr;
    }

    void resolve_order(const std::string &target_name,
                       const KasProjectConfig::KasProject &project,
                       std::vector<std::string> &ordered_list,
                       std::set<std::string> &visited) {
        if (visited.count(target_name))
            return;

        const auto *target = find_target(project, target_name);
        if (!target)
            return;

        for (const auto &dep : target->dependencies) {
            resolve_order(dep, project, ordered_list, visited);
        }

        visited.insert(target_name);
        ordered_list.push_back(target_name);
    }

    bool needs_recompile(const std::string &source_p, const std::string &object_p) {
        if (!std::filesystem::exists(object_p))
            return true;

        auto source_time = std::filesystem::last_write_time(source_p);
        auto object_time = std::filesystem::last_write_time(object_p);

        return source_time > object_time;
    }

    bool should_build_obj(const std::string &src, const std::string &obj) {
        if (needs_recompile(src, obj))
            return true;

        std::filesystem::path p(obj);
        p.replace_extension(".d");
        std::string dep_file = p.string();

        if (std::filesystem::exists(dep_file)) {
            std::vector<std::string> dependencies = KasFS::parse_dep_file(dep_file);
            const auto obj_time = std::filesystem::last_write_time(obj);
            for (const auto &dep : dependencies) {
                if (std::filesystem::exists(dep)) {
                    KasLog::log(KasLog::Level::DEBUG, "Checking dependency: %s", dep.c_str());
                    if (std::filesystem::last_write_time(dep) > obj_time) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void append_include_flags(std::string &includes, const std::vector<std::string> &include_dirs) {
        for (const auto &dir : include_dirs) {
            includes += " -I" + dir;
        }
    }

    std::vector<std::string> gather_sources(const KasProjectConfig::TargetConfig &target) {
        std::vector<std::string> sources;
        for (const auto &src_pattern : target.sources) {
            std::vector<std::string> src_files = KasFS::resolve_pattern(src_pattern, "cpp");
            sources.insert(sources.end(), src_files.begin(), src_files.end());
        }
        return sources;
    }

    void resolve_target_dependencies(const KasProjectConfig::KasProject &project,
                                     const KasProjectConfig::TargetConfig &target,
                                     std::string &libs,
                                     std::string &libs_flags,
                                     bool &all_deps_ok) {
        for (const auto &lib : target.system_libs) {
            if (!check_system_dependency(lib)) {
                all_deps_ok = false;
            }
            KasUtils::append_spaced(libs, lib);
        }

        if (!libs.empty()) {
            libs_flags = KasUtils::exec_cmd(make_pkg_config_command("--libs", libs).c_str());
        }

        for (const auto &dep : target.dependencies) {
            const auto *dep_target = find_target(project, dep);
            if (!dep_target) {
                KasLog::log(KasLog::Level::ERR, "Dependency target not found: %s", dep.c_str());
                all_deps_ok = false;
                continue;
            }

            if (dep_target->type == "shared_lib") {
                KasUtils::append_spaced(libs_flags, "-L" + dep_target->output_dir);
                KasUtils::append_spaced(libs_flags, "-l" + dep_target->output_name);
            }
        }
    }

    std::string build_extra_flags(const KasProjectConfig::KasProject &project, const KasProjectConfig::TargetConfig &target) {
        std::string extra_flags = KasUtils::join_flags(project.toolchain.cxxflags);
        extra_flags += KasUtils::join_flags(target.cxxflags);

        KasUtils::ensure_flag(extra_flags, "-MMD");
        KasUtils::ensure_flag(extra_flags, "-MP");

        if (target.type == "shared_lib") {
            KasUtils::ensure_flag(extra_flags, "-fPIC");
        }

        return extra_flags;
    }

    int run_compile_target(const KasProjectConfig::KasProject &project, const KasProjectConfig::TargetConfig &target) {
        bool all_deps_ok = true;
        bool needs_relink = false;
        std::string libs;
        std::string libs_flags;
        std::string includes;

        resolve_target_dependencies(project, target, libs, libs_flags, all_deps_ok);

        if (!libs.empty()) {
            includes = KasUtils::exec_cmd(make_pkg_config_command("--cflags", libs).c_str());
        }

        if (!all_deps_ok) {
            KasLog::log(KasLog::Level::ERR, "Missing system dependencies. Aborting compilation.");
            return 1;
        }

        const std::vector<std::string> sources = gather_sources(target);
        for (const auto &inc_pattern : target.includes) {
            const std::vector<std::string> inc_dirs = KasFS::resolve_pattern(inc_pattern, "", true);
            append_include_flags(includes, inc_dirs);
        }

        const std::string extra_flags = build_extra_flags(project, target);
        const unsigned int detected_threads = std::thread::hardware_concurrency();
        const int max_threads = (project.toolchain.jobs > 0) ? project.toolchain.jobs : static_cast<int>(detected_threads == 0 ? 1 : detected_threads);
        KasConcurrency::Semaphore sem(max_threads);
        std::vector<std::future<int>> tasks;

        KasLog::log(KasLog::Level::INFO, "Compiling target %s using %d threads", target.name.c_str(), max_threads);
        std::string objects;
        for (const auto &src : sources) {
            auto obj = KasFS::get_object_path(src, target.build_dir);
            objects += obj + " ";
            if (!should_build_obj(src, obj)) {
                KasLog::log(KasLog::Level::DEBUG, "Up to date: %s", src.c_str());
                continue;
            }

            needs_relink = true;
            sem.wait();
            tasks.push_back(std::async(std::launch::async, [&sem, &project, src, obj, includes, extra_flags]() {
                int res = KasEngine::run_compile(src, obj, project, includes, extra_flags);
                sem.notify();
                return res;
            }));
        }

        bool success = true;
        for (auto &t : tasks) {
            if (t.get() != 0)
                success = false;
        }

        if (!success) {
            KasLog::log(KasLog::Level::ERR, "Compilation failed for target: %s", target.name.c_str());
            return 1;
        }

        if (needs_relink) {
            KasEngine::run_linker(project, target, objects, libs_flags);
        }
        return 0;
    }
} // namespace

namespace KasBuild {
    bool build_target(const KasProjectConfig::KasProject &project, std::string target_req) {
        std::vector<std::string> build_queue;
        std::set<std::string> visited;

        if (target_req == "all") {
            for (const auto &t : project.targets) {
                resolve_order(t.name, project, build_queue, visited);
            }
        } else {
            if (find_target(project, target_req)) {
                resolve_order(target_req, project, build_queue, visited);
            } else {
                KasLog::log(KasLog::Level::ERR, "Target not found: %s", target_req.c_str());
                return false;
            }
        }

        for (const auto &t_name : build_queue) {
            const auto *target = find_target(project, t_name);
            KasLog::log(KasLog::Level::INFO, "Compiling target: %s", t_name.c_str());

            if (target == nullptr || run_compile_target(project, *target) != 0) {
                KasLog::log(KasLog::Level::ERR, "Error compiling target: %s", t_name.c_str());
                return false;
            }
        }

        return true;
    }
} // namespace KasBuild