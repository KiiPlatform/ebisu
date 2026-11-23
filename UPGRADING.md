# Upgrading

What an application, or the build that packages ebisu, has to change or check
when it moves to a newer 2.x release. Fixes and additions that ask nothing of
an application are left to each version's
[release notes](https://github.com/KiiPlatform/ebisu/releases).

## From 2.0.0 to 2.0.x

### Relink: the libraries' soname has the minor version

2.0.0's shared libraries were `libkii.so.2` and `libtio.so.2`, with the major
version alone, and `libkhc.so` and `libjkii.so`, with none. From 2.0.1 all four
carry the major and the minor: `libkii.so.2.0`, `libtio.so.2.0`,
`libkhc.so.2.0` and `libjkii.so.2.0`. The files themselves are
`lib<name>.so.2.0.<patch>`, and `lib<name>.so` stays as the link to build
against.

A program linked against 2.0.0's libraries doesn't load 2.0.1's: build it again
against them. No link is installed under 2.0.0's names.

From now on:

- Every 2.0.x keeps the ABI of 2.0.1: a program built against one runs with
  another's libraries, unchanged.
- A 2.1 may change the ABI, as it may add to the structs an application
  allocates, and so has another soname, `.so.2.1`. It keeps 2.0's API: what
  builds against 2.0 builds against 2.1.

### Check: where the version comes from

2.0.0 took its version, and the soname from it, from `git describe --tags`
wherever it was built. Now:

- **In ebisu's own clone, worktree or submodule**, it still comes from
  `git describe --tags`, and the build configures again when a commit, a
  checkout or a tag changes what that says.
- **From a source archive**, such as GitHub's tarball of a release, it is the
  release's version. 2.0.0 built one as 0.0.0.
- **Copied into another project's repository**, ebisu no longer takes that
  project's tags for its own: it builds as 0.0.0, with a warning saying why.
  To give it a version there, commit a `version/VERSION` file.
- **`version/VERSION`**, when there is one, says the version over git and an
  archive. It holds one line, the version as `2.0.1`, which may have a `v`
  before it and a suffix such as `-rc.1` after it. Anything else fails the
  configure. ebisu's repository has none.
