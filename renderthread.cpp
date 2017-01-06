#include "renderthread.h"

#include <QtWidgets>
#include <cmath>

RenderThread::RenderThread(QObject *parent)
    : QThread(parent)
{
    //initialisation des variables qui controlent le flux de lancement de la thread
    restart = false;
    abort = false;

    //initialisation de l'ensemble des couleurs utilises
    for (int i = 0; i < ColormapSize; ++i)
        colormap[i] = rgbFromWaveLength(380.0 + (i * 400.0 / ColormapSize));
}

RenderThread::~RenderThread()
{
    //Quand la thread est detruite
    mutex.lock(); // Bloque les variables de la thread
    abort = true; //Booleen indiquant la fin de la thread
    condition.wakeOne(); //On reveille la thread si elle avait ete mise en pause
    mutex.unlock(); //debloque les variables

    wait(); //Attend fin de la fonction run pour terminer la destruction
}

void RenderThread::render(double centerX, double centerY, double scaleFactor,
                          QSize resultSize)
{
    //Fonction appelee par l'interface, pour creer un nouvel ensemble

    QMutexLocker locker(&mutex);

    this->centerX = centerX;
    this->centerY = centerY;
    this->scaleFactor = scaleFactor;
    this->resultSize = resultSize; //Taille de la Qimage

    if (!isRunning()) {
        // Si pas de thread en cours, on en lance un
        start(LowPriority);
    } else {
        // Sinon, on dit à la fonction run de s'arreter et de recommencer avec les nouveaux parametres
        restart = true;
        condition.wakeOne();
    }
}

void RenderThread::run()
{
    // Qt keyword a la place de foreach
    forever {
        // Variables locales pour minimiser l'utilisation du mutex qui bloque l'acces aux variables aux autres thread
        // Permet de modifier les variables de calcul de la fractale avec la fonction render plus facilement
        mutex.lock();
        QSize resultSize = this->resultSize;
        double scaleFactor = this->scaleFactor;
        double centerX = this->centerX;
        double centerY = this->centerY;
        mutex.unlock();

        int halfWidth = resultSize.width() / 2;
        int halfHeight = resultSize.height() / 2;
        QImage image(resultSize, QImage::Format_RGB32);

        const int NumPasses = 8;
        int pass = 0;
        while (pass < NumPasses) {
            const int MaxIterations = (1 << (2 * pass + 6)) + 32;
            const int Limit = 4;
            bool allBlack = true;

            for (int y = -halfHeight; y < halfHeight; ++y) {
                if (restart)
                    break;
                if (abort)
                    return;

                uint *scanLine =
                        reinterpret_cast<uint *>(image.scanLine(y + halfHeight));
                double ay = centerY + (y * scaleFactor);

                for (int x = -halfWidth; x < halfWidth; ++x) {
                    double ax = centerX + (x * scaleFactor);
                    double a1 = 0.0;//ax;
                    double b1 = 0.0;//ay;
                    int numIterations = 0;

                    do {
                        ++numIterations;
                        double a2 = (a1 * a1) - (b1 * b1) + ax;
                        double b2 = (2 * a1 * b1) + ay;
                        if ((a2 * a2) + (b2 * b2) > Limit)
                            break;

                        ++numIterations;
                        a1 = (a2 * a2) - (b2 * b2) + ax;
                        b1 = (2 * a2 * b2) + ay;
                        if ((a1 * a1) + (b1 * b1) > Limit)
                            break;
                    } while (numIterations < MaxIterations);

                    /*do {
                        ++numIterations;
                        double a2 = (a1 * a1 * a1) - (3 * a1 * b1 * b1) + ax;
                        double b2 = (3 * a1 * a1 * b1) - (b1 * b1 * b1) + ay;
                        if ((a2 * a2) + (b2 * b2) > Limit)
                            break;

                        ++numIterations;
                        a1 = (a2 * a2 * a2) - (3 * a2 * b2 * b2) + ax;
                        b1 = (3 * a2 * a2 * b2) - (b2 * b2 * b2) + ay;
                        if ((a1 * a1) + (b1 * b1) > Limit)
                            break;
                    } while (numIterations < MaxIterations);*/

                    if (numIterations < MaxIterations) {
                        *scanLine++ = colormap[numIterations % ColormapSize];
                        allBlack = false;
                    } else {
                        *scanLine++ = qRgb(0, 0, 0);
                    }
                }
            }

            if (allBlack && pass == 0) {
                pass = 4;
            } else {
                if (!restart)
                    emit renderedImage(image, scaleFactor); // signal de fin de creation de l'image
                ++pass;
            }
        }

        mutex.lock();
        if (!restart)
            condition.wait(&mutex); // met en pause le thread en attendant nouveaux parametres
        restart = false;
        mutex.unlock();
    }
}

// Fonction creant l'ensemble des couleurs utilisees
uint RenderThread::rgbFromWaveLength(double wave)
{
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;

    if (wave >= 380.0 && wave <= 440.0) {
        r = -1.0 * (wave - 440.0) / (440.0 - 380.0);
        b = 1.0;
    } else if (wave >= 440.0 && wave <= 490.0) {
        g = (wave - 440.0) / (490.0 - 440.0);
        b = 1.0;
    } else if (wave >= 490.0 && wave <= 510.0) {
        g = 1.0;
        b = -1.0 * (wave - 510.0) / (510.0 - 490.0);
    } else if (wave >= 510.0 && wave <= 580.0) {
        r = (wave - 510.0) / (580.0 - 510.0);
        g = 1.0;
    } else if (wave >= 580.0 && wave <= 645.0) {
        r = 1.0;
        g = -1.0 * (wave - 645.0) / (645.0 - 580.0);
    } else if (wave >= 645.0 && wave <= 780.0) {
        r = 1.0;
    }

    double s = 1.0;
    if (wave > 700.0)
        s = 0.3 + 0.7 * (780.0 - wave) / (780.0 - 700.0);
    else if (wave <  420.0)
        s = 0.3 + 0.7 * (wave - 380.0) / (420.0 - 380.0);

    r = std::pow(r * s, 0.8);
    g = std::pow(g * s, 0.8);
    b = std::pow(b * s, 0.8);
    return qRgb(int(r * 255), int(g * 255), int(b * 255));
}
