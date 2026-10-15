// Regression: static-library consumers must not select weak empty provenance.
#include "anvil/BuildProvenance.hpp"
#include <QCoreApplication>
#include <QProcess>
#include <cstdio>
#include <string>
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QProcess git;
    git.start("git",
              {"-C", QStringLiteral(TFORGE_SOURCE_ROOT), "rev-parse", "HEAD"});
    if (!git.waitForFinished() || git.exitCode() != 0)
        return 1;
    const auto expected = git.readAllStandardOutput().trimmed().toStdString();
    const std::string actual = anvil::buildprov::gitSha;
    const bool ok = actual == expected && actual.size() == 40 &&
                    std::string(anvil::buildprov::origin) ==
                        "build_generated" &&
                    (std::string(anvil::buildprov::gitDirty) == "true" ||
                     std::string(anvil::buildprov::gitDirty) == "false");
    std::printf("player static-library build provenance: %s (sha=%s origin=%s "
                "dirty=%s)\n",
                ok ? "OK" : "FAIL", actual.c_str(), anvil::buildprov::origin,
                anvil::buildprov::gitDirty);
    return ok ? 0 : 1;
}
