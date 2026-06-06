#!/usr/bin/env bash
# Normalize contract smoke tests for Bazel runfiles (TEST_SRCDIR).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREAMBLE='if [[ -n "${TEST_SRCDIR:-}" ]]; then
  if [[ -d "${TEST_SRCDIR}/_main" ]]; then
    cd "${TEST_SRCDIR}/_main"
  else
    cd "${TEST_SRCDIR}"
  fi
else
  cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
fi'

for f in \
  "$ROOT/services/gateway/tests/"*.sh \
  "$ROOT/services/recommendation-server/tests/"*.sh \
  "$ROOT/services/tracking-server/tests/"*.sh \
  "$ROOT/services/user-server/tests/"*.sh; do
  [[ -f "$f" ]] || continue
  if grep -q 'TEST_SRCDIR' "$f"; then
    continue
  fi
  python3 - "$f" <<'PY'
import re, sys
path = sys.argv[1]
text = open(path).read()
# Drop ROOT_DIR file existence preamble through first python block.
text = re.sub(
    r'^#!/usr/bin/env bash\nset -euo pipefail\n\n'
    r'ROOT_DIR=.*?\n\n(?:\[\[ -f "\$[^"]+" \]\].*\n)*',
    '#!/usr/bin/env bash\nset -euo pipefail\n\n'
    'if [[ -n "${TEST_SRCDIR:-}" ]]; then\n'
    '  if [[ -d "${TEST_SRCDIR}/_main" ]]; then\n'
    '    cd "${TEST_SRCDIR}/_main"\n'
    '  else\n'
    '    cd "${TEST_SRCDIR}"\n'
    '  fi\nelse\n'
    '  cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"\n'
    'fi\n\n',
    text,
    count=1,
    flags=re.DOTALL,
)
open(path, 'w').write(text)
print('patched', path)
PY
  chmod +x "$f"
done
