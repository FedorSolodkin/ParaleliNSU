# Инструкция по загрузке файлов на сервер

## Способ 1: Использование SCP (Secure Copy Protocol)

### Синтаксис:
```bash
scp [опции] <локальный_файл> <пользователь>@<сервер>:<путь_назначения>
```

### Примеры команд:

#### Загрузка одного файла:
```bash
scp second_lesson_report.pdf user@server.example.com:/path/to/directory/
```

#### Загрузка нескольких файлов:
```bash
scp task1/run_benchmarks.sh task2/run_benchmarks.sh user@server.example.com:/path/to/directory/
```

#### Загрузка всей папки рекурсивно:
```bash
scp -r secondlesson/ user@server.example.com:/path/to/directory/
```

#### Загрузка с указанием порта (если SSH не на стандартном порту 22):
```bash
scp -P 2222 second_lesson_report.pdf user@server.example.com:/path/to/directory/
```

---

## Способ 2: Использование SFTP (SSH File Transfer Protocol)

### Подключение к серверу:
```bash
sftp user@server.example.com
```

### Основные команды в SFTP:
```bash
# Перейти в директорию на сервере
cd /path/to/directory

# Загрузить файл
put second_lesson_report.pdf

# Загрузить несколько файлов
put *.sh

# Загрузить папку рекурсивно
put -r secondlesson/

# Посмотреть текущую директорию на сервере
pwd

# Посмотреть файлы на сервере
ls

# Выйти из SFTP
exit
```

---

## Способ 3: Использование rsync (рекомендуется для больших объёмов данных)

### Синтаксис:
```bash
rsync -avz [опции] <локальный_путь> <пользователь>@<сервер>:<путь_назначения>
```

### Примеры:
```bash
# Загрузка файлов с прогрессом и сжатием
rsync -avz --progress second_lesson_report.pdf user@server.example.com:/path/to/directory/

# Загрузка всей папки с синхронизацией
rsync -avz --progress secondlesson/ user@server.example.com:/path/to/directory/

# Загрузка только изменённых файлов (при повторной загрузке)
rsync -avz --update secondlesson/ user@server.example.com:/path/to/directory/
```

**Преимущества rsync:**
- Загружает только изменённые файлы
- Поддерживает возобновление прерванной загрузки
- Показывает прогресс загрузки

---

## Способ 4: Использование графических клиентов

### Для Windows:
- **WinSCP** - бесплатный SFTP/SCP клиент
- **FileZilla** - поддерживает SFTP через SSH

### Для macOS:
- **Cyberduck** - бесплатный FTP/SFTP клиент
- **Transmit** - платный, но мощный клиент

### Для Linux:
- **FileZilla** - доступен в репозиториях
- **Nautilus** (GNOME Files) - встроенная поддержка SFTP

#### Пример подключения в FileZilla:
1. Откройте FileZilla
2. Введите:
   - Хост: `sftp://server.example.com`
   - Имя пользователя: `user`
   - Пароль: `your_password`
   - Порт: `22` (или ваш порт SSH)
3. Нажмите "Быстрое соединение"
4. Перетащите файлы из локальной панели в панель сервера

---

## Способ 5: Через Git (если на сервере настроен Git)

### Если есть доступ к Git-репозиторию на сервере:
```bash
# Добавить удалённый репозиторий
git remote add server ssh://user@server.example.com/path/to/repo.git

# Запушить изменения
git push server main
```

---

## Полезные советы:

1. **Проверка подключения к серверу:**
   ```bash
   ssh user@server.example.com
   ```

2. **Копирование SSH-ключа для беспарольного доступа:**
   ```bash
   ssh-copy-id user@server.example.com
   ```

3. **Если используете нестандартный порт SSH:**
   ```bash
   scp -P 2222 file.txt user@server.example.com:/path/
   sftp -P 2222 user@server.example.com
   rsync -e 'ssh -p 2222' file.txt user@server.example.com:/path/
   ```

4. **Загрузка с ограничением скорости (чтобы не загружать канал):**
   ```bash
   rsync -avz --limit-bw=1000 file.txt user@server.example.com:/path/
   ```

5. **Проверка целостности после загрузки:**
   ```bash
   # На локальной машине
   md5sum second_lesson_report.pdf
   
   # На сервере через SSH
   ssh user@server.example.com "md5sum /path/to/directory/second_lesson_report.pdf"
   ```

---

## Быстрая команда для загрузки всех необходимых файлов:

```bash
# Загрузка PDF отчёта
scp secondlesson/second_lesson_report.pdf user@server.example.com:/path/to/lab/

# Загрузка скриптов бенчмарков
scp secondlesson/task1/run_benchmarks.sh user@server.example.com:/path/to/lab/task1/
scp secondlesson/task2/run_benchmarks.sh user@server.example.com:/path/to/lab/task2/
scp secondlesson/lab2/parallelki/2/2_3/run_benchmarks.sh user@server.example.com:/path/to/lab/lab2/parallelki/2/2_3/

# Или всё сразу одной командой (рекурсивно)
scp -r secondlesson/ user@server.example.com:/path/to/lab/
```

Замените `user`, `server.example.com` и `/path/to/...` на ваши реальные данные!
