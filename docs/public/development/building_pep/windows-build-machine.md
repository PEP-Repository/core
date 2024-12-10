# Windows build machine

**THESE INSTRUCTIONS ARE OUTDATED.** New and improved instructions can be found [here](https://docs.pages.pep.cs.ru.nl/private/ops/master/Creating-a-Windows-CI-runner-VM/).

Gitlab runners for Windows can be virtualized or installed directly on iron. Although virtual machines on FreeBSD jails have been observed to have dismal performance, the provisioning of such a VM is still documented here in case we need it at some point. The installation of Windows on non-virtual machines (or on VMs based on technology other than FreeBSD jails) is not documented on this page.

The installation of any Windows CI runner machine (whether virtual or physical) requires Windows to be installed and Remote Desktop to be enabled: Windows Explorer -> right-click "This PC" -> select "Properties" from the drop-down menu -> click "Remote settings" -> under "Remote Desktop", select "Allow remote connections to this computer" -> confirm. Test whether the machine is accessible by connecting to the machine, either by DNS name or by IP address.

Either continue with [Virtual machine installation](#virtual-machine-installation) or, if installing a non-virtual runner, with [the `Install and configure` section](#install-and-configure) (below).

## Virtual machine installation

To create a new virtualized Gitlab runner for Windows builds, start by selecting a FreeBSD machine to host the (new) Windows VM. SSH into that machine and `su` to gain root access.

### Create Virtual Machine

Create a template file with settings for Windows VMs:

`cd /vm/.templates`

`cat > windows.conf`

Copy and paste the following text into the file (i.e. into your SSH session):

```shell
uefi="yes"
cpu=3
memory=3G
xhci_mouse="yes"
network0_type="virtio-net"
network0_switch="public"
disk0_type="ahci-hd"
disk0_name="disk0"
disk0_dev="sparse-zvol"
graphics="yes"
graphics_wait="yes"
```

Save the file by pressing `Ctrl+D`. If you're connecting from a Windows client, verify that it has correct line endings: `file windows.conf` should say something like `ASCII text`, without reporting `with CRLF line terminators`. Once the file is OK, create the VM using the name `win` and a 100 Gb (virtual) hard disk:

`vm create -t windows -s 100G win`

### Install Windows

Determine the HTTP URL of an ISO image containing the Windows version (preferably 64-bit) you want to install. You might want to use one [provided by C&CZ](http://install.science.ru.nl/science/MS%20Windows/). Then retrieve the image (replacing the URL by the one you're using):

`vm iso http://install.science.ru.nl/science/MS%20Windows/10/1803/SW_DVD5_Win_Pro_Ent_Edu_N_10_1803_64BIT_English_-2_MLF_X21-79647.iso`

Mount the image in the VM (replacing the disc image name by the one you just downloaded):

`vm install win SW_DVD5_Win_Pro_Ent_Edu_N_10_1803_64BIT_English_-2_MLF_X21-79647.iso`

The machine and/or Windows installation will be suspended until you connect to the VM's console. Use [TightVNC](https://www.tightvnc.com/download.php) Viewer to connect to `the-host-machine:5900`. The Windows installer will prompt you to press a key to boot from the mounted ISO. Press that key! Then complete the Windows installation wizard. Skip the network configuration for now, since we'll (need to) install a different (virtual) network card later.

The VM will reboot several times during the installation process, causing the (Tight)VNC connection to be closed. Reconnect every time until you're presented with the Windows desktop.

### Finalize Windows and VM configuration

Use TightVNC to shut down the VM and open an SSH console into the host machine. Download and mount the disc image that will allow you to install the "virtio" network drivers:

```shell
vm iso https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/stable-virtio/virtio-win.iso
vm install win virtio-win.iso
```

Reconnect to the VM using (Tight)VNC. (The machine may complain about not being able to boot from CD-ROM. Ignore this and wait for Windows to be started.) Log on.

Open Device Manager. (Windows Explorer -> right-click "This PC" -> select "Manage" from the drop-down menu -> Select "Device Manager" in the tree view.) It will have an "Ethernet Controller" that Windows warns about (yellow triangle). Open (double-click) this device and click the "Update Driver..." button. Select "Browse my computer for driver software", locate directory "D:\NetKVM" and select the subdirectory appropriate for your operating system (e.g. "w10" for Windows 10) and architecture (e.g. "amd64" for 64-bit Windows). Complete the wizard to install the driver. The VM will connect to the network and Windows will prompt whether to allow network discovery. Do not allow this.

Determine the machine's IP address, either in a Windows-y way or by executing `ipconfig /all` from a command prompt. Take note of this IP address, since you'll need it later to connect over RDP.

Allow Remote Desktop access to the machine. (Windows Explorer -> right-click "This PC" -> select "Properties" from the drop-down menu -> click "Remote settings" -> under "Remote Desktop", select "Allow remote connections to this computer" -> confirm.) Test whether the machine is accessible by connecting to the VM's IP address.

Once Remote Desktop is set up and working, shut down the VM one last time, SSH into the host machine and

- Edit file `/vm/win/win.conf`:
  - Remove setting `graphics_wait="yes"` to allow the machine to boot without waiting for a (VNC) client connection.
  - Remove setting `graphics="yes"` to disallow further (Tight)VNC connections to the machine.
- Edit file `/etc/rc.conf`:
  - Add the name of your VM `win` to the space-delimited value of `vm_list` to have the VM started automatically when the host machine starts. E.g. if the setting reads `vm_list="coreos"`, change it to `vm_list="coreos win"`.
- Restart the VM: `vm start win`.

Your VM is now ready to [install and configure](#install-and-configure).

## Install and configure

Connect to the machine using a Remote Desktop client. Install

- **Prometheus node exporter** according to the [installation procedure](https://github.com/prometheus-community/windows_exporter#installation).
  - Ensure you configure the exporter to listen on port `9100` (since other ports won't be accessible due to firewalling).

- **Gitlab runner** according to the [documented installation procedure](https://docs.gitlab.com/runner/install/windows.html). During Gitlab runner's registration process:
  - when prompted for the tags, do not copy them from existing Windows CI runners just yet to prevent CI jobs from being scheduled on this runner right away. Instead specify a (single) unique tag so the new runner's functioning can be tested from a branch (see below).
  - when prompted for the executor, specify `shell`.
  - [ensure that](<https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1231#note_16011>) the `gitlab-runner` service runs under a local user account (i.e. not `Local System`) with administrative privileges (i.e. not a `Default user`).
  - ensure that `gitlab-runner`'s user account is allowed to create symbolic links.

New build servers should be [provisioned using Chocolatey and Conan](#provisioned-runner-based-on-chocolateyconan). The installation of [legacy runners](#legacy-manually-installed-runner) is also still documented.

### Provisioned runner based on Chocolatey/Conan

The installation of Windows CI runners is mostly scripted in [a batch file provided from the `pep/ops` repository](https://gitlab.pep.cs.ru.nl/pep/ops/-/blob/master/config/windows-ci-runner.bat). The script currently requires two pieces of software to be installed by hand:

- Gitlab runner must be installed as described above. Ensure that the runner uses PowerShell for CI job execution. Newer software versions will [default to this](https://gitlab.com/gitlab-org/gitlab-runner/-/issues/91), but if you're using an older version of the software (not recommended), ensure that your `config.toml` specifies `shell = "powershell"`.
- Git must be installed as documented for [development boxes](windows-development-machine.md).

Further installation is scripted in [a batch file provided from the `pep/ops` repository](https://gitlab.pep.cs.ru.nl/pep/ops/-/blob/master/config/windows-ci-runner.bat). Either run the batch file manually on your new machine, or wait for the machine's first CI job to invoke the script and provision the machine for you.

Really PEPpy DevOps wizards will want to automate the installation of Git and Gitlab runner as well. So please include those installations in the provisioning script and update this documentation accordingly.

Continue with [the `Test` section (below)](#test).

### Legacy (manually installed) runner

This section describes how Windows CI runners should be installed for our legacy CI code, which was based on Windows (batch file/`cmd.exe`) scripting. Virtual machines `Windows10Builder` and `Windows10Builder_5` have been installed this way. Follow these instructions only if you need CI builds for a branch that hasn't been migrated to builds based on [Chocolatey/Conan](https://gitlab.pep.cs.ru.nl/pep/core/-/issues/#1231) and [PowerShell](https://gitlab.pep.cs.ru.nl/pep/core/-/issues/#1272) yet.

Newer Gitlab runner software versions will [default to using PowerShell](https://gitlab.com/gitlab-org/gitlab-runner/-/issues/91), but our legacy CI code is based on Windows batch files. If you're using an newer version of the software, ensure that your `config.toml` specifies `shell = "cmd"`. Note that [Gitlab has deprecated](https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1272) the use of the `cmd` shell, so *really* new versions may refuse to accept this configuration.

Install and configure the build server [as if it were a Windows dev box](windows-development-machine.md), but

- when editing settings, be sure to apply your changes to the entire machine (as opposed to only your user account): open Windows Explorer -> right-click `This PC` -> choose `Properties` from the drop-down menu -> click link `Advanced system settings` -> click button "Environment Variables...` ->
use the bottom half of the screen.
- you may choose to skip
  - installation of any software not used to build PEP Assessor.
  - installation of any software used in local development, such as GUI frontends for command line utilities.
  - configuration for local development, such as paths required for debugging.

Install the following additional software (which is required on CI build servers but not dev boxes):

- **QT Installer Framework 3.0**, to be selected during installation of QT.
- [**sed for Windows**](http://gnuwin32.sourceforge.net/packages/sed.htm).
- The [**Windows Installer XML (WiX) toolset**](http://wixtoolset.org/) which, at the time of writing, requires the .NET Framework 3.5, which can be installed as an optional feature in Windows 10.

Edit environment variables (for all users):

- Ensure that the following command line utilities are in the path (for all users):
  - MSBuild. E.g. you'd add `C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin` to the path when using Visual Studio 2017 installed to its default location.
  - sed. E.g. you'd add `C:\Program Files (x86)\GnuWin32\bin` to the path when sed is installed to its default location.
- Define environment variable `BOOST_LIBRARYDIR` to the directory where the Boost libraries for your toolchain are installed, e.g. `C:\local\boost_1_68_0\lib64-msvc-14.1` for Boost 1.68.0 for Visual Studio 2017 installed to its default location.

Confirm when you're done editing environment variables. Continue with [the `Test` section (below)](#test).

## Test

To ensure updated settings are picked up during the build, restart the gitlab runner service:

- either from the "Services" administrative tool,
- or from the command line: `net stop gitlab-runner` followed by `net start gitlab-runner`.

Both methods require administrative privileges.

Create a git branch of master. Within your branch, edit the `.gitlab_ci.yml` file so that

- the Windows build step is performed for commits on your (feature) branch.
- the Windows build step requires the tag you assigned to the new gitlab runner.
- other build steps are skipped (since we don't want to wait for those).

Commit the changes and push them to gitlab. The CI pipeline for your feature branch should perform the Windows build on the new runner. (If not, check the tag settings for the runner and the job.) Fix any issues that come up until the new runner successfully completes the Windows build.

Download the artifacts produced by your runner and compare them (using a diff utility) to the artifacts produced by another runner. Fix any significant discrepancies. Repeat until you're satisfied that your runner produces artifacts the same way that other runners do.

**Do not merge** your test/feature branch back to `master`. Instead delete it, both locally and from gitlab. Then [enable job scheduling](#enable-ci-job-scheduling) on your new machine.

## Enable CI job scheduling

Once the Windows runner can successfully perform the Windows build step(s) in your branch, allow it to pick jobs from other branches by [editing the runner's registration in gitlab](https://gitlab.pep.cs.ru.nl/pep/core/settings/ci_cd):

- Expand the `Runners` heading.
- Locate the new runner: it'll have the name you entered during the registration process.
- Remove the temporary test tag you added earlier.
- Add the tags required for the Windows build step, as defined in the `.gitlab_ci.yml` file.

Your runner is now ready to pick jobs for CI pipelines. Test one final time:

- Pause all other Windows runners in [gitlab](https://gitlab.pep.cs.ru.nl/pep/core/settings/ci_cd).
- Run a `master` or other pipeline. It should be picked up by your new runner. (If not, check the tag settings for the runner and the job.) Fix any issues that come up. Once successful:
- Resume the Windows runners you paused.

## Monitor the build server

Add the newly commissioned server to the [central Prometheus server configuration file](https://gitlab.pep.cs.ru.nl/pep/ops/-/blob/master/config/release/prometheus/prometheus.yml) to have it monitored. If you followed the [installation instructions](#install-and-configure), it should be running (and accessible) on port `9100`. (By default the node exporter exposes itself on port `9182`, which will be inaccessible due to firewalling.)
