#pragma once

#include <pep/cli/Command.hpp>

namespace pep::cli {

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandList(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandStore(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandDelete(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandGet(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandPull(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandExport(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandAma(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandUser(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandPing(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandValidate(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandVerifiers(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandCastor(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandMetrics(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandRegister(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandXEntry(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandQuery(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandHistory(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandFileExtension(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandToken(CliApplication& parent);
std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandStructureMetadata(CliApplication& parent);

std::shared_ptr<ChildCommandOf<CliApplication>> CreateNoLongerSupportedCommand(CliApplication& parent, const std::string& name, const std::string& description);

}
