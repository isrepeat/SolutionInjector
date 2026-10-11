# Публикация SolutionInjector

Запустите `PublishRelease.cmd` двойным кликом и введите новый тег. Скрипт упакует `bin/SolutionInjector.exe` и x64 Visual C++ Runtime DLL из установленной Visual Studio, затем создаст GitHub Release в `isrepeat/SolutionInjector` через авторизованный `gh`. CMD ожидает нажатия клавиши и сохраняет код завершения операции.

Примеры из корня репозитория:

```powershell
./Scripts/PowerShell/PublishRelease.ps1 -Tag v1.0.0 -PrepareOnly
./Scripts/PowerShell/PublishRelease.ps1 -Tag v1.0.0 -Draft
./Scripts/PowerShell/PublishRelease.ps1 -Tag v1.0.0
```

Параметры `-ExePath` и `-RuntimeDirectory` задают готовый EXE и каталог x64 runtime DLL вручную. `-Repository`, `-Target` и `-Title` переопределяют GitHub-репозиторий, commit/tag/branch локального репозитория и заголовок. По умолчанию Target — HEAD; соответствующий commit должен быть доступен на GitHub. Существующий release не перезаписывается.

Архив и описание создаются в `!artifacts/releases/<тег>/<идентификатор>/`. SHA-256 архива и EXE выводятся в консоль и включаются в описание release. Скрипт не собирает EXE, не подтверждает его соответствие выбранному commit и не выполняет commit/push. Перед публикацией подготовьте и проверьте подходящий Release x64 EXE. PrepareOnly создаёт архив без обращения к GitHub; Draft создаёт черновик release.