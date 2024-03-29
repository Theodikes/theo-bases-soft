## Установка

Скачайте файл `theo.exe` со [страницы релизов софта](https://github.com/Theodikes/theo-bases-soft/releases). После скачивания сначала перенесите файл туда, откуда вы его случайно не удалите, затем запустите один раз. Теперь софт можно использовать в любом месте из консоли, вызвав команду `theo`.

**Внимание!** Не удаляйте изначальный файл после запуска, не переносите и не переименовывайте его, потому что тогда собьётся привязка в PATH и команда `theo` в консоли не будет работать.

## Использование софта

Вызов софта производится из консоли (cmd, PowerShell или любая другая аналогичная).

Формат команды - `theo command [command option(s)] path(s) `:

1. `theo` - название софта и исполняемого файла программы
2. `command` - полное либо сокращенное название самой команды, например `d` или `deduplicate` для удаления всех повторяющихся строк в файле, `c` или `count` для подсчёта количества строк в файле и так далее
3. `command options(s)` - опции (в сокращённом или полном варианте), уникальные для каждой команды. Например `-l` (сокращённый вариант) или `--lines` (полный вариант) для команды `split` . Значение этой опции идёт следом за названием, допустим, `theo split -l 1000` - разбить файл на много файлов, в каждом из которых будет по тысяче строк.
4. `path(s)` - Путь к файлу, нескольким файлам или папкам с файлами, которые надо обработать. Обязательно использовать после всех опций, через пробел, в самих путях пробелов быть не должно, если они есть, путь должен быть заключён в кавычки. Пример - `theo m test1.txt "te s t spaces.txt" testfolder`

Программа никак не изменяет входной файл, а всегда создаёт новый с результатом работы. При указании одного файла одновременно входным и выходным, поведение не определено (скорее всего, работать не будет).

## Все доступные команды

**Формат описания:** ссылка на полный гайд по команде - пример команды - краткое описание

1. [Удаление дубликатов в файле](deduplication.md) - `theo d test.txt` - удаляет все совпадающие строки в файле. Размер обрабатываемого файла ограничен лишь размером дискового пространства - программа может дедуплицировать файлы, используя не оперативную память, а диск.
2. [Нормализация баз (файлов) любого формата](normalization.md) - `theo n -b logpass test.txt testfolder` - фильтрует базу от невалидных строк, приводит к нужному формату те, которые возможно. Можно выбрать тип баз (emailpass, numpass или logpass), и сконфигурировать все остальные параметры (допустимые сепараторы, минимальная длина пароля и так далее). Может нормализовывать сразу все файлы в папках (рекурсивно тоже). Обязательно смотрите полный гайд по ссылке; 
3. [Объединение файлов](merging.md) - `theo m test1.txt testfolder test2.txt` - собирает все строки из файлов и записывает в один итоговый. В указанном примере объединит test1.txt, test2.txt и все файлы из папки testfolder в файл merged.txt, может объединять файлы из папок рекурсивно;
4. [Разделение файла по количеству строк](splitting.md) - `theo s -l 100000 test.txt` - разбивает один файл на N файлов, в каждом будет заданное пользователем количество строк (кроме последнего, там будет остаток). В приведённом примере test.txt будет разбит на файлы, в каждом из которых будет по 100 000 строк, итоговые файлы будут называться `test_100000_1.txt`, `test_100000_2.txt` и так далее;
5. [Подсчёт количества строк в файле](counting.md) - `theo c test.txt testfolder` - выводит в консоль количество строк в файле (разделителем строк считается исключительно символ '\n'). Может считать сумму строк в нескольких файлах или даже во всех файлах в директории (как в примере в директории testfolder);
6. [Получение только логинов/емейлов или только паролей](tokenization.md) - `theo t -p last test.txt testfolder` -  сохранение только первой части всех строк из файла (до сепаратора) или только второй (после сепаратора). Пользователь может сам задавать удобные ему сепараторы вместо стандартных - `;` и `:`. В указанном примере сохраняются только пароли из-за параметра `-p last`, по умолчанию при запуске `-p first` - то есть, сохраняются емейлы/логины/номера. Работает с любым количеством файлов и с папками, в том числе рекурсивно;
7. [Перемешивание строк в файле](randomization.md) - `theo r test.txt`  - рандомное перемешивание строк в файле (напоминаю, что исходный файл не изменяется, а создается новый перемешанный). Использует оперативную память практически на полную для ускорения работы.