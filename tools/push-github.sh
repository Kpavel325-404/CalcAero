#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
url="${1:-}"
if [[ -z "$url" ]]; then
  read -r -p 'GitHub repository HTTPS URL: ' url
fi
if [[ ! "$url" =~ ^https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+/?$ ]]; then
  printf '%s\n' 'Expected https://github.com/OWNER/REPOSITORY (no credentials in URL).' >&2
  exit 1
fi
if [[ "$(git branch --show-current)" != main || -n "$(git status --porcelain)" ]]; then
  printf '%s\n' 'Start from the clean main branch.' >&2
  exit 1
fi
if git remote get-url origin >/dev/null 2>&1; then
  existing="$(git remote get-url origin)"
  if [[ "${existing%.git}" != "${url%.git}" ]]; then
    printf '%s\n' 'Existing origin differs. Inspect it before changing the remote.' >&2
    exit 1
  fi
else
  git remote add origin "$url"
fi
git push --set-upstream origin main
if git show-ref --verify --quiet refs/tags/v1.3.1; then
  git push origin v1.3.1
else
  printf '%s\n' 'main pushed. Create v1.3.1 on the intended commit in GitHub Releases.'
fi
