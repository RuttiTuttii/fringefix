#ifndef FRINGEFIX_GAMEDETECT_H
#define FRINGEFIX_GAMEDETECT_H

#include <windows.h>

#include "config.h"

enum { GAME_NONE = 0, GAME_FULLSCREEN, GAME_3D, GAME_LISTED };

/* нужна ли оверлею пауза или облегчённый режим. cfg->auto_pause: 0 проверка выключена, 1 полный экран,
 * 2 ещё и 3D-приложения. полным экраном считается активное окно без рамки с заголовком, закрывающее весь
 * монитор (игра в окне без рамки, монопольный полный экран, видео на F11); 3D-приложением процесс с
 * загруженным d3d9, d3d11, d3d12, vulkan или opengl вне списка известных не игр.
 * cfg->ignore_apps всегда вне проверки, cfg->pause_apps всегда считаются игрой.
 * exe получает имя файла активной программы в UTF-8 без пути, пустое, если процесс не открылся.
 * окна самого fringefix, рабочий стол и панель задач не считаются.
 * 3D-приложение с защитой от чтения процесса (античит) определяется только по полному экрану */
int game_detect(const ff_config *cfg, const RECT *monitor, char exe[64]);

#endif
