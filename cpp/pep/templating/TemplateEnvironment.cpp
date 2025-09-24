#include <pep/templating/TemplateEnvironment.hpp>

namespace pep {
TemplateEnvironment::TemplateEnvironment(const std::filesystem::path& rootDir) :
    mEnvironment((rootDir / ".").lexically_normal().string()) { //Appending "/." to it, and then normalizing, ensures we have a directory separator at the end.
}

std::string TemplateEnvironment::renderTemplate(std::filesystem::path templatePath, const Data& data) {
    return mEnvironment.render_file(templatePath.string(), data);
}
}