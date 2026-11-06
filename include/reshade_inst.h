#ifndef FRINGEFIX_RESHADE_INST_H
#define FRINGEFIX_RESHADE_INST_H

#include <windows.h>

/* установка ReShade-аддона в папку игры. аддон fringefix.addon64 и эффект fringefix.fx вшиты в exe ресурсами,
 * сам ReShade не ставится и не скачивается: он должен быть в игре уже. куда класть файлы, берётся из ReShade.ini
 * игры (AddonPath в [ADDON], EffectSearchPaths в [GENERAL]), по умолчанию рядом с exe и в reshade-shaders\Shaders */

enum {
    RS_OK = 0,
    /* нет ресурсов в exe: сборка без аддона */
    RS_ERR_RESOURCE = -1,
    /* не записался аддон: чаще всего игра запущена и держит dll */
    RS_ERR_ADDON = -2,
    RS_ERR_EFFECT = -3,
    RS_ERR_EXE = -4
};

/* в папке exe есть признаки ReShade: ReShade.ini или dll-прокси крупнее мегабайта */
int rs_find_reshade(const wchar_t *exe);

/* ставит оба файла; при успехе пути записанных файлов в addon и effect (MAX_PATH), при ошибке в addon путь,
 * который не удалось записать */
int rs_install(HINSTANCE inst, const wchar_t *exe, wchar_t addon[MAX_PATH], wchar_t effect[MAX_PATH]);

/* удаляет оба файла, возвращает сколько файлов было удалено; -1 при неверном пути */
int rs_remove(const wchar_t *exe);

#endif
