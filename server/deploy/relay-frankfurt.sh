#!/usr/bin/env bash
# публикует приёмник на краевом сервере: его Caddy в docker пересылает /fringefix/api/ по туннелю на сервер хранения.
# этот Caddy держит TLS и для европейского адреса, и для московского, так что пользователям, которым российский адрес
# недоступен, тот же приёмник отвечает через европейский. Контейнер Caddy не перезапускается, конфиг перечитывается мягко.
# запуск из папки server:
#   ./deploy/relay-frankfurt.sh frankfurt "frankfurt.fragmentzal.store moscow.fragmentzal.store" /opt/vpn/caddy/Caddyfile caddy-sub 10.77.0.1:8791 "https://frankfurt.fragmentzal.store/ https://moscow.fragmentzal.store/"
set -euo pipefail

HOST="${1:?имя ssh-хоста}"
HOSTS="${2:?адреса сайтов через пробел, которые отвечают на запросы}"
FILE="${3:?путь к Caddyfile на хосте}"
CONTAINER="${4:?имя контейнера Caddy}"
UPSTREAM="${5:?адрес приёмника по туннелю, например 10.77.0.1:8791}"
PROBES="${6:?адреса для сравнения кодов ответа до и после}"
STAMP="$(date +%Y%m%d-%H%M%S)"

probe() {
  local u
  for u in $PROBES; do curl -s -o /dev/null -w '%{http_code} ' --max-time 15 "$u" || printf '000 '; done
}
BEFORE="$(probe)"
echo "до: $BEFORE"

# аргументы с пробелами ssh склеивает в одну строку, поэтому экранируем их для удалённой оболочки
ARGS="$(printf '%q ' "$HOSTS" "$FILE" "$CONTAINER" "$UPSTREAM" "$STAMP")"
ssh "$HOST" "bash -s -- $ARGS" <<'REMOTE'
set -euo pipefail
sites="$1"; file="$2"; container="$3"; upstream="$4"; stamp="$5"
cp -p "$file" "$file.bak-$stamp"
if ! grep -q "/fringefix/api" "$file"; then
  python3 - "$sites" "$file" "$upstream" <<'PY'
import sys
sites, path, upstream = sys.argv[1:4]
site = sites.split()[0]
lines = open(path, encoding="utf-8").read().split("\n")
block = f"""    # fringefix report receiver: relays to the store on the other server over the private link
    @fringefix_api {{
        host {sites}
        path /fringefix/api /fringefix/api/*
    }}
    handle @fringefix_api {{
        uri strip_prefix /fringefix/api
        reverse_proxy {upstream} {{
            transport http {{
                dial_timeout 5s
                response_header_timeout 20s
            }}
        }}
    }}
""".split("\n")
# вставка сразу после строки-адреса блока, в котором обслуживается этот сайт на портах TLS
for i, line in enumerate(lines):
    if line.endswith("{") and f"https://{site}:" in line:
        lines[i + 1:i + 1] = block[:-1]
        break
else:
    sys.exit(f"блок сайта {site} не найден")
# запись в тот же файл: он подключён в контейнер как файл, и inode менять нельзя
with open(path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
PY
fi
if ! docker exec "$container" caddy validate --config /etc/caddy/Caddyfile --adapter caddyfile >/dev/null 2>&1; then
  cat "$file.bak-$stamp" > "$file"
  echo "Caddyfile не прошёл проверку, возвращён прежний" >&2
  exit 1
fi
docker exec "$container" caddy reload --config /etc/caddy/Caddyfile --adapter caddyfile
REMOTE

sleep 3
AFTER="$(probe)"
echo "после: $AFTER"
if [ "$BEFORE" != "$AFTER" ]; then
  echo "коды ответа изменились, возвращаю Caddyfile" >&2
  ssh "$HOST" "cat $FILE.bak-$STAMP > $FILE && docker exec $CONTAINER caddy reload --config /etc/caddy/Caddyfile --adapter caddyfile"
  exit 1
fi
for h in $HOSTS; do
  curl -fsS --max-time 20 "https://$h/fringefix/api/v1/health"
  echo
done
