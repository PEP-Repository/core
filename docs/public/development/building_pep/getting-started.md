# Getting started

If you're reading this wiki you have access to the to the PEP git repository. Step one in development is to clone the git repository to your local development machine. Before you do this you will need to register an ssh public key with the gitlab which can be found [here](https://gitlab.pep.cs.ru.nl/help/user/ssh.md).  
Once you've registered your ssh key you can issue:

```shell
git clone git@gitlab.pep.cs.ru.nl:pep/core.git
```

inside the directory you want to work in. This will clone the code into a sub directory called 'core'.

## Git

Git is the version control system in use in pep. The git developers have tutorial available [here](https://git-scm.com/book/en/v2/Getting-Started-About-Version-Control).  
The primary commands you will find yourself using are pull, add, commit and push. In general you pull code from the repository, you add new or changed items to your local tree, commit your changes with a message and push them back to the repository.

## The code

Within the core project there are a number of code directories. At the time of writing these are accessmanager, android, core, ext, hsm-firmware, jni, keyserver, libpepserver, nacl_sha256, storagefacility, and transcryptor.  
The core directory is where most of the code exists. Header files are found in core/include and source files in core/src. If you're new to the code base the header files are easier to read and make for a friendlier introduction point.
