## Общая информация

Объединяет несколько файлов в один. Ограничений по количеству или размеру объединяемых файлов нет, кроме размера физического диска. Можно так же не указывать файлы отдельно, а соединять все файлы из папки или из нескольких папок, в том числе и рекурсивно.

Если последний символ объединяемого файла не является переносом строки, добавляет его, чтобы последняя строка текущего файла и первая строка следующего не склеились в одну.

**Пример:** объединяет файлы `test1.txt` и `test2.txt` с помощью команды `theo m test1.txt test2.txt`

*test1.txt*

```
test@gmail.com:password
somestring
```

*test2.txt*

```
randomemail@test.fake:test
login:password
```

*merged.txt*

```
test@gmail.com:password
somestring
randomemail@test.fake:test
login:password

```

По умолчанию результат будет записан в файл `merged.txt`. В конец файла будет добавлен перенос строки, если в конце последней строки последнего из объединяемых файлов его не было.



## Опции запуска

#### Файловые опции:

- `-d` или `--destination` - путь к итоговому файлу, куда будут записаны все строки со всех объединённых файлов. По умолчанию итоговый файл создаётся в директории, где была запущена программа, а название будет `merged.txt`.

  **Пример:** Консоль, в которую пишется команда, открыта в корневой директории.

  *структура папок и файлов в корневой директории примера*

  ```
  ├── test1.txt
  ├── test2.txt
  └── sub/                   
      ├── test3.txt 
  ```

  После выполнения команды `theo m test1.txt sub test2.txt` итоговым файлом будет `merged.txt` в корневой директории.

  Если пользователь задаст значение параметра при вызове, например, `theo m -d all_tests_merged.txt test1.txt sub test2.txt`, то итоговым файлом будет `all_tests_merged.txt` в корневой директории.

  

- `-r` или `--recursive` - обходить ли переданные директории рекурсивно (если они переданы, на файлы никак не влияет). Булев (логический) параметр, по умолчанию false. НЕ требует передачи значения.

  **Пример:** консоль, из которой вызывается программа, открыта в корневой папке.

  *структура папок и файлов в корневой директории примера*

  ```
  ├── test1.txt            
  └── testfolder/                   
      └── subfolder/
      	├── sub.txt
      ├── test2.txt 
  ```

  При вызове команды `theo m test1.txt testfolder` в итоговом файле будут объединены строки из файла `test1.txt` и из `test2.txt` - файла, располагающегося непосредственно в директории `testfolder`.

  При вызове команды `theo m -r test1.txt testfolder` в итоговом файле будет объединение из всех трех файлов: `test1.txt`, `test2.txt` и `sub.txt`. Если бы в папке `subfolder` были ещё подпапки и в них были ещё текстовые документы, они бы тоже попали в объединённый итоговый файл.