#include "word.h"

// Image word
Word::Word(LazyLoadedImage *image, Type type, const QString &copytext,
           const QString &tooltip, const Link &link)
    : m_image(image)
    , m_text()
    , m_isImage(true)
    , m_type(type)
    , m_copyText(copytext)
    , m_tooltip(tooltip)
    , m_color()
    , m_link(link)
{
    image->width();  // professional segfault test
}

// Text word
Word::Word(const QString &text, Type type, const QColor &color,
           const QString &copytext, const QString &tooltip, const Link &link)
    : m_image(NULL)
    , m_text(text)
    , m_isImage(false)
    , m_type(type)
    , m_copyText(copytext)
    , m_tooltip(tooltip)
    , m_color(color)
    , m_link(link)
{
}