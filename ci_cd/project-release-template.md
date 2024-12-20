# Upgrading a project environment to a new PEP binary version

## Prepare

These steps should have been performed during the previous release. But just to be sure we have a complete and correct starting point, perform the following steps for the branch types that that exist in your project repository:

Your "production" branch.

- [ ] Merge it into:

Your "acceptance" branch.

- [ ] Merge it into:

Your "testing" branch.

- [ ] Merge it into:

Your "development" branch.

### Allow for hotfixes without upgrading

Then, if the PEP (binary) repository has updated its `release` branch to a version with new [prefab CI code](https://gitlab.pep.cs.ru.nl/pep/core/-/blob/master/ci_cd/pep-project-ci-logic.yml), CI pipelines in your project repository will fail because they'll detect [possibly inconsistent CI logic](https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2133). Concretely, you won't be able to run CI pipelines until your project's (acceptance and/or production) environment has been upgraded to the new PEP `release` version. This will a.o. prevent you from deploying configuration changes (e.g. hotfixes) to your customer environment. To allow your customer CI pipelines to run without upgrading them first, perform the following steps **only if prefab CI code has changed**:

- [ ] If `pep/core` prefab CI code has changed: in the `pep/core` repository, create a branch (e.g. named `previous-release`) for the commit on which your project's submodule is based.
- [ ] If `pep/ops` prefab CI code has changed (and your project includes it as a submodule): in the `pep/ops` repository, create a branch (e.g. named `previous-release`) for the commit on which your project's submodule is based.
- [ ] In your project repository's "production" branch, edit the `.gitlab-ci.yml` file's `include` section. For every file that has changed, update the `ref` to the branch name(s) you created (e.g. `previous-release`). (See [this commit](https://gitlab.pep.cs.ru.nl/pep/ppp-config/-/commit/28b3c69020276c9276a9422c2e25a6293fc9187e) for an example.) Commit and push to `origin`, triggering a CI pipeline whose `ci-yml-include-version` job should succeed.
- [ ] Merge your customer's "production" branch into the "acceptance" branch (and the "acceptance" branch into the "testing" branch, if it exists). Now it (they) too will be based on the `previous-release`, allowing CI pipelines to be run.

## Update acceptance or testing environment

The following steps are to be performed in the project repository's "acceptance" branch. If you have a "testing" branch, perform these steps there instead.

- [ ] If (and only if) you switched CI logic to a different branch as instructed in the `Prepare` section above: in your project repository's "acceptance" branch, revert the commit(s) that made this change. The "acceptance" branch's CI logic will now (once again) be based on the `release` version of PEP.
- [ ] In your project repository's "acceptance" branch, update your "FOSS" submodule to the desired commit. If you use default names and settings, from the git root directory:

```shell
cd pep
git checkout release
git pull
git submodule update --recursive --init
cd ..
git stage pep
git commit -m "Upgrade to latest PEP release version"
```

- [ ] In your project repository's "acceptance" branch, update your "DTAP" submodule to the commit corresponding to your "FOSS" submodule. If you use default names and settings, from the git root directory:

```shell
cd dtap
git checkout release
git pull
git submodule update --recursive --init
cd ..
git stage dtap
git commit -m "Upgrade to latest DTAP release version"
```

- [ ] Update your project's (acceptance) configuration according to the new release's requirements: see the PEP `CHANGELOG`.
- [ ] In the project root you find the file `version.json`. Increment the version numbers as required. If the pep binary release has no backward incompatible changes, increment the minor version. If the pep binary does have backward incompatible changes, discuss with the team whether or not this requires a major version increment.
- [ ] Push the "acceptance" branch to `origin`, triggering a Gitlab CI pipeline that will update your acceptance environment to the new version.
- [ ] Perform own testing and fix any issues.
- [ ] Have the project team accept the updated acceptance version. Your project repository's documentation should list the people to contact, e.g. in a `docs/private/contact-info.md` file.
- [ ] If these steps were performed in the "testing" branch, merge your "testing" branch back into your "acceptance" branch.

## Update production environment

Ensure that the update of the production environment is coordinated with the people using it. Your project repository's documentation should list the people to contact, e.g. in a `docs/private/contact-info.md` file.

- [ ] Notify users of any changes that may be of interest to them. Ensure that any breaking changes are communicated beforehand, so that users can prepare for them.
- [ ] Merge your project repository's "acceptance" branch into its "production" branch. If you use default names and settings, you'll merge `acc` into `prod`.
- [ ] Update your project's (production) configuration according to the new release's requirements: see the PEP `CHANGELOG`.
- [ ] Push the "production" branch to `origin`, triggering a Gitlab CI pipeline that will stop at the `Unleash` stage.
- [ ] Notify users that the environment will be down for maintenance.
- [ ] Run the `confirm-manually` CI job to have the pipeline proceed and update your production environment to the new version.
    - [ ] If necessary, publish a new version of the Windows software by manually running the `publish-windows-installer` job. **Only skip this step if you're sure that the old version will suffice.**
- [ ] Perform own testing and fix any issues.
- [ ] Have users verify ASAP that the environment works. If things don't work, fix them immediately. If things cannot be fixed, roll back to the previous release code base:
    - [ ] Either (preferably) republish the artifacts produced by the previous correct release build (e.g. the weekly rebuild),
    - [ ] Or perform a hard reset on the `prod` branch, force push it to gitlab and publish the resulting build.

## Wrap up

- [ ] Merge your "production" branch back into your "acceptance" branch.
- [ ] Merge your "acceptance" branch back into your "master" branch.
