# CI pipelines

## Stopping early on failures

To give quick feedback, pipelines for feature branches stop early when something fails:

- the pipeline is canceled as soon as one of its jobs fails;
- a build stops at the first compile error, instead of continuing to compile everything else (as Ninja's `-k 0` does).

Some pipelines always run and build everything, so that they report every problem:

- pipelines for `main`, `stable`, `release-X.Y` and hotfix branches;
- pipelines for ready (non-draft) MRs;
- pipelines run with the `autolinting` input set to `linted`.

Jobs with the linter enabled also always compile everything, so that they report all linter findings.

### Changing this for your feature branch

Two CI/CD variables control this for feature branch pipelines:

| Variable | Default | Effect |
| --- | --- | --- |
| `AUTO_CANCEL_ON_JOB_FAILURE` | `yes` | Set to `no` to let the pipeline run all jobs, even when one fails |
| `BUILD_KEEP_GOING` | `no` | Set to `yes` to keep compiling after a compile error, to see all errors at once |

Set them:

- when starting a pipeline by hand, under *Build* > *Pipelines* > *New pipeline*;
- with a push option, for a single push:

  ```shell
  git push -o ci.variable="AUTO_CANCEL_ON_JOB_FAILURE=no" -o ci.variable="BUILD_KEEP_GOING=yes"
  ```

- or for every push from your clone:

  ```shell
  git config --add push.pushOption "ci.variable=AUTO_CANCEL_ON_JOB_FAILURE=no"
  git config --add push.pushOption "ci.variable=BUILD_KEEP_GOING=yes"
  ```

  Remove them again with `git config --unset-all push.pushOption`.
  Don't use `--global` for these: pushes to servers that don't support push options, such as GitHub, would fail.

Note:

- GitLab doesn't pass push options to MR pipelines. When your branch has an open MR, the push options therefore have no effect: start a pipeline by hand instead.
- Setting CI/CD variables requires a GitLab role that is allowed to use pipeline variables. If GitLab rejects your push or pipeline, ask a maintainer.
- Windows builds use MSBuild, which always continues compiling after an error: `BUILD_KEEP_GOING` doesn't affect them.
