#include "transportbeltwidget.h"
#include <QPainter>
#include <QDebug>
#include <QFile>
#include <QtMath>

TransportBeltWidget::TransportBeltWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(100);
    setMaximumHeight(140);

    moveTimer = new QTimer(this);
    moveTimer->setInterval(30);
    connect(moveTimer, &QTimer::timeout, this, &TransportBeltWidget::animateStep);
}

void TransportBeltWidget::setupWithImage(const QString &resourcePath)
{
    qDebug() << "TransportBeltWidget: cargando recurso" << resourcePath
             << "exists?" << QFile::exists(resourcePath);

    productPixmap = QPixmap(resourcePath);
    if (productPixmap.isNull()) {
        qWarning() << "TransportBeltWidget::setupWithImage - imagen no encontrada:" << resourcePath;
        productPixmap = QPixmap(64,64);
        productPixmap.fill(Qt::gray);
    }

    productPixmap = productPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    productX = -productPixmap.width();
    update();
}

void TransportBeltWidget::startAnimation(int cycles, std::function<void()> onFinished)
{
    // CRÍTICO: Detener completamente cualquier animación en progreso
    if (moveTimer->isActive()) {
        moveTimer->stop();
    }

    // Esperar a que el timer termine
    QCoreApplication::processEvents();

    // Resetear estado
    totalSteps = 0;
    paused = false;
    onFinish = nullptr;

    // Configurar nueva animación
    onFinish = onFinished;

    const int desired_seconds = 5;  // Duración fija
    const int interval_ms = 30;     // Intervalo del timer

    // SOLUCIÓN DEFINITIVA: Calcular basado en el ancho REAL actual
    int currentWidth = width();
    int pw = productPixmap.width() > 0 ? productPixmap.width() : 64;

    // El producto debe viajar desde -pw hasta currentWidth (salir completamente)
    int totalDistance = currentWidth + pw;

    // Calcular cuántos frames tenemos en 5 segundos
    int totalFrames = (desired_seconds * 1000) / interval_ms;  // 5000ms / 30ms = ~166 frames

    // Velocidad necesaria para recorrer totalDistance en totalFrames
    // Redondear hacia ARRIBA para asegurar que llega al final
    double exactSpeed = (double)totalDistance / (double)totalFrames;
    beltSpeed = qMax(4, (int)ceil(exactSpeed));  // ceil() para redondear hacia arriba

    // Recalcular steps basado en la velocidad ajustada
    // Esto asegura que el producto recorra toda la distancia
    totalSteps = (totalDistance + beltSpeed - 1) / beltSpeed;  // División con redondeo hacia arriba

    // SIEMPRE empezar desde la izquierda (fuera de vista)
    productX = -pw;

    // Debug
    qDebug() << "Animación Estación - Width:" << currentWidth
             << "| Distance:" << totalDistance
             << "| Speed:" << beltSpeed << "px/frame"
             << "| Steps:" << totalSteps
             << "| Duración:" << (totalSteps * interval_ms / 1000.0) << "seg";

    // Forzar actualización visual
    update();
    QCoreApplication::processEvents();

    // Iniciar timer
    moveTimer->start();
}

void TransportBeltWidget::stopAnimation()
{
    if (moveTimer->isActive()) {
        moveTimer->stop();
    }

    totalSteps = 0;
    int pw = productPixmap.width() > 0 ? productPixmap.width() : 64;
    productX = -pw;  // Fuera de vista a la izquierda
    paused = false;
    onFinish = nullptr;
    update();
}

void TransportBeltWidget::pauseAnimation()
{
    if (moveTimer->isActive()) {
        moveTimer->stop();
        paused = true;
    }
}

void TransportBeltWidget::resumeAnimation()
{
    if (paused) {
        paused = false;
        if (totalSteps > 0) {
            moveTimer->start();
        }
    }
}

void TransportBeltWidget::resetPosition()
{
    if (moveTimer->isActive()) {
        moveTimer->stop();
    }

    totalSteps = 0;
    int pw = productPixmap.width() > 0 ? productPixmap.width() : 64;
    productX = -pw;  // Fuera de vista a la izquierda
    paused = false;
    onFinish = nullptr;
    update();
}

void TransportBeltWidget::animateStep()
{
    // Verificar que debemos animar
    if (totalSteps <= 0) {
        moveTimer->stop();

        // Asegurar que el producto esté completamente fuera de vista
        int pw = productPixmap.width() > 0 ? productPixmap.width() : 64;
        productX = width() + pw;
        update();

        if (onFinish) {
            auto callback = onFinish;
            onFinish = nullptr;
            callback();
        }
        return;
    }

    // Avanzar el producto
    productX += beltSpeed;
    totalSteps--;

    // Si terminamos los steps
    if (totalSteps <= 0) {
        moveTimer->stop();

        // Producto fuera de vista
        int pw = productPixmap.width() > 0 ? productPixmap.width() : 64;
        productX = width() + pw;
        update();

        if (onFinish) {
            auto callback = onFinish;
            onFinish = nullptr;
            callback();
        }
        return;
    }

    // Continuar animación
    update();
}

void TransportBeltWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r = rect();
    p.fillRect(r, QColor(245, 238, 220));

    // Franjas de la banda (siempre visibles)
    int stripeW = 48;
    int offset = (productX / 4) % stripeW;
    for (int xx = -stripeW + offset; xx < width(); xx += stripeW) {
        QRect stripe(xx, r.center().y() - 18, stripeW/2, 36);
        p.fillRect(stripe, QColor(230, 220, 200));
    }

    // Dibujar producto SOLO si está en rango razonable
    if (productX > -productPixmap.width() && productX < width() + 20) {
        if (!productPixmap.isNull()) {
            int y = (height() - productPixmap.height()) / 2;
            p.drawPixmap(productX, y, productPixmap);
        }
    }
}
