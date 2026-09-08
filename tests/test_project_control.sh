#!/usr/bin/env sh
set -eu

fail() {
  printf 'project control: %s\n' "$1" >&2
  exit 1
}

require_file() {
  test -f "$1" || fail "missing file: $1"
}

require_text() {
  grep -nF -- "$2" "$1" >/dev/null || fail "$1 missing required text: $2"
}

roadmap=ROADMAP.md
required_files="
$roadmap
CONTRIBUTING.md
docs/decisions/README.md
docs/decisions/0001-public-project-control.md
docs/development/agentic-engineering.md
.github/ISSUE_TEMPLATE/bug_report.yml
.github/ISSUE_TEMPLATE/engineering_change.yml
.github/ISSUE_TEMPLATE/config.yml
.github/pull_request_template.md
"
for file in $required_files
do
  require_file "$file"
done

test ! -e PROJECT.md || fail 'retired PROJECT.md exists'
test ! -d docs/milestones || fail 'retired milestone plans remain in the current tree'

require_text "$roadmap" 'Status: living public project control'
require_text "$roadmap" 'This file is the sole live authority'
require_text "$roadmap" 'Active Next:'
# One parser owns roadmap topology, derived counts, temporal relations and
# release-flag consistency. A line ceiling cannot qualify information ownership.
python3 tests/documentation_architecture.py --roadmap-only

all_active_files=$(git ls-files --cached --others --exclude-standard -- '*.md' | while IFS= read -r file; do
  test -f "$file" || continue
  grep -l '^Active Next: ' "$file" || :
done | LC_ALL=C sort)
test "$all_active_files" = 'ROADMAP.md' ||
  fail "Active Next exists outside ROADMAP.md: $all_active_files"

require_text CONTRIBUTING.md '## Before opening work'
require_text CONTRIBUTING.md '## Development order'
require_text CONTRIBUTING.md '## Tests'
require_text CONTRIBUTING.md '## Commit and pull request'
require_text CONTRIBUTING.md 'ROADMAP.md'
require_text docs/decisions/README.md 'Current macro state remains in'
require_text docs/decisions/0001-public-project-control.md '## Decision'
require_text docs/development/agentic-engineering.md 'The only live macro project-control surface'

issue_count=$(find .github/ISSUE_TEMPLATE -maxdepth 1 -type f -name '*.yml' | wc -l | tr -d ' ')
test "$issue_count" -eq 3 || fail "unexpected issue-template count: $issue_count"
require_text .github/ISSUE_TEMPLATE/config.yml 'blank_issues_enabled: false'
require_text .github/pull_request_template.md '## Claims and progression'

printf 'project control: ok\n'
