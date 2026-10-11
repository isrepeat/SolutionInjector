# Публикация SolutionInjector

Запустите `PublishRelease.cmd` без аргументов. Скрипт увеличит patch-версию максимального тега `vX.Y.Z` (например, `v1.1.0` → `v1.1.1`); если таких тегов нет, начнёт с `v1.0.0`. Учитываются релизы, черновики и теги GitHub. Затем он упакует `bin/SolutionInjector.exe` и x64 Visual C++ Runtime DLL, создаст версионированный GitHub Release и обновит стабильный тег `Last` на коммит опубликованного релиза. Ветка `Last` не изменяется. CMD ожидает нажатия клавиши и сохраняет код завершения операции.

Примеры из корня репозитория:

```powershell
./Scripts/PowerShell/PublishRelease.ps1 -Tag v1.0.0 -PrepareOnly
./Scripts/PowerShell/PublishRelease.ps1 -Tag v1.0.0 -Draft
./Scripts/PowerShell/PublishRelease.ps1 -Tag v1.0.0
```

Параметры `-ExePath` и `-RuntimeDirectory` задают готовый EXE и каталог x64 runtime DLL вручную. `-Repository`, `-Target` и `-Title` переопределяют GitHub-репозиторий, commit/tag/branch локального репозитория и заголовок. По умолчанию Target — `refs/heads/Last`, чтобы одноимённый тег не создавал неоднозначности; соответствующий commit должен быть доступен на GitHub. `-Tag` задаёт версию вручную, `-StableTag` переопределяет стабильный тег. Существующий release не перезаписывается. Черновик не обновляет стабильный тег.

Архив и описание создаются в `!artifacts/releases/<тег>/<идентификатор>/`. SHA-256 архива и EXE выводятся в консоль и включаются в описание release. Скрипт не собирает EXE, не подтверждает его соответствие выбранному commit и не выполняет commit/push ветки. Перед публикацией подготовьте и проверьте подходящий Release x64 EXE. PrepareOnly создаёт архив без публикации; при автоматическом выборе версии нужен доступ к GitHub. Для подготовки без сети задайте `-Tag` явно. Draft создаёт черновик release.