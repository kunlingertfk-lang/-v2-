#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QVector>

namespace {

const QString kLocalHalconRoot =
        QStringLiteral("/home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady");
const QString kLegacySuperheHalconRoot =
        QStringLiteral("/home/superhe/桌面/som-halcon/repository/packages.mvtec.com/halcon/halcon-24.11-progress-steady");
const QString kLegacySuperheRuntimeRoot =
        kLegacySuperheHalconRoot + QStringLiteral("/halcon-24.11.2.0-runtime-x64-linux");
const QString kLegacySuperheRuntimeGeneralRoot =
        kLegacySuperheHalconRoot + QStringLiteral("/halcon-24.11.2.0-runtime-general-x64-linux_aarch64-linux_armv7a-linux");
const QString kLegacySuperheLicenseRoot =
        kLegacySuperheRuntimeGeneralRoot + QStringLiteral("/license");
const QString kLocalHalconLicenseRoot =
        kLocalHalconRoot + QStringLiteral("/license");
const QString kOcrModelRelativePath =
        QStringLiteral("ocr/OCRB_0-9A-Z_NoRej.omc");

QString cleanPath(const QString &path)
{
    return QDir::cleanPath(path.trimmed());
}

QString halconRootFromEnvironment()
{
    return QString::fromLocal8Bit(qgetenv("HALCONROOT")).trimmed();
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
    appendUnique(paths, rootDir.filePath(QStringLiteral("lib/x64-linux/libhalconc.so.24.11.2")));
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
    const QVector<QString> licenseRoots = {
        kLocalHalconLicenseRoot,
        kLegacySuperheLicenseRoot
    };

    for (const QString &licenseRoot : licenseRoots) {
        const QDir licenseDir(licenseRoot);
        if (!licenseDir.exists())
            continue;

        const QString standardLicense =
                licenseDir.filePath(QStringLiteral("license.dat"));
        if (QFileInfo(standardLicense).isReadable())
            return standardLicense;

        const QFileInfoList files = licenseDir.entryInfoList(
                    QStringList() << QStringLiteral("license*.dat"),
                    QDir::Files | QDir::Readable,
                    QDir::Time);
        const QStringList preferredNames = {
            QStringLiteral("license_eval_halcon_progress_"),
            QStringLiteral("license_support_halcon_progress_"),
            QStringLiteral("license_support_halcon24.11_steady_")
        };
        for (const QString &prefix : preferredNames) {
            for (const QFileInfo &file : files) {
                if (file.fileName().startsWith(prefix))
                    return file.absoluteFilePath();
            }
        }

        if (!files.isEmpty())
            return files.first().absoluteFilePath();
    }

    return QString();
}

} // namespace

namespace HalconRuntimePaths {

QString initializeHalconEnvironment()
{
    const QString configuredLicense = halconLicenseFromEnvironment();
    if (!configuredLicense.isEmpty()) {
        const QString cleanedLicense = cleanPath(configuredLicense);
        if (QFileInfo(cleanedLicense).isReadable())
            return cleanedLicense;
    }

    const QString bundledLicense = resolveBundledLicense();
    if (!bundledLicense.isEmpty()) {
        qputenv("HALCON_LICENSE_FILE", QFile::encodeName(bundledLicense));
        return bundledLicense;
    }

    return QString();
}

QString defaultHalconRoot()
{
    const QString envRoot = halconRootFromEnvironment();
    return envRoot.isEmpty() ? kLocalHalconRoot : cleanPath(envRoot);
}

QStringList halconLibCandidates(const QString &explicitPath)
{
    QStringList candidates;
    appendUnique(&candidates, explicitPath);

    const QString envRoot = halconRootFromEnvironment();
    if (!envRoot.isEmpty())
        appendHalconLibCandidatesForRoot(&candidates, envRoot);

    appendHalconLibCandidatesForRoot(&candidates, kLocalHalconRoot);
    appendHalconLibCandidatesForRoot(&candidates, kLegacySuperheRuntimeRoot);
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
    appendUnique(&candidates, explicitPath);

    const QString envRoot = halconRootFromEnvironment();
    if (!envRoot.isEmpty())
        appendOcrModelCandidateForRoot(&candidates, envRoot);

    appendOcrModelCandidateForRoot(&candidates, kLocalHalconRoot);
    appendOcrModelCandidateForRoot(&candidates, kLegacySuperheRuntimeGeneralRoot);
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
