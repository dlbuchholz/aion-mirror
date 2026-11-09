#ifndef FAKE_TRANSLATOR_H
#define FAKE_TRANSLATOR_H

#include <QCoreApplication>
#include <QHash>
#include <QTranslator>

// Stands in for a language in tests that need translated texts without depending on the shipped
// translations: answers from its own list, anything else stays as written. Installed while it lives.
class FakeTranslator : public QTranslator {
public:
    explicit FakeTranslator(const QHash<QString, QString>& texts) : texts(texts) {
        QCoreApplication::installTranslator(this);
    }
    ~FakeTranslator() override { QCoreApplication::removeTranslator(this); }

    bool isEmpty() const override { return false; }
    QString translate(const char* context, const char* sourceText, const char* = nullptr, int = -1) const override {
        return texts.value(QString("%1/%2").arg(context, sourceText));
    }

private:
    QHash<QString, QString> texts;                   // "Context/Source text" → translation
};

#endif // FAKE_TRANSLATOR_H
