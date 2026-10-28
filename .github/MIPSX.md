# CI on the MIPSX fork

Branch `mipsx` carries the experimental MIPSX target (the ES3890 in the
Linksys SPA3000) as a series of `[MIPSX]` commits on an LLVM release tag.
Its CI is `.github/workflows/mipsx.yml`:

- builds Clang and lld with only MIPSX (`LLVM_TARGETS_TO_BUILD=""`,
  `LLVM_EXPERIMENTAL_TARGETS_TO_BUILD=MIPSX`), through ccache kept in the
  Actions cache;
- checks out mipsx-sdk and picolibc (branch `mipsx`), runs the SDK's
  `make check`, builds the run-time sysroot with picolibc (`make libc`)
  and runs `make check-llvm`;
- runs the MIPSX lit tests in llvm, clang and lld, with `MIPSX_SDK` set so
  that the execution tests in `llvm/test/CodeGen/MIPSX/sim` and
  `clang/test/CodeGen/MIPSX/sim` run on the simulator (the job fails if
  they are reported unsupported);
- on a manual run, optionally `check-llvm check-clang check-lld` and the
  SDK's builtins and GCC torture suites.

Repository variables `MIPSX_SDK_REPO`, `MIPSX_SDK_REF`, `PICOLIBC_REPO`
and `PICOLIBC_REF` override the default `<owner>/mipsx-sdk` (`main`) and
`<owner>/picolibc` (`mipsx`). Release archives are built by mipsx-sdk's
`release.yml`, not here.

## Upstream workflows

The other files in `.github/workflows` come from upstream and stay
unchanged, so that the series rebases onto a new release without
conflicts there (do not delete or edit them on this branch). On a fork
they are almost all inert:

- most jobs have `if: github.repository == 'llvm/llvm-project'` (or the
  `llvm` owner) and are skipped;
- most push and pull-request triggers are limited to `main` and
  `release/*` branches, so pushes to `mipsx` do not start them;
- scheduled workflows run only on the default branch; GitHub also turns
  scheduled workflows off in forks until they are enabled.

The few that are not guarded (for example `gha-codeql.yml`,
`release-binaries-all.yml`, `hlsl-matrix.yaml`, `issue-write.yml`) can
still start on a schedule or a pull request to `main`. Turn every
workflow except MIPSX off in the repository instead of in the tree; the
setting is per repository and survives rebases:

```
gh workflow list --repo <owner>/llvm-project --all --json name,state \
  --jq '.[] | select(.name != "MIPSX" and .state == "active") | .name' |
while read -r name; do
  gh workflow disable "$name" --repo <owner>/llvm-project
done
```

Run it again after a rebase brings in new upstream workflows. Workflows
appear in this list only once GitHub has seen them on the default
branch, so set the default branch to `mipsx` first (Settings, General).
