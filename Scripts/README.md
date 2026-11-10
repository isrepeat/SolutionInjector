# Публикация SolutionInjector

Запустите `PublishRelease.cmd` без аргументов. Скрипт выберет следующую patch-версию, выполнит Rebuild основного проекта в Release x64 и подготовит ZIP с EXE и x64 Visual C++ Runtime DLL. Путь результата берётся из MSBuild TargetPath по схеме !artifacts/projects/Windows-x64/msbuild/SolutionInjector/<toolset>/<VCToolsVersion>/md/Release/bin/. При ошибке сборки публикация прекращается; старый EXE не подхватывается.

После подготовки скрипт показывает архив, SHA-256, проект, конфигурацию, репозиторий, версию, commit и состояние рабочего дерева. Публикация выполняется только после ввода точного `y` в ответ на `Publish to GitHub? Type y to confirm`. Любой другой ответ отменяет публикацию, сохраняя подготовленный архив. Это подтверждение требуется также для черновика. CMD ожидает нажатия клавиши после завершения и сохраняет код возврата.

Примеры из корня репозитория:

```powershell
./Scripts/PowerShell/PublishRelease.ps1
./Scripts/PowerShell/PublishRelease.ps1 -Tag v1.1.0 -PrepareOnly
./Scripts/PowerShell/PublishRelease.ps1 -Tag v1.1.0 -Draft
./Scripts/PowerShell/PublishRelease.ps1 -ProjectPath Projects/SolutionInjector.Cli.vcxproj
```

По умолчанию собирается SolutionInjector/SolutionInjector.vcxproj. Параметр `-ProjectPath` выбирает другой C++ проект этого репозитория; относительный путь отсчитывается от корня репозитория. Для CLI результат расположен в !artifacts/projects/Windows-x64/msbuild/SolutionInjector.Cli/<toolset>/<VCToolsVersion>/mt/Release/bin/. Выбор готового EXE через -ExePath больше не поддерживается: перед каждой подготовкой выполняется Release x64 Rebuild выбранного проекта. Параметр `-RuntimeDirectory` задаёт каталог x64 runtime DLL вручную.

Без `-Tag` увеличивается patch-версия максимального тега vX.Y.Z среди релизов, черновиков и тегов GitHub; при отсутствии таких тегов используется v1.0.0. `-Repository`, `-Target` и `-Title` задают GitHub-репозиторий, ссылку на commit и заголовок. По умолчанию Target — refs/heads/Last. Target должен разрешаться в текущий HEAD, поскольку собирается рабочее дерево. Наличие незакоммиченных изменений показывается перед подтверждением и записывается в описание релиза. Скрипт не выполняет commit, push или переключение ветки.

Архив создаётся в !artifacts/releases/<тег>/<идентификатор>/. Перед подтверждением публикации рядом записывается ReleaseNotes.txt с параметрами сборки и контрольными суммами. После подтверждения создаётся версионированный GitHub Release; существующий релиз не перезаписывается. Затем стабильный тег Last перемещается на выбранный commit; имя можно изменить через `-StableTag`. Ветка Last не изменяется.

`-PrepareOnly` выполняет сборку и подготовку архива без запроса подтверждения и без публикации. Для автоматического выбора версии требуется доступ к GitHub; явный -Tag исключает сетевые обращения для выбора версии. `-Draft` после подтверждения создаёт черновик и не перемещает стабильный тег. Тесты скрипт не запускает.