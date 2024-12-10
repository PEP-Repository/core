# Releasing PEP binaries

## Update `master`

These steps should have been performed during the previous release and/or during environment updates. But just to be sure we have a complete and correct starting point:

1. [ ] In the `pep/core` project:
    1. [ ] Merge `release` to `stable`
    1. [ ] Merge `stable` to `master`
1. [ ] In the `pep/ops` project:
    1. [ ] Merge `release` to `stable`
    1. [ ] Merge `stable` to `master`

## Update `stable` code base

Update the `stable` branch with the latest `master` changes:

1. [ ] Either work on your local machine, **not pushing to `origin` until the last step**:
    1. [ ] In `pep/core`, merge from the `master` branch to the `stable` branch **without fast-forwarding** so that a merge commit is created. E.g.
        * [ ] Use `git merge --no-ff`.
        * [ ] Merge using SourceTree and check the box labeled `Create a new commit even if fast-forward is possible`.
    1. [ ] Use the merge commit to review all changes. Update the `stable` code base as needed.
    1. [ ] In the `stable` branch, edit the `CHANGELOG` file to document the changes for this release.
    1. [ ] When you're satisfied with the changes (i.e. the current `stable` code base on your local machine):
        1. [ ] Push your `stable` branch to `origin`.
        2. [ ] Merge your `stable` branch (back) to `master` and push to `origin`.
1. [ ] Or work in Gitlab('s Web interface), but note that your merge request will change whenever new code is committed to `master`:
    1. [ ] In `pep/core`, create a Gitlab merge request from the `master` branch to the `stable` branch.
    1. [ ] Use the merge request to review all changes. Update the `master` code base as needed.
    1. [ ] In the `master` branch, edit the `CHANGELOG` file to document the changes for this release.
    1. [ ] Push your local `master` changes to `origin` so that the merge request includes those changes.
    1. [ ] When you're satisfied with the merge request's contents (i.e. the current `master` code base on `origin`), merge it to update `pep/core`'s `stable` branch.

Updating the `stable` branch on `origin` will trigger

1. a `pep/core` CI pipeline for the `stable` branch, which will start
1. a `pep/ops` CI pipeline for the `stable` branch, which will `provide-binaries` by running
1. a `pep/core` pipeline for a temporary branch named `binaries_for_xxxxx` (where `xxxxx` is the ID of the parent pipeline in the `pep/ops` repository).

Let these pipelines run until the `pep/ops` pipeline stops at its `Unleash` stage.

## Update `stable` environment

1. [ ] Look up in the `CHANGELOG` whether or not changes to the `pep/ops` repository should be made, and update the `stable` branch's configuration as required. Push any changes to `origin`, triggering yet another CI pipeline that will stop at its `Unleash` stage. If no changes are required, do not trigger any new pipelines and proceed with the pipelines above.
1. [ ] Manually `Unleash` the `pep/ops` pipeline and let it run to completion to update the environment.
1. [ ] Perform own testing and fix any issues.
1. [ ] Let another team member (e.g. Joep) test, mentioning any issues requiring extra attention.
1. [ ] Merge changes back from `pep/ops`'s `stable` branch to `master` and push to origin. This will start a CI pipeline for the `master` branch.
1. [ ] Merge changes back from `pep/core`'s `stable` branch to `master` and push to origin. This will start a CI pipeline for the `master` branch.
1. [ ] Ensure that the CI pipelines for `master` succeed. When the `pep/ops` pipeline runs without failure up until `data-sync`, it is considered successful.

## Update `release`

1. [ ] In `pep/core`, merge `stable` to `release` **using fast-forwarding** so that no merge commit is created: e.g. use `git merge --ff-only`.
1. [ ] Push your updated `release` branch to origin, starting a CI pipeline in `pep/core` and a second one in `pep/ops`.
1. [ ] In the `pep/ops` repository, merge the `stable` branch into the `release` branch. Push changes to `origin`, triggering yet another CI pipeline for the `release` branch.
1. [ ] In the `pep/ops` repository, make any changes required (e.g. by the `CHANGELOG`) to the `release` branch. If you make such changes, push them to origin, triggering yet another CI pipeline for the `release` branch, then
    1. [ ] Merge changes to `stable` and push that branch to `origin`, triggering a CI pipeline for the `stable` branch.
    1. [ ] Merge changes to `master` and push that branch to `origin`, triggering a CI pipeline for the `master` branch.
1. [ ] In `pep/ops`, run the manual `publish-production-ops-services` CI job to publish new services (i.e. Docker images that will run on the `pep-release` VM).
1. [ ] If the logger image has changed (in significant ways), upgrade logger instances to the new image:
    1. On the `pep-release` VM.
    1. On the `ppp-logger.pep.cs.ru.nl` host (which is managed by @ronny).
1. [ ] Announce the new PEP release to project teams, allowing them to upgrade their environments to the new binary version. The PEP team [maintains instructions](https://gitlab.pep.cs.ru.nl/pep/core/-/blob/master/ci_cd/project-release-template.md) on how to upgrade project environments, and project teams can/should base their own tickets on this template.
