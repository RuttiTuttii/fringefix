#!/usr/bin/env bash
# выкладывает приёмник отчётов на сервер и, если указан сайт, подключает его в Caddy этого же сервера.
# чужие сервисы не перезапускаются: ставится только новый юнит и, при сайте, мягко перечитывается конфиг Caddy.
# запуск из папки server:
#   # Caddy на самом сервере: имя сайта из Caddyfile
#   ./deploy/deploy.sh myhost site.example 127.0.0.1 "https://site.example/"
#   # Caddy в docker подключается отдельным скриптом, поэтому сайт "-" пропускает этот шаг
#   ./deploy/deploy.sh frankfurt - 172.18.0.1 "https://frankfurt.fragmentzal.store/"
set -euo pipefail

HOST="${1:?имя ssh-хоста}"
SITE="${2:?адрес сайта в Caddyfile}"
BIND="${3:?адрес привязки приёмника, например 10.77.0.1}"
PROBES="${4:?адреса для сравнения кодов ответа до и после, через пробел}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
STAMP="$(date +%Y%m%d-%H%M%S)"

# коды ответа уже работающих адресов снаружи; после правки Caddy они не должны измениться
probe() {
  local u
  for u in $PROBES; do curl -s -o /dev/null -w '%{http_code} ' --max-time 15 "$u" || printf '000 '; done
}
BEFORE="$(probe)"
echo "до: $BEFORE"

# тесты на самом сервере: версия Python там своя, ломаться на ней нельзя
tar -C "$HERE" -cf - fringefix_reports tests | ssh "$HOST" 'rm -rf /tmp/ffr-test && mkdir /tmp/ffr-test && tar -C /tmp/ffr-test -xf -'
ssh "$HOST" 'cd /tmp/ffr-test && python3 -m unittest discover -s tests -t . 2>&1 | tail -3 && rm -rf /tmp/ffr-test'

# пакет и юнит; перезапускается только наш сервис
ssh "$HOST" "mkdir -p /opt/fringefix-reports && rm -rf /opt/fringefix-reports/fringefix_reports"
tar -C "$HERE" -cf - fringefix_reports | ssh "$HOST" "tar -C /opt/fringefix-reports -xf - && find /opt/fringefix-reports -name __pycache__ -prune -exec rm -rf {} +"
sed "s/^Environment=FRINGEFIX_BIND=.*/Environment=FRINGEFIX_BIND=$BIND/" "$HERE/deploy/fringefix-reports.service" \
  | ssh "$HOST" 'cat > /etc/systemd/system/fringefix-reports.service'
ssh "$HOST" 'systemctl daemon-reload && systemctl enable fringefix-reports.service >/dev/null 2>&1 && systemctl restart fringefix-reports.service'
sleep 2
ssh "$HOST" "curl -fsS --max-time 5 http://$BIND:8791/v1/health" && echo

if [ "$SITE" = "-" ]; then
  echo "Caddy не трогаю: сайт не указан"
  exit 0
fi

# Caddy: копия, вставка блока в сайт, проверка конфига; reload мягкий, соединения не рвутся
sed "s#@UPSTREAM@#$BIND:8791#" "$HERE/deploy/Caddyfile.snippet" | ssh "$HOST" 'cat > /tmp/fringefix.caddy.snippet'
ssh "$HOST" bash -s -- "$SITE" "$STAMP" <<'REMOTE'
set -euo pipefail
site="$1"; stamp="$2"
cp -p /etc/caddy/Caddyfile "/etc/caddy/Caddyfile.bak-$stamp"
if ! grep -q "/fringefix/api/" /etc/caddy/Caddyfile; then
  python3 - "$site" <<'PY'
import sys
site = sys.argv[1]
text = open("/etc/caddy/Caddyfile", encoding="utf-8").read()
snippet = open("/tmp/fringefix.caddy.snippet", encoding="utf-8").read()
head = site + " {\n"
if head not in text:
    sys.exit(f"блок сайта {site} не найден в Caddyfile")
# запись в тот же файл: inode не меняется
with open("/etc/caddy/Caddyfile", "w", encoding="utf-8") as f:
    f.write(text.replace(head, head + snippet, 1))
PY
fi
if ! caddy validate --config /etc/caddy/Caddyfile --adapter caddyfile >/dev/null 2>&1; then
  cat "/etc/caddy/Caddyfile.bak-$stamp" > /etc/caddy/Caddyfile
  echo "Caddyfile не прошёл проверку, возвращён прежний" >&2
  exit 1
fi
systemctl reload caddy
rm -f /tmp/fringefix.caddy.snippet
REMOTE

sleep 3
AFTER="$(probe)"
echo "после: $AFTER"
if [ "$BEFORE" != "$AFTER" ]; then
  echo "коды ответа изменились, возвращаю Caddyfile" >&2
  ssh "$HOST" "cat /etc/caddy/Caddyfile.bak-$STAMP > /etc/caddy/Caddyfile && systemctl reload caddy"
  exit 1
fi
curl -fsS --max-time 15 "https://$SITE/fringefix/api/v1/health"
echo
