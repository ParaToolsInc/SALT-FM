# Agent Instructions for SALT-FM

Keep the diff as small, DRY and YAGNI as possible.
Re-read relevant files after each prompt and preserve user edits and comments.
Before finishing, check your work and point out anything the user may not have considered.
Use American English spelling in code, comments, docs and commit messages.
Update `README.md` when the behavior it describes changes, and note user-visible changes under `## [Unreleased]` in `CHANGELOG.md`.
Write code test-first, red-green-refactor: add a failing test and see it fail, write the least code to pass it, then refactor with the suite green; only push green commits.
Tests are ctest cases defined in `CMakeLists.txt`, with sources and fixtures under `tests/`.
Mirror CI locally: run the jobs in `.github/workflows/` with the same commands and flags. The Linux matrix (`CI.yaml`, LLVM 19-22 with TAU in `salt-dev` containers) needs Docker; without it, say it was not reproduced.

## Git

- Inspect `git diff` and keep it focused.
- Never commit to `master`. Branch from `origin/master` with a short, relevant name.
- Fold related fixes into the commit they fix and update its message rather than adding a follow-up commit, until the branch is pushed; after that, fix in a new commit and never rewrite pushed commits.
- Pass multiline messages through `git commit -F` rather than literal `\n`.
- Open pull requests but never merge them; the user reviews and merges.

## Commit Message Format

- Conventional Commits: `type(scope): summary`, e.g. `fix(cmake): anchor compiler-classifier regex`.
- Keep the subject, prefix included, under 51 characters.
- When a body is useful, use a dash list with lines under 73 characters focused on why and wrap filenames, code and identifiers in backticks.
- Reference issues with `Closes #123` in the body if applicable.

## CI Failures

- Fetch the complete log with `gh`.
- Reproduce failures locally before fixing when possible: Linux failures in the `salt-dev` Docker containers CI uses.
- Some failures can't be reproduced locally: race conditions, network glitches, or an OS, hardware, or compiler you don't have (e.g. Apple silicon). Re-run the job once first; if it passes, report it as flaky. Otherwise say so, explain the likely cause from the log, and fix from the log.

## AI Attribution

- Never add `Co-Authored-By` or other AI attribution trailers to commits or pull requests and never GPG-sign agent commits.
- Do not identify an AI tool as an author, co-author, committer or signatory of a commit, including through an `Assisted-by`, `Co-developed-by` or similar commit trailer.

## Shell

- Use `&>/dev/null` instead of `>/dev/null 2>&1`.
- Put a comment immediately above each `shellcheck disable` explaining why it is needed.
