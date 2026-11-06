#ifndef FRINGEFIX_RESOURCE_H
#define FRINGEFIX_RESOURCE_H

/* номера ресурсов в src/res/fringefix.rc. значки: цветной для включённого оверлея и серый для выключенного */
#define IDI_APP 1
#define IDI_OFF 2
#define IDI_FLAG_AUTO 10
#define IDI_FLAG_RU 11
#define IDI_FLAG_EN 12

/* номера комплектных профилей, тип RCDATA */
#define IDR_PROFILE_QD34 201

/* ReShade-аддон и его эффект, тип RCDATA; аддон собирается в build/fringefix.addon64 до exe */
#define IDR_ADDON 202
#define IDR_ADDON_FX 203

#endif
