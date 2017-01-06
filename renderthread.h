#ifndef RENDERTHREAD_H
#define RENDERTHREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>

class QImage;

//Herite de QThread, permet de lancer le calcul dans un thread separe de l'interface visuelle
class RenderThread : public QThread
{
    Q_OBJECT

public:
    RenderThread(QObject *parent = 0);
    ~RenderThread();

    void render(double centerX, double centerY, double scaleFactor, QSize resultSize);

signals:
    //signal emis quand le thread a termine de calculer l'image
    void renderedImage(const QImage &image, double scaleFactor);

protected:
    //appelee quand le thread est lance
    void run() Q_DECL_OVERRIDE;

private:
    uint rgbFromWaveLength(double wave);

    QMutex mutex; //Protege les autres variables privees
    QWaitCondition condition; //Condition pour que la thread soit lancee...
    double centerX;
    double centerY;
    double scaleFactor;
    QSize resultSize;
    bool restart;
    bool abort;

    enum { ColormapSize = 512 };
    uint colormap[ColormapSize];
};

#endif // RENDERTHREAD_H
