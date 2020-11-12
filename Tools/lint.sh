#! /bin/bash
#
# Linter script that checks for common style issues in Dolphin's codebase.

set -euo pipefail

if ! [ -x "$(command -v git)" ]; then
  echo >&2 "error: git is not installed"
  exit 1
fi

REQUIRED_CLANG_FORMAT_MAJOR=9
REQUIRED_CLANG_FORMAT_MINOR=0
CLANG_FORMAT=clang-format
CLANG_FORMAT_MAJOR=clang-format-${REQUIRED_CLANG_FORMAT_MAJOR}
CLANG_FORMAT_MAJOR_MINOR=${CLANG_FORMAT_MAJOR}.${REQUIRED_CLANG_FORMAT_MINOR}

if [ -x "$(command -v $CLANG_FORMAT_MAJOR)" ]; then CLANG_FORMAT=$CLANG_FORMAT_MAJOR; fi
if [ -x "$(command -v $CLANG_FORMAT_MAJOR_MINOR)" ]; then CLANG_FORMAT=$CLANG_FORMAT_MAJOR_MINOR; fi

if ! [ -x "$(command -v $CLANG_FORMAT)" ]; then
  echo >&2 "error: clang-format is not installed"
  echo >&2 "Install clang-format version ${REQUIRED_CLANG_FORMAT_MAJOR}.${REQUIRED_CLANG_FORMAT_MINOR}.*"
  exit 1
fi

FORCE=0

if [ $# -gt 0 ]; then
  case "$1" in
    -f|--force)
    FORCE=1
    shift
    ;;
  esac
fi

if [ $FORCE -eq 0 ]; then
  CLANG_FORMAT_VERSION=$($CLANG_FORMAT -version | cut -d' ' -f3)
  CLANG_FORMAT_MAJOR=$(echo $CLANG_FORMAT_VERSION | cut -d'.' -f1)
  CLANG_FORMAT_MINOR=$(echo $CLANG_FORMAT_VERSION | cut -d'.' -f2)

  if [ $CLANG_FORMAT_MAJOR != $REQUIRED_CLANG_FORMAT_MAJOR ] || [ $CLANG_FORMAT_MINOR != $REQUIRED_CLANG_FORMAT_MINOR ]; then
    echo >&2 "error: clang-format is the wrong version (${CLANG_FORMAT_VERSION})"
    echo >&2 "Install clang-format version ${REQUIRED_CLANG_FORMAT_MAJOR}.${REQUIRED_CLANG_FORMAT_MINOR}.* or use --force to ignore"
    exit 1
  fi
fi

fail=0

# Default to staged files, unless a commit was passed.
COMMIT=${1:---cached}

# Get modified files (must be on own line for exit-code handling)
modified_files=$(git diff --name-only --diff-filter=ACMRTUXB $COMMIT)

# Loop through each modified file.
for f in ${modified_files}; do
  # Filter them.
  if ! echo "${f}" | egrep -q "[.](cpp|h|mm)$"; then
    continue
  fi
  if ! echo "${f}" | egrep -q "^Source"; then
    continue
  fi

  # Check for clang-format issues.
  d=$($CLANG_FORMAT ${f} | (diff -u "${f}" - || true))
  if ! [ -z "${d}" ]; then
    echo "!!! ${f} not compliant to coding style, here is the fix:"
    echo "${d}"
    fail=1
  fi

  # Check for newline at EOF.
  last_line="$(tail -c 1 ${f})"
  if [ -n "${last_line}" ]; then
    echo "!!! ${f} not compliant to coding style:"
    echo "Missing newline at end of file"
    fail=1
  fi
done

exit ${fail}