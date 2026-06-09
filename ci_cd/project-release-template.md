# Upgrading a project environment to a new PEP binary version

## Prepare

These steps should have been performed during the previous release. But just to be sure we have a complete and correct starting point, perform the following steps for the branch types that exist in your project repository:

Your "production" branch.

- [ ] Merge it into:

Your "acceptance" branch.

- [ ] Merge it into:

Your "testing" branch.

- [ ] Merge it into:

Your "development" branch.

## Update development branch

The following steps are to be performed in the project repository's "development" branch unless you have specific reasons why changes should not yet be merged from development to other branches.

- [ ] Determine the PEP binary version that you want to base your project on (usually the latest supported release). The `pep/core` and `pep/ops` repositories use the branch name `release-X.Y` to maintain this version, where `X` is the major version number and `Y` the minor. When these instructions refer to a `release-X.Y` branch or `X.Y` text, replace the text by the actual branch name.
- [ ] Upgrade the PEP version that your project repository's "development" branch depends on:
  - [ ] Update your "FOSS" submodule to the new release. If you use default names and settings, from the git root directory:

    ```shell
    cd pep
    git fetch
    git checkout release-X.Y
    git submodule update --recursive --init
    cd ..
    git stage pep
    ```

  - [ ] Update your "DTAP" submodule to the new release. If you use default names and settings, from the git root directory:

    ```shell
    cd dtap
    git fetch
    git checkout release-X.Y
    git submodule update --recursive --init
    cd ..
    git stage dtap
    ```

  - [ ] Edit your project's `.gitlab-ci.yml` file. If it `include`s prefab CI files from `pep/core` and/or `pep/ops`, update the `ref`s of those `include`s to the new release branch name, i.e. `release-X.Y`.
  - [ ] Commit the upgraded submodules and CI logic:

    ```shell
    git commit -m "Upgraded to PEP release X.Y"
    ```

- [ ] Edit your project's `version.json` file and increment the version numbers as required. If the pep binary release has no backward incompatible changes, increment the minor version. If the pep binary does have backward incompatible changes, discuss with the team whether or not this requires a major version increment. In general you can follow the same version as the pep binary release, but you may choose to deviate from this if your project has its own versioning scheme.
- [ ] Update your project's configuration according to the new release's requirements: see the PEP `CHANGELOG`.

## Update testing and/or acceptance environment

- [ ] Merge your "development" branch into your "testing" branch (if it exists), and push to origin.
- [ ] Perform own testing and fix any issues.
- [ ] Merge your "testing" branch (if it exists, else "development" branch) into your "acceptance" branch. Push the "acceptance" branch to `origin`, triggering a Gitlab CI pipeline that will update your acceptance environment to the new version.
- [ ] Have the project team accept the updated acceptance version. Your project repository's documentation should list the people to contact, e.g. in a `docs/private/contact-info.md` file.

## Update production environment

Ensure that the update of the production environment is coordinated with the people using it. Your project repository's documentation should list the people to contact, e.g. in a `docs/private/contact-info.md` file.

- [ ] Notify users of any changes that may be of interest to them. Ensure that any breaking changes are communicated beforehand, so that users can prepare for them.
- [ ] Merge your project repository's "acceptance" branch into its "production" branch. If you use default names and settings, you'll merge `acc` into `prod`.
- [ ] Update your project's (production) configuration according to the new release's requirements: see the PEP `CHANGELOG`.
- [ ] Push the "production" branch to `origin`, triggering a Gitlab CI pipeline that will stop at the `Unleash` stage.
- [ ] Notify users that the environment will be down for maintenance.
- [ ] Run the `confirm-manually` CI job to have the pipeline proceed and update your production environment to the new version.
  - [ ] If necessary, publish new versions of the Windows, MacOS and Flatpak software by manually running the appropriate job(s) in the `publish` stage. **Only skip this step if you're sure that the old version will suffice.**
- [ ] Perform own testing and fix any issues.
- [ ] Have users verify ASAP that the environment works. If things don't work, fix them immediately. If things cannot be fixed, roll back to the previous release code base:
  - [ ] Either (preferably) republish the artifacts produced by the previous correct release build (e.g. the weekly rebuild),
  - [ ] Or perform a hard reset on the `prod` branch, force push it to gitlab and publish the resulting build.

## Wrap up

Your "production" branch.

- [ ] Merge it into:

Your "acceptance" branch.

- [ ] Merge it into:

Your "testing" branch.

- [ ] Merge it into:

Your "development" branch.
