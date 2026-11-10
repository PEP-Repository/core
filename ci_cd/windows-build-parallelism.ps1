#Requires -PSEdition Core

<#
.SYNOPSIS
  Determine the parallelism of (MSBuild-based) Windows builds from the number of logical CPUs and the amount of RAM.

.DESCRIPTION
  Writes NAME=VALUE lines to stdout, which windows-ci-build.bat sets as environment variables:

  - MultiProcMaxCount: the maximum number of concurrent compiler (cl.exe) processes.
    With UseMultiToolTask=true and EnforceProcessCountAcrossBuilds=true (see windows-ci-build.bat),
    this limit is shared by all projects and all MSBuild instances (and therefore all concurrent CI jobs) on the machine.
  - PEP_MSBUILD_MAX_CPU_COUNT: the maximum number of projects that MSBuild builds concurrently (-maxCpuCount).

  A variable that is already set in the environment (e.g. via the GitLab runner configuration) is kept as is.

  The memory estimate per compiler process can be overridden by setting PEP_MEMORY_PER_COMPILE_GIB.
#>

$ErrorActionPreference = 'Stop'

# Rough estimate of the peak memory use of a single cl.exe process for our code base:
#  heavy use of Boost, Qt, and /bigobj, especially in Debug builds.
$MemoryPerCompileGiB = if ($env:PEP_MEMORY_PER_COMPILE_GIB) { [double]$env:PEP_MEMORY_PER_COMPILE_GIB } else { 2 }

# Upper bound for the number of projects that MSBuild builds concurrently.
# Compilation is already limited separately (machine-wide) by MultiProcMaxCount, and a single project can use all of
#  those compiler processes by itself. So we only need enough concurrent projects to keep the compiler processes busy
#  around project boundaries (e.g. while a project is linking), and to link multiple projects concurrently.
# Linking is not limited by MultiProcMaxCount and can use a lot of memory, and every concurrent project also needs its
#  own MSBuild node process. So we don't scale this with the (possibly very large) number of CPUs.
$MaxConcurrentProjects = 8

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
