# Обновление CalcAero на GitHub

Репозиторий: https://github.com/Kpavel325-404/CalcAero

1. В GitHub Desktop выбери CalcAero, затем Fetch/Pull, если доступны изменения.
2. Распакуй CalcAero-1.3.1-Update.zip в отдельную папку.
3. Скопируй **содержимое GitHub-Update** в корень существующего CalcAero,
   рядом с README.md, src и Makefile. Объедини папки и замени совпадающие файлы.
4. Проверь изменения: два языка, ресурсы, EBOOT, документация и отчёты.
5. Summary: `Add Portuguese and French: CalcAero 1.3.1`.
6. Нажми **Commit to main**, затем **Push origin**.

Архив не заменяет историю Git и не меняет origin. Он не выполняет push
автоматически. Старую папку репозитория не удаляй. Для консоли нужна только
отдельная папка PSP из архива.

## Release

Новый тег: `v1.3.1`, target: main. Название:
`CalcAero 1.3.1 — Portuguese & French (28 languages)`.
Описание: чистый Markdown из RELEASE-NOTES-1.3.1.md.
Приложи CalcAero-1.3.1-Update.zip; пользователю PSP нужна папка PSP внутри.

Аппаратные подтверждения OFW/ARK относятся к 1.3; для 1.3.1 пока есть новые
проверки сборки, подписи и эмулятора. Старые записи и установщик 1.3 оставь
в прежнем выпуске: новый EBOOT имеет другую контрольную сумму.

About: `Scientific calculator for PSP with 28 languages, 8 themes and a signed EBOOT.`
Новая английская картинка меню получена из EBOOT 1.3.1 в PPSSPP;
старые снимки 1.3 сохранены с прежним происхождением.

## Командная строка

После проверки изменений и коммита:

```sh
git push origin main
git tag v1.3.1
git push origin v1.3.1
```

Создавай тег один раз на нужном коммите. Не используй force для перезаписи
существующих тегов или чужих изменений.
