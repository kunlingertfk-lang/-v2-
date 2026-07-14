#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

const QString kLocalHalconRoot = QStringLiteral("/opt/halcon");
const QString kLocalHalconLicense =
        kLocalHalconRoot + QStringLiteral("/license/license.txt");
const QString kOcrModelRelativePath =
        QStringLiteral("ocr/OCRB_0-9A-Z_NoRej.omc");

QString cleanPath(const QString &path)
{
    return QDir::cleanPath(path.trimmed());
}

QString halconLicenseFromEnvironment()
{
    return QString::fromLocal8Bit(qgetenv("HALCON_LICENSE_FILE")).trimmed();
}

void appendUnique(QStringList *paths, const QString &path)
{
    if (!paths)
        return;

    const QString cleaned = cleanPath(path);
    if (cleaned.isEmpty() || paths->contains(cleaned))
        return;

    paths->append(cleaned);
}

void appendHalconLibCandidatesForRoot(QStringList *paths, const QString &root)
{
    const QString cleanedRoot = cleanPath(root);
    if (cleanedRoot.isEmpty())
        return;

    const QDir rootDir(cleanedRoot);
    appendUnique(paths, rootDir.filePath(QStringLiteral("lib/x64-linux/libhalconc.so")));
    appendUnique(paths, rootDir.filePath(QStringLiteral("lib/x64-linux/libhalconc.so.20.11.1")));
}

void appendOcrModelCandidateForRoot(QStringList *paths, const QString &root)
{
    const QString cleanedRoot = cleanPath(root);
    if (cleanedRoot.isEmpty())
        return;

    appendUnique(paths, QDir(cleanedRoot).filePath(kOcrModelRelativePath));
}

QString resolveFirstExisting(const QStringList &candidates)
{
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }

    return QString();
}

QString resolveBundledLicense()
{
    return QFileInfo(kLocalHalconLicense).isReadable()
            ? kLocalHalconLicense : QString();
}

bool isLegacyHalcon24Path(const QString &path)
{
    return path.contains(QStringLiteral("24.11"), Qt::CaseInsensitive);
}

} // namespace

namespace HalconRuntimePaths {

QString initializeHalconEnvironment()
{
    const QString configuredLicense = halconLicenseFromEnvironment();
    if (!configuredLicense.isEmpty())
        return cleanPath(configuredLicense);

    const QString bundledLicense = resolveBundledLicense();
    if (!bundledLicense.isEmpty()) {
        qputenv("HALCON_LICENSE_FILE", QFile::encodeName(bundledLicense));
        return bundledLicense;
    }

    return QString();
}

QString defaultHalconRoot()
{
    return kLocalHalconRoot;
}

QString expectedHalconVersion()
{
    return QStringLiteral("20.11.1");
}

QStringList halconLibCandidates(const QString &explicitPath)
{
    QStringList candidates;
    if (!isLegacyHalcon24Path(explicitPath))
        appendUnique(&candidates, explicitPath);
    appendHalconLibCandidatesForRoot(&candidates, kLocalHalconRoot);
    if (isLegacyHalcon24Path(explicitPath))
        appendUnique(&candidates, explicitPath);
    return candidates;
}

QString resolveHalconLibPath(const QString &explicitPath, QStringList *tried)
{
    const QStringList candidates = halconLibCandidates(explicitPath);
    if (tried)
        *tried = candidates;
    return resolveFirstExisting(candidates);
}

QStringList ocrModelCandidates(const QString &explicitPath)
{
    QStringList candidates;
    if (!isLegacyHalcon24Path(explicitPath))
        appendUnique(&candidates, explicitPath);
    appendOcrModelCandidateForRoot(&candidates, kLocalHalconRoot);
    if (isLegacyHalcon24Path(explicitPath))
        appendUnique(&candidates, explicitPath);
    return candidates;
}

QString resolveOcrModelPath(const QString &explicitPath, QStringList *tried)
{
    const QStringList candidates = ocrModelCandidates(explicitPath);
    if (tried)
        *tried = candidates;
    return resolveFirstExisting(candidates);
}

QString formatTriedPaths(const QStringList &paths)
{
    if (paths.isEmpty())
        return QStringLiteral("<none>");

    return paths.join(QStringLiteral("; "));
}

} // namespace HalconRuntimePaths
