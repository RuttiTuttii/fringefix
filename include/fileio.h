#ifndef FRINGEFIX_FILEIO_H
#define FRINGEFIX_FILEIO_H

#include <stddef.h>
#include <wchar.h>

/* пишет файл целиком через временный файл рядом и переименование поверх старого: сбой питания или полный
 * диск оставляют прежнее содержимое, а не пустой файл. 0 или -1 */
int ff_write_file(const wchar_t *path, const void *data, size_t len);

#endif
