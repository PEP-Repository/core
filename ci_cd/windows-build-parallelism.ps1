#Requires -PSEdition Core

<#
.SYNOPSIS
  Determine the parallelism of Windows builds from the number of logical CPUs and the amount of RAM.

.DESCRIPTION
  Writes NAME=VALUE lines to stdout, which windows-ci-build.bat sets as environment variables:

  - MultiProcMaxCount: the maximum number of concurrent compiler (cl.exe) processes.
    With UseMultiToolTask=true and EnforceProcessCountAcrossBuilds=true (see windows-ci-build.bat),
    this limit is shared by all projects and all MSBuild instances (and therefore all concurrent CI jobs) on the machine.
    This limit may also be used for Ninja-based builds.
  - PEP_MSBUILD_MAX_CPU_COUNT: the maximum number of projects that MSBuild builds concurrently (-maxCpuCount).

  A variable that is already set in the environment (e.g. via the GitLab runner configuration) is kept as is.

  The memory estimate per compiler process can be overridden by setting PEP_MEMORY_PER_COMPILE_GIB.
#>

$ErrorActionPreference = 'Stop'

# Estimate of the peak memory use per concurrent compiler process for our code base, including a share
#  of the linkers and MSBuild nodes running at the same time. Measured (Nov 2026, all-targets): the whole build
#  peaked at 0.5-0.6 GiB per concurrent compiler process, in both Release and Debug. 1 GiB leaves some headroom.
$MemoryPerCompileGiB = if ($env:PEP_MEMORY_PER_COMPILE_GIB) { [double]$env:PEP_MEMORY_PER_COMPILE_GIB } else { 1 }

# Upper bound for the number of projects that MSBuild builds concurrently (-maxCpuCount).
# Compilation is already limited separately (machine-wide) by MultiProcMaxCount. But with the Visual Studio generator, a project
#  only starts compiling once all projects it references have been built completely, so few concurrent projects leave most
#  compiler slots unused. Measured on pep-build9 (all-targets Release, MultiProcMaxCount=64..96).
#  Linking is not limited by MultiProcMaxCount, and every concurrent project needs its own MSBuild node.
$MaxConcurrentProjects = 32

$cpuCount = [Environment]::ProcessorCount
$ramGiB = (Get-CimInstance -ClassName Win32_ComputerSystem).TotalPhysicalMemory / 1GB

$compileCount = [Math]::Max(1, [Math]::Min($cpuCount, [Math]::Floor($ramGiB / $MemoryPerCompileGiB)))
if ($env:MultiProcMaxCount) {
  $compileCount = [int]$env:MultiProcMaxCount
}

$projectCount = [Math]::Min($compileCount, $MaxConcurrentProjects)
if ($env:PEP_MSBUILD_MAX_CPU_COUNT) {
  $projectCount = [int]$env:PEP_MSBUILD_MAX_CPU_COUNT
}

[Console]::Error.WriteLine("Build parallelism: $cpuCount logical CPUs, $([Math]::Round($ramGiB, 1)) GiB RAM," +
  " $MemoryPerCompileGiB GiB per compiler process => $compileCount compiler processes, $projectCount concurrent projects")

"MultiProcMaxCount=$compileCount"
"PEP_MSBUILD_MAX_CPU_COUNT=$projectCount"
