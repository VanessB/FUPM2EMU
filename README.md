# FUPM2EMU
Эмулятор компьютера FUPM2 и операционной системы FUPM2OS.

## Спецификация
[FUPM_HomeTask2.pdf](http://www.babichev.org/mipt/FUPM_HomeTask2.pdf) - спецификация архитектуры компьютера FUPM2 и системных вызовов FUPM2OS. Примеры программ.

## Начало работы
### Построение
Для построения проекта требуется CMake версии не ниже 3.7.2. Листинг команд, используемых для построения проекта из директории, находящейся на два уровня ниже файла CMakeLists.txt:
```
cmake -DCMAKE_BUILD_TYPE=Release ../..
make
```

### Справка
Для получения справки по эмулятору используйте ключ `--help` или `-h`.

### Загрузка файла состояния
Для загрузки из файла и выполнения состояния эмулируемой машины поместите файл состояния *state.bin* в одну папку с программой и выполните
```
./FUPM2EMU --load state.bin
```
или
```
./FUPM2EMU -l state.bin
```

### Загрузка файла ассемблерного кода
Для загрузки из файла, трансляции и выполнения ассемблерного кода поместите файл с исходным кодом *program.asm* в одну папку с программой и выполните
```
./FUPM2EMU --assemble program.asm
```
или
```
./FUPM2EMU -a program.asm
```

### Измерение времени выполнения
Для измерения времени выполнения программы (а также времени трансляции в случае загрузки программы как исходного кода) используйте дополнительный ключ `--benchmark` или `-b`.
```
./FUPM2EMU -a tickets.asm -b
[BENCHMARK]: Assembling CPU time used: 0.08ms
55252
[BENCHMARK]: Execution CPU time used: 38.66ms
```

## Запланировано к реализации
* Системные вызовы для работы с файлами и динамически выделяемой памятью.
* Дизассемблер.
* Возможность сохранения сгенерированного состояния машины.
* Приведение используемого формата исполнимого файла программы для FUPM2 к формату, указанному в спецификации.