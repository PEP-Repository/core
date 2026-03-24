# `base_integration_data`

`base_integration_data` contains data used to test if the current version of the software still works with an older database etc., by calling `integration.sh` with `--generated-data-dir ./tests/test_input/base_integration_data --reuse-secrets-and-data`.

When manual changes are required for a release, this data may need to be updated.

## Updating the data

1. Disable the watchdog stressor to prevent it from generating large binary files
   1. In `/config/integration/config/watchdog/config.yaml`, set `stressor.enabled` to `false`.
   2. In `integration.sh`, remove `-instant-stressor` options.
2. Run:
   ```shell
   sudo ./tests/integration.sh --local --build-dir ./build/Debug/ \
     --tests-to-skip s3-roundtrip \
     --generated-data-dir ./tests/test_input/base_integration_data
   ```
   This will replace the `base_integration_data` directory.
   We skip `s3-roundtrip` because it generates many large files.
   You may need to customize the build directory etc.
3. Own the files:
   ```shell
   sudo chown -R "$USER:$USER" ./tests/test_input/base_integration_data
   ```
4. Commit the new files (except for files in the `.gitignore`).
5. Revert changes you made regarding the watchdog stressor.
