# n2k_engine_fw

HALMET firmware that sends engine temperatures on NMEA 2000. Start with [README.md](README.md); the design is in [docs/SPEC.md](docs/SPEC.md).

## Rules

- **Commits straight to `main` until the first release tag;** after that, pull requests only (branch from `main`, one topic per PR, CI must pass; the assistant opens PRs, the owner reviews and merges, the assistant never approves, merges or pushes to `main`).
- **Conventional Commits:** `type: summary` (`feat`, `fix`, `docs`, `test`, `ci`, `build`, `refactor`, `perf`, `chore`). PRs are squash-merged, so the PR title is the commit on `main`.
- **Tests with behaviour.** New or fixed behaviour adds host tests where it can.
- **Standalone product.** Don't reference the owner's other repos or projects.
- **Stay general.** Public product: no boat names, hostnames or boat-specific details in docs, examples, test data or screenshots. "Tested on a boat" is fine.
- **No AI attribution** in commits or PRs (no Co-Authored-By lines).
- **Never guess** PGN fields, specs or library behaviour; check the source (canboat, the NMEA2000 library, the device) or say it's unknown. Mark unchecked claims in the SPEC **(verify)**.
- **Search the whole repo** for related references when changing something.
- **Knowledge goes in the repo,** not the assistant's local memory: the owner works on several machines.
- Code from the HALMET example firmware is Apache-2.0: keep its copyright notices.
