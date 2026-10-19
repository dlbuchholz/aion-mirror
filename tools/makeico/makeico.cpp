// makeico <logo.svg> <out.ico>
//
// Renders the logo at the sizes Windows uses (Explorer, taskbar, Alt+Tab) and writes them into
// one .ico file. Each size is stored as a PNG, which Windows reads since Vista. The build runs
// this whenever the logo changes, so the .exe icon always matches assets/aion-logo.svg.
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QList>
#include <QPainter>
#include <QSvgRenderer>
#include <QtEndian>
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: makeico <logo.svg> <out.ico>\n");
        return 2;
    }
    QSvgRenderer renderer(QString::fromLocal8Bit(argv[1]));
    if (!renderer.isValid()) {
        std::fprintf(stderr, "makeico: can't read %s\n", argv[1]);
        return 1;
    }

    const QList<int> sizes = {16, 20, 24, 32, 40, 48, 64, 128, 256};
    QList<QByteArray> images;
    for (int size : sizes) {
        QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter, QRectF(0, 0, size, size));
        painter.end();

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.convertToFormat(QImage::Format_ARGB32).save(&buffer, "PNG");
        images.append(png);
    }

    // Header, one 16-byte directory entry per image, then the images
    QByteArray ico;
    auto put16 = [&ico](quint16 value) { char bytes[2]; qToLittleEndian(value, bytes); ico.append(bytes, 2); };
    auto put32 = [&ico](quint32 value) { char bytes[4]; qToLittleEndian(value, bytes); ico.append(bytes, 4); };
    put16(0);                                        // Reserved
    put16(1);                                        // Type: icon
    put16(quint16(sizes.size()));
    quint32 offset = quint32(6 + 16 * sizes.size());
    for (int i = 0; i < sizes.size(); ++i) {
        const char dimension = char(sizes[i] >= 256 ? 0 : sizes[i]);   // 0 means 256
        ico.append(dimension);                       // Width
        ico.append(dimension);                       // Height
        ico.append(char(0));                         // No palette
        ico.append(char(0));                         // Reserved
        put16(1);                                    // Colour planes
        put16(32);                                   // Bits per pixel
        put32(quint32(images[i].size()));
        put32(offset);
        offset += quint32(images[i].size());
    }
    for (const QByteArray& png : images) {
        ico.append(png);
    }

    QFile out(QString::fromLocal8Bit(argv[2]));
    if (!out.open(QIODevice::WriteOnly) || out.write(ico) != ico.size()) {
        std::fprintf(stderr, "makeico: can't write %s\n", argv[2]);
        return 1;
    }
    return 0;
}
