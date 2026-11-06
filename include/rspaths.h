#ifndef FRINGEFIX_RSPATHS_H
#define FRINGEFIX_RSPATHS_H

#include <stddef.h>

/* разбор ReShade.ini для установки аддона: куда ReShade ищет эффекты и аддоны. модуль без платформенного кода,
 * поэтому проверяется на любой системе; пути остаются строками, склеивает их вызывающий */

/* значение ключа в секции ini (имена без учёта регистра, BOM и комментарии пропускаются); 1, если найдено,
 * в out значение без пробелов по краям. при нехватке места значение обрезается */
int ff_rs_ini_get(const char *text, const char *section, const char *key, char *out, size_t n);

/* первый путь из списка поиска эффектов: до первой запятой, без кавычек, без хвоста из звёздочек и разделителя
 * ("." + "\reshade-shaders\Shaders\**" даёт ".\reshade-shaders\Shaders"); возвращает длину, 0 если пути нет */
size_t ff_rs_first_path(const char *value, char *out, size_t n);

/* путь абсолютный: буква диска с двоеточием или UNC */
int ff_rs_is_absolute(const char *path);

#endif
