#ifndef UPDATER_H
#define UPDATER_H

#include <QObject>

#ifdef __OBJC__
@class AppUpdaterDelegate;
#endif

class SparkleUpdater : public QObject
{
    Q_OBJECT
    
public:
    SparkleUpdater();
    void checkForUpdateInformation();
    void changeUpdateStatus(bool updateFound);

signals:
    bool updateStatusChanged(bool updateFound);

public slots:
    void checkForUpdates();

private:
#ifdef __OBJC__
    AppUpdaterDelegate *_updaterDelegate;
#else
    void *_updaterDelegate;
#endif
};

#endif