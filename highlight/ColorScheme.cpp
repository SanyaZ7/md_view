#include "ColorScheme.h"

namespace highlight
{

ColorScheme::ColorScheme()
{
    fallback_ = {0x24292e, false, false, false};

    /*
     * Единая светлая схема для всех языков, согласованная с фоном
     * блоков кода (#f3f3f3) в preview.
     *
     * Задаются базовые типы; составные (keyword.function,
     * string.special.key, punctuation.bracket, ...) наследуют
     * оформление родителя через style_for(), поэтому перечислять
     * их не нужно.
     */
    set("keyword",          {0x0000ff, false, false, false});
    set("string",           {0xa31515, false, false, false});
    set("number",           {0x098658, false, false, false});
    set("comment",          {0x008000, false, true,  false});
    set("function",         {0x795e26, false, false, false});
    set("function.special", {0x795e26, true,  false, false});
    set("type",             {0x267f99, false, false, false});
    set("variable",         {0x001080, false, false, false});
    set("variable.builtin", {0x0070c1, false, false, false});
    set("constant",         {0x0070c1, false, false, false});
    set("property",         {0x001080, false, false, false});
    set("label",            {0xaf00db, false, false, false});
    set("module",           {0x267f99, false, false, false});
    set("operator",         {0x24292e, false, false, false});
    set("delimiter",        {0x24292e, false, false, false});
    set("punctuation",      {0x24292e, false, false, false});
    set("embedded",         {0x24292e, false, false, false});

    /* Разметка (HTML/XML/CSS) и прочие распространённые типы. */
    set("tag",              {0x267f99, false, false, false});
    set("attribute",        {0x267f99, false, false, false});
    set("boolean",          {0x0070c1, false, false, false});
    set("character",        {0xa31515, false, false, false});
    set("float",            {0x098658, false, false, false});
    set("parameter",        {0x001080, false, false, false});
    set("field",            {0x001080, false, false, false});
    set("constructor",      {0x795e26, false, false, false});
    set("escape",           {0xa31515, false, false, false});
    set("markup",           {0x24292e, false, false, false});
    set("markup.heading",   {0x0000ff, true,  false, false});
    set("markup.link",      {0x267f99, false, false, false});
    set("markup.raw",       {0x24292e, false, false, false});
    set("error",            {0xd73a49, false, true,  true});
}

void ColorScheme::set(const std::string& type, const TokenStyle& style)
{
    styles_[type] = style;
}

TokenStyle ColorScheme::style_for(const std::string& type) const
{
    /*
     * Точное совпадение, иначе — оформление родительского типа:
     * "string.special.key" -> "string.special" -> "string".
     */
    std::string key = type;

    while (true) {
        const auto found = styles_.find(key);

        if (found != styles_.end()) {
            return found->second;
        }

        const std::size_t dot = key.find_last_of('.');

        if (dot == std::string::npos) {
            return fallback_;
        }

        key.resize(dot);
    }
}

const ColorScheme& ColorScheme::default_light()
{
    static const ColorScheme scheme;
    return scheme;
}

} // namespace highlight
