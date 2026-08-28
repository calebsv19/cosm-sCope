# DataLab Main Edit Worktree

The persistent Main Edit lane is the default single-writer integration lane
for ongoing `datalab` / sCope functional development.

## Lane Identity

- canonical branch: `main`
- Main Edit branch: `codex/datalab-main-edit`
- worktree convention: `<workspace>/_worktrees/datalab_main_edit`
- development app: `sCope Main Edit.app`
- bundle ID: `com.cosm.scope.main-edit`
- runtime/log namespace: `DataLab-Main-Edit`
- identity schema: `codework_local_development_build_identity_v1`

The program `VERSION`, canonical `com.cosm.scope` bundle identifier, public
package identities, and private Linux candidate remain unchanged.

## Start And Checkpoint Gates

```sh
git -C <repo> status --short
git -C <repo> worktree list --porcelain
git -C <repo> rev-list --left-right --count main...codex/datalab-main-edit
make -C <workspace>/_worktrees/datalab_main_edit
make -C <workspace>/_worktrees/datalab_main_edit main-edit-package-contract-checks
make -C <workspace>/_worktrees/datalab_main_edit test-stable
make -C <workspace>/_worktrees/datalab_main_edit run-headless-smoke
make -C <workspace>/_worktrees/datalab_main_edit package-desktop-self-test
make -C <workspace>/_worktrees/datalab_main_edit package-desktop-main-edit-self-test
git -C <workspace>/_worktrees/datalab_main_edit diff --check
```

Before editing, read every registered worktree and stop on unexpected ownership,
dirty source paths, canonical product drift, or a second writer. The existing
image-pipeline specialist checkout and its generated build outputs are separate
ownership and must not be reset, cleaned, repurposed, or absorbed implicitly.

## Integration And Retention

Classify canonical-only commits and reconcile them into Main Edit before
adoption. Fast-forward only when canonical remains an ancestor of the verified
Main Edit tip. Retain the named lane by default; recycling requires a separate
cleanliness, reachability, ignored-artifact, and process-ownership decision.

Source adoption does not authorize a version change, release artifact,
Registry mutation, publication, deployment, push, Desktop-app replacement,
remote Linux work, or worktree cleanup.
