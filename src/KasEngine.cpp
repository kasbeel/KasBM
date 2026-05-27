#include <KasEngine.hpp>
#include <KasLog.hpp>

namespace KasEngine {
    int run_compile(const std::string &src, const std::string &obj, const KasProjectConfig::KasProject &project, const std::string &inc, const std::string &extra) {
        fs::create_directories(fs::path(obj).parent_path());
        std::string cmd = project.toolchain.compiler + " -std=" + project.toolchain.standards.at("cxx") +
                          " -c " + src + " -o " + obj + " " + inc + " " + extra;

        KasLog::log(KasLog::Level::DEBUG, "Compile command: %s", cmd.c_str());
        int res = std::system(cmd.c_str());
        if (res != 0)
            throw KasException::KasBMException("Compilation failed for: " + src);
        return 0;
    }

    void run_linker(const KasProjectConfig::KasProject &project, const KasProjectConfig::TargetConfig &target, const std::string &objs, const std::string &libs) {
        fs::create_directories(target.output_dir);
        std::string out_name = (target.type == "shared_lib") ? "lib" + target.output_name + target.output_extension + "." + project.version : target.output_name;

        std::string cmd = project.toolchain.compiler + " " + KasUtils::join_flags(project.toolchain.ldflags) +
                          KasUtils::join_flags(target.ldflags) + objs + " -o " + target.output_dir + "/" + out_name;

        if (target.type == "shared_lib")
            cmd += " -shared ";
        cmd += " " + libs;

        KasLog::log(KasLog::Level::DEBUG, "Link command: %s", cmd.c_str());
        if (std::system(cmd.c_str()) != 0)
            throw KasException::KasBMException("Linking failed for target: " + target.name);
        if (target.type == "shared_lib")
            KasFS::update_symlink(out_name, target.output_dir + "/" + "lib" + target.output_name + target.output_extension);
    }
} // namespace KasEngine