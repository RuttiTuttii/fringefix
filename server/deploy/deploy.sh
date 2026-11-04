#!/usr/bin/env bash
# выкладывает приёмник отчётов на сервер по ssh и подключает его в существующий Caddy.
# запуск из папки server: ./deploy/deploy.sh msk_direct moscow.fragmentzal.store
set -euo pipefail

HOST="${1:?имя ssh-хоста первым аргументом}"
SITE="${2:?адрес сайта в Caddyfile вторым аргументом}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
STAMP="$(date +%Y%m%d-%H%M%S)"

# пакет и юнит на сервер
ssh "$HOST" "mkdir -p /opt/fringefix-reports"
tar -C "$HERE" -cf - fringefix_reports | ssh "$HOST" "tar -C /opt/fringefix-reports -xf -"
scp -q "$HERE/deploy/fringefix-reports.service" "$HOST:/etc/systemd/system/fringefix-reports.service"
scp -q "$HERE/deploy/Caddyfile.snippet" "$HOST:/tmp/fringefix.caddy.snippet"

# Caddyfile: копия до правки, вставка блока в сайт, проверка; при ошибке возврат к копии
ssh "$HOST" bash -s -- "$SITE" "$STAMP" <<'REMOTE'
set -euo pipefail
site="$1"; stamp="$2"
cp /etc/caddy/Caddyfile "/etc/caddy/Caddyfile.bak-$stamp"
if ! grep -q "/fringefix/api/" /etc/caddy/Caddyfile; then
  python3 - "$site" <<'PY'
import sys
site = sys.argv[1]
text = open("/etc/caddy/Caddyfile", encoding="utf-8").read()
snippet = open("/tmp/fringefix.caddy.snippet", encoding="utf-8").read()
head = site + " {\n"
if head not in text:
    sys.exit(f"блок сайта {site} не найден в Caddyfile")
open("/etc/caddy/Caddyfile", "w", encoding="utf-8").write(text.replace(head, head + snippet, 1))
PY
fi
if ! caddy validate --config /etc/caddy/Caddyfile --adapter caddyfile >/dev/null 2>&1; then
  cp "/etc/caddy/Caddyfile.bak-$stamp" /etc/caddy/Caddyfile
  echo "Caddyfile не прошёл проверку, возвращён прежний" >&2
  exit 1
fi
systemctl daemon-reload
systemctl enable --now fringefix-reports.service
systemctl restart fringefix-reports.service
systemctl reload caddy
rm -f /tmp/fringefix.caddy.snippet
REMOTE

# проверка снаружи: тот же путь, которым пойдёт клиент
sleep 2
curl -fsS "https://$SITE/fringefix/api/v1/health"
echo
