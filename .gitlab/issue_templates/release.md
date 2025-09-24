# Releasing PEP binaries

## Update `master`

These steps should have been performed during previous release(s) and/or during environment updates. But just to be sure we have a complete and correct starting point:

1. [ ] In the `pep/core` project:
   1. [ ] Merge the `latest-release` branch into the corresponding `release-X.Y` branch.
   1. [ ] Merge all (supported) release branches to `stable`
   1. [ ] Merge `stable` to `master`
   1. [ ] In the `master` branch, ensure that the version in file `version.json` is greater than the version number of the previous release. E.g. if the previous release was "46.2", the `version.json` file in the `master` branch should have been set to "46.3" during the previous release cycle. If it wasn't, do so now (or increase the major version as described below).
1. [ ] In the `pep/ops` project:
   1. [ ] Merge the `latest-release` branch into the corresponding `release-X.Y` branch.
   1. [ ] Merge all (supported) release branches to `stable`
   1. [ ] Merge `stable` to `master`
1. [ ] In the `pep/docker-build` project:
   1. [ ] Merge the `latest-release` branch into the corresponding `release-X.Y` branch.
   1. [ ] Merge all (supported) release branches to `stable`
   1. [ ] Merge `stable` to `master`

## Version number for the new release

1. [ ] Determine if the new release will constitute a minor version update (as the provisional version number in `version.json` indicates in the `master` branch). You'll probably need/want to
   - check the `CHANGELOG` for breaking changes, and/or
   - compare the current `master` code base to the previous release's, and/or
   - check whether the network protocol (checksum) has changed since the previous release, and/or
   - discuss with the team whether the new release's feature set warrants a major version increase.
1. [ ] If the new release should be a major version update, edit the `version.json` file in the `master` branch and update the values accordingly. E.g. if the `version.json` file currently specifies version "46.3", update it so it specifies version "47.0" instead.

The name for Git(lab) branches for the new release will be `release-X.Y`, where `X` is the new release's major version (number) and `Y` is its minor version (number). When later instructions specify a branch name of `release-X.Y`, replace the `X` and `Y` by the appropriate values for this release.

## Create and update branches

1. [ ] In the `pep/core` repository, create a branch named `release-X.Y` based on the `master` branch.
1. [ ] In the newly created `release-X.Y` branch, edit the `CHANGELOG` file so that the `Changes in upcoming release` are moved to an appropriately named (and positioned) `Release X.Y` section. The `Changes in upcoming release` section near the top of the document should be empty afterwards. Update the `Changes in upcoming release` header to the next (future) release's version number.
1. [ ] Merge the changes back from your `release-X.Y` branch to `master`.
1. [ ] In **the `master` branch** of the `pep/core` repository, edit the `version.json` file and increase the _minor_ version number by one. E.g. if the `version.json` file currently specifies version "46.2", update it to "46.3". This will be the (provisional) version number for the next (future) release, i.e. the release _after_ the one we're currently preparing.
1. [ ] In the `pep/core` repository, push your new `release-X.Y` branch and the updated `master` branch to `origin`, triggering CI pipelines in `pep/core` and followup pipelines in other repositories. One or both of the pipelines in the `pep/ops` repository will fail their `versioning-consistency` job, so:
1. [ ] In the `pep/ops` repository, update the `version.json` file to the appropriate values where needed:
   1. [ ] in the `master` branch (which will definitely have failed because of the version number change).
   1. [ ] in the `release-X.Y` branch (which may have failed depending on commit/push timing and whether you're releasing a major or minor version).
1. [ ] In the `pep/ops` repository, push the updated branch(es) `release-X.Y` and/or `master` to `origin`, triggering CI pipelines. Ensure that these pipelines succeed.
1. [ ] In the `pep/docker-build` repository, create a branch named `release-X.Y` based on the current `master` branch. Push the new branch to `origin`, triggering a CI pipeline in `pep/docker-build` and (a) followup pipeline(s) elsewhere. Ensure that these pipelines succeed.

## Update `stable` code base

**TIP**: You'll be reviewing all changes since the last release, which can amount to quite a lot of diffs. If there are large changes that don't need much attention (such as search-and-replace renames), it may be helpful to isolate these from other changes before reviewing.

Update the `stable` branch with the changes from the new `release-X.Y` branch:

1. [ ] Either work on your local machine, **not pushing to `origin` until the last step**:
   1. [ ] In `pep/core`, merge from the `release-X.Y` branch to the `stable` branch **without fast-forwarding** so that a merge commit is created. E.g.
      * [ ] Use `git merge --no-ff`.
      * [ ] Merge using SourceTree and check the box labeled `Create a new commit even if fast-forward is possible`.
   1. [ ] Use the merge commit to review all changes. Update the `stable` code base as needed.
   1. [ ] When you're satisfied with the changes (i.e. the current `stable` code base on your local machine):
      - [ ] merge any `stable` changes back to the `release-X.Y` branch.
      - [ ] push the `stable` and `release-X.Y` branches to `origin`.
1. [ ] Or work in Gitlab('s Web interface):
   1. [ ] In `pep/core`, create a Gitlab merge request from the `release-X.Y` branch to the `stable` branch.
   1. [ ] Use the merge request to review all changes. Update the `release-X.Y` code base locally as needed, pushing each change to `origin` so that the merge request includes those changes.
   1. [ ] When you're satisfied with the merge request's contents (i.e. the current `release-X.Y` code base on `origin`), merge it to update `pep/core`'s `stable` branch.

Once you've updated the `stable` branch on `origin`:

1. [ ] In the `pep/core` repository, a CI pipeline will run for the `stable` branch, which will trigger a followup `pep/ops` pipeline, which may trigger a followup `pep/core` pipeline for a temporary branch named `binaries_for_xxxxx` (where `xxxxx` is the ID of the parent pipeline in the `pep/ops` repository). Let these pipelines run to completion.
1. [ ] In the `pep/core` repository, merge the `stable` branch to `master` and push to `origin`, triggering a CI pipeline in `pep/core` and a followup pipeline in `pep/ops`. Ensure that these pipelines succeed.

## Update `stable` environment

1. [ ] In the `pep/ops` repository, merge the `release-X.Y` branch to the `stable` branch.
1. [ ] Look up in the `CHANGELOG` whether or not changes to the `pep/ops` repository should be made, and update the `stable` branch's configuration as required.
1. [ ] In the `pep/ops` repository's `stable` branch, edit the `config/stable/pep-monitoring/watchdog/config.yaml` file and
   1. add appropriate entries to the `checkPipelines` section make the (stable) watchdog monitor the newly created release branch(es).
   1. remove the configuration for the oldest release's branch(es) from the `checkPipelines` section to stop monitoring the release that we will no longer support.
1. [ ] Push the updated `stable` branch to `origin`, triggering a new `stable` CI pipeline in `pep/ops` that will update the environment.
1. [ ] Merge changes back from `pep/ops`'s `stable` branch to `master` and push to origin. This will start a `pep/ops` CI pipeline for the `master` branch. Ensure that this pipeline succeeds. (If the pipeline runs without failure up until `data-sync`, it is considered successful.)
1. [ ] In the `pep/docker-build` repository, merge the `release-X.Y` branch to the `stable` branch. Ensure that these pipelines succeed.
1. [ ] Perform own testing on the `stable` environment and fix any issues.
1. [ ] Let another team member (e.g. Joep) test, mentioning any issues requiring extra attention.

## Create scheduled pipelines

1. [ ] Log in to gitlab using [pep-support's credentials](https://gitlab.pep.cs.ru.nl/pep/ops/-/blob/master/passwords/gitlab-pepsupport.txt?ref_type=heads). (If you're already logged in to gitlab under a personal account, you'll need to use a second browser or an incognito tab or window.)
1. [ ] Go to Build -> Pipeline schedules and create a new schedule for the new `release-X.Y` branch. Schedule it every week on sunday (e.g. `0 4 * * 0`, see: <https://docs.gitlab.com/ee/topics/cron/>). Make sure to stagger the scheduled pipelines by an hour.
   1. [ ] in the `pep/core` repository. Set the `BUILD_ALL_TARGETS` variable to `yes`.
   1. [ ] in the `pep/docker-build` repository.
1. [ ] In general we'll only support the release that we just created, plus the previous one (to allow projects to keep running that version for a while, until they get a chance to update). If (a) pipeline schedule(s) exist(s) for an older release that will now become unsupported, remove that schedule
   1. [ ] from the `pep/core` repository.
   1. [ ] from the `pep/docker-build` repository.
1. [ ] Log `pep-support` out of gitlab.

## Wrap up

1. [ ] In the `pep/core` repository, merge the new `release-X.Y` branch to the `latest-release` branch. It will start a followup pipeline in `pep/ops` pipeline that will fail.
1. [ ] In the `pep/ops` repository, merge the new `release-X.Y` branch to the `latest-release` branch. It will start a pipeline that will stop at the manual job in its `Unleash` stage.
1. [ ] In the `pep/docker-build` repository, merge the `release-X.Y` into the current `latest-release` branch. Push the changes to `origin`, triggering a CI pipeline in `pep/docker-build` and (a) followup pipeline(s) elsewhere.
1. [ ] In the `pep/ops` repository, if the new release updates `ops`-related services in significant ways, consider deploying the new version(s), but ensure that (all) running projects can still interface with the updated service(s):
   1. [ ] Manually `Unleash` the `pep/ops` pipeline for the `latest-release` branch and let it run to completion to update services on the `pep-release` VM.
   1. [ ] If applicable, ensure that the new logger image is used on the `ppp-logger.pep.cs.ru.nl` host (which is managed by Ronny).
1. [ ] In the **`release-X.Y`** pipelines, update the release documentation by running the following manual jobs:
   - [ ] `publish-public-docs` and `publish-private-docs` in `pep/ops`.
   - [ ] `publish-public-docs` in `pep/core`.
1. [ ] Announce the new PEP release to project teams, allowing them to upgrade their environments to the new binary version. The PEP team [maintains instructions](https://gitlab.pep.cs.ru.nl/pep/core/-/blob/master/ci_cd/project-release-template.md) on how to upgrade project environments, and project teams can/should base their own tickets on this template.
