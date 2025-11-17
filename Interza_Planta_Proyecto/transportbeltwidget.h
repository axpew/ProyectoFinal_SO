#ifndef TRANSPORTBELTWIDGET_H
#define TRANSPORTBELTWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <functional>

class TransportBeltWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TransportBeltWidget(QWidget *parent = nullptr);

    // resourcePath debe ser del tipo ":/images/ensamble.png"
    void setupWithImage(const QString &resourcePath);

    // cycles controla cuántas pasadas; usamos 1 por defecto
    void startAnimation(int cycles = 1, std::function<void()> onFinished = nullptr);
    void stopAnimation();

    // pausa suave de la animación (detiene ticks, mantiene posición)
    void pauseAnimation();
    void resumeAnimation();

    // reset visual (vuelve producto a la izquierda)
    void resetPosition();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void animateStep();

private:
    QPixmap productPixmap;
    QTimer *moveTimer = nullptr;
    int productX = 0;
    int beltSpeed = 2;       // será recalculado para lograr ~5s
    int totalSteps = 0;
    std::function<void()> onFinish = nullptr;
    bool paused = false;
};

#endif // TRANSPORTBELTWIDGET_H
