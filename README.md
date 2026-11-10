# CehPrint

Android-приложение печати от КодоЦех. Печатает по USB и по сети. Для принтеров без собственных языков печати использует встроенные драйверы Linux: brlaser, SpliX (Samsung, Xerox), Gutenprint (Canon, Epson), HPLIP (HP), Epson ESC/P-R и Canon CAPT (LBP6000, LBP3010 и другие).

<img src="docs/logo/CehPrint_icon_512.png" width="96" alt="CehPrint">

## Скачать

**[CehPrint.apk](https://github.com/cebers227-cmd/CehPrint/releases/latest/download/CehPrint.apk)** — последняя версия.
Все версии: [Releases](https://github.com/cebers227-cmd/CehPrint/releases).

- Android 6.0 и новее, 4 архитектуры (arm64-v8a, armeabi-v7a, x86_64, x86).
- Для установки разрешите установку из неизвестных источников.
- Проверка файла: `sha256sum CehPrint.apk`. Контрольные суммы лежат в описании каждого релиза.
- Подпись APK: SHA-256 сертификата `c38032cd6eacb8bdadade1bf8c94f60cb958c91b3a83e4f0c587867eee4b5152`.

## Что проверено

Драйверы brlaser, SpliX, Gutenprint, HPLIP и Epson ESC/P-R проверены побайтно против Linux и на тестовом принтере-имитации. Canon CAPT (LBP6000 и др.) проверен только на имитации принтера, на живом железе пока нет: считайте его экспериментальным. Если у вас получилось или не получилось, напишите в issue.

## Исходный код

Код приложения закрыт, все права защищены. В этом репозитории лежат только исходники GPL-программ, которые входят в APK как отдельные исполняемые файлы:

| Программа | Лицензия | Где |
|---|---|---|
| captjob (Canon CAPT, на основе captdriver) | GPL-3.0-or-later | [drivers/capt](drivers/capt) |
| cehraster (растеризатор CUPS-растра) | GPL-3.0-or-later | [drivers/cehraster](drivers/cehraster) |
| brlaser, SpliX, Gutenprint, HPLIP, epson-escpr | GPL-2.0 / GPL-2.0+ | точные версии, ссылки и наши патчи: [drivers/SOURCES.md](drivers/SOURCES.md), [drivers/patches](drivers/patches) |

Сборка драйверов: [drivers/build.sh](drivers/build.sh) (Android NDK 26). Полный список сторонних лицензий: [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
Если нужны исходники какого-то из этих компонентов в том виде, в каком они в APK, откройте issue в этом репозитории.

Выковано в КодоЦех · [codoceh.ru](https://codoceh.ru)
