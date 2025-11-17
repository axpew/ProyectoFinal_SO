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
    onFinish = onFinished;

    const int desired_seconds = 5;

    int distance = width();
    if (distance < 200) distance = 600;

    int pw = productPixmap.width() > 0 ? productPixmap.width() : 64;
    int totalDistance = distance + pw;

    int interval_ms = moveTimer->interval();

    double required_speed = (double)totalDistance * interval_ms / (1000.0 * desired_seconds);
    int calcSpeed = qMax(1, (int)qRound(required_speed));

    beltSpeed = calcSpeed;

    int stepsPerCycle = totalDistance / qMax(1, beltSpeed);
    totalSteps = qMax(1, cycles) * stepsPerCycle;

    productX = -pw;
    paused = false;
    if (!moveTimer->isActive()) moveTimer->start();
}

void TransportBeltWidget::stopAnimation()
{
    if (moveTimer->isActive()) moveTimer->stop();
    totalSteps = 0;
    productX = -productPixmap.width();
    paused = false;
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
        if (totalSteps > 0) moveTimer->start();
    }
}

void TransportBeltWidget::resetPosition()
{
    if (moveTimer->isActive()) moveTimer->stop();
    totalSteps = 0;
    productX = -productPixmap.width();
    paused = false;
    update();
}

void TransportBeltWidget::animateStep()
{
    productX += beltSpeed;
    totalSteps--;

    if (totalSteps <= 0) {
        moveTimer->stop();
        productX = -productPixmap.width();  // Ocultar
        update();

        if (onFinish) {
            onFinish();
        }
        return;
    }

    if (productX > width()) {
        productX = -productPixmap.width();
    }

    update();
}

void TransportBeltWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r = rect();
    p.fillRect(r, QColor(245, 238, 220));

    // Franjas
    int stripeW = 48;
    int offset = (productX / 4) % stripeW;
    for (int xx = -stripeW + offset; xx < width(); xx += stripeW) {
        QRect stripe(xx, r.center().y() - 18, stripeW/2, 36);
        p.fillRect(stripe, QColor(230, 220, 200));
    }

    // Producto
    if (productX > -productPixmap.width() && productX < width()) {
        if (!productPixmap.isNull()) {
            int y = (height() - productPixmap.height()) / 2;
            p.drawPixmap(productX, y, productPixmap);
        }
    }
}
