#ifndef ALGORITHMS_HALCON_HALCONRUNTIMEPATHS_H
#define ALGORITHMS_HALCON_HALCONRUNTIMEPATHS_H

#include <QString>
#include <QStringList>

namespace HalconRuntimePaths {

QString initializeHalconEnvironment();
QString defaultHalconRoot();
QStringList halconLibCandidates(const QString &explicitPath = QString());
QString resolveHalconLibPath(const QString &explicitPath = QString(),
                             QStringList *tried = nullptr);
QStringList ocrModelCandidates(const QString &explicitPath = QString());
QString resolveOcrModelPath(const QString &explicitPath = QString(),
                            QStringList *tried = nullptr);
QString formatTriedPaths(const QStringList &paths);

} // namespace HalconRuntimePaths

#endif // ALGORITHMS_HALCON_HALCONRUNTIMEPATHS_H
