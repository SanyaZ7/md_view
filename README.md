Проект представляет собой простой Qt6/C++ редактор с возможностью рендеринга md файлов и математических формул. В отличие от программы MarkText не сбивается когда языковая модель путает разметку, а отображает содержимое. Отображение формул в целом корректное, но встречаются небольшие
огрехи отображения формул иногда. Рендеринг включается в контексном меню правой кнопки мыши. Основное преимущество - малый размер, 253 килобайта базовая оптимизированная компилятором по размеру версия. То есть как бы небольшая альтернатива тайпскрипт стеку, размер приложений которых 
начинается от 100 МБ. Проект был начат из-за плохой ситуации с GUI под языковые модели в С++ да и в целом в компилируемых языках типа rust и zig. Сначала нужно собрать 2 библиотеки - Tex_AST_v2 и md_lexer_v2 через make; потом основное приложений через cmake или из QtCreator.
В написании использовались gpt-5.6-luna; gpt-6-luna; deepseek-v4.1-flash; редактор koda (https://kodacode.ru/, koda-base модель). А также совсем немного: deepseek-v4-pro; gpt-5.6-terra; laguna-xs-2.1.

This project is a simple Qt6/C++ editor with Markdown and mathematical formula rendering capabilities. Unlike MarkText, it does not break when a language model produces malformed markup, but instead continues to display the content. 
Formula rendering is generally accurate, though occasional minor glitches may occur. Rendering is enabled via the right-click context menu. The main advantage is its small footprint: the base version, optimized for size by the compiler, is only 253 KB. 
It serves as a lightweight alternative to the TypeScript stack, where application sizes typically start at 100 MB. The project was initiated due to the challenges of GUI development for LLMs in C++ and, more broadly, in compiled languages like Rust and Zig. 
First, build two libraries—Tex_AST_v2 and md_lexer_v2—using make; then build the main application via cmake or Qt Creator.

Used gpt-5.6-luna; gpt-6-luna; deepseek-v4.1-flash; koda editor (https://kodacode.ru/, koda-base model). Also a little bit: deepseek-v4-pro; gpt-5.6-terra; laguna-xs-2.1.
